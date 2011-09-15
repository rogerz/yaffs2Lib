#include "yportenv.h"
#include "yaffs_vxif.h"
#include "yaffs_packedtags2.h"
#include "nandflash_api.h"

int ROWS_PER_CHIP=524288;
#define CHIP(chunk)	(chunk/ROWS_PER_CHIP)
#define ROW(chunk)	(chunk%ROWS_PER_CHIP)
#define TAG_OFFSET 0x10

int ynand_WriteChunkWithTagsToNAND(yaffs_Device * dev, int chunkInNAND,
				      const __u8 * data,
				      const yaffs_ExtendedTags * tags) {
	int i;
	char error=0;
	yaffs_PackedTags2 pt;

	T(YAFFS_TRACE_MTD,
			(TSTR("write chunk %d data %x tags %x" TENDSTR),chunkInNAND,(unsigned)data, (unsigned)tags));

	dev->nPageWrites++;

	if(data || tags){
		yaffs_PackTags2(&pt,tags);
		if(data){
			error |= nandflash_page_program(CHIP(chunkInNAND),0,ROW(chunkInNAND),
					(__u8*)data,dev->nDataBytesPerChunk);
		}

		if(tags){
			error |= nandflash_page_program(CHIP(chunkInNAND),dev->nDataBytesPerChunk+TAG_OFFSET,ROW(chunkInNAND),
					(__u8*)&pt,sizeof(pt));
		}

		if(error){
			T(YAFFS_TRACE_ERROR,
					(TSTR("Write error: chunk %d data %x tags %x" TENDSTR),chunkInNAND,(unsigned)data, (unsigned)tags));
			return YAFFS_FAIL;
		}
	}else{
		T(YAFFS_TRACE_ERROR,
				(TSTR("Invalid args: chunk %d data %x tags %x" TENDSTR),chunkInNAND,(unsigned)data, (unsigned)tags));
		return YAFFS_FAIL;
	}

	return YAFFS_OK;
}

int ynand_ReadChunkWithTagsFromNAND(yaffs_Device * dev, int chunkInNAND,
				       __u8 * data, yaffs_ExtendedTags * tags) {
	yaffs_PackedTags2 pt;
	char error=0;

	dev->nPageReads++;
	
	T(YAFFS_TRACE_MTD,
			(TSTR("read chunk %d data %x tags %x" TENDSTR),chunkInNAND,(unsigned)data, (unsigned)tags));
	if(data || tags){
		if(data){
			error |= nandflash_read(CHIP(chunkInNAND),0,ROW(chunkInNAND),
					data, dev->nDataBytesPerChunk);
		}
		if(tags){
			error |= nandflash_read(CHIP(chunkInNAND),dev->nDataBytesPerChunk+TAG_OFFSET,ROW(chunkInNAND),
					(__u8*)&pt, sizeof(pt));
			yaffs_UnpackTags2(tags, &pt);
		}

		if(error){
			T(YAFFS_TRACE_ERROR,
					(TSTR("Read error: chunk %d data %x tags %x" TENDSTR),chunkInNAND,(unsigned)data, (unsigned)tags));
			return YAFFS_FAIL;
		}
	}else{
		T(YAFFS_TRACE_ERROR,
				(TSTR("Invalid args: chunk %d data %x tags %x" TENDSTR),chunkInNAND,(unsigned)data, (unsigned)tags));
		return YAFFS_FAIL;
	}

	return YAFFS_OK;

}

int ynand_MarkNANDBlockBad(struct yaffs_DeviceStruct *dev, int blockNo) {
	char dummy=0;
	char error=0;

	T(YAFFS_TRACE_MTD,
			(TSTR("ynand_MarkNANDBlockBad %d" TENDSTR), blockNo));

	error |= nandflash_page_program(CHIP(blockNo*dev->nChunksPerBlock),
			dev->nDataBytesPerChunk,
			ROW(blockNo*dev->nChunksPerBlock), &dummy, 1);
	error |= nandflash_page_program(CHIP(blockNo*dev->nChunksPerBlock),
			dev->nDataBytesPerChunk,
			ROW(blockNo*dev->nChunksPerBlock)+1, &dummy, 1);

	if(error) return YAFFS_FAIL;

	return YAFFS_OK;
}

int ynand_QueryNANDBlock(struct yaffs_DeviceStruct *dev, int blockNo,
			    yaffs_BlockState * state, __u32 *sequenceNumber) {
	char bad;
	char dummy[2];
	char allff[]={0xff,0xff};

	T(YAFFS_TRACE_MTD,
			(TSTR("ynand_QueryNANDBlock %d" TENDSTR), blockNo));
	
	nandflash_read(CHIP(blockNo*dev->nChunksPerBlock),
			dev->nDataBytesPerChunk,
			ROW(blockNo*dev->nChunksPerBlock), &dummy[0], 1);

	nandflash_read(CHIP(blockNo*dev->nChunksPerBlock),
			dev->nDataBytesPerChunk,
			ROW(blockNo*dev->nChunksPerBlock)+1, &dummy[1], 1);

	bad=memcmp(dummy,allff,2);
	if(bad){
		T(YAFFS_TRACE_MTD, (TSTR("block is bad, bad flag = %x %x" TENDSTR),dummy[0],dummy[1]));

		*state = YAFFS_BLOCK_STATE_DEAD;
		*sequenceNumber = 0;
	} else {
		yaffs_ExtendedTags t;
		ynand_ReadChunkWithTagsFromNAND(dev,
						   blockNo *
						   dev->nChunksPerBlock, NULL,
						   &t);

		if (t.chunkUsed) {
			*sequenceNumber = t.sequenceNumber;
			*state = YAFFS_BLOCK_STATE_NEEDS_SCANNING;
		} else {
			*sequenceNumber = 0;
			*state = YAFFS_BLOCK_STATE_EMPTY;
		}
	}

	T(YAFFS_TRACE_MTD,
	  (TSTR("block query result seq %d state %d" TENDSTR), *sequenceNumber,
	   *state));

	if (bad == 0)
		return YAFFS_OK;
	else
		return YAFFS_FAIL;
}

int ynand_EraseBlockInNAND(yaffs_Device * dev, int blockNumber) {
	int ret;
	int page=blockNumber*dev->nChunksPerBlock;
	T(YAFFS_TRACE_MTD,
			(TSTR("erase block %d\n" TENDSTR), blockNumber));

	ret = nandflash_block_erase(CHIP(page),page);
	return ret==0?YAFFS_OK:YAFFS_FAIL;
}

int ynand_InitialiseNAND(yaffs_Device * dev) {
	
	return YAFFS_OK;
}
