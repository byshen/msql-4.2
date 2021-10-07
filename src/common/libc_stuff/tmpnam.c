#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *msql_tmpnam(baseName)
        char *baseName;
{
    	static char buf[100];
    	static int nr = 0;
 
    	nr++;
 
    	if (nr == 9999999)
		nr = 1;
 
	sprintf(buf, "/tmp/%s.%07d.%07d", baseName ? baseName:"msql", nr,
		(int)getpid());
	return (char *)strdup(buf);
}

