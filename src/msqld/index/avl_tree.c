/*      
** avl_tree.c	- AVL Tree Library
**
** Copyright (c) 1996  Hughes Technologies Pty Ltd
**
*************************************************************************
**
** This library implements a balanced tree capable of holding key/offset
** pairs.  The offset is assumed to be used as an offset into a data table.
** The library supports user defined key lengths on a per tree basis, and
** support for multiplte data types (i.e. int's, char strings and reals are
** inserted into the tree in the correct order, not just the order of the
** bytes that comprise the value).  This allows the btree to be used for
** range indexing.  Duplicate keys are supported.
**
** This library was written for Mini SQL.
**
** Mods for mSQL 2.1.x - Data stats are mainteined in the super block 
** including the total number of entries and the number of unique keys.
** The engine can use that info to determine index density when working
** on "union index" creation.
**
*/


/* #define AVL_DEBUG */

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <common/config.h>
#include <common/config_extras.h>
#include <common/portability.h>

#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif
#ifdef HAVE_STDINT_H
#  include <stdint.h>
#endif


#include <common/debug/debug.h>

#include "index.h"
#include "avl_tree.h"

#define	AVL_MAGIC 		0x01020304
#define	AVL_INVALID 		0xAABBCCDD

#define	SBLK_SIZE		(sizeof(avl_sbk) + (8 - (sizeof(avl_sbk)% 8)))
#define DISK_NODE_SIZE(L)	(sizeof(avl_nod)+(L))
#define OFF_TO_NODE(t,o)	((o - SBLK_SIZE) / t->sblk->nodeLen + 1)
#define NODE_TO_OFF(t,n)	(((n - 1) * t->sblk->nodeLen) + SBLK_SIZE)
#define	REG			register


#define bcopy4(s,d) \
      ((((unsigned char *)d)[3] = ((unsigned char *)s)[3]), \
       (((unsigned char *)d)[2] = ((unsigned char *)s)[2]), \
       (((unsigned char *)d)[1] = ((unsigned char *)s)[1]), \
       (((unsigned char *)d)[0] = ((unsigned char *)s)[0]))

#define bcopy8(s,d) \
      ((((unsigned char *)d)[7] = ((unsigned char *)s)[7]), \
       (((unsigned char *)d)[6] = ((unsigned char *)s)[6]), \
       (((unsigned char *)d)[5] = ((unsigned char *)s)[5]), \
       (((unsigned char *)d)[4] = ((unsigned char *)s)[4]), \
       (((unsigned char *)d)[3] = ((unsigned char *)s)[3]), \
       (((unsigned char *)d)[2] = ((unsigned char *)s)[2]), \
       (((unsigned char *)d)[1] = ((unsigned char *)s)[1]), \
       (((unsigned char *)d)[0] = ((unsigned char *)s)[0]))


#define moveValue(src,srcnum,dst,dstnum,tree) \
	copyValue(src->keys[srcnum],dst->keys[dstnum],tree);	\
	dst->data[dstnum] = src->data[srcnum];			\
	dst->dups[dstnum] = src->dups[srcnum]


/*************************************************************************
**************************************************************************
**                                                                      **
**                         UTILITY ROUTINES                             **
**                                                                      **
**************************************************************************
*************************************************************************/

#define DISK_EXTEND_LENGTH		(1024 * 5)
#define MEM_EXTEND_LENGTH		256

static int extendTree(tree)
	avltree	*tree;
{
	int	nodeLen,
		count,
		extendLen;
	off_t	nodeOff,
		curOff;
	avl_nod	*cur;


	if (tree->memTree == NULL)
	{
		/*
		** Unmap the file, extend it and then re-map it
		*/
		extendLen = DISK_EXTEND_LENGTH;
		tree->oldRegion = tree->mapRegion;
		tree->oldLen = tree->mapLen;
		tree->remapped = 1;
		nodeLen = tree->sblk->nodeLen;
		munmap(tree->mapRegion, tree->mapLen);
		lseek(tree->fd, (extendLen * nodeLen) - 1, SEEK_END );
		if (write(tree->fd, "\0", 1) < 0)
		{
			perror("write failed");
			return(-1);
		}
		nodeOff = tree->mapLen;
		tree->mapLen += extendLen * nodeLen;
		tree->mapRegion = mmap(NULL, tree->mapLen, PROT_READ|PROT_WRITE,
			MAP_SHARED, tree->fd, (off_t)0);
		if (tree->mapRegion == (void*)-1)
		{
			char msg[160];
			sprintf(msg,"mmap of %u failed",(unsigned int)
				tree->mapLen);
			perror(msg);
			return(-1);
		}
		tree->sblk = (avl_sbk *)tree->mapRegion;
		tree->sblk->fileLen = tree->mapLen;
	}
	else
	{
		extendLen = MEM_EXTEND_LENGTH;
		nodeOff = tree->memLen;
		nodeLen = tree->sblk->nodeLen;
		tree->memTree = realloc(tree->memTree,tree->memLen +
			(extendLen * nodeLen));
		bzero(tree->memTree + tree->memLen, extendLen * nodeLen);
		tree->sblk = (avl_sbk *)tree->memTree;
		tree->mapRegion = (char *)tree->memTree;
		tree->memLen += extendLen * tree->sblk->nodeLen;
	}
	
	/*
	** Setup the free list to wind through the new nodes
	*/
	curOff = nodeOff;
	for (count = 0; count < extendLen; count++)
	{
		cur = (avl_nod *)(tree->mapRegion + curOff);
		cur->parent = tree->sblk->freeList;
		tree->sblk->freeList = OFF_TO_NODE(tree, curOff);
		curOff += tree->sblk->nodeLen;
	}
	return(0);
}



