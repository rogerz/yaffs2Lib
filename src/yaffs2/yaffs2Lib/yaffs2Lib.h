#ifndef _YAFFS2LIB_H_
#define _YAFFS2LIB_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct YAFFS_VOLUME_DESC * YAFFS_VOLUME_DESC_ID;

typedef struct{
	int device_id;
	int maker_code;
	int nBytesPerChunk;
	int spareBytesPerChunk;
	int nChunksPerBlock;
	int nBlocksPerChip;
	int nChips;
	int nBlocks;
}FLASH_ARRAY_INFO;

#define FSTAB_MAX 8

typedef struct {
	int nDevice;
	struct {
		char devName[32];
		int startBlock;
		int endBlock;
	}fsTab[FSTAB_MAX];
}FLASH_FS_INFO;

#define M_yaffs2Lib	(200<<16)	/* See vwModNum.h */

#define S_yaffs2Lib_32BIT_OVERFLOW		(M_yaffs2Lib |  1)
#define S_yaffs2Lib_DISK_FULL			(M_yaffs2Lib |  2)
#define S_yaffs2Lib_FILE_NOT_FOUND		(M_yaffs2Lib |  3)
#define S_yaffs2Lib_NO_FREE_FILE_DESCRIPTORS 	(M_yaffs2Lib |  4)
#define S_yaffs2Lib_NOT_FILE			(M_yaffs2Lib |  5)
#define S_yaffs2Lib_NOT_SAME_VOLUME	        (M_yaffs2Lib |  6)
#define S_yaffs2Lib_NOT_DIRECTORY		(M_yaffs2Lib |  7)
#define S_yaffs2Lib_DIR_NOT_EMPTY		(M_yaffs2Lib |  8)
#define S_yaffs2Lib_FILE_EXISTS			(M_yaffs2Lib |  9)
#define S_yaffs2Lib_INVALID_PARAMETER		(M_yaffs2Lib | 10)
#define S_yaffs2Lib_NAME_TOO_LONG		(M_yaffs2Lib | 11)
#define S_yaffs2Lib_UNSUPPORTED			(M_yaffs2Lib | 12)
#define S_yaffs2Lib_VOLUME_NOT_AVAILABLE		(M_yaffs2Lib | 13)
#define S_yaffs2Lib_INVALID_NUMBER_OF_BYTES	(M_yaffs2Lib | 14)
#define S_yaffs2Lib_ILLEGAL_NAME			(M_yaffs2Lib | 15)
#define S_yaffs2Lib_CANT_DEL_ROOT		(M_yaffs2Lib | 16)
#define S_yaffs2Lib_READ_ONLY			(M_yaffs2Lib | 17)
#define S_yaffs2Lib_ROOT_DIR_FULL		(M_yaffs2Lib | 18)
#define S_yaffs2Lib_NO_LABEL			(M_yaffs2Lib | 19)
#define S_yaffs2Lib_NO_CONTIG_SPACE		(M_yaffs2Lib | 20)
#define S_yaffs2Lib_FD_OBSOLETE			(M_yaffs2Lib | 21)
#define S_yaffs2Lib_DELETED			(M_yaffs2Lib | 22)
#define S_yaffs2Lib_INTERNAL_ERROR		(M_yaffs2Lib | 23)
#define S_yaffs2Lib_WRITE_ONLY			(M_yaffs2Lib | 24)
#define S_yaffs2Lib_ILLEGAL_PATH			(M_yaffs2Lib | 25)
#define S_yaffs2Lib_ACCESS_BEYOND_EOF		(M_yaffs2Lib | 26)
#define S_yaffs2Lib_DIR_READ_ONLY		(M_yaffs2Lib | 27)
#define S_yaffs2Lib_UNKNOWN_VOLUME_FORMAT	(M_yaffs2Lib | 28)
#define S_yaffs2Lib_ILLEGAL_CLUSTER_NUMBER	(M_yaffs2Lib | 29)


#ifdef __cplusplus
}
#endif

#endif /* _YAFFS2LIB_H_ */
