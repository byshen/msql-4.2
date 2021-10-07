/*
** cpi.c   - Clusteres Page Index library (CPI)
**
** Copyright (c) 1998  Hughes Technologies Pty Ltd
**
** This library was written for Mini SQL 3.
**
*/

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h> 

#include <common/config.h>
#include <common/config_extras.h>
#include <common/portability.h>

#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif

#include "cpi_priv.h"

#define	_REG register

/*************************************************************************
**************************************************************************
**
**                        CACHE ROUTINES
**
**************************************************************************
*************************************************************************/

static void cacheInitialise(idx)
	cpi	*idx;
{
	char	*cp;
	_REG	cpi_ch	*cur;
	_REG	int	count;

	idx->cache = (char *)malloc(CPI_SBK(idx)->cacheSize * sizeof(cpi_ch));
	cp = idx->cache;
	for (count = 0; count < CPI_SBK(idx)->cacheSize; count++)
	{
		cur = (cpi_ch *)cp;
		cur->age = -1;
		cur->page = -1;
		cur->ptr = NULL;  
		cur->slot = count;
		cp += sizeof(cpi_ch);
	}
}



static cpi_ch *readPage(idx, pageNum)
	cpi	*idx;
	u_int	pageNum;
{
	char	*cp;
	int	slot,
		age,
		hash,
		oldHash;
	off_t	offset;
	static	cpi_cch accessHash[CPI_CCH_SIZE];
	_REG	int count;
	_REG	cpi_ch	*cur;

	/*
	** To speed up access to cached pages we "cache" the last 
	** accesses to the cache in a "cache cache hash".  Sounds wierd
	** but it doubles the overall performance of various ops.
	*/
	hash = pageNum % CPI_CCH_SIZE;
	if (accessHash[hash].pageNum == pageNum && accessHash[hash].idx == idx)
	{
		CPI_SBK(idx)->cacheLookups++;
		CPI_SBK(idx)->cacheHits++;
		cur = accessHash[hash].page;
		cur->age = 0;
		return(cur);
	}
	
	/*
	** Scan the cache for the page and keep note of any slots we
	** can use if we need to load the page
	*/
	cur = NULL;
	CPI_SBK(idx)->cacheLookups++;
	age = 0;
	slot = count = 0;
	cp = idx->cache;
	while(count < CPI_SBK(idx)->cacheSize)
	{
		cur = (cpi_ch *)cp;
		if (cur->page == pageNum)
		{
			cur->age = 0;
			CPI_SBK(idx)->cacheHits++;
			accessHash[hash].pageNum = pageNum;
			accessHash[hash].idx = idx;
			accessHash[hash].page = cur;
			return(cur);
		}
		if (cur->age == -1)
		{
			/* free slot */
			age = -1;
			slot = count;
			break;
		}
		cur->age++;
		if (cur->age > age)
		{
			age = cur->age;
			slot = count;
		}
		cp += sizeof(cpi_ch);
		count++;
	}

	/*
	** OK, we didn't find it in the cache so load it at the oldest
	** location (or empty location if we found an age of -1).
	*/
	cur = (cpi_ch*) (idx->cache + slot * sizeof(cpi_ch));
	if (age != -1)
	{
		if (cpiDebugCache)
		{
			printf("DEBUG  Unloading page %d from slot %d\n",
				cur->page, slot);
		}
		oldHash = cur->page % CPI_CCH_SIZE;
		if (accessHash[oldHash].pageNum == cur->page &&
			accessHash[oldHash].idx == idx)
		{
			accessHash[oldHash].idx = NULL;
		}
			
		munmap(cur->ptr, CPI_SBK(idx)->mapSize);
		cur->ptr = NULL;
	}
	if (cpiDebugCache)
	{
		printf("DEBUG  Cache load of page %d into slot %d\n", 
			pageNum, slot);
	}
	cur->age = 0;
	cur->page = pageNum;
	offset = (off_t) CPI_PAGE_OFFSET(idx, pageNum);
	cur->ptr = (caddr_t)mmap(NULL, CPI_SBK(idx)->mapSize, 
		(PROT_READ|PROT_WRITE), MAP_SHARED, idx->fd,  offset);
	accessHash[hash].pageNum = pageNum;
	accessHash[hash].idx = idx;
	accessHash[hash].page = cur;
	return(cur);
}


#define getCluster(idx, cachePtr, cluster, clusterPtr) { 		\
	(clusterPtr)->clusterNum = (cluster);				\
	(clusterPtr)->cacheSlot = cachePtr->slot;			\
	(clusterPtr)->numHeaders = (int *)cachePtr->ptr;		\
	(clusterPtr)->pageNum = (int *)(cachePtr->ptr + sizeof(u_int));	\
	(clusterPtr)->maxValue = cachePtr->ptr + (2 * sizeof(u_int));	\
}

#define getHeader(idx, cachePtr, cluster, header, headerPtr) {		\
	char	*basePtr;						\
	basePtr=cachePtr->ptr+CPI_CLS_SIZE(idx)+((header)*CPI_HDR_SIZE(idx));\
	(headerPtr)->numRecords = (int *)basePtr;			\
	(headerPtr)->pageNum = (int *)(basePtr + sizeof(int));		\
	(headerPtr)->maxValue = basePtr + (2 * sizeof(int));		\
	(headerPtr)->clusterNum = cluster;				\
	(headerPtr)->headerNum = header;				\
}

#define getRecord(idx, cachePtr, cluster, header, record, nodePtr) { 	\
	char	*basePtr; 						\
	basePtr = cachePtr->ptr + ((record) * CPI_REC_SIZE(idx));	\
	(nodePtr)->key = basePtr;					\
	(nodePtr)->data = *(u_int *)(basePtr + CPI_KEY_SIZE(idx));	\
	(nodePtr)->clusterNum = cluster;				\
	(nodePtr)->headerNum = header;					\
	(nodePtr)->recordNum = record;					\
}

/*************************************************************************
**************************************************************************
**
**                        DEBUG ROUTINES
**
**************************************************************************
*************************************************************************/


void cpiPrintIndexStats(idx)
	cpi	*idx;
{
        printf("CPI Index stats\n===============\n\n");
        printf("Path = %s\n",idx->path);
        printf("Version = %d\n",CPI_SBK(idx)->version);
        printf("Key Size = %d\n",CPI_SBK(idx)->keySize);
        printf("Rec Size = %d\n",(int)CPI_REC_SIZE(idx));
        printf("Page Size = %d\n",CPI_SBK(idx)->pageSize);
        printf("Records Per Page = %d\n",CPI_SBK(idx)->recsPerPage);
        printf("Headers Per Page = %d\n",CPI_SBK(idx)->hdrsPerPage);
        printf("Cache Size = %d\n",CPI_SBK(idx)->cacheSize);
        printf("Num Clusters = %d\n",CPI_SBK(idx)->numClusters);
        printf("Num Records = %d\n",CPI_SBK(idx)->numRecords);
        printf("Num Keys = %d\n",CPI_SBK(idx)->numKeys);
        printf("Cache Lookups = %.0f\n",CPI_SBK(idx)->cacheLookups);
        printf("Cache Hits = %.0f\n",CPI_SBK(idx)->cacheHits);
        printf("Cache Hit Rate = %d%%\n",CPI_CACHE_HIT_RATE(idx));
        printf("\n\n");
}



