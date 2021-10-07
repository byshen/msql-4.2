/*
**	Test program to determine who to calculate the offset from GMT
*/

#include <sys/types.h>
#include <time.h>

main()
{
	struct	tm	gmt;
	int	offset;

#ifdef HAVE_GMTOFF
        offset = gmt.tm_gmtoff;
#endif
#ifdef HAVE_TIMEZONE
        offset = timezone;
#endif

}
