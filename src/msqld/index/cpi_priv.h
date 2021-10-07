/*
** cpi_priv.h   - Clustered Page Index library (CPI)
**
** Copyright (c) 1998  Hughes Technologies Pty Ltd
** 
** This header provides the internal data structures, macro definitions
** and function prototypes used by the CPI library itself.  It is not
** intended to be included in client application code.
**
** This library was written for Mini SQL 3.
**
*/ 

#include "cpi.h"

/*************************************************************************
**************************************************************************
**
**                       Internal Structures
** 
**************************************************************************
*************************************************************************/

typedef struct cpi_superblock_struct {
	char	fileType[40];	/* Dummy entry for file(1) */
	int	magic,		/* magic number */
		version,	/* CPI version */
		keySize,	/* Size of key value */
		keyType,	/* Type of key value */
		pageSize,	/* Size of index page in bytes */
		mapSize,	/* VM Page alligned mapping size */
		vmPageSize,	/* The size of a VM page */
		cacheSize,	/* Number of cache pages */
		recsPerPage,	/* Number of tuples that will fit in a page */
		hdrsPerPage,	/* Number of page headers per page */
		numClusters,	/* Number of clusters in file */
		freeList;	/* Page number of first free page */

	double	cacheLookups,	/* Number of cache lookups */
		cacheHits;	/* Number of cache hits */
	u_int	numRecords,	/* Total number of recs in index */
		numKeys;	/* Number of distinct key values */

	char	status;		/* Closed status of index */
} cpi_sbk;



typedef struct cpi_cluster_struct {
	int 	*numHeaders,	/* Number of headers in cluster */
		*pageNum,	/* Page on which this cluster starts */
		clusterNum;	/* Cluster number */
	char 	*maxValue;	/* Max key value in this cluster */
	int	cacheSlot;
} cpi_cls;



typedef struct cpi_header_struct{
	int 	*numRecords,	/* Number of tuples in the data page */
		*pageNum,	/* Page number of data page */
		headerNum,	/* Header number */
		clusterNum;	/* Cluster in which header lives */
	char	*maxValue;	/* max key value in this page */
	int	cacheSlot;
} cpi_hdr;



typedef struct cpi_page_cache_struct{
	int	slot,		/* Cache slot number */
		age;		/* "time" since cache element last accessed */
	u_int	page;		/* Page number */
	void	*ptr;		/* Pointer to mapped page data */
} cpi_ch;


typedef struct {
        u_int   pageNum;
        cpi     *idx;
        cpi_ch  *page;
} cpi_cch;				/* "cache cache" struct */



/*************************************************************************
**************************************************************************
**
**                    General Definitions and Macros
**
** Note : The debugging macros are for debugging the CPI algorithms.
**	Setting DEBUG will generate general tracing info about the
**	API calls.  Setting VERBOSE_DEBUG will general a graphical
**	representation of the index after each modification.  Setting
**	FORCE_SMALL_PAGES limits the number of headers and records per
**	page to exercise the page handling routines.
**************************************************************************
*************************************************************************/


#define CPI_MAGIC		0x01020304
#define CPI_VERSION		1
#define	CPI_DEFAULT_CACHE	64

#define	CPI_FORCE		1
#define CPI_IF_NEEDED		2

#define	CPI_SHIFT_UP		1
#define CPI_SHIFT_DOWN		-1

#define CPI_CCH_SIZE		64



#ifdef DEBUG
	static int cpiDebug = 1;
#else
	static int cpiDebug = 0;
#endif
#ifdef VERBOSE_DEBUG
	static int cpiVerbose = 1;
#else
	static int cpiVerbose = 0;
#endif
#ifdef CACHE_DEBUG
	static int cpiDebugCache = 1;
#else
	static int cpiDebugCache = 0;
#endif



/* convenience macro for the opaque superblock */
#define CPI_SBK(idx) \
	((cpi_sbk *)((idx)->opaque))

/* Byte offset in file for specified page */
#define CPI_PAGE_OFFSET(idx,page) \
 	(CPI_SBK(idx)->pageSize * (page))

/* Work out the cluster header page for a given cluster */
#define	CPI_CLS_PAGE(idx, cluster) \
        (((cluster) * ( CPI_SBK(idx)->hdrsPerPage + 1)) + 1)

