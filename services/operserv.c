/**
 * \file operserv.c
 * \brief Implementation of OperServ
 *
 * OperServ-related procedures and commands that effect changes to 
 * the global state of services and/or the IRC network
 *
 * \wd \taz \greg \mysid
 * \date 1996-2001
 *
 * $Id$
 */

/*
 * Copyright (c) 1996-1997 Chip Norkus
 * Copyright (c) 1997 Max Byrd
 * Copyright (c) 1997 Greg Poma
 * Copyright (c) 1998-2001 James Hess
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

#include "services.h"
#include "nickserv.h"
#include "chanserv.h"
#include "operserv.h"
#include "infoserv.h"
#include "gameserv.h"
#include "clone.h"
#include "hash.h"
#include "log.h"
#include "macro.h"
#include "interp.h"
#include "hash/md5pw.h"

// *INDENT-OFF*

void listCloneAlerts(UserList *);

u_long counterOldCSFmt = 0;
static UserList *os_user_override = (UserList *)0;

/// OperServ command table
interp::service_cmd_t operserv_commands[] = {
  /* string     function     Flags  L */
  { "help",     os_help,     0,		LOG_NO },
  { "autokill", os_akill,    0,		LOG_NO },
  { "akill",    os_akill,    0,		LOG_NO },  /* alias for autokill */
  { "tempakill",os_tempakill,0,		LOG_DB },  /* for temporary autokills */
#ifdef ENABLE_AHURT
  { "ahurt",    os_akill,    0,		LOG_NO },
  { "autohurt", os_akill,    0,		LOG_NO },
#endif
  { "clonerule",os_clonerule,0,		LOG_DB },
  { "ignore",   os_akill,    0,		LOG_NO },
  { "mode",     os_mode,     0,		LOG_OK },
  { "raw",      os_raw,      ORAW,	LOG_OK },
  { "shutdown", os_shutdown, OROOT,	LOG_OK },
  { "reset",    os_reset,    OROOT,	LOG_OK },
  { "rehash",   os_reset,    OROOT,	LOG_OK },  /* alias for reset */
  { "jupe",     os_jupe,     OJUPE,	LOG_OK },
  { "uptime",   os_uptime,   0,		LOG_NO },
  { "timers",   os_timers,   1,		LOG_NO },
  { "sync",     os_sync,     OROOT,	LOG_OK },
  { "trigger",  os_trigger,  0,		LOG_OK },
  { "match",    os_match,    0,		LOG_NO },
  { "cloneset", os_cloneset, 0,		LOG_OK },
#ifndef _DOXYGEN
  { MSG_REMSRA,  os_remsra,  0,	LOG_NO },
#endif
  { "setop",    os_setop,    0,		LOG_NO },
  { "grpop",    os_grpop  ,  0,		LOG_DB },
  { "override", os_override, 0,		LOG_NO },
  { "heal",	os_heal,     0,		LOG_OK },
  { "strike",   os_strike,   0,		LOG_ALWAYS },
  { "nixghost", os_nixghost, 0,		LOG_ALWAYS },
  { NULL,       NULL,        0,		LOG_NO }
};

/// Possible clone flags
const char *cloneset_bits[] = {
  "KILL", "IGNORE", "", "OK", NULL
};

// *INDENT-ON*
/************************************************************************/

/// Set or clear the specified flag from the `bar' bitset
#define SETCLR(var, flag, onoff) do {   \
  if (onoff)                            \
    (var) |= (flag);                    \
  else                                  \
    (var) &= ~(flag);                   \
} while (0)

/**
 * \brief Parse an OperServ message
 * \param tmp Pointer to online user initiating the msesage
 * \param args Args of the message, where args[0] is the command and
 *        the extra parameters follow
 * \param numargs Highest index in the args[] array passed plus 1.
 *        so args[numargs - 1] is the highest index that can be safely
 *        accessed.
 */
void sendToOperServ(UserList * nick, char **args, int numargs)
{
	char *from = nick->nick;
	interp::parser * cmd;

	if (numargs < 1) {
		sSend(":%s NOTICE %s :huh?", OperServ, from);
		return;
	}

	if (!isOper(nick)) {
		sSend(":%s NOTICE %s "
			  ":You must be an IRC Operator to use this service.",
			  OperServ, from);
		return;
	}


	cmd =
		new interp::parser(OperServ, getOpFlags(nick), operserv_commands,
						   args[0]);
	if (!cmd)
		return;
	switch (cmd->run(nick, args, numargs)) {
	default:
		break;
	case RET_FAIL:
		sSend(":%s NOTICE %s :Unknown command %s.\r\n"
			  ":%s NOTICE %s :Please try /msg %s HELP", OperServ, from,
			  args[0], OperServ, from, OperServ);
		break;
	}
	delete cmd;
	return;
}

/************************************************************************/

/// Is the specified user using /OS override?
int userOverriding(UserList * nick)
{
	return (os_user_override == nick);
}

/************************************************************************/


/**
 * \oscmd Help
 * \syntax Help [\<topic>]
 * \plr View OperServ online help
 */
OCMD(os_help)
{
	help(nick->nick, OperServ, args, numargs);
	return RET_OK;
}


/************************************************************************/

/**
 * \oscmd Setop
 * \plr See Setopflags in #ns_commands, Setop is actually a nick command,
 * even though its main interface is through OperServ
 */
OCMD(os_setop)
{
	sendToNickServ(nick, args, numargs);
	return RET_OK;
}

/************************************************************************/
/**
 * \oscmd        Autokill   (or akill)
 * \par Command: Autohurt   (or ahurt)
 * \par Command: Ignore
 * \plr
 *  - Autokill: the network ban list
 *  - Autohurt: the network "weak" ban list
 *  - Ignore: the services ignore list
 *
 * \plr \e Command ::= { \e Autokill, \e Autohurt, \e Ignore }
 * \syntax \e Command
 * \plr Show entries in the list
 * \plr Available to all opers.  All other usages require a services
 *      access flag.
 * \plr Services access flags:
 *       - \e Flag name, Full access, oper access
 *       - Autokill - +K, +k
 *       - Autohurt - +H, n/a
 *       - Ignore - +I, n/a
 *
 * \syntax \e Command - \e pattern
 * \plr Delete an entry from the list.
 * \plr Must be the creator of the entry or have the full access flag
 *      to the list.
 *
 * \syntax \e Command \e duration \e pattern \e reason
 * \plr Add an entry to the list.
 * \plr Must have the "oper" or "full" access permission to the list
 * \plr Duration may not exceed (3) hours and mask cannot be site-wide
 *      without full access.
 * \plr A duration of "0" indicates an entry with no expiration period.
 */
/**
 * Autokill command - for editing the network ban (akill) list 
 *
 * Altered so that AHURTs and Services ignores, akills all fall under
 * this one command instead of having 3 copy-pasted versions of this
 * function that just use different names and list item flags. -Mysid
 */
