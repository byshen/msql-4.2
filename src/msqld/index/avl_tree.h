/*      
** avl_tree.h	AVL Balanced Tree Library Header
**
** Copyright (c) 1996  Hughes Technologies Pty Ltd
**
*************************************************************************
**
*/

#ifndef __AVL_TREE_H
#define __AVL_TREE_H


/* 
** AVL Tree Dump Options 
*/
#define AVL_QUIET	0
#define	AVL_VERBOSE	1


typedef struct {
	int	magic,		/* Magic number for tree (for testing) */
		keyLen,		/* Length of key values */
		nodeLen,	/* On disk node length */
		keyType,	/* Data type  of key */
		flags;		/* Flags for unique etc. */
	int	root,		/* Node number of tree root */
		freeList;	/* Node number of start of free list */
	off_t	fileLen;	/* Has another proc extended the file?  */
	u_int	numEntries,	/* Total number of entries in tree */
		numKeys;	/* Number of distinct key values */
	
} avl_sbk;


typedef struct {
	char	*mapRegion,	/* Pointer to mapping of the file */
		*oldRegion,	/* Pointer to last mapping location*/
		*memTree;	/* Pointer to malloc space for in-core tree*/
	off_t	mapLen,		/* Length of file mapping */
		memLen,		/* Size of malloced region of in-core tree */
		oldLen;		/* Length of last mapping */
	avl_sbk	*sblk;		/* Pointer to file superblock */
	int	fd,		/* Open file descriptor of tree file */
		remapped,	/* Flag to indicate the tree was remapped */
		curNode;	/* Flags last node of last tree search */
} avltree;



typedef struct {
	int	curNode,	/* Current node for get/next */
		curDup;		/* Current dup for get/next */
} avl_cur;


typedef struct node_s {
	int	nodeNum,	/* Node number for this node */
		left,		/* Node number of left node */
		right,		/* Node number of right node */
		parent,		/* Node number of parent node */
		dup;		/* Node number of head of dup list */
	short	height;		/* Node height indicator */
	char	*key;		/* Pointer to key value */
	off_t	data;		/* Offset value for main data table */
} avl_nod;


/* Prototypes */

int 	avlCreate();
int 	avlInsert();
int 	avlDelete();
int 	avlTestTree();
void 	avlClose();
void    avlSync();
void	avlDumpTree();
void	avlSetCursor();
void	avlPrintElement();
avltree	*avlOpen();
avltree	*avlMemCreate();
avl_nod *avlGetNode();
avl_nod	*avlFindNode();
avl_nod	*avlGetFirst();
avl_nod	*avlGetLast();
avl_nod	*avlGetNext();
avl_nod	*avlGetPrev();
avl_nod	*avlLookup();


#define avlKey(n) ((char*)n+sizeof(avl_nod))


#endif