static void checkTreeRemap(tree)
	avltree	*tree;
{
	off_t	oldLen,
		newLen;

	if (tree->sblk->fileLen > tree->mapLen)
	{
		oldLen = tree->mapLen;
		newLen = tree->sblk->fileLen;
		munmap(tree->mapRegion, oldLen);
		tree->mapRegion = mmap(NULL, newLen, PROT_READ|PROT_WRITE,
			MAP_SHARED, tree->fd, (off_t)0);
		tree->sblk = (avl_sbk *)tree->mapRegion;
		tree->mapLen = newLen;
	}
}
		


static avl_nod *getFreeNode(tree)
	avltree	*tree;
{
	int	freeNode = 0;
	avl_nod	*cur;


	checkTreeRemap(tree);
	if (tree->sblk->freeList == 0)
	{
		if (extendTree(tree) < 0)
			return(NULL);
	}
	freeNode = tree->sblk->freeList;
	cur = (avl_nod*)(tree->mapRegion + NODE_TO_OFF(tree,freeNode));
	tree->sblk->freeList = cur->parent;
	cur->left = cur->right = cur->parent = cur->height = cur->dup = 0;
	cur->key = (char *)((char *)cur)+sizeof(avl_nod);
	cur->nodeNum = freeNode;
	return(cur);
}



static void dropNode(tree,cur)
        avltree	*tree;
        avl_nod	*cur;
{
        cur->parent = tree->sblk->freeList;
        tree->sblk->freeList = cur->nodeNum;
}


avl_nod *avlGetNode(tree, num)
	avltree	*tree;
	int	num;
{
	avl_nod	*cur;

	if (num == 0)
		return(NULL);
	checkTreeRemap(tree);
	cur = (avl_nod *)(tree->mapRegion + NODE_TO_OFF(tree, num));
	cur->key = ((char *)cur) + sizeof(avl_nod);
	return(cur);
}




void avlPrintElement(data,type,len)
	char	*data;
	int	type,
		len;
{
	switch(type)
	{
		case IDX_CHAR:
			printf("char '%s'",data);
			break;

		case IDX_BYTE:
			{
				int	count = 0;
				printf("byte ");
				while(count < len)
				{
					printf("%u.", (u_char)*(data+count));
					count++;
				}
			}
			break;

		case IDX_INT8:
			printf("int8 '%hhd'",(int8_t)*(int8_t*)data);
			break;

		case IDX_INT16:
			printf("int16 '%hd'",(int16_t)*(int16_t*)data);
			break;

		case IDX_INT32:
			printf("int '%d'",(int)*(int*)data);
			break;

		case IDX_INT64:
			printf("int64 '%lld'",(int64_t)*(int64_t*)data);
			break;
			
		case IDX_UINT8:
			printf("uint8 '%hhu'",(uint8_t)*(uint8_t*)data);
			break;

		case IDX_UINT16:
			printf("uint16 '%hu'",(uint16_t)*(uint16_t*)data);
			break;

		case IDX_UINT32:
			printf("uint '%u'",(u_int)*(u_int*)data);
			break;

		case IDX_UINT64:
			printf("uint64 '%llu'",(uint64_t)*(uint64_t*)data);
			break;

		case IDX_REAL:
			printf("real '%f'",(double)*(double*)data);
			break;

		default:
			printf("Unknown Type!!!");
			break;
	}
}


static void copyValue(src,dst,tree)
	char	*src,
		*dst;
	avltree	*tree;
{
	switch(tree->sblk->keyType)
	{
		case IDX_INT8:
		case IDX_UINT8:
			bcopy(src,dst,1);
			break;
		case IDX_INT16:
		case IDX_UINT16:
			bcopy(src,dst,2);
			break;
		case IDX_INT32:
		case IDX_UINT32:
			bcopy4(src,dst);
			break;
		case IDX_REAL:
			bcopy8(src,dst);
			break;
		case IDX_CHAR:
			strncpy(dst,src,tree->sblk->keyLen-1);
			*(dst+tree->sblk->keyLen-1)=0;
			break;
		default:
			bcopy(src, dst, tree->sblk->keyLen);
	}
}




static void showIndent(d)
	int	d;
{
	int	i;

	for(i = 0; i < d; i++)
	{
		printf("   ");
	}
}