static void printValue(val, type)
	char	*val;
	int	type;
{
	switch(type)
	{
		case CPI_CHAR:
			printf("%s",val);
			return;

		case CPI_INT:
			printf("%d",(int)*(int *)val);
			return;

		case CPI_UINT:
			printf("%u",(u_int)*(u_int *)val);
			return;

		case CPI_REAL:
			printf("%f",(double)*(double *)val);
			return;

		default:
			printf("xxx");
			return;
	}
}



static void printHeader(idx, headerPtr)
	cpi	*idx;
	cpi_hdr	*headerPtr;
{
	printf("DEBUG  Header %d : Page=%d, Items=%d, Max=",
		headerPtr->headerNum, *headerPtr->pageNum, 
		*headerPtr->numRecords);
	printValue(headerPtr->maxValue, CPI_SBK(idx)->keyType);
	printf("\n");
}



static void printCluster(idx, clusterPtr)
	cpi	*idx;
	cpi_cls	*clusterPtr;
{
	printf("DEBUG  Cluster %d : Page=%d, Headers=%d, Max=",
		clusterPtr->clusterNum, *clusterPtr->pageNum, 
		*clusterPtr->numHeaders);
	printValue(clusterPtr->maxValue, CPI_SBK(idx)->keyType);
	printf("\n");
}




void cpiDumpIndex(idx)
	cpi	*idx;
{
	char	*lastValue = NULL,
		*barfReason = NULL;
	cpi_nod	nodeBuf;
	cpi_hdr	headerBuf;
	cpi_cls	clusterBuf;
	cpi_ch	*cachePtr;
	int	numHeaders,
		numRecords,
		res = 0,
		curCluster,
		curHeader,
		curRecord,
		barfCluster = -1,
		barfHeader = -1;
	

	curCluster = 0;
	while(curCluster < CPI_SBK(idx)->numClusters)
	{
		cachePtr = readPage(idx, CPI_CLS_PAGE(idx,curCluster));
		getCluster(idx, cachePtr, curCluster, &clusterBuf);
		numHeaders = *clusterBuf.numHeaders;
		curHeader = 0;
		while (curHeader < numHeaders)
		{
			cachePtr=readPage(idx,CPI_CLS_PAGE(idx,curCluster));
			getHeader(idx,cachePtr,curCluster,curHeader,&headerBuf);
			numRecords = *headerBuf.numRecords;
			curRecord = 0;
			printf("+H%d:%d P%d ---------------- Max = '", 
				curCluster,curHeader, *headerBuf.pageNum);
			printValue(headerBuf.maxValue, CPI_SBK(idx)->keyType);
			printf("'\n");

			cachePtr = readPage(idx, *headerBuf.pageNum);
			while (curRecord < numRecords)
			{
				getRecord(idx, cachePtr, curCluster,
					curHeader, curRecord, &nodeBuf);
				printf("|\t");
				printValue(nodeBuf.key, CPI_SBK(idx)->keyType);
				printf("\n");
				curRecord++;
				lastValue = nodeBuf.key;
			}
			compareValues(idx,lastValue,headerBuf.maxValue,res);
			if (res != 0)
			{
				if (barfCluster == -1)
				{
					barfCluster = curCluster;
					barfHeader = curHeader;
					barfReason = "Bad header max for %d:%d";
				}
			}
			curHeader++;
		}
		compareValues(idx,lastValue,clusterBuf.maxValue,res);
		if (res != 0)
		{
			if (barfCluster == -1)
			{
				barfCluster = curCluster;
				barfHeader = curHeader;
				barfReason = "Bad cluster max for %d";
			}
		}
		curCluster++;
	}
	printf("+------------------------\n\n");
	if (barfCluster != -1)
	{
		printf("\n\nERROR : Index corrupted\n\n\t");
		printf(barfReason,barfCluster, barfHeader);
		printf("\n\n");
		exit(1);
	}
	return;
}





int cpiTestIndex(idx)
	cpi	*idx;
{
	char	*lastValue = NULL,
		*barfReason = NULL;
	cpi_nod	nodeBuf;
	cpi_hdr	headerBuf;
	cpi_cls	clusterBuf;
	cpi_ch	*cachePtr;
	int	numHeaders,
		numRecords,
		res = 0,
		curCluster,
		curHeader,
		curRecord,
		barfCluster = -1,
		barfHeader = -1,
		recordCount;
	

	curCluster = 0;
	recordCount = 0;
	while(curCluster < CPI_SBK(idx)->numClusters)
	{
		cachePtr = readPage(idx, CPI_CLS_PAGE(idx,curCluster));
		getCluster(idx, cachePtr, curCluster, &clusterBuf);
		numHeaders = *clusterBuf.numHeaders;
		curHeader = 0;
		while (curHeader < numHeaders)
		{
			cachePtr=readPage(idx,CPI_CLS_PAGE(idx,curCluster));
			getHeader(idx,cachePtr,curCluster,curHeader,&headerBuf);
			numRecords = *headerBuf.numRecords;
			curRecord = 0;

			cachePtr = readPage(idx, *headerBuf.pageNum);
			while (curRecord < numRecords)
			{
				getRecord(idx, cachePtr, curCluster,
					curHeader, curRecord, &nodeBuf);
				curRecord++;
				recordCount++;
				lastValue = nodeBuf.key;
			}
			compareValues(idx,lastValue,headerBuf.maxValue,res);
			if (res != 0)
			{
				if (barfCluster == -1)
				{
					barfCluster = curCluster;
					barfHeader = curHeader;
					barfReason = "Bad header max for %d:%d";
				}
			}
			curHeader++;
		}
		compareValues(idx,lastValue,clusterBuf.maxValue,res);
		if (res != 0)
		{
			if (barfCluster == -1)
			{
				barfCluster = curCluster;
				barfHeader = curHeader;
				barfReason = "Bad cluster max for %d";
			}
		}
		curCluster++;
	}
	if (barfCluster != -1)
	{
		printf("\n\nERROR : Index corrupted\n\n\t");
		printf(barfReason,barfCluster, barfHeader);
		printf("\n\n");
		cpiDumpIndex(idx);
		printf("\n\n");
		exit(1);
	}
	return(recordCount);
}



/*************************************************************************
**************************************************************************
**
**                        UTILITY ROUTINES
**
**************************************************************************
*************************************************************************/



static int writeRecord(idx, clusterNum, headerNum, recordNum, nodePtr)
	cpi	*idx;
	int	clusterNum,
		headerNum,
		recordNum;
	cpi_nod	*nodePtr;
{
	static	cpi_hdr	headerBuf;
	static	cpi_ch	*cachePtr,
			*nodeCachePtr;
	static	char	*pagePtr;
	char	*basePtr;

	cachePtr = readPage(idx, CPI_CLS_PAGE(idx, clusterNum));

	getHeader(idx, cachePtr, clusterNum, headerNum, &headerBuf);
	nodeCachePtr = readPage(idx, *headerBuf.pageNum);
	pagePtr = nodeCachePtr->ptr;
	if (pagePtr == NULL)
		return(-1);
	basePtr = pagePtr + (recordNum * CPI_REC_SIZE(idx));

	bcopy(nodePtr->key, basePtr, CPI_KEY_SIZE(idx));
	*(int *)(basePtr + CPI_KEY_SIZE(idx)) = nodePtr->data;
	return(0);
}



