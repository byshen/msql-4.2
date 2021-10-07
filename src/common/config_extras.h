/* Is  u_int defined in sys/types.h */
#define HAVE_U_INT

/* Do we have the bit types */
#define HAVE_BIT_TYPES

/* do we have a isdst struct entry for timezones */
#define HAVE_ISDST

/* do rlimit work on this target */
#define HAVE_RLIMIT_NOFILE

/* How do we work out our GMT offset */
#define HAVE_GMTOFF

/* do we have sys_err */
#define HAVE_SYS_ERRLIST

/* Do we have a working (shared read/write) mmap */
#define HAVE_MMAP

/* How many args does msync need */
#define MSYNC_3

/* How do we do directory access on this target */
#define HAVE_DIRENT



/* Do we have siglist */


/* How do we pass a file descriptor using IPC */
#define NEW_BSD_MSG

/* How do we do 64 bit ints */
#define HUGE_T int64_t
#define UHUGE_T uint64_t
