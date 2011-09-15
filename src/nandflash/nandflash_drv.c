#include "nandflash.h" 
#include "vxworks.h"
#include "ixp425.h"
#include "semLib.h"

unsigned int nandflash_debug = DRV_DEBUG_ERROR;
BOOL nandflash_init_flag=FALSE;
volatile unsigned char * const nandflash_ctrl_ptr = (unsigned char *)IXP425_EXPANSION_BUS_CS5_BASE;
volatile unsigned char * const nandflash_data_ptr = (unsigned char *)IXP425_EXPANSION_BUS_CS6_BASE;

NANDFLASH_VENDOR flash_vendor=NANDFLASH_SAMSUNG;
NANDFLASH_TYPE flash_type=K9K8G08U0A;

SEM_ID nandflash_sem;

/* init ixp for nand flash operation */
int nandflash_init()
{
#define IXP425_EXP_TIMING_BASE	0xC4000000
	unsigned int value;
	unsigned int *ptr;
	int i;
	if(nandflash_init_flag){
		return -1;
	}

	for(i=5;i<7;i++){
		ptr=(unsigned int *)(IXP425_EXP_TIMING_BASE+4*i);
		value = *ptr;
		DRV_LOG(DRV_DEBUG_TRACE, "IXP425_EXP_TIMING_CS%d = %#08X->\n", i,value,0,0,0,0);
		value |= 0xbfff2043; /*80000003;*/	/* CSx_EN, WR_EN, BYTE_EN = 1*/
		value &= (~(1<<4));	/* MUX_EN = 0 */
		*ptr = value;
		DRV_LOG(DRV_DEBUG_TRACE, "%#08X\n", value,0,0,0,0,0);
	}

	nandflash_sem = semMCreate(SEM_Q_PRIORITY|SEM_DELETE_SAFE);
	if(nandflash_sem==NULL){
	   	DRV_LOG(DRV_DEBUG_ERROR, "nandflash_sem create failed\n",0,0,0,0,0,0);
		return -1;
	}

	nandflash_init_flag = TRUE;
	return 0;
}

int nandflash_lock(){
	semTake(nandflash_sem, WAIT_FOREVER);
	return 0;
}

int nandflash_unlock(){
	semGive(nandflash_sem);
	return 0;
}

/* P17 - Command Latch Cycle */
int nandflash_cmd_latch(int chip, unsigned char cmd)
{
	unsigned char ctrl_byte = NAND_FLASH_CTRL_DFLT & (~NAND_FLASH_CEx_N(chip)) | NAND_FLASH_CLE;
	nandflash_lock();
	*nandflash_ctrl_ptr = ctrl_byte;
	*nandflash_data_ptr = cmd;
/*	*nandflash_ctrl_ptr = NAND_FLASH_CTRL_DFLT;	*/
	nandflash_unlock();
	return 0;
}

/* P17 - Address Latch Cycle */
int nandflash_addr_latch(int chip, int col, int row)
{
	unsigned char ctrl_byte = NAND_FLASH_CTRL_DFLT & (~NAND_FLASH_CEx_N(chip)) | NAND_FLASH_ALE;
	*nandflash_ctrl_ptr = ctrl_byte;
	/* offset */
	if(col>=0){
		*nandflash_data_ptr = col & 0xff;
		*nandflash_data_ptr = (col>>8) & 0xff;
	}
	/* page */
	if(row>=0){
		*nandflash_data_ptr = row & 0xff;
		*nandflash_data_ptr = (row>>8) & 0xff;
		/* k9k8g08u0a require three cycle row address latch */
		if(flash_type==K9K8G08U0A){
			*nandflash_data_ptr = (row>>16) & 0xff;
		}
	}

/*	*nandflash_ctrl_ptr = NAND_FLASH_CTRL_DFLT;	*/
	return 0;
}

/* P18 - Input Data Cycle */
int nandflash_data_latch(int chip, unsigned char buf[], int length)
{
	int i;
	unsigned char ctrl_byte = NAND_FLASH_CTRL_DFLT & (~NAND_FLASH_CEx_N(chip)); 
	*nandflash_ctrl_ptr = ctrl_byte;

	for(i=0;i<length;i++){
		*nandflash_data_ptr = buf[i];
	}

/*	*nandflash_ctrl_ptr = NAND_FLASH_CTRL_DFLT;	*/
	return 0;
}

/* P18 check status */
int nandflash_ready(int chip)
{
	unsigned char status_byte;
	unsigned char ctrl_byte = NAND_FLASH_CTRL_DFLT & (~NAND_FLASH_CEx_N(chip)); 
	*nandflash_ctrl_ptr = ctrl_byte;
	status_byte = *nandflash_ctrl_ptr;
	return status_byte & NAND_FLASH_RBNx(chip);
}