static void initialiseCluster(idx, clusterNum)
	cpi	*idx;
	int	clusterNum;
{
	int	pageNum,
		listHead,
		fd;
	off_t	offset;
	u_int 	uval;
	_REG	int loop;


	if (cpiDebug)
	{
		printf("DEBUG  initialiseCluster %d\n",clusterNum);
	}
	/*
	** Setup the cluster / header page
	*/
	fd = idx->fd;
	pageNum = CPI_CLS_PAGE(idx,clusterNum);
	lseek(fd,CPI_PAGE_OFFSET(idx, pageNum), SEEK_SET);

	/* Cluster record */
	uval = 0;
	write(fd,&uval, sizeof(uval));
	uval = pageNum;
	write(fd,&uval, sizeof(uval));
	lseek(fd,CPI_SBK(idx)->keySize, SEEK_CUR);

	/* Header records */
	uval = 0;
	for (loop = 0; loop < CPI_SBK(idx)->hdrsPerPage; loop++)
	{
		write(fd,&uval, sizeof(uval));
		lseek(fd,CPI_SBK(idx)->keySize + sizeof(u_int), SEEK_CUR);
	}


	/*
	** Initialise the data pages and setup the free list.
	*/
	listHead = CPI_SBK(idx)->freeList;
	for (loop = 1; loop <= CPI_SBK(idx)->hdrsPerPage; loop++)
	{
		offset = CPI_PAGE_OFFSET(idx, pageNum + loop);
		lseek(fd,offset, SEEK_SET);
		if (loop < CPI_SBK(idx)->hdrsPerPage)
			uval = pageNum + loop + 1;
		else
			uval = listHead;  /* end of list */
		write(fd,&uval, sizeof(uval));
	}
	offset = CPI_PAGE_OFFSET(idx, pageNum + loop) - sizeof(uval);
	lseek(fd,offset, SEEK_SET);
	uval = 0;
	write(fd,&uval, sizeof(uval));

	/*
	** Update the super block
	*/
	CPI_SBK(idx)->numClusters += 1;
	CPI_SBK(idx)->freeList = pageNum + 1;
}



static char *getFreePage(idx, pageNum)
	cpi	*idx;
	int	*pageNum;
{
	cpi_ch	*cacheEntry;
	int	tmp;
	char	*pagePtr;

	/*
	** Pages in the free list have the page number of the next
	** free page stored in the first sizeof(int) bytes of the page.
	** The page num of the head of the free list is stored in the
	** superblock and must be reset on disk after we're done.
	*/
	if (CPI_SBK(idx)->freeList == 0)
	{
		abort();
	}
	*pageNum = CPI_SBK(idx)->freeList;
	if (cpiDebug)
	{
		printf("DEBUG  getFreePage returning page %d\n", *pageNum);
	}
	cacheEntry = readPage(idx, *pageNum);
	pagePtr = cacheEntry->ptr;
	tmp = (int)*(int *)pagePtr;
	CPI_SBK(idx)->freeList = tmp;
	return(pagePtr);
}




static int shiftDataPage(idx, clusterPtr, headerPtr, recordNum, direction)
	cpi	*idx;
	cpi_cls	*clusterPtr;
	cpi_hdr	*headerPtr;
	int	recordNum,
		direction;
{
	cpi_ch	*cacheEntry;
	char	*pagePtr,
		*cp,
		*cp1;

	/*
	** Get a pointer to the page holding the data records
	*/
	cacheEntry = readPage(idx, *headerPtr->pageNum);
	pagePtr = cacheEntry->ptr;
	if (pagePtr == NULL)
		return(-1);
	
	/*
	** Shuffle all the records in the page one slot starting
	** at the specified record.  
	*/
	if (cpiDebug)
	{
		printf("DEBUG  shiftDataPage %s %d:%d from %d\n",
			direction == CPI_SHIFT_UP?"up":"down",
			clusterPtr->clusterNum, headerPtr->headerNum, 
			recordNum);
	}
	if (direction == CPI_SHIFT_UP)
	{
		cp = pagePtr + (recordNum * CPI_REC_SIZE(idx));
		cp1 = cp + CPI_REC_SIZE(idx);
		bcopy(cp,cp1,(CPI_SBK(idx)->recsPerPage-recordNum-1) * 
			CPI_REC_SIZE(idx));
	}
	else
	{
		recordNum++;
		cp = pagePtr + (recordNum * CPI_REC_SIZE(idx));
		cp1 = cp - CPI_REC_SIZE(idx);
		bcopy(cp, cp1, (*headerPtr->numRecords-recordNum) * 
			CPI_REC_SIZE(idx));
	}
	return(0);
}




static void shiftHeaderPage(idx, clusterNum, headerNum, direction)
	cpi	*idx;
	int	clusterNum,
		headerNum,
		direction;
{
	cpi_cls	clusterBuf;
	cpi_ch	*cacheEntry;
	char	*cp,
		*cp1,
		*pagePtr;


	/*
	** Shuffle all the headers in the page one slot starting
	** at the specified header.  
	*/
	if (cpiDebug)
	{
		printf("DEBUG  shiftHeaderPage %s Cluster %d from %d\n",
			direction == CPI_SHIFT_UP?"up":"down",
			clusterNum, headerNum);
	}

	cacheEntry = readPage(idx, CPI_CLS_PAGE(idx, clusterNum));
	getCluster(idx, cacheEntry, clusterNum, &clusterBuf);
	pagePtr = cacheEntry->ptr;
	if (direction == CPI_SHIFT_UP)
	{
		cp = pagePtr + CPI_CLS_SIZE(idx) + 
			(headerNum * CPI_HDR_SIZE(idx));
		cp1 = cp + CPI_HDR_SIZE(idx);
		bcopy(cp,cp1,(CPI_SBK(idx)->hdrsPerPage-headerNum-1) * 
			CPI_HDR_SIZE(idx));
	}
	else
	{
		headerNum ++;
		if (headerNum < *clusterBuf.numHeaders)
		{
			cp = pagePtr + CPI_CLS_SIZE(idx) + 
				(headerNum * CPI_HDR_SIZE(idx));
			cp1 = cp - CPI_HDR_SIZE(idx);
			bcopy(cp,cp1,(CPI_SBK(idx)->hdrsPerPage-headerNum-1) * 
				CPI_HDR_SIZE(idx));
		}
	}
	*(clusterBuf.numHeaders) += direction;
}




static void setMaxValue(idx, clusterNum, headerNum, key, flag)
	cpi	*idx;
	int	headerNum,
		clusterNum;
	char	*key;
	int	flag;
{
	int	res = 0;
	cpi_cls	clusterBuf;
	cpi_hdr	headerBuf;
	cpi_ch	*cachePtr;


	/*
	** Reset the maxValue of the page header if required. If the
	** data page is empty we set the value regardless.
	*/
	cachePtr = readPage(idx, CPI_CLS_PAGE(idx,clusterNum));
	getHeader(idx, cachePtr, clusterNum, headerNum, &headerBuf);
	if (*headerBuf.numRecords > 0 && flag == CPI_IF_NEEDED)
	{
		compareValues(idx, key, headerBuf.maxValue, res);
		if (res <= 0)
		{
			return;
		}
	}
	if (cpiDebug)
	{
		printf("DEBUG  Max value for hdr %d, page %d set to ",
			headerBuf.headerNum, *headerBuf.pageNum);
		printValue(key, CPI_SBK(idx)->keyType);
		printf("\n");
	}
	bcopy(key, headerBuf.maxValue, CPI_SBK(idx)->keySize);

	/*
	** If this is the last header in the cluster then reset the
	** cluster's max value too
	*/
	getCluster(idx,cachePtr,clusterNum,&clusterBuf);
	if (headerNum == *(clusterBuf.numHeaders) - 1)
	{
		bcopy(key, clusterBuf.maxValue, CPI_SBK(idx)->keySize);
		if (cpiDebug)
		{
			printf("DEBUG  Max value for cluster %d set to ",
				clusterNum);
			printValue(key, CPI_SBK(idx)->keyType);
			printf("\n");
		}
	}
}





