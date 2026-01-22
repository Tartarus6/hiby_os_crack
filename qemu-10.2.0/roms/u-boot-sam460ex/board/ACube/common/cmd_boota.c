/* cmd_boota.c , AKA the AOS 4.x primary bootloader.
   Idea, design and (broken) code by Andrea Vallinotto.
   Have fun, have phone, have gun, etc...
*/

#include <common.h>
#include <command.h>
#include <malloc.h>
#include <../net/tftp.h>
#include <asm/processor.h>
#include <asm/io.h>
#include <asm/mmu.h>
#include <asm/4xx_pcie.h>
#include <asm/gpio.h>
#include "cmd_boota.h"
#include "slb/sbl_errcodes.h" //For the error codes.
#include "sys_dep.h"
#include "net.h"
#include "vesa_video.h"
#include "../menu/menu.h"
#include "../../../drivers/bios_emulator/vesa.h"

#undef DEBUG
#ifdef DEBUG
#define PRINTF(fmt,args...)	printf (fmt ,##args)
#else
#define PRINTF(fmt,args...)
#endif

DECLARE_GLOBAL_DATA_PTR;

#define MAX_BLOCKSIZE 32768 //This is the PHYSICAL size, not logical!!

extern u8 *logo_buf8;
extern u8 *logo_buf16;
extern struct FrameBufferInfo *fbi;
extern int console_col; /* cursor col */
extern int console_row; /* cursor row */
extern int scrolled;
extern int isgadget;
extern int firsttime;
extern int valid_elf_image(unsigned long addr);
extern unsigned long load_elf_image(unsigned long addr);

static char * blockbuffer ; //IO buffer used many times.

static void interpret_sbl_failure(const WORD err)
	 /* Codes are as follows:
	(see slb/sbl.h)
	 */
{
	char *stringout;

	switch(err)
	{
		case SBL_COULDNT_INIT:
			stringout = "Couldn't initialize second-level bootloader (out of memory ?).\n";
			break;

		case SBL_PROTOCOL_TOO_OLD:
			stringout = "Second level bootloader is too old; please upgrade it.\n";
			break;

		case SBL_PROTOCOL_TOO_NEW:
			stringout = "Second level bootloader requires a newer BIOS version; please upgrade.\n";
			break;

		case SBL_NO_CONFIG_FILES_FOUND:
			stringout = "No configuration file found in any partitions.\n";
			break;

		case SBL_FAILED_LOADING_KERNEL_IMAGE:
			stringout = "Failed loading kernel (or kickstart) image file(s).\n";
			break;

		case SBL_UNKNOWN_ERROR:
		default:
			stringout = "Unknown return code from secondary bootloader.\n";
			break;
	}

	if (gd->flags & GD_FLG_SILENT)
	{
		gd->flags &= ~GD_FLG_SILENT;
		console_row = CONSOLE_ROW_START + 4;
		console_col = 0;
		isgadget = 0;
	}

	printf(stringout);
}

static BOOL good_checksum(const LONG * bl, const UWORD len)
{
	ULONG chsum=0;
	UWORD cnt;

	for(cnt=0;cnt<len;cnt++)
		chsum+=*bl++;

	return (BOOL)(chsum==0);
}

static BOOL is_good_bootsector(const struct BootstrapCodeBlock * const bcb,
			const unsigned long blocksize)
{
	if(bcb->bcb_ID == IDNAME_BOOTSTRAPCODE)
	{
		if(bcb->bcb_SummedLongs <= (blocksize>>2))
		{
			if(good_checksum((const LONG * const)bcb, bcb->bcb_SummedLongs))
			    return TRUE;
			else
			{
			    gpio_config(30, GPIO_OUT, GPIO_SEL, GPIO_OUT_1);
			    printf("Bad checksum while reading second level bootloader\n");
			}
		}
		else printf("Bad block structure while reading second level bootloader: summedlongs not good: %lu instead of %lu\n", bcb->bcb_SummedLongs, blocksize>>2);
	}

	return FALSE;
}

