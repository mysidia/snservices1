#ifndef ___interp_h___
#define ___interp_h___

/**
 * \file interp.h
 * \brief Command interpreter header
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

class SLogfile;
extern SLogfile *nicklog, *chanlog, *operlog;

/*
 *
 * These are short names to eliminate redundancy -- the primary reason
 * they are used is so that common names for parameters are enforced
 * for all command functions: IOW, no matter what command function you
 * are looking at, 'nick' is always the sender, for example.
 *
 * -Mysid
 */

/// Declare a command function
#define SCMD_DECL(func)	cmd_return func(UserList *nick, char **args, int numargs)

/// An OperServ Command function
#define OCMD(func)	SCMD_DECL(func)

/// A NickServ Command function
#define NCMD(func)	SCMD_DECL(func)

/// A ChanServ Command function
#define CCMD(func)	SCMD_DECL(func)

/// A MemoServ Command function
#define MCMD(func)	SCMD_DECL(func)

/// A GameServ Command function
#define GCMD(func)	SCMD_DECL(func)

/// An InfoServ Command function
#define ICMD(func)	SCMD_DECL(func)

/// Command return status values
typedef enum {
          /// Command was successful but mostly passive
     RET_OK,

          /// Command was successful and services' databases were affected
     RET_OK_DB,

          /// Command was successful and services' databases were not affected
     RET_OK_NOCHANGE = RET_OK,

          /// Command was successful and services' databases were changed
     RET_OK_DBCHANGE = RET_OK_DB,

          /// The command was not invoked with appropriate parameters
     RET_SYNTAX,

          /// The user lacks permissions necessary for the command to succeed
     RET_NOPERM,

          /// The command requires a password and the user supplied a wrong one
     RET_BADPW,

          /// The user is not a Services Root Admin but the command requires that
          /// level of access of the user.
     RET_NOROOT,

          /// Error condition; the parameter could not be read, or an
          /// internal error/failure as a result of a user or system error
          /// prevented successful completion
     RET_EFAULT,

          /// The target of the command does not exist or was invalid
     RET_NOTARGET,

          /// The command was issued properly and failed but it was not
          /// the result of a fault condition
     RET_FAIL,

          /// The command requires that memory be allocated to complete it
          /// but services wasn't able to complete the allocation
     RET_MEMORY,

          /// The command is not a valid command for this user right now
     RET_INVALID,

          /// This *MUST* be the value returned if the sender was killed
          /// The command must do its own logging if it returns this.
     RET_KILLED,
} cmd_return;

/* OperServ Commands */
OCMD(os_shutdown);
OCMD(os_reset);
OCMD(os_jupe);
OCMD(os_uptime);
OCMD(os_timers);
OCMD(os_sync);
OCMD(os_trigger);
OCMD(os_match);
OCMD(os_cloneset);
OCMD(os_remsra);
OCMD(os_tempakill);
OCMD(os_clonerule);
OCMD(os_grpop);
OCMD(os_setop);
OCMD(os_override);
OCMD(os_strike);
OCMD(os_nixghost);
OCMD(os_help);
OCMD(os_akill);
OCMD(os_ignore);
OCMD(os_help);
OCMD(os_ahurt);
OCMD(os_mode);
OCMD(os_raw);
OCMD(os_akill);
OCMD(os_ignore);
OCMD(os_heal);

/* NickServ Commands */
NCMD(ns_help);
NCMD(ns_identify);
NCMD(ns_cidentify);
NCMD(ns_set);
NCMD(ns_register);
NCMD(ns_info);
NCMD(ns_drop);
NCMD(ns_addmask);
NCMD(ns_access);
NCMD(ns_acc);
NCMD(ns_release);
NCMD(ns_setflags);
NCMD(ns_setopflags);
NCMD(ns_bypass);
NCMD(ns_banish);
NCMD(ns_getpass);
NCMD(ns_getrealpass);
NCMD(ns_delete);
NCMD(ns_list);
NCMD(ns_ghost);
NCMD(ns_recover);
NCMD(ns_save);
NCMD(ns_mark);
NCMD(ns_vacation);
NCMD(ms_save);
NCMD(ns_logoff);
NCMD(ns_verify);
NCMD(ns_setemail);
NCMD(ns_setemail);
NCMD(ns_setpass);