static int getNodeHeight(node)
	avl_nod	*node;
{
	return( (node)? node->height : -1);
}


#define setNodeHeight(tree, node)			\
{							\
	int	_leftH,					\
		_rightH;				\
	avl_nod	*_lNode,				\
		*_rNode;				\
							\
	if (node)					\
	{						\
		_lNode = avlGetNode(tree,node->left);	\
		_leftH = getNodeHeight(_lNode);		\
		_rNode = avlGetNode(tree,node->right);	\
		_rightH = getNodeHeight(_rNode);	\
		if (_leftH > _rightH)			\
			node->height = 1 + _leftH;	\
		else					\
			node->height = 1 + _rightH;	\
	}						\
}


static void rotateRight(tree, node)
	avltree	*tree;
	avl_nod	*node;
{
	avl_nod	*parent,
		*left,
		*tmp;

#ifdef AVL_DEBUG
	printf("Right rotate @ ");
	avlPrintElement(node->key,tree->sblk->keyType,tree->sblk->keyLen);
	printf("\n");
#endif

	parent = avlGetNode(tree,node->parent);
	left = avlGetNode(tree, node->left);
	node->left = left->right;
	tmp = avlGetNode(tree,node->left);
	if (tmp)
		tmp->parent = node->nodeNum;
	left->right = node->nodeNum;
	if (parent)
	{
		if (parent->left == node->nodeNum)
			parent->left = left->nodeNum;
		else
			parent->right = left->nodeNum;
	}
	else
	{
		tree->sblk->root = left->nodeNum;
	}
	left->parent = node->parent;
	node->parent = left->nodeNum;
	setNodeHeight(tree,left);
	setNodeHeight(tree,node);
}



static void rotateLeft(tree, node)
	avltree	*tree;
	avl_nod	*node;
{
	avl_nod	*parent,
		*right,
		*tmp;

#ifdef AVL_DEBUG
	printf("Left rotate @ ");
	avlPrintElement(node->key,tree->sblk->keyType,tree->sblk->keyLen);
	printf("\n");
#endif
	parent = avlGetNode(tree,node->parent);
	right = avlGetNode(tree, node->right);
	node->right = right->left;
	tmp = avlGetNode(tree,node->right);
	if (tmp)
		tmp->parent = node->nodeNum;
	right->left = node->nodeNum;
	if (parent)
	{
		if (parent->right == node->nodeNum)
			parent->right = right->nodeNum;
		else
			parent->left = right->nodeNum;
	}
	else
	{
		tree->sblk->root = right->nodeNum;
	}
	right->parent = node->parent;
	node->parent = right->nodeNum;
	setNodeHeight(tree,right);
	setNodeHeight(tree,node);
}



static void doubleRight(tree, node)
	avltree	*tree;
	avl_nod	*node;
{
	avl_nod	*tmp;

	tmp = avlGetNode(tree, node->left);
	rotateLeft(tree,tmp);
	rotateRight(tree,node);
}


static void doubleLeft(tree, node)
	avltree	*tree;
	avl_nod	*node;
{
	avl_nod	*tmp;

	tmp = avlGetNode(tree,node->right);
	rotateRight(tree,tmp);
	rotateLeft(tree,node);
}

static void balanceTree(tree, node)
	avltree	*tree;
	avl_nod	*node;
{
	int	leftHeight,
		rightHeight;
	avl_nod	*left,
		*right,
		*child;

	left = avlGetNode(tree, node->left);
	right = avlGetNode(tree, node->right);
	leftHeight = getNodeHeight(left);
	rightHeight = getNodeHeight(right);

	if (leftHeight > rightHeight + 1)
	{
		child = avlGetNode(tree, node->left);
		left = avlGetNode(tree,child->left);
		right = avlGetNode(tree,child->right);
		leftHeight = getNodeHeight(left);
		rightHeight = getNodeHeight(right);

		if (leftHeight >= rightHeight)
		{
			rotateRight(tree,node);
		}
		else
		{
			doubleRight(tree,node);
		}
	}
	else if (rightHeight > leftHeight + 1)
	{
		child = avlGetNode(tree, node->right);
		left = avlGetNode(tree,child->left);
		right = avlGetNode(tree,child->right);
		leftHeight = getNodeHeight(left);
		rightHeight = getNodeHeight(right);

		if (rightHeight >= leftHeight)
			rotateLeft(tree,node);
		else
			doubleLeft(tree,node);
	}
}


/*************************************************************************
**************************************************************************
**                                                                      **
**                       TYPE HANDLING ROUTINES                         **
**                                                                      **
**************************************************************************
*************************************************************************/


static int int8Compare(v1,v2)
	char	*v1,
		*v2;
{
	int8_t	int1,
		int2;

	bcopy(v1,&int1,1);
	bcopy(v2,&int2,1);
	if (int1 > int2)
		return(1);
	if (int1 < int2)
		return(-1);
	return(0);
}


static int uint8Compare(v1,v2)
	char	*v1,
		*v2;
{
	uint8_t	int1,
		int2;

	bcopy(v1,&int1,1);
	bcopy(v2,&int2,1);
	if (int1 > int2)
		return(1);
	if (int1 < int2)
		return(-1);
	return(0);
}

