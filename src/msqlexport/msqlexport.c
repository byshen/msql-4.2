/*
**      msqlexport.c  - 
**
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#if defined(_OS_WIN32)
#  include <winsock.h>
#endif


#include <common/portability.h>
#include <libmsql/msql.h>

#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif

#define	BUF_LEN	(5*1024)
/* Exit codes */

#define EX_USAGE 1
#define EX_MSQLERR 2
#define EX_CONSCHECK 3

char	usage[] = "\n\n\
usage: 	msqlexport [-h host] [-v] [-s Char] [-q Char] [-e Char] database table\n\n\
	Produce an ASCII export of the table.\n\n\
	-v		Verbose\n\
	-s Char		Use the character Char as the separation character\n\
			Default is a comma.\n\
	-q Char		Quote each value with the specified character\n\
	-e Char		Use the specifed Char as the escape character\n\
			Default is \\\n\n";

int	verbose = 0;
int	sock = -1;
char	insert_pat[BUF_LEN];



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


char *escapeText(str,sep,esc)
	char	*str,
		sep,
		esc;
{
	register char	*cp,
			*cp2,
			*tmp;
	int	numQuotes;
	static	char	empty[1]="";

	if (!str)
		return(empty);
	cp = str;
	numQuotes = 0;
	while((cp = (char *)index(cp,sep)))
	{
		numQuotes++;
		cp++;
	}
	cp = str;
	while((cp = (char *)index(cp,sep)))
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
			if (*cp == sep || *cp == esc)
				*cp2++=esc;
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
** dumpTable saves database contents as a series of INSERT statements.
*/

void dumpTable(table,sep,quote,esc)
	char	*table,
		sep,
		quote,
		esc;
{
	char		query[48];
	m_result 	*res;
	m_row 		row;
	int		i;

	if (verbose)
		fprintf(stderr, "Sending SELECT query...\n");
	snprintf(query,sizeof(query), "SELECT * FROM %s", table);
	if (msqlQuery(sock, query) == -1)
		DBerror();
	if (!(res=msqlStoreResult()))
		DBerror();
	if (verbose)
	{
		fprintf(stderr, "Retrieved %d rows. Processing...\n", 
			msqlNumRows(res) );
	}

	while ((row=msqlFetchRow(res)))
	{
		for (i = 0; i < msqlNumFields(res); i++) 
		{
			if (quote)
				printf("%c%s%c",quote, 
					escapeText(row[i],quote,esc),
					quote);
			else
				printf("%s", escapeText(row[i],sep,esc));
			if (i < msqlNumFields(res) - 1)
				printf("%c",sep?sep:',');
		}
		printf("\n");
	}
}




int main(argc, argv)
	int	argc;
	char	*argv[];
{
	int	c,
		errFlag = 0;
	char	*host = NULL,
		*database = NULL,
		*table = NULL,
		sep=0,
		quote=0,
		esc=0;
	extern	char *optarg;
	extern	int optind;

	/*
	** Check out the args
	*/
	while((c=getopt(argc,argv,"h:s:q:e:v"))!= -1)
	{
		switch(c)
		{
			case 'h':
				if (host)
					errFlag++;
				else
					host = optarg;
				break;

			case 's':
				if (sep)
					errFlag++;
				else
					sep = *optarg;
				break;

			case 'q':
				if (quote)
					errFlag++;
				else
					quote = *optarg;
				break;

			case 'e':
				if (esc)
					errFlag++;
				else
					esc = *optarg;
				break;

			case 'v':
				if (verbose)
					errFlag++;
				else
					verbose++;
				break;

			case '?':
				errFlag++;
		}
	}
	if (errFlag)
	{
		fprintf(stderr,"%s", usage);
		exit(EX_USAGE);
	}

	database = argv[optind++];
	table = argv[optind++];

	if (!database || !table)
	{
		fprintf(stderr,"%s", usage);
		exit(EX_USAGE);
	}
	

	dbConnect(host,database);
	dumpTable(table,sep?sep:',',quote,esc?esc:'\\');
	dbDisconnect(host);
	printf("\n");
	exit(0);
}
