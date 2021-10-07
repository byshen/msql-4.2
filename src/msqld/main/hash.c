
#include <stdio.h>

#define NUM_HASH	32
/* #define	COUNT		99999999 */
#define	COUNT		1

main(argc,argv)
	int	argc;
	char	*argv[];
{
	char	*cp1;
        int     hash=0,
		loop;

        cp1 = argv[1];
	for (loop = 0; loop < COUNT; loop++)
	{
		/*
        	while(*cp1)
        	{
                	hash += *cp1++;
        	}
		*/
		switch(strlen(cp1))
		{

			case 1:
                		hash = *(cp1) * 2;
				break;
			default:
                		hash = (*(cp1) * 2) + *(cp1+1);
				break;
			/*
			case 1:
				hash = *(cp1);
				break;
			case 2:
				hash = *(cp1) + *(cp1+1);
				break;
			case 3:
				hash = *(cp1) + *(cp1+1) + *(cp1+2);
				break;
			default:
				hash = *(cp1) + *(cp1+1) + *(cp1+2) + *(cp1+3);
				break;
			*/
		}
	
        	hash = hash & (NUM_HASH - 1);
	}
	/* printf("%s =  %d.\n",argv[1], hash); */
	printf("%d = %s.\n",hash, argv[1]);
}