static int int16Compare(v1,v2)
	char	*v1,
		*v2;
{
	int16_t	int1,
		int2;

	bcopy(v1,&int1,2);
	bcopy(v2,&int2,2);
	if (int1 > int2)
		return(1);
	if (int1 < int2)
		return(-1);
	return(0);
}


static int uint16Compare(v1,v2)
	char	*v1,
		*v2;
{
	uint16_t int1,
		int2;

	bcopy(v1,&int1,2);
	bcopy(v2,&int2,2);
	if (int1 > int2)
		return(1);
	if (int1 < int2)
		return(-1);
	return(0);
}

static int int32Compare(v1,v2)
	char	*v1,
		*v2;
{
	int	int1,
		int2;

	bcopy4(v1,&int1);
	bcopy4(v2,&int2);
	if (int1 > int2)
		return(1);
	if (int1 < int2)
		return(-1);
	return(0);
}


static int uint32Compare(v1,v2)
	char	*v1,
		*v2;
{
	u_int	int1,
		int2;

	bcopy4(v1,&int1);
	bcopy4(v2,&int2);
	if (int1 > int2)
		return(1);
	if (int1 < int2)
		return(-1);
	return(0);
}

static int int64Compare(v1,v2)
	char	*v1,
		*v2;
{
	int64_t	int1,
		int2;

	bcopy8(v1,&int1);
	bcopy8(v2,&int2);
	if (int1 > int2)
		return(1);
	if (int1 < int2)
		return(-1);
	return(0);
}


static int uint64Compare(v1,v2)
	char	*v1,
		*v2;
{
	uint64_t int1,
		int2;

	bcopy8(v1,&int1);
	bcopy8(v2,&int2);
	if (int1 > int2)
		return(1);
	if (int1 < int2)
		return(-1);
	return(0);
}



static int realCompare(v1,v2)
	char	*v1,
		*v2;
{
	double	r1,
		r2;

	bcopy8(v1,&r1);
	bcopy8(v2,&r2);
	if (r1 > r2)
		return(1);
	if (r1 < r2)
		return(-1);
	return(0);
}



static int charCompare(v1,v2)
	char	*v1,
		*v2;
{
	char	*cp1,
		*cp2;

	cp1 = v1;
	cp2 = v2;
	while (*cp1 == *cp2)
	{
		if (*cp1 == 0)
		{
			return(0);
		}
		cp1++;
		cp2++;
	}
	if ((unsigned char )*cp1 > (unsigned char)*cp2)
		return(1);
	return(-1);
}


static int byteCompare(v1,v2,len)
	char	*v1,
		*v2;
	int	len;
{
	char	*cp1,
		*cp2;

	cp1 = v1;
	cp2 = v2;
	while (*cp1 == *cp2 && len)
	{
		cp1++;
		cp2++;
		len--;
	}
	if (len == 0)
		return(0);
	if ( (unsigned char) *cp1 > (unsigned char)*cp2)
		return(1);
	return(-1);
}





static int compareValues(v1,v2,tree)
	char	*v1,
		*v2;
	avltree	*tree;
{
	switch(tree->sblk->keyType)
	{
		case IDX_INT8:
			return(int8Compare(v1,v2));
		case IDX_INT16:
			return(int16Compare(v1,v2));
		case IDX_INT32:
			return(int32Compare(v1,v2));
		case IDX_INT64:
			return(int64Compare(v1,v2));
		case IDX_UINT8:
			return(uint8Compare(v1,v2));
		case IDX_UINT16:
			return(uint16Compare(v1,v2));
		case IDX_UINT32:
			return(uint32Compare(v1,v2));
		case IDX_UINT64:
			return(uint64Compare(v1,v2));
		case IDX_REAL:
			return(realCompare(v1,v2));
		case IDX_CHAR:
			return(charCompare(v1,v2));
		default:
			return(byteCompare(v1,v2,tree->sblk->keyLen));
	}
}


