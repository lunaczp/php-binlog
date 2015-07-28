dnl $Id$
dnl config.m4 for extension mysqlbinlog

PHP_ARG_WITH(mysql-binlog, for mysql replication listener support,
[  --with-mysql-binlog=DIR Specify where mysql replication listener is installed])

if test "$PHP_MYSQL_BINLOG" != "no"; then

  dnl --with-mysql-binlog -> check with-path
  SEARCH_PATH="/usr/local /usr"
  SEARCH_FOR_REPLICATION="/include/mysql-binlog-events/binlog.h"

  if test -r $PHP_MYSQL_BINLOG/$SEARCH_FOR_REPLICATION; then
    MYSQL_REPLICATION_DIR=$PHP_MYSQL_BINLOG
  else
    AC_MSG_CHECKING([for mysql-binlog-events libraries in default path])
    for i in $SEARCH_PATH ; do
      if test -r $i/$SEARCH_FOR_REPLICATION; then
        MYSQL_REPLICATION_DIR=$i
        AC_MSG_RESULT(found in $i)
      fi
    done
  fi

  dnl test replication dir
  if test -z "$MYSQL_REPLICATION_DIR"; then
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([Please ensure that --with-mysql-binlog is really the install prefix for listener])
  fi

  PHP_REQUIRE_CXX()

  PHP_ADD_INCLUDE($MYSQL_REPLICATION_DIR/include/mysql-binlog-events/)

  PHP_SUBST(MYSQLBINLOG_SHARED_LIBADD)
  PHP_ADD_LIBRARY(stdc++, 1, MYSQLBINLOG_SHARED_LIBADD)
  PHP_ADD_LIBRARY(pthread, 1, MYSQLBINLOG_SHARED_LIBADD)
  
  PHP_ADD_LIBPATH($MYSQL_REPLICATION_DIR"/lib")
  PHP_ADD_LIBRARY(mysqlstream, 1, MYSQLBINLOG_SHARED_LIBADD)
  PHP_ADD_LIBRARY(binlogevents, 1, MYSQLBINLOG_SHARED_LIBADD)

  PHP_NEW_EXTENSION(mysqlbinlog, mysqlbinlog.cpp \
                                 MyBinlog.cpp \
                                 MyEvent.cpp \
                                 , $ext_shared)
fi
