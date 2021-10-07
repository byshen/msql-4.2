/*
** Copyright (c) 2000-2001  Hughes Technologies Pty Ltd.  All rights
** reserved.  Terms under which this software may be used or copied are
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
** $Id: config.c,v 1.14 2012/01/17 02:26:22 bambi Exp $
**
*/

/*
** Module	: config
** Purpose	: Interface to the system configuration files
** Exports	: configLoadFile()
**		  configGetIntEntry()
**		  configGetCharEntry()
**		  configGetError()
** Depends Upon	: 
*/



#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>

#include "common/portability.h"

#include "config_priv.h"
#include "config.h"

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif


/**************************************************************************
** GLOBAL VARIABLES
**************************************************************************/

static	int curLine = 0,	/* used for parse error reporting */
	configLoaded = 0;	/* don't double load config files */

static	char errorMessage[1024];

/*
** Configuration table.  Defines what section/entry pairings are valid
** and what the default values are
**
**-------------------------------------------------------------------------
** 	section          	entry            type
**	char def         	int def   	  NULL 
*/

static config_entry conf_table [] = {
 {	"general",     		"msql_user",    CONFIG_CHAR_TYPE,
	"daemon",		0, 		0
 },
 {	"general",     		"admin_user",   CONFIG_CHAR_TYPE,
	"root",			0, 		0
 },
 {	"general",     		"pid_file",     CONFIG_CHAR_TYPE,
	"%I/msqld.pid", 	0, 		0
 },
 {	"general",     		"inst_dir",     CONFIG_CHAR_TYPE,
	INST_DIR,       	0, 		0
 },
 {	"general",     		"db_dir",       CONFIG_CHAR_TYPE,
	"%I/msqldb",    	0, 		0
 },
 {	"general",     		"tcp_port",     CONFIG_INT_TYPE,
	NULL,           	1114, 		0
 },
 {	"general",     		"unix_port",	CONFIG_CHAR_TYPE,
	"%I/msqld.sock",    	0, 		0
 },
 {	"system",      		"msync_timer",  CONFIG_INT_TYPE,
	NULL,           	120, 		0
 },
 {	"system",      		"sync_updates", CONFIG_INT_TYPE,
	NULL,           	0, 		0
 },
 {	"system",      		"force_munmap", CONFIG_INT_TYPE,
	NULL,           	0, 		0
 },
 {	"system",      		"host_lookup",  CONFIG_INT_TYPE,
	NULL,           	1, 		0
 },
 {	"system",      		"read_only",    CONFIG_INT_TYPE,
	NULL,           	0, 		0
 },
 {	"system",      		"remote_access",CONFIG_INT_TYPE,
	NULL,           	0, 		0
 },
 {	"system",      		"local_access", CONFIG_INT_TYPE,
	NULL,           	1, 		0
 },
 {	"system",      		"query_log",    CONFIG_INT_TYPE,
	NULL,           	0, 		0
 },
 {	"system",      		"query_log_file", CONFIG_CHAR_TYPE,
	"%I/query.log",		0, 		0
 },
 {	"system",      		"update_log",    CONFIG_INT_TYPE,
	NULL,           	0, 		0
 },
 {	"system",      		"update_log_file", CONFIG_CHAR_TYPE,
	"%I/update.log",	0, 		0
 },
 {	"system",      		"num_children", CONFIG_INT_TYPE,
	NULL,           	2, 		0
 },
 {	"system",      		"table_cache", 	CONFIG_INT_TYPE,
	NULL,           	16, 		0
 },
 {	"system",      		"sort_max_mem",CONFIG_INT_TYPE,
	NULL,           	1000, 		0
 },
 {	"system",      		"system_has_swap",CONFIG_INT_TYPE,
	NULL,           	1, 		0
 },
 {	"system",      		"export_delimiter", CONFIG_CHAR_TYPE,
	",",			0, 		0
 },
 { 	NULL,         		NULL,		0,              
	NULL,			0, 		0
 }
};



