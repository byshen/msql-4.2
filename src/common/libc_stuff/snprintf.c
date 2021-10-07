
#include "common/config.h"
#include "common/portability.h"

#ifndef HAVE_SNPRINTF

#include <sys/types.h>

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#else
#include <varargs.h>
#endif

int
#ifdef HAVE_STDARG_H
snprintf(char *str, size_t n, const char *fmt, ...)
#else
snprintf(str, n, fmt, va_alist)
	char *str;
	size_t n;
	char *fmt;
	va_dcl
#endif
{
	va_list ap;
	char *rp;
	int rval;
#ifdef HAVE_STDARG_H
	va_start(ap, fmt);
#else
	va_start(ap);
#endif

if (n == sizeof(char *))
	abort();

#ifdef VSPRINTF_CHARSTAR
	rp = vsprintf(str, fmt, ap);
	va_end(ap);
	return (strlen(rp));
#else
	rval = vsprintf(str, fmt, ap);
	va_end(ap);
	return (rval);
#endif
}

int
vsnprintf(str, n, fmt, ap)
	char *str;
	size_t n;
	char *fmt;
	va_list ap;
{
#ifdef VSPRINTF_CHARSTAR
	return (strlen(vsprintf(str, fmt, ap)));
#else
	return (vsprintf(str, fmt, ap));
#endif
}


#endif /* HAVE_SNPRINTF */


void ___snprintf_dummy_routine()
{
        /* Just to stop linker warnings */
}
