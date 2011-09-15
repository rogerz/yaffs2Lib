#include "vxWorks.h"
#include "yaffs2LibP.h"
#include "yaffs_guts.h"

int yaffsDrvNum = ERROR;

/* for debug only */

#define TTRACE TTRACE1
#define TTRACE1 printf("%d:%s called\n",__LINE__, __func__);
#define TTRACE2 logMsg("%d:%s called\n",__LINE__, __func__,0,0,0,0);

/* create yaffs device in VxWorks ios */

STATUS yaffsDevCreate(
		char *pDevName,	/* device name */
		yaffs_Device *pDev,
		int maxFiles	/* max number of simultaneously open files */
		){
	YAFFS_VOLUME_DESC_ID pVolDesc = NULL;

	/* install yaffs system as a driver if not yet */

	if(yaffsDrvNum == ERROR && yaffsLibInit(0) == ERROR) return ERROR;

	/* allocate device descriptor */

	pVolDesc = (YAFFS_VOLUME_DESC_ID) malloc(sizeof(*pVolDesc));
	if(pVolDesc == NULL) return ERROR;
	bzero((char *)pVolDesc, sizeof(*pVolDesc));

	/* init semaphores */

	pVolDesc->devSem = semMCreate(SEM_Q_PRIORITY | SEM_DELETE_SAFE);
	if(NULL == pVolDesc->devSem){
		YFREE((char *)pVolDesc);
		return (ERROR);
	}

	pVolDesc->shortSem = semMCreate(SEM_Q_PRIORITY|SEM_DELETE_SAFE);
	if(NULL == pVolDesc->shortSem){
		semDelete(pVolDesc->devSem);
		YFREE((char *)pVolDesc);
		return (ERROR);
	}

	/* Initialise file descriptors */

	maxFiles = maxFiles?maxFiles:YAFFS_NFILES_DEFAULT;
	pVolDesc->pFdList = (YAFFS_FILE_DESC_ID)YMALLOC(maxFiles*sizeof(struct YAFFS_FILE_DESC));

	if(NULL!=pVolDesc->pFdList){
		bzero((char*)pVolDesc->pFdList,(maxFiles*sizeof(*(pVolDesc->pFdList))));
		pVolDesc->maxFiles = maxFiles;
	}else{
		semDelete(pVolDesc->devSem);
		semDelete(pVolDesc->shortSem);
		YFREE((char *)pVolDesc);
		return (ERROR);
	}

	/* install the device */

	pVolDesc->magic = YAFFS_MAGIC;
	pVolDesc->pYaffsDev=pDev;

	if(iosDevAdd((void *)pVolDesc, pDevName, yaffsDrvNum) == ERROR){
		pVolDesc->magic = NONE;
		return ERROR;
	}

	/* auto mount */

	T(YAFFS_TRACE_OS,(TSTR("yaffs: Mounting %s" TENDSTR),pDevName));		
	if(yaffsVolMount(pVolDesc)==ERROR) return ERROR;

	return OK;
}

/* yaffsFdGet - get an available file descriptor
 *
 * This routine obtains a free yaffs file descriptor.
 *
 * RETURNS: Pointer to file descriptor, or NULL, if none available.
 *
 * ERRNO:
 * S_yaffs2Lib_NO_FREE_FILE_DESCRIPTORS
 *
 */

LOCAL YAFFS_FILE_DESC_ID yaffsFdGet(YAFFS_VOLUME_DESC_ID pVolDesc){
	int i;

	semTake(pVolDesc->devSem, WAIT_FOREVER);

	for(i=0;i<pVolDesc->maxFiles;i++){
		if(!pVolDesc->pFdList[i].inUse){
			memset(&pVolDesc->pFdList[i],0,sizeof(pVolDesc->pFdList[i]));
			pVolDesc->pFdList[i].inUse=1;
			pVolDesc->pFdList[i].pVolDesc=pVolDesc;
			semGive(pVolDesc->devSem);
			return &(pVolDesc->pFdList[i]);
		}
	}

	semGive(pVolDesc->devSem);
	errnoSet(S_yaffs2Lib_NO_FREE_FILE_DESCRIPTORS);
	return NULL;
}

/* yaffsFdFree - Free a file descriptor
 *
 * This routine marks a file descriptor as free and decreases
 * reference count of a referenced file handle.
 *
 * RETURNS: N/A.
 */

LOCAL void yaffsFdFree(YAFFS_FILE_DESC_ID pFd){
	YAFFS_VOLUME_DESC_ID pVolDesc = pFd->pVolDesc;
	assert(pFd!=NULL);
	semTake(pVolDesc->devSem, WAIT_FOREVER);
	pFd->inUse=0;
	pFd->obj=NULL;
	semGive(pVolDesc->devSem);
}