OCMD(os_akill)
{
	int i, t, count, akill_type, has_prim = 0;
	interp::services_cmd_id log_type = OS_AKILL;
	char akreason[IRCBUF];
	const char *listProper, *p;
	flag_t permPrim, perm2;
	char *from = nick->nick;
#       if AKREASON_LEN >= IRCBUF
#          error AKREASON_LEN cannot be >= IRCBUF!
           *;
#       endif

	permPrim = perm2 = OROOT;

	if (numargs < 1)
		return RET_EFAULT;

	/*
	 * First figure out which command we are, will later simplify this
	 * by introducing the notion of a 'sub command' into the parser
	 */

	if (!str_cmp(args[0], "akill") || !str_cmp(args[0], "autokill")) 
	{
		permPrim = OAKILL;
		perm2 = ORAKILL;
		akill_type = A_AKILL;
		log_type = OS_AKILL;
	}
	else if (!str_cmp(args[0], "ahurt") || !str_cmp(args[0], "autohurt"))
	{
		permPrim = OAHURT;
		akill_type = A_AHURT;
		log_type = OS_AHURT;
	}
	else if (!str_cmp(args[0], "ignore"))
	{
		permPrim = OIGNORE;
		akill_type = A_IGNORE;
		log_type = OS_IGNORE;
	}
	else {
		sSend(":%s NOTICE %s :Internal Error (args[0] = %s)",
		      OperServ, from, args[0]);
		sSend(":%s GLOBOPS :os_akill: Internal Error (args[0] = %s)",
		      OperServ, args[0]);
		return RET_FAIL;
	}

	// Store the proper name of the list being edited

	listProper = aktype_str(akill_type, 0);

	/*
	 * No subcommand means list, and so does "list"
	 */
	if ((numargs < 2) || (!str_cmp(args[1], "list"))) {
		if (!opFlagged(nick, OVERRIDE))
			listAkills(from, akill_type);
		else
			listAkills(from, 0);
		return RET_OK;
	}

	/*
	 * More than a list of XXX requires more permissions.
	 */
	if (!isServop(nick) && !isRoot(nick) && !opFlagged(nick, permPrim)
	    && !opFlagged(nick, perm2)) {
		sSend(":%s NOTICE %s :Permission denied.", OperServ, from);

		return RET_NOPERM;
	}
	else if (isRoot(nick) || isServop(nick) || opFlagged(nick, permPrim))
	{
		has_prim = 1;
	}

	/*
	 * delete a nickname
	 */
	if (args[1][0] == '-' || !strcasecmp(args[1], "del")) {
		if (args[1][0] != '-' || args[1][1] == '\0')
		{
			if ((numargs < 3) || !strchr(args[2], '!')
				|| !strchr(args[2], '@')) 
			{
				PutError(OperServ, nick, ERR_AKILLSYNTAX_1ARG, args[0], 0, 0);
				PutReply(OperServ, nick, RPL_AKILLHELP_2ARG, OperServ, listProper, 0);
				return RET_SYNTAX;
			}
			if (!removeAkillType(from, args[2], akill_type,
			        has_prim == 0 ? 1 : 0))
				operlog->log(nick, log_type, args[2], LOGF_OFF);
		}
		else {
			if ((numargs != 2) || !strchr(args[1], '!')
				|| !strchr(args[1], '@')) 
			{
				PutError(OperServ, nick, ERR_AKILLSYNTAX_1ARG, args[0], 0, 0);
				PutReply(OperServ, nick, RPL_AKILLHELP_2ARG, OperServ, listProper, 0);
				return RET_SYNTAX;
			}

			// operserv override ignore -<blah> for example
			// can remove 'bad items', for example
			if (!opFlagged(nick, OVERRIDE)) {
				if (!removeAkillType(from, args[1]+1, akill_type, has_prim == 0 ? 1 : 0))
					operlog->log(nick, log_type, args[1]+1, LOGF_OFF);
			}
			else {
				if (!removeAkill(from, args[1]+1))
					operlog->log(nick, log_type, args[1]+1, LOGF_OFF);
			}
		}

		return RET_OK_DB;
	}

	/*
	 * Must be an add.  If it has less than 4 arguments is is invalid.
	 * Also, if the second argument doesn't have a @ and a ! in it,
	 * it is also invalid.
	 */
	if ((numargs < 4)
		|| (strchr(args[2], '!') == NULL)
		|| (strchr(args[2], '@') == NULL)
		|| match("*?!?*@?*", args[2])) {

		PutError(OperServ, nick, ERR_AKILLSYNTAX_1ARG, args[0], 0, 0);
		PutReply(OperServ, nick, RPL_AKILLHELP_2ARG, OperServ, listProper, 0);
		return RET_SYNTAX;
	}

	if (((p = strchr(args[2], '!')) && strchr(p+1, '!')) ||
 	    ((p = strchr(args[2], '%')) && strchr(p+1, '%')) ||
 	    (strchr(args[2], '@') < strchr(args[2], '!')) ||
	    (((p = strchr(args[2], '@')) && strchr(p+1, '@')))) {
		sSend(":%s NOTICE %s :Your specified pattern \2%s\2 does not "
		      "seem to be well-formed.",
		       OperServ, from, args[2]);
		return RET_FAIL;
	}

	/*
	 * find out how long this will last
	 */
	if (!str_cmp(args[1], "add") || !str_cmp(args[1], "forever"))
		t = 0;
	else if (*args[1] == '-' || isdigit(*args[1]))
		t = atoi(args[1]) * 3600;
	else {
		PutError(OperServ, nick, ERR_AKILLSYNTAX_1ARG, args[0], 0, 0);
		PutReply(OperServ, nick, RPL_AKILLHELP_2ARG, OperServ, listProper, 0);
		return RET_SYNTAX;
	}

	if (!match(args[2], "...!..@...com")
		|| !match(args[2], "...!..@...net")
		|| !match(args[2], "...!..@...edu")
		|| !match(args[2], "...!..@...org")) {
		sSend(":%s NOTICE %s :That %s is too broad.", OperServ,
			  from, listProper);
		sSend(":%s GLOBOPS :%s attempts to set %s for %s", OperServ,
			  from, listProper, args[2]);
		return RET_NOPERM;
	}
	if (!opFlagged(nick, permPrim)) {
		char nick[MAXBUF], user[MAXBUF], host[MAXBUF];
		if (t > (10800 * 1) || !t)
			t = 10800;
		sscanf(args[2], "%s!%s@%s", nick, user, host);

		if (strlen(args[2]) > 255 || strlen(nick) > 2*NICKLEN ||
                     strlen(user) > 2*USERLEN || strlen(host) > 2*HOSTLEN
                    || (strlen(nick)+strlen(user)+strlen(host)) > (NICKLEN+USERLEN+HOSTLEN+2)) {
			sSend(":%s NOTICE %s :Pattern too long.", OperServ, from);
			return RET_SYNTAX;
		}

		if ((strchr(host, '*') || strchr(host, '?'))
			&& (strchr(user, '*') || strchr(user, '?'))
			&& (strchr(nick, '*') || strchr(nick, '?'))) {
			sSend
				(":%s NOTICE %s :Your %s access is restricted: you cannot wildcard more than one of the 2 parts of the mask (nick, user, or host).",
				 OperServ, from, listProper);
			return RET_NOPERM;
		}
	}

	strncpyzt(akreason, args[3], AKREASON_LEN);
	count = strlen(akreason);

	for (i = 4; i < numargs; i++) {
		if ((AKREASON_LEN - count) <= 0)
			break;
		count +=
			snprintf(akreason + count, AKREASON_LEN - count, " %s",
					 args[i]);
	}
	sSend(":%s NOTICE %s :Adding %s for %s for %d hours [%s]", OperServ,
		  from, listProper, args[2], t / 3600, akreason);
	if (!addakill(t, args[2], from, akill_type, akreason))
		operlog->log(nick, log_type, args[2], LOGF_ON,
					 "%d: %s", t, akreason);
	return RET_OK_DB;
}