int nandflash_wait_timeout(int chip, int timeout)
{
	double dummy=0;
	int i;

	/* waste sometime for CS change */
	for(i=0;i<10;i++){
		dummy+=i;
	}

	while(!nandflash_ready(chip)){
		timeout--;
		if(timeout==0){
			DRV_LOG(DRV_DEBUG_ERROR,"%s() timeout\n", __func__,0,0,0,0,0);
			return TRUE;
		} 
	}

	for(i=0;i<10;i++){
		dummy+=timeout;
	}

	return FALSE;
}

/* P18 - read n bytes from page */
int nandflash_read(int chip, int col, int row, char buf[], int length)
{
	int i;
	nandflash_lock();

	nandflash_reset(chip);
	nandflash_cmd_latch(chip, 0x00);
	nandflash_addr_latch(chip, col, row);
	nandflash_cmd_latch(chip, 0x30);

	if(nandflash_wait_timeout(chip, 10000)) return -1;

	nandflash_chip_enable(chip);
	for(i=0;i<length;i++){
		buf[i]=*nandflash_data_ptr;
	}

	nandflash_unlock();
	return 0;
}

/* P19 - Status Read Cycle */
char nandflash_status_read(int chip, int edc)
{
	char status;
	edc && nandflash_cmd_latch(chip, 0x7B) || nandflash_cmd_latch(chip, 0x70);
	nandflash_chip_enable(chip);
	status = *nandflash_data_ptr;
	DRV_LOG(DRV_DEBUG_TRACE, "%s() status=0x%02X\n",__func__, status,0,0,0,0);
	return status;
}

int nandflash_check_error(chip)
{
	char status=nandflash_status_read(chip, 0);
	int error=status & NAND_FLASH_STATUS_FAIL;
	if(error){
		DRV_LOG(DRV_DEBUG_ERROR, "%s, status error %#2X\n", __func__, status,0,0,0,0);
	}
	return error;
}

int nandflash_check_edc(chip)
{
	char status=nandflash_status_read(chip, 1);
	int error=(status & NAND_FLASH_STATUS_FAIL) || (status & (NAND_FLASH_STATUS_EDC|NAND_FLASH_STATUS_VALID));
	if(error){
		DRV_LOG(DRV_DEBUG_ERROR, "%s, status error %#2X\n", __func__, status,0,0,0,0);
	}
	return error;
}
/* P21 - Random Data Output In a Page */
/* This command should always succeed nandflash_read */
int nandflash_random_output(int chip, int col, char buf[], int length)
{
	int i;
	nandflash_lock();

	nandflash_cmd_latch(chip, 0x05);
	nandflash_addr_latch(chip, col, -1);
	nandflash_cmd_latch(chip, 0xE0);

	nandflash_chip_enable(chip);
	for(i=0;i<length;i++){
		buf[i]=*nandflash_data_ptr;
	}

	nandflash_unlock();
	return 0;
}

/* P22 - Page Program */
int nandflash_page_program(int chip ,int col, int row, char buf[], int length)
{
	int i,ret;

	nandflash_lock();
	nandflash_reset();

	DRV_LOG(DRV_DEBUG_TRACE, "%s() chip %d col %d row %d buf 0x%02X%02X\n",
			__func__, chip, col, row, buf[0], buf[1]);
	nandflash_cmd_latch(chip, 0x80);
	nandflash_addr_latch(chip, col, row);
	nandflash_data_latch(chip, buf, length);

	nandflash_cmd_latch(chip, 0x10);
	if(nandflash_wait_timeout(chip, 100000)) ret=-1;
	else ret=nandflash_check_error(chip);

	nandflash_unlock();
	return ret;
}

/* P22 - Page Program With Random Data Input Preparation*/
int nandflash_page_program_prepare(int chip, int col, int row, char buf[], int length)
{
	int i;
	nandflash_lock();
	nandflash_reset();
	DRV_LOG(DRV_DEBUG_TRACE, "%s() chip %d col %d row %d buf %x length %d\n",
			__func__, chip, col, row, buf, length);
	nandflash_cmd_latch(chip, 0x80);
	nandflash_addr_latch(chip, col, row);
	nandflash_data_latch(chip, buf, length);
	nandflash_unlock();
	return 0;
}

/* P23 - page program operation with random data input*/
int nandflash_page_program_random(int chip, int col, char buf[], int length, int commit)
{
	int ret;
	nandflash_lock();
	DRV_LOG(DRV_DEBUG_TRACE, "%s() chip %d col %d buf %x length %d\n",
			__func__, chip, col, buf, length,0);
	nandflash_cmd_latch(chip, 0x85);
	nandflash_addr_latch(chip, col, -1);
	nandflash_data_latch(chip, buf, length);
	if(commit){
		nandflash_cmd_latch(chip,0x10);
		if(nandflash_wait_timeout(chip, 100000)) ret=-1;
		else ret=nandflash_check_error(chip);
	}else{
		ret=0;
	}
	nandflash_unlock();
	return ret;
}