#if 0

LOCAL yaffs_Object *yaffsFollowLink(YAFFS_VOLUME_DESC_ID pVolDesc, yaffs_Object *obj,int symDepth){
	while(obj&&obj->vairantType==YAFFS_OBJECT_TYPE_SYMLINK){
		YCHAR *alias=obj->variant.symLinkVariant.alias;
		if(strchr(YAFFS_PATH_DIVIDERS, *alias)){
			obj = yaffsFindObject(pVolDesc, NULL,alias,symDepth++);
		}else{
			obj = yaffsFindObject(pVolDesc, obj->parent,alias,symDepth++);
		}
	}
	return obj;
}

#endif 

LOCAL yaffs_Object * yaffsFindDirectory(
		YAFFS_VOLUME_DESC_ID pVolDesc, 
		yaffs_Object *startDir, 
		const YCHAR *path,
		YCHAR **name 	/* save extracted name */
		){
	yaffs_Object *dir;
	YCHAR *restOfPath;
	YCHAR str[YAFFS_MAX_NAME_LENGTH+1];
	int i;

	if(startDir)
	{
		dir = startDir;
		restOfPath = (YCHAR *)path;
	}
	else
	{
		dir = pVolDesc->pYaffsDev->rootDir;
		restOfPath = (YCHAR *)path;
	}

	while(dir)
	{
		// parse off /.
		// curve ball: also throw away surplus '/'
		// eg. "/ram/x////ff" gets treated the same as "/ram/x/ff"
		while(*restOfPath && strchr(YAFFS_PATH_DIVIDERS, *restOfPath))
		{
			restOfPath++; // get rid of '/'
		}

		*name = restOfPath;
		i = 0;

		while(*restOfPath && !strchr(YAFFS_PATH_DIVIDERS, *restOfPath))
		{
			if (i < YAFFS_MAX_NAME_LENGTH)
			{
				str[i] = *restOfPath;
				str[i+1] = '\0';
				i++;
			}
			restOfPath++;
		}

		if(!*restOfPath)
		{
			// got to the end of the string
			return dir;
		}
		else
		{
			if(yaffs_strcmp(str,_Y(".")) == 0)
			{
				// Do nothing
			}
			else if(yaffs_strcmp(str,_Y("..")) == 0)
			{
				dir = dir->parent;
			}
			else
			{
				dir = yaffs_FindObjectByName(dir,str);

				if(dir && dir->variantType != YAFFS_OBJECT_TYPE_DIRECTORY)
				{
					dir = NULL;
				}
			}
		}
	}
	// directory did not exist.
	return NULL;
}

LOCAL yaffs_Object * yaffsFindObject(
		YAFFS_VOLUME_DESC_ID pVolDesc,
	   	yaffs_Object *relativeDirectory,
	   	const YCHAR *path
	   	){
	yaffs_Object *dir;
	YCHAR *name;

	dir=yaffsFindDirectory(pVolDesc, relativeDirectory, path, &name);

	if(dir&&*name){
		return yaffs_FindObjectByName(dir,name);
	}
	return dir;
}

/*
 * yaffsDelete - delete a yaffs file/directory
 */

LOCAL STATUS yaffsDelete(
		YAFFS_VOLUME_DESC_ID	pVolDesc,
		char	*pPath){
	int result;
	yaffs_Object *dir=NULL;
	yaffs_Object *obj=NULL;
	char *name;
	YAFFS_FILE_DESC_ID pFd;

	semTake(pVolDesc->devSem, WAIT_FOREVER);
	obj = yaffsFindObject(pVolDesc, NULL, pPath);
	dir = yaffsFindDirectory(pVolDesc, NULL, pPath, &name);
	if(!dir){
		errnoSet(S_yaffs2Lib_ILLEGAL_PATH);
	}else if(!obj){
		errnoSet(S_yaffs2Lib_FILE_NOT_FOUND);
	}else{
		result = yaffs_Unlink(dir,name);
		if(result == YAFFS_FAIL && obj->variantType == YAFFS_OBJECT_TYPE_DIRECTORY){
			errnoSet(S_yaffs2Lib_DIR_NOT_EMPTY);
		}
	}

	semGive(pVolDesc->devSem);
	return result==YAFFS_OK?OK:ERROR;
}

/* mount a yaffs volume */
STATUS yaffsVolUnmount(YAFFS_VOLUME_DESC_ID pVolDesc){
	yaffs_Device *dev=NULL;
	semTake(pVolDesc->devSem, WAIT_FOREVER);
	dev=pVolDesc->pYaffsDev;

	if(dev->isMounted){
		int i;
		int inUse;
		yaffs_FlushEntireDeviceCache(dev);
		yaffs_CheckpointSave(dev);
		inUse=0;
		for (i=0;i<pVolDesc->maxFiles;i++){
			if(pVolDesc->pFdList[i].inUse && pVolDesc->pFdList[i].obj->myDev==dev){
				inUse=1;
			}
		}
		if(!inUse){
			yaffs_Deinitialise(dev);
		}else{
			semGive(pVolDesc->devSem);
			return ERROR;
		}
	}
	semGive(pVolDesc->devSem);
	return OK;
}