/************************************************************************/

/**
 * \oscmd Tempakill
 * \syntax Tempakill \e nick
 * \syntax Tempakill \e hostname
 * \plr Sets a one hour akill for the target host/nick (no wildcards).
 *      Available to any globally opered connection.
 */
OCMD(os_tempakill)
{
	char *from = nick->nick;
	char akreason[IRCBUF];
	int i, count;
	UserList *user;
	char addr[128];

	if (numargs < 3) {
		sSend(":%s NOTICE %s :Invalid syntax.  Try /msg %s help tempakill",
			  OperServ, from, OperServ);

		return RET_SYNTAX;
	}

	if (strchr(args[1], '*') || strchr(args[1], '?')) {
		sSend(":%s NOTICE %s :No wildcards are allowed in a tempakill.",
			  OperServ, from);

		return RET_EFAULT;
	}

	if (strchr(args[1], '!') || strchr(args[1], '@')) {
		sSend
			(":%s NOTICE %s :TempAKill should be placed on a nick or hostname, not a full mask.",
			 OperServ, from);

		return RET_EFAULT;
	}

	if (strchr(args[1], '.'))
		snprintf(addr, 128, "*!*@%s", args[1]);	/* they gave an address, not a nick */
	else {
		user = getNickData(args[1]);	/* they gave what we assume is a nickname */
		if (!user) {
			sSend(":%s NOTICE %s :No one by that nick is online.",
				  OperServ, from);

			return RET_EFAULT;
		}
		snprintf(addr, 128, "*!*@%s", user->host);
	}

	sSend(":%s NOTICE %s :Adding a temp akill for %s for 1 hour.",
		  OperServ, from, addr);

	strncpyzt(akreason, args[2], AKREASON_LEN);
	count = strlen(akreason);

	for (i = 3; i < numargs; i++) {
		if ((AKREASON_LEN - count) <= 0)
			break;
		count +=
			snprintf(akreason + count, AKREASON_LEN - count, "%s%s",
					 i != 3 ? " " : "", args[i]);
	}
	addakill(3600, addr, from, A_AKILL, akreason);
	return RET_OK_DB;
}


/************************************************************************/

/**
 * \oscmd Mode
 * \syntax Mode \e channel \e flags
 * \plr Requires the +o services access flag.
 */
OCMD(os_mode)
{
	char *from = nick->nick;
	char modeString[IRCBUF];
	int on;
	int onarg;
	int i;
	ChanList *tmp;
	cBanList *tmpban;
	cNickList *tmpnick;


	if (numargs < 3) {
		sSend(":%s NOTICE %s :You must specify a channel and mode.",
			  OperServ, from);

		return RET_SYNTAX;
	}

	tmp = getChanData(args[1]);
	on = 1;
	onarg = 3;

	if (!tmp) {
		sSend("%s NOTICE %s :No such channel.", OperServ, from);

		return RET_EFAULT;
	}

	for (i = 0; args[2][i]; i++) {
		switch (args[2][i]) {
		case '+':
			on = 1;
			break;
		case '-':
			on = 0;
			break;

		case 'b':
			if (on) {
				tmpban = (cBanList *) oalloc(sizeof(cBanList));

				/// String truncates here
				if (strlen(args[onarg]) > sizeof(tmpban->ban))
					args[onarg][sizeof(tmpban->ban) - 1] = '\0';
				strcpy(tmpban->ban, args[onarg]);
				addChanBan(tmp, tmpban);
			} else {
				tmpban = getChanBan(tmp, args[onarg]);
				if (tmpban)
					delChanBan(tmp, tmpban);
			}

			onarg++;
			break;

		case 'i':
			SETCLR(tmp->modes, PM_I, on);
			break;

		case 'l':
			SETCLR(tmp->modes, PM_L, on);
			if ((on == 0) && tmp->reg)
				tmp->reg->limit = 0;
			onarg++;
			break;

		case 'k':
			SETCLR(tmp->modes, PM_K, on);
			if ((on == 0) && tmp->reg)
				tmp->reg->key[0] = 0;
			onarg++;
			break;

		case 'm':
			SETCLR(tmp->modes, PM_M, on);
			break;

		case 'n':
			SETCLR(tmp->modes, PM_N, on);
			break;

		case 'o':
			tmpnick = getChanUserData(tmp, getNickData(args[onarg]));
			onarg++;
			if (tmpnick == NULL)
				continue;

			SETCLR(tmpnick->op, CHANOP, on);
			break;

		case 'p':
			SETCLR(tmp->modes, PM_P, on);
			break;

		case 's':
			SETCLR(tmp->modes, PM_S, on);
			break;

		case 't':
			SETCLR(tmp->modes, PM_T, on);
			break;

		case 'v':
			tmpnick = getChanUserData(tmp, getNickData(args[onarg]));
			onarg++;
			if (tmpnick == NULL)
				continue;

			SETCLR(tmpnick->op, CHANVOICE, on);
			break;
		}
	}

	modeString[0] = 0;

	for (i = 2; i < onarg; i++) {
		strcat(modeString, args[i]);
		strcat(modeString, " ");
	}

	modeString[strlen(modeString) - 1] = 0;
	sSend(":%s MODE %s %s", ChanServ, args[1], modeString);

	sSend(":%s GLOBOPS :%s MODE %s (%s)", OperServ, from, args[1], modeString);
	return RET_OK;
}

/************************************************************************/

/* RAW - cause services to send a RAW line, restricted to root because it */
/*       has security implications.                                       */

/**
 * \oscmd Raw
 * \plr      Send Raw IRC messages
 * \internal sra command.
 */
OCMD(os_raw)
{
	char *from = nick->nick;
	char msg[IRCBUF];
	int i;

	strncpyzt(msg, args[1], sizeof(msg));

	for (i = 2; i < numargs; i++) {
		strcat(msg, " ");
		strcat(msg, args[i]);
	}

#ifdef GLOBOPS_ON_RAW
	sSend(":%s GLOBOPS :%s used RAW: %s", OperServ, from, msg);
#endif

	sSend("%s", msg);
	return RET_OK;
}