static void swapNodes(tree, src, dst)
	avltree	*tree;
	avl_nod	*src,
		*dst;
{
	int	tmpLeft,
		tmpRight,
		tmpParent,
		tmpHeight;
	avl_nod	*parent,
		*tmp;

	/*
	** Re-link the nodes
	*/
	tmpLeft = dst->left;
	tmpRight = dst->right;
	tmpParent = dst->parent;
	tmpHeight = dst->height;

	dst->left = src->left;
	dst->right = src->right;
	dst->parent = src->parent;
	dst->height = src->height;

	src->left = tmpLeft;
	src->right = tmpRight;
	src->parent = tmpParent;
	src->height = tmpHeight;

	/*
	** Check for wierd links left over from swapping neighbours
	*/
	if (dst->left == dst->nodeNum)
		dst->left = src->nodeNum;
	if (dst->right == dst->nodeNum)
		dst->right = src->nodeNum;
	if (dst->parent == dst->nodeNum)
		dst->parent = src->nodeNum;
	if (src->left == src->nodeNum)
		src->left = dst->nodeNum;
	if (src->right == src->nodeNum)
		src->right = dst->nodeNum;
	if (src->parent == src->nodeNum)
		src->parent = dst->nodeNum;
	


	/*
	** Relink the parents
	*/
	if (src->parent != dst->nodeNum)
	{
		parent = avlGetNode(tree, src->parent);
		if (parent)
		{
			if (parent->left == dst->nodeNum)
				parent->left = src->nodeNum;
			else
				parent->right = src->nodeNum;
		}
		else
		{
			tree->sblk->root = src->nodeNum;
		}
	}
	if (dst->parent != src->nodeNum)
	{
		parent = avlGetNode(tree,dst->parent);
		if (parent)
		{
			if (parent->left == src->nodeNum)
				parent->left = dst->nodeNum;
			else
				parent->right = dst->nodeNum;
		}
		else
		{
			tree->sblk->root = dst->nodeNum;
		}
	}

	/*
	** Relink the kids
	*/

	if (src->left != dst->nodeNum)
	{
		tmp = avlGetNode(tree,src->left);
		if (tmp)
			tmp->parent = src->nodeNum;
	}
	if (src->right != dst->nodeNum)
	{
		tmp = avlGetNode(tree,src->right);
		if (tmp)
			tmp->parent = src->nodeNum;
	}
	if (dst->left != dst->nodeNum)
	{
		tmp = avlGetNode(tree,dst->left);
		if (tmp)
			tmp->parent = dst->nodeNum;
	}
	if (dst->right != dst->nodeNum)
	{
		tmp = avlGetNode(tree,dst->right);
		if (tmp)
			tmp->parent = dst->nodeNum;
	}
}


/*************************************************************************
**************************************************************************
**                                                                      **
**                        INTERFACE ROUTINES                            **
**                                                                      **
**************************************************************************
*************************************************************************/


int avlCreate(path,mode,keyLen,keyType,flags)
	char	*path;
	int	mode,
		keyLen,
		keyType,
		flags;
{
	int	fd;
	avl_sbk	sblk;
	char	blank[1];

	/*
	** Create the actual file
	*/
	fd = open(path,O_RDWR | O_CREAT, mode);
	if (fd < 0)
	{
		return(-1);
	}

	/*
	** Create the base contents
	*/
	bzero(&sblk, sizeof(avl_sbk));
	sblk.magic = AVL_MAGIC;
	sblk.keyType = keyType;
	if (keyType == IDX_CHAR)
		keyLen++;
	sblk.keyLen = keyLen;
	sblk.flags = flags;
	sblk.nodeLen = DISK_NODE_SIZE(keyLen) + (8 -
                (DISK_NODE_SIZE(keyLen) % 8));

	if (write(fd,&sblk,sizeof(sblk)) < sizeof(sblk))
	{
		close(fd);
		unlink(path);
		return(-1);
	}
	if (sizeof(sblk) < SBLK_SIZE)
	{
		*blank = 0;
		lseek(fd,SBLK_SIZE-1,SEEK_SET);
		write(fd,blank,1);
	}
	close(fd);
	return(0);
}



avltree *avlMemCreate(keyLen,keyType,flags)
	int	keyLen,
		keyType,
		flags;
{
	avltree	*new;
	int	nodeLen;


	/*
	** Create the base contents
	*/
	new = (avltree *)malloc(sizeof(avltree));
	bzero(new, sizeof(avltree));
	nodeLen = DISK_NODE_SIZE(keyLen) + (8 - (DISK_NODE_SIZE(keyLen) % 8));
	new->memLen = SBLK_SIZE;
	new->memTree = (char *)malloc(new->memLen);
	bzero(new->memTree, new->memLen);

	new->sblk = (avl_sbk *)new->memTree;
	new->sblk->magic = AVL_MAGIC;
	new->sblk->keyType = keyType;
	if (keyType == IDX_CHAR)
		keyLen++;
	new->sblk->keyLen = keyLen;
	new->sblk->flags = flags;
	new->sblk->nodeLen = DISK_NODE_SIZE(keyLen) + (8 -
                (DISK_NODE_SIZE(keyLen) % 8));

	new->mapLen = new->memLen;
	new->mapRegion = new->memTree;
	return(new);
}


avltree *avlOpen(path)
	char	*path;
{
	int	fd;
	avltree	*new;
	struct	stat sbuf;


	fd = open(path,O_RDWR, 0);
	if (fd < 0)
	{
		return(NULL);
	}

	if (fstat(fd, &sbuf) < 0)
	{
		return(NULL);
	}
	new = (avltree *)malloc(sizeof(avltree));
	bzero(new,sizeof(avltree));
	new->fd = fd;
	new->mapLen = sbuf.st_size;
	new->mapRegion = mmap(NULL, new->mapLen, PROT_READ|PROT_WRITE,
		MAP_SHARED, new->fd, (off_t)0);
	if (new->mapRegion == (caddr_t)-1)
	{
		close(fd);
		free(new);
		return(NULL);
	}
	new->sblk = (avl_sbk *)new->mapRegion;
	new->sblk->fileLen = new->mapLen;
	if (new->sblk->magic != AVL_MAGIC)
	{
		close(fd);
		free(new);
		return(NULL);
	}
	return(new);
}


