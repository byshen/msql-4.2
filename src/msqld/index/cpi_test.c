#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include "cpi.h"


#define MINVAL	0
#define MAXVAL  250000

#define WRITE_TEST


#define INIT_VAL	17
#define	MUL_VAL		7
#define	MOD_VAL		500000

static	int	writeTest = 0,
		debug = 0,
		test = 0;

void bail()
{
	printf("\n\nBailing\n\n");
	exit(1);
}

/*
char *makeValue(val)
	int	val;
{
	static	char	cBuf[11];

	sprintf(cBuf,"%d",val);
	return(cBuf);
}
*/
#define	makeValue(val) (char *)(&val)


int main(argc, argv)
	int	argc;
	char	*argv[];
{
	cpi	*idx;
	int	val,
		count,
		recordCount,
		lastRecordCount,
		failCount;
	cpi_nod	node;
	cpi_cur	cursor;
	cpi_env	env;
	int	time1, time2;
	char	*file;

#ifdef	WRITE_TEST
	writeTest = 1;
#endif
#ifdef	DEBUG
	debug = 1;
#endif
#ifdef	TEST
	test = 1;
#endif

	if (argc > 1)
		file = argv[1];
	else
		file = "/var/tmp/test.idx";
	env.pageSize = 0;
	env.cacheSize = 64;
	setvbuf(stdout, NULL, _IONBF, 0);



	/*
	** Create and initialise the index file
	*/
	if (writeTest)
	{
		unlink(file);
		if (cpiCreate(file,0600, 4, CPI_INT, 0, &env) < 0)
		{
			perror("cpiCreate");
			exit(1);
		}
	}

	idx = cpiOpen(file, &env);
	if (!idx)
	{
		bail();
	}

	cpiPrintIndexStats(idx);



	/*
	** Insert the data
	*/
	if (writeTest)
	{
		printf("Inserting %d records ... ",MAXVAL - MINVAL);
		time1 = time(NULL);
		val = INIT_VAL;
		for (count = MINVAL; count < MAXVAL; count++)
		{
			if (cpiInsert(idx, makeValue(val), count) < 0)
			{
				if (debug)
					printf("Insert of %d failed \n\n",val);
			}
			else
			{
				if (debug)
				printf("%d:%d inserted\n\n",val,count);
			}
			val = val * MUL_VAL;
			val = val % MOD_VAL;
			if (test)
				cpiTestIndex(idx);
		}
		time2 = time(NULL);
		printf("Done.  Inserts took %d seconds\n", time2-time1);
	}


	/*
	** Peform lookups on every known value
	*/
	failCount = 0;
	printf("\nLooking up all inserted values.... ");
	val = INIT_VAL;
	time1 = time(NULL);
	for (count = MINVAL; count < MAXVAL; count++)
	{
		if (cpiLookup(idx, makeValue(val), &node, CPI_EXACT) < 0)
		{
			if (debug)
				printf("Lookup of %d failed \n",val);
			failCount++;
		}
		else
			if (debug)
				printf("%d -> %u\n",val, node.data);
		val = val * MUL_VAL;
		val = val % MOD_VAL;
	}
	time2 = time(NULL);
	printf("Done.\nLookups took %d seconds with %d failures\n", 
		time2-time1, failCount);



	/*
	** Scan the index forwards
	*/
	printf("\nTraversing index.... ");
	time1 = time(NULL);
	count = 0;
	cpiGetFirst(idx,&node);
	cpiSetCursor(idx,&cursor);
	while(1)
	{
		count++;
		if (cpiGetNext(idx,&cursor, &node) < 0)
			break;
	}
	time2 = time(NULL);
	printf("Done.  Found %d items in %d seconds\n\n", 
		count, time2-time1);


	/*
	** Scan the index backwards
	*/
	printf("\nTraversing index.... ");
	printf("\nTraversing index backwards.... ");
	cpiGetLast(idx,&node);
	cpiSetCursor(idx,&cursor);
	time1 = time(NULL);
	count = 0;
	while(1)
	{
		count++;
		if (cpiGetPrev(idx,&cursor, &node) < 0)
			break;
	}
	time2 = time(NULL);
	printf("Done.  Found %d items in %d seconds\n\n", 
		count, time2-time1);



	/*
	** Delete some records from the index
	*/
	lastRecordCount = MAXVAL - MINVAL;
	printf( "Deleting %d records ... ",(MAXVAL - MINVAL) / 5);
	time1 = time(NULL);
	val = INIT_VAL;
	for (count = 0; count < (MAXVAL - MINVAL) / 5; count++)
	{
		if (debug)
			printf("About to delete %d:%d\n\n",val,count);
		if (cpiDelete(idx, makeValue(val), count) < 0)
		{
			printf("Delete of %d failed \n\n",val);
		}
		val = val * MUL_VAL;
		val = val % MOD_VAL;
		if (test)
		{
			recordCount = cpiTestIndex(idx);
			if (recordCount != lastRecordCount - 1)
			{
				printf("\n\nBad record count (%d not %d)\n\n",
					recordCount, lastRecordCount - 1);
				cpiDumpIndex(idx);
				exit(1);
			}
			lastRecordCount--;
		}
	}
	time2 = time(NULL);
	printf( "Done.  Deletes took %d seconds\n", time2-time1);

	if (debug)
	{
		printf("\n\n");
		cpiDumpIndex(idx);
		printf("\n\n");
	}



	/*
	** Scan the index forwards
	*/
	printf("\nTraversing index.... ");
	time1 = time(NULL);
	count = 0;
	cpiGetFirst(idx,&node);
	cpiSetCursor(idx,&cursor);
	while(1)
	{
		count++;
		if (cpiGetNext(idx,&cursor, &node) < 0)
			break;
	}
	time2 = time(NULL);
	printf("Done.  Found %d items in %d seconds\n", 
		count, time2-time1);



	/*
	** Lookup the know values an count the failures
	*/
	printf("\nDoing exists test on all inserted values.... ");
	failCount = 0;
	val = INIT_VAL;
	time1 = time(NULL);
	for (count = MINVAL; count < MAXVAL; count++)
	{
		if (cpiExists(idx, makeValue(val), count) < 0)
		{
			failCount++;
			if (debug)
				printf("Lookup of %d : %d failed \n",val,count);
		}
		else
		{
			if (debug)
				printf("%d -> %u\n",val, count);
		}
		val = val * MUL_VAL;
		val = val % MOD_VAL;
	}
	time2 = time(NULL);
	printf("Done.\n  Lookups took %d seconds with %d failures\n", 
		time2-time1, failCount);
	printf("\n");

	cpiPrintIndexStats(idx);
	cpiClose(idx);
	exit(0);
}
