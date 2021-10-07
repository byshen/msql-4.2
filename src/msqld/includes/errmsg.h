/*
**      errmsg.h  - Error messages
**
**
** Copyright (c) 1993-95  David J. Hughes
** Copyright (c) 1995  Hughes Technologies Pty Ltd
**
** Permission to use, copy, and distribute for non-commercial purposes,
** is hereby granted without fee, providing that the above copyright
** notice appear in all copies and that both the copyright notice and this
** permission notice appear in supporting documentation.
**
** This software is provided "as is" without any expressed or implied warranty.
**
** ID = "$Id:"
**
*/

/*
** German translations by Nils Faerber <gl055@appl2.hrz.uni-siegen.de>
*/
 


/*************************************************************************
**
**	ENGLISH MESSAGES
**
*/

#ifdef LANG_ENGLISH

   /* CLIENT LIBRARY MESSAGES */

#  define SOCK_ERROR        	"Can't create UNIX socket"
#  define CONNECTION_ERROR    	"Can't connect to local mSQL server"
#  define IPSOCK_ERROR        	"Can't create IP socket"
#  define UNKNOWN_HOST        	"Unknown mSQL Server Host (%s)"
#  define CONN_HOST_ERROR     	"Can't connect to mSQL server on %s"
#  define SERVER_GONE_ERROR   	"mSQL server has gone away"
#  define UNKNOWN_ERROR       	"Unknown mSQL error"
#  define PACKET_ERROR         	"Bad packet received from server"
#  define USERNAME_ERROR    	"Can't find your username. Who are you?"
#  define VERSION_ERROR		\
		"Protocol mismatch. Server Version = %d Client Version = %d"

   /* SERVER MESSAGE */

#  define CON_COUNT_ERROR	"Too many connections"
#  define BAD_HOST_ERROR	"Hostname / IP address mismatch"
#  define HANDSHAKE_ERROR	"Bad handshake"
#  define ACCESS_DENIED_ERROR	"Access to database denied"
#  define NO_DB_ERROR		"No Database Selected"
#  define PERM_DENIED_ERROR	"Permission denied"
#  define UNKNOWN_COM_ERROR	"Unknown command"
#  define BAD_DIR_ERROR		"Can't open directory \"%s\" (%s)"
#  define BAD_TABLE_ERROR	"Unknown table \"%s\""
#  define TABLE_READ_ERROR	"Error reading table \"%s\" definition (%s)"
#  define TMP_MEM_ERROR		"Out of memory for temporary table"
#  define TMP_CREATE_ERROR	"Couldn't create temporary table (%s)"
#  define DATA_OPEN_ERROR	"Couldn't open data file for %s (%s)"
#  define KEY_OPEN_ERROR	"Couldn't open key file for %s (%s)"
#  define STACK_OPEN_ERROR	"Couldn't open stack file for %s (%s)"
#  define WRITE_ERROR		"Data write failed (%s)"
#  define KEY_WRITE_ERROR	"Write of key failed"
#  define SEEK_ERROR		"Seek into data table failed!"
#  define KEY_SEEK_ERROR	"Seek into key table failed!"
#  define BAD_NULL_ERROR	"Field \"%s\" cannot be null"
#  define FIELD_COUNT_ERROR	"Too many fileds in query"
#  define TYPE_ERROR		"Literal value for \'%s\' is wrong type"
#  define BAD_FIELD_ERROR	"Unknown field \"%s.%s\""
#  define BAD_ORD_FIELD_ERROR	"Bad order field. Field \"%s.%s\" was not selected"
#  define BAD_FIELD_2_ERROR	"Unknown field \"%s\""
#  define BAD_ORD_FIELD_2_ERROR	"Bad order field. Field \"%s\" was not selected"
#  define COND_COUNT_ERROR	"Too many fields in condition"
#  define ORDER_COUNT_ERROR	"Too many fields in order specification"
#  define BAD_LIKE_ERROR	"Evaluation of LIKE clause failed"
#  define UNQUAL_ERROR		"Unqualified field in comparison"
#  define BAD_TYPE_ERROR	"Bad type for comparison of '%s'"
#  define INT_LIKE_ERROR	"Can't perform LIKE on int value"
#  define REAL_LIKE_ERROR	"Can't perform LIKE on real value"
#  define LIKE_ERROR		"Invalid field type for LIKE"
#  define BAD_DB_ERROR		"Unknown database \"%s\""
#  define TABLE_EXISTS_ERROR	"Table \"%s\" exists"
#  define TABLE_FAIL_ERROR	"Can't create table \"%s\""
#  define TABLE_WIDTH_ERROR	"Too many fields in table (%d Max)"
#  define CATALOG_WRITE_ERROR	"Error writing catalog"
#  define KEY_CREATE_ERROR	"Creation of key table failed"
#  define DATA_FILE_ERROR	"Error creating table file for \"%s\" (%s)"
#  define BAD_TABLE_ERROR	"Unknown table \"%s\""
#  define NO_VALUE_ERROR	"No value specified for field '%s'"
#  define NON_UNIQ_ERROR	"Field '%s' not unique"
#  define KEY_UNIQ_ERROR	"Non unique value for unique index"
#  define UNSELECT_ERROR	"Reference to un-selected table \"%s\""
#  define UNQUAL_JOIN_ERROR	"Unqualified field \"%s\" in join"
#  define TEXT_COND_ERROR	"Can't use TEXT fields in comparison\n"
#  define TEXT_REGEX_ERROR	"Can't use RLIKE or SLIKE with TEXT fields\n"
#  define SYSVAR_ERROR		"Unknown system variable \"%s\""

