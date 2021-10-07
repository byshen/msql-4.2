/*
**      msqldump.c  - Dump a tables contents and format to an ASCII file
**
**
** Original version written by Igor Romanenko (igor@frog.kiev.ua) under
** the name of msqlsave.c and placed in the public domain.  This program
** remains in the public domain to continue the spirit of its original
** author although the modifications made to the original program are :-
**
** 		     Copyright (c) 1994  David J. Hughes
** 		     Copyright (c) 1995-2001  Hughes Technologies Pty Ltd
**
**
** This software is provided "as is" without any expressed or implied warranty.
**
** The author's original notes follow :-
**
**		******************************************************
**		*                                                    *
**		* MSQLSAVE.C -- saves the contents of an mSQL table  *
**		*               in .msql format.                     *
**		*                                                    *
**		* AUTHOR: Igor Romanenko (igor@frog.kiev.ua)         *
**		* DATE:   December 3, 1994                           *
**		* WARRANTY: None, expressed, impressed, implied      *
**		*           or other                                 *
**		* STATUS: Public domain                              *
**		*                                                    *
**		******************************************************
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <common/portability.h>
#include <common/msql_defs.h>
#include <libmsql/msql.h>

#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif

#define	BUF_LEN	(5 * 1024)

/* Exit codes */

#define EX_USAGE 1
#define EX_MSQLERR 2
#define EX_CONSCHECK 3

char	usage[] = "\n\n\
usage: 	msqldump [-h host] [-f conf] [-vtdcq] [-w where_clause] database [table]\n\n\
	Produce an ASCII dump of a database table or an entire database\n\n\
	-v	verbose\n\
	-d	dump just the data\n\
	-t	dump just the table schema\n\
	-c	use long insert format\n\
	-q	quiet mode.  Suppress some output.\n\n";

int	verbose = 0,
	cFlag = 0,
	dFlag = 0,
	qFlag = 0,
	tFlag = 0;
int	sock = -1;
char	insert_pat[BUF_LEN],
	*whereClause = NULL,
	colDef[ NAME_LEN * (MAX_FIELDS + 2) + 1];



/*
** DBerror -- prints mSQL error message and exits the program.
*/

void DBerror()
{
	fprintf(stderr, "mSQL error: %s\n", msqlErrMsg);
	if ( sock != -1 )
		msqlClose(sock);
	exit(EX_MSQLERR);
}


char *escapeText(str)
	char	*str;
{
	register char	*cp,
			*cp2,
			*tmp;
	int	numQuotes;

	cp = str;
	numQuotes = 0;
	while((cp = (char *)index(cp,'\'')))
	{
		numQuotes++;
		cp++;
	}
	cp = str;
	while((cp = (char *)index(cp,'\\')))
	{
		numQuotes++;
		cp++;
	}

	if (numQuotes)
	{
		tmp = (char *)malloc(strlen(str)+numQuotes+1);
		cp2 = tmp;
		cp = str;
		while(*cp)
		{
			if (*cp == '\'' || *cp == '\\')
				*cp2++='\\';
			*cp2++ = *cp++;
		}
		*cp2 = '\0';
		return(tmp);
	}
	else
	{
		return((char *)strdup(str));
	}
}


/*
** dbConnect -- connects to the host and selects DB.
**            Also checks whether the tablename is a valid table name.
*/

void dbConnect(host,database)
	char	*host,
		*database;
{
	if (verbose)
	{
		fprintf(stderr, "Connecting to %s...\n", 
			host ? host : "localhost");
	}
	sock = msqlConnect(host);
	if ( sock == -1 )
		DBerror();
	if ( verbose )
		fprintf(stderr, "Selecting data base %s...\n", database);
	if ( msqlSelectDB(sock, database) == -1 )
		DBerror();
}



/*
** dbDisconnect -- disconnects from the host.
*/

void dbDisconnect(host)
	char	*host;
{
	if (verbose)
	{
		fprintf(stderr, "Disconnecting from %s...\n", 
			host ? host : "localhost");
	}
	msqlClose(sock);
}



/*
** getStructure -- retrievs database structure, prints out corresponding
**                 CREATE statement and fills out insert_pat.
*/