/************************************************************************/

/* Shutdown - terminate services                                          */

/**
 * \oscmd      *Shutdown
 * \plr        Shutdown services
 * \plr        SRA command
 */
OCMD(os_shutdown)
{
	char *from = nick->nick;
	char msg[IRCBUF];
	int i;

	if (numargs > 1) {
		strncpyzt(msg, args[1], sizeof(msg));

		for (i = 2; i < numargs; i++) {
			strcat(msg, " ");
			strcat(msg, args[i]);
		}

		sSend(":%s WALLOPS :!SHUTDOWN! From %s (%s)", myname, from, msg);
	} else
		sSend(":%s WALLOPS :!SHUTDOWN! From %s", myname, from);

	sshutdown(0);
	return RET_OK;
}

/************************************************************************/

/* Rehash - reload services' config                                       */

/**
 * \oscmd Reset
 * SRA command.
 */

/// Rehash for services.  Reloads the configuration.
OCMD(os_reset)
{
	char *from = nick->nick;
	char msg[IRCBUF];
	int i;

	if (numargs > 1) {
		strncpyzt(msg, args[1], sizeof(msg));

		for (i = 2; i < numargs; i++) {
			strcat(msg, " ");
			strcat(msg, args[i]);
		}

		sSend(":%s WALLOPS :Services has been rehashed by %s (%s)", myname,
			  from, msg);
	} else
		sSend(":%s WALLOPS :Services has been rehashed by %s", myname,
			  from);

	readConf();
	flush_help_cache();
	return RET_OK;
}

/************************************************************************/

/* Jupe - jupe a server                                                   */
/* jupe is restricted to root because it can inadvertently cause services */
/* to be dropped...                                                       */

/**
 * \oscmd Jupe
 * \plr Jupiter a server.
 */
OCMD(os_jupe)
{
	char *from = nick->nick;
	time_t now = time(NULL);
	char msg[IRCBUF];
	int i;

	if (numargs < 2) {
		sSend(":%s NOTICE %s :Must specify a server to jupe", OperServ,
			  from);
		return RET_SYNTAX;
	}

	if (numargs > 2) {
		strncpyzt(msg, args[2], sizeof(msg));

		for (i = 2; i < numargs; i++) {
			strcat(msg, " ");
			strcat(msg, args[i]);
		}

		sSend("SERVER %s 2 :Jupitered server [%s:%s] (%s)", args[1], from,
			  ctime(&now), msg);
		sSend(":%s GLOBOPS :Server %s Jupitered by %s at %s (%s)", myname,
			  args[1], from, ctime(&now), msg);
	} else {
		sSend("SERVER %s 2 :Jupitered server [%s:%s]", args[1], from,
			  ctime(&now));
		sSend(":%s GLOBOPS :Server %s Jupitered by %s at %s", myname,
			  args[1], from, ctime(&now));
	}
	return RET_OK;
}

/************************************************************************/

/**
 * \oscmd Uptime
 * \syntax Uptime
 * report the amount of time services' have been online since last
 * startup.
 */
/* ARGSUSED1 */
OCMD(os_uptime)
{
	char *from = nick->nick;

	time_t now;
	int days, hours, mins, seconds;

	now = time(NULL);
	now -= startup;
	days = (now / (24 * 3600));
	now %= (24 * 3600);
	hours = (now / 3600);
	now %= 3600;
	mins = (now / 60);
	now %= 60;
	seconds = now;

	sSend(":%s NOTICE %s :Services has been online for:\r\n"
		  ":%s NOTICE %s :%i days, %i hours, %i minutes, and %i seconds",
		  OperServ, from, OperServ, from, days, hours, mins, seconds);
	sSend(":%s NOTICE %s :(%ld chanserv commands interpreted by hack)",
		  OperServ, from, counterOldCSFmt);
	return RET_OK;
}

/************************************************************************/

/**
 * \oscmd Timers
 * \internal DEBUG: Timers - list services' internal timers
 */
/* ARGSUSED1 */
OCMD(os_timers)
{
	char *from = nick->nick;

	dumptimer(from);
	return RET_OK;
}

/************************************************************************/

/**
 * \oscmd Sync
 * \internal sra command.
 */
/// DEBUG: Sync the disk copy of services databases                
OCMD(os_sync)
{
	void writeServicesTotals();

#ifdef GLOBOP_ON_SYNC
	char *from = nick->nick;
#endif

#ifdef GLOBOP_ON_SYNC
	sSend(":%s GLOBOP :Saving all databases (%s)", OperServ, from);
#endif
	strcpy(args[0], "save");
	sendToNickServ(nick, args, numargs);
	sendToChanServ(nick, args, numargs);
	sendToInfoServ(nick, args, numargs);
	strcpy(args[0], "savememo");
	sendToNickServ(nick, args, numargs);
	saveakills();
	writeServicesTotals();

	return RET_OK_DB;
}


/************************************************************************/

/**
 * \oscmd clonerule
 * \syntax Clonerule LIST
 * \syntax Clonerule INFO \e rule
 * \syntax Clonerule ADD \e trigger-pattern
 * \syntax Clonerule INSERT \e trigger-pattern \e index
 * \syntax Clonerule DEL \e rule
 * \syntax Clonerule SET \e rule \e variable \e value
 *
 * \plr Maintain clone trigger rules
 */