void avlClose(tree)
	avltree	*tree;
{
	if (tree == NULL)
	{
		return;
	}
	if (tree->memTree == NULL)
	{
		munmap(tree->mapRegion, tree->mapLen);
		close(tree->fd);
	}
	else
	{
		free(tree->memTree);
		tree->memTree = NULL;
	}
	free(tree);
}



void avlSync(tree)
	avltree	*tree;
{
	if (tree != NULL && tree->mapLen)
		MSYNC(tree->mapRegion, tree->mapLen, 0);
}



avl_nod *avlLookup(tree, key, flags)
	avltree	*tree;
	char	*key;
	int	flags;
{
	REG 	int	res;
	REG	avl_nod	*cur;


#ifdef AVL_DEBUG
	printf("AVL Lookup of ");
	avlPrintElement(key,tree->sblk->keyType,tree->sblk->keyLen);
	printf("\n");
#endif

	tree->curNode = 0;
	cur = avlGetNode(tree,tree->sblk->root);
	while(cur)
	{
		tree->curNode = cur->nodeNum;
		res = compareValues(key, avlKey(cur), tree);
#ifdef AVL_DEBUG
		printf("\tCompare node %d ",cur->nodeNum);
		avlPrintElement(cur->key,tree->sblk->keyType,
			tree->sblk->keyLen);
		printf(".  Result = %d\n",res);
#endif
		if (res > 0)
		{
			if (cur->right == 0)
			{
				if (flags & IDX_EXACT)
					return(NULL);
				else
					return(cur);
			}
			cur = avlGetNode(tree,cur->right);
			continue;
		}
		if (res < 0)
		{
			if (cur->left == 0)
			{
				if (flags & IDX_EXACT)
					return(NULL);
				else
					return(cur);
			}
			cur = avlGetNode(tree,cur->left);
			continue;
		}
		return(cur);
	}
	return(NULL);
}



void avlSetCursor(tree, cursor)
	avltree	*tree;
	avl_cur	*cursor;
{
	cursor->curNode = tree->curNode;
	cursor->curDup = 0;
}



avl_nod *avlGetFirst(tree)
	avltree	*tree;
{
	avl_nod	*cur;

	cur = avlGetNode(tree,tree->sblk->root);
	tree->curNode = 0;
	if (!cur)
		return(NULL);
	while(cur->left)
	{
		cur = avlGetNode(tree,cur->left);
	}
	tree->curNode = cur->nodeNum;
	return(cur);
}


avl_nod *avlGetLast(tree)
	avltree	*tree;
{
	avl_nod	*cur;

	cur = avlGetNode(tree,tree->sblk->root);
	tree->curNode = 0;
	if (!cur)
		return(NULL);
	while(cur->right)
	{
		cur = avlGetNode(tree,cur->right);
	}
	tree->curNode = cur->nodeNum;
	return(cur);
}



avl_nod *avlGetNext(tree,cursor)
	avltree	*tree;
	avl_cur	*cursor;
{
	avl_nod	*cur;
	int	prevNode;

	/*
	** Are we somewhere down a dup chain?
	*/
	if (cursor->curDup != 0)
	{
		cur = avlGetNode(tree,cursor->curDup);
		if (cur->dup)
		{
			cur = avlGetNode(tree,cur->dup);
			cursor->curDup = cur->nodeNum;
			return(cur);
		}
	}
	cur = avlGetNode(tree,cursor->curNode);
	if (cur->dup != 0 && cursor->curDup == 0)
	{
		cursor->curDup = cur->dup;
		cur = avlGetNode(tree,cur->dup);
		return(cur);
	}


	/*
	** No dups to return so move onto the next node
	*/
	if (cur->right)
	{
		cur = avlGetNode(tree,cur->right);
		while(cur->left)
			cur = avlGetNode(tree,cur->left);
		cursor->curNode = cur->nodeNum;
		cursor->curDup = 0;
		return(cur);
	}

	while (cur->parent)
	{
		prevNode = cur->nodeNum;
		cur = avlGetNode(tree,cur->parent);
		if (cur->left == prevNode)
		{
			cursor->curNode = cur->nodeNum;
			cursor->curDup = 0;
			return(cur);
		}
	}
	cursor->curNode = cursor->curDup = 0;
	return(NULL);
}



