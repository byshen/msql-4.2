/*
** Copyright (c) 1995-2001  Hughes Technologies Pty Ltd.  All rights
** reserved.  
**
** Terms under which this software may be used or copied are
** provided in the  specific license associated with this product.
**
** Hughes Technologies disclaims all warranties with regard to this 
** software, including all implied warranties of merchantability and 
** fitness, in no event shall Hughes Technologies be liable for any 
** special, indirect or consequential damages or any damages whatsoever 
** resulting from loss of use, data or profits, whether in an action of 
** contract, negligence or other tortious action, arising out of or in 
** connection with the use or performance of this software.
**
**
** $Id: acl.c,v 1.4 2002/10/15 00:03:28 bambi Exp $
**
*/

/*
** Module	: main : acl
** Purpose	: 
** Exports	: 
** Depends Upon	: 
*/



/**************************************************************************
** STANDARD INCLUDES
**************************************************************************/

#include <common/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#  include <string.h>
#endif
#ifdef HAVE_STRINGS_H
#  include <strings.h>
#endif

#include <common/portability.h>

/**************************************************************************
** MODULE SPECIFIC INCLUDES
**************************************************************************/

#include <ctype.h>

#include <common/msql_defs.h>
#include <common/debug/debug.h>
#include <common/config/config.h>
#include <msqld/index/index.h>
#include <msqld/includes/msqld.h>
#include <msqld/main/acl.h>


/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/

static acl_t	*aclHead = NULL;
static int	accessPerms;

extern  char    *packet;

/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/

static int checkToken(tok)
	char	*tok;
{
	if (strcmp(tok,"database") == 0)
		return(DATABASE);
	if (strcmp(tok,"read") == 0)
		return(READ);
	if (strcmp(tok,"write") == 0)
		return(WRITE);
	if (strcmp(tok,"host") == 0)
		return(HOST);
	if (strcmp(tok,"access") == 0)
		return(ACCESS);
	if (strcmp(tok,"option") == 0)
		return(OPTION);
	return(-1);
}



static void freeTlist(head)
	mTable_t	*head;
{
	mTable_t	*curText,
			*prevText;

	curText = head;
	while(curText)
	{
		prevText = curText;
		curText = curText->next;
		(void)free(prevText);
	}
}


static void freeAcc(head)
	acc_t	*head;
{
	register acc_t	*curAcc,
			*prevAcc;

	curAcc = head;
	while(curAcc)
	{
		prevAcc = curAcc;
		curAcc = curAcc->next;
		(void)free(prevAcc);
	}
}


static void freeAcls()
{
	register acl_t	*curAcl,
			*prevAcl;

	curAcl = aclHead;
	while(curAcl)
	{
		freeAcc(curAcl->host);
		freeAcc(curAcl->read);
		freeAcc(curAcl->write);
		freeTlist(curAcl->access);
		freeTlist(curAcl->option);
		prevAcl = curAcl;
		curAcl = curAcl->next;
		(void)free(prevAcl);
	}
	aclHead = NULL;
}



static int matchToken(pattern,tok)
	char	*pattern,
		*tok;
{
	register char 	*cp;
	char	*buf1,
		*buf2,
		*cp2;
	int	length1,
		length2;

	/*
	** Put everything to lower case
	*/
	buf1 = (char *)strdup(pattern);
	buf2 = (char *)strdup(tok);

	cp = buf1;
	while(*cp)
	{
		*cp = tolower(*cp);
		cp++;
	}

	cp = buf2;
	while(*cp)
	{
		*cp = tolower(*cp);
		cp++;
	}
	
	/*
	** Is this a wildcard?
	*/
	cp = pattern;
	if (*cp == '*')
	{
		if (*(cp+1) == 0)	/* match anything */
		{
			(void)free(buf1);
			(void)free(buf2);
			return(1);
		}
		length1 = strlen(cp)-1;
		length2 = strlen(tok);
		if (length1 > length2)
		{
			(void)free(buf1);
			(void)free(buf2);
			return(0);
		}
		cp2 = buf2 + length2 - length1;
		if (strcmp(cp+1,cp2) == 0)
		{
			(void)free(buf1);
			(void)free(buf2);
			return(1);
		}
		else
		{
			(void)free(buf1);
			(void)free(buf2);
			return(0);
		}
	}

	/*
	** OK, does the actual text match
	*/
	if (strcmp(buf1,buf2) == 0)
	{
		(void)free(buf1);
		(void)free(buf2);
		return(1);
	}
	else
	{
		(void)free(buf1);
		(void)free(buf2);
		return(0);
	}
}



static int matchTextList(list,tok)
	mTable_t	*list;
	char		*tok;
{
	mTable_t	*cur;

	cur = list;
	while(cur)
	{
		if (matchToken(cur->name, tok))
		{
			return(1);
		}
		cur = cur->next;
	}
	return(0);
}



static int matchAccessList(list,tok)
	acc_t	*list;
	char	*tok;
{
	acc_t	*cur;

	cur = list;
	while(cur)
	{
		if (matchToken(cur->name, tok))
		{
			if (cur->access == ALLOW)
			{
				return(1);
			}
			else
			{
				return(0);
			}
		}
		cur = cur->next;
	}
	return(0);
}


