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
** $Id: relshow.c,v 1.8 2012/01/15 06:19:59 bambi Exp $
**
*/

/*
** Module	: relshow
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


/**************************************************************************
** MODULE SPECIFIC INCLUDES
**************************************************************************/

#include <common/msql_defs.h>
#include <common/debug/debug.h>
#include <libmsql/msql.h>


/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/


/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/


/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/


char	PROGNAME[] = "Relshow";

void usage()
{
	printf("\nUsage : relshow [-h host] [-f conf] [dbName [relName] [index | _seq]]\n\n");
	printf("         Where   dbName is the name of a database\n");
	printf("                 relname is the name of a relation\n");
	printf("                 index is the name of an index \n");
	printf("                 _seq is the sequence system variable\n");
	printf("\n");
	printf("If no database is given, list the known databases\n");
	printf("If no relation is given, list relations in the database\n");
	printf("If database and relation given, list fields ");
	printf("in the given relation\n");
	printf("If database, relation and index are given, list the ");
	printf("details of the index\n");
	printf("If database, relation and _seq are given, show ");
	printf("details of the table sequence\n");
	printf("\n\007");
}




int main(argc,argv)
	int	argc;
	char	*argv[];
{
	char	dbShow = 0,
		relShow = 0,
		fieldShow = 0,
		indexShow = 0,
		seqShow = 0,
		*idxType;
	char	typeName[20];
	int	sock,
		argsLeft,
		errFlag = 0,
		iFlag = 0,
		c;
	m_row	cur;
	m_result *res;
	m_field	*curField;
	m_tinfo	*info;
        char    *host = NULL,
		*confFile = NULL;
        extern  int optind;
        extern  char *optarg;
#if defined(_OS_WIN32)
	WORD	wVersion;
	WSADATA	wsaData;
#endif


        while((c=getopt(argc,argv,"f:h:i"))!= -1)
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

			case 'i':
				iFlag++;
				break;

                        case '?':
                                errFlag++;
                                break;
                }
        }

	argsLeft = argc - optind;

	if (errFlag)
	{
		usage();
		exit(1);
	}

	/*
	** Work out what we here to do
	*/

	switch(argsLeft)
	{
		case 0:	dbShow++;
			break;
		case 1: relShow++;
			break;
		case 2:	fieldShow++;
			break;
		case 3:	if (strcmp(argv[optind+2],"_seq") == 0)
				seqShow++;
			else
				indexShow++;
			break;
		default:usage();
			exit(1);
	}


	/*
	**  Fire up mSQL
	*/

#if defined(_OS_WIN32)
	wVersion = MAKEWORD(1,1);
	if (WSAStartup(wVersion, &wsaData) != 0)
	{
		printf("Can't initialise WinSOCK!\n\n");
		exit(0);
	}
