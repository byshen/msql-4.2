/*
** msqld Header File	(Shared between msqld modules)
*/


/***********************************************************************
** Standard header preamble.  Ensure singular inclusion, setup for
** function prototypes and c++ inclusion
*/

#ifndef MSQLD_H

#define MSQLD_H 1

#if defined(__STDC__) || defined(__cplusplus)
#  define __ANSI_PROTO(x)       x
#else
#  define __ANSI_PROTO(x)       ()
#endif

#ifdef __cplusplus
extern "C" {
#endif



/***********************************************************************
** Macro Definitions
*/

#define	PKT_LEN		(128*1024)	/* Max size of client/server packet */
#define MAX_ERR_MSG	200		/* Max len of error msg */
#define	OFB_SIZE	(128-sizeof(u_int))/* Size of overflow buffers */
#define NUM_INDEX	5		/* Max indices per table */
#define MAX_INDEX_WIDTH	10		/* Max fields per index */
#define MAX_QUERY_LEN	10240
#define	MAX_CONNECTIONS	256

#define CACHE_FDS	(CACHE_SIZE * (NUM_INDEX + 2))

#define VC_HEAD_SIZE	(sizeof(int)+sizeof(u_int))

#define	MSQL_IDX_AVL	1

#define	EQ_OP		1
#define	LT_OP		2
#define	GT_OP		3
#define	NE_OP		4
#define	LE_OP		5
#define GE_OP		6
#define LIKE_OP		7
#define NOT_LIKE_OP	8
#define RLIKE_OP	9
#define CLIKE_OP	10
#define SLIKE_OP	11
#define NOT_RLIKE_OP	12
#define NOT_CLIKE_OP	13
#define NOT_SLIKE_OP	14
#define BETWEEN_OP	15

#define	NO_BOOL		0
#define	AND_BOOL	1
#define	OR_BOOL		2


#define	MEM_ALIGN	4
#define NO_POS          0xFFFFFFFF

#define QUIT		1
#define	INIT_DB		2
#define QUERY		3
#define DB_LIST		4
#define TABLE_LIST	5
#define FIELD_LIST	6
#define	CREATE_DB	7
#define DROP_DB		8
#define RELOAD_ACL	9
#define SHUTDOWN	10
#define INDEX_LIST	11
#define	SERVER_STATS	12
#define	SEQ_INFO	13
#define	MOVE_DB		14
#define	COPY_DB		15
#define	EXPORT		16
#define	EXPLAIN		17
#define CHECK_TABLE	18
#define	IMPORT		19
#define TABLE_INFO	20

#define NO_REMAP	0
#define DATA_REMAP	1
#define KEY_REMAP	2
#define STACK_REMAP	4
#define FULL_REMAP	0xffff

#define DEST_CLIENT	1
#define DEST_TABLE	2

#define	NO_ACCESS	0
#define	READ_ACCESS	1
#define	WRITE_ACCESS	2
#define RW_ACCESS	READ_ACCESS | WRITE_ACCESS

#define	IGNORE_IDENT	1
#define KEEP_IDENT	0

#define	NO_EXPLAIN	0
#define	DO_EXPLAIN	1

#define	QUERY_FIELDS_ONLY 1
#define ALL_FIELDS 2

#define MALLOC_BLK      1
#define MMAP_BLK        2

#define HEADER_SIZE     sizeof(hdr_t)
#define SBLK_SIZE       sizeof(sblk_t)

#define	BOOL_FALSE	0
#define BOOL_TRUE	1

#define	LAYOUT_FLAT	1
#define LAYOUT_HASH	2


#define safeFree(x)	if(x)free(x)

/* inlined, unaligned, 4-byte copy */
#define bcopy4(s,d) \
      ((((unsigned char *)d)[0] = ((unsigned char *)s)[0]), \
       (((unsigned char *)d)[1] = ((unsigned char *)s)[1]), \
       (((unsigned char *)d)[2] = ((unsigned char *)s)[2]), \
       (((unsigned char *)d)[3] = ((unsigned char *)s)[3]))

/* inlined, unaligned, 8-byte copy */
#define bcopy8(s,d) \
      ((((unsigned char *)d)[0] = ((unsigned char *)s)[0]), \
       (((unsigned char *)d)[1] = ((unsigned char *)s)[1]), \
       (((unsigned char *)d)[2] = ((unsigned char *)s)[2]), \
       (((unsigned char *)d)[3] = ((unsigned char *)s)[3]), \
       (((unsigned char *)d)[4] = ((unsigned char *)s)[4]), \
       (((unsigned char *)d)[5] = ((unsigned char *)s)[5]), \
       (((unsigned char *)d)[6] = ((unsigned char *)s)[6]), \
       (((unsigned char *)d)[7] = ((unsigned char *)s)[7]))


/***********************************************************************
** Type Definitions
*/


typedef struct {
        unsigned long   high,
                        low;
} i64_t;


typedef struct ident_s {
        char    	seg1[NAME_LEN + 1],
                	seg2[NAME_LEN + 1];
} mIdent_t;

typedef struct val_s {
        struct val_u {
		int8_t	int8Val;
		int16_t	int16Val;
                int32_t	int32Val;
		int64_t	int64Val;
                u_char  *charVal;
                double  realVal; 
                mIdent_t *identVal;
		void	*byteVal;
        } val;
        int     	type,
                	nullVal,
                	dataLen;
        char    	precision;
} mVal_t;


typedef struct tname_s {
	char		name[NAME_LEN + 1],
			cname[NAME_LEN + 1];
	int		done;
	struct tname_s 	*next;
} mTable_t;


typedef struct finfo_ps {
	char 		name[NAME_LEN + 1],
			outputName[NAME_LEN + 1];
	int		functNum,
			flist[MAX_FIELDS + 1],
			returnType,
			functType,
			sequence;
	struct field_ps	*paramHead,
			*paramTail;
} mFinfo_t;


typedef struct field_ps {		
	char		table[NAME_LEN + 1],
			name[NAME_LEN + 1];
	mVal_t 		*value;
	mFinfo_t	*function;
	int		type,
			sysvar,
			length,
			dataLength,
			offset,
			null,
			flags,
			fieldID,
			literalParamFlag,
			functResultFlag;
	u_int		overflow;
	void		*entry;
	struct field_ps *next;
} mField_t;

/*
** Insert value structures (for single and bulk inserts)
*/

typedef struct val_list_s {
	int		offset;
	mVal_t 		*value;
	struct val_list_s *next;
} mValList_t;


/* 
** Where clause list element 
*/
typedef struct cond_s {			
	char		table[NAME_LEN + 1],
			name[NAME_LEN + 1];
	int     	clist[MAX_FIELDS];	/* only used in the head */
	mVal_t 		*value,
			*maxValue;
	int		op,
			bool,
			type,
			length,
			sysvar,
			fieldID;
	struct cond_s 	*next,
			*subCond;
} mCond_t;


/* 
** Order clause list element 
*/
typedef struct order_s {		
	char		table[NAME_LEN + 1],
			name[NAME_LEN + 1];
	int		dir,
			type,
			length;
	void		*entry;
	struct order_s 	*next;
} mOrder_t;




typedef struct seq_s {
	char		table[NAME_LEN + 1];
	int		step;
	u_int		value;
} mSeq_t;


/*
** Shared config info
*/
typedef struct gconfig_s {
	char		*instDir,
			*dbDir;
	int		readOnly,
			sortMaxMem,
			tableCache,
			cacheDescriptors,
			msyncTimer,
			hasBroker,
			needFileLock;
} mConfig_t;


/***********************************************************************
** Type Definitions
*/

/*
** On-disk data structures
*/

/* Table row header struct */
typedef struct  hdr_s {
        char    	active;
        time_t  	timestamp;
} hdr_t;


/* Table row struct */
typedef struct row_s {
        int     	rowID;
        hdr_t   	*header;
        u_char  	*data,
                	*buf;
} row_t;


/* Atomic sequence structure */
typedef struct {
        int     	step;
        u_int   	value;
} seq_t;


/* Data file superblock */
typedef struct sblk_s {
        char    	version;
        u_int   	freeList;
        u_int   	numRows;
        u_int   	activeRows;
        seq_t   	sequence;
	off_t		dataSize;
} sblk_t;


/*
** Internal index structure
*/
typedef struct mindex_s {
        char    	table[NAME_LEN + 1],
                	name[NAME_LEN + 1],
                	unique, 
                	*buf;
        int     	fields[MAX_INDEX_WIDTH + 1],
                	length,
                	fieldCount,
                	idxType,
                	keyType;
        idx_hnd 	handle;
        idx_env 	environ;
        struct mindex_s *next;
} mIndex_t;
 


/*
** Structures used by the parser (and everything else)
*/

typedef struct cstk_s {
        mCond_t  	*head,
                	*tail;
        struct cstk_s	*next;
} cstk_t;

typedef struct {
	int		command,
			rowLimit,
			rowOffset,
			selectWildcard,
			selectDistinct,
			explainOnly,
			clientSock,
			fieldCount,
			queryTime,
			functSeq,
			numConds;
	char		*targetTable,
			*curDB,
			*curUser;
	mTable_t	*tableHead, *tableTail;
	mField_t	*fieldHead, *fieldTail, *lastField;
	mCond_t		*condHead, *condTail;
	mOrder_t	*orderHead, *orderTail;
	mIndex_t	indexDef;
	mSeq_t		sequenceDef;
	mValList_t	*insertValHead, *insertValTail;
	char		*queryText;
} mQuery_t;



/*
** Connection tracking structure
*/
typedef struct cinfo_s {
        char    	*db,
                	*host,
                	*user,
			*clientIP;
        int     	access,
                	sock;
        u_int   	connectTime,
                	lastQuery,
                	numQueries;
} cinfo_t;



/*
** Table cache structure
*/
typedef struct cache_s {
        char    	db[NAME_LEN + 1],
                	cname[NAME_LEN + 1],
                	table[NAME_LEN + 1],
                	resInfo[4 * (NAME_LEN+1)],
                	dirty;
        row_t   	row;
        int     	age,
                	dataFD,
                	overflowFD,
                	rowDataLen,
                	rowLen,
                	result,
			fileLayout,
			dirHash;
        mField_t 	*def;
        mIndex_t 	*indices;
        int     	remapData,
                	remapOverflow;
        caddr_t 	dataMap,
                	overflowMap;
        off_t   	size,
                	overflowSize;
        sblk_t  	*sblk;
} cache_t;


/***********************************************************************
** Function Prototypes
*/


/***********************************************************************
** Standard header file footer.  
*/

#ifdef __cplusplus
	}
#endif /* __cplusplus */
#endif /* file inclusion */
