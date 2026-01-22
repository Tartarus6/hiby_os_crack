#define CONFIG_SYS_MAX_FLASH_BANKS	1	/* max number of memory banks		*/
#define CONFIG_SYS_MAX_FLASH_SECT	128	/* max number of sectors on one chip	*/

#define CONFIG_SYS_FLASH_ERASE_TOUT	150000	/* Timeout for Flash Erase (in ms)	*/
#define CONFIG_SYS_FLASH_WRITE_TOUT	800	/* Timeout for Flash Write (in ms)	*/

#define CONFIG_SYS_FLASH_EMPTY_INFO		/* print 'E' for empty sector on flinfo */

#define CONFIG_SYS_FLASH_WORD_SIZE unsigned char
#define CONFIG_SYS_FLASH_ADDR0 0x555
#define CONFIG_SYS_FLASH_ADDR1 0x2aa