avl_nod *avlGetPrev(tree,cursor)
	avltree	*tree;
	avl_cur	*cursor;
{
	avl_nod	*cur;
	int	prevNode;

	/*
	** Are we somewhere down a dup chain?
	*/
	if (cursor->curDup != 0)
	{
		cur = avlGetNode(tree,cursor->curDup);
		if (cur->dup)
		{
			cur = avlGetNode(tree,cur->dup);
			cursor->curDup = cur->nodeNum;
			return(cur);
		}
	}
	cur = avlGetNode(tree,cursor->curNode);
	if (cur->dup != 0 && cursor->curDup == 0)
	{
		cursor->curDup = cur->dup;
		cur = avlGetNode(tree,cur->dup);
		return(cur);
	}

	cur = avlGetNode(tree,cursor->curNode);
	if (cur->left)
	{
		cur = avlGetNode(tree,cur->left);
		while(cur->right)
			cur = avlGetNode(tree,cur->right);
		cursor->curNode = cur->nodeNum;
		cursor->curDup = 0;
		return(cur);
	}

	while (cur->parent)
	{
		prevNode = cur->nodeNum;
		cur = avlGetNode(tree,cur->parent);
		if (cur->right == prevNode)
		{
			cursor->curNode = cur->nodeNum;
			cursor->curDup = 0;
			return(cur);
		}
	}
	cursor->curNode = 0;
	cursor->curDup = 0;
	return(NULL);
}


int avlInsert(tree, key, data)
	avltree	*tree;
	char	*key;
	off_t	data;
{
	avl_nod	*cur,
		*new;
	int	res,
		oldHeight;

#ifdef AVL_DEBUG
	printf("Inserting value ");
	avlPrintElement(key,tree->sblk->keyType,tree->sblk->keyLen);
	printf("\n\n");
#endif

	/*
	** Find the insertion point
	*/
	new = getFreeNode(tree);
	if (new == NULL)
	{
		return(IDX_UNKNOWN);
	}
	cur = avlLookup(tree, key, IDX_CLOSEST);
	if (!cur)
	{
		/*
		** Tree must be empty
		*/
		new->left = new->right = new->parent = new->height = 0;
		copyValue(key, new->key, tree);
		new->data = data;
		tree->sblk->root = new->nodeNum;
		return(IDX_OK);
	}

	/*
	** Add the node
	*/
	res = compareValues(key,avlKey(cur),tree);

#ifdef AVL_DEBUG
	switch (res)
	{
		case 0:
			printf("Duplicate value!\n");
			break;
		case 1:
			printf("Inserting to the right\n");
			break;
		case -1:
			printf("Inserting to the left\n");
			break;
	}
#endif
	if (res == 0)
	{
		/*
		** Duplicate Value
		*/
		new->dup = cur->dup;
		cur->dup = new->nodeNum;
		new->height = -1;
		new->data = data;
		copyValue(key, new->key, tree);
		tree->sblk->numEntries++;
		return(IDX_OK);
	}

	/*
	** It's not a dup
	*/
	if (res < 0)
		cur->left = new->nodeNum;
	else
		cur->right = new->nodeNum;
	new->left = new->right = new->height = 0;
	copyValue(key, new->key, tree);
	new->data = data;
	new->parent = cur->nodeNum;
	tree->sblk->numEntries++;
	tree->sblk->numKeys++;

	/*
	** Update height values as required if this is a new layer
	** (i.e. this is the first node placed under our parent)
	*/
	if (cur->left == 0 || cur->right == 0)
	{
		while(cur)
		{
			oldHeight = cur->height;
			setNodeHeight(tree,cur);
			if (cur->height == oldHeight)
				break;
			balanceTree(tree,cur);
			cur = avlGetNode(tree,cur->parent);
		}
	}
	return(IDX_OK);
}




