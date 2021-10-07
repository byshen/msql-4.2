/*
** A small test program to see if msync takes 2 or 3 args
**
**						bambi
*/

#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>

main()
{
	caddr_t	cp;
	size_t	len;
	int	flags;

#ifdef MSYNC_3
	msync(cp, len, flags);
#endif
#ifdef MSYNC_2
	msync(cp, len);
#endif
}