STATUS yaffsVolMount(YAFFS_VOLUME_DESC_ID pVolDesc){
	int result;
	semTake(pVolDesc->devSem, WAIT_FOREVER);
	if(!pVolDesc->pYaffsDev->isMounted){
		result = yaffs_GutsInitialise(pVolDesc->pYaffsDev);
		if(result == YAFFS_FAIL){
			semGive(pVolDesc->devSem);
			return ERROR;
		}
	}
	semGive(pVolDesc->devSem);
	return OK;
}

/* yaffsOpen - open a file on yaffs volumn
 * flags - O_RDONLY/O_WRONLY/O_RDWR/O_CREAT/O_TRUNC
 * ignore O_CREAT if already exist
 * mode - FSTAT_DIR
 */

LOCAL YAFFS_FILE_DESC_ID yaffsOpen(
		YAFFS_VOLUME_DESC_ID	pVolDesc,
		char	*pPath,
		int		flags,
		int 	mode){
	YAFFS_FILE_DESC_ID pFd=NULL;
	yaffs_Object *obj = NULL;
	yaffs_Object *dir = NULL;
	BOOL devSemOwned;
	BOOL error=FALSE;
	BOOL alreadyOpen, alreadyExclusive, openDenied=FALSE;
	YCHAR *name;
	int i;

	if(pVolDesc == NULL || pVolDesc->magic != YAFFS_MAGIC){
		errnoSet(S_yaffs2Lib_INVALID_PARAMETER);
		return NULL;
	}

	if(pPath == NULL || strlen(pPath) > PATH_MAX){
		errnoSet(S_yaffs2Lib_ILLEGAL_PATH);
		return NULL;
	}

	if(FALSE == pVolDesc->pYaffsDev->isMounted){
		if(yaffsVolMount(pVolDesc)==ERROR)
			goto ret;
	}

	/* only regular file and directory creation are supported */
	pFd=yaffsFdGet(pVolDesc);
	if(pFd==NULL){
		errnoSet(S_yaffs2Lib_NO_FREE_FILE_DESCRIPTORS);
		goto ret;
	}

	semTake(pVolDesc->devSem, WAIT_FOREVER);
	devSemOwned = TRUE;

	obj=yaffsFindObject(pVolDesc, NULL, pPath);
/* TODO: don't support symlink
	if(obj&&obj->variantType == YAFFS_OBJECT_TYPE_SYMLINK) obj=yaffsfs_FollowLink(obj,1);
*/
	if(obj){
		/* obj exist */
		if(obj->variantType == YAFFS_OBJECT_TYPE_DIRECTORY){
			pFd->obj=obj;
		}else if(obj && obj->variantType == YAFFS_OBJECT_TYPE_FILE){
			/* open file */
			/* check if the object is already in use */
			for(i=0;i<pVolDesc->maxFiles;i++){
				if(pFd!=&pVolDesc->pFdList[i] && pVolDesc->pFdList[i].inUse && obj==pVolDesc->pFdList[i].obj){
					alreadyOpen = TRUE;
					if(pVolDesc->pFdList[i].exclusive) alreadyExclusive = TRUE;
				}
			}
			/* check flags */
			if(((flags&O_EXCL)&&alreadyOpen)||alreadyExclusive) openDenied = TRUE;
			if((flags&O_EXCL) && (flags&O_CREAT)){
				openDenied = TRUE;
			}
#if 0
/* vxworks doesn't support open mode */
			if((flags&(O_RDWR|O_WRONLY))==0 && !(obj->yst_mode&S_IREAD)) openDenied = TRUE;	/* ie O_RDONLY */
			if((flags&O_RDWR) && !(obj->yst_mode&S_IREAD)) openDenied = TRUE;
			if((flags&(O_RDWR|O_WRONLY)) && !(obj->yst_mode&S_IWRITE)) openDenied = TRUE;
#endif
		}else{
			/* TODO: non-support type */
		}
	}else{	/* !obj */
		/* obj non-exist */
		if(flags & O_CREAT){
			if(mode&FSTAT_DIR){
				yaffs_Object *parent=NULL;
				yaffs_Object *dir=NULL;
				parent=yaffsFindDirectory(pVolDesc,NULL,pPath,&name);
				if(parent){
					obj=yaffs_MknodDirectory(parent,name,mode|YAFFS_DFLT_DIR_MASK,0,0);
				}
				if(!obj){
					errnoSet(S_yaffs2Lib_FILE_NOT_FOUND);
					error=TRUE;
				}
			}else{	/* !FSTAT_DIR */
				/* mkfile */
				dir=yaffsFindDirectory(pVolDesc, NULL, pPath, &name);
				if(dir){
					obj=yaffs_MknodFile(dir,name,mode|YAFFS_DFLT_FILE_MASK,0,0);
				}else{
					errnoSet(S_yaffs2Lib_NOT_DIRECTORY);
					error=TRUE;
				}
			}
		}else{	/* O_CREAT */
			errnoSet(S_yaffs2Lib_FILE_NOT_FOUND);
			error=TRUE;
		}
	}

	if(obj&&!openDenied){
		pFd->obj=obj;
		pFd->inUse=1;
		pFd->readOnly=(flags&(O_WRONLY|O_RDWR))?0:1;
		pFd->append=(flags&O_APPEND)?1:0;
		pFd->exclusive=(flags&O_EXCL)?1:0;
		pFd->position=0;
		pFd->obj->inUse++;
		if((flags&O_TRUNC)&& !pFd->readOnly) yaffs_ResizeFile(obj, 0);
	}

ret:
	if(devSemOwned){
		semGive(pVolDesc->devSem);
	}
	if(error||openDenied){
		if(pFd!=NULL) yaffsFdFree(pFd);
		return (void *)ERROR;
	}

	return pFd;
}

