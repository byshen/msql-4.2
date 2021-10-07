/* msqlimport.c: Import / Export a table.
**
**   The basis of this software was originally written by Pascal Forget,
**   a long-time member and contributor in the mSQL community. This code
**   has been rewritten from scratch to look more like the other mSQL code
**   and to improve reliability.
**   					bambi@Hughes.com.au
**   
** ---------------------------------------------------------------------
**   Modified by Georg Horn, horn@uni-koblenz.de, to support:
**   - Mapping of input field number n to column number n.
**   - Storing constant values into columns.
**   - Storing result of 'select _seq from table' intop columns.
**     This requires a sequence to be defined on the table...
**   - Number of input fields must not match number of columns,
**     excess fields are ignored, missing fields are set to "".
**   - If the last char of a line is a backslash, it is replaced by
**     a newline and the next line is appended to the current line.
**   - Included exporting of tables from msqlexport.
**     - Rewrote and fixed escapeText() to proper count number of
**       needed escape chars and to also escape newlines.
**     - Added command line option -Q String to use String for
**       the select query.
**
**   Besides that, did some cosmetic changes and cleanups...
**   (Sorry for mixing up the code so badly... ;-))
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>

#if defined(_OS_WIN32)
#include <winsock.h>
#endif

#include <common/portability.h>
#include <common/msql_defs.h>
#include <libmsql/msql.h>


#define	MAX_FIELDS	75

/* Exit codes */

#define EX_USAGE 1
#define EX_MSQLERR 2


/* Usage: */

char usage[] = "\n\
usage: msqlimport [-h host] [-v] [-i] [-x] [-m Mapping] [-s Char] [-q Char]\n\
	    [-e Char] [-Q String] database table\n\n\
    Imports/Exports a formatted ASCII file into/from a table.\n\n\
    -h Host	Connect to msql daemon on Host\n\
		Default is the localhost\n\
    -v		Be Verbose\n\
    -i		Import (Default)\n\
    -x		Export\n\
    -m n:m	Map input field number m to column number n\n\
    -m n:s	Put value of 'select _seq from table' into column number n\n\
    -m n:cblah	Put constant value bla into column number n\n\
    -s Char	Use the character Char as the separation character\n\
		Default is a comma\n\
    -q Char	Use the specifed Char as the quote character\n\
		Default is none\n\
    -e Char	Use the specifed Char as the escape character\n\
		Default is a backslash\n\
    -Q String	Use the specified String for the select query\n\
		Default is \"select * from table\"\n\n";


/* Some globals... */

enum { import, export } mode = import;
int verbose = 0;
int sock = -1;
int lineNo = 0;
int recNo = 0;


/* For mapping fields of the input file to fields of a table row. */

struct {
    enum { fieldval, constval, sequence } type;
    int field;
    char *value;
} map[MAX_FIELDS];


/* dbError: prints mSQL error message and exits the program. */

void dbError()
{
    if (mode == import)
	fprintf(stderr, "\nmSQL error at input line %d: %s\n",
	    lineNo, msqlErrMsg);
    else
	fprintf(stderr, "mSQL error: %s\n", msqlErrMsg);
    if (sock != -1)
	msqlClose(sock);
    exit(EX_MSQLERR);
}


/* dbConnect: connects to the host and selects DB.
   Also checks whether the tablename is a valid table name. */

void dbConnect(host, database)
char *host, *database;
{
    if (verbose)
	fprintf(stderr, "Connecting to %s...\n", host ? host : "localhost");
    sock = msqlConnect(host);
    if (sock == -1)
	dbError();
    if (verbose)
	fprintf(stderr, "Selecting data base %s...\n", database);
    if (msqlSelectDB(sock, database) == -1)
	dbError();
}


/* dbDisconnect: disconnects from the host. */

void dbDisconnect(host)
char *host;
{
    if (verbose)
	fprintf(stderr, "Disconnecting from %s...\n",
	    host ? host : "localhost");
    msqlClose(sock);
}


/* getFieldDefs: get number of fields and data types of the fields. */

int getFieldDefs(table, fDefs)
char *table;
int *fDefs;
{
    int numFields;
    m_result *res;
    m_field *curField;

    if (verbose)
	fprintf(stderr, "Importing into table '%s'...\n", table);
    res = msqlListFields(sock, table);
    if (!res)
	dbError();
    numFields = 0;
    curField = msqlFetchField(res);
    while (curField) {
	if (curField->type > LAST_REAL_TYPE)
	    break;
	fDefs[numFields] = curField->type;
	numFields++;
	if (verbose)
	    fprintf(stderr, "Field %d is '%s' of type %s\n", numFields,
		curField->name, msqlTypeName(curField->type));
	curField = msqlFetchField(res);
    }
    msqlFreeResult(res);
    return numFields;
}


/* escapeText: escapes char sep (and in export mode newlines) within
   string str with char esc */

