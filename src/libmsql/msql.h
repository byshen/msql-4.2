/*
** mSQL API Library Header File	(Public)
*/


/***********************************************************************
** Standard header preamble.  Ensure singular inclusion, setup for
** function prototypes and c++ inclusion
*/

#ifndef MSQL_H

#define MSQL_H 1

#if defined(__STDC__) || defined(__cplusplus)
#  define __ANSI_PROTO(x)       x
#else
#  define __ANSI_PROTO(x)       ()
#endif

#ifdef __cplusplus
extern "C" {
#endif


#include <sys/types.h>
#include <sys/time.h>	/* needed for time_t prototypes */

/***********************************************************************
** Macro Definitions
*/

#ifndef APIENTRY
#  if defined(_OS_OS2)
#    ifdef BCPP
#      define _System   _syscall
#      define _Optlink
#    endif
#    ifdef __EMX__
#      define _System   
#    endif
#    define APIENTRY _System
#  endif /* _OS_OS2 */
#  if defined(_OS_WIN32)
#    define APIENTRY __stdcall
#  endif /* _OS_WIN32 */
#  if defined(_OS_UNIX)
#    define APIENTRY
#  endif
#  if !defined(_OS_OS2) && !defined(_OS_UNIX) && !defined(_OS_WIN32)
#    define APIENTRY
#  endif
#endif

#define	msqlNumRows(res) res->numRows
#define	msqlNumFields(res) res->numFields


#define INT32_TYPE		1
#define CHAR_TYPE		2
#define REAL_TYPE		3
#define IDENT_TYPE		4
#define NULL_TYPE		5
#define TEXT_TYPE		6
#define DATE_TYPE		7
#define UINT32_TYPE		8
#define MONEY_TYPE		9
#define TIME_TYPE		10
#define IPV4_TYPE		11
#define	INT64_TYPE		12
#define UINT64_TYPE		13
#define	INT8_TYPE		14
#define	INT16_TYPE		15
#define	UINT8_TYPE		16
#define	UINT16_TYPE		17
#define BYTE_TYPE		18
#define	CIDR4_TYPE		19
#define	CIDR6_TYPE		20
#define MILLITIME_TYPE		21
#define DATETIME_TYPE		22
#define MILLIDATETIME_TYPE	23
#define IPV6_TYPE		24

#define LAST_REAL_TYPE		24
#define IDX_TYPE		253
#define SYSVAR_TYPE		254
#define	ANY_TYPE		255

/* For compatability with 3.x */
#define INT_TYPE		INT32_TYPE
#define UINT_TYPE		UINT32_TYPE

#define NOT_NULL_FLAG   	1
#define UNIQUE_FLAG		2

#define IS_UNIQUE(n)	(n & UNIQUE_FLAG)
#define IS_NOT_NULL(n)	(n & NOT_NULL_FLAG)

#define	MSQL_PKT_LEN	(128*1024)

#define MSQL_TABLE_OK		0
#define MSQL_TABLE_BAD_INDEX	1
#define MSQL_TABLE_UNKNOWN	255


/*
** NOTE : For backward compatability this will catch most problems
** caused by the change of error handling
*/

#define msqlErrMsg msqlGetErrMsg(NULL)

#ifdef MSQL3_NEED_TYPE_NAMES
	char msqlTypeNames[][12] =
       {"???", "int", "char","real","ident","null","text","date","uint",
       "money","time","ip","int64","uint64","int8","int16","cidr4",
	"cidr6", "???"};
#else
	extern char msqlTypeNames[][12];
#endif

/***********************************************************************
** Type Definitions
*/

typedef	char	** m_row;

typedef struct field_s {
	char	*name,
		*table;
	int	type,
		length,
		flags;
} m_field;


typedef struct 	m_seq_s {
	int	step,
		value;
} m_seq;


typedef	struct	m_data_s {
	int	width;
	m_row	data;
	struct	m_data_s *next;
} m_data;

typedef struct m_fdata_s {
	m_field	field;
	struct m_fdata_s *next;
} m_fdata;



typedef struct result_s {
        m_data 	*queryData,
                *cursor;
	m_fdata	*fieldData,
		*fieldCursor;
	int	numRows,
		numFields;
} m_result;


typedef struct m_tinfo_s {
	off_t	rowLen,
		dataSize,
		fileSize;
	u_int	activeRows,
		totalRows;
} m_tinfo;


/***********************************************************************
** Function Prototypes
*/


	int	msqlLoadConfigFile __ANSI_PROTO((char *));
        char  * APIENTRY msqlGetErrMsg __ANSI_PROTO((char *));    

#if defined(_OS_OS2) || defined(_OS_WIN32)
        int     APIENTRY msqlUserConnect __ANSI_PROTO((char *, char *)); 
#endif
#if defined(_OS_WIN32)
	char * APIENTRY msqlGetWinRegistryEntry __ANSI_PROTO((char*,char*,int));
#endif

	int 	APIENTRY msqlConnect __ANSI_PROTO((char *));
	int 	APIENTRY msqlSelectDB __ANSI_PROTO((int, char*));
	int 	APIENTRY msqlQuery __ANSI_PROTO((int, char*));
	int 	APIENTRY msqlExplain __ANSI_PROTO((int, char*));
	int 	APIENTRY msqlCreateDB __ANSI_PROTO((int, char*));
	int 	APIENTRY msqlDropDB __ANSI_PROTO((int, char*));
	int 	APIENTRY msqlShutdown __ANSI_PROTO((int));
	int 	APIENTRY msqlGetProtoInfo __ANSI_PROTO((void));
	int 	APIENTRY msqlReloadAcls __ANSI_PROTO((int));
	int 	APIENTRY msqlGetServerStats __ANSI_PROTO((int));
	int 	APIENTRY msqlCopyDB __ANSI_PROTO((int, char*, char*));
	int 	APIENTRY msqlMoveDB __ANSI_PROTO((int, char*, char*));
	int 	APIENTRY msqlDiffDates __ANSI_PROTO((char*, char*));
	int 	APIENTRY msqlLoadConfigFile __ANSI_PROTO((char*));
	int 	APIENTRY msqlGetIntConf __ANSI_PROTO((char*,char*));
        int	APIENTRY msqlCheckTable __ANSI_PROTO((int,char*,char*));
        int	APIENTRY msqlExportTable __ANSI_PROTO((int,char*,char*,char*));
        int	APIENTRY msqlImportTable __ANSI_PROTO((int,char*,char*));
	char 	* APIENTRY msqlGetCharConf __ANSI_PROTO((char*,char*));
	char 	* APIENTRY msqlGetServerInfo __ANSI_PROTO((void));
	char 	* APIENTRY msqlGetHostInfo __ANSI_PROTO((void));
	char 	* APIENTRY msqlUnixTimeToTime __ANSI_PROTO((int));
	char 	* APIENTRY msqlUnixTimeToDate __ANSI_PROTO((int));
	char 	* APIENTRY msqlUnixTimeToDatetime __ANSI_PROTO((time_t));
	char 	* APIENTRY msqlUnixTimeToMillitime __ANSI_PROTO((int));
	char 	* APIENTRY msqlUnixTimeToMillidatetime __ANSI_PROTO((int));
	char 	* APIENTRY msqlSumTimes __ANSI_PROTO((char*, char*));
	char 	* APIENTRY msqlDiffTimes __ANSI_PROTO((char*, char*));
	char 	* APIENTRY msqlDateOffset __ANSI_PROTO((char*, int,int,int));
	char 	* APIENTRY msqlTypeName __ANSI_PROTO((int));
	void	APIENTRY msqlClose __ANSI_PROTO((int));
	void 	APIENTRY msqlDataSeek __ANSI_PROTO((m_result*, int));
	void 	APIENTRY msqlFieldSeek __ANSI_PROTO((m_result*, int));
	void 	APIENTRY msqlFreeResult __ANSI_PROTO((m_result*));
        m_row   APIENTRY msqlFetchRow __ANSI_PROTO((m_result*));
	m_seq	* APIENTRY msqlGetSequenceInfo __ANSI_PROTO((int, char*));
	m_field	* APIENTRY msqlFetchField __ANSI_PROTO((m_result *));
	m_tinfo * APIENTRY msqlTableInfo __ANSI_PROTO((int, char*));
	m_result * APIENTRY msqlListDBs __ANSI_PROTO((int));
	m_result * APIENTRY msqlListTables __ANSI_PROTO((int));
	m_result * APIENTRY msqlListFields __ANSI_PROTO((int, char*));
	m_result * APIENTRY msqlListIndex __ANSI_PROTO((int, char*, char*));
	m_result * APIENTRY msqlStoreResult __ANSI_PROTO((void));
	time_t	APIENTRY msqlDateToUnixTime __ANSI_PROTO((char *));
	time_t	APIENTRY msqlTimeToUnixTime __ANSI_PROTO((char *));
	time_t	APIENTRY msqlDatetimeToUnixTime __ANSI_PROTO((char *));
	time_t	APIENTRY msqlMillitimeToUnixTime __ANSI_PROTO((char *));
	time_t	APIENTRY msqlMillidatetimeToUnixTime __ANSI_PROTO((char *));




/***********************************************************************
** Standard header file footer.  
*/

#ifdef __cplusplus
	}
#endif /* __cplusplus */
#endif /* file inclusion */