/**************************************************************************
** PRIVATE ROUTINES
**************************************************************************/


/*
** Private
** expandConf		- Expand macros in a config entry
*/
static void expandConf(str, buf, bufLen)
	char	*str,
		*buf;
	int	bufLen;
{
	char	*cp,*cp2, *val;

	bzero(buf, bufLen);
	if (!str)
		return;
	cp = str;
	cp2 = buf;
	while(*cp)
	{
		if (*cp == '%')
		{
			switch(*(cp+1))
			{
				case 'I':
					val = configGetCharEntry( "general", 
						"inst_dir");
					strcpy(cp2, val);
					cp2 += strlen(val);
					cp+=2;
					break;

				default:
					*cp2++ = *cp++;
					*cp2++ = *cp++;
					break;
			}
			continue;
		}
		*cp2++ = *cp++;
	}
}




/*
** Private
** checkIntVal		- Make sure integer values are exactly that
*/
static int checkIntVal(str)
	char	*str;
{
	char 	*cp = str;

	while(*cp)
	{
		if (!isdigit((int)*cp))
			return(-1);
		cp++;
	}
	return(0);
}



/*
** Private
** setConfigEntry	- Update the internal entray value
*/
static int setConfigEntry(handle,section,value)
	char	*handle,
		*section,
		*value;
{
	config_entry	*cur;
	char	*cp;

	if (!value)
	{
		sprintf(errorMessage,"Invalid value at line %d", curLine);
		return(CONFIG_ERR_VALUE);
	}
	cur = conf_table;
	while(cur->handle)
	{
		/*
		** Is this the correct entry?
		*/
		if (strcmp(cur->handle,handle) != 0 ||
		    strcmp(cur->section,section) != 0)
		{
			cur++;
			continue;
		}

		/*
		** Set the entry value
		*/
		switch(cur->type)
		{
		case CONFIG_CHAR_TYPE:
			if (strcmp(value,"NULL") == 0)
				cur->charVal = NULL;
			else
				cur->charVal = (char *)strdup(value);
			return(CONFIG_OK);
			break;

		case CONFIG_INT_TYPE:
			if (checkIntVal(value) == 0)
			{
				cur->intVal = atoi(value);
			}
			else
			{
				int	goodValue = 0;

				cp = value;
				while(*cp)
				{
					*cp = tolower(*cp);
					cp++;
				}
				if(strcmp(value,"true") == 0)
				{
					cur->intVal = 1;
					goodValue = 1;
				}
				if(strcmp(value,"false") == 0)
				{
					cur->intVal = 0;
					goodValue = 1;
				}
				if (goodValue == 0)
				{
					sprintf(errorMessage,
						"Invalid value at line %d",
						curLine);
					return(CONFIG_ERR_VALUE);
				}
			}
			return(CONFIG_OK);
		}
	}
	snprintf(errorMessage, CONFIG_MAX_ERR_LEN,
		"Unknown section '%s' or element '%s'",
		section,handle);
	return(CONFIG_ERR_ENTRY);
}


/*
** Private
** processDirective	- process a config file line
*/
static int processDirective(sec, dir, val)
	char	*sec,
		*dir,
		*val;
{
	char	*cp;

	/*
	** Map to lower case
	*/
	cp = dir;
	while(*cp)
	{
		*cp = tolower(*cp);
		cp++;
	}

	/*
	** Do your thing
	*/
	return(setConfigEntry(dir,sec,val));
}



/*
** Private
** loadFile	- 
*/