/* ChanServ Commands: */
CCMD(cs_help);
CCMD(cs_clean);
CCMD(cs_clist);
CCMD(cs_info);
CCMD(cs_register);
CCMD(cs_drop);
CCMD(cs_delete);
CCMD(cs_access);
CCMD(cs_addop);
CCMD(cs_addak);
CCMD(cs_wipeak);
CCMD(cs_wipeop);
CCMD(cs_delak);
CCMD(cs_listop);
CCMD(cs_listak);
CCMD(cs_identify);
CCMD(cs_op);
CCMD(cs_deop);
CCMD(cs_modelock);
CCMD(cs_restrict);
CCMD(cs_topiclock);
CCMD(cs_set);
CCMD(cs_save);
CCMD(cs_getpass);
CCMD(cs_getrealpass);
CCMD(cs_invite);
CCMD(cs_unban);
CCMD(cs_list);
CCMD(cs_mdeop);
CCMD(cs_mkick);
CCMD(cs_whois);
CCMD(cs_banish);
CCMD(cs_whois);
CCMD(cs_whois);
CCMD(cs_banish);
CCMD(cs_close);
CCMD(cs_hold);
CCMD(cs_mark);
CCMD(cs_chanop);
CCMD(cs_akick);
CCMD(cs_log);
CCMD(cs_setpass);
CCMD(cs_setrealpass);
CCMD(cs_trigger);
CCMD(cs_dmod);

/* GameServ Commands */
GCMD(gs_roll);
GCMD(gs_help);

/* InfoServ Commands */
ICMD(is_help);
ICMD(is_sendinfo);
ICMD(is_listnews);
ICMD(is_postnews);
ICMD(is_delete);
ICMD(is_save);

/* MemoServ Commands */
MCMD(ms_help);
MCMD(ms_read);
MCMD(ms_send);
MCMD(ms_sendsop);
MCMD(ms_forward);
MCMD(ms_nomemo);
MCMD(ms_clean);
MCMD(ms_delete);
MCMD(ms_list);
NCMD(ms_savememo);
MCMD(ms_unsend);
MCMD(ms_mblock);

