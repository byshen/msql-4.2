/*
**	portability.h	- 
**
**
** Copyright (c) 1993-95  David J. Hughes
** Copyright (c) 1995  Hughes Technologies Pty Ltd
**
** Permission to use, copy, and distribute for non-commercial purposes,
** is hereby granted without fee, providing that the above copyright
** notice appear in all copies and that both the copyright notice and this
** permission notice appear in supporting documentation.
**
** This software is provided "as is" without any expressed or implied warranty.
**
*/


#ifndef PORTABILITY_H
#define PORTABILITY_H 

#include <common/config.h>
#include <limits.h>

#ifndef APIENTRY
#  if defined(OS2)
#    ifdef BCPP
#      define _System   _syscall
#      define _Optlink
#    endif
#    ifdef __EMX__
#      define _System   
#    endif
#    define APIENTRY _System
#  elif defined(WIN32)
#    define APIENTRY __stdcall
#  else
#    define APIENTRY
#  endif
#endif

#ifndef HAVE_BCOPY
#  undef 	bzero
#  undef 	bcopy
#  undef 	bcmp
#  define	bzero(a,l)	memset((void *)a,0,(size_t)l)
#  define	bcopy(s,d,l)	memcpy(d,s,(size_t)l)
#  define	bcmp		memcmp
#endif


#ifndef HAVE_RINDEX
#  undef	index
#  undef	rindex
#  define	index		strchr
#  define	rindex		strrchr
#endif

#ifndef HAVE_RANDOM
#  undef	random
#  undef	srandom
#  define	random		rand
#  define	srandom		srand
#endif

#ifdef HAVE_SELECT_H
	/*
	** AIX has a struct fd_set and can be distinguished by
	** its needing <select.h>
	*/
	typedef struct fd_set fd_set
#endif

#ifndef HAVE_U_INT
	typedef	unsigned int u_int;
#endif


#ifndef HAVE_FTRUNCATE
	/*
	** SCO ODT doesn't have ftruncate() !!! Have to use old Xenix stuff
	*/
#       undef	ftruncate
#  	define 	ftruncate	chsize
#endif


#ifndef BYTE_ORDER
#	define LITTLE_ENDIAN   1234            /* LSB first: i386, vax */
#	define BIG_ENDIAN      4321            /* MSB first: 68000, ibm, net */
#	define BYTE_ORDER      BIG_ENDIAN      /* Set for your system. */
#endif


#ifndef USHRT_MAX
#	define USHRT_MAX               0xFFFF
#	define ULONG_MAX               0xFFFFFFFF
#endif

#ifndef O_ACCMODE                       /* POSIX 1003.1 access mode mask. */
#	define O_ACCMODE       (O_RDONLY|O_WRONLY|O_RDWR)
#endif

#ifndef	O_BINARY
#	define O_BINARY			0
#endif

#ifndef _POSIX2_RE_DUP_MAX              /* POSIX 1003.2 RE limit. */
#	define _POSIX2_RE_DUP_MAX      255
#endif

/*
 * If you can't provide lock values in the open(2) call.  Note, this
 * allows races to happen.
 */
#ifndef O_EXLOCK                        /* 4.4BSD extension. */
#	define O_EXLOCK        0
#endif

#ifndef O_SHLOCK                        /* 4.4BSD extension. */
#	define O_SHLOCK        0
#endif

#ifndef EFTYPE
#	define EFTYPE          EINVAL   /* POSIX 1003.1 format errno. */
#endif

#ifndef WCOREDUMP                       /* 4.4BSD extension */
#	define WCOREDUMP(a)    0
#endif

#ifndef STDERR_FILENO
#	define STDIN_FILENO    0               /* ANSI C #defines */
#	define STDOUT_FILENO   1
#	define STDERR_FILENO   2
#endif

#ifndef SEEK_END
#	define SEEK_SET        0               /* POSIX 1003.1 seek values */
#	define SEEK_CUR        1
#	define SEEK_END        2
#endif

#ifndef TCSASOFT                        /* 4.4BSD extension. */
#	define TCSASOFT        0
#endif

#ifndef MAX                             /* Usually found in <sys/param.h>. */
#	define MAX(_a,_b)      ((_a)<(_b)?(_b):(_a))
#endif
#ifndef MIN                             /* Usually found in <sys/param.h>. */
#	define MIN(_a,_b)      ((_a)<(_b)?(_a):(_b))
#endif

/* Default file permissions. */
#ifndef DEFFILEMODE                     /* 4.4BSD extension. */
#	define DEFFILEMODE (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
#endif

#ifndef S_ISDIR                         /* POSIX 1003.1 file type tests.  */
#	define S_ISDIR(m)      ((m & 0170000) == 0040000)    /* directory */
#	define S_ISCHR(m)      ((m & 0170000) == 0020000)    /* char special*/
#	define S_ISREG(m)      ((m & 0170000) == 0100000)    /* regular file */
#	define S_ISFIFO(m)     ((m & 0170000) == 0010000)    /* fifo */
#endif
#ifndef S_ISBLK                         /* POSIX 1003.1 file type tests.  */
#	define S_ISBLK(m)      ((m & 0170000) == 0060000)    /* block special*/
#endif
#ifndef S_ISLNK                         /* BSD POSIX 1003.1 extensions */
#	define S_ISLNK(m)      ((m & 0170000) == 0120000)    /* symbolic link */
#endif
#ifndef S_ISSOCK                        /* BSD POSIX 1003.1 extensions */
#	define S_ISSOCK(m)     ((m & 0170000) == 0140000)    /* socket */
#endif

/* The type of a va_list. */
#ifndef _BSD_VA_LIST_                   /* 4.4BSD #define. */
#	define _BSD_VA_LIST_   char *
#endif

#ifdef MSYNC_2
#	define	MSYNC(a,l,f)	msync(a,l)
#else
#	define	MSYNC(a,l,f)	msync(a,l,f)
#endif

#define MAX_HUGE	18446744073709551615u

#ifndef	MAX_PATH_LEN
#	define	MAX_PATH_LEN	255
#endif

#endif /* PORTABILTIY_H */
