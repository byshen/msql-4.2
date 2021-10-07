/*
**
*/


#ifndef __MEMHASH_H
#define __MEMHASH_H

#define	HT_HASH_DEFAULT	0

typedef struct {
	int	curBucket,
		curEntry,
		width,
		depth;
	char	*curPage,
		*table;
} mhash_t;

mhash_t *hashCreate();

#endif
