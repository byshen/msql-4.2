/*
** cpi.h   - Clustered Page Index library (CPI)
**
** Copyright (c) 1998  Hughes Technologies Pty Ltd
**
** This header provides the external data types, macro definitions 
** and function prototypes for use by external applications utilising
** the CPI data format.
**
** This library was written for Mini SQL 3.
**
*/


#ifndef	__CPI_H
#define	__CPI_H

#include <sys/types.h>

/*
** Data types associated with index key values
*/
#define	CPI_CHAR	1
#define	CPI_INT		2
#define CPI_UINT	3
#define CPI_REAL	4
#define CPI_BYTE	5


/*
** Node lookup modifiers
*/
#define	CPI_EXACT	1
#define CPI_CLOSEST	2


/*
** Standard error return codes
*/
#define CPI_OK			0
#define	CPI_ERR_DUP		-1
#define CPI_ERR_NOT_FOUND	-2
#define CPI_ERR_UNKNOWN		-3


/*
** Index creation flags
*/
#define	CPI_UNIQUE	1
#define	CPI_DUP		2



/*
** Index struct
*/
typedef struct {
	int	fd;		/* file descriptor of open index */
	char	*cache,		/* Pointer to per index page cache */
		*path;		/* Path of index file */
	u_int	clusterNum,	/* Last lookup - cluster number */
		headerNum,	/* Last lookup - header number */
		recordNum;	/* Last lookup - record number */
	void	*opaque;	/* Internal, private state information */
} cpi;



typedef struct {
	char	*key;		/* Key value of node */
	u_int	data,		/* Data value of node */
		clusterNum,	/* Cluster number of node */
		headerNum,	/* Header number of node */
		recordNum;	/* Record number of node */
} cpi_nod;



typedef struct {
	u_int	clusterNum,	/* Cluster number of current node */
		headerNum,	/* Header number of current node */
		recordNum;	/* Record number of current node */
} cpi_cur;


typedef struct {
        u_int   cacheSize,
                pageSize;
} cpi_env;




/*
** API Function prototypes
*/
#ifdef __ANSI_PROTO
#  undef __ANSI_PROTO
#endif

#if defined(__STDC__) || defined(__cplusplus)
#  define __ANSI_PROTO(x)       x
#else
#  define __ANSI_PROTO(x)       ()
#endif

int	cpiCreate __ANSI_PROTO((char *,int,int,int,int, cpi_env *));
cpi	*cpiOpen __ANSI_PROTO((char *, cpi_env *));
void	cpiSync __ANSI_PROTO((cpi *));
void	cpiClose __ANSI_PROTO((cpi *));

int	cpiLookup __ANSI_PROTO((cpi*,char*,cpi_nod*,int));
int	cpiGetFirst __ANSI_PROTO((cpi*, cpi_nod*));
int	cpiGetLast __ANSI_PROTO((cpi*, cpi_nod*));
int	cpiGetNext __ANSI_PROTO((cpi*, cpi_cur*, cpi_nod*));
int	cpiGetPrev __ANSI_PROTO((cpi*, cpi_cur*, cpi_nod*));
void	cpiSetCursor __ANSI_PROTO((cpi*, cpi_cur*));

int	cpiInsert __ANSI_PROTO((cpi*,char*,u_int));
int	cpiDelete __ANSI_PROTO((cpi*, char*, u_int));
int	cpiExists __ANSI_PROTO((cpi*, char*, u_int));

void	cpiDumpIndex __ANSI_PROTO((cpi*));
int	cpiTestIndex __ANSI_PROTO((cpi*));
void	cpiPrintIndexStats __ANSI_PROTO((cpi*));

#endif	/* __CPI_H */
