/*
**	msqlexplain.c	- 
**
**
** Copyright (c) 1999-2000  Hughes Technologies Pty Ltd
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
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#if defined(_OS_WIN32)
#  include <winsock.h>
#endif

#include <common/portability.h>
#include <common/msql_defs.h>
#include <libmsql/msql.h>

#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif

char *msql_tmpnam();


void usage()
{
	(void)fprintf(stderr,"\n\nUsage : msqlexplain [-f conf_file] [-h host] database\n\n");
	exit(0);
}



void help()
{
	(void)printf("\n\nMiniSQL Explain Help!\n\n");
	(void)printf("The following commands are available :- \n\n");
	(void)printf("\t\\q	Quit\n");
	(void)printf("\t\\g	Go (Send query to database)\n");
	(void)printf("\t\\e	Edit (Edit previous query)\n");
	(void)printf("\t\\p	Print (Print the query buffer)\n");
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
	read(fd,q, MSQL_PKT_LEN);
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

        while((c=getopt(argc,argv,"f:h:"))!= -1)
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

                        case '?':
                                errFlag++;
                                break;
                }
        }
 
        argsLeft = argc - optind;


	printf("\n");
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
		msqlLoadConfigFile(confFile);
	}

	if ((sock = msqlConnect(host)) < 0)
	{
		printf("ERROR : %s\n",msqlErrMsg);
		exit(1);
	}

	if (msqlSelectDB(sock, argv[optind]) < 0)
	{
		printf("ERROR : %s\n",msqlErrMsg);
		exit(1);
	}

	/*
	**  Run in interactive mode like the ingres/postgres monitor
	*/

	printf("Welcome to the miniSQL explainer.  Type \\h for help.\n\n");
		
	inchar = EOF+1;
	(void)bzero(qbuf,sizeof(qbuf));
	cp = qbuf;
	printf("\nExplain > ");
	fflush(stdout);
	while(!feof(stdin))
	{
		inchar = fgetc(stdin);
		qLen ++;
		if (qLen == MSQL_PKT_LEN)
		{
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
					printf("\nExplain > ");
					prompt=0;
					break;
				case 'g':
					if(msqlExplain(sock,qbuf) < 0)
					{
						printf("%s\n",msqlErrMsg);
					}
					newQ = 1;
					qLen = 0;
					inString = 0;
					printf("\nExplain > ");
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
					printf("\n\nBye!\n\n");	
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
					printf("\nExplain > ");
					prompt=0;
					break;
			}
		}
		else
		{
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
					printf("    -> ");
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
	printf("\nBye!\n\n");
	exit(0);
}