#  define NUM_RANGE_ERROR	"Numeric value outside field value range"
#  define VALUE_SIZE_ERROR	"Value for \"%s\" is too large"
#  define NULL_INDEX_ERROR	"Index field \"%s\" cannot be NULL"
#  define NULL_COND_ERR		"Index condition for \"%s\" cannot be NULL"
#  define NULL_JOIN_COND_ERR	"Index condition for \"%s\" in join cannot be NULL"

#  define DATE_ERROR		"Invalid date format"
#  define TIME_ERROR		"Invalid time format"
#  define DATE_TIME_ERROR	"Invalid date/time format"
#  define MILLI_TIME_ERROR	"Invalid mill-time format"
#  define MILLI_DATE_TIME_ERROR	"Invalid milli-date-time format"
#  define IPV4_ERROR		"Invalid IPv4 format"
#  define CIDR4_ERROR		"Invalid cidr4 prefix format"
#  define IPV6_ERROR		"Invalid IPv6 format"
#  define CIDR6_ERROR		"Invalid cidr6 prefix format"

#  define SELECT_EINTR_ERRORS	"The last 100 interations of the main select loop have failed with an\ninterupted system call (EINTR).  The server is aborting\n\n"
#endif


/*************************************************************************
**
**	GERMAN MESSAGES
**
*/


#ifdef LANG_GERMAN

   /* CLIENT LIBRARY MESSAGES */

#  define SOCK_ERROR 		"Kann UNIX-Socket nicht anlegen"
#  define CONNECTION_ERROR	"Keine Verbindung zu lokalem mSQL Server"
#  define IPSOCK_ERROR   	"Kann IP-Socket nicht anlegen"
#  define UNKNOWN_HOST   	"Unbekannter mSQL Server Host (%s)"
#  define CONN_HOST_ERROR 	"Keine Verbindung zu mSQL Server auf %s"
#  define SERVER_GONE_ERROR	"mSQL Server nicht vorhanden"
#  define UNKNOWN_ERROR    	"Unbekannter mSQL Fehler"
#  define PACKET_ERROR     	"Fehlerhaftes Paket von Server empfangen "
#  define USERNAME_ERROR   	"Kann Usernamen nicht herausfinden."
#  define VERSION_ERROR    	\
		"Protokolle ungleich. Server Version = % d Client Version = %d"


   /* SERVER MESSAGE */

#  define CON_COUNT_ERROR	"Too many connections"
#  define BAD_HOST_ERROR	"Can't get hostname for your address"
#  define HANDSHAKE_ERROR	"Bad handshake"
#  define ACCESS_DENIED_ERROR	"Access to database denied"
#  define NO_DB_ERROR		"No Database Selected"
#  define PERM_DENIED_ERROR	"Permission denied"
#  define UNKNOWN_COM_ERROR	"Unknown command"
#  define BAD_DIR_ERROR		"Can't open directory \"%s\""
#  define BAD_TABLE_ERROR	"Unknown table \"%s\""
#  define TABLE_READ_ERROR	"Error reading table \"%s\" definition"
#  define TMP_MEM_ERROR		"Out of memory for temporary table"
#  define TMP_CREATE_ERROR	"Couldn't create temporary table"
#  define DATA_OPEN_ERROR	"Couldn't open data file for %s"
#  define KEY_OPEN_ERROR	"Couldn't open key file for %s"
#  define STACK_OPEN_ERROR	"Couldn't open stack file for %s"
#  define WRITE_ERROR		"Data write failed"
#  define KEY_WRITE_ERROR	"Write of key failed"
#  define SEEK_ERROR		"Seek into data table failed!"
#  define KEY_SEEK_ERROR	"Seek into key table failed!"
#  define BAD_NULL_ERROR	"Field \"%s\" cannot be null"
#  define FIELD_COUNT_ERROR	"Too many fileds in query"
#  define TYPE_ERROR		"Literal value for \'%s\' is wrong type"
#  define BAD_FIELD_ERROR	"Unknown field \"%s.%s\""
#  define BAD_FIELD_2_ERROR	"Unknown field \"%s\""
#  define COND_COUNT_ERROR	"Too many fields in condition"
#  define ORDER_COUNT_ERROR	"Too many fields in order specification"
#  define BAD_LIKE_ERROR	"Evaluation of LIKE clause failed"
#  define UNQUAL_ERROR		"Unqualified field in comparison"
#  define BAD_TYPE_ERROR	"Bad type for comparison of '%s'"
#  define INT_LIKE_ERROR	"Can't perform LIKE on int value"
#  define REAL_LIKE_ERROR	"Can't perform LIKE on real value"
#  define BAD_DB_ERROR		"Unknown database \"%s\""
#  define TABLE_EXISTS_ERROR	"Table \"%s\" exists"
#  define TABLE_FAIL_ERROR	"Can't create table \"%s\""
#  define TABLE_WIDTH_ERROR	"Too many fields in table (%d Max)"
#  define CATALOG_WRITE_ERROR	"Error writing catalog"
#  define KEY_CREATE_ERROR	"Creation of key table failed"
#  define DATA_FILE_ERROR	"Error creating table file for \"%s\" (%s)"
#  define BAD_TABLE_ERROR	"Unknown table \"%s\""
#  define NO_VALUE_ERROR	"No value specified for field '%s'"
#  define NON_UNIQ_ERROR	"Field '%s' not unique"
#  define KEY_UNIQ_ERROR	"Non unique key value in field '%s'"
#  define UNSELECT_ERROR	"Reference to un-selected table \"%s\""
#  define UNQUAL_JOIN_ERROR	"Unqualified field \"%s\" in join"

#  define VALUE_SIZE_ERROR	"Value for \"%s\" is too large"
#  define NULL_INDEX_ERROR	"Index field \"%s\" cannot be NULL"
#  define NULL_COND_ERR		"Index condition for \"%s\" cannot be NULL"

#endif