int getTableStructure(table)
	char	*table;
{
	m_field 	*mf;
	m_result	*tableRes,
			*indexRes;
	m_row		row;
	m_seq		*seq;
	int		init = 1,
			numFields = 0;

	if (verbose)
	{
		fprintf(stderr, "Retrieving table structure for table %s...\n",
			table);
	}
	if (!(tableRes = msqlListFields(sock, table))) 
	{
		fprintf(stderr, "mSQL error: No such table - %s\n", table);
		exit(EX_MSQLERR);
	}

	snprintf(insert_pat, sizeof(insert_pat),
		"INSERT INTO %s %%s VALUES (", table);
	if (!dFlag)
	{
		printf("\n#\n# Table structure for table '%s'\n#\n",table);
		printf("CREATE TABLE %s (\n", table);
	}

	strcpy(colDef,"(");
	while((mf=msqlFetchField(tableRes)))
	{
		if (mf->type > LAST_REAL_TYPE)
		{
			break;
		}
		numFields++;
		if (init)
		{
			init = 0;
		}
		else
		{
			strcat(colDef,", ");
			if (!dFlag)
			{
				printf(",\n");
			}
		}
		strcat(colDef, mf->name);
		if (dFlag)
		{
			continue;
		}
		printf("  %s ", mf->name);
		switch(mf->type) 
		{
			case INT32_TYPE:
				printf("INT");
				break;
			case INT8_TYPE:
				printf("INT8");
				break;
			case INT16_TYPE:
				printf("INT16");
				break;
			case INT64_TYPE:
				printf("INT64");
				break;
			case UINT32_TYPE:
				printf("UINT");
				break;
			case UINT8_TYPE:
				printf("UINT8");
				break;
			case UINT16_TYPE:
				printf("UINT16");
				break;
			case UINT64_TYPE:
				printf("UINT64");
				break;
			case MONEY_TYPE:
				printf("MONEY");
				break;
			case TIME_TYPE:
				printf("TIME");
				break;
			case DATE_TYPE:
				printf("DATE");
				break;
			case DATETIME_TYPE:
				printf("DATETIME");
				break;
			case MILLITIME_TYPE:
				printf("MILLITIME");
				break;
			case MILLIDATETIME_TYPE:
				printf("MILLIDATETIME");
				break;
			case CHAR_TYPE:
				printf("CHAR(%d)", mf->length);
				break;
			case REAL_TYPE:
				printf("REAL");
				break;
			case TEXT_TYPE:
				printf("TEXT(%d)", mf->length);
				break;
			case IPV4_TYPE:
				printf("IPv4");
				break;
			case CIDR4_TYPE:
				printf("CIDR4");
				break;
			case IPV6_TYPE:
				printf("IPv6");
				break;
			case CIDR6_TYPE:
				printf("CIDR6");
				break;
			default:
				fprintf(stderr,
					"Unknown field type: %d\n", 
					mf->type);
				exit(EX_CONSCHECK);
		}
		if(IS_NOT_NULL(mf->flags) )
			printf(" NOT NULL");
	}
	strcat(colDef,")");
	if (!dFlag)
	{
		printf("\n) \\g\n\n");
	}

        seq = msqlGetSequenceInfo(sock,table);
        if (seq)
        {
		if (dFlag)
		{
			printf ("DROP SEQUENCE from %s\\g\n", table);
		}
                printf("CREATE SEQUENCE ON %s STEP %d VALUE %d \\g\n\n",
                        table, seq->step, seq->value);
        }

	if (dFlag)
	{
		msqlFreeResult(tableRes);
		return(numFields);
	}

	while(mf)
	{
		char	*type;

		if (mf->type != IDX_TYPE)
		{
			mf = msqlFetchField(tableRes);
			continue;
		}
		indexRes = msqlListIndex(sock,table,mf->name);
		row = msqlFetchRow(indexRes);
		if (strcmp(row[0],"avl") != 0)
			type = row[0];
		else
			type = NULL;

		/*
		** Dodge the stats info
		*/
		row = msqlFetchRow(indexRes);
		row = msqlFetchRow(indexRes);

		/*
		** output the stuff
		*/
		printf("CREATE %s %s INDEX %s ON %s (",
			IS_UNIQUE(mf->flags)?"UNIQUE":"",
			type?type:"", mf->name, table);
		row = msqlFetchRow(indexRes);
		init = 1;
		while(row)
		{
			if (init)
			{
				init=0;
				printf("\n");
			}
			else
			{
				printf(",\n");
			}
			printf("\t%s",row[0]);
			row = msqlFetchRow(indexRes);
		}
		printf("\n) \\g\n\n");
		msqlFreeResult(indexRes);
		mf = msqlFetchField(tableRes);
	}
	msqlFreeResult(tableRes);

	return(numFields);
}




/*
** dumpTable saves database contents as a series of INSERT statements.
*/

