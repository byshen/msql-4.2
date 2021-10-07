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
** $Id: msqladmin.c,v 1.8 2011/11/22 11:47:18 bambi Exp $
**
*/

/*
** Module	: 
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

#include <common/msql_defs.h>
#include <common/config/config.h>
#include <libmsql/msql.h>
#include <msqld/main/version.h>


/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/


/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/


/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/



int	qFlag = 0;
char	PROGNAME[] = "msqladmin";


void usage()
{
	printf("\n\nusage : msqladmin [-h host] [-f conf] [-q] <Command>\n\n");
	printf("where command =");
	printf("\t drop DatabaseName\n");
	printf("\t\t create DatabaseName\n");
	printf("\t\t export DatabaseName TableName FilePath\n");
	printf("\t\t copy FromDB ToDB\n");
	printf("\t\t move FromDB ToDB\n");
	printf("\t\t checktable DatabaseName TableName\n");
	printf("\t\t tableinfo DatabaseName TableName\n");
	printf("\t\t shutdown\n");
	printf("\t\t reload\n");
	printf("\t\t version\n");
	printf("\t\t stats\n");
	printf("\n -q\tQuiet mode.  No verification of commands.\n\n");
}


void createDB(sock,db)
	int	sock;
	char	*db;
{
	if(msqlCreateDB(sock,db) < 0)
	{
		fprintf(stderr,"\nmSQL Command failed!\nServer error = %s\n\n",
				msqlGetErrMsg(NULL));
		msqlClose(sock);
		exit(1);
	}
	else
	{
		printf("Database \"%s\" created.\n",db);
	}
}


void copyDB(sock,fromDB, toDB)
	int	sock;
	char	*fromDB,
		*toDB;
{
	if(msqlCopyDB(sock,fromDB, toDB) < 0)
	{
		fprintf(stderr,"\nmSQL Command failed!\nServer error = %s\n\n",
				msqlGetErrMsg(NULL));
		msqlClose(sock);
		exit(1);
	}
	else
	{
		printf("Database \"%s\" copied to \"%s\".\n",fromDB, toDB);
	}
}


void moveDB(sock,fromDB, toDB)
	int	sock;
	char	*fromDB,
		*toDB;
{
	if(msqlMoveDB(sock,fromDB, toDB) < 0)
	{
		fprintf(stderr,"\nmSQL Command failed!\nServer error = %s\n\n",
				msqlGetErrMsg(NULL));
		msqlClose(sock);
		exit(1);
	}
	else
	{
		printf("Database \"%s\" moved to \"%s\".\n",fromDB, toDB);
	}
}


void dropDB(sock,db)
	int	sock;
	char	*db;
{
	char	buf[10];


	if (!qFlag)
	{
		printf("\n\nDropping the database is potentially a very bad ");
		printf("thing to do.\nAny data stored in the database will be");
		printf(" destroyed.\n\nDo you really want to drop the ");
		printf("\"%s\" ",db);
		printf("database?  [Y/N] ");
		fflush(stdout);
		bzero(buf,10);
		fgets(buf,10,stdin);
		if ( (*buf != 'y') && (*buf != 'Y'))
		{
			printf("\n\nOK, aborting database drop!\n\n");
			msqlClose(sock);
			exit(0);
		}
	}
	if(msqlDropDB(sock,db) < 0)
	{
		fprintf(stderr,"\nmSQL Command failed!\nServer error = %s\n\n",
			msqlGetErrMsg(NULL));
		msqlClose(sock);
		exit(1);
	}
	else
	{
		fprintf(stderr,"Database \"%s\" dropped\n",db);
	}
}


void exportTable(sock, db, table, path, sortField)
	int	sock;
	char	*db,
		*table,
		*path,
		*sortField;
{
	int	result;

	if (msqlSelectDB(sock, db) < 0)
	{
		fprintf(stderr,
			"\nAccess to database failed!\nServer error = %s\n\n",
			msqlGetErrMsg(NULL));
		msqlClose(sock);
		exit(1);
	}
	result = msqlExportTable(sock,table,path,sortField);
	if(result < 0)
	{
		fprintf(stderr,"\nmSQL Command failed!\nServer error = %s\n\n",
			msqlGetErrMsg(NULL));
		msqlClose(sock);
		exit(1);
	}
	printf("\nTable exported to %s.\n\n", path);
}


	
void importTable(sock, db, table, path)
	int	sock;
	char	*db,
		*table,
		*path;
{
	int	result;

	if (msqlSelectDB(sock, db) < 0)
	{
		fprintf(stderr,
			"\nAccess to database failed!\nServer error = %s\n\n",
			msqlGetErrMsg(NULL));
		msqlClose(sock);
		exit(1);
	}
	result = msqlImportTable(sock,table,path);
	if(result < 0)
	{
		fprintf(stderr,"\nmSQL Command failed!\nServer error = %s\n\n",
			msqlGetErrMsg(NULL));
		msqlClose(sock);
		exit(1);
	}
	printf("\nTable imported from %s.\n\n", path);
}
	
void checkTable(sock, db, table)
	int	sock;
	char	*db,
		*table;
{
	int	result;

	result = msqlCheckTable(sock,db,table);
	if(result < 0)
	{
		fprintf(stderr,"\nmSQL Command failed!\nServer error = %s\n\n",
			msqlGetErrMsg(NULL));
		msqlClose(sock);
		exit(1);
	}
	switch(result)
	{
		case MSQL_TABLE_OK:
			fprintf(stderr,"\nTable OK.\n\n");
			break;

		case MSQL_TABLE_BAD_INDEX:
			fprintf(stderr, "\nCheck failed for %s.%s.  ",db,table);
			fprintf(stderr, "Invalid index\n\n");
			break;

		default:
			fprintf(stderr, "\nCheck failed for %s.%s.  ",db,table);
			fprintf(stderr, "Unknown error\n\n");
			break;
	}
}
	

void tableInfo(sock, db, table)
	int	sock;
	char	*db,
		*table;
{
	m_tinfo	*info;

	if(msqlSelectDB(sock,db) < 0)
	{
		printf("\nCan't select database - %s\n\n",msqlGetErrMsg(NULL));
		msqlClose(sock);
		exit(1);
	}
	info = msqlTableInfo(sock,table);
	if(!info)
	{
		fprintf(stderr,"\nmSQL Command failed!\nServer error = %s\n\n",
			msqlGetErrMsg(NULL));
		msqlClose(sock);
		exit(1);
	}
	printf("Table data size : %llu\n", 
		(long long unsigned int)info->dataSize);
	printf("Table file size : %llu\n", 
		(long long unsigned int)info->fileSize);
	printf("Table row length : %llu\n", 
		(long long unsigned int)info->rowLen);
	printf("Table active rows : %llu\n", 
		(long long unsigned int)info->activeRows);
	printf("Table total rows : %llu\n", 
		(long long unsigned int)info->totalRows);
	printf("\n");
}
	

int main(argc,argv)
	int	argc;
	char	*argv[];
{
	int	sock,
		c,
		argsLeft,
		errFlag = 0;
	char	*host = NULL,
		*confFile = NULL;
	extern	int optind;
	extern	char *optarg;

#if defined(_OS_WIN32)
	WORD	wVersion;
	WSADATA	wsaData;

#endif


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
				if (qFlag)
					errFlag++;
				else
					qFlag++;
				break;
			case '?':
				errFlag++;
				break;
		}
	}

	argsLeft = argc - optind;

	if (errFlag || argsLeft == 0)
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


	configLoadFile(confFile);

        if ((sock = msqlConnect(host)) < 0)
        {
                fprintf(stderr,"ERROR : %s\n",msqlGetErrMsg(NULL));
                exit(1);
        }

	if (strcmp(argv[optind],"create") == 0)
	{
		if (argsLeft != 2)
		{
			usage();
			msqlClose(sock);
			exit(1);
		}
		createDB(sock,argv[optind+1]);
		msqlClose(sock);
		exit(0);
	}
	if (strcmp(argv[optind],"drop") == 0)
	{
		if (argsLeft != 2)
		{
			usage();
			msqlClose(sock);
			exit(1);
		}
		dropDB(sock,argv[optind+1]);
		msqlClose(sock);
		exit(0);
	}
	if (strcmp(argv[optind],"copy") == 0)
	{
		if (argsLeft != 3)
		{
			usage();
			msqlClose(sock);
			exit(1);
		}
		copyDB(sock,argv[optind+1],argv[optind+2]);
		msqlClose(sock);
		exit(0);
	}
	if (strcmp(argv[optind],"move") == 0)
	{
		if (argsLeft != 3)
		{
			usage();
			msqlClose(sock);
			exit(1);
		}
		moveDB(sock,argv[optind+1],argv[optind+2]);
		msqlClose(sock);
		exit(0);
	}
	if (strcmp(argv[optind],"export") == 0)
	{
		if (argsLeft == 4)
		{
			exportTable(sock,argv[optind+1],argv[optind+2],
				argv[optind+3], NULL);
			msqlClose(sock);
			exit(0);
		}
		if (argsLeft == 5)
		{
			exportTable(sock,argv[optind+1],argv[optind+2],
				argv[optind+3], argv[optind+4]);
			msqlClose(sock);
			exit(0);
		}
		usage();
		msqlClose(sock);
		exit(1);
	}
	if (strcmp(argv[optind],"import") == 0)
	{
		if (argsLeft != 4)
		{
			usage();
			msqlClose(sock);
			exit(1);
		}
		importTable(sock,argv[optind+1],argv[optind+2],argv[optind+3]);
		msqlClose(sock);
		exit(0);
	}
	if (strcmp(argv[optind],"shutdown") == 0)
	{
		if (argsLeft != 1)
		{
			usage();
			msqlClose(sock);
			exit(1);
		}
		if(msqlShutdown(sock) < 0)
		{
			printf("\nmSQL Command failed!\nServer error = %s\n\n",
				msqlGetErrMsg(NULL));
			msqlClose(sock);
			exit(1);
		}
		exit(0);
	}
	if (strcmp(argv[optind],"reload") == 0)
	{
		if (argsLeft != 1)
		{
			usage();
			msqlClose(sock);
			exit(1);
		}
		if(msqlReloadAcls(sock) < 0)
		{
			printf("\nmSQL Command failed!\nServer error = %s\n\n",
				msqlGetErrMsg(NULL));
			msqlClose(sock);
			exit(1);
		}
		msqlClose(sock);
		exit(0);
	}
	if (strcmp(argv[optind],"version") == 0)
	{
		if (argsLeft != 1)
		{
			usage();
			msqlClose(sock);
			exit(1);
		}
		printf("\nVersion Details :-\n\n");
		printf("\tmsqladmin version \t%s\n",SERVER_VERSION);
		printf("\tmSQL server version \t%s\n", msqlGetServerInfo());
		printf("\tmSQL protocol version \t%d\n", msqlGetProtoInfo());
		printf("\tmSQL connection \t%s\n",msqlGetHostInfo());
		printf("\tTarget platform \t%s\n\n",TARGET);

		printf("Configuration Details :-\n\n");
		printf("\tDefault config file\t%s/msql.conf\n",INST_DIR);
		printf("\tTCP socket         \t%d\n", 
			configGetIntEntry("general", "tcp_port"));
		printf("\tUNIX socket        \t%s\n", 
			(char *)configGetCharEntry("general", "unix_port"));
		printf("\tmSQL user         \t%s\n", 
			(char *)configGetCharEntry("general", "msql_user"));
		printf("\tAdmin user         \t%s\n", 
			(char *)configGetCharEntry("general", "admin_user"));
		printf("\tInstall directory  \t%s\n",
			(char *)configGetCharEntry("general", "inst_dir"));
		printf("\tPID file location  \t%s\n",
			(char *)configGetCharEntry("general", "pid_file"));
		printf("\tMemory Sync Timer  \t%d\n",
			configGetIntEntry("system", "msync_timer"));
		printf("\tHostname Lookup    \t%s\n",
			(configGetIntEntry("system", "host_lookup")==0)?
			"False" : "True");
		printf("\tBackend Processes  \t%d\n",
			configGetIntEntry("system", "num_children"));
		printf("\n\n");
		msqlClose(sock);
		exit(0);
	}
	if (strcmp(argv[optind],"stats") == 0)
	{
		if (argsLeft != 1)
		{
			usage();
			msqlClose(sock);
			exit(1);
		}
		printf("\nServer Statistics\n");
		printf("-----------------\n\n");
		if (msqlGetServerStats(sock) == 0)
		{
			printf("\n\n");
			msqlClose(sock);
			exit(0);
		}
		else
		{
			printf("\nError getting server stats : %s\n\n",
				msqlGetErrMsg(NULL));
			exit(1);
		}
	}
	if (strcmp(argv[optind],"checktable") == 0)
	{
		if (argsLeft != 3)
		{
			usage();
			msqlClose(sock);
			exit(1);
		}
		checkTable(sock,argv[optind+1],argv[optind+2]);
		msqlClose(sock);
		exit(0);
	}
	if (strcmp(argv[optind],"tableinfo") == 0)
	{
		if (argsLeft != 3)
		{
			usage();
			msqlClose(sock);
			exit(1);
		}
		tableInfo(sock,argv[optind+1],argv[optind+2]);
		msqlClose(sock);
		exit(0);
	}
	usage();
	msqlClose(sock);
	exit(1);
}
