#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "cpi.h"


void usage()
{
	printf("\n\nCPI Stat - show CPI file statistics\n\n");
	printf("usage : cpi_stat FileName\n\n");
}

int main(argc,argv)
	int	argc;
	char	*argv[];
{
	cpi	*idx;

	if (argc != 2)
	{
		usage();
		exit(1);
	}
	idx = cpiOpen(argv[1], NULL);
	if (!idx)
	{
		printf("\nError : couldn't open CPI file %s\n\n",argv[1]);
		exit(1);
	}

	printf("\n");
	cpiPrintIndexStats(idx);
	cpiClose(idx);
	exit(0);
}