/*
 * yaffsClose - close a yaffs file
 *
 * This routine closes the specified yaffs file.
 * If file reference is smaller than zero and marked unlinked, the file will be deleted
 *
 * RETURNS: OK, or ERROR if directory couldn't be flushed
 * or entry couldn't be found.
 *
 * ERRNO:
 * S_yaffs2Lib_INVALID_PARAMETER
 * S_yaffs2Lib_DELETED
 * S_yaffs2Lib_FD_OBSOLETE
 */

LOCAL STATUS yaffsClose(
		YAFFS_FILE_DESC_ID pFd) {
	YAFFS_VOLUME_DESC_ID pVolDesc;

	if(pFd==NULL||pFd==(void *)ERROR||!pFd->inUse||pFd->pVolDesc->magic!=YAFFS_MAGIC){
		assert(FALSE);
		errnoSet(S_yaffs2Lib_INVALID_PARAMETER);
		return ERROR;
	}

	assert(pFd-pFd->pVolDesc->pFdList<pFd->pVolDesc->maxFiles);
	pVolDesc=pFd->pVolDesc;

	semTake(pVolDesc->devSem, WAIT_FOREVER);

	/* flush file content and update time */

	yaffs_FlushFile(pFd->obj,1);

	/* delete file if it is not in use and unlinked */

	pFd->obj->inUse--;
	if(pFd->obj->inUse<=0 && pFd->obj->unlinked){
		errnoSet(S_yaffs2Lib_DELETED);
		yaffs_DeleteFile(pFd->obj);
	}

	/* free the file descriptor */

	yaffsFdFree(pFd);

	/* Auto save checkpoint when closing */
	if(!pVolDesc->pYaffsDev->isCheckpointed){
		yaffs_FlushEntireDeviceCache(pVolDesc->pYaffsDev);
		yaffs_CheckpointSave(pVolDesc->pYaffsDev);
	}

	semGive(pVolDesc->devSem);
	return OK;
}

/* yaffsCreate - create a yaffs file
 *
 * this routine creats a file with the specific name and opens it
 * with specified flags.
 * If the file already exists, it is truncated to zero length, but not
 * actually created.
 * A yaffs file descriptor is initialized for the file.
 *
 * RETURNS: Pointer to yaffs file descriptor
 * or ERROR if error in create.
 */

LOCAL YAFFS_FILE_DESC_ID yaffsCreate(
		YAFFS_VOLUME_DESC_ID	pVolDesc,
		char	*pName,
		int	flags){
	/* create file via yaffsOpen */
	return yaffsOpen(pVolDesc, pName, flags|O_CREAT|O_TRUNC, S_IFREG);

	/* TODO: maybe update date info here. see dosFsLib.c */
}

/*******************************************************************************
 *
 * yaffsRead - read a file.
 *
 * This routine reads the requested number of bytes from a file.
 *
 * RETURNS: number of bytes successfully read, or 0 if EOF,
 * or ERROR if file is unreadable.
 */


