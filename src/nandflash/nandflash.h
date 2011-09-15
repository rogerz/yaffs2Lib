#ifndef _NAND_FLASH_H_
#define _NAND_FLASH_H_

/* Control bits map */

#define NAND_FLASH_CTRL_DFLT	0x3F
#define NAND_FLASH_CEx_N(chip) (1<<chip)
#define NAND_FLASH_ALE (1<<6)
#define NAND_FLASH_CLE (1<<7)

/* Status bits map */

#define NAND_FLASH_RBNx(chip) (1<<chip)

/* Status Register Bitmap */

#define NAND_FLASH_STATUS_FAIL 	0x01
#define NAND_FLASH_STATUS_EDC 	0x02
#define NAND_FLASH_STATUS_VALID	0x04
#define NAND_FLASH_STATUS_READY 0x40
#define NAND_FLASH_STATUS_WPN	0x80	/* 0-Write Protect */

/* CS address space */

extern volatile unsigned char * const nandflash_ctrl_ptr; 
extern volatile unsigned char * const nandflash_data_ptr;
		
#define nandflash_chip_enable(chip) \
	do { \
		int x=0; \
		*nandflash_ctrl_ptr = NAND_FLASH_CTRL_DFLT & (~NAND_FLASH_CEx_N(chip)); \
		do{ \
			x++; \
		}while(x<10); \
	} while(0)

/* debug flag */
#define DRV_DEBUG_OFF           0x00000000
#define DRV_DEBUG_TRACE			0x00000001
#define DRV_DEBUG_ERROR			0x00001000
#define DRV_DEBUG_ALL           0xffffffff

#define DRV_LOG(FLG, X0, X1, X2, X3, X4, X5, X6)                        \
    do {                                                                \
    if (nandflash_debug & FLG)                                            \
		logMsg (X0, (int)X1, (int)X2, (int)X3, (int)X4,       \
                            (int)X5, (int)X6);                          \
    } while (0)

typedef struct{
	char maker_code;
	char device_code;
	char cell_type;
	char size_info;
	char plane_info;
} FLASH_DEVICE_ID;

typedef enum{
	NANDFLASH_SAMSUNG = 0xEC,
} NANDFLASH_VENDOR;

typedef enum{
	K9K8G08U0A = 0xD3,
	K9F1G08U0A = 0xF1,
} NANDFLASH_TYPE;

extern unsigned int nandflash_debug;
extern NANDFLASH_VENDOR flash_vendor;
extern NANDFLASH_TYPE flash_type;

#endif	/* _NAND_FLASH_H_ */
