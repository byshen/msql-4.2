/*
** ACL Header File	(Private)
*/


/***********************************************************************
** Standard header preamble.  Ensure singular inclusion, setup for
** function prototypes and c++ inclusion
*/

#ifndef MAIN_ACL_H

#define MAIN_ACL_H 1

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

#define	ALLOW	1
#define REJECT	2

#define	ERR(msg)	if (verbose) printf msg 

#define	DATABASE	1
#define	READ		2
#define	WRITE		3
#define	HOST		4
#define	ACCESS		5
#define OPTION		6

/***********************************************************************
** Type Definitions
*/

typedef	struct acc_s {
	char	name[50];
	int	access;
	struct	acc_s *next;
} acc_t;


typedef struct acl_s {
	char	db[NAME_LEN];
	acc_t	*host,
		*read,
		*write;
	mTable_t	*access,
		*option;
	struct	acl_s *next;
} acl_t;


/***********************************************************************
** Function Prototypes
*/

int aclLoadFile __ANSI_PROTO((int));
int aclCheckAccess __ANSI_PROTO((char*, cinfo_t*));
int aclCheckLocal __ANSI_PROTO((cinfo_t*));
int aclCheckPerms __ANSI_PROTO((int));

void aclReloadFile __ANSI_PROTO((int));
void aclSetPerms __ANSI_PROTO((int));



/***********************************************************************
** Standard header file footer.  
*/

#ifdef __cplusplus
	}
#endif /* __cplusplus */
#endif /* file inclusion */
