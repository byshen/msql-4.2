#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <libmsql/msql.h>


#define INSERT_QUERY "insert into test (name,num) values ('item %d', %d)"

#define VERBOSE

int main(argc,argv)
	int	argc;
	char	*argv[];
{
	unsigned int count, num;
	int	sock,
		oldTime,
		curTime,
		cur;
	char	qbuf[160],
		*host = NULL,
		*db,
		*qty;
	
	if (argc != 3 && argc != 5)
	{
		printf("usage : insert_test [-h host] <dbname> <Num>\n\n");
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

	num = atoi(qty);
	count = 0;
	cur = 0;
	oldTime = time(NULL);
	while (count < num)
	{
		snprintf(qbuf,sizeof(qbuf),INSERT_QUERY,count,count);
		if(msqlQuery(sock,qbuf) < 0)
		{
			printf("Query failed for count=%d(%s)\n",count,
				msqlErrMsg);
			printf("Inserted %d rows\n",count);
			exit(1);
		}
		count++;
		cur++;
		if (cur == 100000)
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