static void resetMaxValue(idx, clusterNum, headerNum)
	cpi	*idx;
	int	headerNum,
		clusterNum;
{
	cpi_nod	nodeBuf;
	cpi_hdr	headerBuf;
	cpi_ch	*cachePtr;
	int	pageNum;

	cachePtr = readPage(idx, CPI_CLS_PAGE(idx,clusterNum));	
	getHeader(idx, cachePtr, clusterNum, headerNum, &headerBuf);
	pageNum = *headerBuf.pageNum;
	cachePtr = readPage(idx, pageNum);
	getRecord(idx, cachePtr, clusterNum, headerNum,  
		*headerBuf.numRecords - 1, &nodeBuf);
	if (cpiDebug)
	{
		printf("DEBUG  Reset Max for hdr %d:%d page %d\n", 
			clusterNum,headerNum, pageNum);
	}
	setMaxValue(idx, clusterNum, headerNum, nodeBuf.key, CPI_FORCE);
}





static void splitClusters(idx, cluster1, cluster2)
	cpi	*idx;
	int	cluster1, cluster2;
{
	cpi_cls	cluster1Buf, cluster2Buf;
	cpi_ch	*cache1, *cache2;
	char	*ptr1, *ptr2, *cp;
	int	spare,
		splitDest;


	cache1 = readPage(idx, CPI_CLS_PAGE(idx, cluster1));
	cache2 = readPage(idx, CPI_CLS_PAGE(idx, cluster2));
	getCluster(idx, cache1, cluster1, &cluster1Buf);
	getCluster(idx, cache2, cluster2, &cluster2Buf);
	ptr1 = cache1->ptr + CPI_CLS_SIZE(idx);
	ptr2 = cache2->ptr + CPI_CLS_SIZE(idx);
	if (*cluster1Buf.numHeaders < *cluster2Buf.numHeaders)
	{
		spare = CPI_SBK(idx)->hdrsPerPage - *cluster1Buf.numHeaders;
		splitDest = 1;
	}
	else
	{
		spare = CPI_SBK(idx)->hdrsPerPage - *cluster2Buf.numHeaders;
		splitDest = 2;
	}
	if (cpiDebug)
	{
		printf("DEBUG  splitClusters : Cls1=%d, Cls2=%d, ",
			cluster1, cluster2);
		printf("spare slots=%d, dest=hdr%d\n",
			spare, splitDest);
	}

	/*
	** Copy (spare / 2) headers from the full cluster to the other
	*/
	if (splitDest == 1)
	{
		cp = ptr1 + (*cluster1Buf.numHeaders * CPI_HDR_SIZE(idx));
		bcopy(ptr2, cp, (spare / 2) * CPI_HDR_SIZE(idx));
		cp = ptr2 + ((spare / 2) * CPI_HDR_SIZE(idx));
		bcopy(cp,ptr2,(*cluster2Buf.numHeaders - 
			(spare / 2)) * CPI_HDR_SIZE(idx));
		*(cluster1Buf.numHeaders) += spare / 2;
		*(cluster2Buf.numHeaders) -= spare / 2;
	}
	else
	{
		cp = ptr2;
		bcopy(cp, cp + (spare / 2 * CPI_HDR_SIZE(idx)), 
			*cluster2Buf.numHeaders * CPI_HDR_SIZE(idx));
		cp = ptr1 + ((*cluster1Buf.numHeaders - (spare / 2)) 
			* CPI_HDR_SIZE(idx));
		bcopy(cp, ptr2, spare / 2 * CPI_HDR_SIZE(idx)); 
		*(cluster1Buf.numHeaders) -= spare / 2;
		*(cluster2Buf.numHeaders) += spare / 2;
	}


	/*
	** The maxValue of the clusters may have changed so force a reset
	*/
	resetMaxValue(idx, cluster1, *cluster1Buf.numHeaders - 1);
	resetMaxValue(idx, cluster2, *cluster2Buf.numHeaders - 1);

	if (cpiDebug)
	{
		printCluster(idx, &cluster1Buf);
		printCluster(idx, &cluster2Buf);
	}
}




static void splitPages(idx, cluster1, header1, cluster2, header2)
	cpi	*idx;
	int	cluster1, header1,
		cluster2, header2;
{
	cpi_hdr	header1Buf, header2Buf;
	cpi_ch	*cachePtr1, *cachePtr2,
		*nodeCachePtr1, *nodeCachePtr2;
	char	*page1Ptr, *page2Ptr,
		*cp;
	int	spare,
		splitDest;


	cachePtr1 = readPage(idx, CPI_CLS_PAGE(idx, cluster1));
	getHeader(idx, cachePtr1, cluster1, header1, &header1Buf);
	cachePtr2 = readPage(idx, CPI_CLS_PAGE(idx, cluster2));
	getHeader(idx, cachePtr2, cluster2, header2, &header2Buf);
	if (*header1Buf.numRecords < *header2Buf.numRecords)
	{
		spare = CPI_SBK(idx)->recsPerPage - *header1Buf.numRecords;
		splitDest = 1;
	}
	else
	{
		spare = CPI_SBK(idx)->recsPerPage - *header2Buf.numRecords;
		splitDest = 2;
	}
	nodeCachePtr1 = readPage(idx, *header1Buf.pageNum);
	nodeCachePtr2 = readPage(idx, *header2Buf.pageNum);
	page1Ptr = nodeCachePtr1->ptr;
	page2Ptr = nodeCachePtr2->ptr;
	if (cpiDebug)
	{
		printf("DEBUG  splitPages : hdr1=%d:%d, ", cluster1, header1);
		printf("hdr2=%d:%d, spare slots=%d, dest=hdr%d\n",
			cluster2, header2, spare, splitDest);
	}

	/*
	** Copy (spare / 2) records from the full page to the other
	*/
	if (splitDest == 1)
	{
		cp = page1Ptr + (*header1Buf.numRecords * CPI_REC_SIZE(idx));
		bcopy(page2Ptr, cp, (spare / 2) * CPI_REC_SIZE(idx));
		cp = page2Ptr + ((spare / 2) * CPI_REC_SIZE(idx));
		bcopy(cp,page2Ptr,(*header2Buf.numRecords - 
			(spare / 2)) * CPI_REC_SIZE(idx));
		*(header1Buf.numRecords) += spare / 2;
		*(header2Buf.numRecords) -= spare / 2;
	}
	else
	{
		cp = page2Ptr;
		bcopy(cp, cp + (spare / 2 * CPI_REC_SIZE(idx)), 
			*header2Buf.numRecords * CPI_REC_SIZE(idx));
		cp = page1Ptr + ((*header1Buf.numRecords - (spare / 2)) 
			* CPI_REC_SIZE(idx));
		bcopy(cp, page2Ptr, spare / 2 * CPI_REC_SIZE(idx)); 
		*(header1Buf.numRecords) -= spare / 2;
		*(header2Buf.numRecords) += spare / 2;
	}

	/*
	** The maxValue of the pages may have changed so
	** force a reset in the headers.
	*/
	resetMaxValue(idx, cluster1, header1);
	resetMaxValue(idx, cluster2, header2);

	if (cpiDebug)
	{
		printHeader(idx, &header1Buf);
		printHeader(idx, &header2Buf);
	}
}




