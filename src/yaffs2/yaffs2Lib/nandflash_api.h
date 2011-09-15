#ifndef _NANDFLASH_API_H_
#define _NANDFLASH_API_H_

int nandflash_page_program(int chip ,int col, int row, char buf[], int length);
int nandflash_read(int chip, int col, int row, char buf[], int length);
int nandflash_block_erase(int chip, int row);

#endif