LOCAL int yaffsRead(YAFFS_FILE_DESC_ID pFd, char *pBuf, int maxBytes){
	yaffs_Object *obj=NULL;
	YAFFS_VOLUME_DESC_ID pVolDesc;
	int pos=0;
	int nRead = -1;
	unsigned int maxRead;

	if(pFd==NULL){
	   	errnoSet(S_yaffs2Lib_INVALID_PARAMETER);
		return ERROR;
	}
	
	pVolDesc=pFd->pVolDesc;
	obj=pFd->obj;
	if(pVolDesc==NULL||obj==NULL){
		errnoSet(S_yaffs2Lib_INVALID_PARAMETER);
		return ERROR;
	}

	semTake(pVolDesc->devSem, WAIT_FOREVER);
	
	/* Get max readable bytes */

	pos=pFd->position;
	if(yaffs_GetObjectFileLength(obj) > pos){
		maxRead=yaffs_GetObjectFileLength(obj)-pos;
	}
	else{
		maxRead = 0;
	}

	/* sorry we have only maxRead bytes */
	if(maxBytes>maxRead){
		maxBytes=maxRead;
	}

	if(maxBytes>0){
		nRead=yaffs_ReadDataFromFile(obj,pBuf,pos,maxBytes);
		if(nRead>=0){
			pFd->position = pos+nRead;
		}else{
			YBUG();
		}
	}else{
		nRead=0;
	}

	semGive(pVolDesc->devSem);
	return (nRead>=0?nRead:ERROR);
}

/*
 * yaffsWrite - write a file.
 *
 * This routine writes the requested number of bytes to a file.
 *
 * RETURNS: number of bytes successfully written,
 * or ERROR if file is unwritable.
 */

LOCAL int yaffsWrite(YAFFS_FILE_DESC_ID pFd, char *pBuf, int maxBytes){
	yaffs_Object *obj=NULL;
	YAFFS_VOLUME_DESC_ID pVolDesc;
	int pos=0;
	int nWritten = -1;
	unsigned int writeThrough=0;

	if(pFd==NULL){
	   	errnoSet(S_yaffs2Lib_INVALID_PARAMETER);
		return ERROR;
	}
	
	pVolDesc=pFd->pVolDesc;
	obj=pFd->obj;
	if(pVolDesc==NULL||obj==NULL){
		errnoSet(S_yaffs2Lib_INVALID_PARAMETER);
		return ERROR;
	}

	semTake(pVolDesc->devSem, WAIT_FOREVER);

	if(pFd->append){
		pos=yaffs_GetObjectFileLength(obj);
	}else{
		pos=pFd->position;
	}
	
	nWritten=yaffs_WriteDataToFile(obj,pBuf,pos,maxBytes,writeThrough);

	if(nWritten>=0){
		pFd->position = pos+nWritten;
	}else{
		YBUG();
	}

	semGive(pVolDesc->devSem);

	return(nWritten>=0)?nWritten:ERROR;
}

/*******************************************************************************
*
* yaffsRename - change name of yaffs file
*
* This routine changes the name of the specified file to the specified
* new name.
*
* RETURNS: OK, or ERROR if pNewPath already in use, 
* or unable to write out new directory info.
*
* ERRNO:
* S_yaffs2Lib_INVALID_PARAMETER
* S_yaffs2Lib_NOT_SAME_VOLUME
*/

LOCAL STATUS yaffsRename(YAFFS_FILE_DESC_ID pFd, char *pNewName){
	yaffs_Object *olddir,*newdir;
	char oldname[YAFFS_MAX_NAME_LENGTH+1];
	char *newname;
	STATUS result=YAFFS_FAIL;
	int renameAllowed = 1;
	yaffs_Object *obj;
	YAFFS_VOLUME_DESC_ID pVolDesc;
	u_char newPath[PATH_MAX+1];

	if(pFd==NULL||pNewName==NULL){
		errnoSet(S_yaffs2Lib_INVALID_PARAMETER);
		return ERROR;
	}

	if (ioFullFileNameGet (pNewName, (DEV_HDR **) &pVolDesc, newPath) != OK)
	{
		return (ERROR);
	}

	/* Old and new devices must be the same */

	if ( pVolDesc != pFd->pVolDesc )
	{
		errnoSet( S_yaffs2Lib_NOT_SAME_VOLUME );
		return (ERROR);
	}	

	obj=pFd->obj;
	pVolDesc=pFd->pVolDesc;

	olddir=pFd->obj->parent;
	yaffs_GetObjectName(obj, oldname, YAFFS_MAX_NAME_LENGTH);
	newdir=yaffsFindDirectory(pVolDesc,NULL,newPath,&newname);

	if(!olddir||!newdir){
		errnoSet(S_yaffs2Lib_FILE_NOT_FOUND);
		renameAllowed = 0;
	}else if(olddir->myDev!=newdir->myDev){
		errnoSet(S_yaffs2Lib_NOT_SAME_VOLUME);
		renameAllowed = 0;
	}else if(obj && obj->variantType==YAFFS_OBJECT_TYPE_DIRECTORY){
		/* It is a directory check that it is not being renamed
		 * to being its own decendent
		 * Do this by tracing from the new directory back to the root , checking for obj
		 */
		yaffs_Object *tmp=newdir;
		while(renameAllowed && tmp){
			if(tmp==obj) renameAllowed = 0;
			tmp=tmp->parent;
		}
		if(!renameAllowed) errnoSet(S_yaffs2Lib_ILLEGAL_PATH);
	}

	if(renameAllowed){
		result = yaffs_RenameObject(olddir,oldname,newdir,newname);
	}

	return result==YAFFS_OK?OK:ERROR;
}