static ULONG find_secondary_bootloader_start_HD(const unsigned long blocksize)
{
	ULONG currsec = 0;

	PRINTF("Entered find_sec_bl_start %d %p\n",blocksize,blockbuffer);

	if ( ! blocksize) return (ULONG)-1;
	if ( ! blockbuffer) return (ULONG)-1;

	while(loadsector(currsec, blocksize, 1, blockbuffer))
	{
		PRINTF("Reading sector %lu\n", currsec);

		if(is_good_bootsector((struct BootstrapCodeBlock *)blockbuffer, blocksize))
			return currsec;

		if(++currsec > SBL_HIGHEST)
			return (ULONG)-1;
	}
	PRINTF("Loadsector failed\n");

	return (ULONG)-1; //This means a read error.
}

static ULONG secondary_bootloader_length(ULONG start_sect, const UWORD blocksize, ULONG * const dest_len)
{
	ULONG next = start_sect, res=0;
	BOOL readres;
	struct BootstrapCodeBlock * bcb = (struct BootstrapCodeBlock *)blockbuffer;

	do	{
		readres = loadsector(next, blocksize, 1, blockbuffer);

		if(!readres)
		{
			*dest_len = res;
			return (READ_ERROR|(next & 0xffff));
		}

		if(!is_good_bootsector((struct BootstrapCodeBlock *)blockbuffer, blocksize))
			return (READ_SYNTAX_ERR|next);

		res++;
	}
	while((next=bcb->bcb_Next) != UNUSED_BLOCK_ADDRESS);

	res--; // Excludes last sector.
	res *= (blocksize - HEADER_INFO_SIZE); // -20 is to exclude header information.
	res += ((bcb->bcb_SummedLongs<<2) - HEADER_INFO_SIZE); //Last sector.

	*dest_len = res;
	return LOAD_OK;
}

static void load_secondary_bootloader(ULONG start_sect, char * dest_buffer, const UWORD blocksize,
				      const ULONG len)
{
	/* No error check is made, so be careful everything's ok before calling */
	ULONG nextsec=start_sect, chunklen;
	char * copystart = blockbuffer + HEADER_INFO_SIZE;
	ULONG * current = (ULONG *)dest_buffer;
	struct BootstrapCodeBlock * bcb = (struct BootstrapCodeBlock *)blockbuffer;

	do
	{
		loadsector(nextsec, blocksize, 1, blockbuffer);
		memcpy((char *)current, copystart, (chunklen=bcb->bcb_SummedLongs-(HEADER_INFO_SIZE>>2))<<2);
		current+=chunklen;
   }
	while((nextsec=bcb->bcb_Next) != UNUSED_BLOCK_ADDRESS);
}

static void start_secondary_bootloader(void * start, struct sbl_callback_context * context)
{
	WORD (* bls)(struct sbl_callback_context *);
	unsigned long entrypoint;
	WORD result;

	if(!valid_elf_image((u32)start))
	{
		printf("Error: no real ELF image installed as bootloader!\n");
		return;
	}

	entrypoint = load_elf_image((u32)start);
	bls = (WORD (* )(struct sbl_callback_context *))entrypoint;

	result = bls(context);

	interpret_sbl_failure(result);
}

static BOOL is_good_bootsource(const char * const str)
{
  /* Table as follows, from bios_menu.c
	 floppy -> internal floppy (not yet supported)
	 cdrom  -> ide CDROM(s)
	 ide    -> ide disk(s)
	 net    -> TFTP
	 scdrom -> SCSI CDROM(s)
	 scsi   -> SCSI disk(s)
	 ucdrom -> USB CDROM(s)
	 usb    -> USB disk(s)

  */
  if(find_dae(str))
	return TRUE;

  return FALSE;
}