static void createSpaceForCluster(idx, clusterNum)
	cpi	*idx;
	int	clusterNum;
{
	cpi_cls	clusterBuf;
	cpi_hdr	headerBuf;
	cpi_ch	*cachePtr;
	int	foundRoom = 0,
		curCluster,
		cluster1 = 0, 
		cluster2 = 0, 
		curHeader,
		freeSpace;
	cpi_ch	*cacheEntry1,
		*cacheEntry2;

	if (cpiDebug)
	{
		printf("DEBUG  createSpaceForCluster( %d)\n",clusterNum);
	}
	/*
	** If there's room in a neighbour cluster then grab half
	** of it.  Try the left neighbour first if there is one.
	*/
	if (clusterNum > 0)
	{
		curCluster = clusterNum - 1;
		cachePtr = readPage(idx, CPI_CLS_PAGE(idx,curCluster));
		getCluster(idx, cachePtr, curCluster,&clusterBuf);
		freeSpace = CPI_SBK(idx)->hdrsPerPage - *clusterBuf.numHeaders;
		if (freeSpace > CPI_SBK(idx)->hdrsPerPage / 8 && freeSpace > 3)
		{
			cluster1 = curCluster;
			cluster2 = clusterNum;
			foundRoom = 1;
		}
	}

	/*
	** How about the right neighbour
	*/
	if ( ! foundRoom)
	{
		if (clusterNum < CPI_SBK(idx)->numClusters -1 )
		{
			curCluster = clusterNum + 1;
			cachePtr=readPage(idx,CPI_CLS_PAGE(idx,curCluster));
			getCluster(idx, cachePtr, curCluster,&clusterBuf);
			freeSpace = CPI_SBK(idx)->hdrsPerPage - 
				*clusterBuf.numHeaders;
			if (freeSpace > CPI_SBK(idx)->hdrsPerPage / 8 && 
				freeSpace > 3)
			{
				cluster1 = clusterNum;
				cluster2 = curCluster;
				foundRoom = 1;
			}
		}
	}

	/*
	** If there's no room near here then we have to create a new cluster.
	** Drop a new cluser on the end of the file and roll the clusters
	** down one from clusterNum.
	*/
	if ( ! foundRoom)
	{
		curCluster = CPI_SBK(idx)->numClusters - 1;
		initialiseCluster(idx, curCluster + 1);
		cacheEntry2 = readPage(idx,CPI_CLS_PAGE(idx,curCluster+1));
		while(curCluster > clusterNum)
		{
			cacheEntry1 = readPage(idx,
				CPI_CLS_PAGE(idx,curCluster));
			bcopy(cacheEntry1->ptr, cacheEntry2->ptr, 
				CPI_SBK(idx)->pageSize);

			getCluster(idx,cacheEntry2,curCluster+1,&clusterBuf);
			*clusterBuf.pageNum=CPI_CLS_PAGE(idx,curCluster+1);
			cacheEntry2 = cacheEntry1;
			curCluster--;
		}
		cluster1 = clusterNum;
		cluster2 = clusterNum + 1;

		/*
		** Zero out the values in the old cluster so we can
		** reuse it as a fresh page
		*/
		cachePtr = readPage(idx, CPI_CLS_PAGE(idx,cluster2));
		getCluster(idx, cachePtr, cluster2, &clusterBuf);
		curHeader = 0;
		while(curHeader < *clusterBuf.numHeaders)
		{
			getHeader(idx, cachePtr, cluster2, curHeader, 
				&headerBuf);
			*headerBuf.numRecords = 0;
			*headerBuf.pageNum = 0;
			curHeader++;
		}
		*clusterBuf.numHeaders = 0;
	}
	splitClusters(idx, cluster1, cluster2);
}


static void createSpaceForHeader(idx, clusterNum, headerNum)
	cpi	*idx;
	int	clusterNum,
		headerNum;
{
	cpi_cls	clusterBuf;
	cpi_hdr	headerBuf;
	cpi_ch	*cachePtr;
	int	foundRoom = 0,
		curCluster, curHeader,
		cluster1 = 0, header1 = 0,
		cluster2 = 0, header2 = 0,
		freeSpace,
		validPage;

	if (cpiDebug)
	{
		printf("DEBUG  createSpaceForHeader( %d,%d)\n",clusterNum,
			headerNum);
	}
	/*
	** If there's room in a neighbour page then grab half
	** of it.  Try the left neighbour first.
	*/
	curCluster = clusterNum;
	curHeader = headerNum;
	validPage = 1;
	if (curHeader == 0)
	{
		if (curCluster > 0)
		{
			curCluster --;
			cachePtr = readPage(idx,CPI_CLS_PAGE(idx,curCluster));
			getCluster(idx,cachePtr,curCluster,&clusterBuf);
			curHeader = *clusterBuf.numHeaders - 1;
		}
		else
		{
			validPage = 0;
		}
	}
	else
	{
		curHeader --;
	}
	if (validPage)
	{
		cachePtr = readPage(idx,CPI_CLS_PAGE(idx,curCluster));
		getHeader(idx, cachePtr, curCluster, curHeader, &headerBuf);
		freeSpace = CPI_SBK(idx)->recsPerPage - *headerBuf.numRecords;
		if (freeSpace > CPI_SBK(idx)->recsPerPage / 4 &&
			freeSpace > 3)
		{
			cluster1 = curCluster;
			header1 = curHeader;
			cluster2 = clusterNum;
			header2 = headerNum;
			foundRoom = 1;
		}
	}

	/*
	** How about the right neighbour
	*/
	if ( ! foundRoom)
	{
		curCluster = clusterNum;
		curHeader = headerNum;
		validPage = 1;
		cachePtr = readPage(idx,CPI_CLS_PAGE(idx,curCluster));
		getCluster(idx,cachePtr,curCluster,&clusterBuf);
		if (curHeader == CPI_SBK(idx)->hdrsPerPage - 1)
		{
			curCluster ++;
			curHeader = 0;
			if (curCluster == CPI_SBK(idx)->numClusters)
			{
				validPage = 0;
			}
		}
		else
		{
			curHeader ++;
			if (curHeader == *clusterBuf.numHeaders)
			{
				validPage = 0;
			}
		}
		if (validPage)
		{
			cachePtr = readPage(idx,CPI_CLS_PAGE(idx,curCluster));
			getHeader(idx,cachePtr,curCluster,curHeader,&headerBuf);
			freeSpace = CPI_SBK(idx)->recsPerPage - 
				*headerBuf.numRecords;
			if (freeSpace > CPI_SBK(idx)->recsPerPage / 4 &&
				freeSpace > 3)
			{
				cluster2 = curCluster;
				header2 = curHeader;
				cluster1 = clusterNum;
				header1 = headerNum;
				foundRoom = 1;
			}
		}
	}

	/*
	** If there's no room near here then we have to create a new page.
	** Note, the above code will leave curCluster and curHeader set
	** to the header directly after the one we are working on.  If
	** the page wasn't valid before, it will be after the shiftHeader()
	*/
	if ( ! foundRoom)
	{
		int	tmpPageNum;

		getFreePage(idx, &tmpPageNum);
		shiftHeaderPage(idx, clusterNum, headerNum, CPI_SHIFT_UP);

		cachePtr = readPage(idx,CPI_CLS_PAGE(idx,clusterNum));
		getHeader(idx, cachePtr, clusterNum, headerNum, &headerBuf);
		*headerBuf.pageNum = tmpPageNum;
		*headerBuf.numRecords = 0;
		cluster2 = curCluster;
		header2 = curHeader;
		cluster1 = clusterNum;
		header1 = headerNum;
	}
	splitPages(idx, cluster1, header1, cluster2, header2);
}