#endif

	if (confFile)
	{
		msqlLoadConfigFile(confFile);
	}

	if ((sock = msqlConnect(host)) < 0)
	{
		printf("\nError connecting to database : %s\n\n", 
			msqlGetErrMsg(NULL));
		exit(1);
	}

	if (!dbShow)
	{
		if(msqlSelectDB(sock,argv[optind]) < 0)
		{
			printf("\n%s\n\n",msqlGetErrMsg(NULL));
			msqlClose(sock);
			exit(1);
		}
	}


	/*
	** List the available databases if required
	*/

	if (dbShow)
	{
		res = msqlListDBs(sock);
		if (!res)
		{
			printf("\nERROR : Couldn't get database list!\n");
			exit(1);
		}
		printf("\n\n  +---------------------------+\n");
		printf("  |         Databases         |\n");
		printf("  +---------------------------+\n");
		while((cur = msqlFetchRow(res)))
		{
			printf("  | %-25.25s |\n", cur[0]);
		}
		printf("  +---------------------------+\n\n");
		msqlFreeResult(res);
		msqlClose(sock);
		exit(0);

	}


	/*
	** List the available relations if required
	*/

	if (relShow)
	{

		res = msqlListTables(sock);
		if (!res)
		{
			printf("\n");
			printf("ERROR : Unable to list tables in database %s\n",
				argv[optind]);
			exit(1);
		}
		printf("\n\nDatabase = %s\n\n",argv[optind]);
		printf("  +-------------------------------+\n");
		printf("  |            Table              |\n");
		printf("  +-------------------------------+\n");
		while((cur = msqlFetchRow(res)))
		{
			printf("  | %-29.29s |\n", cur[0]);
		}
		printf("  +-------------------------------+\n\n");
		msqlFreeResult(res);
		msqlClose(sock);
		exit(0);
	}


	/*
	** List the attributes and types if required
	*/

	if (fieldShow)
	{
		/*
		** Show the table info if requested
		*/

		info = NULL;	
		if (iFlag)
		{
			info = msqlTableInfo(sock, argv[optind+1]);
			if (!info)
			{
				printf("\nERROR : %s\n\n",msqlGetErrMsg(NULL));
				exit(1);
			}
		}


		/*
		** Get the list of attributes
		*/

		res = msqlListFields(sock,argv[optind+1]);
		if (!res)
		{
			printf("\nERROR : %s\n\n",msqlGetErrMsg(NULL));
			exit(1);
		}

		/*
		** Display the information
		*/

		printf("\nDatabase  = %s\n",argv[optind]);
		printf("Table     = %s\n",argv[optind + 1]);
		if (info)
		{
			printf("Row count = %lu\n", 
				(unsigned long)info->activeRows);
			printf("Data size = %lu\n", 
				(unsigned long)info->dataSize);
			printf("File size = %lu\n", 
				(unsigned long)info->fileSize);
		}
		printf("\n");

		printf(" +-----------------+----------------+--------+----------+--------------+\n");
		printf(" |     Field       |      Type      | Length | Not Null | Unique Index |\n");
		printf(" +-----------------+----------------+--------+----------+--------------+\n");
		while((curField = msqlFetchField(res)))
		{
			printf(" | %-15.15s | ",curField->name);
			switch(curField->type)
			{
				case INT32_TYPE:
					strcpy(typeName,"int");
					break;

				case INT8_TYPE:
					strcpy(typeName,"int8");
					break;

				case INT16_TYPE:
					strcpy(typeName,"int16");
					break;

				case UINT32_TYPE:
					strcpy(typeName,"uint");
					break;

				case UINT8_TYPE:
					strcpy(typeName,"uint8");
					break;

				case UINT16_TYPE:
					strcpy(typeName,"uint16");
					break;

				case INT64_TYPE:
					strcpy(typeName,"int64");
					break;

				case UINT64_TYPE:
					strcpy(typeName,"uint64");
					break;

				case DATE_TYPE:
					strcpy(typeName,"date");
					break;

				case TIME_TYPE:
					strcpy(typeName,"time");
					break;

				case DATETIME_TYPE:
					strcpy(typeName,"datetime");
					break;

				case MILLITIME_TYPE:
					strcpy(typeName,"millitime");
					break;

				case MILLIDATETIME_TYPE:
					strcpy(typeName,"millidatetime");
					break;

				case MONEY_TYPE:
					strcpy(typeName,"money");
					break;

				case CHAR_TYPE:
					strcpy(typeName,"char");
					break;

				case TEXT_TYPE:
					strcpy(typeName,"text");
					break;

				case REAL_TYPE:
					strcpy(typeName,"real");
					break;

				case IPV4_TYPE:
					strcpy(typeName,"IPv4");
					break;

				case CIDR4_TYPE:
					strcpy(typeName,"cidr4");
					break;

				case IPV6_TYPE:
					strcpy(typeName,"IPv6");
					break;

				case CIDR6_TYPE:
					strcpy(typeName,"cidr6");
					break;

				case IDX_TYPE:
					strcpy(typeName,"index");
					break;
					
				default:
					sprintf(typeName,"Unknown (%d)",
						curField->type);
					break;
			}
			printf("%-14.14s |",typeName);
			if (curField->type != IDX_TYPE)
			{
				printf(" %-6d |",curField->length);
				printf(" %-8.8s |",IS_NOT_NULL(curField->flags)?
					"Y":"N");
				printf(" N/A          |\n");
			}
			else
			{
				printf(" N/A    | N/A      |");
				printf(" %s |\n",IS_UNIQUE(curField->flags)?
					"Y           ": "N           ");
			}
		}
		printf(" +-----------------+----------------+--------+----------+--------------+\n");
		printf("\n\n");
		msqlFreeResult(res);
		msqlClose(sock);
	}


	if (indexShow)
	{
		/*
		** Get the list of index fields
		*/

		res = msqlListIndex(sock,argv[optind+1],argv[optind+2]);
		if (!res)
		{
			printf("\nERROR : Couldn't find index '%s' in %s!\n\n",
				argv[optind+2], argv[optind+1]);
			exit(1);
		}

		/*
		** Display the information
		*/

		cur = msqlFetchRow(res);
		if (!cur)
		{
			printf("\nERROR : Couldn't find index '%s' in %s!\n\n",
				argv[optind+2], argv[optind+1]);
			exit(1);
		}
		printf("\nDatabase    = %s\n",argv[optind]);
		printf("Table       = %s\n",argv[optind + 1]);
		printf("Index       = %s\n",argv[optind + 2]);
		if (strncmp(cur[0],"avl",3) == 0)
		{
			printf("Index Type  = Memory mapped AVL tree (%s)\n",
				cur[0] + 4);
		}
		else
		{
			printf("Index Type  = %s\n", cur[0]);
		}
		cur = msqlFetchRow(res);
		printf("Num Entries = %s\n",cur[0]);
		cur = msqlFetchRow(res);
		printf("Num Keys    = %s\n\n",cur[0]);

		printf("  +---------------------+\n");
		printf("  |       Field         |\n");
		printf("  +---------------------+\n");
		while((cur = msqlFetchRow(res)))
		{
			printf("  | %-19.19s |\n", cur[0]);
		}
		printf("  +---------------------+\n\n");
		msqlFreeResult(res);
		msqlClose(sock);
		exit(0);
	}

	if (seqShow)
	{
		m_seq	*seq;

		printf("\nDatabase       = %s\n",argv[optind]);
		printf("Table          = %s\n",argv[optind + 1]);
		seq = msqlGetSequenceInfo(sock, argv[optind + 1]);
		if (!seq)
		{
			printf("\nERROR : %s\n\n",msqlGetErrMsg(NULL));
			exit(1);
		}
		printf("Sequence Step  = %d\n",seq->step);
		printf("Sequence Value = %d\n",seq->value);
		msqlClose(sock);
		exit(0);
	}

	exit(0);
}
