/*
**	debug.h	- 	definitions for the debugger
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
** ID = "debug.h,v 1.3 1994/08/19 08:02:56 bambi Exp"
*/


#define	MOD_ANY		0
#define	MOD_CACHE	1
#define MOD_QUERY	2
#define	MOD_KEY		4
#define	MOD_ERR		8
#define MOD_GENERAL	16
#define MOD_TRACE	32
#define MOD_MALLOC	64
#define MOD_MMAP	128
#define MOD_ACCESS	256
#define MOD_TEXT	512
#define MOD_CANDIDATE	1024
#define MOD_LOCK	2048
#define MOD_BROKER	4096

#define TRACE_IN	1
#define TRACE_OUT	2

void	_msqlDebug();
#ifdef HAVE_STDARG_H
  void	_debugTrace(int, ...);
#else
  void	_debugTrace();
#endif
void	initDebug();
int	debugSet();

extern	int debugLevel;
extern	char msqlDebugBuf[];
#define	DEBUG_BUF_LEN	10240

#define	msqlDebug0(level,fmt)		if(debugLevel) { 	\
					strncpy(msqlDebugBuf, 	\
					fmt, DEBUG_BUF_LEN);	\
					_msqlDebug(level); }

#define	msqlDebug1(level,fmt,x)		if(debugLevel) { 	\
					snprintf(msqlDebugBuf, 	\
					DEBUG_BUF_LEN, 		\
					fmt, x);		\
					_msqlDebug(level); }

#define	msqlDebug2(level,fmt,x,y)	if(debugLevel) { 	\
					snprintf(msqlDebugBuf, 	\
					DEBUG_BUF_LEN, 		\
					fmt, x, y);		\
					_msqlDebug(level); }

#define	msqlDebug3(level,fmt,x,y,z)	if(debugLevel) { 	\
					snprintf(msqlDebugBuf, 	\
					DEBUG_BUF_LEN, 		\
					fmt, x,y,z);		\
					_msqlDebug(level); }

#define	msqlDebug4(level,fmt,w,x,y,z)	if(debugLevel) { 	\
					snprintf(msqlDebugBuf, 	\
					DEBUG_BUF_LEN, 		\
					fmt, w,x,y,z);		\
					_msqlDebug(level); }

#define	msqlDebug5(level,fmt,v,w,x,y,z)	if(debugLevel) { 	\
					snprintf(msqlDebugBuf, 	\
					DEBUG_BUF_LEN, 		\
					fmt, v,w,x,y,z);	\
					_msqlDebug(level); }

#define	debugTrace	if(debugLevel) _debugTrace
