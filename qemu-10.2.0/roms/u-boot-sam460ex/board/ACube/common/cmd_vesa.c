#include <common.h>
#include <command.h>
#include <asm/cache.h>
#include "sys_dep.h"
#include "../../../drivers/bios_emulator/vesa.h"

int do_vesa(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	DoVesa(argc, argv);
	return 0;
}

U_BOOT_CMD(
	vesa,    5,     1,     do_vesa,
	"set a vesa mode\n",
	"hex mode"
);