/*************************************************************************
**************************************************************************
**
**                        INTERFACE ROUTINES
**
**************************************************************************
*************************************************************************/


/*
static void apiInit()
{
}
*/
#define	apiInit()


int cpiCreate(path, mode, keySize, keyType, flags, envPtr)
        char    *path;
        int     mode,
                keySize,
                keyType,
                flags;
	cpi_env	*envPtr;
{
        int     fd;
        cpi_sbk sbk;
	cpi	idx;

        /*
        ** Create the actual file
        */
	apiInit();
        fd = open(path,O_RDWR | O_CREAT, mode);
        if (fd < 0)
        {
                return(-1);
        }

        /*
        ** Setup the basic superblock info
        */
        strcpy(sbk.fileType,"#! Clustered Page Index Format");
        sbk.magic = CPI_MAGIC;
        sbk.version = CPI_VERSION;
        sbk.keyType = keyType;
        if (keyType == CPI_CHAR)
                keySize++;
        sbk.keySize = keySize;


	/*
	** Now the important stuff.  Set the page size and ensure the
	** mapping size is VM page aligned
	*/
	sbk.pageSize = 0;
	if (envPtr)
		sbk.pageSize = envPtr->pageSize;
	if (sbk.pageSize == 0)
		sbk.pageSize = getpagesize();
	sbk.cacheSize = 0;
	if (envPtr)
		sbk.cacheSize = envPtr->cacheSize;
	if (sbk.cacheSize == 0)
		sbk.cacheSize = CPI_DEFAULT_CACHE;

	sbk.vmPageSize = getpagesize();
	if (sbk.pageSize % sbk.vmPageSize != 0)
	{
		sbk.mapSize = ((sbk.pageSize / sbk.vmPageSize) + 1) *
			sbk.vmPageSize;
	}
	else
	{
		sbk.mapSize = sbk.pageSize;
	}


#	ifdef FORCE_SMALL_PAGES
	sbk.recsPerPage = 5;
	sbk.hdrsPerPage = 6;
#	else
	sbk.recsPerPage = (sbk.pageSize / (keySize + sizeof(u_int)));
	sbk.hdrsPerPage = (sbk.pageSize - (keySize + 2 * sizeof(u_int))) / 
		(keySize + 2 * sizeof(u_int));
#	endif

	sbk.numRecords = 0;
	sbk.numKeys = 0;
	sbk.numClusters = 1;
	sbk.cacheLookups = 0;
	sbk.cacheHits = 0;
	sbk.freeList = 2;
	
        if (write(fd,&sbk,sizeof(sbk)) < sizeof(sbk))
        { 
                close(fd);
                unlink(path);
                return(-1);
        }       


	/*
	** Initialise the first cluster
	*/
	idx.opaque = &sbk;
	idx.fd = fd;
	initialiseCluster(&idx, 0);
	close(fd);
	return(0);
}




cpi *cpiOpen(path, envPtr)
	char	*path;
	cpi_env	*envPtr;
{
	cpi_ch	*cacheEntry;
	cpi	*new;


	/*
	** Open the file and map the superblock.  Ensure that the
	** file is an CPI file and it's the correct CPI version.
	*/
	apiInit();
	new = (cpi *)malloc(sizeof(cpi));
	new->fd = open(path, O_RDWR, 0);
	if (new->fd < 0)
	{
		perror("cpiOpen - open()");
		free(new);
		return(NULL);
	}
	new->opaque = (void *)mmap(NULL, getpagesize(),
		(PROT_READ|PROT_WRITE), MAP_SHARED, new->fd, (off_t)0);
	if (CPI_SBK(new)->magic != CPI_MAGIC)
	{
		free(new);
		fprintf(stderr, "\nCPI_ERROR : %s is not an CPI file!\n\n",
			path);
		return(NULL);
	}
	if (CPI_SBK(new)->version != CPI_VERSION)
	{
		free(new);
		fprintf(stderr, 
			"\nCPI_ERROR : Wrong version for %s (%d not %d)!\n\n",
			path, CPI_SBK(new)->version, CPI_VERSION);
		return(NULL);
	}
	new->path = (char *)strdup(path);

	/*
	** Initialise the cache and prime it with the important pages
	*/
	cacheInitialise(new);
	cacheEntry = readPage(new, 0);
	cacheEntry = readPage(new, 1);
	return(new);
}




void cpiSync(idx)
	cpi	*idx;
{
	char	*cp;
	cpi_ch	*cur;
	int	count;

	apiInit();
	cp = idx->cache;
	for(count = 0; count < CPI_SBK(idx)->cacheSize; count++)
	{
		cur = (cpi_ch *)cp;
		if (cur->ptr == NULL)
			continue;
		MSYNC(cur->ptr, CPI_SBK(idx)->mapSize,0);
                cp += sizeof(cpi_ch);
	}
}




void cpiClose(idx)
	cpi	*idx;
{
	char	*cp;
	cpi_ch	*cur;
	int	count;

	apiInit();
	cp = idx->cache;
	for(count = 0; count < CPI_SBK(idx)->cacheSize; count++)
	{
		cur = (cpi_ch *)cp;
		if (cur->ptr == NULL)
			continue;
		munmap(cur->ptr, CPI_SBK(idx)->mapSize);
		cur->ptr = NULL;
                cp += sizeof(cpi_ch);
	}
	free(idx->cache);
	idx->cache = NULL;
	munmap(idx->opaque, getpagesize());
	close(idx->fd);
}