void dumpTable(numFields,table)
	int	numFields;
	char	*table;
{
	char		query[BUF_LEN],
			*tmp;
	m_result 	*res;
	m_field 	*field;
	m_row 		row;
	int		i;
	int		init = 1;

	if (tFlag)
		return;
	if (verbose)
		fprintf(stderr, "Sending SELECT query...\n");
	if (!qFlag)
	printf("\n#\n# Dumping data for table '%s'\n#\n\n",table);
	if (whereClause)
	{
		snprintf(query, sizeof(query), "SELECT * FROM %s where %s", 
			table, whereClause);
	}
	else
	{
		snprintf(query, sizeof(query), "SELECT * FROM %s ", table);
	}
	if (msqlQuery(sock, query) == -1)
		DBerror();
	if (!(res=msqlStoreResult()))
		DBerror();
	if (verbose)
	{
		fprintf(stderr, "Retrieved %d rows. Processing...\n", 
			msqlNumRows(res) );
	}
	if (msqlNumFields(res) != numFields)
	{
		fprintf(stderr,"Error in field count!  Aborting.\n\n");
		exit(EX_CONSCHECK);
	}

	while ((row=msqlFetchRow(res)))
	{
		printf(insert_pat, cFlag?colDef:"");
		init = 1;
		msqlFieldSeek(res,0);
		for (i = 0; i < msqlNumFields(res); i++) 
		{
			if (!(field = msqlFetchField(res))) 
			{
				fprintf(stderr,"Not enough fields! Aborting\n");
				exit(EX_CONSCHECK);
			}
			if (!init )
				printf(",");
			else
				init=0;
			if (row[i])
			{
				if (field->type == CHAR_TYPE || 
				    field->type == TEXT_TYPE ||
				    field->type == DATE_TYPE ||
				    field->type == TIME_TYPE ||
				    field->type == DATETIME_TYPE ||
				    field->type == MILLITIME_TYPE ||
				    field->type == MILLIDATETIME_TYPE ||
				    field->type == IPV4_TYPE ||
				    field->type == CIDR4_TYPE ||
				    field->type == IPV6_TYPE ||
				    field->type == CIDR6_TYPE)
				{
					tmp = escapeText(row[i]);
					printf("\'%s\'", tmp);
					free(tmp);
				}
				else
				{
					printf("%s", row[i]);
				}
			}
			else
			{
				printf("NULL");
			}
		}
		printf(")\\g\n");
	}
	msqlFreeResult(res);
}



char *getTableName()
{
	static m_result *res = NULL;
	m_row		row;

	if (!res)
	{
		res = msqlListTables(sock);
		if (!res)
			return(NULL);
	}
	row = msqlFetchRow(res);
	if (row)
	{
		return((char *)row[0]);
	}
	else
	{
		msqlFreeResult(res);
		return(NULL);
	}
}




int main(argc, argv)
	int	argc;
	char	*argv[];
{
	int	c,
		numRows,
		errFlag = 0;
	char	*host = NULL,
		*database = NULL,
		*table = NULL,
		*confFile = NULL;
	extern	char *optarg;
	extern	int optind;

	/*
	** Check out the args
	*/
	while((c=getopt(argc,argv,"h:f:w:vcdtq"))!= -1)
	{
		switch(c)
		{
			case 'h':
				if (host)
					errFlag++;
				else
					host = optarg;
				break;

			case 'w':
				if (whereClause)
					errFlag++;
				else
					whereClause = optarg;
				break;

			case 'v':
				if (verbose)
					errFlag++;
				else
					verbose++;
				break;

                        case 'f':
                                if (confFile)
                                        errFlag++;
                                else
                                        confFile = optarg;
                                break;

                        case 'c':
                                if (cFlag)
                                        errFlag++;
                                else
                                        cFlag++;
                                break;

                        case 'd':
                                if (dFlag)
                                        errFlag++;
                                else
                                        dFlag++;
                                break;

                        case 'q':
                                if (qFlag)
                                        errFlag++;
                                else
                                        qFlag++;
                                break;

                        case 't':
                                if (tFlag)
                                        errFlag++;
                                else
                                        tFlag++;
                                break;

			case '?':
				errFlag++;
		}
	}
	if (errFlag)
	{
		fprintf(stderr, "%s", usage);
		exit(EX_USAGE);
	}

	if (optind < argc)
		database = argv[optind++];
	if (optind < argc)
		table = argv[optind++];

	if (!database)
	{
		fprintf(stderr, "%s", usage);
		exit(EX_USAGE);
	}
	
        /*
        ** If we have a config file override the default config
        */
        if (confFile)
        {
                msqlLoadConfigFile(confFile);
        }

	if (!qFlag)
	{
		printf("#\n# mSQL Dump  (requires mSQL 3.0 or newer)\n#\n");
		printf("# Host: %s    Database: %s\n",
	    		host ? host : "localhost", database);
		printf("#--------------------------------------------------"
			"------\n\n");
	}
	dbConnect(host,database);
	if (table)
	{
		numRows = getTableStructure(table);
		dumpTable(numRows,table);
	}
	else
	{
		while((table = getTableName()))
		{
			numRows = getTableStructure(table);
			dumpTable(numRows,table);
		}
	}
	dbDisconnect(host);
	printf("\n");
	exit(0);
}