OCMD(os_clonerule)
{
	extern void saveTriggerData(void);
	char *from = nick->nick, *pCheck;
	int c = 0, ct = 0, minargs = 3;
	CloneRule *rule, orig;
	typedef enum
	{ CLONEARG_NONE, CLONEARG_LIST, CLONEARG_INFO, CLONEARG_ADD,
		CLONEARG_INSERT, CLONEARG_SET, CLONEARG_DEL
	}
	clonerule_cmds;

	if (numargs > 1 && *args[1])
		if (!strcasecmp(args[1], "LIST")) {
			c = CLONEARG_LIST;
			--minargs;
		} else if (!strcasecmp(args[1], "INFO")) {
			c = CLONEARG_INFO;
		} else if (!strcasecmp(args[1], "ADD")) {
			c = CLONEARG_ADD;
		} else if (!strcasecmp(args[1], "INSERT")) {
			c = CLONEARG_INSERT;
		} else if (!strcasecmp(args[1], "SET")) {
			c = CLONEARG_SET;
			++minargs;
		} else if (!strcasecmp(args[1], "DEL")) {
			c = CLONEARG_DEL;
		}

	if (numargs < minargs || c == CLONEARG_NONE) {
		sSend(":%s NOTICE %s :Syntax: CLONERULE <list|info|add|del|set>",
			  OperServ, from);
		sSend(":%s NOTICE %s :        CLONERULE list [mask]", OperServ,
			  from);
		sSend(":%s NOTICE %s :        CLONERULE info <rule#|mask>",
			  OperServ, from);
		if (opFlagged(nick, OOPER | OCLONE) || isRoot(nick)) {
			sSend
				(":%s NOTICE %s :        CLONERULE add <mask> [host trigger]",
				 OperServ, from);
			sSend
				(":%s NOTICE %s :        CLONERULE insert <mask> [before #]",
				 OperServ, from);
			sSend
				(":%s NOTICE %s :        CLONERULE set <rule#|mask> FLAGS <flag>",
				 OperServ, from);
			sSend
				(":%s NOTICE %s :        CLONERULE set <rule#|mask> UTRIGGER <number|0>",
				 OperServ, from);
			sSend
				(":%s NOTICE %s :        CLONERULE set <rule#|mask> HTRIGGER <number|0>",
				 OperServ, from);
			sSend
				(":%s NOTICE %s :        CLONERULE set <rule#|mask> WARNMSG <message>",
				 OperServ, from);
			sSend(":%s NOTICE %s :        CLONERULE del <rule#|mask>",
				  OperServ, from);
		}
		return RET_SYNTAX;
	}

	if (!opFlagged(nick, OOPER | OCLONE) && !isRoot(nick)
		&& (c > CLONEARG_INFO)) {
		sSend(":%s NOTICE %s :Permission denied.", OperServ, from);
		return RET_NOPERM;
	}

	switch (c) {
		/* cryptic code block starts here */
	case CLONEARG_INFO:
	case CLONEARG_LIST:
		if (c == CLONEARG_LIST)
			sSend(":%s NOTICE %s :***** \2Clone Trigger rules\2 *****",
				  OperServ, from);
		for (rule = first_crule, ct = 1; rule; rule = rule->next, ++ct)
			/* show an entry if the 'list' command had no extra args 
			 * or if the argument IS the current mask */
			if ((numargs < 3)
				|| ((isdigit(*args[2]) && atoi(args[2]) == ct)
					|| !strcasecmp(args[2], rule->mask))) {
				if (c == CLONEARG_LIST)
					sSend(":%s NOTICE %s :\2%3d\2.  %-45s %3d %3d",
						  OperServ, from, ct, rule->mask, rule->trigger,
						  rule->utrigger);
				else {
					sSend
						(":%s NOTICE %s :***** \2Persistent clone trigger rule information\2 *****",
						 OperServ, from);
					sSend(":%s NOTICE %s :Mask: \2%s\2", OperServ, from,
						  rule->mask);
					sSend
						(":%s NOTICE %s :Username trigger level: \2%3d\2, Host Trigger level: \2%3d\2",
						 OperServ, from, rule->utrigger, rule->trigger);
					sSend(":%s NOTICE %s :Flags: \2%s\2", OperServ, from,
						  flagstring(rule->flags, cloneset_bits));
					sSend(":%s NOTICE %s :Kill Message: \2%s\2", OperServ,
						  from,
						  rule->kill_msg ? rule->kill_msg : "(default)");
					sSend(":%s NOTICE %s :Warning Message: \2%s\2",
						  OperServ, from,
						  rule->warn_msg ? rule->warn_msg : "(none)");
					sSend(":%s NOTICE %s :***** \2End of info\2 *****",
						  OperServ, from);
				}
			}
		if (c == CLONEARG_LIST)
			sSend(":%s NOTICE %s :***** \2End of list\2 *****", OperServ,
				  from);
		return RET_OK;
		return RET_OK;
	case CLONEARG_ADD:
	case CLONEARG_INSERT:
		if (index(args[2], '!') || !index(args[2], '@')
			|| !index(args[2], '.')) {
			sSend(":%s NOTICE %s :\2%s\2: Invalid mask.", OperServ, from,
				  args[2]);
			return RET_EFAULT;
		}
		if (GetCrule(args[2])) {
			sSend(":%s NOTICE %s :There's already a rule for that mask.",
				  OperServ, from);
			return RET_FAIL;
		}
		if (!(rule = NewCrule()))
			return RET_MEMORY;
		orig = *rule;
		rule->kill_msg = NULL;
		strncpyzt(rule->mask, args[2], sizeof(rule->mask));
		rule->trigger = 0;
		if (c == CLONEARG_ADD)
			if (numargs > 3 && *args[3] && isdigit(*args[3]))
				rule->trigger = rule->utrigger = atoi(args[3]);
		if (!rule->trigger) {	/* keep defaults by default */
			rule->utrigger = 0;
			rule->trigger = 0;
		}
		rule->flags = DEFCLONEFLAGS;
		if (c == CLONEARG_ADD || numargs < 4)
			AddCrule(rule, -1);
		else {
			AddCrule(rule, atoi(args[3]));
			sSend(":%s NOTICE %s :--- inserting at point %d", OperServ,
				  from, atoi(args[3]));
		}
		sSend(":%s GLOBOPS :%s adding trigger rule for \2%s\2", OperServ,
			  from, args[2]);
		UpdateCrule(orig, rule);
		saveTriggerData();
		return RET_OK_DB;
	case CLONEARG_SET:
		if (!(rule = GetCrule(args[2]))) {
			sSend(":%s NOTICE %s :\2%s\2: No such trigger default.",
				  OperServ, from, args[2]);
			return RET_NOTARGET;
		}
		if (numargs < 5) {
			sSend(":%s NOTICE %s :Set it to what?", OperServ, from);
			return RET_SYNTAX;
		}

		orig = *rule;

		if (!strcasecmp(args[3], "flags")) {
			if ((ct = flagbit(args[4], cloneset_bits)) < 0) {
				sSend(":%s NOTICE %s :\2%s\2: No such cloneset flag.",
					  OperServ, from, args[4]);
				return RET_EFAULT;
			}
			sSend(":%s NOTICE %s :\2%s\2 flag %s \2%s\2.", OperServ, from,
				  args[4],
				  (rule->flags & (1 << ct)) ? "removed from" : "added to",
				  rule->mask);
			sSend(":%s GLOBOPS :%s %s %s flag on trigger rule for \2%s\2",
				  OperServ, from,
				  rule->flags & (1 << ct) ? "cleared" : "set", args[4],
				  rule->mask);
			rule->flags ^= (1 << ct);
			UpdateCrule(orig, rule);
		} else if (!strcasecmp(args[3], "htrigger")) {
			if (!isdigit(*args[4])) {
				sSend(":%s NOTICE %s :Invalid host trigger level.",
					  OperServ, from);
				return RET_EFAULT;
			}


			if (!(pCheck = strchr(args[3], '@')) && atoi(args[4])) {
				sSend(":%s NOTICE %s :Cannot set host trigger "
				      "rewrite unless rule contains a @.",
					  OperServ, from);
				return RET_EFAULT;
			}

			if (pCheck && (strchr(pCheck, '*') || strchr(pCheck, '?'))
			    && atoi(args[4]) 
			    && !opFlagged(nick, OROOT | OVERRIDE)) 
			{
				sSend(":%s NOTICE %s :Cannot set host trigger "
				      "rewrite with a wildcarded host.",
					  OperServ, from);
				return RET_EFAULT;
			}

			sSend
				(":%s GLOBOPS :%s set host trigger for rule of \2%s\2 %d->%d",
				 OperServ, from, rule->mask, rule->trigger, atoi(args[4]));
			rule->trigger = atoi(args[4]);
			sSend(":%s NOTICE %s :Trigger level is now %d.", OperServ,
				  from, rule->trigger);
			UpdateCrule(orig, rule);
		} else if (!strcasecmp(args[3], "utrigger")) {
			if (!isdigit(*args[4])) {
				sSend(":%s NOTICE %s :Invalid user trigger level.",
					  OperServ, from);
				return RET_EFAULT;
			}
			sSend
				(":%s GLOBOPS :%s set user trigger for rule of \2%s\2 %d->%d",
				 OperServ, from, rule->mask, rule->utrigger,
				 atoi(args[4]));
			rule->utrigger = atoi(args[4]);
			sSend(":%s NOTICE %s :User trigger level is now %d.", OperServ,
				  from, rule->utrigger);
			UpdateCrule(orig, rule);
		} else if (!strcasecmp(args[3], "kill")
				   || !strcasecmp(args[3], "killmsg")) {
			char tmpbuf[8192] = "";
			if (args[4][0] == '\0' || !strcasecmp(args[4], "none")) {
				if (rule->kill_msg)
					FREE(rule->kill_msg);
				rule->kill_msg = NULL;
				sSend(":%s NOTICE %s :Kill message cleared.", OperServ,
					  from);
			} else {
				parse_str(args, numargs, 4, tmpbuf, IRCBUF);
				if (strlen(tmpbuf) > MEMOLEN) {
					sSend
						(":%s NOTICE %s :Kill message cannot exceed %d characters.",
						 OperServ, from, MEMOLEN);
					return RET_EFAULT;
				}
				if (rule->kill_msg)
					FREE(rule->kill_msg);
				rule->kill_msg = strdup(tmpbuf);
				if (!rule->kill_msg) {
					logDump(corelog,
							"Error allocating (%d) bytes of memory for clonerule kill message. (%s!%s@%s)",
							strlen(tmpbuf), from, nick->user, nick->host);
					sSend
						(":%s GLOBOPS :Error allocating (%d) bytes of memory for clonerule kill message. (%s!%s@%s)",
						 NickServ, strlen(tmpbuf), from, nick->user,
						 nick->host);
					return RET_MEMORY;
				}
				sSend(":%s NOTICE %s :Kill message set.", OperServ, from);
			}
		} else if (!strcasecmp(args[3], "warn")
				   || !strcasecmp(args[3], "warnmsg")) {
			char tmpbuf[8192] = "";
			if (args[4][0] == '\0' || !strcasecmp(args[4], "none")) {
				if (rule->warn_msg)
					FREE(rule->warn_msg);
				rule->warn_msg = NULL;
				sSend(":%s NOTICE %s :Warning message cleared.", OperServ,
					  from);
			} else {
				parse_str(args, numargs, 4, tmpbuf, IRCBUF);
				if (strlen(tmpbuf) > MEMOLEN) {
					sSend
						(":%s NOTICE %s :Warning message cannot exceed %d characters.",
						 OperServ, from, MEMOLEN);
					return RET_EFAULT;
				}
				if (rule->warn_msg)
					FREE(rule->warn_msg);
				rule->warn_msg = strdup(tmpbuf);
				if (!rule->warn_msg) {
					logDump(corelog,
							"Error allocating (%d) bytes of memory for clonerule warn message. (%s!%s@%s)",
							strlen(tmpbuf), from, nick->user, nick->host);
					sSend
						(":%s GLOBOPS :Error allocating (%d) bytes of memory for clonerule arn message. (%s!%s@%s)",
						 NickServ, strlen(tmpbuf), from, nick->user,
						 nick->host);
					return RET_MEMORY;
				}
				sSend(":%s NOTICE %s :Warning message set.", OperServ,
					  from);
			}
		} else {
			sSend
				(":%s NOTICE %s :Valid settings are: FLAGS, HTRIGGER, UTRIGGER, KILLMSG, and WARNMSG",
				 OperServ, from);
			sSend(":%s NOTICE %s :Valid flags are: %s", OperServ, from,
				  flagstring(~0, cloneset_bits));
			return RET_OK;
		}
		saveTriggerData();
		return RET_OK_DB;
	case CLONEARG_DEL:
		if (!(rule = GetCrule(args[2]))) {
			sSend(":%s NOTICE %s :\2%s\2: No such trigger default.",
				  OperServ, from, args[2]);
			return RET_NOTARGET;
		}
		RemoveCrule(rule);
		sSend(":%s NOTICE %s :Trigger rule for %s removed.", OperServ,
			  from, rule->mask);
		sSend(":%s GLOBOPS :%s removing trigger rule for \2%s\2", OperServ,
			  from, rule->mask);
		if (rule->kill_msg)
			FREE(rule->kill_msg);
		if (rule->warn_msg)
			FREE(rule->warn_msg);
		FREE(rule);
		saveTriggerData();
		return RET_OK_DB;
	}
	sSend(":%s NOTICE %s :Huh?", OperServ, from);
	return RET_SYNTAX;
}