char *escapeText(str, sep, esc)
char *str, sep, esc;
{
    char *cp, *cp2;
    int needlen = 0;
    static int resultlen = 0;
    static char *result = NULL;

    if (!str || !*str)
	return "";
    cp = str;
    while (*cp) {
	needlen++;
	if (*cp == sep || *cp == esc || ((mode == export) && (*cp == '\n')))
	    needlen++;
	cp++;
    }
    needlen++;
    if (needlen > resultlen) {
	result = result ? realloc(result, needlen) : malloc(needlen);
	resultlen = needlen;
    }
    cp2 = result;
    cp = str;
    while (*cp) {
	if (*cp == sep || *cp == esc || ((mode == export) && (*cp == '\n')))
	    *cp2++ = esc;
	*cp2++ = *cp++;
    }
    *cp2 = '\0';
    return result;
}


/* generateQuery: build the query string to insert a row into the table. */

char *generateQuery(table, numFields, fDefs, fVals)
char *table;
int numFields, *fDefs;
char **fVals;
{
    static char qBuf[5 * 1024];
    char val[16], *p;
    int count;

    snprintf(qBuf, sizeof(qBuf), "insert into %s values ( ", table);
    count = 0;
    val[0] = 0;
    while (count < numFields) {
	if (map[count].type == sequence) {
	    if (!val[0]) {
		static char q[64] = "";
		m_result *res;
		m_row row;
		if (!q[0])
		    snprintf(q, sizeof(q), "select _seq from %s", table);
		if (msqlQuery(sock, q) < 0)
		    dbError();
		if (!(res = msqlStoreResult()))
		    dbError();
		if (!(row = msqlFetchRow(res)))
		    dbError();
		strcpy(val, row[0]);
		msqlFreeResult(res);
	    }
	    map[count].value = val;
	}
	p = (map[count].type == fieldval) ? fVals[map[count].field] :
	    map[count].value;
	switch (fDefs[count]) {
	case CHAR_TYPE:
	case TEXT_TYPE:
	case DATE_TYPE:
	case TIME_TYPE:
	case DATETIME_TYPE:
	case MILLITIME_TYPE:
	case MILLIDATETIME_TYPE:
	case IPV4_TYPE:
	case CIDR4_TYPE:
	case IPV6_TYPE:
	case CIDR6_TYPE:
	    strcat(qBuf, "'");
	    strcat(qBuf, escapeText(p, '\'', '\\'));
	    strcat(qBuf, "'");
	    break;
	default:
	    if (*p)
		strcat(qBuf, p);
	    else
		strcat(qBuf, "NULL");
	    break;
	}
	count++;
	if (count < numFields)
	    strcat(qBuf, ", ");
    }
    strcat(qBuf, ")");
    return qBuf;
}


/* splitLine: split an input line into the different fields. */

void splitLine(numFields, fVals, line, esc, sep, quote)
int numFields;
char *fVals[], *line, esc, sep, quote;
{
    int count = 0, qState = 0;
    char *cp1, *cp2;

    cp1 = cp2 = line;
    while (*cp2) {
	if (quote) {
	    if (qState == 0 && *cp2 == quote) {
		cp2++;
		qState = 1;
		continue;
	    }
	    if (qState == 1 && *cp2 == quote && *(cp2 + 1) == quote &&
		    quote == esc) {
		cp2++;
		cp2++;
		continue;
	    }
	    if (*cp2 == quote) {
		cp2++;
		if ((*cp2 == sep) || *cp2 == 0)
		    qState = 2;
		continue;
	    }
	}
	if (*cp2 == esc) {
	    cp2++;
	    cp2++;
	    continue;
	}
	if (*cp2 == sep) {
	    if (qState == 1) {
		cp2++;
		continue;
	    }
	    if (qState == 2) {
		*(cp2 - 1) = 0;
		cp1++;
	    }
	    *cp2 = 0;
	    fVals[count++] = cp1;
	    qState = 0;
	    cp1 = cp2 = cp2 + 1;
	    continue;
	}
	cp2++;
    }
    if (qState == 2) {
	*(cp2 - 1) = 0;
	cp1++;
    }
    *cp2 = 0;
    fVals[count++] = cp1;
    while (count < MAX_FIELDS)
	fVals[count++] = "";
}


/* unescapeData: remove escape chars from the fields of an input line. */

void unescapeData(numFields, fVals, esc)
int numFields;
char **fVals, esc;
{
    char *cp1, *cp2;
    int count = 0;

    while (count < numFields) {
	cp1 = cp2 = fVals[count];
	if (cp1 && *cp1) {
	    while (*cp2) {
		if (*cp2 == esc)
		    cp2++;
		*cp1++ = *cp2++;
	    }
	    *cp1 = 0;
	}
	count++;
    }
}


/* processInput: process the lines of the input file. */

