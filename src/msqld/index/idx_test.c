#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include "index.h"


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
	idx_hnd	idx;
	int	val,
		recordCount,
		lastRecordCount,
		failCount,
		idxType;
	idx_nod	node;
	idx_cur	cursor;
	idx_env	env;
	int	time1, time2;
	char	*file,
		*type;
	off_t	count;

#ifdef	WRITE_TEST
	writeTest = 1;
#endif
#ifdef	DEBUG
	debug = 1;
#endif
#ifdef	TEST
	test = 1;
#endif

	if (argc < 2)
	{
		printf("\nUsage : idx_test idxType\n\n");
		exit(1);
	}
	type = argv[1];
	idxType = -1;
	if (strcmp(type,"cpi") == 0)
		idxType = IDX_CPI;
	if (strcmp(type,"avl") == 0)
		idxType = IDX_AVL;
	if (idxType < 0)
	{
		printf("\nInvalid index type!  Use 'cpi' or 'avl'\n\n");
		exit(1);
	}

	if (argc > 2)
		file = argv[2];
	else
		file = "/var/tmp/test.idx";
	env.pageSize = 0;
	env.cacheSize = 0;
	setvbuf(stdout, NULL, _IONBF, 0);



	/*
	** Create and initialise the index file
	*/
	if (writeTest)
	{
		unlink(file);
		if(idxCreate(file,idxType,0600,4,IDX_INT32,0,&env) != IDX_OK)
		{
			perror("idxCreate");
			exit(1);
		}
	}

	if(idxOpen(file, idxType, &env, &idx) != IDX_OK)
	{
		bail();
	}

	idxPrintIndexStats(&idx);



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
			if (idxInsert(&idx, makeValue(val), 4, count)!=IDX_OK)
			{
				if (debug)
					printf("Insert of %d failed \n\n",val);
			}
			else
			{
				if (debug)
				printf("%d:%d inserted\n\n",val,(int)count);
			}
			val = val * MUL_VAL;
			val = val % MOD_VAL;
			if (test)
				idxTestIndex(&idx);
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
		if(idxLookup(&idx,makeValue(val),4,CPI_EXACT,&node)!=IDX_OK)
		{
			if (debug)
				printf("Lookup of %d failed \n",val);
			failCount++;
		}
		else
			if (debug)
				printf("%d -> %u\n",val, (u_int)node.data);
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
	idxGetFirst(&idx,&node);
	idxSetCursor(&idx,&cursor);
	while(1)
	{
		count++;
		if (idxGetNext(&idx,&cursor, &node) < 0)
			break;
	}
	time2 = time(NULL);
	printf("Done.  Found %d items in %d seconds\n\n", 
		(int)count, time2-time1);


	/*
	** Scan the index backwards
	*/
	printf("\nTraversing index.... ");
	printf("\nTraversing index backwards.... ");
	idxGetLast(&idx,&node);
	idxSetCursor(&idx,&cursor);
	time1 = time(NULL);
	count = 0;
	while(1)
	{
		count++;
		if (idxGetPrev(&idx,&cursor, &node) < 0)
			break;
	}
	time2 = time(NULL);
	printf("Done.  Found %d items in %d seconds\n\n", 
		(int)count, time2-time1);



	/*
	** Delete some records from the index
	*/
	lastRecordCount = MAXVAL - MINVAL;
	printf( "Deleting %d records ... ",(MAXVAL - MINVAL) / 5);
	time1 = time(NULL);
	val = INIT_VAL;
	for (count = MINVAL; count < (MAXVAL - MINVAL) / 5; count++)
	{
		if (debug)
			printf("About to delete %d:%d\n\n",val,(int)count);
		if (idxDelete(&idx, makeValue(val), 4, count) < 0)
		{
			printf("Delete of %d,%u failed \n\n",val, (int)count);
		}
		val = val * MUL_VAL;
		val = val % MOD_VAL;
		if (test)
		{
			recordCount = idxTestIndex(&idx);
			if (recordCount != lastRecordCount - 1)
			{
				printf("\n\nBad record count (%d not %d)\n\n",
					recordCount, lastRecordCount - 1);
				idxDumpIndex(&idx);
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
		idxDumpIndex(&idx);
		printf("\n\n");
	}



	/*
	** Scan the index forwards
	*/
	printf("\nTraversing index.... ");
	time1 = time(NULL);
	count = 0;
	idxGetFirst(&idx,&node);
	idxSetCursor(&idx,&cursor);
	while(1)
	{
		count++;
		if (idxGetNext(&idx,&cursor, &node) < 0)
			break;
	}
	time2 = time(NULL);
	printf("Done.  Found %d items in %d seconds\n", 
		(int)count, time2-time1);



	/*
	** Lookup the know values an count the failures
	*/
	printf("\nDoing exists test on all inserted values.... ");
	failCount = 0;
	val = INIT_VAL;
	time1 = time(NULL);
	for (count = MINVAL; count < MAXVAL; count++)
	{
		if (idxExists(&idx, makeValue(val), 4, count) < 0)
		{
			failCount++;
			if (debug)
				printf("Lookup of %d : %d failed \n",val,
					(int)count);
		}
		else
		{
			if (debug)
				printf("%d -> %u\n",val, (int)count);
		}
		val = val * MUL_VAL;
		val = val % MOD_VAL;
	}
	time2 = time(NULL);
	printf("Done.\n  Lookups took %d seconds with %d failures\n", 
		time2-time1, failCount);
	printf("\n");

	idxPrintIndexStats(&idx);
	idxClose(&idx);
	exit(0);
}