/************************************************************************/

/**
 * \oscmd Trigger
 * \plr   Set clone triggers
 */
OCMD(os_trigger)
{
	char *from = nick->nick;
	char user[USERLEN];
	char host[HOSTLEN];
	HostClone *hc;
	UserClone *uc;
	int i;

	if (numargs < 2) {
		sSend(":%s NOTICE %s :Default host trigger: \2%i\2", OperServ, from,
			DEFHOSTCLONETRIGGER);
		sSend(":%s NOTICE %s :Default user trigger: \2%i\2", OperServ, from,
			DEFUSERCLONETRIGGER);
		sSend(":%s NOTICE %s :/OPERSERV trigger list' to show "
		      "any outstanding alerts.", OperServ, from);

		return RET_OK;
	}

	if (numargs >= 2 && !str_cmp(args[1], "list")) {
		listCloneAlerts(nick);

		return RET_OK;
	}

	/*
	 * If they specified a user as well as a host handle it right.
	 * If they only asked about a host, just use that one.
	 */
	if (index(args[1], '@')) {
		user[0] = host[0] = '\0';
		sscanf(args[1], "%[^@]@%s", user, host);

		hc = getCloneData(host);
		if (hc != NULL)
			uc = getUserCloneData(hc, user);
		else
			uc = NULL;

		if ((uc == NULL) || (hc == NULL)) {
			sSend(":%s NOTICE %s :No such user@host (%s@%s), ", OperServ,
				  from, user, host);

			return RET_NOTARGET;
		}

		if (numargs < 3) {
			sSend(":%s NOTICE %s :%s@%s has %d/%d clones (total/trigger)",
				  OperServ, from, user, host, uc->clones, uc->trigger);

			return RET_OK;
		}

		i = atoi(args[2]);
		if (i <= 0) {
			sSend(":%s NOTICE %s :Please specify a proper trigger level",
				  OperServ, from);

			return RET_EFAULT;
		}

		uc->trigger = i;
		sSend(":%s GLOBOPS :%s@%s retriggered to %d by %s", OperServ, user,
			  host, i, from);

		return RET_OK_DB;
	}

	/*
	 * fallthrough case, is just a host specified.
	 */
	strncpyzt(host, args[1], HOSTLEN);

	hc = getCloneData(host);
	if (hc == NULL) {
		sSend(":%s NOTICE %s :%s does not exist", OperServ, from, host);

		return RET_NOTARGET;
	}

	if (numargs < 3) {
		sSend(":%s NOTICE %s :%s has %i/%i clones (total/trigger)",
			  OperServ, from, host, hc->clones, hc->trigger);

		return RET_OK;
	}

	i = atoi(args[2]);

	if (i <= 0) {
		sSend(":%s NOTICE %s :Please specify a proper trigger level",
			  OperServ, from);

		return RET_EFAULT;
	}

	hc->trigger = i;
	sSend(":%s GLOBOPS :%s retriggered to %i by %s", OperServ, host, i,
		  from);
	return RET_OK_DB;
}

