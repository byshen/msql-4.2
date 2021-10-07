/*
**      types.h  - 
**
**
** Copyright (c) 1996-2000  Hughes Technologies Pty Ltd
**
** Permission to use, copy, and distribute for non-commercial purposes,
** is hereby granted without fee, providing that the above copyright
** notice appear in all copies and that both the copyright notice and this
** permission notice appear in supporting documentation.
**
** This software is provided "as is" without any expressed or implied warranty.
**
*/


#ifndef MSQL_TYPES_H
#define MSQL_TYPES_H

#if defined(__STDC__) || defined(__cplusplus)
#  define __ANSI_PROTO(x)       x
#else
#  define __ANSI_PROTO(x)       ()
#endif

#ifdef __cplusplus
extern "C" {
#endif




/*
** Type handling function prototypes
*/
int typeBaseType __ANSI_PROTO((int));
int typeFieldSize __ANSI_PROTO((int));
int typeCompatibleTypes __ANSI_PROTO((int,int));
int typeValidConditionTarget __ANSI_PROTO((int, mVal_t*));
char *typePrintTypeName __ANSI_PROTO((int));
int64_t typeCastIntValTo64(mVal_t *);
int typeCompatibleTypes(int,int);

void typePrintDate __ANSI_PROTO((char *, int, int));
int typeScanDate __ANSI_PROTO((mVal_t *, char *, int));
int typeScanCharDateValue __ANSI_PROTO((char *, char *, int));

void typePrintTime __ANSI_PROTO((char *, int, u_int));
int typeScanTime __ANSI_PROTO((mVal_t *, char *, int));
int typeScanCharTimeValue __ANSI_PROTO((char *, char *, int));

void typePrintMoney __ANSI_PROTO((char *, int, int));
int typeScanMoney __ANSI_PROTO((mVal_t *, char *, int));

void typePrintIPv4 __ANSI_PROTO((char *, int, u_int));
u_int typeScanIPv4 __ANSI_PROTO((mVal_t *, char *, int));
u_int typeScanCharIPv4Value __ANSI_PROTO((char *, char *, int));

void typePrintCIDR4 __ANSI_PROTO((char *, int, void*));
void *typeScanCIDR4 __ANSI_PROTO((mVal_t *, char *, int));
void *typeScanCharCIDR4Value __ANSI_PROTO((char *, char *, int));

void typePrintIPv6 __ANSI_PROTO((char *, int, void*));
void *typeScanIPv6 __ANSI_PROTO((mVal_t *, char *, int));
void *typeScanCharIPv6Value __ANSI_PROTO((char *, char *, int));

void typePrintCIDR6 __ANSI_PROTO((char *, int, void*));
void *typeScanCIDR6 __ANSI_PROTO((mVal_t *, char *, int));
void *typeScanCharCIDR6Value __ANSI_PROTO((char *, char *, int));

void typePrintDateTime __ANSI_PROTO((char *, int, u_char*));
void *typeScanDateTime __ANSI_PROTO((mVal_t *, char *, int));
void *typeScanCharDateTimeValue __ANSI_PROTO((u_char *, char *, int));

void typePrintMilliTime __ANSI_PROTO((char *, int, u_int));
u_int typeScanMilliTime __ANSI_PROTO((mVal_t*, char *, int));
u_int typeScanCharMilliTimeValue __ANSI_PROTO((u_char *, char *, int));

void typePrintMilliDateTime __ANSI_PROTO((char *, int, u_char*));
void *typeScanMilliDateTime __ANSI_PROTO((mVal_t*, char *, int));
void *typeScanCharMilliDateTimeValue __ANSI_PROTO((u_char *, char *, int));

#ifdef __cplusplus
        }
#endif
#endif /* MSQL_TYPES_H */

