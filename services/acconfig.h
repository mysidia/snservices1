/* $Id$ */

/*
 * This is in the top level so that it is in the same "local" directory
 * * as aclocal.m4, so autoreconf causes autoheader to find it. Nothing
 * * actually includes this file, it is always processed into something else.
 */

/*
 * Don't use too large a block, because the autoheader processing can't
 * handle it on some systems.
 */

/* define if your headers do not define sys_errlist */
#undef NEED_SYS_ERRLIST

/* define if your lex defines yylineno */
#undef HAVE_YYLINENO

/* define if you need u_int32_t */
#undef u_int32_t

/* define if you need int32_t */
#undef int32_t

/* define if you need u_int16_t */
#undef u_int16_t

/* define if you need int16_t */
#undef int16_t

/* define if you need u_int8_t */
#undef u_int8_t

/* define if you need int8_t */
#undef int8_t

/* Define to enable +G opflag */
#undef ENABLE_GRPOPS

/* Address that akill/kline mail goes to from services */
#undef AKILLMAILTO

/* Address for ops akill logs (ie: an opers mailing list) */
#undef OPSMAILTO

/* Domain name of network */
#undef NETWORK

/* Path to sendmail and the -t flag */
#undef SENDMAIL

/* Path to help files */
#undef HELP_PATH

/* Path to configuration files and databases */
#undef CFGPATH
