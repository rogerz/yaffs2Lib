#include "yaffs2LibP.h"
#include "yaffs_vxif.h"
#include "stat.h"
#include "nandflash.h"

extern	int	sys_get_board_addr();
extern int ROWS_PER_CHIP;

/* detect flash array type */
int flashArrayDetect(FLASH_ARRAY_INFO *fa){
	FLASH_DEVICE_ID flashID;
	int i;
	if(fa==NULL){
		T(YAFFS_TRACE_ERROR, (TSTR("Invalid flash array pointer %p" TENDSTR), fa));
		return -1;
	}
	fa->device_id=0;
	fa->maker_code=0;
	fa->nChips=0;

	for(i=0;i<6;i++){
		/* detect nandflash type */
		nandflash_read_device_id(i, &flashID);

		/* detect ends */
		if(flashID.maker_code != NANDFLASH_SAMSUNG) break;
		
		if(!fa->device_id){	/* first flash chip */
			fa->device_id=flashID.device_code;
			fa->maker_code=flashID.maker_code;
			fa->nChips++;
		}else if(fa->device_id != flashID.device_code || fa->maker_code != flashID.maker_code){
			T(YAFFS_TRACE_ERROR, (TSTR("Mixed type of flash detected both %#x and %#x are present" TENDSTR),
						fa->device_id, flashID.device_code));
			break;
		}else{
			fa->nChips++;
		}
	}

	if(fa->nChips==0){
		T(YAFFS_TRACE_ERROR, (TSTR("No flash chips detected" TENDSTR)));
		return -1;
	}else{
		T(YAFFS_TRACE_TRACING, (TSTR("%d flash chips detected, device_id=%#x" TENDSTR),
				   fa->nChips, fa->device_id));
	}

	switch(fa->device_id){
		case K9K8G08U0A:
			fa->nBlocksPerChip=8192;
			ROWS_PER_CHIP=524288;
			break;
		case K9F1G08U0A:
			fa->nBlocksPerChip=1024;
			ROWS_PER_CHIP=65536;
			break;
		default:
			T(YAFFS_TRACE_ERROR, (TSTR("Unsupport flash type %#x." TENDSTR),
						fa->device_id));
			return -1;
	}

	fa->nBlocks = fa->nChips * fa->nBlocksPerChip;
	fa->nBytesPerChunk=2048;
	fa->spareBytesPerChunk=64;
	fa->nChunksPerBlock=64;
	return 0;
}

/* make yaffs fs */
int flashMakeFs(const FLASH_ARRAY_INFO *fa, const FLASH_FS_INFO *fs){
	int i, lastBlock=-1, startBlock=0, endBlock=0, totalBlocks=0;
	yaffs_Device *pDev;
	char *pName;

	if(fa==NULL||fs==NULL){
		T(YAFFS_TRACE_ERROR, (TSTR("Invalid pointer fa=%p, fs=%p" TENDSTR), fa, fs));
		return -1;
	}

	totalBlocks=fa->nBlocks;
	
	/* validate fs info */
	if(fs->nDevice==0||fs->nDevice>=FSTAB_MAX){
		T(YAFFS_TRACE_ERROR, (TSTR("Device number invalide %d" TENDSTR), fs->nDevice));
		return -1;
	}

	for(i=0;i<fs->nDevice;i++){
		startBlock=fs->fsTab[i].startBlock;
		endBlock=fs->fsTab[i].endBlock;
		pName=(char *)fs->fsTab[i].devName;
		if(startBlock<=lastBlock||endBlock<startBlock||endBlock>totalBlocks){
			T(YAFFS_TRACE_ERROR, (TSTR("Invalid device %d, start=%d, end=%d, last=%d, total=%d" TENDSTR), 
						i, startBlock, endBlock, lastBlock, totalBlocks));
			break;
		}
		lastBlock=endBlock;
		if(*pName=='\0'){
			/* set default name */
			sprintf(pName,"yaffs%d:",i);
		}
		pDev=(yaffs_Device *)malloc(sizeof(yaffs_Device));
		memset(pDev,0,sizeof(yaffs_Device));
		if(pDev==NULL){
			T(YAFFS_TRACE_ERROR, (TSTR("malloc pDev error" TENDSTR)));
		   	break;
		}
		pDev->nChunksPerBlock = fa->nChunksPerBlock;
		pDev->spareBytesPerChunk = fa->spareBytesPerChunk;
		pDev->totalBytesPerChunk = fa->nBytesPerChunk;
		pDev->startBlock = startBlock;
		pDev->endBlock = endBlock;
		pDev->nReservedBlocks = 4;
		pDev->useNANDECC = 1;
		pDev->nShortOpCaches = 5;
		pDev->isYaffs2 = 1;
		pDev->genericDevice = (void *) 0;
		pDev->writeChunkWithTagsToNAND = ynand_WriteChunkWithTagsToNAND;
		pDev->readChunkWithTagsFromNAND = ynand_ReadChunkWithTagsFromNAND;
		pDev->eraseBlockInNAND = ynand_EraseBlockInNAND;
		pDev->initialiseNAND = ynand_InitialiseNAND;
		pDev->markNANDBlockBad = ynand_MarkNANDBlockBad;
		pDev->queryNANDBlock = ynand_QueryNANDBlock;
		if(yaffsDevCreate(pName, pDev, 20)==ERROR){
			T(YAFFS_TRACE_ERROR, (TSTR("Create %s failed" TENDSTR), pName));
		}
		YYIELD();
	}
	return 0;
}