/************************************************************************/

/**
 * \oscmd Match
 * \plr   Debug entrypoint for testing pattern matching.
 */
/// DEBUG: Match a test string                                             
OCMD(os_match)
{
	char *from = nick->nick;

	if (numargs > 2)
		sSend(":%s NOTICE %s :match(%s, %s) returns %i", OperServ, from,
			  args[1], args[2], match(args[1], args[2]));
	return RET_OK;
}

/************************************************************************/

/**
 * \oscmd Cloneset
 * \syntax Cloneset \e host ("+"|"-") \e flag
 * \plr    Set clone host flags
 * \plr    Flags:
 *         - kill
 *         - ignore
 */
OCMD(os_cloneset)
{
	char *from = nick->nick;
	HostClone *hc;

	if (numargs < 2) {
		sSend(":%s NOTICE %s :You must specify at least an address",
			  OperServ, from);
		return RET_SYNTAX;
	}

	hc = getCloneData(args[1]);
	if (hc == NULL) {
		sSend(":%s NOTICE %s :No such host %s", OperServ, from, args[1]);

		return RET_NOTARGET;
	}

	if (numargs < 3) {
		sSend(":%s NOTICE %s :%s has flags:%s%s.", OperServ, from, args[1],
			  ((hc->flags & CLONE_KILLFLAG) ? " Auto Remove" : ""),
			  ((hc->flags & CLONE_IGNOREFLAG) ? "Ignore all" : ""));
	} else {
		if (!strcasecmp(args[2], "kill"))
			hc->flags |= CLONE_KILLFLAG;
		else if (!strcasecmp(args[2], "-kill"))
			hc->flags &= ~CLONE_KILLFLAG;
		else if (!strcasecmp(args[2], "ok"))
			hc->flags |= CLONE_OK;
		else if (!strcasecmp(args[2], "-ok"))
			hc->flags &= ~CLONE_OK;
		else if (!strcasecmp(args[2], "ignore"))
			hc->flags |= CLONE_IGNOREFLAG;
		else if (!strcasecmp(args[2], "-ignore"))
			hc->flags &= ~CLONE_IGNOREFLAG;
		else {
			sSend(":%s NOTICE %s :Unknown flag %s", OperServ, from,
				  args[2]);
			return RET_EFAULT;
		}

		sSend(":%s GLOBOPS :%s changed cloneflags for %s (now %s%s)",
			  OperServ, from, hc->host,
			  ((hc->flags & CLONE_KILLFLAG) ? " Auto Remove" : ""),
			  ((hc->flags & CLONE_IGNOREFLAG) ? "Ignore all" : ""));
		return RET_OK_DB;
	}
	return RET_OK;
}

/**************************************************************************
 * The existence of this command and its syntax is SRA privileged         *
 * information.                                                           *
 *************************************************************************/
OCMD(os_remsra)
{
	char NickGetEnc(RegNickList *);
	char *from = nick->nick;
	RegNickList *root;

	if (!nick->reg)
		return RET_NOPERM;

	if ((numargs < 3)
		|| (!(nick->reg->opflags & OREMROOT))
		|| !Valid_pw(args[2], nick->reg->password, NickGetEnc(nick->reg))) {
		operlog->log(nick, OS_REMSRA, (numargs > 1 ? args[1] : ""), 0,
					 "failed");
		sSend(":%s NOTICE %s :Unknown command %s.\r\n"
			  ":%s NOTICE %s :Please try /msg %s HELP", OperServ, from,
			  args[0], OperServ, from, OperServ);
		return RET_NOPERM;
	}

	root = getRegNickData(args[1]);
	if (!root)
		return RET_NOPERM;
	delOpData(root);
	root->opflags &= ~(OROOT | OPROT);
	if (root->opflags)
		addOpData(root);
	sSend(":%s NOTICE %s :Rootflags for %s removed", OperServ, from,
		  args[1]);
	operlog->log(nick, OS_REMSRA, (numargs > 1 ? args[1] : ""));
	return RET_OK_DB;
}

/**
 * \oscmd Grpop
 * \deprecated This command is obsoleted by setop.
 */
/**************************************************************************
 * List those with the +g oper flag who can use getrealpass to help users *
 * This is deprecated and will eventually be removed.                     *
 **************************************************************************/