/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/

int aclLoadFile(verbose)
	int	verbose;
{
	acl_t	*new = NULL,
		*aclTail = NULL;
	mTable_t	*tNew,
		*tTail;
	acc_t	*accNew,
		*accTail;
	char	buf[1024],
		path[MSQL_PATH_LEN],
		*tok;
	FILE	*fp;
	int	newEntry,
		lineNum;


	/*
	** Open the acl file
	*/
	(void)snprintf(path,MSQL_PATH_LEN,"%s/msql.acl", 
		(char *)configGetCharEntry("general", "inst_dir"));
	fp = fopen(path,"r");
	if (!fp)
	{
		if (verbose)
		{
			printf("\n\tWarning : No ACL file.  " \
				"Using global read/write access.\n");
		}
		snprintf(packet,PKT_LEN, 
			"-1:Warning - Couldn't open ACL file\n");
		return(-1);
	}


	/*
	** Process the file
	*/
	setbuf(fp, NULL);
	fgets(buf,sizeof(buf),fp);
	newEntry = 1;
	lineNum = 1;
	while(!feof(fp))
	{
		tok = (char *)strtok(buf," \t\n\r=,");
		if (!tok)
		{
			/* Blank line ends a db entry */
			newEntry = 1;
			fgets(buf,sizeof(buf),fp);
			lineNum++;
			continue;
		}
			

		if (*tok == '#')
		{
			/* Comments are skipped */
			fgets(buf,sizeof(buf),fp);
			lineNum++;
			continue;
		}



		switch(checkToken(tok))
		{
		    case DATABASE:
			if (!newEntry)
			{
				ERR(("Bad entry header location at line %d\n",
					lineNum));
				snprintf(packet,PKT_LEN,
				   "-1:Bad entry header location at line %d\n"
					,lineNum);
				fclose(fp);
				return(-1);
			}
			newEntry = 0;
			tok = (char *)strtok(NULL," \t\n\r");
			if (!tok)
			{
				ERR(("Missing database name at line %d\n",
					lineNum));
				snprintf(packet, PKT_LEN,
					"-1:Missing database name at line %d\n",
					lineNum);
				fclose(fp);
				return(-1);
			}
			new = (acl_t *)malloc(sizeof(acl_t));
			bzero(new,sizeof(acl_t));
			(void)strcpy(new->db,tok);
			if (aclHead)
			{
				aclTail->next = new;
				aclTail = new;
			}
			else
			{
				aclHead = aclTail = new;
			}
			break;


		    case READ:
			if (newEntry)
			{
				ERR(("Bad entry header location at line %d\n",
					lineNum));
				snprintf(packet, PKT_LEN,
				    "-1:Bad entry header location at line %d\n"
					,lineNum);
				fclose(fp);
				return(-1);
			}
			if (new->read)
			{
				accTail = new->read;
				while(accTail->next)
					accTail = accTail->next;
			}
			else
			{
				accTail = NULL;
			}
			tok = (char *)strtok(NULL," \t,\n\r");
			while(tok)
			{
				accNew = (acc_t *)malloc(sizeof(acc_t));
				bzero(accNew,sizeof(acc_t));
				if (*tok == '-')
				{
					strcpy(accNew->name,tok+1);
					accNew->access = REJECT;
				}
				else
				{
					strcpy(accNew->name,tok);
					accNew->access = ALLOW;
				}
				if (accTail)
				{
					accTail->next = accNew;
				}
				else
				{
					new->read = accNew;
				}
				accTail = accNew;
				tok = (char *)strtok(NULL," \t,\n\r");
			}
			break;


		    case WRITE:
			if (newEntry)
			{
				ERR(("Bad entry header location at line %d\n",
					lineNum));
				snprintf(packet,PKT_LEN,
				    "-1:Bad entry header location at line %d\n"
					,lineNum);
				fclose(fp);
				return(-1);
			}
			if (new->write)
			{
				accTail = new->write;
				while(accTail->next)
					accTail = accTail->next;
			}
			else
			{
				accTail = NULL;
			}
			tok = (char *)strtok(NULL," \t,\n\r");
			while(tok)
			{
				accNew = (acc_t *)malloc(sizeof(acc_t));
				bzero(accNew,sizeof(acc_t));
				if (*tok == '-')
				{
					strcpy(accNew->name,tok+1);
					accNew->access = REJECT;
				}
				else
				{
					strcpy(accNew->name,tok);
					accNew->access = ALLOW;
				}
				if (accTail)
				{
					accTail->next = accNew;
				}
				else
				{
					new->write = accNew;
				}
				accTail = accNew;
				tok = (char *)strtok(NULL," \t,\n\r");
			}
			break;


		    case HOST:
			if (newEntry)
			{
				ERR(("Bad entry header location at line %d\n",
					lineNum));
				snprintf(packet,PKT_LEN,
				    "-1:Bad entry header location at line %d\n"
					,lineNum);
				fclose(fp);
				return(-1);
			}
			if (new->host)
			{
				accTail = new->host;
				while(accTail->next)
					accTail = accTail->next;
			}
			else
			{
				accTail = NULL;
			}
			tok = (char *)strtok(NULL," \t,\n\r");
			while(tok)
			{
				accNew = (acc_t *)malloc(sizeof(acc_t));
				bzero(accNew,sizeof(acc_t));
				if (*tok == '-')
				{
					strcpy(accNew->name,tok+1);
					accNew->access = REJECT;
				}
				else
				{
					strcpy(accNew->name,tok);
					accNew->access = ALLOW;
				}
				if (accTail)
				{
					accTail->next = accNew;
				}
				else
				{
					new->host = accNew;
				}
				accTail = accNew;
				tok = (char *)strtok(NULL," \t,\n\r");
			}
			break;


		    case ACCESS:
			if (newEntry)
			{
				ERR(("Bad entry header location at line %d\n",
					lineNum));
				snprintf(packet,PKT_LEN,
				    "-1:Bad entry header location at line %d\n"
					,lineNum);
				fclose(fp);
				return(-1);
			}
			if (new->access)
			{
				tTail = new->access;
				while(tTail->next)
					tTail = tTail->next;
			}
			else
			{
				tTail = NULL;
			}
			tok = (char *)strtok(NULL," \t,\n\r");
			while(tok)
			{
				tNew = (mTable_t *)malloc(sizeof(mTable_t));
				bzero(tNew,sizeof(mTable_t));
				strcpy(tNew->name,tok);
				if (tTail)
				{
					tTail->next = tNew;
				}
				else
				{
					new->access = tNew;
				}
				tTail = tNew;
				tok = (char *)strtok(NULL," \t,\n\r");
			}
			break;


		    case OPTION:
			if (newEntry)
			{
				ERR(("Bad entry header location at line %d\n",
					lineNum));
				snprintf(packet,PKT_LEN,
				    "-1:Bad entry header location at line %d\n"
					,lineNum);
				fclose(fp);
				return(-1);
			}
			if (new->option)
			{
				tTail = new->option;
				while(tTail->next)
					tTail = tTail->next;
			}
			else
			{
				tTail = NULL;
			}
			tok = (char *)strtok(NULL," \t,\n\r");
			while(tok)
			{
				tNew = (mTable_t *)malloc(sizeof(mTable_t));
				bzero(tNew,sizeof(mTable_t));
				strcpy(tNew->name,tok);
				if (tTail)
				{
					tTail->next = tNew;
				}
				else
				{
					new->option = tNew;
				}
				tTail = tNew;
				tok = (char *)strtok(NULL," \t,\n\r");
			}
			break;


		    default:
			ERR(("Unknown ACL command \"%s\" at line %d\n", 
				tok,lineNum));
			snprintf(packet,PKT_LEN,
				"-1:Unknown ACL command \"%s\" at line %d\n", 
				tok,lineNum);
			fclose(fp);
			return(-1);
			break;

		}

		fgets(buf,sizeof(buf),fp);
		lineNum++;
	}
	fclose(fp);
	return(0);
}