/***************************************************************************
 *
 * yaffsSeek - change file's current character position
 *
 * This routine sets the specified file's current character position to
 * the specified position.  This only changes the pointer, doesn't affect
 * the hardware at all.
 *
 * If the new offset pasts the end-of-file (EOF), attempts to read data
 * at this location will fail (return 0 bytes).
 *
 * RETURNS: OK, or ERROR if invalid file position.
 *
 * ERRNO:
 * S_yaffsLib_NOT_FILE
 *
 */

LOCAL STATUS yaffsSeek(YAFFS_FILE_DESC_ID pFd, fsize_t newPos){
	yaffs_Object *obj;
	if(newPos>=0){
		pFd->position=newPos;
	}else{
		return ERROR;
	}
	return OK;
}

/*******************************************************************************
 *
 * dosFsStatGet - get file status (directory entry data)
 *
 * This routine is called via an ioctl() call, using the FIOFSTATGET
 * function code.  The passed stat structure is filled, using data
 * obtained from the directory entry which describes the file.
 *
 * RETURNS: OK or ERROR if disk access error.
 *
 */

LOCAL STATUS yaffsStatGet(YAFFS_FILE_DESC_ID pFd, struct stat *pStat){
	yaffs_Object *obj;

	if(pFd==NULL||pFd->obj==NULL||pStat==NULL){
		errnoSet(S_yaffs2Lib_INVALID_PARAMETER);
		return ERROR;
	}

	obj=yaffs_GetEquivalentObject(pFd->obj);

	pStat->st_dev=(int)obj->myDev->genericDevice;
	pStat->st_ino=obj->objectId;
	pStat->st_mode=obj->yst_mode & ~S_IFMT;
	if(obj->variantType==YAFFS_OBJECT_TYPE_DIRECTORY){
		pStat->st_mode |= S_IFDIR;
	}else if(obj->variantType==YAFFS_OBJECT_TYPE_SYMLINK){
		pStat->st_mode |= S_IFLNK;
	}else if(obj->variantType==YAFFS_OBJECT_TYPE_FILE){
		pStat->st_mode |= S_IFREG;
	}
	pStat->st_nlink=yaffs_GetObjectLinkCount(obj);
	pStat->st_uid=0;
	pStat->st_gid=0;
	pStat->st_rdev = obj->yst_rdev;
	pStat->st_size=yaffs_GetObjectFileLength(obj);
	pStat->st_atime=obj->yst_atime;
	pStat->st_ctime=obj->yst_ctime;
	pStat->st_mtime=obj->yst_mtime;
	pStat->st_blksize = obj->myDev->nDataBytesPerChunk;
	pStat->st_blocks=(pStat->st_size+pStat->st_blksize-1)/pStat->st_blksize;

	return OK;
}

/******************************************************************************
 *
 * yaffsFSStatGet - get statistics about entire file system
 *
 * Used by fstatfs() and statfs() to get information about a file
 * system.  Called through yaffsIoctl() with FIOFSTATFSGET code.
 *
 * RETURNS:  OK.
 */
LOCAL STATUS yaffsFsStatGet(YAFFS_FILE_DESC_ID pFd, struct statfs *pStat){
	yaffs_Device *dev;

	if(pFd==NULL||pFd->obj==NULL||pFd->obj->myDev==NULL||pStat==NULL){
		errnoSet(S_yaffs2Lib_INVALID_PARAMETER);
		return ERROR;
	}

	dev=pFd->obj->myDev;
	pStat->f_type=0;
	pStat->f_bsize=dev->nDataBytesPerChunk;
	pStat->f_blocks=dev->endBlock-dev->startBlock+1;
	pStat->f_bfree=yaffs_GetNumberOfFreeChunks(dev);
	pStat->f_bavail=pStat->f_bfree;
	pStat->f_files=-1;	/* not supported */
	pStat->f_ffree=-1;

	return OK;
}

/* goto next entry */

LOCAL void yaffsDirAdvance(DIR *pDir){
	if(pDir==NULL||pDir->dd_cookie==0){
		YBUG();
		errnoSet(S_yaffs2Lib_INVALID_PARAMETER);
	}else{
		(struct ylist_head *)(pDir->dd_cookie)=((struct ylist_head *)(pDir->dd_cookie))->next;
	}
	return;
}

