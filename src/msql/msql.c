/*
** Copyright (c) 1995-2001  Hughes Technologies Pty Ltd.  All rights
** reserved.  
**
** Terms under which this software may be used or copied are
** provided in the  specific license associated with this product.
**
** Hughes Technologies disclaims all warranties with regard to this 
** software, including all implied warranties of merchantability and 
** fitness, in no event shall Hughes Technologies be liable for any 
** special, indirect or consequential damages or any damages whatsoever 
** resulting from loss of use, data or profits, whether in an action of 
** contract, negligence or other tortious action, arising out of or in 
** connection with the use or performance of this software.
**
**
** $Id: msql.c,v 1.14 2012/01/15 06:19:59 bambi Exp $
**
*/

/*
** Module	: msql
** Purpose	: 
** Exports	: 
** Depends Upon	: 
*/



/**************************************************************************
** STANDARD INCLUDES
**************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <common/config.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#  include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif


/**************************************************************************
** MODULE SPECIFIC INCLUDES
**************************************************************************/

#include <fcntl.h>
#include <common/msql_defs.h>
#include <common/config/config.h>
#include <libmsql/msql.h>


/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/

char *msql_tmpnam();
int quietFlag;

/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/


/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/



void usage()
{
	(void)fprintf(stderr,"\n\nUsage : msql [-f conf_file] [-q] [-h host] database\n\n");
}



void help()
{
	(void)printf("\n\nMiniSQL Help!\n\n");
	(void)printf("The following commands are available :- \n\n");
	(void)printf("\t\\q	Quit\n");
	(void)printf("\t\\g	Go (Send query to database)\n");
	(void)printf("\t\\e	Edit (Edit previous query)\n");
	(void)printf("\t\\p	Print (Print the query buffer)\n");
	(void)printf("\t;	Aias for 'Go' for compatability\n");
}


#ifdef max
#  undef max
#endif

int max(v1,v2)
	int	v1,
		v2;
{
	if (v1 > v2)
		return(v1);
	else
		return(v2);
}


void bufFill(buf,length,max,filler)
	char	*buf;
	int	length,
		max;
	char	filler;
{
	int	count;
	char	tmpBuf[2];

	tmpBuf[0] = filler;
	tmpBuf[1] = 0;
	count = max - length;
	while (count-- >= 0)
	{
		strcat(buf,tmpBuf);
	}
}



void fill(length,max,filler)
	int	length,
		max;
	char	filler;
{
	int	count;

	count = max - length;
	while (count-- >= 0)
	{
		printf("%c",filler);
	}
}


int getFieldWidth(type)
	int	type;
{
	switch(type)
	{
		case INT8_TYPE:
		case UINT8_TYPE:
			return(4);

		case INT16_TYPE:
		case UINT16_TYPE:
			return(6);

		case INT32_TYPE:
		case UINT32_TYPE:
		case MONEY_TYPE:
		case TIME_TYPE:
			return(8);

		case INT64_TYPE:
		case UINT64_TYPE:
			return(16);

		case DATE_TYPE:
			return(11);

		case DATETIME_TYPE:
			return(20);

		case MILLITIME_TYPE:
			return(13);

		case MILLIDATETIME_TYPE:
			return(24);

		case REAL_TYPE:
			return(12);

		case CIDR4_TYPE:
			return(21);

		case IPV4_TYPE:
			return(19);

		case IPV6_TYPE:
			return(40);

		case CIDR6_TYPE:
			return(43);
	}
	return(-1);
}