/* P24 - copy-back program operation */
int nandflash_copyback_program(int chip, int col1, int row1, int col2, int row2, int length){
	int ret;
	nandflash_lock();
	DRV_LOG(DRV_DEBUG_TRACE, "%s() chip %d col1 %d row1 %d col2 %d row2 %d\n",
			__func__, chip, col1, row1, col2, row2);
#define ROW_TO_PLANE(row) (row/64/4096*2+row/64%2)
	if(ROW_TO_PLANE(row1)!=ROW_TO_PLANE(row2)){
		logMsg(DRV_DEBUG_ERROR, "%s() row %d and %d are in different plane\n",__func__,row1,row2);
		nandflash_unlock();
		return -1;
	}

	nandflash_cmd_latch(chip, 0x00);
	nandflash_addr_latch(chip, col1, row1);
	nandflash_cmd_latch(chip, 0x35);
	if(nandflash_wait_timeout(chip, 100000)) ret=-1;
	else{
		nandflash_cmd_latch(chip, 0x85);
		nandflash_addr_latch(chip, col2, row2);
		nandflash_cmd_latch(chip, 0x10);
		if(nandflash_wait_timeout(chip, 100000)) ret=-1;
		else ret=nandflash_check_edc(chip);
	}

	nandflash_unlock();

	return ret;
}

/* P24 - copy-back program operation preparation */
int nandflash_copyback_program_prepare(int chip, int col1, int row1, int col2, int row2, char buf[], int length){
	nandflash_lock();
	nandflash_reset();
	DRV_LOG(DRV_DEBUG_TRACE, "%s() chip %d col1 %d row1 %d col2 %d row2 %d\n",
			__func__, chip, col1, row1, col2, row2);
	nandflash_cmd_latch(chip, 0x00);
	nandflash_addr_latch(chip, col1, row1);
	nandflash_cmd_latch(chip, 0x35);
	if(nandflash_wait_timeout(chip, 100000)) return -1;

	nandflash_cmd_latch(chip, 0x85);
	nandflash_addr_latch(chip, col2, row2);
	nandflash_data_latch(chip, buf, length);
	nandflash_unlock();
	return 0;
}

/* P25 - block erase */
int nandflash_block_erase(int chip, int row)
{
	int ret;
	nandflash_lock();
	nandflash_reset();
	DRV_LOG(DRV_DEBUG_TRACE, "%s() chip %d, row %d\n", __func__, chip, row,0,0,0);
	nandflash_cmd_latch(chip, 0x60);
	nandflash_addr_latch(chip, -1, row);
	nandflash_cmd_latch(chip, 0xD0);
	if(nandflash_wait_timeout(chip, 100000)) ret=-1;
	else ret=nandflash_check_error(chip);
	nandflash_unlock();
	return ret;
}

/* P26 - read device id */
int nandflash_read_device_id(int chip, FLASH_DEVICE_ID *id)
{
	unsigned char ctrl_byte = NAND_FLASH_CTRL_DFLT;
	unsigned char data_byte=0;

	if(id==NULL){
		DRV_LOG(DRV_DEBUG_ERROR, "id is null pointer\n", 0,0,0,0,0,0);
		return -1;
	}

	nandflash_lock();

	ctrl_byte &= (~(NAND_FLASH_CEx_N(chip)));
	ctrl_byte |= NAND_FLASH_CLE;
	*nandflash_ctrl_ptr = ctrl_byte;

	data_byte=0x90;
	*nandflash_data_ptr = data_byte;

	ctrl_byte &= (~NAND_FLASH_CLE);
	ctrl_byte |= NAND_FLASH_ALE;
	*nandflash_ctrl_ptr = ctrl_byte;

	data_byte=0x00;
	*nandflash_data_ptr = data_byte;

	ctrl_byte &= (~NAND_FLASH_ALE);
	*nandflash_ctrl_ptr = ctrl_byte;

	id->maker_code = *nandflash_data_ptr;
	id->device_code = *nandflash_data_ptr;
	id->cell_type = *nandflash_data_ptr;
	id->size_info = *nandflash_data_ptr;
	id->plane_info = *nandflash_data_ptr;

	DRV_LOG(DRV_DEBUG_TRACE, "%s() id=0x%02X %02X %02X %02X %02X\n", 
			__func__, id->maker_code, id->device_code, id->cell_type, id->size_info, id->plane_info);

	nandflash_unlock();

	return 0;
}

/* P32 - reset */
int nandflash_reset(int chip)
{
	nandflash_cmd_latch(chip, 0xff);
}