OCMD(os_grpop)
{
	char *from = nick->nick;
	RegNickList *grpop;
	int bucket;

	if (!nick->reg)
		return RET_NOPERM;

	if ((numargs < 2) || (!strcasecmp(args[1], "list"))) {
		sSend(":%s NOTICE %s :Now listing GRP-Ops:", OperServ, from);

		for (bucket = 0; bucket < NICKHASHSIZE; bucket++) {
			for (grpop = LIST_FIRST(&RegNickHash[bucket]); grpop;
				 grpop = LIST_NEXT(grpop, rn_lst))
				if (grpop->opflags & OGRP)
					sSend(":%s NOTICE %s :%s", OperServ, from,
						  grpop->nick);
		}
		return RET_OK;
	}

	if (!isRoot(nick)) {
		sSend(":%s NOTICE %s :Permission denied.", OperServ, from);
		return RET_NOPERM;
	}

	if ((!strcasecmp(args[1], "convert"))) {
		sSend(":%s NOTICE %s :Now converting GRP-Ops to new format.",
			  OperServ, from);

		for (bucket = 0; bucket < NICKHASHSIZE; bucket++) {
			for (grpop = LIST_FIRST(&RegNickHash[bucket]); grpop;
				 grpop = LIST_NEXT(grpop, rn_lst))
				if (grpop->flags & NGRPOP) {
					grpop->flags &= ~NGRPOP;
					grpop->opflags |= OGRP;
					sSend(":%s NOTICE %s :%s", OperServ, from,
						  grpop->nick);
				}
		}
		return RET_OK_DB;
	}

	sSend(":%s NOTICE %s :-- This command is deprecated, use setop.",
		  OperServ, from);

	if (!strcasecmp(args[1], "del")) {
		if (numargs != 3) {
			sSend
				(":%s NOTICE %s :Improper format.  Try /msg %s help grpop",
				 OperServ, from, OperServ);
			return RET_SYNTAX;
		}
		if ((grpop = getRegNickData(args[2])) != NULL) {
			grpop->opflags &= ~OGRP;
			if (!grpop->opflags)
				delOpData(grpop);
			sSend(":%s NOTICE %s :%s no longer flagged GRP-op.", OperServ,
				  from, args[2]);
		} else {
			sSend
				(":%s NOTICE %s :Target nickname, %s, is not registered. No changes made.",
				 OperServ, from, args[2]);
			return RET_NOTARGET;
		}
	} else if (!strcasecmp(args[1], "add")) {
		if (numargs != 3) {
			sSend
				(":%s NOTICE %s :Improper format.  Try /msg %s help grpop",
				 OperServ, from, OperServ);
			return RET_SYNTAX;
		}
		if ((grpop = getRegNickData(args[2])) != 0) {
			if (!grpop->opflags)
				addOpData(grpop);
			grpop->opflags |= OGRP;
			sSend(":%s NOTICE %s :%s flagged GRP-op.", OperServ, from,
				  args[2]);
		} else {
			sSend
				(":%s NOTICE %s :Target nickname, %s, is not registered. No changes made.",
				 OperServ, from, args[2]);
			return RET_NOTARGET;
		}
	}
	return RET_OK_DB;
}

/************************************************************************/

/**
 * \oscmd Override
 * \syntax Override \e service \e command { \e arg ...}
 * \plr   Command to override certain user access restrictions for opers
 *        where they shouldn't automatically be overriden
 */
OCMD(os_override)
{
	UserList *tmp = nick;
	char *orig_cmd, *service;
	char stuff[IRCBUF + 25];
	int i = 0;

	if (!nick)
		return RET_NOPERM;

	if (!isOper(nick) || os_user_override) {
		sSend(":%s NOTICE %s :I don't think so!", ChanServ, nick->nick);
		return RET_INVALID;
	}

	if ((!nick->reg || !(nick->reg->opflags & OACC)) && !isRoot(nick)) {
		sSend(":%s NOTICE %s "
			  ":You do not have the proper flags to use this command.",
			  OperServ, nick->nick);
		return RET_NOPERM;
	}

	if (numargs < 3) {
		sSend
			(":%s NOTICE %s :Override user access restrictions, but for what service and what command?",
			 ChanServ, nick->nick);
		return RET_SYNTAX;
	}

	orig_cmd = args[0];
	service = args[1];
	for (i = 0; i < (numargs - 2); i++)
		args[i] = args[i + 2];
	args[numargs - 1] = NULL;
	numargs -= 2;
	os_user_override = nick;

	parse_str(args, numargs, 0, stuff, IRCBUF);
	operlog->log(nick, OS_OVERRIDE, args[1], LOGF_OK, "%s", stuff);

	if (!strncasecmp(service, OperServ, strlen(OperServ)))
		sendToOperServ(tmp, args, numargs);
	else if (!strncasecmp(service, NickServ, strlen(NickServ)))
		sendToNickServ(tmp, args, numargs);
	else if (!strncasecmp(service, ChanServ, strlen(ChanServ)))
		sendToChanServ(tmp, args, numargs);
	else if (!strncasecmp(service, MemoServ, strlen(MemoServ)))
		sendToMemoServ(tmp, args, numargs);
	else if (!strncasecmp(service, InfoServ, strlen(InfoServ)))
		sendToInfoServ(tmp, args, numargs);
	else if (!strncasecmp(service, GameServ, strlen(GameServ)))
		sendToGameServ(tmp, args, numargs);
	else
		sSend(":%s NOTICE %s :No such service.", OperServ, nick->nick);
	os_user_override = NULL;
	return RET_KILLED;			/* We really don't know if they're still alive or not */
}

/************************************************************************/

/// Test command
OCMD(os_strike)
{
#ifdef USE_SQL
	char *tnick, *p, *mask;
	char query_ln[(2 * (NICKLEN + 1)) + 512];

	if (!isRoot(nick)) {
		sSend
			(":%s NOTICE %s :This is a test command and requires root acess currently.",
			 OperServ, nick->nick);
		return RET_NOPERM;
	}

	if (numargs < 2 || !strchr(args[1], '!') || !strchr(args[1], '@')
		|| !strchr(args[1], '.')) {
		sSend(":%s NOTICE %s :Usage: /operserv strike nick!user@host",
			  OperServ, nick->nick);
		return RET_SYNTAX;
	}

	tnick = args[1];
	p = strchr(args[1], '!');
	*p = '\0';
	mask = p + 1;

	sprintf(query_ln,
			"INSERT into strikes (setby, nick, mask, status, auth_num) "
			"VALUES (\'%s\', \'%s\', \'%s\', 0, %ld);", nick->nick, tnick,
			mask, (time(NULL) / 100000) ^ nick->nick[0]);
	if (PQsendQuery(dbConn, query_ln) < 0) {
		sSend
			(":%s NOTICE %s :Services is busy at the moment, please try again in a bit.",
			 OperServ, nick->nick);
		return RET_OK_DB;
	}
#else
	sSend(":%s NOTICE %s :This test command is not compiled in.", OperServ,
		  nick->nick);
#endif
	return RET_OK;
}

/************************************************************************/

/**
 * \oscmd Heal
 * \plr   Have services heal an AHURT client and mark them as healed
 *        (so they can use services)
 * \brief Syntax: /OS HEAL \<nick>
 */
OCMD(os_heal)
{
	const char *from = nick->nick;
	UserList *tarNick;

	if (numargs < 2) {
		sSend(":%s NOTICE %s :Syntax Error.", OperServ, from);
		sSend(":%s NOTICE %s :Usage: /msg %s HEAL <nick>", OperServ, from,
			  OperServ);
		return RET_FAIL;
	}

	if (!(tarNick = getNickData(args[1]))) {
		sSend(":%s NOTICE %s :No user by that name.", OperServ, from);
		return RET_NOTARGET;
	}

	sSend(":%s HEAL %s", NickServ, args[1]);
	tarNick->oflags &= ~(NISAHURT);
	return RET_OK;
}

/**************************************************************************/

/* Debugging command */

OCMD(os_nixghost) {
	if (numargs < 2)
		return RET_SYNTAX;
	if (isGhost(args[1])) {
		sSend(":%s QUIT :nixxed", args[1]);
	}
}

/************************************************************************/

/* $Id$ */