void handleQuery(sock, q)
	int	sock;
	char	*q;
{
	char	sepBuf[MSQL_PKT_LEN],
		*error;
	int	off,
		maxLen,
		length,
		fieldWidth,
		res;
	m_result *result;
	m_row	cur;
	m_field	*curField;

	if (!q)
	{
		printf("No query specified !!\n");
		return;
	}
	if (!*q)
	{
		printf("No query specified !!\n");
		return;
	}

	res = msqlQuery(sock,q);
	if (res < 0)
	{
		error = msqlGetErrMsg(NULL);
		printf("\n\nERROR : ");
		fflush(stdout);
		write(fileno(stdout),error,strlen(error));
		printf("\n");
		if (quietFlag == 0)
		{
			printf("Query : %s\n",q);
		}
		printf("\n");
		return;
	}
	if (quietFlag == 0)
	{
		printf("\nQuery OK.  %d row(s) modified or retrieved.\n\n",res);
	}

	result = msqlStoreResult();
	if (!result)
	{
		if (quietFlag == 0)
		{
			printf("\n\n");	
		}
		return;
	}

	/*
	** Print a pretty header .... 
	*/
	(void)bzero(sepBuf,sizeof(sepBuf));
	strcat(sepBuf," +");
	maxLen = 1;
	length = 0;
	while((curField = msqlFetchField(result)))
	{
		fieldWidth = getFieldWidth(curField->type);
		if (fieldWidth < 0)
		{
			fieldWidth = curField->length;
		}
		length = strlen(curField->name);
		if (length < fieldWidth)
		{
			length = fieldWidth;
		}
		maxLen += length;
		if (maxLen >= MSQL_PKT_LEN)
		{
			printf("\n\nRow length is too long to be displayed\n");
			msqlFreeResult(result);
			return;
		}
		bufFill(sepBuf,0,length,'-');
		strcat(sepBuf,"-+");
	}
	strcat(sepBuf,"\n");
	printf("%s", sepBuf);
	msqlFieldSeek(result,0);

	printf(" |");
	while((curField = msqlFetchField(result)))
	{
		fieldWidth = getFieldWidth(curField->type);
		if (fieldWidth < 0)
		{
			fieldWidth = curField->length;
		}
		length = strlen(curField->name);
		if (length < fieldWidth)
		{
			length = fieldWidth;
		}
		printf(" %s",curField->name);
		fill(strlen(curField->name),length,' ');
		printf("|");
	}
	printf("\n");
	msqlFieldSeek(result,0);
	printf("%s", sepBuf);



	/*
	** Print the returned data
	*/
	while ((cur = msqlFetchRow(result)))
	{
		off = 0;
		printf(" |");
		while(off < msqlNumFields(result))
		{
			curField = msqlFetchField(result);
			fieldWidth = getFieldWidth(curField->type);
			if (fieldWidth < 0)
			{
				fieldWidth = curField->length;
			}
			length = strlen(curField->name);
			if (length < fieldWidth)
			{
				length = fieldWidth;
			}
			if (cur[off])
			{
				printf(" %s",cur[off]);
				fill(strlen(cur[off]),length,' ');
			}
			else
			{
				printf(" NULL");
				fill(4,length,' ');
			}
			printf("|");
			off++;
		}
		printf("\n");
		msqlFieldSeek(result,0);
	}
	printf("%s", sepBuf);
	msqlFreeResult(result);
	printf("\n\n");
}



void editQuery(q)
	char	*q;
{
	char	*filename,
		*editor,
		combuf[1024];
	int	fd;

	filename = (char *)msql_tmpnam(NULL);

#if defined(_OS_OS2)
        if( NULL != strrchr(filename, '\\') )
                filename = strrchr( filename, '\\' );
        if( NULL != strrchr(filename, '/') )
                filename = strrchr( filename, '/' );
#endif

	fd = open(filename,O_CREAT | O_WRONLY, 0777);
	editor = (char *)getenv("VISUAL");
	if (!editor)
	{
		editor = (char *)getenv("EDITOR");
	}
	if (!editor)
	{
#if defined(_OS_OS2)
		editor = "e";
#endif
#if defined(_OS_WIN32)
		editor = "edit";
#endif
#if defined(_OS_UNIX)
		editor = "vi";
#endif
	}
	write(fd,q,strlen(q));
	close(fd);
	snprintf(combuf,sizeof(combuf),"%s %s",editor,filename);
	system(combuf);
	fd = open(filename,O_RDONLY, 0777);
	bzero(q,MSQL_PKT_LEN);
	read(fd,q,MSQL_PKT_LEN);
	close(fd);
	unlink(filename);

#if defined(_OS_OS2)
        /*
        ** filter all EOFs (EOF --> SPACE)
        */
        editor = q;
        fd     = MSQL_PKT_LEN;
        while( 0 < fd )
        {
                if( 0x1A == *editor )
                        *editor = ' ';
                editor++;
                fd--;
        }
#endif
}
	


