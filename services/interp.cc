/*
 * Copyright (c) 2000-2001 James Hess
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

/**
 * \file interp.cc
 * \brief Command parser module
 *
 * This module implements dispatch-table parsing of all services
 * commands rather than a nasty maze of strcasecmps or discrete
 * dispatch table implementations per service; this module
 * provides for a unified interface that can be used to parse
 * nearly any command for any service.
 *
 * \mysid
 * \date 2000-2001
 *
 * $Id$
 */

#include "services.h"
#include "operserv.h"
#include "log.h"
#include "interp.h"

/** @def TBL
 *  \brief Enter an event log constant into the command table
 */

/** @def TF
 *  \brief Enter a log constant bound to a command function
 *         into the command table.
 */

/**
 * \brief Command interpreter data and utilities
 *        tables are also contained for the benefit of
 *        the services logging subsystem.
 */
namespace interp
{
#define TBL(ccname) { ccname, #ccname }
#define TF(ccname, fname) { ccname, #ccname, fname }
	cmd_name_table global_cmd_table[] = {
		// ChanServ name mappings
		TF(CS_HELP, cs_help),
		TF(CS_INFO, cs_info),
		TF(CS_ACCESS, cs_access),
		TF(CS_CHANOP, cs_chanop),
		TF(CS_LISTOP, cs_listop),
		TF(CS_ADDOP, cs_addop),
		TBL(CS_DELOP) /* actually uses cs_addop */ ,
		TF(CS_AKICK, cs_akick),
		TF(CS_LISTAK, cs_listak),
		TF(CS_ADDAK, cs_addak),
		TF(CS_DELAK, cs_delak),
		TF(CS_WIPEAK, cs_wipeak),
		TF(CS_REGISTER, cs_register),
		TF(CS_IDENTIFY, cs_identify),
		TF(CS_MDEOP, cs_mdeop),
		TF(CS_MKICK, cs_mkick),
		TF(CS_DELETE, cs_delete),
		TF(CS_DROP, cs_drop),
		TF(CS_OP, cs_op),
		TF(CS_DEOP, cs_deop),
		TF(CS_CLIST, cs_clist),
		TF(CS_BANISH, cs_banish),
		TF(CS_CLOSE, cs_close),
		TF(CS_HOLD, cs_hold),
		TF(CS_MARK, cs_mark),
		TF(CS_MLOCK, cs_modelock),
		TF(CS_RESTRICT, cs_restrict),
		TF(CS_TOPICLOCK, cs_topiclock),
		TF(CS_SET, cs_set),
		TF(CS_GETPASS, cs_getpass),
		TBL(CSS_GETPASS_XFER),
		TF(CS_GETREALPASS, cs_getrealpass),
		TF(CS_SAVE, cs_save),
		TF(CS_UNBAN, cs_unban),
		TF(CS_INVITE, cs_invite),
		TF(CS_LIST, cs_list),
		TF(CS_WHOIS, cs_whois),
		TF(CS_LOG, cs_log),
		TF(CS_CLEAN, cs_clean),
		TF(CS_WIPEOP, cs_wipeop),
		TF(CS_SETPASS, cs_setpass),
		TF(CS_SETREALPASS, cs_setrealpass),
		TF(CS_DMOD, cs_dmod),
		TF(CS_TRIGGER, cs_trigger),

		// NickServ name mappings
		TF(NS_HELP, ns_help),
		TF(NS_CIDENTIFY, ns_cidentify),
		TF(NS_IDENTIFY, ns_identify),
		TBL(NS_XIDENTIFY),
		TBL(NS_LOGOUT),
		TF(NS_REGISTER, ns_register),
		TF(NS_INFO, ns_info),
		TF(NS_GHOST, ns_ghost),
		TF(NS_RECOVER, ns_recover),
		TF(NS_RELEASE, ns_release),
		TF(NS_SET, ns_set),
		TF(NS_DROP, ns_drop),
		TF(NS_ADDMASK, ns_addmask),
		TF(NS_ACCESS, ns_access),
		TF(NS_ACC, ns_acc),
		TF(NS_SETFLAG, ns_setflags),
		TF(NS_SETOP, ns_setopflags),
		TF(NS_MARK, ns_mark),
		TF(NS_BYPASS, ns_bypass),
		TF(NS_BANISH, ns_banish),
		TF(NS_GETPASS, ns_getpass),
		TBL(NSS_GETPASS_XFER),
		TF(NS_GETREALPASS, ns_getrealpass),
		TF(NS_DELETE, ns_delete),
		TF(NS_LIST, ns_list),
		TF(NS_SAVE, ns_save),
		TF(NS_SAVEMEMO, ms_save),
		TF(NS_LOGOFF, ns_logoff),
		TF(NS_SETPASS, ns_setpass),
		TF(NS_SETEMAIL, ns_setemail),

		// OperServ Commands
		TF(OS_HELP, os_help),
		TF(OS_AKILL, os_akill),
		TF(OS_TEMPAKILL, os_tempakill),
#ifdef ENABLE_AHURT
		TBL(OS_AHURT),
#endif
		TF(OS_CLONERULE, os_clonerule),
		TBL(OS_IGNORE),
		TF(OS_MODE, os_mode),
		TF(OS_RAW, os_raw),
		TF(OS_SHUTDOWN, os_shutdown),
		TF(OS_RESET, os_reset),
		TF(OS_REHASH, os_reset),
		TF(OS_JUPE, os_jupe),
		TF(OS_UPTIME, os_uptime),
		TF(OS_TIMERS, os_timers),
		TF(OS_SYNC, os_sync),
		TF(OS_TRIGGER, os_trigger),
		TF(OS_MATCH, os_match),
		TF(OS_CLONESET, os_cloneset),
		TF(OS_REMSRA, os_remsra),
		TF(OS_SETOP, os_setop),
		TF(OS_GRPOP, os_grpop),
		TF(OS_OVERRIDE, os_override),
		TF(OS_STRIKE, os_strike),
		TF(OS_HEAL, os_heal),
		TF(OS_NIXGHOST, os_nixghost),

		// InfoServ Commands
		TF(IS_HELP, is_help),
		TF(IS_READ, is_sendinfo),
		TF(IS_LIST, is_listnews),
		TF(IS_POST, is_postnews),
		TF(IS_DEL, is_delete),
		TF(IS_SAVE, is_save),

		// MemoServ Commands
		TF(MS_HELP, ms_help),
		TF(MS_READ, ms_read),
		TF(MS_LIST, ms_list),
		TF(MS_SEND, ms_send),
		TF(MS_SENDSOP, ms_sendsop),
		TF(MS_DEL, ms_delete),
		TF(MS_DELETE, ms_delete),
		TF(MS_PURGE, ms_clean),
		TF(MS_FORWARD, ms_forward),
		TF(MS_NOMEMO, ms_nomemo),
		TF(MS_UNSEND, ms_unsend),
		TF(MS_MBLOCK, ms_mblock),
		TF(MS_SAVEMEMO, ms_savememo),

		// GameServ Commands
		TF(GS_HELP, gs_help),
		TF(GS_ROLL, gs_roll),
		TF(GS_WW, gs_ww),

		// Services Events
		TBL(CS_SET_FOUNDER),
		TBL(NSE_EXPIRE),
		TBL(CSE_EXPIRE),
		TBL(CSE_IPC),

		{SVC_CMD_NONE, NULL}
	};

