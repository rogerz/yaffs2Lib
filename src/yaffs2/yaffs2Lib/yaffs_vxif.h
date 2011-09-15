/* yaffs vxworks interface */

#ifndef _YAFFS_VXIF_H_
#define _YAFFS_VXIF_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "yaffs_guts.h"

int ynand_WriteChunkWithTagsToNAND(yaffs_Device * dev, int chunkInNAND,
				      const __u8 * data,
				      const yaffs_ExtendedTags * tags) ;

int ynand_ReadChunkWithTagsFromNAND(yaffs_Device * dev, int chunkInNAND,
				       __u8 * data, yaffs_ExtendedTags * tags) ;

int ynand_MarkNANDBlockBad(struct yaffs_DeviceStruct *dev, int blockNo) ;


int ynand_QueryNANDBlock(struct yaffs_DeviceStruct *dev, int blockNo,
			    yaffs_BlockState * state, __u32 *sequenceNumber) ;

int ynand_EraseBlockInNAND(yaffs_Device * dev, int blockNumber) ;

int ynand_InitialiseNAND(yaffs_Device * dev) ;

#ifdef __cplusplus
}
#endif

#endif /* _YAFFS_VXIF_H_ */