int cpiInsert(idx, key, data)
	cpi	*idx;
	char	*key;
	u_int	data;
{
	cpi_ch	*cachePtr;
	cpi_nod	nodeBuf;
	cpi_hdr	headerBuf;
	cpi_cls	clusterBuf;
	int	insertPos,
		res;


	apiInit();
	if (cpiDebug)
	{
		char	*file;

		file = (char *)rindex(idx->path,'/');
		if (!file)
			file = idx->path;
		printf("DEBUG  Inserting '");
		printValue(key, CPI_SBK(idx)->keyType);
		printf("':%u in '%s'\n",data, file);
	}
	/*
	** Find the header of the page we're going to drop this
	** record into
	*/
	res = privateLookup(idx, key, &clusterBuf, &headerBuf,
		&nodeBuf, CPI_CLOSEST);
	if (res == -1)
		return(-1);
	if (res == CPI_ERR_NOT_FOUND)
	{
		/* The new value key is larger than any other key */
		nodeBuf.clusterNum = CPI_SBK(idx)->numClusters - 1;
		if (nodeBuf.clusterNum == -1)
		{
			/* First value in index */
			nodeBuf.clusterNum = 0;
		}
		cachePtr = readPage(idx,CPI_CLS_PAGE(idx,nodeBuf.clusterNum));
		getCluster(idx, cachePtr, nodeBuf.clusterNum, &clusterBuf);
		if (*clusterBuf.numHeaders == 0)
			nodeBuf.headerNum = 0;
		else
			nodeBuf.headerNum = *clusterBuf.numHeaders - 1;
		getHeader(idx, cachePtr, nodeBuf.clusterNum, nodeBuf.headerNum,
			&headerBuf);
	}

	/*
	** If there's room in this header then find the correct record pos.
	** Special case code for the first insertion (the only time we
	** create a new header without splitting into it)
	*/
	insertPos = -1;
	if (*headerBuf.numRecords == 0 && headerBuf.headerNum == 0 &&
		headerBuf.clusterNum == 0)
	{
		insertPos = 0;
		*(clusterBuf.numHeaders) += 1;
		getFreePage(idx, headerBuf.pageNum);
	}
	else
	{
		if (*headerBuf.numRecords < CPI_SBK(idx)->recsPerPage)
		{
			if (res != CPI_ERR_NOT_FOUND)
				insertPos = nodeBuf.recordNum;
			else
				insertPos = *headerBuf.numRecords;
		}
	}

	/*
	** If there's no room in this page we have to make some room.
	** Either we shift the headers to make a spare slot or we split
	** the cluster if there's no room left in this cluster.
	** The desired header position may have moved to another
	** cluster to just take the easy option and recurse on this
	** insertion rather than trying to remember where things are at.
	*/
	if (insertPos == -1)
	{
		if (*clusterBuf.numHeaders == CPI_SBK(idx)->hdrsPerPage)
		{
			createSpaceForCluster(idx, nodeBuf.clusterNum);
		}
		else
		{
			createSpaceForHeader(idx, nodeBuf.clusterNum, 
				nodeBuf.headerNum);
		}
		return(cpiInsert(idx,key,data));
	}

	/*
	** OK, insert the record at the determined position
	*/
	nodeBuf.key = key;
	nodeBuf.data = data;
	if (shiftDataPage(idx,&clusterBuf,&headerBuf,insertPos,CPI_SHIFT_UP)<0)
	{
		return(-1);
	}
	if (writeRecord(idx, nodeBuf.clusterNum, nodeBuf.headerNum, 
		insertPos, &nodeBuf) < 0)
	{
		return(-1);
	}
	*(headerBuf.numRecords) += 1;
	setMaxValue(idx, nodeBuf.clusterNum, nodeBuf.headerNum, key, 
		CPI_IF_NEEDED);
	if (cpiVerbose)
		cpiDumpIndex(idx);
	return(0);
}


static int privateLookup(idx, key, clusterPtr, headerPtr, nodePtr, flags)
	cpi	*idx;
	char	*key;
	cpi_cls	*clusterPtr;
	cpi_hdr	*headerPtr;
	cpi_nod	*nodePtr;	
	int	flags;
{
	int	curCluster,
		curHeader,
		curRecord,
		res = 0,
		min, max, pivot;
	cpi_ch	*clsCachePtr,
		*hdrCachePtr,
		*nodeCachePtr;
	cpi_cls	tmpClusterBuf;
	cpi_hdr	tmpHeaderBuf;


	if (clusterPtr == NULL)
		clusterPtr = &tmpClusterBuf;
	if (headerPtr == NULL)
		headerPtr = &tmpHeaderBuf;

	/*
	** Traverse the clusters looking for the desired cluster
	*/
	curCluster = 0;
	while (curCluster < CPI_SBK(idx)->numClusters)
	{
		clsCachePtr = readPage(idx, CPI_CLS_PAGE(idx, curCluster));
		getCluster(idx, clsCachePtr, curCluster, clusterPtr);
		if (*clusterPtr->numHeaders == 0)
			return(CPI_ERR_NOT_FOUND);
		compareValues(idx, key, clusterPtr->maxValue, res);
		if (res <= 0)
			break;
		curCluster++;
	}
	if (curCluster == CPI_SBK(idx)->numClusters)
		return(CPI_ERR_NOT_FOUND);


	/*
	** OK, which header in this cluster is the one we want ?
	** Use binary search.
	*/
	pivot = 0;
	min = 0;
	max = *clusterPtr->numHeaders - 1;
	curHeader = 0;
	hdrCachePtr = readPage(idx, CPI_CLS_PAGE(idx, curCluster));
	while(1)
	{
		if (max - min < 3)
			break;
		pivot = max - ((max - min) / 2);
		getHeader(idx, hdrCachePtr, curCluster, pivot,headerPtr);
		compareValues(idx, key, headerPtr->maxValue, res);
		if (res < 0)
		{
			max = pivot;
			continue;
		}
		if (res > 0)
		{
			min = pivot;
			continue;
		}
		max = min = pivot;
		break;
	}
	curHeader = min;
	while(curHeader <= max)
	{
		getHeader(idx, hdrCachePtr, curCluster, curHeader,headerPtr);
		compareValues(idx, key, headerPtr->maxValue, res);
		if (res <= 0)
		{
			break;
		}
		curHeader++;
	}
	
	if (curHeader == *clusterPtr->numHeaders)
		return(CPI_ERR_NOT_FOUND);

	/*
	** OK, found the right header.  Find the record we're looking for
	** using a binary search to trim the scan set
	*/
	nodeCachePtr = readPage(idx, *headerPtr->pageNum);
	pivot = 0;
	res = -1;
	min = 0;
	max = *headerPtr->numRecords - 1;
	while(1)
	{
		if (max - min < 3)
			break;
		pivot = max - ((max - min) / 2);
		getRecord(idx, nodeCachePtr, clusterPtr->clusterNum,
			headerPtr->headerNum, pivot, nodePtr);
		compareValues(idx, nodePtr->key, key, res);
		if (res == 0)
			break;
		if (res < 0)
			min = pivot;
		if (res > 0)
			max = pivot;
	}

	if (res == 0)
	{
		curRecord = pivot;
	}
	else
	{
		curRecord = min;
		while (curRecord <= max)
		{
			getRecord(idx, nodeCachePtr, clusterPtr->clusterNum,
				headerPtr->headerNum, curRecord, nodePtr);
			compareValues(idx, nodePtr->key, key, res);
			if (res >= 0)
				break;
			curRecord++;
		}
		if (curRecord == *headerPtr->numRecords)
			return(CPI_ERR_NOT_FOUND);

		if (res > 0  &&  flags == CPI_EXACT)
			return(CPI_ERR_NOT_FOUND);
	}
	while(1)
	{
		curRecord--;
		if (curRecord < 0)
			break;
		getRecord(idx, nodeCachePtr, clusterPtr->clusterNum,
			headerPtr->headerNum, curRecord, nodePtr);
		compareValues(idx, nodePtr->key, key, res);
		if (res != 0)
			break;
	}
	curRecord++;
	getRecord(idx, nodeCachePtr, clusterPtr->clusterNum,
		headerPtr->headerNum, curRecord, nodePtr);
		

	nodePtr->clusterNum = idx->clusterNum = clusterPtr->clusterNum;
	nodePtr->headerNum = idx->headerNum = headerPtr->headerNum;
	nodePtr->recordNum = idx->recordNum =  curRecord;
	
	return(0);
}




int cpiLookup(idx, key, nodePtr, flags)
	cpi	*idx;
	char	*key;
	cpi_nod	*nodePtr;
	int	flags;
{
	apiInit();
	return(privateLookup(idx, key, NULL, NULL, nodePtr, flags));
}



void cpiSetCursor(idx, cursor)
	cpi	*idx;
	cpi_cur	*cursor;
{
	cursor->clusterNum = idx->clusterNum;
	cursor->headerNum = idx->headerNum;
	cursor->recordNum = idx->recordNum;
}