int aclCheckAccess(db,info)
	char		*db;
	cinfo_t		*info;
{
	char 		*host,
			*user;
	acl_t		*curAcl;
	int		perms;

	host = info->host;
	user = info->user;
	
	/*
	** Find an ACL entry that matches this DB
	*/
	curAcl = aclHead;
	while(curAcl)
	{
		if(matchToken(curAcl->db,db))
		{
			break;
		}
		curAcl = curAcl->next;
	}

	if (!curAcl)
	{
		return(RW_ACCESS);	/* default if no specific ACL */
	}

	/*
	** Now check the connection details
	*/
	if (!host)
	{
		/* No host == local connection */
		
		if (!matchTextList(curAcl->access,"local"))
		{
			return(NO_ACCESS);
		}
	}
	else
	{
		if (!matchTextList(curAcl->access,"remote"))
		{
			return(NO_ACCESS);
		}
		if (!matchAccessList(curAcl->host,host))
		{
			return(NO_ACCESS);
		}
	}

	/*
	** Now check the access perms
	*/
	perms = 0;
	if (matchAccessList(curAcl->read, user))
	{
		perms |= READ_ACCESS;
	}
	if (matchAccessList(curAcl->write, user))
	{
		perms |= WRITE_ACCESS;
	}
	if (perms == 0)
	{
		return(NO_ACCESS);
	}


	/*
	** Now perform any options for this connection
	*/

	
	return(perms);
}




void aclReloadFile(sock)
	int	sock;
{
	freeAcls();
	aclLoadFile(0);
}


void aclSetPerms(perms)
	int	perms;
{
	accessPerms = perms;
}

int aclCheckPerms(access)
	int	access;
{
	return( (accessPerms & access) == access);
}



int aclCheckLocal(info)
	cinfo_t	*info;
{
	char 	*host,
		*user;

	host = info->host;
	user = info->user;

	if ( (!info->host || strcmp(host,"localhost") == 0))
	{
		if (strcmp(info->user,
			(char *)configGetCharEntry("general","admin_user"))!=0)
		{
			return(0);
		}
	}
	else
	{
		return(0);
	}
	return(1);
}