	const char *cmd_name(int cmd)
	{
		int i = 0;
		for (i = 0; global_cmd_table[i].name; i++)
			if (global_cmd_table[i].cnum == cmd)
				  return global_cmd_table[i].name;
		  return "??_??";
	}

	parser::parser(const char *service, u_int32_t opflags,
				   service_cmd_t * cmdlist,
				   const char *incmd):theOpflags(opflags),
		theService(service)
	{
		struct service_cmd_t *p;

		for (p = cmdlist; p->cmd != NULL; p++)
		{
			if ((opflags & p->root) != p->root)
				continue;
			if (!(p->flags & CMD_MATCH) && strcasecmp(p->cmd, incmd) == 0)
				break;
			else if ((p->flags & CMD_MATCH) && match(p->cmd, incmd) == 0)
				break;
		}
		if (!p->cmd)
		{
			for (p = cmdlist; p->cmd != NULL; p++)
				if (!(p->flags & CMD_MATCH)
					&& strcasecmp(p->cmd, incmd) == 0) break;
				else if ((p->flags & CMD_MATCH)
						 && match(p->cmd, incmd) == 0) break;
		}
		theCmd = p->cmd ? p : NULL;
		return;
	}

	cmd_return parser::run(UserList * nick, char **args, int numargs)
	{
		cmd_return retval;
		bool fLog = false;

		if (!theCmd)
			  return RET_FAIL;
		if ((theCmd->flags & CMD_REG) &&
#ifdef REQ_EMAIL
		 (!nick->reg || !(nick->reg->flags & NACTIVE) ))
#else
		 !nick->reg)
#endif
		{
			if (nick->reg)
				sSend
					(":%s NOTICE %s :Your nickname must be activated to use this command.",
					 theService, nick->nick);
			else
				sSend
					(":%s NOTICE %s :Your nickname must be registered to use this command.",
					 theService, nick->nick);
			return RET_EFAULT;
		}
		if ((theCmd->flags & CMD_IDENT)
			&& (!nick->reg || !isIdentified(nick, nick->reg)))
		{
			sSend(":%s NOTICE %s :You must identify to use this command.",
				  theService, nick->nick);
			sSend
				(":%s NOTICE %s :Type \2/msg %s HELP IDENTIFY\2 for more information.",
				 theService, nick->nick, NickServ);
			return RET_EFAULT;
		}
#ifdef ENABLE_AHURT
		if (!(theCmd->flags & CMD_AHURT) && (nick->oflags & NISAHURT)
			&& !(nick->oflags & NISOPER)) {
			sSend
				(":%s NOTICE %s :You are AutoHurt and can use services only to "
				 "identify to a registered nickname.", theService,
				 nick->nick);
			sSend(":%s NOTICE %s :Type '/msg %s help' and/or "
				  "'/msg %s help identify' for more information.",
				  NickServ, nick->nick, NickServ, NickServ);
			return RET_EFAULT;
		}
#endif