#define CHECK_IMAGE_AND_ZERO_IF_BAD(pnt) \
	if(!valid_elf_image((u32)pnt)) \
		{ \
		free(pnt);\
		pnt = 0;\
		printf("bad ELF image loaded; skipping!");\
		} \
	else { } //printf("found SLB\n");

int do_boota(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	/* - Scan sequenziale secondo le variabili boot(x).
	   - Se ? forzata una selezione di media type,
	   - si cerca quella.
	   - se non si trova, si ricomincia.
	   - Quindi la funzione di scansione ritorna vero se si ? trovato qualcosa; in ingresso dovr?
	     prendere il tipo di device che si vuole.
	*/
	char *env;
	static char *argarray[5] = { 0 };
	UWORD argcnt=0;
	ULONG sector_size;
	SCAN_HANDLE scanner = NULL;
	void *sbl_buffer = NULL;
	short TFTP_options_backup = TFTP_quit_on_error;

	// if we were in silent mode and, switch back in graphic mode

	if ( ! (gd->flags & GD_FLG_SILENT))
	{
		int hush = 0;
		char *s = getenv("hush");
		if (s) hush = atoi(s);
		if (hush)
		{
			s = getenv("stdout");
			if ((s) && (strncmp(s,"vga",3) == 0))
			{
				gd->flags |= GD_FLG_SILENT;

				if (fbi && logo_buf8 && logo_buf16)
				{
					if (fbi->BitsPerPixel == 8)
					{
						memcpy(fbi->BaseAddress, logo_buf8, fbi->XSize * fbi->YSize);
					}

					if (fbi->BitsPerPixel == 16)
					{
						memcpy(fbi->BaseAddress, logo_buf16, fbi->XSize * fbi->YSize * 2);
					}

					scrolled = 0;
					gfxbox(IBOX_START_X, IBOX_START_Y, IBOX_END_X, IBOX_END_Y, 1);
				}
			}
		}
	}

	TFTP_quit_on_error = TRUE;

	blockbuffer = alloc_mem_for_iobuffers(MAX_BLOCKSIZE);

	PRINTF("First-level bootloader: entered main\n");
	//Right now argc and argv are ignored....

	//Builds the set of strings to boot from. This is passed as "scan_list" to the lowlevel functions.

	env = getenv("boot1");
	if(env) {
		PRINTF("found: %s\n",env);
		if(is_good_bootsource(env)) argarray[argcnt++]=strdup(env);
	}

	env = getenv("boot2");
	if(env) {
		PRINTF("found: %s\n",env);
		if(is_good_bootsource(env)) argarray[argcnt++]=strdup(env);
	}

	env = getenv("boot3");
	if(env) {
		PRINTF("found: %s\n",env);
		if(is_good_bootsource(env)) argarray[argcnt++]=strdup(env);
	}

	PRINTF("First-level bootloader: got %u valid boot sources\n", argcnt);

	if(!argcnt) //No variables set ?
		return 0;

	argarray[argcnt]=(char *)0; //0 terminates.

	for(scanner = start_unit_scan((void *)argarray, &sector_size);
	  scanner;
	  scanner = next_unit_scan(scanner, &sector_size))
	{
		switch(scanner->ush_device.type) //Here we make distinctions between the different media boot types.
		{
			case DEV_TYPE_HARDDISK:
			{
				ULONG p_loc;
				p_loc = find_secondary_bootloader_start_HD(sector_size);

				PRINTF("Found an HD\n");
				if(p_loc != (ULONG)-1) //Found something!
				{
					ULONG sbl_length = 0, io_res;

					PRINTF("FLB: found something\n");

					io_res = secondary_bootloader_length(p_loc, sector_size, &sbl_length);

					if(io_res == LOAD_OK)
					{
						PRINTF("FLB: SLB of length %lu; loading\n", sbl_length);
						sbl_buffer = alloc_mem_for_bootloader(sbl_length);
						load_secondary_bootloader(p_loc, sbl_buffer, sector_size, sbl_length);
						//printf("Success!\n");
						CHECK_IMAGE_AND_ZERO_IF_BAD(sbl_buffer);
					}
				}

				break;
			}

			case DEV_TYPE_CDROM:
			{
				//El Torito style booting.
				disk_partition_t p_info;
				block_dev_desc_t * blockdev = get_lowlevel_handler(scanner);

				printf("Scanning CD/DVD %s %s %s", scanner->ush_device.vendor, scanner->ush_device.product, scanner->ush_device.revision);

				PRINTF("Found a CD\n");
				get_partition_info(blockdev, 0, &p_info);
				sbl_buffer=alloc_mem_for_bootloader(p_info.size*p_info.blksz);
				PRINTF("AOS CD boot partition on disk is %lu sectors long.\n", p_info.size);

				if(blockdev->block_read(blockdev->dev, p_info.start, p_info.size, sbl_buffer) != p_info.size)
				{
					puts(" read error when trying to load CD secondary booter\n");
					free(sbl_buffer);
					sbl_buffer=0;
				}
				else
				{
					PRINTF("CD boot image (el Torino) loaded.");
					CHECK_IMAGE_AND_ZERO_IF_BAD(sbl_buffer);
				}
				break;
			}

			case DEV_TYPE_NETBOOT:
			{
				//Ok, here we try to load the secondary bootloader via TFTP
				int transfer_size;
				void * temp;
				/* allocates memory for bootloader. Since the uboot very broken implementation
				   of tftp doesn't support the newer extensions, I can't get the damn file size.
				   What the heck, the tftp functions might even choke if the server sends any
				   extension. So a "reasonably big" amount of memory is allocated. */
				temp = alloc_mem_for_bootloader(BOOTLOADER_MAX_BUFFER);

				env = getenv("netboot_file");
				if (env == NULL) env = "OS4Bootloader";

				PRINTF("Starting Net booting procedure; looking for bootloader. Load address will be %lx\n", temp);
				puts("Starting Net booting procedure\n");
				if ((transfer_size = my_NetLoop(env, temp)) != -1)
				{
					//Success.
					sbl_buffer = temp;
					CHECK_IMAGE_AND_ZERO_IF_BAD(sbl_buffer);
					puts("Successfully loaded SLB from network\n");
					break;
				}
				else printf("Couldn't download %s from network.\n",env);
				free(temp);
				break;
			}

			default:
				printf("No known boot method for device type %d\n", scanner->ush_device.type);
		}


		if(sbl_buffer) //Already loaded ? Then skip the other units (devices).
			break;
	}

	end_unit_scan(scanner);
	end_global_scan();

	if(sbl_buffer)
	{
		struct sbl_callback_context * cbc = build_callback_context(argarray);
		//Should it be bootmedia instead of foundmedia ?
		//ULONG *temp=(ULONG *)sbl_buffer;

		PRINTF("FLB: SLB loaded; now launching it\n");

		//New version: loads up an ELF image!
		start_secondary_bootloader(sbl_buffer, cbc);
	}
	else
	{
		if (gd->flags & GD_FLG_SILENT)
		{
			gd->flags &= ~GD_FLG_SILENT;
			console_row = CONSOLE_ROW_START + 4;
			console_col = 0;
			isgadget = 0;
		}

		puts("FLB: no SLB found in any of the designated boot sources; returning to u-boot.\n");
	}

	TFTP_quit_on_error = TFTP_options_backup;

	puts("Press any key to continue\n");
	getc();

	return 0;
}

/* Uboot 1.0.0 support here. */
U_BOOT_CMD(
	boota,      1,      0,      do_boota,
	"start AmigaOS boot procedure",
	". 'Boota' allows to boot AmigaOS alike OSes on Sam\n"
);
