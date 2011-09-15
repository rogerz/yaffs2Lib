/*
 * YAFFS: Yet Another Flash File System. A NAND-flash specific file system.
 *
 * Copyright (C) 2002-2007 Aleph One Ltd.
 *   for Toby Churchill Ltd and Brightstar Engineering
 *
 * Created by Charles Manning <charles@aleph1.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * yaffscfg.c  The configuration for yaffs.
 *
 * This file is intended to be modified to your requirements.
 * There is no need to redistribute this file.
 */

#include "yaffscfg.h"
#include <errno.h>
#include "yaffs_guts.h"
#include "yaffs_vxif.h"
#include "semLib.h"

unsigned yaffs_traceMask = YAFFS_TRACE_OS;
						   ;

SEM_ID yaffs_mutexSem;

static yaffs_Device nandDev[6];

static yaffsfs_DeviceConfiguration yaffsfs_config[] = {

	{ "/n0", &nandDev[0]},
	{ "/n1", &nandDev[1]},
	{ "/n2", &nandDev[2]},
	{ "/n3", &nandDev[3]},
	{ "/n4", &nandDev[4]},
	{ "/n5", &nandDev[5]},
	{(void *)0,(void *)0}
};


void yaffsfs_SetError(int err)
{
	/*Do whatever to set error*/
	errno = err;
}

void yaffsfs_Lock(void)
{
	semTake(yaffs_mutexSem, WAIT_FOREVER);
}

void yaffsfs_Unlock(void)
{
	semGive(yaffs_mutexSem);
}

__u32 yaffsfs_CurrentTime(void)
{
	return time(NULL);
}

void yaffsfs_LocalInitialisation(void)
{
	/* Define locking semaphore.*/
    yaffs_mutexSem= semMCreate ( SEM_Q_PRIORITY | SEM_DELETE_SAFE );
}

void yaffsInitialise(yaffsfs_DeviceConfiguration *cfg){
	while(cfg && cfg->prefix && cfg->dev)
	{
		if(	yaffsDevCreate(cfg->prefix, cfg->dev, 20) ==ERROR){
			T(YAFFS_TRACE_ERROR, (TSTR("Create %s failed" TENDSTR), cfg->prefix));
		};
		cfg++;
	}
}

int yaffs_StartUp(void)
{
	// Set up devices
	int i;
	nandflash_init();
	yaffsLibInit(0);
	// Stuff to configure YAFFS
	// Stuff to initialise anything special (eg lock semaphore).
	yaffsfs_LocalInitialisation();
	for(i=0;i<6;i++){
		nandDev[i].nDataBytesPerChunk = 2048;
		nandDev[i].nChunksPerBlock = 64;
		nandDev[i].spareBytesPerChunk = 64;
		nandDev[i].totalBytesPerChunk = 2048;
		nandDev[i].startBlock = 8192*i;
		nandDev[i].endBlock = 8192*(i+1)-1;
		nandDev[i].nReservedBlocks = 10;
		nandDev[i].useNANDECC = 1;
		nandDev[i].nShortOpCaches = 0;
		nandDev[i].isYaffs2 = 1;
		nandDev[i].genericDevice = (void *) 0;
		nandDev[i].writeChunkWithTagsToNAND = ynand_WriteChunkWithTagsToNAND;
		nandDev[i].readChunkWithTagsFromNAND = ynand_ReadChunkWithTagsFromNAND;
		nandDev[i].eraseBlockInNAND = ynand_EraseBlockInNAND;
		nandDev[i].initialiseNAND = ynand_InitialiseNAND;
		nandDev[i].markNANDBlockBad = ynand_MarkNANDBlockBad;
		nandDev[i].queryNANDBlock = ynand_QueryNANDBlock;
	}

	/* direct interface */
	yaffs_initialise(yaffsfs_config);

	/* native interface */
	yaffsInitialise(yaffsfs_config);

	return 0;
}

