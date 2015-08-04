/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2012 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

extern "C" {
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
}

#include "php_mysqlbinlog.h"
#include "zend_exceptions.h"
#include "MyBinlog.h"
#include "MyEvent.h"

#include <iostream>
#include <map>
#include <string>

ZEND_DECLARE_MODULE_GLOBALS(mysqlbinlog)

ZEND_BEGIN_ARG_INFO_EX(arginfo_connect, 0, 0, 1)
ZEND_ARG_INFO(0, connect_str)
ZEND_ARG_INFO(0, server_id)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_disconnect, 0, 0, 1)
ZEND_ARG_INFO(0, link)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_wait_for_next_event, 0, 0, 1)
ZEND_ARG_INFO(0, link)
ZEND_ARG_INFO(0, db_watch_list)
ZEND_ARG_INFO(0, table_wild_watch_list)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_set_position, 0, 0, 2)
ZEND_ARG_INFO(0, link)
ZEND_ARG_INFO(0, position)
ZEND_ARG_INFO(0, filename)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_get_position, 0, 0, 1)
ZEND_ARG_INFO(0, link)
ZEND_ARG_INFO(1, filename)
ZEND_END_ARG_INFO()


/* True global resources - no need for thread safety here */
#define BINLOG_LINK_DESC "MySQL Binlog 连接句柄"

static void php_mysqlbinlog_init_globals(zend_mysqlbinlog_globals *mysqlbinlog_globals)
{
    mysqlbinlog_globals->filename = "";
    mysqlbinlog_globals->position = 0;
    mysqlbinlog_globals->flag = false;
}

static int le_binloglink;

/* {{{ mysqlbinlog_functions[]
 *
 * Every user visible function must have an entry in mysqlbinlog_functions[].
 */
const zend_function_entry mysqlbinlog_functions[] = {
    PHP_FE(binlog_connect, arginfo_connect)
    PHP_FE(binlog_disconnect, arginfo_disconnect)    
    PHP_FE(binlog_wait_for_next_event, arginfo_wait_for_next_event)
    PHP_FE(binlog_set_position, arginfo_set_position)
    PHP_FE(binlog_get_position, arginfo_get_position)
    PHP_FE_END	/* Must be the last line in mysqlbinlog_functions[] */
};
/* }}} */

/* {{{ mysqlbinlog_module_entry
 */
zend_module_entry mysqlbinlog_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"mysqlbinlog",
	mysqlbinlog_functions,
	PHP_MINIT(mysqlbinlog),
	PHP_MSHUTDOWN(mysqlbinlog),
	PHP_RINIT(mysqlbinlog),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(mysqlbinlog),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(mysqlbinlog),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_MYSQLBINLOG_VERSION, /* Replace with version number for your extension */
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_MYSQLBINLOG
extern "C" {
ZEND_GET_MODULE(mysqlbinlog)
}
#endif