int main(argc,argv)
	int	argc;
	char	*argv[];
{
	char	qbuf[MSQL_PKT_LEN],
		*cp,
		*host = NULL,
		*confFile = NULL;
	int	newQ = 1,
		prompt = 1,
		sock,
		inString = 0,
		c,
		argsLeft,
		errFlag = 0,
		qLen = 0;
	register u_int inchar;

#if defined(_OS_WIN32)
	WORD	wVersion;
	WSADATA	wsaData;
#endif

	extern	int optind;
	extern	char *optarg;

        while((c=getopt(argc,argv,"f:h:q"))!= -1)
        {
                switch(c)
                {
                        case 'h':
                                if (host)
                                        errFlag++;
                                else
                                        host = optarg;
                                break;

			case 'f':
				if (confFile)
					errFlag++;
				else
					confFile = optarg;
				break;

			case 'q':
				quietFlag = 1;
				break;

                        case '?':
                                errFlag++;
                                break;
                }
        }
 
        argsLeft = argc - optind;


	if (quietFlag == 0)
	{
		printf("\n");
	}
	if (argsLeft != 1  ||  errFlag)
	{
		usage();
		exit(1);
	}

#if defined(_OS_WIN32)
	wVersion = MAKEWORD(1,1);
	if (WSAStartup(wVersion, &wsaData) != 0)
	{
		printf("Can't initialise WinSOCK!\n\n");
		exit(1);
	}
#endif


	/*
	** If we have a config file override the default config
	*/
	if (confFile)
	{
		configLoadFile(confFile);
	}

	if ((sock = msqlConnect(host)) < 0)
	{
		printf("ERROR : %s\n",msqlGetErrMsg(NULL));
		exit(1);
	}

	if (msqlSelectDB(sock, argv[optind]) < 0)
	{
		printf("ERROR : %s\n",msqlGetErrMsg(NULL));
		exit(1);
	}

	/*
	**  Run in interactive mode like the ingres/postgres monitor
	*/

	if (quietFlag == 0)
	{
		printf("Welcome to the miniSQL monitor.  Type \\h for help.\n\n");
	}
		
	inchar = EOF+1;
	(void)bzero(qbuf,sizeof(qbuf));
	cp = qbuf;
	if (quietFlag == 0)
	{
		printf("\nmSQL > ");
	}
	fflush(stdout);
	while(!feof(stdin))
	{
		inchar = fgetc(stdin);
		qLen ++;
		if (qLen == MSQL_PKT_LEN)
		{
			if (quietFlag == 0)
			{
				printf("\nQuery = %s\n", qbuf);
			}
			printf("\n\n\nError : Query text too long ( > %d bytes!)\n\n", MSQL_PKT_LEN);
			printf("Check your query to ensure that there isn't an unclosed text field.\n\n");
			exit(1);
		}
		if (inchar == '\\')
		{
			if (inString)
			{
				*cp++ = inchar;
				inchar = fgetc(stdin);
				*cp++ = inchar;
				continue;
			}

			inchar = fgetc(stdin);
			if (inchar == EOF)
				continue;
			switch(inchar)
			{
				case 'h':
					help();
					newQ = 1;
					if (quietFlag == 0)
					{
						printf("\nmSQL > ");
					}
					prompt=0;
					break;
				case 'g':
					handleQuery(sock,qbuf);
					newQ = 1;
					qLen = 0;
					inString = 0;
					if (quietFlag == 0)
					{
						printf("\nmSQL > ");
					}
					prompt=0;
					break;
				case 'e':
					editQuery(qbuf);
					printf("Query buffer\n");
					printf("------------\n");
					printf("%s\n[continue]\n",qbuf);
					printf("    -> ");
					prompt=0;
					cp = qbuf + strlen(qbuf);
					qLen = strlen(qbuf);
					break;
				case 'q':
					msqlClose(sock);
					if (quietFlag == 0)
					{
						printf("\n\nBye!\n\n");	
					}
					exit(0);

				case 'p':
					printf("\nQuery buffer\n");
					printf("------------\n");
					printf("%s\n[continue]\n",qbuf);
					printf("    -> ");
					prompt=0;
					break;
				default:
					printf("\n\nUnknown command.\n\n");
					newQ = 1;
					if (quietFlag == 0)
					{
						printf("\nmSQL > ");
					}
					prompt=0;
					break;
			}
		}
		else
		{
			if (inchar == ';' && inString == 0)
			{
				/* alias for 'Go' command */
				handleQuery(sock,qbuf);
				newQ = 1;
				qLen = 0;
				inString = 0;
				if (quietFlag == 0)
				{
					printf("\nmSQL > ");
				}
				prompt=0;
				continue;
			}
			if (inchar == '\'')
			{
				if (inString)
					inString = 0;
				else
					inString = 1;
			}
			if (inString)
			{
				*cp++ = inchar;
				continue;
			}
			if ((newQ )&& (inchar != '\n'))
			{
				newQ = 0;
				cp = qbuf;
				(void)bzero(qbuf,sizeof(qbuf));
			}
			if (inchar == '#')
			{
				while(!feof(stdin))
				{
					inchar = fgetc(stdin);
					if (inchar == '\n')
					{
						break;
					}
				}
				continue;
			}
			if (inchar == '\n')
			{
				if (prompt)
				{
					if (quietFlag == 0)
					{
						printf("    -> ");
					}
				}
				else
				{
					prompt++;
					continue;
				}
			}
			*cp++ = inchar;
		}
	}
	msqlClose(sock);
	if (quietFlag == 0)
	{
		printf("\nBye!\n\n");
	}
	exit(0);
}