/* Default init routine
 * Create one device for each chip */
int flashDiskInit(){
	int i;
	FLASH_ARRAY_INFO flashArray;
	FLASH_FS_INFO flashFs;

	nandflash_init();
	yaffsLibInit(0);

	if(flashArrayDetect(&flashArray)==-1){
		T(YAFFS_TRACE_ERROR, (TSTR("flashArrayDetect failed" TENDSTR)));
		return -1;
	}

	flashFs.nDevice=flashArray.nChips;

	for(i=0;i<flashArray.nChips;i++){
		flashFs.fsTab[i].startBlock = i * flashArray.nBlocksPerChip;
		flashFs.fsTab[i].endBlock = (i+1) * flashArray.nBlocksPerChip - 1;
		memset(flashFs.fsTab[i].devName, 0, sizeof(flashFs.fsTab[i].devName));
		sprintf(flashFs.fsTab[i].devName, "%c%d:", 'c'+i, sys_get_board_addr());
	}

	if(flashMakeFs(&flashArray, &flashFs)==-1){
		T(YAFFS_TRACE_ERROR, (TSTR("flashMakeFs failed" TENDSTR)));
		return -1;
	};
}


/* show disk status */
int flashDiskShow(char *pDevName){
	char *dummy;
	yaffs_Device *dev;
	int i;

	YAFFS_VOLUME_DESC_ID pVolDesc=(YAFFS_VOLUME_DESC_ID)iosDevFind(pDevName, &dummy);
	if(pVolDesc==NULL || pVolDesc->magic!=YAFFS_MAGIC) return ERROR;
	
	dev=pVolDesc->pYaffsDev;
	printf("\n"
			"yaffs_Device pointer. %#x\n"
			"=======Geometry======\n"
			"nDataBytesPerChunk... %d\n"
			"nChunksPerBlock...... %d\n"
			"spareBytesPerChunk... %d\n"
			"startBlock........... %d\n"
			"endBlock............. %d\n"
			"nReservedBlocks...... %d\n"
			"=======Runtime=======\n"
			"isCheckpointed....... %d\n"
			"nFreeChunks.......... %d\n"
			"nErasedBlocks........ %d\n"
			"allocationBlock...... %d\n"
			"=====Statistics======\n"
			"nPageWrites.......... %d\n"
			"nPageReads........... %d\n"
			"nBlockErasures....... %d\n"
			"nGCCopies............ %d\n"
			"garbageCollections... %d\n"
			"passiveGarbageColl'ns %d\n"
			"\n",
			dev,
			dev->nDataBytesPerChunk,
			dev->nChunksPerBlock,
			dev->spareBytesPerChunk,
			dev->startBlock,
			dev->endBlock,
			dev->nReservedBlocks,

			dev->isCheckpointed,
			dev->nFreeChunks,
			dev->nErasedBlocks,
			dev->allocationBlock,
			
			dev->nPageWrites,
			dev->nPageReads,
			dev->nBlockErasures,
			dev->nGCCopies,
			dev->garbageCollections,
			dev->passiveGarbageCollections
		  );
}

/* format disk */
int flashDiskFormat(char *pDevName, int magic){
	char *dummy;
	yaffs_Device *pDev;
	int i;
	YAFFS_VOLUME_DESC_ID pVolDesc=(YAFFS_VOLUME_DESC_ID)iosDevFind(pDevName, &dummy);
	if(pVolDesc==NULL || pVolDesc->magic!=magic) return ERROR;

	if(yaffsVolUnmount(pVolDesc)==ERROR){
		printf("Unmount error!\nReboot needed after format!\n");
	}
	pDev=pVolDesc->pYaffsDev;
	printf("Erasing blocks.....\n ");
	for(i=pDev->startBlock;i<=pDev->endBlock;i++){
		pDev->eraseBlockInNAND(pDev, i);
		printf("\r%6d /%6d",i-pDev->startBlock+1,pDev->endBlock-pDev->startBlock+1);
	}
	printf("\nErase done\n");
	yaffsVolMount(pVolDesc);
	return OK;
}

int flashDiskSync(char *pDevName){
	char *dummy;
	yaffs_Device *pDev;

	YAFFS_VOLUME_DESC_ID pVolDesc=(YAFFS_VOLUME_DESC_ID)iosDevFind(pDevName, &dummy);
	if(pVolDesc==NULL || pVolDesc->magic!=YAFFS_MAGIC) return ERROR;

	pDev=pVolDesc->pYaffsDev;
	yaffs_FlushEntireDeviceCache(pDev);
	yaffs_CheckpointSave(pDev);
	return OK;
}
