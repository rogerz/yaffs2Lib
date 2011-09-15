#include "nandflash.h"
#include "math.h"
#include "time.h"
#include "stdlib.h"

int dump_data(const char *fmt, void *pdata, int length, int wrap){
	int i;
	if(pdata==NULL) return -1;
	if(fmt) printf("%s", fmt);
	for(i=0;i<length;i++){
		if(wrap && i && i%wrap == 0) printf("\n", i);
		printf("%02X ",((char *)pdata)[i]);
	}
	printf("\n");
	return -1;
}

int nandflash_self_test(int chip)
{
	FLASH_DEVICE_ID device_id;
	int page, page1;
	char test[]="1234567890";
	char buf[]="0987654321";
	char page_buf[2112], page_buf1[2112];
	char k9f1_device_id[]={0xec,0xf1,0x00,0x95,0x40};
	char k9k8_device_id[]={0xec,0xd3,0x51,0x95,0x58};
	int i;
	int rows=65536;

	printf("> verify device id\n");
	nandflash_read_device_id(chip, &device_id);
	if(!memcmp(&device_id, k9f1_device_id, 5)){
		printf("* OK k9f1 device found\n");
		rows=65536;	
		flash_type = K9F1G08U0A;
	}else if(!memcmp(&device_id, k9k8_device_id, 5)){
		printf("* OK k9k8 device found\n");
		rows=524288;
		flash_type = K9K8G08U0A;
	}else{
		printf("! ERROR: Unknown device 0x%02X %02X %02X %02X %02X\n", 
			device_id.maker_code, device_id.device_code, device_id.cell_type, device_id.size_info, device_id.plane_info);
	}
	
	page=1.0*rand()*rows/RAND_MAX;

	printf("> test read/write on page %d\n", page);
	nandflash_block_erase(chip,page);
	nandflash_page_program(chip,0,page,test,10);
	nandflash_read(chip,0,page,buf,10);
	if(strcmp(test, buf)){
		printf("! ERROR\n");
		dump_data("write: ", test, 10, 0);
		dump_data("read: ", buf, 10, 0);
	}else{
		printf("* OK\n");
	}
	nandflash_block_erase(chip,page);

	page=1.0*rand()*rows/RAND_MAX;
	printf("> test random read on page %d\n", page);
	bzero(buf,10);
	nandflash_block_erase(chip,page);
	nandflash_page_program(chip,0,page,test,10);
	nandflash_read(chip,0,page, buf,3);
	nandflash_random_output(chip,5,buf+5,5);
	nandflash_random_output(chip,3,buf+3,2);
	if(memcmp(buf,test,10)){
		printf("! ERROR\n");
		dump_data("write: ", test, 10, 0);
		dump_data("read: ", buf, 10, 0);
	}else{
		printf("* OK\n");
	}
	nandflash_block_erase(chip,page);

	page=1.0*rand()*rows/RAND_MAX;
	printf("> test random write on page %d\n", page);
	bzero(buf,10);
	nandflash_block_erase(chip,page);
	nandflash_page_program_prepare(chip,0,page,test,3);
	nandflash_page_program_random(chip,5,test+5,5,0);
	nandflash_page_program_random(chip,3,test+3,2,1);
	nandflash_read(chip,0,page, buf,10);
	if(memcmp(buf,test,10)){
		printf("! ERROR\n");
		dump_data("write: ", test, 10, 0);
		dump_data("read: ", buf, 10, 0);
	}else{
		printf("* OK\n");
	}
	nandflash_block_erase(chip,page);

	page=1.0*rand()*rows/RAND_MAX;
	if(flash_type==K9K8G08U0A){
		/* make sure the two pages are on the same plane */
		page1=rand()%64+(rand()%2048*2+page/64/4096*4096+page/64%2)*64;
	}else{
		page1=1.0*rand()*rows/RAND_MAX;
	}
	printf("> test copy-back operation from page %d to %d\n", page, page1);
	nandflash_block_erase(chip,page);
	nandflash_block_erase(chip,page1);
	for(i=0;i<2112;i++){
		page_buf[i]=rand() & 0xff;
	}
	nandflash_page_program(chip, 0, page, page_buf, 2112);
	nandflash_copyback_program(chip, 0, page, 0, page1, page_buf, 2112);
	nandflash_read(chip, 0, page1, page_buf1, 2112);
	if(memcmp(page_buf, page_buf1, 2112)){
		printf("! ERROR\n");
		dump_data("write\n", page_buf, 2112, 32);
		dump_data("read\n", page_buf1, 2112, 32);
	}else{
		printf("* OK\n");
	}
	nandflash_block_erase(chip,page);
	nandflash_block_erase(chip,page1);

	return 0;
}

int nandflash_mark_bad_block(int chip, int block)
{
	int error;
	char dummy=0;

	error |= nandflash_page_program(chip, 2048, block<<6,&dummy, 1);
	error |= nandflash_page_program(chip, 2048, block<<6+1,&dummy, 1);

	return error;
}

/* Identifying Initial Invalid Block(s) */

int nandflash_check_blocks(int chip, char bitmap[], int start_block, int end_block)
{
	int i;
	char invalid_flag[2];
	for(i=start_block;i<end_block;i++){
		nandflash_read(chip,2048,i<<6,invalid_flag,1);
		nandflash_read(chip,2048,i<<6+1,invalid_flag+1,1);
		if(invalid_flag[0]!=0xff && invalid_flag[1]!=0xff){
			printf("Invalid block: %d\n", i);
			if(bitmap!=NULL) bitmap[i/8] |= 1<<(i%8);
		}
	}
	return 0;
}

int nandflash_dump(int chip, int page, int offset, int length)
{
	int i;
	const int wrap=16;
	char *buf=(char *)malloc(length);
	if(buf==NULL){
		printf("%s: malloc failed\n", __func__);
		return -1;
	}
	nandflash_read(chip, offset, page, buf, length);
	printf("dumping page %d, offset %d, length %d\n", page, offset, length);
	for(i=offset/wrap*wrap;i<offset+length;i++){
		if(i%wrap == 0) printf("\n0x%08X:  ", i);
		if(i>=offset) printf("%02X ",buf[i-offset]);
		else printf("   ");
	}
	printf("\n\n");
	free(buf);
	return 0;
}

int nandflash_checkff(int chip, int offset, int page, int length)
{
	int i;
	int ret=0;
	char buf[2112];

	if(length>2112) length=2112;

	if(buf==NULL){
		printf("%s: malloc failed\n", __func__);
		return -1;
	}
	nandflash_read(chip, offset, page, buf, length);
	for(i=0;i<length;i++){
		if(buf[i]!=0xff){
			ret=1;
			break;
		}
	}
	return ret;
}

int nandflash_erase_all(int chip, int start_block, int end_block, int check)
{
	int i;
	for(i=start_block;i<end_block;i++){
		if(nandflash_block_erase(chip, i<<6)){
			printf("block %d erase error\n", i);
			if(nandflash_mark_bad_block(chip, i)){
				printf("block %d mark error\n",i);
			}
			/* erase error, so skip check */
			continue;
		}

		if(check){
			int page;
			for(page=i<<6;page<(i+1)<<6;page++){
				if(nandflash_checkff(chip,0,page,2112)){
					printf("block %d check ff error\n");
				}
			}
		}
	}
	return 0;
}

int nandflash_test_all(){
	int i;
	nandflash_init();
	srand((unsigned int)time(NULL));
	for(i=0;i<6;i++){
		nandflash_self_test(i);
	}
}

int nandflash_stress_test(){
}
