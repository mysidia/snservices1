#ifndef __log_h__
#define __log_h__

/**
 * \file log.h
 * \brief Log system headers
 *
 * \mysid
 * $Id$
 */

/*
 * Copyright (c) 2001 James Hess
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the authors nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "interp.h"

extern const char *fullhost1(const UserList *);

/**
 * \brief Used to default NULL format without triggering compiler warnings.
 */
extern const char *nullFmtHack;

/* Log flags */
          /// Normal log entry
#define LOGF_NORMAL	0x0

          /// Bad PW Log
#define LOGF_BADPW	0x1

          /// Failed command log
#define LOGF_FAIL	0x2

          /// 'Value turned off' log
#define LOGF_OFF	0x4

          /// 'Value turned on' log
#define LOGF_ON		0x8

          /// Log of oper action
#define LOGF_OPER	0x10

          /// Data field is a pattern
#define LOGF_PATTERN	0x20

          /// ???
#define LOGF_SCAN	0x40

          /// Successful command log
#define LOGF_OK		0x80

/*!
 * \brief Class for handling logfile operations
 */
class SLogfile {
  public:
    SLogfile(const char *);
    SLogfile(const SLogfile&);
    SLogfile(const char *, const char *);
    ~SLogfile();

    /// Flush output
    void flush();

    /// Normal log entry
    void log(UserList *sender, 
            interp::services_cmd_id cmd,
            const char *target,
            u_int32_t flags = 0, const char *extra = nullFmtHack, ...)
    __attribute__ ((format (printf, 6, 7)));

    /// Working log only
    void logw(UserList *sender, 
            interp::services_cmd_id cmd,
            const char *target,
            u_int32_t flags = 0, const char *extra = nullFmtHack, ...)
    __attribute__ ((format (printf, 6, 7)));

    /// Create a new log entry in the file
    void logx(UserList *sender, int, 
            interp::services_cmd_id cmd,
            const char *target,
            u_int32_t flags = 0, const char *extra = nullFmtHack, va_list ap = NULL);

#if 0
    void write_log_db(const char *key, struct log_file_entry *aaa) {
      DBT key, value;

      if (!db) return;

      key.data = key;
      key.size = strlen(key);
      value.data = *aaa;
      value.size = sizeof(*aaa);
      db->put(db, &key, &value, 0);
    }
#endif
  private:
          /// Log file pointer
   FILE *fp, *fpw;  
   char *logFileName, *logwFileName;
   char fp_noclose, fpw_noclose;

   /* DB *db; */
};

          /// OperServ log handler
extern class SLogfile *operlog;  

          /// NickServ log handler
extern class SLogfile *nicklog;

          /// ChanServ log handler
extern class SLogfile *chanlog;

          /// Core/System Error log file
extern FILE           *corelog;

#endif
