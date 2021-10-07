#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>

main()
{
	char	*cp;

	cp = sys_errlist[1];
}