/* Size of a data rec (a record including the data) */
#define	CPI_REC_SIZE(idx) \
	(CPI_SBK(idx)->keySize + sizeof(u_int))

/* Size of a key */
#define	CPI_KEY_SIZE(idx) \
	(CPI_SBK(idx)->keySize)

/* Size of a header */
#define	CPI_HDR_SIZE(idx) \
	( CPI_SBK(idx)->keySize  +  sizeof(u_int) + sizeof(u_int))

/* Size of a cluster record */
#define	CPI_CLS_SIZE(idx) \
	( CPI_SBK(idx)->keySize  + (2 * sizeof(u_int)))

#define CPI_CACHE_HIT_RATE(idx) \
        (int)(CPI_SBK(idx)->cacheLookups != 0 \
		? (CPI_SBK(idx)->cacheHits / CPI_SBK(idx)->cacheLookups * 100)\
		: 0)


/*************************************************************************
**************************************************************************
**
**                       Function Prototypes
**
**************************************************************************
*************************************************************************/

static char *getFreePage __ANSI_PROTO((cpi*, int*));
static int shiftDataPage __ANSI_PROTO((cpi*, cpi_cls*, cpi_hdr*, int, int));
static void shiftHeaderPage __ANSI_PROTO((cpi*, int, int, int));
static void setMaxValue __ANSI_PROTO((cpi*, int,int,char *,int));
static void splitPages __ANSI_PROTO((cpi*, int,int,int,int));
static int writeRecord __ANSI_PROTO((cpi*, int, int,int, cpi_nod*));
static int privateLookup __ANSI_PROTO((cpi*,char*,cpi_cls*,cpi_hdr*,
	cpi_nod*,int));


 



/*************************************************************************
**************************************************************************
**
**                       TYPE HANDLING MACROS
**
**************************************************************************
*************************************************************************/
 
#define intCompare(v1,v2,res)						\
	if ((int)*(int*)(v1) > (int)*(int*)(v2)) res = 1;		\
	if ((int)*(int*)(v1) < (int)*(int*)(v2)) res = -1;		\
	if ((int)*(int*)(v1) == (int)*(int*)(v2)) res = 0;		

#define uintCompare(v1,v2,res)						\
	if ((u_int)*(u_int*)(v1) > (u_int)*(u_int*)(v2)) res = 1;	\
	if ((u_int)*(u_int*)(v1) < (u_int)*(u_int*)(v2)) res = -1;	\
	if ((u_int)*(u_int*)(v1) == (u_int)*(u_int*)(v2)) res = 0;

#define realCompare(v1,v2,res)						\
	if ((double)*(double*)(v1) > (double)*(double*)(v2)) res = 1;	\
	if ((double)*(double*)(v1) < (double)*(double*)(v2)) res = -1;	\
	if ((double)*(double*)(v1) == (double)*(double*)(v2)) res = 0;	


#define charCompare(v1,v2, res)						\
	{ char    *cp1 = v1, *cp2 = v2;					\
        while (*cp1 == *cp2) {						\
                if (*cp1 == 0) {res = 0;break;}				\
                cp1++; cp2++;						\
        } 								\
	if ((unsigned char )*cp1 > (unsigned char)*cp2) res = 1;	\
	if ((unsigned char )*cp1 < (unsigned char)*cp2) res = -1; 	\
	}



#define byteCompare(v1,v2,l,res) 					\
	{ u_char *cp1 = (u_char*)v1, *cp2 = (u_char *)v2; int len = l;	\
        while (*cp1 == *cp2 && len) {					\
                cp1++; cp2++; len--;  					\
        }       							\
        if (len == 0) res = 0;						\
	else {								\
        	if (*cp1 > *cp2) res = 1;				\
                else res = -1;						\
	}}


#define compareValues(idx, val1, val2, res)				\
	{ switch(CPI_SBK(idx)->keyType) { 				\
		case CPI_CHAR: charCompare(val1, val2, res);		\
			break;						\
		case CPI_INT: intCompare(val1, val2, res);		\
			break;						\
		case CPI_UINT: uintCompare(val1, val2, res);		\
			break;						\
		case CPI_REAL: realCompare(val1, val2, res);		\
			break;						\
		default: byteCompare(val1,val2,				\
			CPI_SBK(idx)->keySize,res);			\
	}}