void processInput(table, numFields, fDefs, esc, sep, quote)
char *table;
int numFields, *fDefs;
char esc, sep, quote;
{
    char *q, line[5 * 1024], *fVals[MAX_FIELDS];
    int l, numesc;

    if (verbose)
	fprintf(stderr, "Input line: ");
    while (fgets(line, sizeof(line), stdin)) {
      again:
	lineNo++;
	if (verbose) {
	    if (!(lineNo % 100)) {
		fprintf(stderr, "%d", lineNo);
	    } else if (!(lineNo % 10)) {
		fprintf(stderr, ".");
	    }
	    fflush(stderr);
	}
	l = strlen(line) - 1;
	line[l] = 0;
	if (*line == 0)
	    continue;
	l--;
	numesc = 0;
	while (l >= 0 && line[l] == esc) {
	    numesc++;
	    l--;
	}
	if (numesc % 2) {
	    line[l + numesc] = '\n';
	    fgets(line + l + numesc + 1, sizeof(line) - l - numesc - 1, stdin);
	    goto again;
	}
	splitLine(numFields, fVals, line, esc, sep, quote);
	unescapeData(numFields, fVals, esc);
	q = generateQuery(table, numFields, fDefs, fVals);
	if (msqlQuery(sock, q) < 0) {
	    fprintf(stderr, "\nQuery was: %s\n", q);
	    dbError();
	}
	recNo++;
    }
    if (verbose)
	fprintf(stderr, "%d\n", lineNo);
}


/* dumpTable: dump a table to stdout. */

void dumpTable(table, sep, quote, esc, query)
char *table, sep, quote, esc, *query;
{
    m_result *res;
    m_row row;
    int i;

    if (verbose)
	fprintf(stderr, "Sending SELECT query...\n");
    if (!query) {
	query = malloc(64);
	snprintf(query, 64, "SELECT * FROM %s", table);
    }
    if (msqlQuery(sock, query) == -1)
	dbError();
    if (!(res = msqlStoreResult()))
	dbError();
    if (verbose)
	fprintf(stderr, "Retrieved %d rows. Processing...\n", msqlNumRows(res));
    while ((row = msqlFetchRow(res))) {
	for (i = 0; i < msqlNumFields(res); i++) {
	    if (quote)
		printf("%c%s%c", quote, escapeText(row[i], quote, esc), quote);
	    else
		printf("%s", escapeText(row[i], sep, esc));
	    if (i < msqlNumFields(res) - 1)
		printf("%c", sep);
	}
	printf("\n");
    }
}


/* main: Here we go. */

int main(argc, argv)
int argc;
char *argv[];
{
    int i, c, errFlag = 0, fDefs[MAX_FIELDS], numFields;
    char *host = NULL, *database = NULL, *table = NULL, *query = NULL;
    char sep = ',', quote = 0, esc = '\\';
    extern char *optarg;
    extern int optind;

    /* map all input fields to their corresponding row fields. */
    for (i = 0; i < MAX_FIELDS; i++) {
	map[i].type = fieldval;
	map[i].field = i;
	map[i].value = "";
    }

    while ((c = getopt(argc, argv, "h:vixm:s:q:e:Q:")) != -1) {
	switch (c) {
	case 'h':
	    host = optarg;
	    break;
	case 'v':
	    verbose++;
	    break;
	case 'i':
	    mode = import;
	    break;
	case 'x':
	    mode = export;
	    break;
	case 'm': {
	    char *cp = optarg;
	    char *sepp = strchr(cp, ':');
	    if (!sepp || *cp < '0' || *cp > '9') {
		errFlag++;
	    } else {
		i = atoi(cp);
		cp = sepp + 1;
		if (!*cp) {
		    errFlag++;
		} else {
		    if (*cp >= '0' && *cp <= '9') {
			map[i].type = fieldval;
			map[i].field = atoi(cp);
		    } else if (*cp == 'c') {
			map[i].type = constval;
			map[i].value = cp + 1;
		    } else if (*cp == 's') {
			map[i].type = sequence;
		    } else {
			errFlag++;
		    }
		}
	    }
	    break;
	}
	case 's':
	    sep = *optarg;
	    break;
	case 'q':
	    quote = *optarg;
	    break;
	case 'e':
	    esc = *optarg;
	    break;
	case 'Q':
	    query = optarg;
	    break;
	default:
	    errFlag++;
	    break;
	}
    }
    if (errFlag) {
	fprintf(stderr, "%s", usage);
	exit(EX_USAGE);
    }
    database = argv[optind++];
    table = argv[optind++];
    if (!database || !table) {
	fprintf(stderr, "%s", usage);
	exit(EX_USAGE);
    }
    if (verbose)
	fprintf(stderr, "\n");
    dbConnect(host, database);
    if (mode == import) {
	numFields = getFieldDefs(table, fDefs);
	processInput(table, numFields, fDefs, esc, sep, quote);
	if (verbose)
	    fprintf(stderr,
		"Done. %d input lines processed, %d records inserted.\n",
		lineNo, recNo);
    } else {
	dumpTable(table, sep, quote, esc, query);
	if (verbose)
	    fprintf(stderr, "Done.\n");
    }
    dbDisconnect(host);
    exit(0);
}
