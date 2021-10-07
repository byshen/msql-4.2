#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <libmsql/msql.h>

#define VERBOSE

#define SELECT_QUERY "select name from test where num = %d"


int main(argc,argv)
	int	argc;
	char	*argv[];
{
	int	count,
		cur,
                oldTime,
                curTime,
		sock,
		num;
	char	qbuf[160],
		*host,
		*db,
		*qty;
	
	if (argc != 3 && argc != 5)
	{
		printf("usage : select_test [-h host] <dbname> <num>\n\n");
		exit(1);
	}
        if (argc == 5)
        {
                host = argv[2];
                db = argv[3];
                qty = argv[4];
        }
        else
        {
                host = NULL;
                db = argv[1];
                qty = argv[2];
        }

	if ((sock = msqlConnect(host)) < 0)
	{
		printf("Couldn't connect to engine!\n%s\n\n", msqlErrMsg);
		perror("");
		exit(1);
	}

	if (msqlSelectDB(sock,db) < 0)
	{
		printf("Couldn't select database %s!\n%s\n",argv[1],msqlErrMsg);
	}

	cur = count = 0;
	num = atoi(qty);
        oldTime = time(NULL);
	while (count < num)
	{
		snprintf(qbuf,sizeof(qbuf),SELECT_QUERY,count);
		if(msqlQuery(sock,qbuf) < 0)
		{
			printf("Query failed (%s)\n",msqlErrMsg);
			printf("Selected %d rows\n",count);
			exit(1);
		}
		count++;
                cur++;
                if (cur == 5000)
                {
#ifdef VERBOSE
                        curTime = time(NULL);
                        printf("%d  (%d seconds)\n",count, curTime - oldTime);
                        oldTime = curTime;
#endif
                        cur = 0;
                }
	}
	msqlClose(sock);
	exit(0);
}