/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("mysqlbinlog.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_mysqlbinlog_globals, mysqlbinlog_globals)
    STD_PHP_INI_ENTRY("mysqlbinlog.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_mysqlbinlog_globals, mysqlbinlog_globals)
PHP_INI_END()
*/
/* }}} */

/* {{{ php_mysqlbinlog_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_mysqlbinlog_init_globals(zend_mysqlbinlog_globals *mysqlbinlog_globals)
{
	mysqlbinlog_globals->global_value = 0;
	mysqlbinlog_globals->global_string = NULL;
}
*/
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(mysqlbinlog)
{
    REGISTER_LONG_CONSTANT("BINLOG_UNKNOWN_EVENT"            , binary_log::UNKNOWN_EVENT            , CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BINLOG_START_EVENT_V3"           , binary_log::START_EVENT_V3           , CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BINLOG_QUERY_EVENT"              , binary_log::QUERY_EVENT              , CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BINLOG_STOP_EVENT"               , binary_log::STOP_EVENT               , CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BINLOG_ROTATE_EVENT"             , binary_log::ROTATE_EVENT             , CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BINLOG_INTVAR_EVENT"             , binary_log::INTVAR_EVENT             , CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BINLOG_LOAD_EVENT"               , binary_log::LOAD_EVENT               , CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BINLOG_SLAVE_EVENT"              , binary_log::SLAVE_EVENT              , CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BINLOG_CREATE_FILE_EVENT"        , binary_log::CREATE_FILE_EVENT        , CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BINLOG_APPEND_BLOCK_EVENT"       , binary_log::APPEND_BLOCK_EVENT       , CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BINLOG_EXEC_LOAD_EVENT"          , binary_log::EXEC_LOAD_EVENT          , CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BINLOG_DELETE_FILE_EVENT"        , binary_log::DELETE_FILE_EVENT        , CONST_CS | CONST_PERSISTENT);

    REGISTER_LONG_CONSTANT("BINLOG_NEW_LOAD_EVENT"           , binary_log::NEW_LOAD_EVENT           , CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BINLOG_RAND_EVENT"               , binary_log::RAND_EVENT               , CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BINLOG_USER_VAR_EVENT"           , binary_log::USER_VAR_EVENT           , CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BINLOG_FORMAT_DESCRIPTION_EVENT" , binary_log::FORMAT_DESCRIPTION_EVENT , CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BINLOG_XID_EVENT"                , binary_log::XID_EVENT                , CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BINLOG_BEGIN_LOAD_QUERY_EVENT"   , binary_log::BEGIN_LOAD_QUERY_EVENT   , CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BINLOG_EXECUTE_LOAD_QUERY_EVENT" , binary_log::EXECUTE_LOAD_QUERY_EVENT , CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BINLOG_TABLE_MAP_EVENT"          , binary_log::TABLE_MAP_EVENT          , CONST_CS | CONST_PERSISTENT);

    REGISTER_LONG_CONSTANT("BINLOG_PRE_GA_WRITE_ROWS_EVENT"  , binary_log::PRE_GA_WRITE_ROWS_EVENT  , CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BINLOG_PRE_GA_UPDATE_ROWS_EVENT" , binary_log::PRE_GA_UPDATE_ROWS_EVENT , CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BINLOG_PRE_GA_DELETE_ROWS_EVENT" , binary_log::PRE_GA_DELETE_ROWS_EVENT , CONST_CS | CONST_PERSISTENT);

    REGISTER_LONG_CONSTANT("BINLOG_WRITE_ROWS_EVENT"         , binary_log::WRITE_ROWS_EVENT         , CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BINLOG_UPDATE_ROWS_EVENT"        , binary_log::UPDATE_ROWS_EVENT        , CONST_CS | CONST_PERSISTENT);
    REGISTER_LONG_CONSTANT("BINLOG_DELETE_ROWS_EVENT"        , binary_log::DELETE_ROWS_EVENT        , CONST_CS | CONST_PERSISTENT);

    REGISTER_LONG_CONSTANT("BINLOG_INCIDENT_EVENT"           , binary_log::INCIDENT_EVENT           , CONST_CS | CONST_PERSISTENT);

    /* If you have INI entries, uncomment these lines 
    REGISTER_INI_ENTRIES();
    */
    le_binloglink = zend_register_list_destructors_ex(binlog_destruction_handler, NULL, BINLOG_LINK_DESC, module_number);

    ZEND_INIT_MODULE_GLOBALS(mysqlbinlog, php_mysqlbinlog_init_globals, NULL);

    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(mysqlbinlog)
{
	/* uncomment this line if you have INI entries
	UNREGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(mysqlbinlog)
{
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(mysqlbinlog)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(mysqlbinlog)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "mysqlbinlog support", "enabled");
    php_info_print_table_row(2, "Version", PHP_MYSQLBINLOG_VERSION);
    php_info_print_table_row(2, "Author", "Roy Gu (guweigang@baidu.com, guweigang@outlook.com)");
    php_info_print_table_row(2, "Author", "Ideal (shangyuanchun@baidu.com, idealities@gmail.com)");    
    php_info_print_table_end();

    /* Remove comments if you have entries in php.ini
    DISPLAY_INI_ENTRIES();
    */
}
/* }}} */

/* this function is called when request shutdown */
/* {{{ binlog_destruction_handler
 */
void binlog_destruction_handler(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
    MyBinlog *bp = (MyBinlog *)rsrc->ptr;
    if (bp) {
        bp->disconnect();
    }
}
/* }}} */

PHP_FUNCTION(binlog_connect)
{
    char *arg = NULL;
    int   arg_len;
    MyBinlog *bp;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &arg, &arg_len) == FAILURE) {
        RETURN_NULL();
    }
    
    bp = new MyBinlog;
    if(MYSQLBINLOG_G(flag) == true) {
        bp->set_position(MYSQLBINLOG_G(position));
        bp->set_filename(MYSQLBINLOG_G(filename));
    }
    try {
        bp->connect(std::string(arg));
    } catch(const std::runtime_error& le) {
        zend_throw_exception(zend_exception_get_default(TSRMLS_C), "Connect to mysql failed", 0 TSRMLS_CC);
    }
    
    ZEND_REGISTER_RESOURCE(return_value, bp, le_binloglink);
}

PHP_FUNCTION(binlog_disconnect)
{
    zval *link; int id = -1;
    MyBinlog *bp;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &link) == FAILURE) {
        RETURN_NULL();
    }
    ZEND_FETCH_RESOURCE(bp, MyBinlog *, &link, id, BINLOG_LINK_DESC, le_binloglink);

    if(!bp) {
        zend_throw_exception(zend_exception_get_default(TSRMLS_C),
                             "Wrong resource handler passed to binlog_disconnect",
                             0 TSRMLS_CC);        
    }
    // free resource
    zend_list_delete(Z_LVAL_P(link));
}

PHP_FUNCTION(binlog_wait_for_next_event)
{
    zval *link; int id = -1;
    zval *db_zval_p  = NULL;
    zval *tbl_zval_p = NULL;
    MyBinlog *bp;
    MyEvent* event = new MyEvent;
    std::string msg;
    
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r|zz", &link, &db_zval_p, &tbl_zval_p) == FAILURE) {
        RETURN_NULL();
    }

    ZEND_FETCH_RESOURCE(bp, MyBinlog *, &link, id, BINLOG_LINK_DESC, le_binloglink);

    if (!bp) {
        zend_throw_exception(zend_exception_get_default(TSRMLS_C),
                             "Wrong resource handler passed to binlog_wait_for_next_event()",
                             0 TSRMLS_CC);
    }
    
    try {
        msg = bp->get_next_event(event);
    } catch(const std::exception& le) {
        RETURN_NULL();
    }

    // array initial
    array_init(return_value);
    
    add_assoc_long(return_value, "type_code", event->event_type);
    add_assoc_string(return_value, "type_str", (char *)(event->event_type_str).c_str(), 1);
    add_assoc_long(return_value, "next_position", event->position);

    if(event->is_data_affected()) {
        add_assoc_string(return_value, "message", (char *)(event->message).c_str(), 1);
    }

    delete event;
}

PHP_FUNCTION(binlog_set_position)
{
    long position;
    zval *file = NULL;
    std::string filename;
    
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l|z!", &position, &file) == FAILURE) {
        RETURN_NULL();
    }
    MYSQLBINLOG_G(flag) = true;
    MYSQLBINLOG_G(position) = position;
    if (!file) {
    } else if (Z_TYPE_P(file) == IS_STRING) {
        MYSQLBINLOG_G(filename) = std::string(Z_STRVAL_P(file));
    } else {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "filename must be a string");
        RETURN_FALSE;
    }
    RETURN_TRUE;
}

PHP_FUNCTION(binlog_get_position)
{
    zval *link, *file = NULL;
    int id = -1;
    unsigned long long position;
    MyBinlog *bp;
    
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r|z", &link, &file) == FAILURE) {
        RETURN_NULL();
    }

    ZEND_FETCH_RESOURCE(bp, MyBinlog *, &link, id, BINLOG_LINK_DESC, le_binloglink);

    if (!bp) {
        php_error_docref(NULL TSRMLS_CC, E_WARNING, "Wrong resource handler passed to binlog_get_position().");
        RETURN_NULL();
    }

    if (!file) {
        position = bp->get_raw()->get_position();
    } else {
        std::string filename;
        zval_dtor(file);
        position = bp->get_raw()->get_position(filename);
        ZVAL_STRING(file, filename.c_str(), 1);
    }
    RETURN_LONG(position);
}

/*
 * Local variables:
 * c-basic-offset: 4
 * End:
 * vim600: et sw=4 ts=4
 * vim<600: et sw=4 ts=4
 */
