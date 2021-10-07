/*
**	debug.c	- Shared debug output routines
**
**
** Copyright (c) 1993-95 David J. Hughes
** Copyright (c) 1995 Hughes Technologies Pty Ltd
**
** Permission to use, copy, and distribute for non-commercial purposes,
** is hereby granted without fee, providing that the above copyright
** notice appear in all copies and that both the copyright notice and this
** permission notice appear in supporting documentation.
**
** This software is provided "as is" without any expressed or implied warranty.
**
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <common/portability.h>

#ifdef HAVE_STDARG_H
#  include <stdarg.h>
#else
#  include <varargs.h>
#endif

#include "debug.h"

int 		debugLevel=0;
extern char	PROGNAME[];
int		titleFlag = 0;
char		msqlDebugBuf[10 * 1024];


void initDebug()
{
	char	*env,
		*tmp,
		*tok;

	env = getenv("MSQL_DEBUG");
	if(env)
	{
		tmp = (char *)strdup(env);
	}
	else
		return;
	fprintf(stderr,
		"\n-------------------------------------------------------\n");
	fprintf(stderr,
		"MSQL_DEBUG found.  %s started with the following :-\n\n",
		PROGNAME);
	tok = (char *)strtok(tmp,":");
	while(tok)
	{
		if (strcmp(tok,"cache") == 0)
		{
			debugLevel |= MOD_CACHE;
			fprintf(stderr,"Debug level : cache\n");
		}
		if (strcmp(tok,"varchar") == 0)
		{
			debugLevel |= MOD_TEXT;
			fprintf(stderr,"Debug level : varchar\n");
		}
		if (strcmp(tok,"query") == 0)
		{
			debugLevel |= MOD_QUERY;
			fprintf(stderr,"Debug level : query\n");
		}
		if (strcmp(tok,"general") == 0)
		{
			debugLevel |= MOD_GENERAL;
			fprintf(stderr,"Debug level : general\n");
		}
		if (strcmp(tok,"error") == 0)
		{
			debugLevel |= MOD_ERR;
			fprintf(stderr,"Debug level : error\n");
		}
		if (strcmp(tok,"key") == 0)
		{
			debugLevel |= MOD_KEY;
			fprintf(stderr,"Debug level : key\n");
		}
		if (strcmp(tok,"malloc") == 0)
		{
			debugLevel |= MOD_MALLOC;
			fprintf(stderr,"Debug level : malloc\n");
		}
		if (strcmp(tok,"trace") == 0)
		{
			debugLevel |= MOD_TRACE;
			fprintf(stderr,"Debug level : trace\n");
		}
		if (strcmp(tok,"mmap") == 0)
		{
			debugLevel |= MOD_MMAP;
			fprintf(stderr,"Debug level : mmap\n");
		}
		if (strcmp(tok,"access") == 0)
		{
			debugLevel |= MOD_ACCESS;
			fprintf(stderr,"Debug level : access\n");
		}
		if (strcmp(tok,"proctitle") == 0)
		{
			titleFlag=1;
			fprintf(stderr,"Debug level : proctitle\n");
		}
		if (strcmp(tok,"candidate") == 0)
		{
			debugLevel |= MOD_CANDIDATE;
			fprintf(stderr,"Debug level : candidate\n");
		}
		if (strcmp(tok,"lock") == 0)
		{
			debugLevel |= MOD_LOCK;
			fprintf(stderr,"Debug level : lock\n");
		}
		if (strcmp(tok,"broker") == 0)
		{
			debugLevel |= MOD_BROKER;
			fprintf(stderr,"Debug level : broker\n");
		}
		tok = (char *)strtok(NULL,":");
	}
	(void)free(tmp);
	fprintf(stderr,
		"\n-------------------------------------------------------\n\n");
}


void _msqlDebug(module)
	int	module;
{
	if (! (module & debugLevel) && module != MOD_ANY)
	{
		return;
	}
	printf("[%s %5d] %s",PROGNAME, (int)getpid(), msqlDebugBuf);
	fflush(stdout);
}


int debugSet(module)
	int	module;
{
	if (! (module & debugLevel))
                return(0);
 	return(1);
}



#ifdef HAVE_STDARG_H
void _debugTrace(int dir, ...)
#else
void _debugTrace(va_alist)
	va_dcl
#endif
{
	va_list args;
	char	*fmt,
		*tag;
	int	loop;
	static	int indent = 0;
	static 	char inTag[] = "-->",
		     outTag[] = "<--";

#ifdef HAVE_STDARG_H
	va_start(args, dir);
#else
	int	dir;

	va_start(args);
	dir = (int)va_arg(args, int);
#endif

	if (! (debugLevel & MOD_TRACE))
	{
		va_end(args);
		return;
	}

	if (dir == TRACE_IN)
	{
		tag = inTag;
		indent++;
	}
	else
		tag = outTag;
	fmt = (char *)va_arg(args, char *);
	if (!fmt)
        	return;
	(void)vsprintf(msqlDebugBuf,fmt,args);
	va_end(args);
	printf("[%s %d] ",PROGNAME, (int)getpid());
	for (loop = 1; loop <indent; loop++)
		printf("  ");
	printf("%s %s\n",tag,msqlDebugBuf);
	fflush(stdout);
	if (dir == TRACE_OUT)
		indent--;
}