		if ((theOpflags & theCmd->root) == theCmd->root)
			retval = (*theCmd->func) (nick, args, numargs);
		else {
			if (!(theOpflags & OROOT) && (theCmd->root & OROOT))
				sSend(":%s NOTICE %s "
					  ":You must be a services root to use this command.",
					  theService, nick->nick);
			else
				sSend(":%s NOTICE %s "
					  ":You do not have the proper flags to use this command.",
					  theService, nick->nick);
			retval = RET_NOPERM;
		}

		if (retval == RET_KILLED)
			return RET_KILLED;

		if ((theCmd->log & LOG_ALWAYS))
			fLog = true;
		if (((theCmd->log & LOG_OK) || (theCmd->log & LOG_OK_WARN))
			&& (retval == RET_OK || retval == RET_OK_DB)) {
                        fLog = true;
		}
	        if ((theCmd->log & LOG_DB) && (retval == RET_OK_DB))
		        fLog = true;
                        
		if (fLog) {
			unsigned int i;
			char stuff[IRCBUF + 2];

			if (!theCmd->cmd_id) {
				for (i = 0; global_cmd_table[i].name; i++)
					if (global_cmd_table[i].func == theCmd->func)
						break;
				if (global_cmd_table[i].name)
					theCmd->cmd_id = global_cmd_table[i].cnum;
			}

			stuff[0] = '\0';
			if (numargs>1) {
				parse_str(args, numargs, 1, stuff, IRCBUF);
			} else strcpy(stuff, "-");

			operlog->log(nick, theCmd->cmd_id, args[0],
						 (retval == RET_OK_DB
						  || retval == RET_OK) ? LOGF_OK : 0, "%s", stuff);
			sSend(":%s NOTICE %s :This command has been logged.", theService,
				  nick->nick);

			if (theCmd->flood && addFlood(nick, theCmd->flood))
				return RET_KILLED;
		}
		if (retval == RET_OK_DB)
			return RET_OK_DB;
		return RET_OK;
	}
}
