#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <common/debug/debug.h>
#include <common/config.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include "avl_tree.h"
#include "cpi.h"
#include "index.h"


static	avltree	*avlMemTree = NULL;


/*********************************************************************
** IDX Value Comparison Routines
*/

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


int idxInt8Compare(v1,v2)
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



int idxUInt8Compare(v1,v2)
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


int idxInt16Compare(v1,v2)
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



int idxUInt16Compare(v1,v2)
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


int idxInt32Compare(v1,v2)
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



int idxUInt32Compare(v1,v2)
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


int idxInt64Compare(v1,v2)
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



int idxUInt64Compare(v1,v2)
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


int idxRealCompare(v1,v2)
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



int idxCharCompare(v1,v2)
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


int idxByteCompare(v1,v2,len)
	char	*v1,
		*v2;
	int	len;
{
	u_char	*cp1,
		*cp2;

	cp1 = (u_char *)v1;
	cp2 = (u_char *)v2;
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


int idxCompareValues(dataType, v1, v2, length)
	int 	dataType;
	char	*v1, *v2;
	int	length;
{
        switch(dataType)
        {       
                case IDX_INT8:
                        return(idxInt8Compare(v1,v2));
                case IDX_UINT8:
                        return(idxUInt8Compare(v1,v2));
                case IDX_INT16:
                        return(idxInt16Compare(v1,v2));
                case IDX_UINT16:
                        return(idxUInt16Compare(v1,v2));
                case IDX_INT32:
                        return(idxInt32Compare(v1,v2));
                case IDX_UINT32:
                        return(idxUInt32Compare(v1,v2));
                case IDX_INT64:
                        return(idxInt64Compare(v1,v2));
                case IDX_UINT64:
                        return(idxUInt64Compare(v1,v2));
                case IDX_REAL:
                        return(idxRealCompare(v1,v2));
                case IDX_CHAR:
                        return(idxCharCompare(v1,v2));
                default:
			return(idxByteCompare(v1,v2, length));
        }
}



/*********************************************************************
** IDX Interface Routines
*/

int idxCreate(path, idxType, mode, length, dataType, flag, env)
	char	*path;
	int	idxType,
		mode,
		length,
		dataType,
		flag;
	idx_env	*env;
{
	cpi_env	envBuf;
	int	res;

	switch(idxType)
	{
		case IDX_AVL:
			if (avlCreate(path, mode, length, dataType, flag) < 0)
				return(IDX_FILE_ERR);
			else
				return(IDX_OK);

		case IDX_CPI:
			if (env)
			{
				envBuf.pageSize = env->pageSize;
				envBuf.cacheSize = env->cacheSize;
				res = cpiCreate(path, mode, length, dataType, 
					flag, &envBuf);
			}
			else
			{
				res = cpiCreate(path, mode, length, dataType, 
					flag, NULL);
			}
			if (res < 0)
				return(IDX_FILE_ERR);
			else
				return(IDX_OK);
		
		case IDX_MEM_AVL:
			if (avlMemTree)
				avlClose(avlMemTree);
			avlMemTree = avlMemCreate(length, dataType, flag);
			if (!avlMemTree)
				return(IDX_FILE_ERR);
			else
				return(IDX_OK);

		default:
			return(IDX_BAD_TYPE);
	}
}


int idxClose(idx)
	idx_hnd	*idx;
{
	switch(idx->idxType)
	{
		case IDX_AVL:
		case IDX_MEM_AVL:
        		avlClose((avltree *)idx->native);
			return(IDX_OK);

		case IDX_CPI:
        		cpiClose((cpi *)idx->native);
			return(IDX_OK);

		default:
			return(IDX_BAD_TYPE);
	}
}


int idxSync(idx)
	idx_hnd	*idx;
{
	switch(idx->idxType)
	{
		case IDX_AVL:
        		avlSync((avltree *)idx->native);
			return(IDX_OK);

		case IDX_CPI:
		case IDX_MEM_AVL:
			return(IDX_OK);

		default:
			return(IDX_BAD_TYPE);
	}
}


int idxOpen(path, idxType, env, idx)
	char	*path;
	int	idxType;
	idx_env	*env;
	idx_hnd	*idx;
{
	cpi_env	envBuf;

	if (path)
	{
		strcpy(idx->path,path);
	}
	else
	{
		*idx->path = 0;
	}
	switch(idxType)
	{
		case IDX_AVL:
			idx->idxType = IDX_AVL;
			idx->native = (void *)avlOpen(path);
			if (idx->native == NULL)
				return(IDX_FILE_ERR);
			idx->dataType = ((avltree*)idx->native)->sblk->keyType;
			return(IDX_OK);
			
		case IDX_CPI:
			idx->idxType = IDX_CPI;
			if (env)
			{
				envBuf.pageSize = env->pageSize;
				envBuf.cacheSize = env->cacheSize;
				idx->native = (void *)cpiOpen(path, &envBuf);
			}
			else
			{
				idx->native = (void *)cpiOpen(path, NULL);
			}
			if (idx->native == NULL)
				return(IDX_FILE_ERR);
			else
				return(IDX_OK);
			
		case IDX_MEM_AVL:
			idx->idxType = IDX_MEM_AVL;
			idx->native = (void *)avlMemTree;
			avlMemTree = NULL;
			if (idx->native == NULL)
				return(IDX_FILE_ERR);
			else
				return(IDX_OK);
			
		default:
			return(IDX_BAD_TYPE);
	}
}


int idxInsert(idx, buf, len, pos)
	idx_hnd *idx;
	char	*buf;
	int	len;
	off_t	pos;
{
	int	res;

	switch(idx->idxType)
	{
		case IDX_AVL:
		case IDX_MEM_AVL:
			res = avlInsert((avltree *)idx->native,buf,pos);
			if (res == IDX_OK)
				return(IDX_OK);
			else
				return(IDX_UNKNOWN);
		
		case IDX_CPI:
			res = cpiInsert((cpi *)idx->native, buf, pos);
			if (res == 0)
				return(IDX_OK);
			else
				return(IDX_UNKNOWN);
			
		default:
			return(IDX_BAD_TYPE);
	}
}


int idxDelete(idx, buf, len, pos)
	idx_hnd *idx;
	char	*buf;
	int	len;
	off_t	pos;
{
	int	res;

	switch(idx->idxType)
	{
		case IDX_AVL:
		case IDX_MEM_AVL:
			res = avlDelete((avltree *)idx->native, buf, pos);
			if (res == IDX_NOT_FOUND)
				return(IDX_NOT_FOUND);
			if (res == IDX_OK)
				return(IDX_OK);
			return(IDX_UNKNOWN);

		case IDX_CPI:
			res = cpiDelete((cpi *)idx->native, buf, pos);
			if (res < 0)
				return(IDX_NOT_FOUND);
			return(IDX_OK);
			
		default:
			return(IDX_BAD_TYPE);
	}
}



int idxLookup(idx, buf, len, flag, node)
	idx_hnd *idx;
	char	*buf;
	int	len;
	int	flag;
	idx_nod	*node;
{
	avl_nod	*avlNode;
	int	res;

	switch(idx->idxType)
	{
		case IDX_AVL:
		case IDX_MEM_AVL:
			avlNode = avlLookup((avltree*)idx->native,buf,flag);
			if (avlNode == NULL)
			{
				node->key = NULL;
				node->native = NULL;
				return(IDX_NOT_FOUND);
			}
			node->native = (void *)avlNode;
			node->key = avlKey(avlNode);
			node->data = avlNode->data;
			return(IDX_OK);

		case IDX_CPI:
			res = cpiLookup((cpi*)idx->native, buf, 
				&(node->cpiNode), flag);
			if (res < 0)
				return(IDX_NOT_FOUND);
			node->key = node->cpiNode.key;
			node->data = node->cpiNode.data;
			return(IDX_OK);
		
		default:
			return(IDX_BAD_TYPE);
	}



}



int idxExists(idx, buf, len, pos)
	idx_hnd *idx;
	char	*buf;
	int	len;
	off_t	pos;
{
	int	res;

	switch(idx->idxType)
	{
		case IDX_AVL:
		case IDX_MEM_AVL:
			return(IDX_OK);

		case IDX_CPI:
			res = cpiExists((cpi*)idx->native, buf, pos);
			if (res < 0)
				return(IDX_NOT_FOUND);
			return(IDX_OK);
		
		default:
			return(IDX_BAD_TYPE);
	}

}

int idxGetFirst(idx, node)
	idx_hnd *idx;
	idx_nod	*node;
{
	avl_nod	*avlNode;
	int	res;

	switch(idx->idxType)
	{
		case IDX_AVL:
		case IDX_MEM_AVL:
			avlNode = avlGetFirst((avltree *)idx->native);
			if (avlNode == NULL)
			{
				node->key = NULL;
				node->native = NULL;
				return(IDX_NOT_FOUND);
			}
			node->native = (void *)avlNode;
			node->key = avlKey(avlNode);
			node->data = avlNode->data;
			return(IDX_OK);

		case IDX_CPI:
			res = cpiGetFirst((cpi*)idx->native,&(node->cpiNode));
			if (res < 0)
				return(IDX_NOT_FOUND);
			node->key = node->cpiNode.key;
			node->data = node->cpiNode.data;
			return(IDX_OK);
		
		default:
			return(IDX_BAD_TYPE);
	}

}


int idxGetLast(idx, node)
	idx_hnd *idx;
	idx_nod	*node;
{
	avl_nod	*avlNode;
	int	res;

	switch(idx->idxType)
	{
		case IDX_AVL:
		case IDX_MEM_AVL:
			avlNode = avlGetLast((avltree *)idx->native);
			if (avlNode == NULL)
			{
				node->key = NULL;
				node->native = NULL;
				return(IDX_NOT_FOUND);
			}
			node->native = (void *)avlNode;
			node->key = avlKey(avlNode);
			node->data = avlNode->data;
			return(IDX_OK);

		case IDX_CPI:
			res = cpiGetLast((cpi*)idx->native,&(node->cpiNode));
			if (res < 0)
				return(IDX_NOT_FOUND);
			node->key = node->cpiNode.key;
			node->data = node->cpiNode.data;
			return(IDX_OK);
		
		default:
			return(IDX_BAD_TYPE);
	}

}



int idxSetCursor(idx, cursor)
	idx_hnd *idx;
	idx_cur	*cursor;
{
	switch(idx->idxType)
	{
		case IDX_AVL:
		case IDX_MEM_AVL:
			avlSetCursor((avltree*)idx->native,&(cursor->avlCur));
			return(IDX_OK);
			
		case IDX_CPI:
			cpiSetCursor((cpi*)idx->native, &(cursor->cpiCur));
			return(IDX_OK);

		default:
			return(IDX_BAD_TYPE);
	}
}




int idxCloseCursor(idx, cursor)
	idx_hnd *idx;
	idx_cur	*cursor;
{
	switch(idx->idxType)
	{
		case IDX_AVL:
		case IDX_MEM_AVL:
			return(IDX_OK);

		case IDX_CPI:
			return(IDX_OK);
			
		default:
			return(IDX_BAD_TYPE);
	}
}



int idxGetNext(idx, cursor, node)
	idx_hnd *idx;
	idx_cur	*cursor;
	idx_nod	*node;
{
	avl_nod	*avlNode;

	switch(idx->idxType)
	{
		case IDX_AVL:
		case IDX_MEM_AVL:
			avlNode = avlGetNext((avltree*)idx->native,
				&(cursor->avlCur));
			if (avlNode == NULL)
			{
				node->key = NULL;
				node->native = NULL;
				return(IDX_NOT_FOUND);
			}
			node->native = (void *)avlNode;
			node->key = avlKey(avlNode);
			node->data = avlNode->data;
			return(IDX_OK);
			
		case IDX_CPI:
			if(cpiGetNext((cpi*)idx->native, &(cursor->cpiCur),
				&(node->cpiNode)) < 0)
			{
				return(IDX_NOT_FOUND);
			}
			node->key = node->cpiNode.key;
			node->data = node->cpiNode.data;
			return(IDX_OK);
			
		default:
			return(IDX_BAD_TYPE);
	}
}


int idxGetPrev(idx, cursor, node)
	idx_hnd *idx;
	idx_cur	*cursor;
	idx_nod	*node;
{
	avl_nod	*avlNode;

	switch(idx->idxType)
	{
		case IDX_AVL:
		case IDX_MEM_AVL:
			avlNode = avlGetPrev((avltree*)idx->native,
				&(cursor->avlCur));
			if (avlNode == NULL)
			{
				node->key = NULL;
				node->native = NULL;
				return(IDX_NOT_FOUND);
			}
			node->native = (void *)avlNode;
			node->key = avlKey(avlNode);
			node->data = avlNode->data;
			return(IDX_OK);
			
		case IDX_CPI:
			if(cpiGetPrev((cpi*)idx->native, &(cursor->cpiCur),
				&(node->cpiNode)) < 0)
			{
				return(IDX_NOT_FOUND);
			}
			node->key = node->cpiNode.key;
			node->data = node->cpiNode.data;
			return(IDX_OK);
			
		default:
			return(IDX_BAD_TYPE);
	}
}


void idxPrintIndexStats(idx)
	idx_hnd	*idx;
{
	switch(idx->idxType)
	{
		case IDX_CPI:
			cpiPrintIndexStats((cpi*)idx->native);
			break;
	}
	return;
}



void idxDumpIndex(idx)
	idx_hnd	*idx;
{
	switch(idx->idxType)
	{
		case IDX_CPI:
			cpiDumpIndex((cpi*)idx->native);
			break;
	}
	return;
}


int idxTestIndex(idx)
	idx_hnd	*idx;
{
	switch(idx->idxType)
	{
		case IDX_CPI:
			return(cpiTestIndex((cpi*)idx->native));
			break;
		case IDX_AVL:
		case IDX_MEM_AVL:
			return(avlTestTree((avltree*)idx->native));
			break;
	}
	return(0);
}

char *idxGetIndexType(idx)
	idx_hnd	*idx;
{
	static	char	indexType[80];
	avltree	*avl;


	switch(idx->idxType)
	{
		case IDX_CPI:
			strcpy(indexType,"cpi");
			return(indexType);
		case IDX_AVL:
        		avl = (avltree *)idx->native;
			/*sprintf(indexType,"avl v%d", avl->version);*/
			return(indexType);
		default:
			strcpy(indexType,"???");
			return(indexType);
	}
}

unsigned int idxGetNumEntries(idx)
	idx_hnd	*idx;
{
	avltree	*avl;

	switch(idx->idxType)
	{
		case IDX_AVL:
        		avl = (avltree *)idx->native;
			if (avl)
			{
				return(avl->sblk->numEntries);
			}
			else
			{
				return(0);
			}

		default:
			return(0);
	}
}

unsigned int idxGetNumKeys(idx)
	idx_hnd	*idx;
{
	avltree	*avl;

	switch(idx->idxType)
	{
		case IDX_AVL:
        		avl = (avltree *)idx->native;
			if (avl)
			{
				return(avl->sblk->numKeys);
			}
			else
			{
				return(0);
			}

		default:
			return(0);
	}
}