int avlDelete(tree, key, data)
	avltree	*tree;
	char	*key;
	off_t	data;
{
	avl_nod	*cur,
		*parent = NULL,
		*next = NULL,
		*prev,
		*tmp;
	avl_cur	cursor;
	int	done = 0,
		oldHeight;


	/*
	** Find the node that matches both the key and the data
	*/
	cur = avlLookup(tree,key,IDX_EXACT);
	if (!cur)
		return(IDX_NOT_FOUND);
	avlSetCursor(tree,&cursor);

	if (cur->dup)
	{
		if (cur->data == data)
		{
			tmp = avlGetNode(tree,cur->dup);
			cur->dup = tmp->dup;
			cur->data = tmp->data;
			dropNode(tree,tmp);
			tree->sblk->numEntries--;
			return(IDX_OK);
		}
		prev = cur;
		cur = avlGetNode(tree,cur->dup);
		while(cur)
		{
			if (cur->data == data)
			{
				prev->dup = cur->dup;
				tree->sblk->numEntries--;
				dropNode(tree,cur);
				return(IDX_OK);
			}
			prev = cur;
			cur = avlGetNode(tree,cur->dup);
		}
		return(IDX_NOT_FOUND);
	}

	/*
	** OK, so I'm a whimp!  Let's take the ultra easy option on
	** this.  What we want is for this node to be a leaf node with
	** no kids.  Lets just keep swapping it with it's next greater
	** or lesser node until it becomes a leaf.  We want to ensure
	** that the swap direction is down the tree so check our kids.
	*/
	if (cur->right)
	{
		while(cur->left || cur->right)
		{
			next = avlGetNext(tree,&cursor);
			if (!next)
				break;
			swapNodes(tree, cur, next);
			avlSetCursor(tree,&cursor);
		}
		if (!next)
		{
			/*
			** Hmmm, can't get away from the child link.
			** We know that there's only 1 child though so
			** we can just collapse this branch
			*/
			next = avlGetNode(tree,cur->left);
			next->parent = cur->parent;
			parent = avlGetNode(tree,cur->parent);
			if (parent->left == cur->nodeNum)
				parent->left = next->nodeNum;
			else
				parent->right = next->nodeNum;
			dropNode(tree,cur);
			tree->sblk->numEntries--;
			tree->sblk->numKeys--;
			done = 1;
		}
	}
	else if (cur->left)
	{
		while(cur->left || cur->right)
		{
			next = avlGetPrev(tree,&cursor);
			if (!next)
				break;
			swapNodes(tree, cur, next);
			avlSetCursor(tree,&cursor);
		}
		if (!next)
		{
			/*
			** As above
			*/
			next = avlGetNode(tree,cur->right);
			next->parent = cur->parent;
			parent = avlGetNode(tree,cur->parent);
			if (parent->left == cur->nodeNum)
				parent->left = next->nodeNum;
			else
				parent->right = next->nodeNum;
			dropNode(tree,cur);
			tree->sblk->numEntries--;
			tree->sblk->numKeys--;
			done = 1;
		}
	}

	/*
	** By this time it must be a leaf node.
	*/
	if (!done)
	{
		parent = avlGetNode(tree,cur->parent);
		if (!parent)
		{
			/*
			** It's the last node in the tree
			*/
			dropNode(tree,cur);
			tree->sblk->root = 0;
			tree->sblk->numEntries = tree->sblk->numKeys = 0;
			return(IDX_OK);
		}
		if (parent->left == cur->nodeNum)
		{
			parent->left = 0;
			if (parent->right == 0)
			{
				parent->height = 0;
				parent = avlGetNode(tree, parent->parent);
			}
			dropNode(tree,cur);
			tree->sblk->numEntries--;
			tree->sblk->numKeys--;
		}
		else if (parent->right == cur->nodeNum)
		{
			parent->right = 0;
			if (parent->left == 0)
			{
				parent->height = 0;
				parent = avlGetNode(tree, parent->parent);
			}
			dropNode(tree,cur);
			tree->sblk->numEntries--;
			tree->sblk->numKeys--;
		}
	}

	/*
	** Rebalance as required
	*/
	while(parent)
	{
		oldHeight = parent->height;
		setNodeHeight(tree,parent);
		if (parent->height == oldHeight)
			break;
		balanceTree(tree,parent);
		parent = avlGetNode(tree,parent->parent);
	}
	return(IDX_OK);
}



/**************************************************************************
** Debugging Code
*/

static void printNode(tree, cur, depth, vFlag)
	avltree	*tree;
	avl_nod	*cur;
	int	depth,
		vFlag;
{
	if (vFlag)
	{
		showIndent(depth);
		printf("N=%d, P=%d, L=%d, R=%d H=%d D=%d\n",cur->nodeNum, 
			cur->parent, cur->left, cur->right, cur->height,
			cur->dup);
		showIndent(depth);
	}
	avlPrintElement(avlKey(cur), tree->sblk->keyType,tree->sblk->keyLen);
	printf("data = %d\n", (int)cur->data);
}



static void dumpSubTree(tree, node, depth, vFlag)
	avltree	*tree;
	int	node,
		depth,
		vFlag;
{
	avl_nod	*cur;

	if (node == 0)
		return;

	cur = avlGetNode(tree, node);
	dumpSubTree(tree, cur->left, depth+1, vFlag);
	printNode(tree, cur, depth, vFlag);
	dumpSubTree(tree, cur->right, depth+1, vFlag);
}



void avlDumpTree(tree, verbose)
	avltree	*tree;
	int	verbose;
{
	dumpSubTree(tree,tree->sblk->root, 0,verbose);
}



int avlTestTree(tree)
	avltree	*tree;
{
	avl_nod	*cur,
		*prev = NULL;
	avl_cur	cursor;
	int	count,
		prev1 = 0,
		prev2 = 0,
		prev3 = 0,
		prev4 = 0;

	cur = avlGetFirst(tree);
	if (!cur)
		return(0);
	count = 1;
	avlSetCursor(tree,&cursor);
	prev = cur;
	cur = avlGetNext(tree,&cursor);
	while(cur)
	{
		if (cur->nodeNum == prev1 || cur->nodeNum == prev2 ||
			cur->nodeNum == prev3 || cur->nodeNum == prev4)
		{
			abort();
		}
		count++;
		if (compareValues(prev->key, cur->key, tree) > 0)
		{
			return(-1);
		}
		prev = cur;
		prev4 = prev3;
		prev3 = prev2;
		prev2 = prev1;
		prev1 = cur->nodeNum;
		cur = avlGetNext(tree,&cursor);
	}
	return(count);
}