static int  loadFile(file)
	char	*file;
{
	FILE	*fp;
	char	buf[MAX_PATH_LEN],
		*cp = NULL,
		*directive,
		*value,
		*path,
		curSection[80];
	int	errVal = 0,
		fileOK = 1;

	/*
	** Find and open the config file
	*/
	configLoaded = 1;
	if (file)
	{
		snprintf(buf,sizeof(buf),"%s/etc/%s", INST_DIR, file);
		fp = fopen(buf,"r");
		if (!fp)
		{
			snprintf(buf,sizeof(buf),"%s/etc/%s.conf",INST_DIR,
				file);
			fp = fopen(buf,"r");
		}
		if (!fp)
		{
			fp = fopen(file,"r");
		}
		if (!fp)
		{
			return(CONFIG_ERR_FILE);
		}
	}
	else
	{
		snprintf(buf,sizeof(buf),"%s/etc/msql.conf", INST_DIR);
		fp = fopen(buf,"r");
	}

	if (!fp)
	{
		path = getenv("CONF_FILE");
		if (path)
		{
			fp = fopen(path,"r");
		}
		if (!fp)
		{
			return(CONFIG_ERR_FILE);
		}
	}


	/*
	** Read and parse the file
	*/
	getLine(buf,160,fp);
	while(!feof(fp))
	{
		/*
		** Dodge blanks
		*/
		cp = buf;
		skipWhite(cp);
		if (*cp == '#' || *cp == '\n' || *cp == '\r')
		{
			getLine(buf,160,fp);
			continue;
		}

		/*
		** Look for a start of section
		*/
		if (*cp == '[')
		{
			directive = (char *)strtok(cp+1," \t]");
			strcpy(curSection,directive);
			getLine(buf,160,fp);
			continue;
		}

		/*
		** Handle directives
		*/
		if (curSection[0] == 0)
		{
			fprintf(stderr,"Config error at line %d : %s\n",
				curLine, "Directive before section header");
			fileOK = CONFIG_ERR_DATA;
		}

		directive = (char *)strtok(cp, " \t=");
		value = (char *)strtok(NULL, " =\t\n\r");

		errVal =  processDirective(curSection, directive,value);
		if (errVal < CONFIG_OK)
		{
			fprintf(stderr,"Config error at line %d : %s\n",
				curLine, errorMessage);
			fileOK = errVal;
		}
		getLine(buf,160,fp);
	}
	fclose( fp);
	if (fileOK != 0)
	{
		return(fileOK);
	}
	else
	{
		return(CONFIG_OK);
	}
}




/**************************************************************************
** PUBLIC ROUTINES
**************************************************************************/


/*
** Public
** configGetIntEntry	- Return value of integer config entry
*/
int configGetIntEntry(section, handle)
	char	*section,
		*handle;
{
	config_entry	*cur;

	cur = conf_table;
	while(cur->handle)
	{
		if (strcmp(cur->section,section)==0 && 
		    strcmp(cur->handle,handle) == 0)
		{
			return(cur->intVal);
		}
		cur++;
	}
	return(-1);
}


/*
** Public
** configGetCharEntry	- Return value of character config entry
*/
char * configGetCharEntry(section,handle)
	char	*section,
		*handle;
{
	config_entry	*cur;
	char		*tmp;
	static	char 	nullVal[] = "NULL",
			buffer[200];

	cur = conf_table;
	while(cur->handle)
	{
		if (strcmp(cur->section,section)==0 && 
		    strcmp(cur->handle,handle) == 0)
		{
			if (cur->charVal == NULL)
			{
				if (cur->allowNull)
					return(NULL);
				else
					return(nullVal);
			}
			tmp = malloc(200);
			expandConf(cur->charVal, tmp, 200);
			strcpy(buffer, tmp);
			free(tmp);
			return(buffer);
		}
		cur++;
	}
	return(NULL);
}


/*
** Public
** configLoadFile	- Load a given or default config file
*/
int configLoadFile(file)
	char	*file;
{
	/*
	** Don't double load the config file
	*/
	if (configLoaded == 1)
	{
		return(CONFIG_ERR_LOADED);
	}
	return(loadFile(file));
}


/*
** Public
** configReloadFile	- Reload a given or default config file
*/
int configReloadFile(file)
	char	*file;
{
	return(loadFile(file));
}


/*
** Public
** configGetError	- Return a pointer to the last error message
*/
char * configGetError()
{
	return(errorMessage);
}