/*******************************************************************************
 *
 * yaffsIoctl - do device specific control function
 *
 * This routine performs the following ioctl functions.
 *
 * RETURNS: OK or function specific 
 * or ERROR if function failed or driver returned error
 *
 */

LOCAL STATUS yaffsIoctl(
		YAFFS_FILE_DESC_ID	pFd,
		int	function,
		int arg){

	STATUS retVal;
	fsize_t buf64=0;
	YAFFS_VOLUME_DESC_ID pVolDesc;

	if(pFd==NULL){
		errnoSet(S_yaffs2Lib_INVALID_PARAMETER);
		return ERROR;
	}
	
	pVolDesc=pFd->pVolDesc;

	switch(function)
	{
		case FIORENAME:
		case FIOMOVE:
			retVal = yaffsRename(pFd, (char *)arg);
			goto ioctlRet;
		case FIOSEEK:
			{
				buf64=(fsize_t)arg;
				/* TODO: vxworks uses fsize_t, yaffs uses off_t */
				retVal = yaffsSeek(pFd, (fsize_t)arg);
				goto ioctlRet;
			}
		case FIOSEEK64:
			{
				buf64=*(fsize_t*)arg;
				retVal = yaffsSeek(pFd, (fsize_t)arg);
				goto ioctlRet;
			}
		case FIOWHERE:
			{
				retVal = pFd->position;
				goto ioctlRet;
			}
		case FIOWHERE64:
			{
				if((void *)arg == NULL){
					errnoSet(S_yaffs2Lib_INVALID_PARAMETER);
					retVal=ERROR;
				} else {
					*(fsize_t *)arg = (pFd->position);
					retVal=OK;
				}
				goto ioctlRet;
			}
		case FIOSYNC:
			{
				yaffs_Device * dev = pFd->obj->myDev;
				semTake(pVolDesc->devSem,WAIT_FOREVER);
				if(dev->isMounted)
				{
					yaffs_FlushEntireDeviceCache(dev);
					yaffs_CheckpointSave(dev);
					retVal = OK;
				}
				else
				{
					//todo error - not mounted.
					errnoSet(S_yaffs2Lib_VOLUME_NOT_AVAILABLE);
					retVal=ERROR;
				}
				semGive(pVolDesc->devSem);
				goto ioctlRet;
			}
		case FIOFLUSH:
			{
				semTake(pVolDesc->devSem,WAIT_FOREVER);
				retVal=yaffs_FlushFile(pFd->obj,1);
				semGive(pVolDesc->devSem);
				goto ioctlRet;
			}
		case FIONREAD:
			/* unread */
			{
				buf64 = pFd->position;
				buf64 = buf64 < pFd->obj->variant.fileVariant.fileSize?
					pFd->obj->variant.fileVariant.fileSize-buf64:0;
				*(u_long*)arg = buf64;
				retVal = OK;
				goto ioctlRet;
			}
		case FIONREAD64:
			{
				buf64 = pFd->position;
				buf64 = buf64 < pFd->obj->variant.fileVariant.fileSize?
					pFd->obj->variant.fileVariant.fileSize-buf64:0;
				*(fsize_t *)arg = buf64;
				retVal = OK;
				goto ioctlRet;
			}
		case FIOUNMOUNT:
			{
				retVal = yaffsVolUnmount(pVolDesc);
				goto ioctlRet;
			}
		case FIONFREE:
			{
				yaffs_Device *dev;
				if((void *)arg == NULL){
					errnoSet(S_yaffs2Lib_INVALID_PARAMETER);
					retVal=ERROR;
					goto ioctlRet;
				}
				dev=pFd->obj->myDev;
				if(dev->isMounted){
					*(u_long *)arg = yaffs_GetNumberOfFreeChunks(dev) * dev->nDataBytesPerChunk;
					retVal=OK;
				}
				goto ioctlRet;
			}
		case FIONFREE64:
			{
				yaffs_Device *dev;
				if((void *)arg == NULL){
					errnoSet(S_yaffs2Lib_INVALID_PARAMETER);
					retVal=ERROR;
					goto ioctlRet;
				}
				dev=pFd->obj->myDev;
				if(pFd->obj->myDev->isMounted){
					*(fsize_t*)arg = yaffs_GetNumberOfFreeChunks(dev) * dev->nDataBytesPerChunk;
					retVal=OK;
				}
				goto ioctlRet;
			}
		case FIOMKDIR:
			{
				u_char path[PATH_MAX+1];
				DEV_HDR *pDevHdr;
				YAFFS_FILE_DESC_ID pFdNew;
				retVal=ERROR;

				if(ioFullFileNameGet((char *)arg,&pDevHdr, path)!=OK){
					errnoSet(S_yaffs2Lib_ILLEGAL_PATH);
				}else if((void *)pDevHdr!=(void *)pVolDesc){
					errnoSet(S_yaffs2Lib_NOT_SAME_VOLUME);
				}else{
					pFdNew=yaffsOpen(pVolDesc,(char *)path,O_CREAT,FSTAT_DIR);
					if(pFdNew!=(void *)ERROR){
						yaffsClose(pFdNew);
						retVal=OK;
					}
				}
				goto ioctlRet;
			}
		case FIORMDIR:
			{
				u_char path[PATH_MAX+1];
				DEV_HDR *pDevHdr;
				retVal=ERROR;

				if(ioFullFileNameGet((char *)arg,&pDevHdr, path)!=OK){
					errnoSet(S_yaffs2Lib_ILLEGAL_PATH);
				}else if((void *)pDevHdr!=(void *)pVolDesc){
					errnoSet(S_yaffs2Lib_NOT_SAME_VOLUME);
				}else{
					retVal=yaffsDelete(pVolDesc,(char *)path);
				}
				goto ioctlRet;
			}
			/*
		case FIOLABELGET:
		case FIOLABELSET:
		case FIOCONTIG:
		*/
		case FIOREADDIR:
			{
				DIR *pDir = (DIR *)arg;
				if((void *)arg == NULL){
					errnoSet(S_yaffs2Lib_INVALID_PARAMETER);
					retVal=ERROR;
					goto ioctlRet;
				}else if(!pFd->obj || pFd->obj->variantType != YAFFS_OBJECT_TYPE_DIRECTORY){
					errnoSet(S_yaffs2Lib_NOT_DIRECTORY);
					retVal=ERROR;
					goto ioctlRet;
				}

				if(pDir->dd_cookie==0){
					/* just opened */
					if(ylist_empty(&pFd->obj->variant.directoryVariant.children)){
						return ERROR;
					}else{
						/* init the cookie */
						(struct ylist_head *)(pDir->dd_cookie)=pFd->obj->variant.directoryVariant.children.next;
					}
				}
				{	/* now dd_cookie!=0 */
					/* read next */
					struct ylist_head *next=(struct ylist_head *)(pDir->dd_cookie);
					yaffs_Object *obj=ylist_entry(next, yaffs_Object, siblings);
					if(next==&pFd->obj->variant.directoryVariant.children){
					   retVal=ERROR;	/* dir end */
					}else{
						/* TODO long name may be truncated as YAFFS_MAX_NAME_LENGTH(255) > NAME_MAX(100) */
						yaffs_GetObjectName(obj, pDir->dd_dirent.d_name,NAME_MAX);
						if(yaffs_strlen(pDir->dd_dirent.d_name)==0){
							yaffs_strcpy(pDir->dd_dirent.d_name, _Y("zz"));
						}
						yaffsDirAdvance(pDir);
						retVal=OK;
					}
				} /* dd_cookie */
				goto ioctlRet;
			}	/* case FIOREADDIR */
		case FIOFSTATGET:
			{
				retVal=yaffsStatGet(pFd, (struct stat *)arg);
				goto ioctlRet;
			}
		case FIOFSTATFSGET:
			{
				retVal=yaffsFsStatGet(pFd, (struct statfs*)arg);
				goto ioctlRet;
			}
		case FIOTIMESET:
			/* ignore time set */
			goto ioctlRet;
		case FIOTRUNC:
		case FIOTRUNC64:
		case FIOCHKDSK:
		default:
			T(YAFFS_TRACE_OS,
					(TSTR("unsupport ioctl function %d" TENDSTR)));
			retVal = ERROR;
			goto ioctlRet;
	}

ioctlRet:
	return retVal;
}

/*******************************************************************************
 *
 * yaffsLibInit - prepare to use the yaffs library
 *
 * This routine initializes the dosFs library.
 * This routine installs yaffsLib as a 
 * driver in the I/O system driver table, and allocates and sets up
 * the necessary structures.
 * The driver number assigned to yaffsLib is placed
 * in the global variable <yaffsDrvNum>.
 *
 * RETURNS: OK or ERROR, if driver can not be installed.
 */

STATUS yaffsLibInit(int ignored){
	if(yaffsDrvNum != ERROR) return OK;

	yaffsDrvNum = iosDrvInstall(
			(FUNCPTR) yaffsCreate,
			(FUNCPTR) yaffsDelete,
			(FUNCPTR) yaffsOpen,
			(FUNCPTR) yaffsClose,
			(FUNCPTR) yaffsRead,
			(FUNCPTR) yaffsWrite,
			(FUNCPTR) yaffsIoctl
			);

	return (yaffsDrvNum==ERROR?ERROR:OK);
}