namespace interp { 
   namespace commands {
   /**
    * Table of commands -- used by the services logging mechanism
    * every constant in here is mapped to a command function with
    * a TF(xxx) macro, in the above table, also - these mappings are
    * used for automatic logging as found in command flags such as
    * #LOG_ALWAYS, #LOG_DB, or #LOG_OK, for example.
    */
   typedef enum
   {
	SVC_CMD_NONE,

	// ChanServ Commands
	CS_HELP,		CS_INFO,	CS_ACCESS,	CS_CHANOP,
	CS_LISTOP,		CS_ADDOP,	CS_DELOP,	CS_AKICK,
	CS_LISTAK,		CS_ADDAK,	CS_DELAK,	CS_WIPEAK,
	CS_WIPEOP,
	CS_REGISTER,		CS_IDENTIFY,	CS_MDEOP,	CS_MKICK,
	CS_DELETE,		CS_DROP,	CS_OP,		CS_DEOP,
	CS_CLIST,		CS_BANISH,	CS_CLOSE,	CS_HOLD,
	CS_MARK,		CS_MLOCK,	CS_RESTRICT,	CS_TOPICLOCK,
	CS_SET,			CS_GETPASS,	CS_GETREALPASS,	CS_SAVE,
	CS_UNBAN,		CS_INVITE,	CS_LIST,	CS_WHOIS,
	CS_LOG,			CS_SETPASS,	CS_CLEAN,	CS_TRIGGER,
	CS_DMOD,		CS_SETREALPASS,
	CSS_GETPASS_XFER,	CSE_IPC,

	// NickServ Commands
	NS_HELP,		NS_CIDENTIFY,	NS_IDENTIFY,	NS_XIDENTIFY,
	NS_LOGOUT,		NS_REGISTER,	NS_INFO,	NS_GHOST,
	NS_RECOVER,		NS_RELEASE,	NS_SET,		NS_DROP,
	NS_ADDMASK,		NS_ACCESS,	NS_ACC,		NS_SETFLAG,
	NS_SETOP,		NS_MARK,	NS_BYPASS,	NS_BANISH,
	NS_GETPASS,		NS_GETREALPASS,	NS_DELETE,	NS_LIST,
	NS_SAVE,		NS_LOGOFF,	NS_SAVEMEMO,	NS_SETPASS,
	NS_SETEMAIL,		NSS_GETPASS_XFER,	NSE_IPC,

	// MemoServ Commands
	MS_HELP,		MS_READ,	MS_LIST,	MS_SEND,
	MS_SENDSOP,		MS_DEL,		MS_DELETE,	MS_PURGE,
	MS_FORWARD,		MS_NOMEMO,	MS_UNSEND,	MS_MBLOCK,
	MS_SAVEMEMO,		MSE_IPC,

	// OperServ Commands
	OS_HELP,		OS_AKILL,	OS_TEMPAKILL,	OS_AHURT,
	OS_CLONERULE,		OS_IGNORE,	OS_MODE,	OS_RAW,
	OS_SHUTDOWN,		OS_RESET,	OS_REHASH,	OS_JUPE,
	OS_UPTIME,		OS_TIMERS,	OS_SYNC,	OS_TRIGGER,
	OS_MATCH,		OS_CLONESET,	OS_REMSRA,	OS_SETOP,
	OS_GRPOP,		OS_OVERRIDE,	OS_STRIKE,	OS_ALLOCSTAT,
	OS_HEAL,		OS_NIXGHOST,	OSE_IPC,

	// InfoServ Commands
	IS_HELP,		IS_READ,	IS_LIST,	IS_POST,
	IS_DEL,			IS_SAVE,

	// GameServ Commands
	GS_HELP,		GS_ROLL,

	// Services events
	NSE_EXPIRE,		CSE_EXPIRE
   } services_cmd_id; 

   typedef enum {
       CMD_NONE     = (0),
       CMD_AHURT    = (1 << 0),		///< Can use while AHURT
       CMD_REG      = (1 << 1),		///< Can only be used when registered
       CMD_MATCH    = (1 << 2),		///< Command is a pattern
       CMD_IDENT    = (1 << 3),		///< Must be identified to use command
   } svc_cmd_flags;

   }
   namespace logging {
       typedef enum {
           LOG_NONE   = (0),		///< Never log this event/command
           LOG_NO     = LOG_NONE,	///< Never log this event/command
           LOG_ALWAYS = (1 << 0),	///< Always log this event/command
           LOG_OK     = (1 << 1), 	///< Log this command on success
           LOG_DB     = (1 << 2), 	///< Log this command on db change
           LOG_OK_WARN = (1 << 3), 	///< Log and warn.
       } os_log_bits;
   }

   using namespace interp::commands;
   using namespace interp::logging;

   extern class cmd_name_table global_cmd_table[];
   const char *cmd_name(int);

   extern struct service_cmd_t {
       char  *cmd;
       cmd_return (*func)(UserList *, char **, int);
       u_int32_t root;
       os_log_bits log;
       u_int32_t flags;
       unsigned short int flood;

       services_cmd_id cmd_id;
   } operserv_commands[];

   struct cmd_name_table {
       const services_cmd_id cnum;
       const char *name;
       cmd_return (*func)(UserList *, char **, int);
   };

   class parser { 
     public:
     parser(const char *service, u_int32_t opflags, service_cmd_t *cmdlist, const char *incmd);
     cmd_return run(UserList *nick, char **args, int numargs);

     u_int32_t getCmdFlags() {
            if (!theCmd)
                return 0;
            return theCmd->flags;
     }

     private:
        u_int32_t      theOpflags;
        service_cmd_t *theCmd;
        const char    *theService;
   };
}

using namespace interp::commands;
using namespace interp::logging;
#endif