int cpiGetNext(idx, cursor, node)
	cpi	*idx;
	cpi_cur	*cursor;
	cpi_nod	*node;
{
	cpi_hdr	headerBuf;
	cpi_cls	clusterBuf;
	cpi_ch	*cachePtr,
		*nodeCachePtr;
	int	curCluster,
		curHeader,
		curRecord;


	apiInit();
	curCluster = cursor->clusterNum;
	curHeader = cursor->headerNum;
	curRecord = cursor->recordNum;

	curRecord++;
	cachePtr = readPage(idx, CPI_CLS_PAGE(idx, curCluster));
	getHeader(idx, cachePtr, curCluster, curHeader, &headerBuf);
	if (curRecord >= *headerBuf.numRecords)
	{
		curRecord = 0;
		curHeader++;
		getCluster(idx, cachePtr, curCluster, &clusterBuf);
		if (curHeader >= *clusterBuf.numHeaders)
		{
			curHeader = 0;
			curCluster++;
			if (curCluster >= CPI_SBK(idx)->numClusters)
				return(-1);
			cachePtr = readPage(idx, CPI_CLS_PAGE(idx, curCluster));
		}
		getHeader(idx, cachePtr, curCluster, curHeader, &headerBuf);
	}
	nodeCachePtr = readPage(idx, *headerBuf.pageNum);
	getRecord(idx, nodeCachePtr, curCluster, curHeader, curRecord, node);
	idx->clusterNum =  cursor->clusterNum = curCluster;
	idx->headerNum =  cursor->headerNum = curHeader;
	idx->recordNum =  cursor->recordNum = curRecord;
	return(0);
}





int cpiGetPrev(idx, cursor, node)
	cpi	*idx;
	cpi_cur	*cursor;
	cpi_nod	*node;
{
	cpi_hdr	headerBuf;
	cpi_cls	clusterBuf;
	cpi_ch	*cachePtr = NULL,
		*nodeCachePtr;
	int	curCluster,
		curHeader,
		curRecord;


	apiInit();
	curCluster = cursor->clusterNum;
	curHeader = cursor->headerNum;
	curRecord = cursor->recordNum;

	curRecord--;
	if (curRecord < 0)
	{
		curHeader--;
		if (curHeader < 0)
		{
			curCluster--;
			if (curCluster < 0)
				return(-1);
			cachePtr = readPage(idx, CPI_CLS_PAGE(idx,curCluster));
			getCluster(idx, cachePtr, curCluster, &clusterBuf);
			curHeader = *clusterBuf.numHeaders - 1;
		}
		if (!cachePtr)
			cachePtr = readPage(idx, CPI_CLS_PAGE(idx,curCluster));
		getHeader(idx, cachePtr, curCluster,curHeader,&headerBuf);
		curRecord = *headerBuf.numRecords - 1;
	}
	if (!cachePtr)
	{
		cachePtr = readPage(idx, CPI_CLS_PAGE(idx,curCluster));
		getHeader(idx, cachePtr, curCluster,curHeader,&headerBuf);
	}
	nodeCachePtr = readPage(idx, *headerBuf.pageNum);
	getRecord(idx, nodeCachePtr, curCluster, curHeader, curRecord, node);
	idx->clusterNum =  cursor->clusterNum = curCluster;
	idx->headerNum =  cursor->headerNum = curHeader;
	idx->recordNum =  cursor->recordNum = curRecord;
	return(0);
}



int cpiGetFirst(idx, node)
	cpi	*idx;
	cpi_nod	*node;
{
	cpi_ch	*cachePtr;

	apiInit();
	cachePtr = readPage(idx, 2); /* sbk id page 0, cluster is page 1 */
	getRecord(idx, cachePtr, 0, 0, 0, node);
	idx->clusterNum = 0;
	idx->headerNum = 0;
	idx->recordNum = 0;
	return(0);
}



int cpiGetLast(idx, nodePtr)
	cpi	*idx;
	cpi_nod	*nodePtr;
{
	cpi_cls	clusterBuf;
	cpi_ch	*cachePtr,
		*nodeCachePtr;
	cpi_hdr	headerBuf;
	int	curCluster,
		curHeader,
		curRecord;

	apiInit();
	curCluster = CPI_SBK(idx)->numClusters - 1;
	cachePtr = readPage(idx,CPI_CLS_PAGE(idx,curCluster));
	getCluster(idx, cachePtr, curCluster, &clusterBuf);
	curHeader = *clusterBuf.numHeaders - 1;
	getHeader(idx, cachePtr, curCluster, curHeader, &headerBuf);

	nodeCachePtr = readPage(idx, *headerBuf.pageNum);
	curRecord = *headerBuf.numRecords - 1;
	getRecord(idx, nodeCachePtr, curCluster, curHeader, curRecord, nodePtr);
	idx->clusterNum = curCluster;
	idx->headerNum = curHeader;
	idx->recordNum = curRecord;
	return(0);
}



int cpiExists(idx, key, data)
	cpi	*idx;
	char	*key;
	u_int	data;
{
	int	res = 0,
		found;
	cpi_nod	nodeBuf;
	cpi_cur	cursor;

	/*
	** Scan the index for an item with matching key AND data values
	*/
	if (cpiLookup(idx, key, &nodeBuf, CPI_EXACT) < 0)
		return(-1);
	cpiSetCursor(idx, &cursor);
	found = 0;
	compareValues(idx, key, nodeBuf.key, res);
	while(res == 0)
	{
		if (nodeBuf.data == data)
		{
			found = 1;
			break;
		}
		if (cpiGetNext(idx, &cursor, &nodeBuf) < 0)
			return(-1);
		compareValues(idx, key, nodeBuf.key, res);
	}
	if (!found)
		return(-1);
	return(0);
}

int cpiDelete(idx, key, data)
	cpi	*idx;
	char	*key;
	u_int	data;
{
	int	res= 0,
		found;
	cpi_ch	*cachePtr;
	cpi_hdr	headerBuf;
	cpi_cls	clusterBuf;
	cpi_nod	nodeBuf;
	cpi_cur	cursor;

	/*
	** Scan the index for an item with matching key AND data values
	*/
	if (cpiLookup(idx, key, &nodeBuf, CPI_EXACT) < 0)
		return(-1);
	cpiSetCursor(idx, &cursor);
	found = 0;
	compareValues(idx, key, nodeBuf.key, res);
	while(res == 0)
	{
		if (nodeBuf.data == data)
		{
			found = 1;
			break;
		}
		if (cpiGetNext(idx, &cursor, &nodeBuf) < 0)
			return(-1);
		compareValues(idx, key, nodeBuf.key, res);
	}
	if (!found)
		return(-1);
	
	/*
	** OK, found it.  Now blow it away
	*/
	cachePtr = readPage(idx, CPI_CLS_PAGE(idx, nodeBuf.clusterNum));
	getCluster(idx, cachePtr, nodeBuf.clusterNum, &clusterBuf);
	getHeader(idx, cachePtr, nodeBuf.clusterNum, nodeBuf.headerNum, 
		&headerBuf);
	shiftDataPage(idx,&clusterBuf,&headerBuf,nodeBuf.recordNum,
		CPI_SHIFT_DOWN);
	(*headerBuf.numRecords) -= 1;
	if (*headerBuf.numRecords == 0)
	{
		shiftHeaderPage(idx, nodeBuf.clusterNum, nodeBuf.headerNum,
			CPI_SHIFT_DOWN);
	}
	resetMaxValue(idx, nodeBuf.clusterNum, nodeBuf.headerNum);
	if (cpiVerbose)
		cpiDumpIndex(idx);
	return(0);
}
