/**
 * \file nickserv.c
 * \brief NickServ Implementation
 *
 * Functions related to the manipulation of registered nick
 * data and nickname authentication and associated operation
 * commands.
 *
 * \wd \taz \greg \mysid
 * \date 1996-2001
 *
 * $Id$
 */

/*
 * Copyright (c) 1996-1997 Chip Norkus,
 * Copyright (c) 1997 Max Byrd
 * Copyright (c) 1997 Greg Poma,
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
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "services.h"
#include "memoserv.h"
#include "queue.h"
#include "macro.h"
#include "nickserv.h"
#include "chanserv.h"
#include "infoserv.h"
#include "email.h"
#include "hash.h"
#include "clone.h"
#include "mass.h"
#include "db.h"
#include "log.h"
#include "hash/md5pw.h"
#define REALLEN 50

struct akill;

/// Alias to structure for keeping NickServ enforcer information
typedef struct ghost_struct GhostList;

GhostList *getGhost(char *);
char *urlEncode(const char *);
const char *GetAuthChKey(const char*, const char*pass, time_t,
                         u_int32_t code_arg);
const char *PrintPass(u_char pI[], char enc);
struct akill* getAkill(char *nick, char *user, char *host);
char* applyAkill(char* nick, char* user, char* host, struct akill* ak);
char* getAkReason(struct akill *ak);
struct akill* getAhurt(char *nick, char *user, char *host);
long getAkillId(struct akill* ak);
void enforce_nickname (char* nick);
int isPasswordAcceptable(const char* password, char* reason);

// *INDENT-OFF*

	/// First item in the index to oper nicks
OperList *firstOper;

	/// Number of CNICKs done
static unsigned long top_cnick = 0;

	/// Highest user identifier
static RegId	top_user_idnum(0, 1);

	/// Highest registered nick identifier
	/// Id numbers 0 - 1000 are reserved for internal use
RegId	top_regnick_idnum(0, 1001);


/// Simple linked list with all the ghost stuff [enforcer] in them
struct ghost_struct {
		/// Nickname of ghost
	char ghost[NICKLEN];

		/// Next ghost (list)
	struct ghost_struct *next;

		/// Previous ghost (list)
	struct ghost_struct *previous;
};

	/// Current number of ghosts
int totalghosts = 0;


/// Oper flag table structure
struct opflag_table { 
		/// Character that represents the flag
	char symbol;

		/// Bitmask of only the flag set
	flag_t flag;

		/// Name of the flag (one word)
	char *name;
};

/// Oper flag definition macro
#define OPFLAG(x, y, z)	{ x,	y,	z }

/// Database of Oper flags
struct opflag_table opflags[] = 
{
  /* ROOT/RR intentionally shouldn't be in this list. */
	OPFLAG('A',	OADMIN,		"ADMIN"),
	OPFLAG('O',	OSERVOP,	"SERVOP"),
	OPFLAG('o',	OOPER,		"o"),
	OPFLAG('k',	ORAKILL,	"AKILL"),
	OPFLAG('K',	OAKILL,		"XAKILL"),
	OPFLAG('p',	OINFOPOST,	"INFO-POST"),
	OPFLAG('s',	OSETOP,		"SETOP"),
	OPFLAG('S',	OSETFLAG,	"SETFLAG"),
	OPFLAG('N',	ONBANDEL,	"NBANDEL"),
	OPFLAG('C',	OCBANDEL,	"CBANDEL"),
	OPFLAG('I',	OIGNORE,	"IGNORE"),
	OPFLAG('G',	OGRP,		"GRp"),
	OPFLAG('r',	ORAW,		"RAW"),
	OPFLAG('j',	OJUPE,		"JUPE"),
	OPFLAG('L',	OLIST,		"LIST"),
	OPFLAG('!',	OPROT,		"FLAG-LOCK"),
	OPFLAG('c',	OCLONE,		"CLONE"),
	OPFLAG('a',	OACC,		"OVERRIDE"),
	OPFLAG('h',	OHELPOP,	"HELPOP"),
	OPFLAG('H',	OAHURT,		"AHURT"),
	OPFLAG('D',	ODMOD,		"DIRECTMOD"),
	OPFLAG('*',	OPFLAG_ADMIN | OPFLAG_DEFAULT | OPFLAG_ROOTSET, "*"),
	{ (char)0, (int)0,  (char *)0 }
};

#undef OPFLAG

/// NickServ set classes
enum ns_set_en { 
	NSS_FLAG,		///< /NICKSERV SET on-off nick -flag- option
	NSS_PROC 		///< /NICKSERV SET special procedure
};

struct nss_tab {
	const char *keyword;		///< Keyword for a set option
	const char *description;	///< Longer description of a set option
	enum ns_set_en set_type;	///< How to handle the set option
	flag_t flag;				///< If it's a flag, which flag?
	flag_t opFlagsRequired;		///< What op flags (if any) to set?

	/// If it's a special set procedure, what function to call?
	cmd_return (*proc)(nss_tab *, UserList *nick, char *a[], int);
};


/// Nickname prefixes services uses when a user's nick is force-changed
char *cnick_prefixes[] = 
{
	"Visitor-",
	"Nobody-",
	"Unnamed-",
	"Nickless-",
	"Nonick-",
	"Untitled-"
};

/// Number of defined CNICK prefixes
#define NUM_CNICKS (sizeof(cnick_prefixes)/sizeof(char *))

GhostList *firstGhost = NULL;	///< First Enforcer
GhostList *lastGhost = NULL;	///< Last Enforcer

/// NickServ command dispatch table
interp::service_cmd_t nickserv_commands[] = 
{
  /* string				function    	 Flags	    L */
  { "help",				ns_help,    	 0,		LOG_NO, CMD_AHURT },
  { "identify",			ns_identify,	 0,		LOG_NO, CMD_AHURT },
  { "identify-plain",	ns_identify,	 0,		LOG_NO, CMD_AHURT },
  { "id-md5",			ns_cidentify,	 0,		LOG_NO, CMD_AHURT },
  { "id",				ns_identify,	 0,		LOG_NO, CMD_AHURT },
  { "sidentify",		ns_identify,	 0,		LOG_NO, CMD_AHURT },
  { "identify-*",		ns_cidentify,	 0,		LOG_NO, CMD_AHURT | CMD_MATCH },
  { "register",			ns_register,	 0,		LOG_NO, 0 },
  { "info",				ns_info,		 0,		LOG_NO, 0 },
  { "ghost",			ns_ghost,		 0,		LOG_NO, 0 },
  { "recover",			ns_recover,		 0,		LOG_NO, 0 },
  { "release",			ns_release,		 0,		LOG_NO, 0 },
  { "set",				ns_set,			 0,		LOG_NO, CMD_REG },
  { "vacation",			ns_vacation,	 0,		LOG_NO, 0 /*MD_REG*/ },
  { "drop",				ns_drop,		 0,		LOG_NO, CMD_REG },
  { "addmask",			ns_addmask,		 0,		LOG_NO, CMD_REG },
  { "access",			ns_access,		 0,		LOG_NO, 0 },
  { "acc",				ns_acc,			 0,		LOG_NO, 0 },
  { "logoff",			ns_logoff,		 0,		LOG_NO, 0 },
  { "logout",			ns_logoff,	 	 0,		LOG_NO, 0 },
  { "setflag",			ns_setflags,	 OOPER,	LOG_NO, 0 },
  { "setop",			ns_setopflags,	 OOPER,	LOG_NO, 0 },
  { "mark",				ns_mark,		 OOPER,	LOG_NO, 0 },
  { "siteok",			ns_bypass,		 OOPER,	LOG_NO, CMD_REG },
  { "bypass",			ns_bypass,		 OOPER,	LOG_NO, CMD_REG },
  { "banish",			ns_banish,		 OOPER,	LOG_NO, CMD_REG },
  { "getpass",			ns_getpass,		 0,	LOG_NO, CMD_REG },
  { "sendpass",			ns_getpass,		 0,	LOG_NO, CMD_REG },
  { "setpass",			ns_setpass,		 0,		LOG_DB, 0 },
  { "setemail",			ns_setemail,		 0,	LOG_NO, CMD_REG },
  { "realgetpass",		ns_getrealpass,	 OOPER,	LOG_NO, CMD_REG },
  { "delete",			ns_delete,		 OOPER,	LOG_DB, CMD_REG },
  { "list",				ns_list,		 OOPER,	LOG_NO, CMD_REG },
  { "save",				ns_save,		 OOPER,	LOG_NO, CMD_REG },
  { "savememo",			ms_save,		 OOPER,	LOG_NO, CMD_REG },
  { NULL }
};

// *INDENT-ON*

/**************************************************************************/

/** 
 * \param nick the nickname (in char * form)
 * \param type Type of desync / What caused the desync? (in char * form)
 * \brief Handle user desyncs
 * \pre Nick and type are non-null pointers to 0-terminated char arrays,
 *      'nick' is the nickname of a client services doesnt see as online.
 * \post Client is killed off the network, desync message is sent if
 *       appropriate.
 * If someone desynched does something and we don't see that they exist,
 * yell about it, and kill them to resync
 */

void nDesynch(char *nick, char *type)
{
	dlogEntry("Desynced user: \"%s\" (%p) removed", nick, nick);

	if (nick) {
		/*sSend(":%s PRIVMSG " DEBUGCHAN
			  " :User %s DESYNCHED during command [%s]", NickServ, nick,
			  type);*/
		sSend(":%s GLOBOPS "
			  " :User %s DESYNCHED during command [%s]", NickServ, nick,
			  type);
		if (!getNickData(nick))
			sSend(":%s KILL %s :%s (Desynch, type: [%s])", myname, nick,
				  myname, type);
	}
	return;
}


/*
 * ===DOC===
 * getNickData will find a nick and retreieve data for it
 * init*Data cleans up a user/nick ent
 * killide, uhm, killides someone, :)
 */
void annoyNickThief(UserList *);
void complainAboutEmail(UserList *);
void killide(char *);
void removeEnforcer(char *);	///< no longer exists

/// Free a userlist record
static void freeUserListRec(UserList *zap)
{
	SetDynBuffer(&zap->host, NULL);
#ifdef TRACK_GECOS
	SetDynBuffer(&zap->gecos, NULL);
#endif
	FREE(zap);
}

/// Free a registered nick record
static void freeRegNickListRec(RegNickList *zap)
{
	if (zap->url != NULL)
		FREE(zap->url);
	SetDynBuffer(&zap->host, NULL);
#ifdef TRACK_GECOS
	SetDynBuffer(&zap->gecos, NULL);
#endif

	SetDynBuffer(&zap->markby, NULL);
	FREE(zap);
}

/**
 * \brief NickServ sees a user
 *
 * Updates NickServ "last seen time", timestamp data,
 * "last seen userhost", and if gecos tracking is enabled,
 * "last seen realname/gecos."
 *
 * \param newnick Online user/nick object
 * \param reg     A registered nickname that an online user has
 *                just acted upon with level ACC 2+ access
 *                either an IDENTIFY command or a LOGON event.
 * \param caccess Level of access user has to the nick presently
 * \param verbose FALSE for no verbosity, any other value for verbose.
 *                If verbose is set, then the user will receive
 *                notifications about unread news, issues about
 *                the nick, incoming memos, etc.
 * \pre           Newnick is a pointer to a listed online user object,
 *                reg is a pointer to a listed registered nick object,
 *                caccess is an integer between 0 and 4 inclusive, and
 *                verbose is #TRUE or #FALSE.
 * \post          Last seen data is updated and appropriate notices
 *                have been sent out over IRC.
 */
void NickSeeUser(UserList *newnick, RegNickList *reg, int caccess,
                 int verbose) 
{

	/* strncpyzt(reg->nick, newnick->nick, NICKLEN); */
	/* The heck was that?  -Mysid */

	strncpyzt(reg->user, newnick->user, USERLEN);
	SetDynBuffer(&reg->host, newnick->host);

#ifdef TRACK_GECOS
	SetDynBuffer(&reg->gecos, newnick->gecos);
#endif

	reg->timestamp = CTime;
	reg->flags &= ~NVACATION;

	if (verbose)
	{
		checkMemos(newnick);
		complainAboutEmail(newnick);
		newsNag(newnick);
	}
}

/**
 * \brief Assigns the nickname, username, host, and possibly gecos
 *        of an online user object.
 *
 * Sets the nick, user, and hostname of an online user object
 *
 * \param newnick Online user/nick object
 * \param Nick nickname of the user to assign to the objecty
 * \param User username to assign the object
 * \param Host hostname to assign the object
 * \param Gecos gecos to assign the object
 * \pre   Newnick is a pointer to an initialized userlist structure
 *        that is either not listed or is listed with a nickname the same
 *        as that in the 'nick' parameter.  Nick, user, and host are
 *        points to valid NUL-terminated character arrays.  Gecos is
 *        a NULL pointer if gecos tracking is disabled or a pointer to
 *        a NUL-terminated character array if the global gecos track
 *        option is enabled.
 * \post  Newnick is assigned copies of the specified nic, user, and
 *        hostname.
 */
static void SetUserNickHostReal(UserList *newnick, char *nick, char *user,
                         char *host, char *gecos)
{
	char HostTmp[HOSTLEN+1];
#ifdef TRACK_GECOS
	char RealTmp[REALLEN+1];
#endif

	strncpyzt(newnick->nick, nick, NICKLEN);
	strncpyzt(newnick->user, user, USERLEN);

	strncpyzt(HostTmp, host, HOSTLEN);
	SetDynBuffer(&newnick->host, HostTmp);
#ifdef TRACK_GECOS
	strncpyzt(RealTmp, gecos, sizeof(RealTmp));
	SetDynBuffer(&newnick->gecos, RealTmp);
#endif

}

/**
 * \brief Get a registered nick encryption prefix
 *
 * \return $ for encrypted or @ for plaintext
 */
char NickGetEnc(RegNickList *rnl)
{
	if (rnl)
		return ((rnl->flags & NENCRYPT) ? '$' : '@');
	return '@';
}

/**
 * \brief Handles a server NICK message by adding a new user connection
 * \param args Argument vector for received NICK server message
 * \param numargs Argument count for received NICK numargs
 * \pre Args is non-NULL pointer to a valid char** array from a NICK message.
 *      Numargs is the number of elements in the outer array, and all inner
 *      elements are NUL-terminated
 * \post A user structure from the supplied NICK message created and added
 *       to the services online user list.
 */
void addNewUser(char **args, int numargs)
{
	UserList *newnick = NULL;
	struct akill* ak;
	char *reason = NULL;
	char realtmp[REALLEN+1];
	struct akill* ahitem;

	if (numargs < 7) {
		sSend
			(":%s GLOBOP :Error adding nick from uplink server!  (args == %d)",
			 OperServ, numargs);
		return;
	}

	if (getNickData(args[1])) {
		sSend
			(":%s GLOBOPS :Desync in adding new user \"%s\" (%p) (%s@%s) -- user exists??",
			 NickServ, args[1], newnick, args[4], args[5]);
		logDump(corelog,
				"Desync in adding new user \"%s\" (%p) (%s@%s) -- user exists??",
				args[1], newnick, args[4], args[5]);
	}

       /* Try to prevent race */
       if (args[1] && isGhost(args[1])) {
         sSend(":%s KILL %s :%s!%s (Nickname currently blocked for NickServ enforcer).", services[1].name, args[1], services[1].host, services[1].name);
         /* delGhost(args[1]); */
         addGhost(args[1]);
         return;
       }

	newnick = (UserList *) oalloc(sizeof(UserList));
	dlogEntry("Adding new user: \"%s\" (%p) (%s@%s)", args[1], newnick,
			  args[4], args[5]);
	if (args[4][0] == '~')
		args[4]++;
	args[7]++;

	parse_str(args, numargs, 7, realtmp, sizeof(realtmp));
	SetUserNickHostReal(newnick, args[1], args[4], args[5], realtmp);

	newnick->reg = getRegNickData(args[1]);
	newnick->timestamp = (time_t) atol(args[3]);
	if ((ak = getAkill(newnick->nick, newnick->user, newnick->host)) &&
            (reason = getAkReason(ak))) {

		sSend(":%s KILL %s :%s!%s (AKilled: %s)", services[1].name,
			  newnick->nick, services[1].host, services[1].name, reason);
		applyAkill(newnick->nick, newnick->user, newnick->host, ak);
		freeUserListRec(newnick);
		/* remUser(args[1]); */
		addGhost(args[1]);
		timer(30, delTimedGhost, strdup(args[1]));
		return;
	}

	newnick->idnum.SetNext(top_user_idnum);

	if (((CTime - 240) <= (newnick->timestamp)) && (ahitem = getAhurt(newnick->nick, newnick->user, newnick->host))) {
#ifdef IRCD_HURTSET
		sSend(":%s HURTSET %s 2 :[#%.6x] Subject to a selective user ban.",
			  OperServ, newnick->nick, getAkillId(ahitem));
#endif
		newnick->oflags |= NISAHURT;
	}
	if (newnick->reg) {
		newnick->reg->flags &= ~(NDOEDIT);

		if (newnick->reg->flags & NBANISH) {
			sSend(":%s KILL %s :%s!%s (Banished nickname)",
				  services[1].name, newnick->nick, services[1].host,
				  services[1].name);
			freeUserListRec(newnick);
			/* remUser(args[1]); */
			addGhost(args[1]);
			timer(15, delTimedGhost, strdup(args[1]));
			return;
		}

		/*if (newnick->reg && !newnick->auth_cookie) {
		 * sSend(":%s NOTICE %s :200 MD5 PLAIN", NickServ, newnick->nick, newnick->auth_cookie, newnick->idnum);
		 * } */
		if ((newnick->reg->flags & NDBISMASK)) {
			newnick->oflags |= NOISMASK;
			sSend(":%s MODE %s :+m", NickServ, newnick->nick);
		}

		if (!checkAccess(newnick->user, newnick->host, newnick->reg)) {
			newnick->caccess = 1;
			annoyNickThief(newnick);
		} else {
			newnick->caccess = 2;
			newnick->reg->timestamp = (time_t) atol(args[3]);

			NickSeeUser(newnick, newnick->reg, 2, 1);
		}
	} else {
		/* Make default +m */
		do {
			newnick->oflags |= NOISMASK;
			sSend(":%s MODE %s :+m", NickServ, newnick->nick);
		} while (0);

#ifdef WELCOME_NOTE
		PutReply(InfoServ, newnick, RPL_IS_GREET_1ARG, WELCOME_NOTE, 0, 0);
#endif
#ifdef DISCLAIMER
		PutReply(InfoServ, newnick, RPL_IS_GREET_1ARG, DISCLAIMER, 0, 0);
#endif
#ifdef COPYRIGHT
		PutReply(InfoServ, newnick, RPL_IS_COPYRIGHT_1ARG, VERSION_NUM, 0, 0);
#endif
	}

	if (addClone(newnick->nick, newnick->user, newnick->host))
		freeUserListRec(newnick);
	else {
		addNick(newnick);
	}

	if (mostusers < totalusers)
		mostusers = totalusers;
}

/**
 * \brief Handles a NICK command, allowing nicknames to change
 * \param from Nickname of user to be changed
 * \param to Target nickname
 * \param ts Timestamp string of nickname change (if any)
 * \pre From, to, and ts are all non-null pointers to 0-terminated
 *      character arrays.
 * \post Nickname of online user is internally recorded as changed
 *       to 'from' at time 'ts'.  The original nick structure is
 *       deleted.  Some members are copied over, but many members are
 *       reset to initial values.
 *
 * Called when someone uses the NICK command to change their
 * nicks obviously...changed from, to, and it hands off the timestamp
 * so that last seen time can be updated
 *
 * \sideffect The entire online user structure is destroyed and re-created.
 * Fields are copied manually, so any online user member that needs to
 * follow the user across nick changes has to be special-cased here.
 */
void changeNick(char *from, char *to, char *ts)
{
	UserList *changeme, *tmp;
	char tmp1[NICKLEN + 2];

	changeme = getNickData(from);
	if (changeme == NULL) {
		nDesynch(from, "NICK");
		return;
	}

	if (changeme->oflags & NCNICK) {
		addGhost(changeme->nick);
		timer(120, delTimedGhost, strdup(changeme->nick));
		changeme->oflags &= ~(NCNICK);
	}

	/* ALOT of changing around goes on here....it rather sucks...ohwell */
	tmp = (UserList *) oalloc(sizeof(UserList));
	tmp->idnum.SetNext(top_user_idnum);
	dlogEntry("Changing nick -> \"%s\" (%p) to \"%s\" (%p)", from, from,
			  to, tmp);
	strcpy(tmp->nick, to);
#ifdef TRACK_GECOS
	SetUserNickHostReal(tmp, to, changeme->user, changeme->host, changeme->gecos);
#else
	SetUserNickHostReal(tmp, to, changeme->user, changeme->host, NULL);
#endif

	tmp->oflags = changeme->oflags;
	tmp->floodlevel = changeme->floodlevel;

	changeNickOnAllChans(changeme, tmp);
	if (ts && *ts) {
		if (*ts == ':')
			ts++;
		tmp->timestamp = (time_t) atol(ts);
	} else
		tmp->timestamp = CTime;

	if (changeme->id.nick) {
		RegNickList *tmptarget = getRegNickData(changeme->id.nick);
		tmp->id.nick = changeme->id.nick;
		tmp->id.idnum = changeme->id.idnum;
		/* Update timestamp unless the nick has a new owner */
		if (tmptarget && (tmptarget->timereg < changeme->id.timestamp))
			tmp->id.timestamp = tmp->timestamp;
		else
			clearIdentify(tmp);
		changeme->id.nick = NULL;
	}

	// Update the "Last Seen" time  
	if (changeme->reg && isIdentified(changeme, changeme->reg))
		changeme->reg->timestamp = CTime;

	addNick(tmp);
	delNick(changeme);
	if (isAKilled(tmp->nick, tmp->user, tmp->host)) {
		sSend(":%s KILL %s :%s!%s (AKilled user)", services[1].name,
			  tmp->nick, services[1].host, services[1].name);
		remUser(to, 1);
		addGhost(to);
		return;
	}

        /* Try to prevent race */
	if (isGhost(tmp->nick)) {
		sSend(":%s KILL %s :%s!%s (Nickname currently blocked for NickServ enforcer).", services[1].name, tmp->nick, services[1].host, services[1].name);
		/* delGhost(tmp->nick); */
		addGhost(tmp->nick);
		remUser(to, 1);
		return;
	}

	tmp->reg = getRegNickData(to);

	if (tmp->reg != NULL) {
		if (tmp->reg->flags & NBANISH) {
			sSend
				(":%s 433 %s :Banished nickname - You have 10 seconds to change your nick.",
				 myname, tmp->nick);
			sprintf(tmp1, "%s@@%i", to, 0);
			timer(10, killide, strdup(tmp1));
		}

		if (tmp->id.timestamp && tmp->id.nick
			&& tmp->id.timestamp > tmp->reg->timereg
			&& tmp->id.idnum == tmp->reg->regnum
			&& !strcasecmp(tmp->id.nick, tmp->nick)) {
			sSend(":%s NOTICE %s :You have already identified for access.",
				  NickServ, tmp->nick);
			clearIdentify(tmp);
			tmp->caccess = 3;
			NickSeeUser(tmp, tmp->reg, 3, 1);

			if (ts[0] == ':')
				ts++;
			tmp->reg->timestamp = (time_t) atol(ts);
			return;
		}
		/* Users may case change their nicks without nagging [Echo] */
		else if (!strcasecmp(tmp->nick, changeme->nick))
			tmp->caccess = changeme->caccess;
		else if (!checkAccess(tmp->user, tmp->host, tmp->reg)) {
			tmp->caccess = 0;
			annoyNickThief(tmp);
		} else {
			tmp->caccess = 2;

			NickSeeUser(tmp, tmp->reg, 2, 1);
			if (ts[0] == ':')
				ts++;
			tmp->reg->timestamp = (time_t) atol(ts);
		}
	}
}

/**
 * \brief Changes the modes of an online user
 * \param nick Nickname of user whose modes are changing
 * \param mode Mode change to be applied to the user
 *
 * \pre Nick and mode are pointers to valid NUL-terminated
 *      character arrays, 'nick' is that of an online user object
 *      found in the user hash table, and 'mode' is a valid
 *      mode-change string.
 * \post User structure is modified to take into account specified
 *       mode changes.
 *
 * Handles a MODE message for a user.  Of particular interest to services
 * are the state of the 'o' and 'm' usermodes.
 */
void setMode(char *nick, char *mode)
{
	UserList *changeme;
	RegNickList *regn;
	int i, c = 0;

	changeme = getNickData(nick);

	dlogEntry("Changing umodes for \"%s\" (%p) to \"%s\" (%p)", changeme,
			  changeme, mode, mode);

	if (changeme == NULL) {
		nDesynch(nick, "UMODE");
		return;
	}

	mode++;						/* We don't need to bother with the : */

	for (i = 0; mode[i] != 0; i++) {
		switch (tolower(mode[i])) {
		case '-':
			c = 0;
			break;
		case '+':
			c = 1;
			break;
		case 'h':
			if (c == 1)
				changeme->oflags |= NISHELPOP;
			else
				changeme->oflags &= ~NISHELPOP;
			break;
		case 'm':
			if (c == 1) {
				changeme->oflags |= NOISMASK;
				if ((regn = getRegNickData(changeme->nick))
					&& isRecognized(changeme, regn))
					regn->flags |= NDBISMASK;
			} else {
				changeme->oflags &= ~NOISMASK;
				if ((regn = getRegNickData(changeme->nick))
					&& isIdentified(changeme, regn))
					regn->flags &= ~NDBISMASK;
			}
			break;
		case 'o':
			if (c == 1) {
				changeme->oflags |= NISOPER;
				changeme->oflags &= ~NISAHURT;
			} else
				changeme->oflags &= ~NISOPER;
			break;
		}
	}

	mode--;
	return;
}


/**
 * \brief Sets online flags of an online user structure
 * \param nick Nickname of the user whom flags are to be changed on
 * \param flag Flag to be added or removed from the user
 * \param change Indicates whether to add or remove the flag
 *
 * \pre Nick is a pointer to a terminated character array and
 *      the nickname of an online user, flag is a valid online
 *      user flag as defined in nickserv.h, and 'change' has
 *      the value of the literal '+' or the literal '-'
 * \post Specified online flag will be added or removed from the structure
 *       of the online user.  If '+' is the value of change, then
 *       addition occurs,  if '-' is the value of change, then
 *       removal of a bit occurs.
 */
void setFlags(char *nick, int flag, char change)
{
	UserList *bah;
	bah = getNickData(nick);
	if (bah == NULL) {
		nDesynch(nick, "SETFLAG(internal source)");
		return;
	}

	dlogEntry("Setting flags for \"%s\" (%p) - %i", nick, bah, flag);

	if (change == '+')
		bah->oflags |= flag;
	else
		bah->oflags &= ~flag;

}

/**
 * \brief Identifies an online user to a registered nickname
 * \param user Pointer to online user who is identifying
 * \param nick Pointer to registered nickname to be identified
 * \pre Neither user nor nick are null pointers.  User is
 *      a valid online user structure, and nick points to a valid
 *      online registered nick structure.
 * \post Online user user will be marked as having identified
 *       to the nickname nick.
 */
void setIdentify(UserList * user, RegNickList * nick)
{
	if (!user || !nick || !nick->nick)
		return;
	if (user->id.nick)
		FREE(user->id.nick);
	user->id.nick = (char *)oalloc(strlen(nick->nick) + 2);
	strcpy(user->id.nick, nick->nick);

	user->id.timestamp = MAX(user->timestamp, CTime);
	user->id.idnum = nick->regnum;
	return;
}

/**
 * \brief Clears any identify by an online user
 * \param user Pointer to the online user who is to be affected
 * \pre User points to a valid, non-null online user object.
 * \post User will no longer be identified to any remote nickname.
 */
void clearIdentify(UserList * user)
{
	if (!user)
		return;
	if (user->id.nick)
		FREE(user->id.nick);
	user->id.nick = NULL;
	user->id.timestamp = 0;
	user->id.idnum = RegId(0,0);
	return;
}

/**
 * \brief Reports TRUE or FALSE indicating whether or not the specified user
 * is identified to the specified nickname
 *
 * \param user Pointer to online user to check for identification from
 * \param nick Pointer to registered nickname to check for identification to
 * \pre User points to a valid online user object, nick points to a valid
 *      registered nickname object.
 * \return FALSE indicates no identification, any other value indicates
 *         that the user is identified.
 */
int isIdentified(UserList * user, RegNickList * nick)
{
	if (!(user && nick) || (nick->flags & NVACATION)
	    || (nick->flags & NFORCEXFER))
		return 0;
	if (user->reg != nick && user->id.nick && user->id.timestamp) {
		if (user->timestamp <= user->id.timestamp &&
			nick->timereg < user->id.timestamp &&
			nick->regnum == user->id.idnum &&
			!strcmp(user->id.nick, nick->nick)) return 1;
		return 0;
	}
	return (user->reg == nick && user->reg && (user->caccess > 2));
}


/**
 * \brief Reports TRUE or FALSE indicating whether or not the specified user
 * is recognized by access list (ACC 2) for the specified nickname
 *
 * \param user Pointer to online user to check for recognition from
 * \param nick Pointer to registered nickname to check for recognition to
 * \pre User points to a valid online user object.  Nick points to a valid
 *      registered nickname object.
 *
 * \return FALSE too indicate no recognition, any other value to indicate
 * the user is recognized as being on the access list of the target nick.
 */
int isRecognized(UserList * user, RegNickList * nick)
{
	if (!(user && nick) || (nick->flags & NVACATION))
		return 0;
	if (checkAccess(user->user, user->host, nick))
		return 1;
	if (!(user && nick) || user->reg != nick)
		return isIdentified(user, nick);
	return (user->reg && user->reg == nick && (user->caccess > 1));
}

/**
 * \param nick Pointer to an online user item
 * \returnFALSE if the specified online user is invalid or not
 *        globally opered.  Any other value indicates the user is an oper.
 * \pre Nick is a NULL pointer or a valid online user object.
 */
int isOper(UserList * nick)
{
	if (nick == NULL)
		return 0;
	else {
		if (nick->oflags & NISOPER)
			return 1;
		else
			return 0;
	}
}

/**
 * \return FALSE if the online user is a null pointer or does not
 *         have all operator flags specified; any other value may
 *         be returned if the oper has all flags specified.
 * \param nick Pointer to online user item to check
 * \param flag Bitmask with flags specified to check set and all others clear
 * \pre Nick points to a valid online user object or is a null pointer.
 *      Flag consists of an operator flag value as found in operserv.h
 *      or a sequence of such flags (ex: OOPER | OSERVOP)
 *
 * \warning Presence of the +a flag cannot be tested with this function.
 *          OACC/OVERRIDE will return true only if /os override is in use.
 */
int opFlagged(UserList * nick, flag_t flag)
{
	int need_oper = (flag & OOPER);
	int need_user_override = (flag & OACC);
	extern int userOverriding(UserList *);

	if (nick == NULL)
		return 0;
	flag &= ~(OOPER);
	if (need_user_override && !userOverriding(nick))
		return FALSE;
	if (nick->reg == NULL || (flag && !isIdentified(nick, nick->reg)))
		return 0;
	if ((nick->reg->opflags & OROOT))
		if (!flag || ((flag & OPFLAG_ROOT) == flag))
			return (!need_oper || isOper(nick));
	if (!flag || ((nick->reg->opflags & flag) == flag))
		return (!need_oper || isOper(nick));
	return 0;
}

/**
 * \brief Returns the effective flags of an online user
 * \param nick Pointer to online user item to extract flags from
 * \pre Nick points to a valid online user object or is a null pointer.
 *
 * A bitmask is returned with all flags that the user has access to
 * set.  The exception being that the '+a' flag is only set if
 * /OPERSERV OVERRIDE is being used.  If a null pointer is provided,
 * the return value is zero.
 *
 * \warning Presence of the +a flag cannot be tested with this function.
 *          OACC/OVERRIDE will return true only if /os override is in use.
 */
flag_t getOpFlags(UserList * nick)
{
	flag_t flags = 0;
	extern int userOverriding(UserList *);

	if (nick == NULL)
		return 0;
	if (isOper(nick))
		flags |= OOPER;
	if (nick->reg == NULL || (!isIdentified(nick, nick->reg)))
		return flags;
	if (userOverriding(nick))
		flags |= OACC;
	if ((nick->reg->opflags & OROOT))
		return (flags | (OPFLAG_PLUS));
	return (flags | nick->reg->opflags);
}


/** \internal */
int issRoot(UserList* nick) {
	int ofl;

	if (!nick || !isRoot(nick)) {
		return 0;
	}

	ofl = getOpFlags(nick);
	return ((ofl & 0x2)) ? 1 : 0;
}

/**
 * \return FALSE if nick is a null pointer or the user pointed
 *        to does not have services root privileges
 * \pre Nick points to a valid online user object or is a null pointer.
 * \param nick Pointer to an online user item
 */
int isRoot(UserList * nick)
{
	if (nick == NULL)
		return 0;
	if (nick->reg == NULL)
		return 0;

	if (nick->reg->opflags & OROOT && isIdentified(nick, nick->reg))
		return isOper(nick);
	else
		return 0;
}

/**
 * \return FALSE if the specified nick is a null pointer or not currently
 *         a Services Operator, or IRCop with services operator access.
 * \pre Nick points to a valid online user object or is a null
 *      pointer.
 * \param nick Pointer to an online user item
 */
int isServop(UserList * nick)
{
	if (nick == NULL)
		return 0;
	if (nick->reg == NULL)
		return 0;

	if ((nick->reg->opflags & OSERVOP || nick->reg->opflags & OROOT)
		&& isIdentified(nick, nick->reg))
		return isOper(nick);
	else
		return 0;
}

#ifdef ENABLE_GRPOPS
/**
 * \return FALSE if the specified online user is a null pointer or has
 *         no access to the NickServ getrealpass command; else a value
 *         other than FALSE is returned.
 * \pre Nick points to a valid online user object or is a null pointer.
 * \param nick Pointer to an online user item
 */
int isGRPop(UserList * nick)
{
	if (nick == NULL)
		return 0;
	if (nick->reg == NULL)
		return 0;

	if ((nick->reg->opflags & OGRP) && isIdentified(nick, nick->reg))
		return isOper(nick);
	else
		return 0;
}
#endif

/**
 * \return FALSE if the specified online user is not presently a HelpOp
 *         or nick was a NULL pointer.  Else a value other than FALSE is
 *         returned.
 * \pre Nick is initialized to point to a valid online user object
 *      or is a null pointer.
 * \param nick Pointer to an online user item
 */
int isHelpop(UserList * nick)
{
	if (nick == NULL)
		return 0;
	if (nick->reg == NULL)
		return 0;

	if ((nick->oflags & NISHELPOP) && isIdentified(nick, nick->reg))
		return 1;
	else
		return 0;
}

/**
 * \return FALSE if nick is a null pointer or has no access to the
 *         AKILL command as a result of the OAKILL opflag, specifically.
 * \pre Nick is initialized to point to a valid online user or is
 *      a null pointer.
 * \param nick Nickname to check for akill access
 */
int canAkill(UserList * nick)
{
	if (nick == NULL)
		return 0;
	if (nick->reg == NULL)
		return 0;

	if (nick->reg->opflags & OAKILL && isIdentified(nick, nick->reg))
		return isOper(nick);
	else
		return 0;
}


/**
 * \brief Handles the QUIT server message, it removes a user
 * from the nickname list and destroys the allocated memory area.
 *
 * \pre Nick is initialized to point to a NUL-terminated character
 *      array that contains the nickname know to be that of a valid
 *      online user structure.
 * \post After a call to this function, nick is no longer in the online list,
 *       any pointer to that nickname's online user structure is now invalid,
 *       and services no longer considers the user to be on IRC.
 *
 * \param nick Nickname of online user
 * \param ignoreDesync Non-zero value means to ignore desyncs
 */
void remUser(char *nick, int ignoreDesync)
{
	UserList *killme;

	killme = getNickData(nick);

	if (killme == NULL) {
		if (!ignoreDesync)
			nDesynch(nick, "QUIT");
		return;
	}

	dlogEntry("Removing user \"%s\" (%p)", nick, killme);
	remFromAllChans(killme);
	delClone(killme->user, killme->host);

	if (killme->reg && isIdentified(killme, killme->reg)) {
		cleanMemos(killme);
		killme->reg->timestamp = CTime;
	}

	delNick(killme);
}


/**
 * \brief Adds the online user item to the user hash tables and
 * user list
 *
 * \pre Newnick is initialized to point to an allocated
 *      structure of type newnick.
 * \post The structure newnick is added to the online users hash table.
 * \param nick User to add to the list
 */
void addNick(UserList * newnick)
{
	HashKeyVal hashEnt;

	dlogEntry("Adding nick \"%s\" (%p)", newnick->nick, newnick);

	hashEnt = getHashKey(newnick->nick) % NICKHASHSIZE;
	dlogEntry("Added nick \"%s\" with hash entry %i",
			  newnick->nick, hashEnt);
	LIST_ENTRY_INIT(newnick, ul_lst);
	LIST_INSERT_HEAD(&UserHash[hashEnt], newnick, ul_lst);
	totalusers++;
}

/**
 * \brief Adds the registered nick item to the nickname hash tables and
 * user list
 * \pre Newnick is initialized to point to a freshly-allocated
 *      nickname structure, it is not a null pointer, and no registered
 *      nick structure listed has the same nickname.
 * \post The registered nick structure is added onto the
 *       hash table of registered nicknames.
 *
 * \param nick Pointer to registered nickname item
 */
void addRegNick(RegNickList * newnick)
{
	RegNickList *tmp;
	RegNickIdMap *mapPtr, *mapPtrIter, *mapPtrIterNext;
	HashKeyVal hashEnt;

	/*
	 * If this nick is already registered, don't do a thing.
	 */
	if ((tmp = getRegNickData(newnick->nick))) {
		logDump(corelog, "addRegNick(%p) : %s already exists",
				newnick, newnick->nick);
		assert(tmp == NULL);

		abort();
		freeRegNickListRec(newnick);
		return;
	}

	/*
	 * hash the nick, and insert it into the correct linked list.
	 */
	hashEnt = getHashKey(newnick->nick) % NICKHASHSIZE;
	LIST_ENTRY_INIT(newnick, rn_lst);
	LIST_INSERT_HEAD(&RegNickHash[hashEnt], newnick, rn_lst);


	mapPtr = (RegNickIdMap *)oalloc(sizeof(RegNickIdMap));
	mapPtr->nick = newnick;
	mapPtr->id   = newnick->regnum;

	hashEnt = newnick->regnum.getHashKey() % IDHASHSIZE;
	LIST_ENTRY_INIT(mapPtr, id_lst);
	if (!LIST_FIRST(&RegNickIdHash[hashEnt]))
		LIST_INSERT_HEAD(&RegNickIdHash[hashEnt], mapPtr, id_lst);
	else {
		for(mapPtrIter = LIST_FIRST(&RegNickIdHash[hashEnt]);
		    mapPtrIter;
		    mapPtrIter = mapPtrIterNext)
		{
			mapPtrIterNext = LIST_NEXT(mapPtrIter, id_lst);

			if ((mapPtr->id) > (mapPtrIter->id)) {
				if (mapPtrIterNext)
					continue;
				LIST_INSERT_AFTER(mapPtrIter, mapPtr, id_lst);
				break;
			}
			else {
				LIST_INSERT_BEFORE(mapPtrIter, mapPtr, id_lst);
				break;
			}
		}

	}
}


/**
 * \brief Removes the online user item from the user hash tables and
 * user list.  When this function returns, 'killme' is no longer a valid
 * pointer.
 * \pre Killme points to a valid, non-null online user object that is
 *      found in the online user list (or table).
 * \post The online user nick object is removed from the online user
 *       table and freed from memory.
 * \param nick Pointer to an online user item to delete
 */
void delNick(UserList * killme)
{
	LIST_REMOVE(killme, ul_lst);
	clearIdentify(killme);

	freeUserListRec(killme);
	flush_ad(killme);
	totalusers--;
}

/**
 * \brief Removes the registered nick item from the nickname hash tables and
 * nick list.  When this function returns, 'killme' is no longer a valid
 * pointer.
 *
 * \pre Killme points to a valid, non-null registered nickname object
 *      that is found in the nick table.
 * \post The registered nick object is removed from the nick
 *       table and freed from memory.
 * \param nick Registered nick object to delete
 */
void delRegNick(RegNickList * killme)
{
	MemoList *memos;
	MemoList *memos_next;
	RegNickList *to;
	RegNickIdMap *mapPtr;

	LIST_REMOVE(killme, rn_lst);

	mapPtr = killme->regnum.getIdMap();
	if (mapPtr)
	{
		LIST_REMOVE(mapPtr, id_lst);

		FREE(mapPtr);
	}

	delOpData(killme);
	if (killme->memos)
	{
		memos = LIST_FIRST(&killme->memos->mb_sent);
		while (memos != NULL) {
			memos_next = LIST_NEXT(memos, ml_sent);
			to = memos->realto;
			if (to == NULL)
				return;
			LIST_REMOVE(memos, ml_sent);
			delMemo(to->memos, memos);
			memos = memos_next;
		}
	}
	freeRegNickListRec(killme);
}

/**
 * \brief Searches the user hash table for information about the specified
 * online user by nickname.  Returns a pointer to the online user item
 * if found, otherwise returns NULL
 * \pre Nick points to a valid NUL-terminated character array.
 *
 * \param nick Nickname of the user to find
 * \return A null pointer if no online user structure found in the table
 *         has the specified nickname, else a pointer to one of the
 *         online user structures found.
 */
UserList *getNickData(char *nick)
{
	HashKeyVal hashEnt;
	UserList *tmpnick;

	hashEnt = getHashKey(nick) % NICKHASHSIZE;
	for (tmpnick = LIST_FIRST(&UserHash[hashEnt]); tmpnick != NULL;
		 tmpnick = LIST_NEXT(tmpnick, ul_lst)) {
		if (!strcasecmp(tmpnick->nick, nick))
			return tmpnick;
	}

	return NULL;
}

/**
 * \brief Searches the regnick hash table for information about the specified
 * registered nick data by nickname.
 * \return A pointer to the registered nick record if one could be 
 *         found.  Else a null pointer.
 * \pre Nick points to a valid NUL-terminated character array.
 *
 * \param nick Nickname of the registered nick item to find
 */
RegNickList *getRegNickData(const char *nick)
{
	HashKeyVal hashEnt;
	RegNickList *tmpnick;
	int l;

	if (nick == NULL || (l = strlen(nick)) > NICKLEN || l == 0)
		return NULL;

	hashEnt = getHashKey(nick) % NICKHASHSIZE;
	for (tmpnick = LIST_FIRST(&RegNickHash[hashEnt]); tmpnick != NULL;
		 tmpnick = LIST_NEXT(tmpnick, rn_lst))
		if (strcasecmp(tmpnick->nick, nick) == 0)
			return tmpnick;

	return NULL;
}

/**
 * \brief Used to test whether the specified user\@hostname can be matched 
 * against an entry in an access list.
 *
 * \pre Both user and host point to valid NUL-terminated character
 *      arrays containing respectively an IRC username and IRC hostname
 *      of an online or recent user.  Nick points to a valid registered
 *      nickname record.
 *      
 * \param user Pointer to the username to match
 * \param host Pointer to the hostname to match
 * \param nick Registered nickname to search the access list of
 */
int checkAccess(char *user, char *host, RegNickList * nick)
{
	nAccessList *tmp;
	char theirmask[HOSTLEN + USERLEN + 3];

	if (nick == NULL || user == NULL || host == NULL)
		return 0;

	sprintf(theirmask, "%.*s@%.*s", USERLEN, user, HOSTLEN, host);

	/* otherwise, check their access masks */
	for (tmp = LIST_FIRST(&nick->acl); tmp; tmp = LIST_NEXT(tmp, al_lst))
		if (!match(tmp->mask, theirmask))
			return 1;			/* match */
	return 0;					/* NoMatch */
}

/**
 * \brief Message handler for NickServ
 * \pre Tmp points to a valid online user record.
 *      Args points to a valid C-string vector with numargs outer
 *      indices, the sum of whose lengths is IRCBUF or less, and whose
 *      contents are the text of a valid IRC PRIVMSG command split
 *      along spaces and starting after the message target.
 * \post If NickServ command requested of the user was valid, then it 
 *       will have been executed.
 * \param tmp Pointer to online user initiating the msesage
 * \param args Args of the message, where args[0] is the command and
 *        the extra parameters follow
 * \param numargs Highest index in the args[] array passed plus 1.
 *        so args[numargs - 1] is the highest index that can be safely
 *        accessed.
 */
void sendToNickServ(UserList * tmp, char **args, int numargs)
{
	char *from = tmp->nick;
	interp::parser * cmd;

	tmp = getNickData(from);
	if (tmp == NULL) {
		nDesynch(from, "PRIVMSG");
		return;
	}

	cmd = new interp::parser(NickServ, getOpFlags(tmp),
							 nickserv_commands, args[0]);
	if (!cmd)
		return;

	switch (cmd->run(tmp, args, numargs)) {
	default:
		break;
	case RET_FAIL:
		sSend(":%s NOTICE %s :Unknown command %s.\r\n"
			  ":%s NOTICE %s :Please try /msg %s HELP",
			  NickServ, from, args[0], NickServ, from, NickServ);
		break;
	case RET_OK_DB:
		sSend(":%s NOTICE %s :Next database synch(save) in %ld minutes.",
			  NickServ, tmp->nick, ((nextNsync - CTime) / 60));
		break;
	}

	delete cmd;
}

/* && 
			!Valid_md5key(args[2], nick->auth, tmp->passwd,
			NickGetEnc(tmp))) {*/


/**
 * \param user User requesting information
 * \param target Nick about whom host information is requested
 * \pre User is a null pointer or valid online user record.
 *      Target is a null pointer or valid registered nickname record.
 * \return The last seen host of the target as it should be shown
 *         to the user requesting that information.
 */
char *regnick_ugethost(UserList * user, RegNickList * target, int sM)
{
	UserList *online;
	static char rep[NICKLEN + USERLEN + HOSTLEN + 275];

	if (!user || !target || (strlen(target->host) > (sizeof(rep) - 200)))
		return "<ERROR>";

	if ((online = getNickData(target->nick))) {
		if (isRecognized(online, target)) {
			if (!(online->oflags & NOISMASK))
				return strcpy(rep, target->host);
			else if (!(user->oflags & NISOPER)) {
				strncpyzt(rep, genHostMask(target->host), sizeof(rep));
				return rep;
			} else if ((user->oflags & NISOPER)) {
				strncpyzt(rep, target->host, sizeof(rep) - 11);
				if (sM)
					sprintf(rep + strlen(rep), " (\2masked\2)");
				return rep;
			}
		}
	}

	if (!(target->flags & NDBISMASK))
		return strcpy(rep, target->host);
	if (!(user->oflags & NISOPER)) {
		strncpyzt(rep, genHostMask(target->host), sizeof(rep));
		return rep;
	} else {
		strcpy(rep, target->host);
		if (sM)
			sprintf(rep + strlen(rep), " (\2masked\2)");
		return rep;
	}
	return ("<ERROR>@<ERROR>");
}

/**
 * \brief Handle the ACCESS ADD command
 * \param mask Pointer to host address pattern to add to access list
 * \param nick List of online user requesting the add
 * \pre Mask points to a valid NUL-terminated character array that
 *      consists of a valid NickServ access mask of the form user\@host.
 *      Nick points to the record of an online user that is also
 *      a registered nick.  nick->reg points to a valid registered
 *      nickname record.
 * \post The specified mask is added to the online user's registered
 *       nickname.
 */
void addAccessMask(char *mask, UserList * nick)
{
	if (!isIdentified(nick, nick->reg)) {
		sSend(":%s NOTICE %s :You must identify to use ACCESS ADD",
			  NickServ, nick->nick);
		return;
	}

	if (USERLEN + HOSTLEN + 3 < strlen(mask)) {
		sSend(":%s GLOBOPS :%s tried to add a HUGE access mask", NickServ,
			  nick->nick);
		sSend(":%s NOTICE %s :That access mask is WAY too long", NickServ,
			  nick->nick);
		return;
	}

	if (nick->reg->amasks >= AccessLimit) {
		sSend
			(":%s NOTICE %s :You have reached the limit of access masks, you cannot add any more",
			 NickServ, nick->nick);
		sSend(":%s GLOBOPS :%s attempts to add too many access items.",
			 NickServ, nick->nick);
		return;
	}

	if (addAccItem(nick->reg, mask) == 0)
		sSend
			(":%s NOTICE %s :%s was \002not\002 added to your access list, it is an improper mask or already present.",
			 NickServ, nick->nick, mask);
	else {
		sSend(":%s NOTICE %s :%s is added to your access list",
			  NickServ, nick->nick, mask);
	}
}

/**
 * \brief Handle the ACCESS DEL command
 * \param mask Pointer to host address pattern to delete from the access list
 * \param nick List of online user requesting the delete
 * \pre Mask points to a valid NUL-terminated character array.
 *      Nick points to the record of an online user that is also
 *      a registered nick with a nick->reg member pointing to
 *      that record.
 * \post The specified mask is removed from the online user's registered
 *       nickname if it could be found.  If the mask specified is '-',
 *       then the registered nickname's access list is wiped clean.
 */
void delAccessMask(char *mask, UserList * nick)
{
	nAccessList *acl;
	char deleted[71];

	if (!isIdentified(nick, nick->reg)) {
		sSend(":%s NOTICE %s :You must identify to use ACCESS DEL",
			  NickServ, nick->nick);
		return;
	}

	if (LIST_FIRST(&nick->reg->acl) == NULL)
		return;

	if (*mask == '-') {
		for (acl = LIST_FIRST(&nick->reg->acl); acl != NULL;
			 acl = LIST_NEXT(acl, al_lst))
			delAccItem(nick->reg, acl->mask, deleted);

		sSend(":%s NOTICE %s :Your access list has been cleared",
			  NickServ, nick->nick);
		nick->reg->amasks = 0;

		return;
	}

	if (delAccItem(nick->reg, mask, deleted)) {
		sSend(":%s NOTICE %s :The mask %s has been deleted"
			  " from your access list", NickServ, nick->nick, deleted);
		nick->reg->amasks--;
	} else if (strchr(mask, '@') == NULL)
		sSend(":%s NOTICE %s :Access list entry #%s was not found",
			  NickServ, nick->nick, mask);
	else
		sSend(":%s NOTICE %s :%s was not in your access list",
			  NickServ, nick->nick, mask);
}

/**
 * \brief Nag users who never set an e-mail address and never did the
 *        '/NICKSERV SET ADDRESS NONE'
 * \pre Nick points to a valid online user record
 * \post If necessary an invalid e-mail address warning has been sent.
 * \param nick Pointer to the structure of the online user to be nagged
 */
void complainAboutEmail(UserList * nick)
{
	if (!nick->reg || (nick->reg->email[0] &&
					   (strncasecmp(nick->reg->email, "(none)", 6))
					   || index(nick->reg->email, ' ')))
		return;

	sSend(":%s NOTICE %s :***** NOTICE *****", NickServ, nick->nick);
	sSend(":%s NOTICE %s :You have not specified an e-mail address.",
		  NickServ, nick->nick);
	sSend
		(":%s NOTICE %s :Should you forget or lose your NickServ password, the operators will most likely be unable to assist you,",
		 NickServ, nick->nick);
	sSend(":%s NOTICE %s :unless you specify one.", NickServ, nick->nick);
	sSend
		(":%s NOTICE %s :To set an e-mail address and eliminate this message:",
		 NickServ, nick->nick);
	sSend
		(":%s NOTICE %s :      type \2/msg nickserv set address YOUREMAIL\2 ",
		 NickServ, nick->nick);
	sSend
		(":%s NOTICE %s :Once set, NickServ will NOT divulge your address to other users unless you explicitly tell it to.",
		 NickServ, nick->nick);
	sSend(":%s NOTICE %s :", NickServ, nick->nick);
	sSend
		(":%s NOTICE %s :To keep no e-mail address but eliminate this message:",
		 NickServ, nick->nick);
	sSend(":%s NOTICE %s :      type \2/msg nickserv set address NONE",
		  NickServ, nick->nick);
	sSend(":%s NOTICE %s :", NickServ, nick->nick);
	sSend
		(":%s NOTICE %s :Type \2/join #help\2 if you have any questions regarding this.",
		 NickServ, nick->nick);
	sSend(":%s NOTICE %s :***** NOTICE *****", NickServ, nick->nick);
	sSend
		(":%s PRIVMSG %s :You have not yet set an e-mail address or indicated your desire to send none: please see the notice from services (it may be in a status window).",
		 NickServ, nick->nick);
}

/**
 * \brief Basically bother a nick owner into giving a nick back
 *        (and/or enforce nick kill)
 * \pre   Nick points to a valid online user record, who is also a
 *        registered nickname and whose 'reg' member points to that
 *        registered nickname record.
 * \post  The user has been warned that they are using the nickname
 *        that is possibly owned by another user.
 * \param nick Pointer to an online user item
 */
void annoyNickThief(UserList * nick)
{
	char buffer[NICKLEN+HOSTLEN+41];

	if (nick->reg->flags & NKILL) {
		sSend(":%s NOTICE %s :This nick belongs to another user."
			  "  Please change nicks or it will be forcefully changed"
			  " in %i seconds.", NickServ, nick->nick, nick->reg->idtime);
		sSend(":%s NOTICE %s :If this is your nick please try:"
			  " /msg %s ID password", NickServ, nick->nick, NickServ);

		/* Start a timer to killide the nick using their ID time */
		if (nick->reg->idtime > NICKWARNINT) {
			if ((nick->reg->idtime - NICKWARNINT) < NICKWARNINT)
				sSend(":%s 433 %s %s :Belongs to another user",
					  myname, nick->nick, nick->nick);
			sprintf(buffer, "%s@%s@%i", nick->nick, nick->host,
					nick->reg->idtime);
			timer(NICKWARNINT, killide, strdup(buffer));
		} else {
			sSend(":%s 433 %s %s :Belongs to another user",
				  myname, nick->nick, nick->nick);
			sprintf(buffer, "%s@%s@%i", nick->nick, nick->host, 0);
			timer(nick->reg->idtime, killide, strdup(buffer));
		}
		return;
	}

	sSend(":%s NOTICE %s :This nick belongs to another user."
		  "  Please change nicks", NickServ, nick->nick);
	sSend(":%s NOTICE %s :If this is your nick please try:"
		  " /msg %s ID password", NickServ, nick->nick, NickServ);
	return;
}


void guest_cnick(UserList* ul)
{
#ifndef	CNICK_UNSUPPORTED
	unsigned int j = 0;
	extern int dice(int, int);
	char forcenick[NICKLEN];
	const char* nick;

	/// Format of a CNICK message
#	define CNICK_FMT ":%s CNICK %s %s %ld"
		
	if (!ul)
		return;
	nick = ul->nick;

	do {
		snprintf(forcenick, NICKLEN, "%s%lX%X",
			 cnick_prefixes[(top_cnick + j) % NUM_CNICKS],
			 (++top_cnick) / NUM_CNICKS, dice(3, 200) + 1);
	} while ((j++ < NUM_CNICKS && getNickData(forcenick))
					 || (getNickData(forcenick) && ++top_cnick));	

	sSend(":%s NOTICE %s :Your nickname is now being "
		  "changed to \2%s\2.", NickServ, nick, forcenick);
	sSend(CNICK_FMT, myname, nick, forcenick, CTime);
	if (ul)
		ul->oflags |= NCNICK;
#endif				 
}

void enforce_nickname (char* nick)
{
	UserList* ul = nick ? getNickData(nick) : 0;

	if (ul == 0) {
		return;
	}

#ifndef CNICK_UNSUPPORTED
	guest_cnick(ul);
#else
	sSend(":%s KILL %s :%s!%s (Nick protection enforced)",
		  NickServ, nick, services[1].host, services[1].name);
	remUser(nick, 1);
	addGhost(nick);
	timer(120, delTimedGhost, strdup(nick));
#endif
}

/**
 * \brief Timed force nick change (or nag) of a user whom services does not
 *        believe owns the nickname
 * \param info String indicating the nickname and time remaining (in seconds)
 *        in the form \<nick>\@\<host>\@\<time>
 *
 * \pre   Info is a pointer to a freshly allocated memory area. Info has
 *        been filled with a NUL-terminated string of the form
 *        \<nick>\@\<host>\@\<time> nick is registered, hostname is the
 *        host of the current user of the nickname on IRC, and time is
 *        an index of time remaining before potential nickname enforcement
 *        from services.
 *
 * \post  As appropriate: warnings may be sent out, nicknames may be
 *        changed, or users might be killed.  One or more online user
 *        records may be removed from that table and no longer be valid
 *        records after this procedure.  The supplied info parameter is
 *        no longer a valid pointer value, because this procedure frees
 *        the allocated memory pointed to by info.
 */
void killide(char *info)
{
	char tmp[NICKLEN + HOSTLEN + 10];
	UserList *check;
	RegNickList *regnick;
	char *nick;
	char *host;
	char *time;

	nick = strtok(info, "@");
	host = strtok(NULL, "@");
	time = strtok(NULL, "\0");

	unsigned int left = 0;		/* Amount of time remaining (in seconds) */

	if (nick == NULL) {
		return;
	}

	/*
	 * If they don't exist, or they have access to the nick, just clean
	 * up and return.
	 */
	check = getNickData(nick);
	regnick = getRegNickData(nick);

	if (check == NULL) {
		FREE(info);
		return;
	} else if (isIdentified(check, regnick)) {
		FREE(info);
		return;
	} else if (host != NULL) {
		if (strcmp(check->host, host)) {
			FREE(info);
			return;
		}
	} else if (time == NULL) {
		FREE(info);
		return;
	}

	left = atoi(time);

	if (left == 0) {			/* All warnings have expired.  Change nick now. */
		if (getNickData(nick)) {
			enforce_nickname( nick );
		}		
	} else {					/* There are warning(s) left, Warn the user and reset timer */
		if (getNickData(nick)) {
			if (left >= NICKWARNINT) {
				left -= NICKWARNINT;
			} else {
				/* If the code fails, reset the time left to a
				 * sane value and log. */
				left = NICKWARNINT;
				logDump(corelog, "Time left sanity check failed in "
						  "killide().  Resetting to %i.", NICKWARNINT);
			}

			if (left >= NICKWARNINT) {
				sSend(":%s NOTICE %s :You have %i seconds to "
					  "change your nick, or %s will be forced "
					  "to change it for you.", NickServ, nick,
					  left, NickServ);
				if ((left - NICKWARNINT) < NICKWARNINT)
					sSend(":%s 433 %s %s :Belongs to "
						  "another user", myname, nick, nick);
				sprintf(tmp, "%s@%s@%i", nick, host, left);
				timer(NICKWARNINT, killide, strdup(tmp));
			} else {
				sprintf(tmp, "%s@%s@%i", nick, host, 0);
				timer(left, killide, strdup(tmp));
			}
		}
		FREE(info);
	}
}

/**
 * \brief Add a NickServ enforcer
 * \param ghost What nickname to place a services enforcer on
 * \pre  Ghost is a pointer to a valid NUL-terminated character array
 *       containing an IRC nickname consisting of alphanumeric
 *       characters and a length between 1 and (NICKLEN - 1) characters.
 *       There is not a record in the online user table with a
 *       nickname the same as that specified.
 *
 * \post A services enforcer is signed on with the specified nickname.
 * \warning The enforcer is not scheduled for removal to do so,
 *          a timer must be set to call the delTimedGhost procedure with
 *          the ghost nickname as a parameter.
 */
void addGhost(char *ghost)
{
	GhostList *newghost = (GhostList *)0; 
	char host[HOSTLEN];
	

	if (ghost) {
		for(newghost = firstGhost; newghost; newghost = newghost->next)
		{
			if (!strcasecmp(ghost, newghost->ghost)) {
				break;
			}
		}
	}

	if (!newghost) {
		newghost = (GhostList *) oalloc(sizeof(GhostList));

		strncpyzt(newghost->ghost, ghost, NICKLEN);

		if (firstGhost == NULL) {
			firstGhost = newghost;
			newghost->previous = NULL;
		} else {
			lastGhost->next = newghost;
			newghost->previous = lastGhost;
		}

		lastGhost = newghost;
		newghost->next = NULL;

		totalusers++;
		totalghosts++;
	}

	snprintf(host, sizeof(host), "nicks.%s", NETWORK);
	addUser(ghost, "owned", host, "Nick Enforcement", "i");
}

/**
 * \brief Remove a NickServ enforcer
 * \param ghost Nickname of the enforcer to remove
 * \pre  Ghost is a pointer to a valid NUL-terminated character
 *       array bearing a legal IRC nickname.
 * \post If the nickname was that of an online services enforcer,
 *       then services has removed the enforcer and the information
 *       stored about the enforcer.
 */
void delGhost(char *ghost)
{
	GhostList *killme;


	sSend(":%s QUIT :Ahhh, they finally let me off work!", ghost);

	killme = getGhost(ghost);

	if (killme == NULL)
		return;

	if (killme->previous)
		killme->previous->next = killme->next;
	else
		firstGhost = killme->next;
	if (killme->next)
		killme->next->previous = killme->previous;
	else
		lastGhost = killme->previous;

	FREE(killme);
	totalghosts--;
	totalusers--;

	return;
}

/**
 * \brief Handle expiration of a NickServ ghost timer by removing the enforcer
 * \param ghost Nickname of timed ghost
 * \sideffect the supplied ghost pointer value is freed
 * \pre  Ghost is a pointer to a valid NUL-terminated character
 *       array bearing a legal IRC nickname.
 * \post delGhost() was called with the supplied value for 'ghost'
 *       to remove an enforcer if it was present, AND the memory pointed
 *       to by 'ghost' was freed.
 */
void delTimedGhost(char *ghost)
{
	delGhost(ghost);
	FREE(ghost);
}

/**
 * \brief Retrieve information about a NickServ enforcer
 * \param ghost Nickname of the ghost
 */
GhostList *getGhost(char *ghost)
{
	GhostList *tmp;

	for (tmp = firstGhost; tmp; tmp = tmp->next) {
		if (strcasecmp(tmp->ghost, ghost) == 0)
			return tmp;
	}
	return NULL;
}

/**
 * \pre Ghost is a pointer to a valid NUL-terminated character array
 *      whose contents are a valid IRC nickname string.
 * \return FALSE if the specified nickname is not a NickServ 
 *         enforcer.  Else a non-zero value.
 * \param ghost Nickname to check
 */
int isGhost(char *ghost)
{
	if (getGhost(ghost))
		return 1;
	else
		return 0;
}

// Services Flood Protection ////////////////////////////////////////////

RateInfo::RateInfo() :
  lastEventTime(0),  rateFloodValue(0),  numOfEvents(0),
  warningsSent(0)
{

}

void RateInfo::Event(int weightOfEvent, time_t tNow)
{
	// If the last message was [ period1 length ] or more seconds ago, then
	// cut in half
	if ((tNow - lastEventTime) >= FLOOD_DET_HALF_PERIOD)
		rateFloodValue >>= 1;

	// If more than [ period2 length ] seconds ago, then flood level
        // restarts, else: the flood weight is added

	if ((tNow - lastEventTime) <= FLOOD_DET_Z_PERIOD)
		rateFloodValue += weightOfEvent;
	else
		rateFloodValue = weightOfEvent;

	lastEventTime = tNow;
	if (numOfEvents < INT_MAX)
		numOfEvents++;
}

void RateInfo::Warn() {
	if (warningsSent < UCHAR_MAX)
		warningsSent++;
}

int RateInfo::Warned() {
	return (int) warningsSent;
}

int RateInfo::GetLev() {
	return rateFloodValue;
}

/// Record a bad password attempt by `sender' on `target'
int BadPwNick(UserList *sender, RegNickList *target)
{
	if (sender == NULL || target == NULL)
		return 0;

	if (sender->badpws < UCHAR_MAX)
		sender->badpws++;

	if (target->badpws < UCHAR_MAX)
		target->badpws++;

	if ((sender->badpws > NPW_TH_SENDER_1 && target->badpws > NPW_TH_TARGET_1) ||
            (sender->badpws > NPW_TH_SENDER_2 && target->badpws > NPW_TH_TARGET_2) ||
            (sender->badpws > NPW_TH_SENDER_3 && target->badpws > NPW_TH_TARGET_3)) 
        {
		sSend(":%s GLOBOPS :Possible hack attempt %s!%s@%s (%u) -> %s (%u)",
			NickServ, sender->nick, sender->user, sender->host, (u_int)sender->badpws,
			target->nick, (u_int)target->badpws
		);

		if (addFlood(sender, FLOODVAL_BADPW))
			return 1;
	}

	return 0;
}

/// Record a successful password entry by `sender' on `target'
int GoodPwNick(UserList *sender, RegNickList *target)
{
	char timeJunk[80];

	if (sender == NULL || target == NULL)
		return 0;

	if (target->badpws > 0)
	{
		sSend(":%s NOTICE %s :%d failed password attempt(s) for %s since last access",
			 NickServ, sender->nick, target->badpws, target->nick);
		if (strftime(timeJunk, 80, "%a %Y-%b-%d %T %Z", localtime(&target->timestamp)) > 0) {
			sSend(":%s NOTICE %s :%s seen %s from %s@%s.",
				 NickServ, sender->nick, target->nick, timeJunk, target->user, regnick_ugethost(sender, target));
		}

		target->badpws = 0;
	}

	return 0;
}

/**
 * \brief Services Flood Protection -- addFlood adds the specified amount
 * to a user's flood level, assuming this was added within tens econds of
 * the last addition otherwise, simply makes floodlevel equal to whatever
 * is sent.  This function also updates lastmsg.
 *
 * \param tmp Pointer to online user item
 * \param addtoflood Number of flood points the command is worth
 * \pre  Tmp is a pointer to a valid online user record, and
 *       addtoflood is an integer between of 0 and 100.
 * \post Services flood checks have been performed.  If a user
 *       floods excessively, they might have been killed.
 *       If a user has been killed then this procedure will
 *       have returned a non-zero value and their online
 *       user record can no longer be treated as valid without
 *       performing a nick lookup from the online users table
 *       based on a 'safe' copy of the nick string.
 *
 * \warning If a user structure is killed, then so are all strings on 
 *          the structure, example: 'char *from = nick->nick;'
 *          if the 'nick' structure is killed, then from is no
 *          longer valid.  For most procedures, processing should
 *          return up back to a command interpreter once
 *          addFlood(nick, ...) returns a non-zero value.
 */

int addFlood(UserList * tmp, int addtoflood)
{
	int fIsIgnored = 0;
	long timenow = time(NULL);

	if (tmp == NULL)
		return 0;

	if (isRoot(tmp) == 1) {
		return 0;
	}

	if (tmp->nick && tmp->user && tmp->host)
		fIsIgnored = isIgnored(tmp->nick, tmp->user, tmp->host);

	tmp->floodlevel.Event(addtoflood, timenow);

	if (tmp->floodlevel.Warned() && tmp->floodlevel.GetLev() >= MAXFLOODLEVEL) {
		char user[USERLEN], host[HOSTLEN],
		     theirmask[USERLEN + HOSTLEN + 3];
		char tmpstr[NICKLEN + USERLEN + HOSTLEN + 4];
		sSend
			(":%s KILL %s :%s!%s (Flooding services is not permitted [You are now on services ignore])",
			 OperServ, tmp->nick, services[0].host, OperServ);
		if (!fIsIgnored)
			sSend
				(":%s GLOBOPS :\002Severe Flooding\002: %s!%s@%s  [User killed] (Flood: %i/%i)",
				 myname, tmp->nick, tmp->user, tmp->host, tmp->floodlevel.GetLev(),
				 MAXFLOODLEVEL);

		strncpyzt(user, tmp->user, USERLEN);
		strncpyzt(host, tmp->host, HOSTLEN);

		mask(user, host, 0, theirmask);
		sprintf(tmpstr, "*!%.*s", USERLEN + HOSTLEN + 4, theirmask);
		if (!isIgnored(tmp->nick, tmp->user, tmp->host))
			addakill((10 * 3600), tmpstr, OperServ, A_IGNORE,
					 "Flooding Services");
		remUser(tmp->nick, 1);

		return 1;
	}

	else if (tmp->floodlevel.GetLev() >= (.75 * MAXFLOODLEVEL) && !fIsIgnored) 
	{
		sSend(":%s GLOBOPS :\002Flooding\002: %s!%s@%s (Flood: %i/%i)",
			  myname, tmp->nick, tmp->user, tmp->host, tmp->floodlevel.GetLev(),
			  MAXFLOODLEVEL);
		sSend(":%s NOTICE %s :Flooding is not good for your health or mine, "
		      "stop now before you are removed from the network and placed "
		      "on services ignore (Floodlevel %i/%i)", NickServ, tmp->nick,
		      tmp->floodlevel.GetLev(), MAXFLOODLEVEL);
		tmp->floodlevel.Warn();

		return 1;
	}
	return 0;
}

/**
 * \fn void expireNicks(char *dummy)
 * \param dummy Value is not used
 *
 * \brief This function handles timed NickServ expiration runs,
 * deletes non-held nicknames that have not been used for their
 * maximum inactivity period before expiration.
 */

/* ARGSUSED0 */
void expireNicks(char *dummy)
{
	time_t timestart, timeend;
	RegNickList *tmp;
	RegNickList *next;
	char backup[30];
	char cmd[500];         ///< \bug FIX ME
	int i = 0, bucket = 0;

	mostnicks = 0;
	timestart = time(NULL);

	/*
	 * back up the nickserv database
	 */
	strftime(backup, 30, "nickserv.db%d%m%Y", localtime(&timestart));
	sprintf(cmd, "cp nickserv/nickserv.db nickserv/backups/%s", backup);
	system(cmd);

	/*
	 * back up memoserv database as well, as memos are tied to nicks
	 */
	strftime(backup, 30, "memoserv.db%d%m%Y", localtime(&timestart));
	sprintf(cmd, "cp memoserv/memoserv.db memoserv/backups/%s", backup);
	system(cmd);


	/*
	 * Run through all hash buckets, checking nick status and if
	 * appropriate, last used times
	 */
	for (bucket = 0; bucket < NICKHASHSIZE; bucket++) {
		tmp = LIST_FIRST(&RegNickHash[bucket]);

		while (tmp != NULL) {
			next = LIST_NEXT(tmp, rn_lst);

			if (getNickData(tmp->nick) != NULL)
				tmp->timestamp = CTime;

			if (!isIdentified(getNickData(tmp->nick), tmp)) {

				if ((tmp->flags & (NHOLD | NBANISH)) != 0) {
					;
				} else if (tmp->flags & NVACATION) {
					if ((timestart - tmp->timestamp)
						>= (3 * NICKDROPTIME)) {
						nicklog->log(NULL, NSE_EXPIRE, tmp->nick, 0,
									 "%ld %ld %ld vacation", timestart,
									 tmp->timestamp,
									 timestart - tmp->timestamp);
						delRegNick(tmp);
						i++;
					}
				} else if ((timestart - tmp->timestamp) >= NICKDROPTIME) {
					nicklog->log(NULL, NSE_EXPIRE, tmp->nick, 0,
								 "%ld %ld %ld", timestart, tmp->timestamp,
								 timestart - tmp->timestamp);
					delRegNick(tmp);
					i++;
				}
			}

			tmp = next;
			mostnicks++;
		}
	}

	timeend = time(NULL);
	sSend(":%s GLOBOPS :NickServ EXPIRE(%i/%lu) %lu seconds",
		  NickServ, i, mostnicks, (timeend - timestart));

	timer(2 * 3600, (void (*)(char *))expireNicks, NULL);
}

/**
 * \brief Handles the work of placing an access item in a registered
 * nickname's access list.
 *
 * \param nick Registered nickname item to add an access list item to
 * \param mask Mask string to add a copy of
 * \pre Nick is a pointer to a registered nickname record, and
 *      mask is a pointer to a NUL-terminated character array that
 *      is a valid user\@host NickServ access mask.
 */
int addAccItem(RegNickList * nick, char *mask)
{
	nAccessList *acl;

	if (!nick)
		return 0;

	if (!strcmp(mask, "*@*") || !strcmp(mask, "*@*.*") ||
		!match(mask, "..@...com") ||
		!match(mask, "..@...edu") ||
		!match(mask, "..@...org") ||
		!match(mask, "..@...net") ||
		!match(mask, "..@...info") ||
		!match(mask, "..@...co.uk") ||
		!match(mask, "..@...br") ||
		index(mask, '!') || !index(mask, '@'))
		return 0;

	for (acl = LIST_FIRST(&nick->acl); acl != NULL;
		 acl = LIST_NEXT(acl, al_lst)) if (!strcasecmp(mask, acl->mask))
			return 0;

	acl = (nAccessList *) oalloc(sizeof(nAccessList));
	strncpyzt(acl->mask, mask, 70);

	LIST_INSERT_HEAD(&(nick->acl), acl, al_lst);

	nick->amasks++;

	return 1;
}

/**
 * \brief Handles the work of removing an access item from a registered
 * nickname's access list.
 *
 * \param nick Registered nickname item to delete an access list item from
 * \param mask Mask to remove
 * \pre Nick pointers to a valid registered nickname record,
 *      mask points to a valid NUL-terminated character array, and
 *      deletedmask points to a character memory area of at least
 *      size 70 bytes.
 * \post If mask is found in the access list of nick, then that
 *       mask is removed.  The string of the deleted mask is
 *       copied into the area pointed by deletedmask.
 *
 * \return 1 If a mask was deleted, 0 if no mask was removed.
 */
int delAccItem(RegNickList * nick, char *mask, char *deletedmask)
{
	nAccessList *acl;
	int found = 0;
	int x = 0;
	int idx;

	if (nick == NULL)
		return 0;

	x = atoi(mask);
	idx = 1;

	for (acl = LIST_FIRST(&nick->acl); acl != NULL;
		 acl = LIST_NEXT(acl, al_lst)) {
		if (!strcasecmp(mask, acl->mask) || (idx++ == x)) {
			found = 1;
			break;
		}
	}

	if (found == 0)
		return 0;

	strncpyzt(deletedmask, acl->mask, 70);

	LIST_REMOVE(acl, al_lst);

	FREE(acl);

	return 1;
}

/**
 * \brief Saves the NickServ database
 * \param next When the NickServ database should next be saved (UTC)
 */
void syncNickData(time_t next)
{
	nextNsync = next;
	saveNickData();
}

/**
 * \brief Saves the MemoServ database
 * \param next When the MemoServ database should next be saved (UTC)
 */
void syncMemoData(time_t next)
{
	nextMsync = next;
	saveMemoData();
}

/* ARGSUSED1 */
/**
 * \brief Sync MemoServ database to disk
 */
NCMD(ms_save)
{
	char *from = nick->nick;

	if (isRoot(nick) == 0) {
		PutReply(MemoServ, nick, ERR_NOACCESS, 0, 0, 0);
		return RET_FAIL;
	}

#ifdef GLOBOP_ON_SAVE
	sSend(":%s GLOBOPS :Saving database. (%s)", MemoServ, from);
#else
	sSend(":%s PRIVMSG " LOGCHAN " :Saving database. (%s)", MemoServ,
		  from);
#endif
	saveMemoData();
	return RET_OK_DB;
}

/**
 * \brief Adds a user with opflags to the index
 * \param nick Pointer to registered nickname item to add
 */
void addOpData(RegNickList * nick)
{
	OperList *oper;

	if (!nick || !nick->opflags)
		return;
	for (oper = firstOper; oper; oper = oper->next)
		if (oper->who == nick)
			return;

	if (!firstOper || (nick->opflags & OROOT)) {
		oper = (OperList *) oalloc(sizeof(OperList));
		oper->next = firstOper;
		firstOper = oper;
		oper->who = nick;
	} else {
		for (oper = firstOper; oper->next; oper = oper->next);
		oper->next = (OperList *) oalloc(sizeof(OperList));
		oper->next->who = nick;
		oper->next->next = NULL;
	}
}

/**
 * \brief Removes and frees a user's index item from the oper index
 * \param nick Pointer to registered nickname item to remove
 */
void delOpData(RegNickList * nick)
{
	OperList *oper, *nextoper, *tmp;

	for (oper = firstOper, tmp = NULL; oper; oper = nextoper) {
		nextoper = oper->next;
		if (oper->who == nick) {
			if (tmp)
				tmp->next = nextoper;
			else
				firstOper = nextoper;
			FREE(oper);
			continue;
		} else
			tmp = oper;
	}
}

/**************************************************************************/

/**
 * \nscmd Help
 * \syntax Help \<topic>
 */
NCMD(ns_help)
{
	help(nick->nick, NickServ, args, numargs);
	return RET_OK;
}

/**
 * \nscmd Vacation
 * \syntax Vacation
 * \plr NickServ vacation mode, extended expire times
 * \plr Nicknames on vacation have tripled min expire time length.
 */
NCMD(ns_vacation)
{
	char *from = nick->nick;
	time_t timestart;

	timestart = time(NULL);

	if (!nick->reg) {
		PutError(NickServ, nick, ERR_NEEDREGNICK_1ARG, nick->nick, 0, 0);
		return RET_FAIL;
	}

	if ((timestart - nick->reg->timereg) < NICKDROPTIME) {
		sSend
			(":%s NOTICE %s :Your nick has not been registered long enough to set vacation mode.  The current requirement is %u days",
			 NickServ, from, (NICKDROPTIME / 3600 / 24));
		return RET_FAIL;
	}

	if (!isIdentified(nick, nick->reg)) {
		sSend
			(":%s NOTICE %s :You must be identifed with NickServ to use VACATION",
			 NickServ, from);
		return RET_FAIL;
	}
	nick->reg->flags |= NVACATION;

	sSend(":%s NOTICE %s :Your nick is now marked as on vacation.",
		  NickServ, from);
	sSend
		(":%s NOTICE %s :Please be aware that this will be automatically removed next time you return to IRC.",
		 NickServ, from);
	sSend
		(":%s NOTICE %s :Your nick will automatically expire in %u days if you do not return.",
		 NickServ, from, (NICKDROPTIME / 3600 / 24 * 2));
	return RET_OK_DB;
}

/**
 * \nscmd Identify
 * \syntax {Id | Identify{-type}} [\<nick>] \<password> 
 * \plr Cause NickServ to recognize you as a nickname's owner
 */
NCMD(ns_identify)
{
	RegNickList *tonick = nick->reg;
	char *s_nick = NULL, *passwd;
	char *from = nick->nick;

	if (addFlood(nick, 5))
		return RET_KILLED;

	if (numargs < 2) {
		sSend(":%s NOTICE %s :The IDENTIFY command requires a password",
			  NickServ, from);
		return RET_SYNTAX;
	}

	if (numargs < 3)
		passwd = args[1];
	else {
		s_nick = args[1];
		passwd = args[2];
		tonick = getRegNickData(s_nick);
	}

	if (!tonick) {
		PutError(NickServ, nick, ERR_NICKNOTREG_1ARG,
				 s_nick ? s_nick : nick->nick, 0, 0);
		PutHelpInfo(NickServ, nick, "HELP");

		return RET_NOTARGET;
	}

	if ((tonick->flags & NBANISH)) {
		PutError(NickServ, nick, ERR_NICKBANISHED_1ARG,
				 s_nick ? s_nick : nick->nick, 0, 0);

		return RET_EFAULT;
	}

	if (!(tonick->flags & NFORCEXFER) 
	    && Valid_pw(passwd, tonick->password, NickGetEnc(tonick))) {
		if (tonick->chpw_key) {
			PutReply(NickServ, nick, RPL_CHKEY_DEAD, 0, 0, 0);
			tonick->chpw_key = 0;
		}

		if (nick->reg == tonick) 
		{
			nick->caccess = 3;

			assert(!strcasecmp(nick->reg->nick, nick->nick));
			strcpy(nick->reg->nick, nick->nick); /* Case update */

			PutReply(NickServ, nick, RPL_IDENTIFYOK_NOARG, 0, 0, 0);

			if ((tonick && !(tonick->flags & NUSEDPW))) {
				tonick->flags |= NUSEDPW;
				PutReply(NickServ, nick, RPL_MASKHELP, NickServ, NickServ,
						 0);
			}
		} else {
			PutReply(NickServ, nick, RPL_IDENTIFYOK_NICKARG, tonick->nick,
					 0, 0);
			clearIdentify(nick);
			setIdentify(nick, tonick);
			if ((tonick && !(tonick->flags & NUSEDPW))) {
				tonick->flags |= NUSEDPW;
				PutReply(NickServ, nick, RPL_MASKHELP, NickServ, NickServ,
						 0);
			}
		}

#ifdef ENABLE_AHURT
		/*
		 * Identify invokes AHURT bypass
		 */
		if ((nick->oflags & NISAHURT)) {
			if ((tonick && !(tonick->flags & NBYPASS)))
				PutError(NickServ, nick, ERR_NOAHURTBYPASS, 0, 0, 0);
			else {
				PutReply(NickServ, nick, RPL_AHURTBYPASS, 0, 0, 0);
				PutReply(NickServ, nick, RPL_BYPASSISLOGGED, 0, 0, 0);
				sSend(":%s HEAL %s", NickServ, from);
				nick->oflags &= ~(NISAHURT);
				nicklog->log(nick, NS_IDENTIFY, tonick->nick, 0, "AHURT");
			}
		}
#endif

		/*
		 * Update last seen host, badpws, and time to reflect the
		 * use of identify.
		 */
		NickSeeUser(nick, tonick, 3, 0);
		GoodPwNick(nick, tonick);
		tonick->timestamp = CTime;

		/*
		 * Local identify, check memos e-mail and news
		 */
		if (nick->reg == tonick) {
			checkMemos(nick);
			complainAboutEmail(nick);
			newsNag(nick);
		}
		else if ((nick->oflags & NOISMASK) /* Remote id, update masked status */
			 && !(tonick->flags & NDBISMASK)) tonick->flags |= NDBISMASK;
		return RET_OK;
	} else { 
		/*
		 * Incorrect Password
		 */

		/* failed identify takes away even the access list ID */
		if (nick->reg == tonick) { /* Local */
			nick->caccess = 1;
			PutError(NickServ, nick, ERR_BADPW, 0, 0, 0);
			if (BadPwNick(nick, nick->reg))
				return RET_KILLED;
			nicklog->logw(nick, NS_IDENTIFY, tonick->nick, LOGF_BADPW);
		} else {
			PutError(NickServ, nick, ERR_BADPW, 0, 0, 0);
			nicklog->logw(nick, NS_IDENTIFY, tonick->nick, LOGF_BADPW);

			if (BadPwNick(nick, tonick))
				return RET_KILLED;
			return RET_BADPW;
		}
	}
	return RET_OK;
}


#ifdef MD5_AUTH
/**
 * \brief Identify with MD5 authenticator 
 * \smysid
 *
 * Sample dialog:
 *         /msg nickserv cid
 *         -NickServ- 002 S/MD5 1.0 abcxyz:5a
 *         /exec md5 -s 'passwd'
 *         <hash1>
 *         /exec md5 -s 'nick:abcxyz:5a:<hash1>'
 *         <hash2>
 *         /msg nickserv cid <hash2>
 *         -NickServ- Validated
 *
 */

NCMD(ns_cidentify)
{
	RegNickList *tonick = nick->reg;
	struct auth_data auth_info[1];
	char *s_nick = NULL, *passwd;
	int valid = 0;

	if (addFlood(nick, 5))
		return RET_KILLED;

	/*
	 * Send info about the available types
	 */
	if (!strcasecmp(args[0], "identify-types")) {
		PutError(NickServ, nick, RPL_AUTH_TYPES, 0, 0, 0);
		return RET_OK;
	}

	/*
	 * Check for valid auth type, default is MD5 (for cidentify)
	 */
	if (!strncasecmp(args[0], "identify-", 9)
		&& strncasecmp((args[0]) + 9, "md5", 3)) {
		PutReply(NickServ, nick, ERR_AUTH_NOTYPE, 0, 0, 0);
		return RET_OK;
	}

	/*
	 * No cookie, but nick specified? Bad.
	 */
	if (numargs >= 2 && !nick->auth_cookie) {
		PutError(NickServ, nick, ERR_AUTH_CHAL, 0, 0, 0);
		nick->auth_cookie = 0;
		return RET_OK;
	}

	/*
	 * Cookie but no arguments? Cancel previous cookie, issue new one.
	 */
	if (nick->auth_cookie && numargs < 2) {
		PutError(NickServ, nick, RPL_AUTH_NORESPONSE, 0, 0, 0);
		nick->auth_cookie = 0;
	}

	/**
         * It doesn't need to actually be random, just irreproducible 
         * and well-outside the user's control
         *
         * -Mysid
         */
	if (numargs < 2) {
		/*
		 * Cookie requested
		 */
		nick->auth_cookie = time(NULL);
		if (nick->auth_cookie > 900000000)
			nick->auth_cookie -= 900000000;
		PutReply(NickServ, nick, RPL_AUTH_SEED,
				 (unsigned int)nick->auth_cookie,
				 (unsigned int)nick->idnum.getHashKey(), 0);
		return RET_OK;
	}

	/*
	 * Identify request and they indeed have a challenge to answer
	 * this message should be it.
	 */

	if (numargs < 3)
		passwd = args[1];
	else {
		s_nick = args[1];
		passwd = args[2];
		tonick = getRegNickData(s_nick);
	}

	/*
	 * Uh-oh not registered
	 */

	if (!tonick) {
		PutError(NickServ, nick, ERR_AUTH_NOTREGISTERED_2ARG, "nickname",
				 args[1], 0);
		PutHelpInfo(NickServ, nick, "HELP IDENTIFY");
		return RET_NOTARGET;
	}
	
	auth_info->auth_cookie = nick->auth_cookie;
	auth_info->auth_user = nick->idnum.getHashKey();
	auth_info->format = 1;

	valid = Valid_md5key(passwd, auth_info, tonick->nick, tonick->password, NickGetEnc(tonick));
//xValid_md5
	nick->auth_cookie = 0;

	/*
	 * If the key was valid
	 */

	if ( valid != 0 ) {
		if (tonick->chpw_key) {
			PutReply(NickServ, nick, RPL_CHKEY_DEAD, 0, 0, 0);
			tonick->chpw_key = 0;
		}
		if ((tonick && !(tonick->flags & NUSEDPW)))
			tonick->flags |= NUSEDPW;

		/*
		 * Local identify
		 */
		if (nick->reg == tonick) {
			nick->caccess = 4;

			assert(!strcasecmp(nick->reg->nick, nick->nick));
			strcpy(nick->reg->nick, nick->nick);	/* Case update */
			PutReply(NickServ, nick, RPL_AUTH_OK_0ARG, 0, 0, 0);
			GoodPwNick(nick, tonick);
		} else { /* Remote */
			PutReply(NickServ, nick, RPL_AUTH_OK_1ARG, tonick->nick, 0, 0);
			clearIdentify(nick);
			setIdentify(nick, tonick);
			GoodPwNick(nick, tonick);
		}

#ifdef ENABLE_AHURT
		/*
		 * Handle the ahurt bypass case
		 */
		if ((nick->oflags & NISAHURT)) {
			if ((tonick && !(tonick->flags & NBYPASS)))
				PutError(NickServ, nick, ERR_NOAHURTBYPASS, 0, 0, 0);
			else {
				PutReply(NickServ, nick, RPL_AHURTBYPASS, 0, 0, 0);
				PutReply(NickServ, nick, RPL_BYPASSISLOGGED, 0, 0, 0);

				sSend(":%s HEAL %s", NickServ, nick->nick);
				nick->oflags &= ~(NISAHURT);
				nicklog->log(nick, NS_IDENTIFY, tonick->nick, 0, "AHURT");
			}
		}
#endif

		/*
		 * Update user information last seen host and time
		 */
		NickSeeUser(nick, tonick, 3, 0);
		tonick->timestamp = CTime;

		/*
		 * Local nick, check memos and for bad e-mail field
		 */
		if (nick->reg == tonick) {
			checkMemos(nick);
			complainAboutEmail(nick);
		}
		return RET_OK;
	} else {
		/*
		 * Uh-oh... they didn't answer the challenge correctly.
		 */
		if (nick->reg == tonick) {
			PutError(NickServ, nick, ERR_AUTH_BAD_1ARG, tonick->nick, 0,
					 0);
			nicklog->log(nick, NS_IDENTIFY, tonick->nick, LOGF_BADPW,
						 "MD5");
			if (BadPwNick(nick, tonick))
				return RET_KILLED;
		}
		else {
			PutError(NickServ, nick, ERR_AUTH_BAD_1ARG, tonick->nick, 0,
					 0);
			nicklog->log(nick, NS_IDENTIFY, tonick->nick, LOGF_BADPW,
						 "MD5");
			if (BadPwNick(nick, tonick))
				return RET_KILLED;
			return RET_BADPW;
		}
	}
	return RET_OK;
}
#endif

/**
 * \brief The /NICKSERV SET PASSWORD command
 */
cmd_return ns_set_passwd(nss_tab *setEnt, UserList *nick,
                         char *args[], int numargs)
{
	const char *from = nick->nick;
	char pw_reason[IRCBUF];

	if (numargs < 2) {
		sSend(":%s NOTICE %s :You must specify a parameter", NickServ,
			  from);
		return RET_EFAULT;
	}

	if (PASSLEN < strlen(args[1])) {
		sSend(":%s NOTICE %s :Please specify a password of %d "
			  "characters or less.", NickServ, from, PASSLEN);
		return RET_EFAULT;
	}

	if (isPasswordAcceptable(args[1], pw_reason) == 0) {
		sSend(":%s NOTICE %s :Sorry, %s isn't a password that you can use.",
			NickServ, from, args[1]);
		sSend(":%s NOTICE %s :%s", NickServ, from, pw_reason);
		return RET_EFAULT;
	}
	
	pw_enter_password(args[1], nick->reg->password, NickGetEnc(nick->reg));
	sSend(":%s NOTICE %s :Your password is now %s, do NOT forget it, we"
		  " are not responsible for lost passwords", NickServ, from,
		  args[1]);
	return RET_OK_DB;
}



/**
 * \brief The /NICKSERV SET URL command
 */
cmd_return ns_set_url(nss_tab *setEnt, UserList *nick,
                         char *args[], int numargs)
{
	const char *from = nick->nick;
	int urllen;

	/*
	 * Clear the URL field?
	 */
	if (numargs < 2) {
		if (nick->reg->url)
			FREE(nick->reg->url);
		nick->reg->url = NULL;
		sSend(":%s NOTICE %s :Your URL has been cleared",
			  NickServ, from);
		return RET_OK_DB;
	}

	/*
	 * Check the length, and report an error if it is too long
	 */
	urllen = strlen(args[1]);
	if (urllen > URLLEN - 1) {
		sSend(":%s NOTICE %s :URL too long."
			  "  Max is %d characters.", NickServ, from, URLLEN - 1);
		return RET_EFAULT;
	}

	/*
	 * Free the old URL space, and allocate a new chunk.
	 */
	if (nick->reg->url)
		FREE(nick->reg->url);
	nick->reg->url = strdup(args[1]);
	sSend(":%s NOTICE %s :Your URL has been set to: %s",
		  NickServ, from, nick->reg->url);
	return RET_OK_DB;
}


/**
 * \brief The /NICKSERV SET URL command
 */
cmd_return ns_set_encrypt(nss_tab *setEnt, UserList *nick,
                         char *args[], int numargs)
{
	const char *from = nick->nick;
	int do_enc = 0;

	if (numargs < 2) {
		sSend(":%s NOTICE %s :Your password is %scurrently encrypted.",
			  NickServ, from, (nick->reg->flags & NENCRYPT) ? "" : "\2not\2 ");
		return RET_OK_DB;
	}

	if (!str_cmp(args[1], "ON") || !str_cmp(args[1], "ENCRYPT") ||
	    !str_cmp(args[1], "YES"))
		do_enc = 1;
	else if (!str_cmp(args[1], "OFF") || !str_cmp(args[1], "UNENCRYPT") ||
	    !str_cmp(args[1], "NO"))
		do_enc = 0;
	else {
		sSend(":%s NOTICE %s :Invalid setting.  Must be ON or OFF.",
			  NickServ, from);
	}

	if (numargs < 3) {
		sSend(":%s NOTICE %s :Your password must also be specified to "
		      "change this setting.", NickServ, from);
		return RET_OK_DB;
	}

	if (!Valid_pw(args[2], nick->reg->password, NickGetEnc(nick->reg))) {
		PutReply(NickServ, nick, ERR_BADPW, 0, 0, 0);
		if (BadPwNick(nick, nick->reg))
			return RET_KILLED;

		return RET_BADPW;
	}
	else
		GoodPwNick(nick, nick->reg);


	if (do_enc) {
		sSend(":%s NOTICE %s :You password is now encrypted within services.",
		       NickServ, from);
		nick->reg->flags |= NENCRYPT;
	}
	else {
		sSend(":%s NOTICE %s :You password is no longer encrypted within services.",
		       NickServ, from);
		nick->reg->flags &= ~NENCRYPT;
	}

	pw_enter_password(args[2], nick->reg->password, NickGetEnc(nick->reg));

	return RET_OK_DB;
}


/**
 * \brief The /NICKSERV SET IDTIME command
 */
cmd_return ns_set_idtime(nss_tab *setEnt, UserList *nick,
                         char *args[], int numargs)
{
	const char *from = nick->nick;
	int delay;

	if (numargs < 2) {
		sSend(":%s NOTICE %s :Your current ID time is: %i "
			  "seconds", NickServ, from, nick->reg->idtime);
		sSend(":%s NOTICE %s :If you wish to set your ID time,"
			  " you must specify a delay", NickServ, from);
		return RET_OK;
	}

	if ((delay = atoi(args[1]))) {
		if ((delay > ENF_MAXDELAY) || (delay < ENF_MINDELAY)) {
			sSend(":%s NOTICE %s :Invalid delay value",
				  NickServ, from);
			sSend(":%s NOTICE %s :Please enter a value in "
				  "the range of: %i - %i seconds",
				  NickServ, from, ENF_MINDELAY, ENF_MAXDELAY);
			return RET_EFAULT;
		}
	} else {
		sSend(":%s NOTICE %s :Invalid delay value", NickServ, from);
		sSend(":%s NOTICE %s :Please enter a value in the "
			  "range of: %i - %i seconds", NickServ, from,
			  ENF_MINDELAY, ENF_MAXDELAY);
		return RET_EFAULT;
	}

	nick->reg->idtime = delay;
	sSend(":%s NOTICE %s :Your ID time has been set to: %i seconds",
		  NickServ, from, nick->reg->idtime);

	return RET_OK_DB;
}


#ifdef REQ_EMAIL
/*
 * Construct an activation e-mail              -Mysid
 */
void ActivateEmail(UserList *nick, char *addy)
{
	char buf[IRCBUF * 2];
	EmailMessage email;

	email.from = "NickName services <nickserv@"NETWORK">";
	email.to = addy;
	email.subject = "Nickname verification key";
	email.body = "Someone, possibly you has specified the e-mail "
                     "contact address of: ";
	email.to += addy;
	email.to += "\n";
	email.to += "If you do not follow the instructions found in this message, \n";
	email.to += "then it will be assumed that the e-mail address specified\n";
	email.to += " was not correct.\n";
	email.to += "\n";
	sprintf(buf, "Your nickname activation key is: %lu\n", nick->reg->email_key);
	email.to += buf;
	sprintf(buf, "To activate your nickname, switch to the nickname %s and\n", nick->reg->nick);
	email.to += buf;
	sprintf(buf, "/msg %s ACTIVATE %lu\n", nick->reg->email_key);
	email.to += buf;
	email.to += "\n";
	sprintf(buf, "NOTICE: mail sent by %s@%s using the nickname %s\n", nick->user, nick->host, nick->nick);
	email.to += buf;
	sprintf(buf, "%s\n", ctime(&CTime));
	email.to += buf;
	email.to += "\n";
	email.send();
	email.reset();
}
#endif

/**
 * \brief The /NICKSERV SET EMAIL command
 */
cmd_return ns_set_email(nss_tab *setEnt, UserList *nick,
                        char *args[], int numargs)
{
	const char *from = nick->nick;

	if (numargs < 2) {
		sSend(":%s NOTICE %s :You must specify an e-mail address",
			  NickServ, from);
		return RET_EFAULT;
	}

	if (!str_cmp(args[1], "on")) {
		nick->reg->flags |= NEMAIL;
		PutReply(NickServ, nick, RPL_SWITCHNOW_ARG2,
			"public e-mail", "ON", 0);
		return RET_OK_DB;
	} else if (!str_cmp(args[1], "off")) {
		nick->reg->flags &= ~(NEMAIL);
		PutReply(NickServ, nick, RPL_SWITCHNOW_ARG2,
			"public e-mail", "OFF", 0);
		return RET_OK_DB;
	}

	if ((!strchr(args[1], '@') 
#ifndef REQ_EMAIL
	   && strcasecmp(args[1], "none")
#endif
	   ) || *args[1] == '|' || strchr(args[1], '/') || strchr(args[1], '<')
	     || strchr(args[1], '>') || strchr(args[1], '\"')) {
		sSend(":%s NOTICE %s :Please specify a valid e-mail address"
			  "in the form of user@host", NickServ, from);
		return RET_EFAULT;
	}

#ifdef REQ_EMAIL
	srandom(time(NULL));

	if (emailAbuseCheck(nick, args[1], 0))
		return;

	nick->reg->email_key = random();
	nick->reg->flags |= NDEACC;

	sSend(":%s NOTICE %s :Your nick has been temporarily de-activated to verify your new address", NickServ, from);
	sSend(":%s NOTICE %s :To re-activate please follow the same procedure as when you registered", NickServ, from);
	sSend(":%s NOTICE %s :All your previous data will be saved", NickServ, from);
	sSend(":%s NOTICE %s :Registration key sent to %s", NickServ, from, args[1]);

	ActivateEmail(nick, args[1]);
#endif

	strncpyzt(nick->reg->email, args[1], EMAILLEN);

	sSend(":%s NOTICE %s :Your email address has been set to %s",
		  NickServ, from, args[1]);
	if (!strcasecmp(args[1], "none")) {
		sSend(":%s NOTICE %s :Please note: should you forget your "
			  "password and have no e-mail address set, your password "
			  "cannot be recovered for you.", NickServ, from);
		strcpy(nick->reg->email, "(none) ");
	}

	return RET_OK_DB;
}



/**
 * \nscmd Set
 * \syntax Set \<setting> *{ \<new value> }
 * \plr Set various NickName options such as kill enforcement
 *        or idtime.
 * \plr -Mysid
 */
NCMD(ns_set)
{
	char *from = nick->nick;
	int i;

        // *INDENT-OFF*
	nss_tab ns_set_table[] = {
		{ "enforce",	"nick protection",
			NSS_FLAG,	NKILL,		0,		},
		{ "kill",		"nick protection",
			NSS_FLAG,	NKILL, 		0,		},
		{ "ident",		"identification requirement",
			NSS_FLAG,	NIDENT,		0,		},
		{ "noaddop",	"Noaddop option",
			NSS_FLAG,	NOADDOP,	0,		},
		{ "terse",		"Terse option",
			NSS_FLAG,	NTERSE,		0,		},
		{ "maskadd",	"hide mask option",
			NSS_FLAG,	NDBISMASK,	0,		},
		{ "iphide",		"hide mask option",
			NSS_FLAG,	NDBISMASK,	0,		},
		{ "hideip",		"hide mask option",
			NSS_FLAG,	NDBISMASK,	0,		},
		{ "showemail",	"public e-mail",
			NSS_FLAG,	NEMAIL,		0,		},
		{ "passwd",		"password",
			NSS_PROC,	0,			0,		ns_set_passwd },
		{ "password",	"password",
			NSS_PROC,	0,			0,		ns_set_passwd },
		{ "url",		"url",
			NSS_PROC,	0,			0,		ns_set_url },
		{ "idtime",		"identify delay",
			NSS_PROC,	0,			0,		ns_set_idtime },
		{ "email",		"e-mail address",
			NSS_PROC,	0,			0,		ns_set_email },
		{ "address",	"e-mail address",
			NSS_PROC,	0,			0,		ns_set_email },
		{ "encrypt",	"encrypted password",
			NSS_PROC,	0,			0,		ns_set_encrypt },
		{ NULL,		NULL,
			(ns_set_en)0,	0,		0,		NULL },
	};
        // *INDENT-ON*


	if (addFlood(nick, 2))
		return RET_KILLED;

	if (numargs < 2) {
		sSend(":%s NOTICE %s :A variable is required with NickServ SET",
			  NickServ, from);
		sSend(":%s NOTICE %s :See /msg %s HELP SET", NickServ, from,
			  NickServ);

		return RET_SYNTAX;
	}

	if (!isIdentified(nick, nick->reg)) {
		PutError(NickServ, nick, ERR_NOTIDENTIFIED, 0, 0, 0);
		return RET_NOPERM;
	}


	for (i = 0; ns_set_table[i].keyword; i++)
		if (!str_cmp(ns_set_table[i].keyword, args[1]) &&
			(!ns_set_table[i].opFlagsRequired ||
			 opFlagged(nick, ns_set_table[i].opFlagsRequired)))
			break;

	if (!ns_set_table[i].keyword) {
		sSend(":%s NOTICE %s :Unknown set command %s, please "
			  "try /msg %s HELP SET", NickServ, from, args[1], NickServ);
		return RET_SYNTAX;
	}

	if (ns_set_table[i].set_type == NSS_FLAG) {
		int flag = ns_set_table[i].flag;

		if (numargs < 3) {
			PutReply(NickServ, nick, RPL_SWITCHIS_ARG2,
					 ns_set_table[i].description,
					 !(nick->reg->flags & (flag)) ? "off" : "on", 0);

			return RET_OK;
		} else if (!str_cmp(args[2], "on")) {
			nick->reg->flags |= flag;
			PutReply(NickServ, nick, RPL_SWITCHNOW_ARG2,
					 ns_set_table[i].description, "ON", 0);
		} else if (!str_cmp(args[2], "off")) {
			nick->reg->flags &= ~(flag);
			PutReply(NickServ, nick, RPL_SWITCHNOW_ARG2,
					 ns_set_table[i].description, "OFF", 0);
		} else {
			sSend(":%s NOTICE %s :Unknown variable for SET %s",
				  NickServ, from, ns_set_table[i].keyword);
			sSend(":%s NOTICE %s :Syntax: /msg %s SET %s <on|off>",
				  NickServ, from, NickServ, ns_set_table[i].keyword);

			return RET_SYNTAX;
		}

		return RET_OK_DB;
	}
	else if (ns_set_table[i].set_type == NSS_PROC) {
		return (* ns_set_table[i].proc)(&ns_set_table[i], nick,
										args+1, numargs-1);
	}

	return RET_OK;
}


/**
 * \nscmd Register
 * \syntax Register \<email>
 * \syntax Register \<password> \<email>
 *
 * \plr Request a new nickname, activation code sent to \<email>
 * \plr Arguments:
 *     -# argv[1] == password
 *     -# argv[2] == email (if supplied)
 */
NCMD(ns_register)
{
	char *from;
	char user2[USERLEN];
	char host2[HOSTLEN];
	char pw_reason[IRCBUF];
	char mailTemp[EMAILLEN] = "(none)";
	char *p;
	int wasvalid = 0;
	unsigned int i = 0;
#ifdef REQ_EMAIL
	time_t doot = time(NULL);

	srandom(time(NULL));
#endif

	from = nick->nick;

	if (addFlood(nick, 20))
		return RET_KILLED;

	if (nick->reg != NULL) {
		sSend(":%s NOTICE %s :This nickname is already registered.",
			  NickServ, from);
		return RET_EFAULT;
	}

	for (i = 0; i < NUM_CNICKS; i++)
		if (!strncasecmp
			(cnick_prefixes[i], nick->nick, strlen(cnick_prefixes[i]))) {
			sSend
				(":%s NOTICE %s :The nick that you are using is a temporary nickname owned by services: it cannot be registered.",
				 NickServ, from);
			return RET_EFAULT;
		}

	if (!strncasecmp("Auth-", nick->nick, strlen("Auth-"))) {
		sSend
			(":%s NOTICE %s :The nick that you are using is a temporary nickname owned by services: it cannot be registered.",
			 NickServ, from);
		return RET_EFAULT;
	}

	if (numargs < 2) {
		sSend(":%s NOTICE %s :You must specify a password to register",
			  NickServ, from);
		return RET_SYNTAX;
	}

	if (isPasswordAcceptable(args[1], pw_reason) == 0) {
		sSend(":%s NOTICE %s :Sorry, %s isn't a password that you can use.",
			NickServ, from, args[1]);
		sSend(":%s NOTICE %s :%s", NickServ, from, pw_reason);
		return RET_EFAULT;
	}

	if (strlen(args[1]) > 15) {
		sSend(":%s NOTICE %s :Please specify a password of "
			  "%d characters or less", NickServ, from, PASSLEN);
		return RET_EFAULT;
	}

	if (strcasecmp(args[1], from) == 0) {
		sSend(":%s NOTICE %s :Your password cannot be your nickname.",
			  NickServ, from);
		return RET_EFAULT;
	}

	if (strchr(args[1], '@')
		|| (args[1] == strchr(args[1], '<') && (p = strrchr(args[1], '>'))
			&& !*(p + 1))) {
		if (strchr(args[1], '@'))
			sSend
				(":%s NOTICE %s :Your registration password cannot contain the @ symbol.",
				 NickServ, from);
		if (args[1] == strchr(args[1], '<') && strchr(args[1], '>')) {
			sSend
				(":%s NOTICE %s :Your registration password should not be surrounded with <>.",
				 NickServ, from);
			sSend
				(":%s NOTICE %s :The '<' and '>' symbols surrounding a word in services' helpfiles is an indicator that 'password' is a field that you should specify a value for.",
				 NickServ, from);
		}
		sSend(":%s NOTICE %s :\2/msg %s HELP REGISTER\2 for assistance",
			  NickServ, from, NickServ);
		return RET_EFAULT;
	}

        if (strlen(args[1]) < 5)
        {
                sSend(":%s NOTICE %s :Your password cannot be %s.  A password should be impossible to guess and must have a length of 5 or more characters (letters, numbers, and symbols).",
                      NickServ, from, args[1]);
		return RET_EFAULT;
        }

	if (strcasecmp(args[1], "password") == 0 || strcasecmp(args[1], "<password>") == 0
		|| strcasecmp(args[1], "1234") == 0 || strcasecmp(args[1], "fz345") == 0
		|| strcasecmp(args[1], "word") == 0)
	{
		sSend(":%s NOTICE %s :Your password cannot be %s.", NickServ, from,
			  args[1]);
		return RET_EFAULT;
	}

	if (numargs < 3) {
#ifndef REQ_EMAIL
		sSend
			(":%s NOTICE %s :To register you must also either specify your e-mail address or NONE.",
			 NickServ, from);
		sSend
			(":%s NOTICE %s :E-mail addresses provided are treated as private material and are used ONLY to assist with lost passwords.",
			 NickServ, from);
		sSend
			(":%s NOTICE %s :Specifying an e-mail address of NONE indicates that, should you lose your password, operators will be unable to retrieve the password for you.",
			 NickServ, from);
		sSend
			(":%s NOTICE %s :examples: /msg nickserv register \2MYSECRETCODE\2 \2me@myemail.com\2",
			 NickServ, from);
		sSend
			(":%s NOTICE %s :          /msg nickserv register \2MYSECRETCODE\2 \2NONE\2",
			 NickServ, from);
#else
		sSend(":%s NOTICE %s :You must specify an e-mail address and a "
		      "password to register.", NickServ, from);
#endif
		return RET_SYNTAX;
	}

#ifdef REQ_EMAIL
	if (numargs > 2)
#else
	if (numargs > 2 && strcasecmp(args[2], "none"))
#endif
		if (!strchr(args[2], '@') || !strchr(args[2], '.'))
		{
			sSend
				(":%s NOTICE %s :specified a e-mail address is invalid. "
				 "Must be of the form user@host", NickServ, from);
#ifdef EMAIL_OK_IF_SPECIFIED
			return;
#endif
		}
		else {
			wasvalid = 1;
			strncpyzt(mailTemp, args[2], EMAILLEN);
	} else if (numargs > 2 && !strcasecmp(args[2], "none"))
		strcpy(mailTemp, "(none) ");
	else
		strcpy(mailTemp, "(none)");

	if (wasvalid == 0)
		sSend
			(":%s NOTICE %s :To enable us to recover your password in the event that it is lost, you must specify your e-mail address by using /msg NickServ set address <email address>",
			 NickServ, from);

#ifdef REQ_EMAIL
	if (emailAbuseCheck(nick, args[1], 1))
		return;
#endif

	strncpyzt(user2, nick->user, USERLEN);
	strncpyzt(host2, nick->host, HOSTLEN);

	nick->reg = (RegNickList *) oalloc(sizeof(RegNickList));

	strncpyzt(nick->reg->nick, nick->nick, NICKLEN);
	strncpyzt(nick->reg->user, nick->user, USERLEN);
	SetDynBuffer(&nick->reg->host, nick->host);
	strncpyzt(nick->reg->email, mailTemp, EMAILLEN);
#ifdef TRACK_GECOS
	SetDynBuffer(&nick->reg->gecos, nick->gecos);
#endif

	nick->reg->timestamp = CTime;
	nick->reg->timereg = CTime;
#ifdef REQ_EMAIL
	nick->reg->email_key = random();
#endif
	nick->caccess = 3;
	nick->reg->flags |= NENCRYPT;
	pw_enter_password(args[1], nick->reg->password, NickGetEnc(nick->reg));

	nicklog->log(nick, NS_REGISTER, nick->nick);
	ADD_MEMO_BOX(nick->reg);
	mostnicks++;

	nick->reg->amasks = 0;
	nick->reg->url = NULL;
	nick->reg->chans = 0;
	nick->reg->idtime = DEF_NDELAY;
	nick->reg->regnum.SetNext(top_regnick_idnum);

	addRegNick(nick->reg);
#ifdef REQ_EMAIL
	ActivateEmail(nick, args[1]);
#endif

	if (nick->oflags & NOISMASK)
		nick->reg->flags |= NDBISMASK;
#if defined(AHURT_BYPASS_BY_DEFAULT)
	if (!
		(isAHurt(tmp->nick, tmp->user, tmp->host)
		 || isAKilled(tmp->nick, tmp->user,
					  tmp->host))) nick->reg->flags |= NBYPASS;
	else {
		sSend
			(":%s NOTICE %s :Warning: you will not be able to use this registration to bypass select bans without further assistance.",
			 NickServ, nick->nick);
		sSend
			(":%s NOTICE %s :This is true for all nicks registered from a domain while selective users from it are being banned for security reasons.",
			 NickServ, nick->nick);
	}
#endif

	sSend
		(":%s NOTICE %s :Your nickname has been registered with the password %s",
		 NickServ, from, args[1]);

	if (nick->oflags & NOISMASK)
		nick->reg->flags |= NDBISMASK;
	return RET_OK_DB;
}


/**
 * \nscmd Info
 * \plr NinfoCmd(nick, args, numargs)
 * - args[0..1]
 * - args[0] = "info"
 * - args[1] = [nick]
 *
 * \syntax /msg NickServ Info [\<nick>]
 *
 * \plr Simple function.. checks various things on the nick and reports them
 * to users and opers.
 *
 * \plr Revision History:
 * Mon Jun 15 15:23:30 PDT 1998 by some2
   - Fixed Bug report 90
     - New replies if a user is online
   - Added this header
   
 - Wed Jun 16 11:45:20 CDT 1999 by dave
   - Reply is shortened for those with terse enabled
  - Sat Oct  2 11:05:53 PDT 1999 by Echostar
    - Added info replies for GRPOP status
 - Fri Oct 09:46:36 CST 2001 by Mysid
    - Major revamp.
 */
void SendNickInfo(UserList *nick, RegNickList *rnl, int terseOption)
{
	char *from = nick->nick;
	struct tm *time;
	char temp[80];
	unsigned int j;
	UserList *online = NULL;
	struct {
		const char *optname;
		const char *optline;
		flag_t bit;
		int verbatim, oper_only;
 	} regnick_flag_tab[] = {
		{ "Enforce", "nick protection",					NKILL, 0 },
		{ "Held",    "This nickname will not expire.",	NHOLD, 1 },
		{ "Vacation", "This nickname has vacation set and has an "
                      "extended expiration period.", NVACATION, 1 },
		{ "Nosendpass", NULL, NOSENDPASS, 0, 1}, 
		{ NULL }
 	};

	sSend(":%s NOTICE %s :Information for %s:", NickServ, from, rnl->nick);

	if ((online = getNickData(rnl->nick))) {
		if (isIdentified(online, rnl)) {
				PutReply(NickServ, nick, RPL_INFONLINE_ID, rnl->nick, online->caccess, 0);
		} else if (isRecognized(online, rnl)) {
				PutReply(NickServ, nick, RPL_INFONLINE_NOID, rnl->nick, online->caccess, 0);

				/* sSend(":%s NOTICE %s :%s is online, but not identified",
					  NickServ, from, rnl->nick); */
		}
	}

	if (rnl->flags & NBANISH) {
		PutReply(NickServ, nick, RPL_NS_BANISH, rnl->nick, 0, 0);
		PutReply(NickServ, nick, RPL_NS_ENDINFO, rnl->nick, 0, 0);
		return;
	}

#ifdef REQ_EMAIL
	if (!(rnl->flags & NACTIVE)) {
		sSend(":%s NOTICE %s :%s is not activated.",
			 NickServ, from, rnl->nick);
		if (!isOper(nick))
			return;
	}
#endif
#ifdef TRACK_GECOS
	if (rnl->gecos)
		sSend(":%s NOTICE %s :%s[%s@%s] (%s)", NickServ, from,
			  rnl->nick, rnl->user, regnick_ugethost(nick, rnl),
			  rnl->gecos);
	else
#endif
		sSend(":%s NOTICE %s :%s[%s@%s]", NickServ, from,
			  rnl->nick, rnl->user, regnick_ugethost(nick, rnl));

	if (rnl->url)
		sSend(":%s NOTICE %s :Url            :  %s", NickServ, from,
			  rnl->url);


	if (((rnl->flags & NEMAIL) || isIdentified(nick, rnl))
		&& rnl->email[0])
		sSend(":%s NOTICE %s :Email          :  %s", NickServ, from,
			  rnl->email);
	do
	{
		char optBuf[IRCBUF];

		time = localtime(&rnl->timestamp);
		if ( strftime(temp, 80, "%a %Y-%b-%d %T %Z", time) > 0 )
			sSend(":%s NOTICE %s :Last seen time :  %s", NickServ, from, temp);

		time = localtime(&rnl->timereg);

		if ( strftime(temp, 80, "%a %Y-%b-%d %T %Z", time) > 0 )
			sSend(":%s NOTICE %s :Registered at  :  %s", NickServ, from, temp);

		time = localtime(&CTime);

		if ( strftime(temp, 80, "%a %Y-%b-%d %T %Z", time) > 0 )
			sSend(":%s NOTICE %s :Current time   :  %s", NickServ, from, temp);

		memset(optBuf, 0, sizeof(optBuf));

		for(j = 0; regnick_flag_tab[j].optname; j++)
		{
			if (regnick_flag_tab[j].oper_only && !isOper(nick))
				continue;

			if (terseOption || !regnick_flag_tab[j].optline) {
				if ((rnl->flags & regnick_flag_tab[j].bit) &&
				    (strlen(optBuf) < (IRCBUF - strlen(regnick_flag_tab[j].optname) - 25)))
				{
					if (*optBuf)
						strcat(optBuf, ", ");
					strcat(optBuf, regnick_flag_tab[j].optname);
				}
				continue;
			}

			if (rnl->flags & regnick_flag_tab[j].bit)
			{
				if (!regnick_flag_tab[j].verbatim)
					sSend(":%s NOTICE %s :This user has enabled \2%s\2.", NickServ,
						  from, regnick_flag_tab[j].optline);
				else
					sSend(":%s NOTICE %s :%s", NickServ,
						 from, regnick_flag_tab[j].optline);
			}
		}

		if (terseOption) {
			if (rnl->memos) {
				/* Note, 22 characters space is allowed for above, 8 of that
                 * is used here.
                 */
				if (rnl->memos->flags & MNOMEMO)
					sprintf(optBuf + strlen(optBuf), "%sNoMemo",
					        *optBuf ? ", " : "");
			}
		}	

		if (rnl->memos) {
			if (!terseOption && (rnl->memos->flags & MNOMEMO))
				sSend(":%s NOTICE %s :%s has chosen not to receive memos",
					  NickServ, from, rnl->nick);
			if (rnl->memos->flags & MFORWARDED)
				sSend(":%s NOTICE %s :Memos forwarded to: %s",
					 NickServ, from, rnl->memos->forward->nick);
		}

		if (*optBuf)
			sSend(":%s NOTICE %s :Nick options   :  %s", NickServ, from,
				  optBuf);
#ifdef ENABLE_GRPOPS
		if (((rnl->opflags & OGRP) && !(rnl->opflags & OROOT)))
			sSend(":%s NOTICE %s :User is a GRP-op.",
				 NickServ, from);
		else if (!isOper(nick) && (rnl->opflags & OROOT))
			sSend(":%s NOTICE %s :User is a SRA and GRP-op.",
				 NickServ, from);
#else
		else if (!isOper(nick) && (rnl->opflags & OROOT))
			sSend(":%s NOTICE %s :User is a Services root admin, GRP-op.",
				 NickServ, from);
#endif
		else
		if (online && !isOper(nick) && (isHelpop(online) || isOper(online)))
			sSend(":%s NOTICE %s :User can mail lost passwords.", NickServ, from);
	} while(0);

	if (isOper(nick) == 1) {
		if (!terseOption)
		{
			sSend(":%s NOTICE %s :+++ IRCop Data +++", NickServ, from);
			if (rnl->opflags & OROOT)
				sSend(":%s NOTICE %s :+ This user is a Warlock/Sorceress (Services Root)",
					 NickServ, from);
			else if (rnl->opflags & OPFLAG_ROOTSET)
				sSend(":%s NOTICE %s :+ This user is a Wizard/Witch (Services Operator)",
					 NickServ, from);
		}
		else
		{
			if (rnl->opflags & OROOT)
				sSend(":%s NOTICE %s :+ Services root admin.",
					 NickServ, from);
			else if (rnl->opflags & OPFLAG_ROOTSET)
				sSend(":%s NOTICE %s :+ Services operator.",
					 NickServ, from);
		}

		{
			char buf[EMAILLEN+15];

			*buf = '\0';

			if (((rnl->flags & NEMAIL) == 0) && (rnl->email[0])
	                    && !isIdentified(nick, rnl))
				snprintf(buf, sizeof(buf), ", Prv Email: %s", rnl->email);
			sSend(":%s NOTICE %s :+ Masks: %i, Bypass: %s%s", NickServ, from,
				  rnl->amasks, (rnl->flags & NBYPASS) ? "Yes" : "No", buf);
		}


		if (online) {
			sSend(":%s NOTICE %s :+ %s flood level: %i/100", NickServ,
				  from, rnl->nick, online->floodlevel.GetLev());
		}
		if ((rnl->flags & NMARK)) {
			sSend(":%s NOTICE %s :+ %s MARKED%s%s", NickServ,
				  from, rnl->nick, rnl->markby ? " by " : "",
				  rnl->markby ? rnl->markby : "");
		}
	}

	if (isHelpop(nick) == 1 && isOper(nick) == 0) {
		if (((rnl->flags & NEMAIL) == 0) && (rnl->email[0]))
			sSend(":%s NOTICE %s :+ Email address: Hidden (not displayed)",
				  NickServ, from);
	}
	sSend(":%s NOTICE %s :End of info on %s", NickServ, from,
		  rnl->nick);
}

/// Query NickServ for information on a nickname
NCMD(ns_info)
{
	char *from = nick->nick;
	unsigned int i;
	int terseOption, k;
	RegNickList *regnick;

	if (addFlood(nick, 5))
		return RET_KILLED;

	terseOption = (nick->reg && (nick->reg->flags & NTERSE));

	for(k = 1; k < numargs; k++)
	{
		if (args[k][0] != '-')
			break;
		if (strcmp(args[k], "-short") == 0 || strcmp(args[k], "-terse") == 0)
			terseOption = 1;
		else if (strcmp(args[k], "-long") == 0 || strcmp(args[k], "-verbose") == 0)
			terseOption = 0;
	}

	if (k < numargs)
		regnick = getRegNickData(args[k]);
	else
		regnick = nick->reg;

#if defined(NICKSERV_INFO_TZ_ARG)
	if (regnick && numargs > 2 && args[2])
		tzapply(args[2]);
#endif

	if (numargs > 1)
		for (i = 0; i < NUM_CNICKS; i++)
			if (!strncasecmp
				(cnick_prefixes[i], args[k], strlen(cnick_prefixes[i]))) {
				sSend
					(":%s NOTICE %s :That is a services temporary user nickname.",
					 NickServ, from);
				return RET_OK;
			}

	if (regnick == NULL) {
		sSend(":%s NOTICE %s :%s is not registered", NickServ, from,
			  numargs > 1 ? args[k] : from);
		return RET_FAIL;
	}

	SendNickInfo(nick, regnick, terseOption);

#if defined(NICKSERV_INFO_TZ_ARG)
	if (numargs > 2 && args[2])
		tzapply(DEF_TIMEZONE);
#endif
	return RET_OK;
}

/**
 * \brief Drop a registered nickname
 */
NCMD(ns_drop)
{
	char *from = nick->nick;

	if (addFlood(nick, 3))
		return RET_KILLED;

	if (!nick->reg) {
		sSend(":%s NOTICE %s :Your present nickname is not registered.",
			  NickServ, nick->nick);
		return RET_EFAULT;
	}

	if ((nick->reg->flags & NBANISH)) {
		sSend
			(":%s NOTICE %s :Sorry, but that nickname is currently banished.",
			 NickServ, from);
		return RET_FAIL;
	}

	if (numargs > 1) {
		if (!Valid_pw(args[1], nick->reg->password, NickGetEnc(nick->reg))) {
			sSend(":%s NOTICE %s :Incorrect password.", NickServ,
				  nick->nick);
			BadPwNick(nick, nick->reg);
			return RET_BADPW;
		}
		else
			GoodPwNick(nick, nick->reg);
	}

	if (!isIdentified(nick, nick->reg)) {
		sSend(":%s NOTICE %s :You must identify to drop your nick",
			  NickServ, nick->nick);
		return RET_NOPERM;
	}

	mostnicks--;
	sSend(":%s NOTICE %s :Your nick %s has been dropped.",
		  NickServ, from, from);
	nicklog->log(nick, NS_DROP, nick->nick);
	delRegNick(nick->reg);
	nick->reg = NULL;
	return RET_OK_DB;
}

/**
 * \nscmd Addmask
 * \syntax Addmask
 * \plr Add a default hostmask
 */
NCMD(ns_addmask)
{

	if (addFlood(nick, 2))
		return RET_KILLED;

	if (!isIdentified(nick, nick->reg)) {
		sSend(":%s NOTICE %s :You must identify to use ADDMASK", NickServ,
			  nick->nick);
		return RET_NOPERM;
	}

	if (numargs < 2) {
		char *user, *host, theirmask[USERLEN + HOSTLEN + 3];

		user = host = NULL;

		SetDynBuffer(&user, nick->user);
		SetDynBuffer(&host, nick->host);
		mask(user, host, 0, theirmask);
		addAccessMask(theirmask, nick);
		SetDynBuffer(&user, NULL);
		SetDynBuffer(&host, NULL);

		return RET_OK_DB;
	} else if (numargs < 4) {
		if (!strcasecmp(args[1], "S")) {
			addAccessMask(args[2], nick);
			return RET_OK_DB;
		}
	}

	sSend(":%s NOTICE %s :Incorrect use of ADDMASK", NickServ, nick->nick);
	return RET_SYNTAX;
}

/**
 * \nscmd Access
 * \syntax Access add \<mask>
 * \syntax Access del \<mask>
 * \syntax Access del \<number>
 * \syntax Access del -
 *
 * \plr \b Purpose: Maintain a NickName's access list
 * \par
 * - /ns access add user@*.host.com   -- add access mask
 * - /ns access del user@*.host.com   -- del access mask
 * - /ns access del 1                 -- del access mask #1
 * - /ns access del -                 -- wipe access list clean
 */
NCMD(ns_access)
{
	char *from = nick->nick;
	nAccessList *acl;
	int idx;

	if (addFlood(nick, 2))
		return RET_KILLED;

	if (numargs < 2) {
		sSend(":%s NOTICE %s :Your access is %i",
			  NickServ, nick->nick, nick->caccess);
		return RET_OK;
	}

	if (strcasecmp(args[1], "list") == 0) {
		if (numargs < 3) {
			if (!nick->reg) {
				sSend(":%s NOTICE %s :Your nickname is not registered.",
					  NickServ, from);
				return RET_FAIL;
			}
			if (!isIdentified(nick, nick->reg)) {
				sSend(":%s NOTICE %s :You must identify "
					  "to get the access list", NickServ, from);
				return RET_NOPERM;
			}

			if (LIST_FIRST(&nick->reg->acl) == NULL) {
				sSend(":%s NOTICE %s :Access list for %s"
					  " is empty", NickServ, nick->nick, nick->nick);
				return RET_OK;
			}

			sSend(":%s NOTICE %s :Access list for %s:",
				  NickServ, nick->nick, nick->nick);

			idx = 1;
			for (acl = LIST_FIRST(&nick->reg->acl); acl != NULL;
				 acl = LIST_NEXT(acl, al_lst)) {
				sSend(":%s NOTICE %s :%2i:  %s",
					  NickServ, nick->nick, idx++, acl->mask);
			}
			sSend(":%s NOTICE %s :End of list for %s",
				  NickServ, nick->nick, nick->nick);
		} else {
			RegNickList *tmpnick;

			tmpnick = getRegNickData(args[2]);

			if (tmpnick == NULL) {
				sSend(":%s NOTICE %s :%s is not registered",
					  NickServ, from, args[2]);
				return RET_NOTARGET;
			}

			if (!isOper(nick) && !isIdentified(nick, tmpnick)) {
				sSend(":%s NOTICE %s :Access denied to list for %s",
					  NickServ, nick->nick, args[2]);
				return RET_NOPERM;
			}

			if (LIST_FIRST(&tmpnick->acl) == NULL) {
				sSend(":%s NOTICE %s :Access list for %s"
					  " is empty", NickServ, nick->nick, tmpnick->nick);
				return RET_OK;
			}

			sSend(":%s NOTICE %s :Access list for %s:",
				  NickServ, nick->nick, args[2]);

			idx = 1;
			for (acl = LIST_FIRST(&tmpnick->acl); acl != NULL;
				 acl = LIST_NEXT(acl, al_lst))
				sSend(":%s NOTICE %s :%2i:  %s",
					  NickServ, nick->nick, idx++, acl->mask);

			sSend(":%s NOTICE %s :End of list for %s",
				  NickServ, nick->nick, args[2]);
		}
	} else if (strcasecmp(args[1], "add") == 0) {
		if (!nick->reg) {
			sSend(":%s NOTICE %s :Your nickname is not registered.",
				  NickServ, from);
			return RET_FAIL;
		}
		if (numargs < 3) {
			sSend(":%s NOTICE %s :You must specify a mask to add",
				  NickServ, nick->nick);
			return RET_SYNTAX;
		}

		addAccessMask(args[2], nick);
		return RET_OK_DB;
	} else if (strncasecmp(args[1], "del", 3) == 0) {
		/* also accepts delete */
		if (numargs < 3) {
			sSend(":%s NOTICE %s :You must specify a mask to delete",
				  NickServ, nick->nick);
			return RET_SYNTAX;
		}

		if (!nick->reg) {
			sSend(":%s NOTICE %s :Your nickname is not registered.",
				  NickServ, from);
			return RET_FAIL;
		}

		delAccessMask(args[2], nick);
		return RET_OK_DB;
	} else {
		UserList *info;

		info = getNickData(args[1]);

		if (info == NULL) {
			sSend(":%s NOTICE %s :%s is not online, access is \0020\002",
				  NickServ, nick->nick, args[1]);
			return RET_OK;
		} else {
			if (!isOper(nick) && nick != info)
				sSend
					(":%s NOTICE %s :Current access level for \002%s\002!%s@%s is \002%i\002",
					 NickServ, nick->nick, info->nick, info->user, genHostMask(info->host),
					 info->caccess);
			else
				sSend
					(":%s NOTICE %s :Current access level for \002%s\002!%s@%s is \002%i\002",
					 NickServ, nick->nick, info->nick, info->user, info->host,
					 info->caccess);
			return RET_OK;
		}
	}
	return RET_OK;
}

/**
 * \nscmd Acc
 * \syntax Acc {[\<nick>] | \<nick1> \<nick2>}
 * \plr Check identified/NickServ recognition level of a user  -Mysid
 */
NCMD(ns_acc)
{
	char *from = nick->nick;
	RegNickList *tmpnick;

	if (addFlood(nick, 2))
		return RET_KILLED;

	if (numargs < 2) {
		tmpnick = getRegNickData(from);
	} else {
		if (numargs < 3)
			tmpnick = getRegNickData(args[1]);
		else
			tmpnick = getRegNickData(args[2]);
	}

	{
		UserList *info;
		char *ustr;
		char *nstr;

		if (numargs >= 2)
			info = getNickData(args[1]);
		else
			info = nick;
		ustr = (numargs > 1) ? args[1] : from;
		nstr = (numargs > 2) ? args[2] : (numargs > 1) ? args[1] : from;

		if (!tmpnick) {
			if ((numargs > 2))
				sSend(":%s NOTICE %s :%s -> %s ACC 0 (not registered)",
					  NickServ, nick->nick, ustr, nstr);
			else
				sSend(":%s NOTICE %s :%s ACC 0 (not registered)",
					  NickServ, nick->nick, ustr);
			return RET_OK;
		} else if (info == NULL) {
			if ((numargs > 2))
				sSend(":%s NOTICE %s :%s -> %s ACC 0 (offline)",
					  NickServ, nick->nick, args[1], nstr);
			else
				sSend(":%s NOTICE %s :%s ACC 0 (offline)",
					  NickServ, nick->nick,
					  (numargs > 1) ? args[1] : from);
			return RET_OK;
		} else {
			if ((numargs > 2)) {
				int cacc = 1;

				if (isRecognized(info, tmpnick))
					cacc = 2;
				if (isIdentified(info, tmpnick))
					cacc = 3;
				if ((info->reg == tmpnick) && (info->caccess > 3))
					cacc = info->caccess;

				sSend(":%s NOTICE %s :%s -> %s ACC %i",
					  NickServ, nick->nick, ustr, nstr, cacc);
			} else
				sSend(":%s NOTICE %s :%s ACC %i",
					  NickServ, nick->nick, info->nick, info->caccess);
		}
	}
	return RET_OK;
}


/**
 * \nscmd Ghost
 * \plr NghostCmd(nick, args, numargs)
 * - args[1..2]:
 * - args[0] = "ghost"
 * - args[1] = nick
 * - args[2] = <password>
 * \plr
 * - Used to 'ghost', or remove a non-existing (ghost) clients via NickServ.
 *
 * - Command Syntax: /msg NickServ GHOST [nickname] <password>
 *
 * - Modification History (most recent at top):
 * - Mon Jun 15 13:39:55 PDT 1998 by some2:
    - Added top comments, fixed parenthesis and spacing
      Added code to check that the user being ghosted is recorded by
      services as existing
 *
 */
NCMD(ns_ghost)
{
	RegNickList *tmp;
	UserList *taruser;
	char *from = nick->nick;

	if (addFlood(nick, 3))
		return RET_KILLED;

	if (numargs < 2) {
		sSend
			(":%s NOTICE %s :you must specify an argument after the GHOST command",
			 NickServ, nick->nick);
		return RET_SYNTAX;
	}

#ifndef ALLOW_GHOST_YOURSELF
	if (!strcasecmp(nick->nick, args[1])) {
		sSend(":%s NOTICE %s :You cannot ghost yourself", NickServ, from);
		return RET_EFAULT;
	}
#endif

	if (getGhost(args[1])) {
		tmp = getRegNickData(args[1]);
		if (tmp == NULL) {
			sSend(":%s NOTICE %s :%s is not registered", NickServ, from,
				  args[1]);
			return RET_EFAULT;
		}

		if (Valid_pw(args[2], tmp->password, NickGetEnc(tmp)) ||
			( !(tmp->flags & NIDENT) &&
			  checkAccess(tmp->user, tmp->host, nick->reg) )
		)
		{
			sSend(":%s NOTICE %s :%s has been ghosted", NickServ,
				  nick->nick, args[1]);
			if (isGhost(args[1])) {
				delGhost(args[1]);
			}
		}
		return RET_OK;
	}


	tmp = getRegNickData(args[1]);
	taruser = getNickData(args[1]);

	if (tmp != NULL)
		if (checkAccess(nick->user, nick->host, tmp)
			|| Valid_pw(args[2], tmp->password, NickGetEnc(tmp))) {
			taruser = getNickData(args[1]);

			if (!taruser && (getGhost(args[1]) == NULL)) {
				sSend(":%s NOTICE %s :%s is not online.", NickServ,
					  nick->nick, args[1]);
			}

			if (!taruser)
				sSend(":%s KILL %s :%s!%s (Ghosted. By: %s (%s@%s)", NickServ,
					  args[1], services[1].host, NickServ, nick->nick, nick->user, nick->host);
			else {
				if (strcmp(nick->host, taruser->host))
					sSend(":%s KILL %s :%s!%s (Ghosted. By: %s (%s@%s))",
						  NickServ, args[1], services[1].host, NickServ,
						  nick->nick, nick->user,
						  (nick->oflags & NOISMASK) ? genHostMask(nick->
																  host) :
						  nick->host);
				else
					sSend(":%s KILL %s :%s!%s (Ghosted. By: %s)", NickServ,
						  args[1], services[1].host, NickServ, nick->nick);

			}

			sSend(":%s NOTICE %s :Removing ghost, '%s'", NickServ,
				  nick->nick, args[1]);
			remUser(args[1], 1);
			if (isGhost(args[1])) {
				delGhost(args[1]);
			}
			return RET_OK;
		} else if ((tmp == NULL)) {
			sSend(":%s NOTICE %s :%s is not registered.", NickServ,
				  nick->nick, args[1]);
			return RET_EFAULT;
		} else {
			PutReply(NickServ, nick, ERR_NOACCESS, 0, 0, 0);
			return RET_NOPERM;
		}
	return RET_FAIL;
}


/**
 * \nscmd Recover
 * \plr ns_recover(nick, args, numargs)
 * - args[1..2]:
 * - args[0] = "recover"
 * - args[1] = nick
 * - args[2] = password
 * \plr
 * - Used to recover an owned nickname in use by another user
 *
 * - Command Syntax: /msg NickServ RECOVER <nickname> <password>
 *
 * - Created by Mysidia (jmh): based on ns_ghost
 */

NCMD(ns_recover)
{
	RegNickList *tmp;
	UserList *taruser;
	char *from = nick->nick;

	if (addFlood(nick, 3))
		return RET_KILLED;

	if (numargs < 2) {
		sSend
			(":%s NOTICE %s :you must specify an argument after the	RECOVER command",
			 NickServ, nick->nick);
		return RET_SYNTAX;
	}


	if (!strcasecmp(nick->nick, args[1])) {
		sSend(":%s NOTICE %s :You are already using the nickname.", NickServ, from);
		return RET_EFAULT;
	}

	tmp = getRegNickData(args[1]);
	if (tmp == 0) {
		PutReply(NickServ, nick, ERR_NICKNOTREG_1ARG, args[1], 0, 0);
		return RET_EFAULT;
	}
	
	
	if (numargs >= 3)
	{
		struct auth_data auth_info[] = {{
			nick->auth_cookie,
			nick->idnum.getHashKey(),
			2
		}};
		
		if (isMD5Key(args[2]))
		{
			if (!Valid_md5key(args[2], auth_info, tmp->nick, tmp->password, NickGetEnc(tmp))) 
			{
				sSend(":%s NOTICE %s :Invalid MD5 key.", NickServ, nick->nick);
				nick->auth_cookie = 0;
                       		 if (BadPwNick(nick, tmp))
                       		         return RET_KILLED;

				return RET_BADPW;
			}
			nick->auth_cookie = 0;
		}
		else if (!Valid_pw(args[2], tmp->password, NickGetEnc(tmp))) {
			PutReply(NickServ, nick, ERR_BADPW_NICK_1ARG, args[1], 0, 0);
                        if (BadPwNick(nick, tmp))
                                return RET_KILLED;

			return RET_BADPW;
		}
	}

#ifndef ALLOW_PASSLESS_RECOVER
	if (numargs < 3) {
#else
	if ((numargs < 3) && !isFullyRecognized(nick, tmp))
#endif
		sSend(":%s NOTICE %s :Permission denied: you will need to specify a password.", NickServ, nick->nick);
		PutHelpInfo(NickServ, nick, "HELP RECOVER");
		return RET_NOPERM;
	}
	

	if (isGhost(args[1])) {
		sSend(":%s NOTICE %s :%s is currently held by a %s enforcer.", NickServ,
			  nick->nick, args[1], NickServ);
		PutHelpInfo(NickServ, nick, "HELP RELEASE");
		return RET_EFAULT;
	}


	if (getGhost(args[1])) {
		return RET_OK;
	}

	taruser = getNickData(args[1]);

	if (taruser == 0) {
		sSend(":%s NOTICE %s :%s is not currently in use.", NickServ,
			  nick->nick, args[1]);
		return RET_NOTARGET;
	}

	enforce_nickname (taruser->nick);
	sSend(":%s NOTICE %s :Recovering nickname, '%s'", NickServ,
		  nick->nick, args[1]);
	return RET_OK;
}


/**
 * \nscmd Release
 * \syntax Release \<nick> \<password>
 * \plr args[0..2]
 */
NCMD(ns_release)
{

	char *from = nick->nick;
	RegNickList *tmp;

	if (addFlood(nick, 1))
		return RET_KILLED;

	if (numargs < 2) {
		sSend
			(":%s NOTICE %s :You must specify a registered nick and its password after the RELEASE command",
			 NickServ, from);
		return RET_NOTARGET;
	}
	if (numargs < 3) {
		sSend
			(":%s NOTICE %s :You must specify the nick's password after the specifed nick",
			 NickServ, from);
		return RET_SYNTAX;
	}

	if (isGhost(args[1])) {
		tmp = getRegNickData(args[1]);
		/* I don't know if this is more readable -- but it helped me */

		if (tmp == NULL) {
			sSend(":%s NOTICE %s :The nick %s is not registered.",
				  NickServ, from, args[1]);
			return RET_NOTARGET;
		}

		if ((numargs >= 3) && !Valid_pw(args[2], tmp->password, NickGetEnc(tmp))) {
			PutReply(NickServ, nick, ERR_BADPW, 0, 0, 0);
			if (BadPwNick(nick, tmp))
				return RET_KILLED;

			return RET_BADPW;
		}
		else if ((numargs < 3)
			 && !checkAccess(nick->user, nick->host, tmp)) {
			PutReply(NickServ, nick, ERR_NOACCESS, 0, 0, 0);
			return RET_NOPERM;
		}
		else
			GoodPwNick(nick, tmp);

		if ((tmp != NULL)) {
			if (isGhost(args[1])) {
				delGhost(args[1]);
			}
			sSend(":%s NOTICE %s :The nick %s has been released",
				  NickServ, from, args[1]);
		}
		return RET_OK;
	}

	sSend(":%s NOTICE %s :Services are not currently holding that nick.",
		  NickServ, from);
	return RET_NOTARGET;
}


/************************************************************************
* Operator flag system                                                  *
* Nsetopflagscmd()   Change Operator Flags                      -Mysid  *
************************************************************************/

/**
 * \nscmd Setopflags (/NS Setop)
 * \syntax Setop  \<nick>
 * \plr Show access flags of \e nick.
 * \syntax Setop  \<nick> {(+|-) \e Flags ...}
 * \plr \e Flags = [AOokKpsSNCIGrjL!cahD]
 * \plr Change access flags of \e nick.
 * \syntax Setop del \<nick>
 * \plr Strip \e nick of access flags.
 * \syntax Setop add \<nick>
 * \plr Give \e nick basic oper flags (setop +o does this too unless
 *      overriden with setop \<nick> +o-pka).
 */
/// Query/Change Operator flags
NCMD(ns_setopflags)
{
	char *from = nick->nick;
	char modebuf[IRCBUF] = "";
	char totbuf[IRCBUF] = "";
	RegNickList *regnick;
	int mode = 1;
	flag_t setflags, thisflag;
	u_int i, j, x;

	if (isOper(nick) && (numargs < 2)) {
		sSend(":%s NOTICE %s :\2Your flag status\2:", OperServ, from);
		for (i = 0; opflags[i].symbol; i++) {
			if (opflags[i].symbol == 'A' || opflags[i].symbol == '*')
				continue;
			j = opFlagged(nick, opflags[i].flag);
			sSend(":%s NOTICE %s :%c (%-15s)   : %s", OperServ, from,
				  opflags[i].symbol, opflags[i].name, j ? "YES" : "NO ");
		}
		if (addFlood(nick, 15))
			return RET_KILLED;
		return RET_OK;
	}

	if (addFlood(nick, 1))
		return RET_KILLED;
	if (!opFlagged(nick, OOPER)) {
		sSend
			(":%s NOTICE %s :You must be an IRC Operator to use this command.",
			 OperServ, from);
		return RET_NOPERM;
	}

	if (numargs < 2) {
		sSend(":%s NOTICE %s :You must specify a nick.", OperServ, from);
		return RET_SYNTAX;
	} else if (!strcasecmp(args[1], "list")) {
		OperList *oper;

		for (oper = firstOper; oper; oper = oper->next)
			if (oper->who) {
				for (x = 0, j = 0; opflags[j].symbol; j++)
					if ((oper->who->opflags & opflags[j].flag)) {
						if (opflags[j].symbol == '*')
							continue;
						if (!x)
							modebuf[x++] = '+';
						modebuf[x++] = opflags[j].symbol;
					}
				modebuf[x++] = '\0';
				if (!(oper->who->opflags & OROOT))
					sSend(":%s NOTICE %s :%-17s  (\37%-20s\37)", OperServ,
						  from, oper->who->nick, modebuf);
				else
					sSend
						(":%s NOTICE %s :%-17s  (\2Root                \2)",
						 OperServ, from, oper->who->nick);
			}
		return RET_OK;
	} else if (!strcasecmp(args[1], "add")) {
		if (opFlagged(nick, OSETOP | OOPER) == 0) {
			PutReply(OperServ, nick, ERR_NOACCESS, 0, 0, 0);
			return RET_NOPERM;
		}
		if (numargs < 3) {
			sSend(":%s NOTICE %s :Add opflags to what nick?", OperServ,
				  from);
			return RET_NOPERM;
		}
		if (!(regnick = getRegNickData(args[2]))) {
			sSend(":%s NOTICE %s :The nick %s is not registered.",
				  OperServ, from, args[2]);
			return RET_EFAULT;
		}
		if (regnick->opflags) {
			sSend(":%s NOTICE %s :%s already has opflags.", NickServ, from,
				  args[2]);
			return RET_FAIL;
		}
		regnick->opflags |= OPFLAG_DEFAULT;
		addOpData(regnick);
		return RET_OK_DB;
	} else if (!strcasecmp(args[1], "del")) {
		if (opFlagged(nick, OSETOP | OOPER) == 0) {
			PutReply(OperServ, nick, ERR_NOACCESS, 0, 0, 0);
			return RET_NOPERM;
		}
		if (numargs < 3) {
			sSend(":%s NOTICE %s :Del opflags from what nick?", OperServ,
				  from);
			return RET_SYNTAX;
		}
		if (!(regnick = getRegNickData(args[2]))) {
			sSend(":%s NOTICE %s :The nick %s is not registered.",
				  OperServ, from, args[2]);
			return RET_EFAULT;
		}
		if (!regnick->opflags) {
			sSend(":%s NOTICE %s :%s has no opflags.", OperServ, from,
				  args[2]);
			return RET_EFAULT;
		}
		if ((regnick->opflags & (OROOT | OREMROOT))
			|| ((regnick->opflags & (OPFLAG_ROOTSET))
				&& !opFlagged(nick, OOPER | OROOT))) {
			sSend
				(":%s NOTICE %s :%s holds one or more flags that you cannot remove.",
				 NickServ, from, args[2]);
			return RET_EFAULT;
		}
		regnick->opflags = 0;
		delOpData(regnick);
		return RET_OK_DB;
	}

	if (!(regnick = getRegNickData(args[1]))) {
		sSend(":%s NOTICE %s :%s is not registered", OperServ, from,
			  args[1]);
		return RET_EFAULT;
	}

	if (numargs < 3) {
		for (x = 0, j = 0; opflags[j].symbol; j++)
			if ((regnick->opflags & opflags[j].flag)) {
				if (opflags[j].symbol == '*')
					continue;
				if (!x)
					modebuf[x++] = '+';
				modebuf[x++] = opflags[j].symbol;
			}
		modebuf[x++] = '\0';
		sSend(":%s NOTICE %s :Current opflags of user %s are %s.",
			  OperServ, from, args[1], modebuf);
		return RET_OK;
	}

	if (!opFlagged(nick, OSETOP | OOPER)) {
		PutReply(OperServ, nick, ERR_NOACCESS, 0, 0, 0);
		return RET_NOPERM;
	}

	if (!opFlagged(nick, OOPER | OROOT)
		&& (regnick->opflags & (OROOT | OPROT))) {
		sSend(":%s NOTICE %s :You cannot alter the opflags of %s.",
			  OperServ, from, args[1]);
		return RET_NOPERM;
	}

	setflags = regnick->opflags;

	for (i = 0; args[2] && args[2][i]; i++) {
		switch (args[2][i]) {
		case '+':
			mode = 1;
			break;
		case '-':
			mode = 0;
			break;
		default:
			for (j = 0; opflags[j].symbol; j++)
				if (opflags[j].symbol == args[2][i]) {
					thisflag = opflags[j].flag;

					if (mode) {
						if (thisflag & OADMIN)
							thisflag |= OPFLAG_ADMIN;
						if (thisflag & OOPER)
							thisflag |= OPFLAG_DEFAULT;
						thisflag &= ~OADMIN;
						if (mode && (thisflag & OSERVOP))
							sSend
								(":%s NOTICE %s :Note: the servop opflag is deprecate.",
								 OperServ, from);
					}

					/* flag permissions check */
					if (!isRoot(nick)
						&& (OPFLAG_ROOTSET & opflags[j].flag)) {
						sSend
							(":%s NOTICE %s :Setting the flag '%c' requires root access.",
							 OperServ, from,
							 opflags[j].symbol ? opflags[j].symbol : ' ');
						continue;
					}
					if (opflags[j].flag & (OROOT | OREMROOT)) {
						sSend
							(":%s NOTICE %s :SETOP cannot change this flag.",
							 OperServ, from);
						continue;
					}
					if (mode)
						regnick->opflags |= thisflag;
					else
						regnick->opflags &= ~thisflag;
				}
			break;
		}
	}
	for (x = 0, j = 0; opflags[j].symbol; j++)
		if (!(setflags & opflags[j].flag) &&
			(regnick->opflags & opflags[j].flag)) {
			if (opflags[j].symbol == '*')
				continue;
			if (!x)
				modebuf[x++] = '+';
			modebuf[x++] = opflags[j].symbol;
		}
	for (mode = 0, j = 0; opflags[j].symbol; j++)
		if ((setflags & opflags[j].flag) &&
			!(regnick->opflags & opflags[j].flag)) {
			if (opflags[j].symbol == '*')
				continue;
			if (!mode) {
				modebuf[x++] = '-';
				mode++;
			}
			modebuf[x++] = opflags[j].symbol;
		}
	modebuf[x++] = '\0';
	for (x = 0, j = 0; opflags[j].symbol; j++)
		if ((regnick->opflags & opflags[j].flag)) {
			if (opflags[j].symbol == '*')
				continue;
			if (!x)
				totbuf[x++] = '+';
			totbuf[x++] = opflags[j].symbol;
		}
	totbuf[x++] = '\0';
	if (*modebuf && nick->reg && nick->reg != regnick)
		sSend(":%s GLOBOPS :%s set opflags %s on user %s.", OperServ, from,
			  modebuf, regnick->nick);
	operlog->log(nick, NS_SETOP, regnick->nick, 0, "%s (%s)", modebuf,
				 totbuf);
	sSend(":%s NOTICE %s :Opflags for user \2%s\2 are now \2%s\2.",
		  OperServ, from, regnick->nick, totbuf);
	if (regnick->opflags && !setflags)
		addOpData(regnick);
	else if (setflags && !regnick->opflags)
		delOpData(regnick);
	return RET_OK_DB;
}


/**************************************************************************/

/**
 * \nscmd Mark
 * \syntax Mark \<nick>
 * \plr   Check state of mark flag
 * \syntax Mark \<nick> ("on" | "off")
 * \plr   On means mark, off means unmark.
 * \plr   A nick mark is an indication that an oper should be particularly
 *        wary in handling a password request for the nickname.
 * \plr   Getrealpass, and getpass should both warn about this, but only
 *        GRp does.
 * \plr   -Mysid
 */
NCMD(ns_mark)
{
	char *from = nick->nick;
	RegNickList *target;

	if (isServop(nick) == 0) {
		PutReply(NickServ, nick, ERR_NOACCESS, 0, 0, 0);
		return RET_NOPERM;
	}

	if (numargs < 2) {
		sSend(":%s NOTICE %s :Must supply at least one argument",
			  NickServ, from);
		return RET_SYNTAX;
	}

	target = getRegNickData(args[1]);
	if (target == NULL) {
		sSend(":%s NOTICE %s :Nick not found", NickServ, from);
		return RET_EFAULT;
	}
	if (numargs < 3) {
		if (!target->markby)
			sSend(":%s NOTICE %s :%s is \2%s\2.", NickServ, from, args[1],
				  target->flags & NMARK ? "Marked" : "Not Marked");
		else if (target->flags & NMARK)
			sSend(":%s NOTICE %s :%s was \2%s\2 by %s.", NickServ, from,
				  args[1], target->flags & NMARK ? "Marked" : "Not Marked",
				  target->markby);
		return RET_OK;
	}
	if (!strcasecmp(args[2], "on") || !strcasecmp(args[2], "yes")) {
		if (target->flags & NMARK) {
			sSend(":%s NOTICE %s :%s is already marked%s%s.", NickServ,
				  from, args[1], target->markby ? " by " : "",
				  target->markby ? target->markby : "");
			return RET_OK;
		}
		sSend(":%s NOTICE %s :%s is now \2Marked\2.", NickServ, from,
			  args[1]);
		target->flags |= NMARK;
		if (target->markby)
			FREE(target->markby);
		target->markby = strdup(nick->nick);
		nicklog->log(nick, NS_MARK, target->nick, LOGF_ON);
		return RET_OK_DB;
	} else if (!strcasecmp(args[2], "off") || !strcasecmp(args[2], "no")) {
		sSend(":%s NOTICE %s :%s is now \2Unmarked\2.", NickServ, from,
			  args[1]);
		target->flags &= ~NMARK;
		if (target->markby)
			FREE(target->markby);
		target->markby = NULL;
		nicklog->log(nick, NS_MARK, target->nick, LOGF_OFF);
		return RET_OK_DB;
	} else {
		sSend(":%s NOTICE %s :Unrecognized usage.", NickServ, from);
		sSend(":%s NOTICE %s :MARK <nick> <on|off>", NickServ, from);
		return RET_OK;
	}
	return RET_FAIL;
}


/**************************************************************************/

/**
 * \nscmd Setflag
 * \syntax Setflag \<nick> [+-]\<flags>
 * \plr Change a NickName's settings or flags
 */
NCMD(ns_setflags)
{
	char *from = nick->nick;
	RegNickList *regnick;
	int mode = 1;
	u_int i;

	if (addFlood(nick, 1))
		return RET_KILLED;

	if (opFlagged(nick, OOPER | OSETFLAG) == 0 && isRoot(nick) == 0) {
		PutReply(NickServ, nick, ERR_NOACCESS, 0, 0, 0);
		return RET_NOPERM;
	}

	else if (numargs < 3) {
		sSend(":%s NOTICE %s :You must specify at least 2"
			  " parameters after setflags", NickServ, from);
		return RET_SYNTAX;
	}

	regnick = getRegNickData(args[1]);
	if (regnick == NULL) {
		sSend(":%s NOTICE %s :%s is not registered",
			  NickServ, from, args[1]);
		return RET_EFAULT;
	}
	if (isRoot(nick) == 0 && (regnick->opflags & OROOT)) {
		sSend
			(":%s NOTICE %s :Permission denied -- in altering flags of %s.",
			 NickServ, from, args[1]);
		return RET_NOPERM;
	}

	sSend(":%s NOTICE %s :Flags for %s are now %s",
		  NickServ, from, args[1], args[2]);
	sSend(":%s GLOBOPS :%s set mode %s on %s",
		  myname, from, args[2], args[1]);
	operlog->log(nick, NS_SETFLAG, args[1], 0, "%s", args[2]);

	for (i = 0; i < strlen(args[2]); i++) {
		switch (args[2][i]) {
		case '+':
			mode = 1;
			break;
		case '-':
			mode = 0;
			break;
		case 'k':
			if (mode)
				regnick->flags |= NKILL;
			else
				regnick->flags &= ~NKILL;
			break;
		case 'v':
			if (mode)
				regnick->flags |= NVACATION;
			else
				regnick->flags &= ~NVACATION;
			break;
		case 'f':
			if (mode && !(regnick->opflags & OROOT))
				regnick->flags |= NFORCEXFER;
			else
				regnick->flags &= ~NFORCEXFER;
			break;
		case 'e':
			if (mode)
				regnick->flags |= NEMAIL;
			else
				regnick->flags &= ~NEMAIL;
		case 'h':
			if (isRoot(nick) == 0)
				sSend(":%s NOTICE %s :You cannot set the 'h' flag",
					  NickServ, from);
			else if (mode)
				regnick->flags |= NHOLD;
			else
				regnick->flags &= ~NHOLD;
			break;
		case 'b':
			if (mode)
				regnick->flags |= NBANISH;
			else
				regnick->flags &= ~NBANISH;
			break;
		case 'g':
			if (isRoot(nick) == 0)
				sSend(":%s NOTICE %s :You cannot set the 'g' flag",
					  NickServ, from);
			else if (mode)
				regnick->opflags |= OGRP;
			else
				regnick->opflags &= ~OGRP;
			break;
		case 's':
			if (isRoot(nick) == 0)
				sSend(":%s NOTICE %s :You cannot set the 's' flag",
					  NickServ, from);
			else if (mode)
				regnick->opflags |= OSERVOP;
			else
				regnick->opflags &= ~OSERVOP;
			break;
		case 'B':
			if (opFlagged(nick, OAKILL) == 0)
				sSend(":%s NOTICE %s :You cannot set the 'o' flag",
					  NickServ, from);
			else if (mode)
				regnick->flags |= NBYPASS;
			else
				regnick->flags &= ~NBYPASS;
			break;
		default:
			sSend(":%s NOTICE %s :Unknown mode flag: %c", NickServ, from,
				  args[2][i]);
			break;
		}
	}
	return RET_OK_DB;
}

/**
 * \nscmd Banish
 * \syntax Banish \<nick>
 * \plr Banish a NickName from use and registration by users
 */
NCMD(ns_banish)
{

	char *from = nick->nick;
	char buf[IRCBUF];
	RegNickList *bannick;

	if (addFlood(nick, 1))
		return RET_KILLED;

	if (opFlagged(nick, OOPER | ONBANDEL) == 0) {
		PutReply(NickServ, nick, ERR_NOACCESS, 0, 0, 0);
		return RET_NOPERM;
	}

	if (numargs < 2) {
		sSend
			(":%s NOTICE %s :You must specify an argument after the banish command",
			 NickServ, from);
		return RET_SYNTAX;
	}

	bannick = getRegNickData(args[1]);

	if (bannick == NULL) {
		bannick = (RegNickList *) oalloc(sizeof(RegNickList));
		strncpyzt(bannick->nick, args[1], NICKLEN);
		bannick->regnum.SetNext(top_regnick_idnum);

		addRegNick(bannick);
	} else if (isRoot(nick) == 0 && (bannick->opflags & OROOT)) {
		sSend(":%s NOTICE %s :Permission denied.", NickServ, from);
		return RET_NOPERM;
	}

	strncpyzt(bannick->nick, args[1], NICKLEN);
/* #warning XXX This assumes details of RegNickList */
	strcpy(bannick->user, "banished");

	snprintf(buf, HOSTLEN, "nicks.%s", NETWORK);
	SetDynBuffer(&bannick->host, buf);

	snprintf(bannick->email, EMAILLEN, "nickserv@%s", NETWORK);

	snprintf(buf, URLLEN, "http://www.%s", NETWORK);	/* sprintf is safe */
	SetDynBuffer(&bannick->url, buf);

	memset(bannick->password, 0, PASSLEN);
	bannick->amasks = 0;
	bannick->timereg = 0;
	bannick->timestamp = 0;
	bannick->flags = 0;
	bannick->flags |= NBANISH|NENCRYPT;

	/*
	 * destroy all access list items
	 */
	while (LIST_FIRST(&bannick->acl))
		LIST_REMOVE(LIST_FIRST(&bannick->acl), al_lst);

	sSend(":%s GNOTICE :%s Banished nick %s", myname, from, args[1]);
	operlog->log(nick, NS_BANISH, args[1]);
	return RET_OK_DB;
}

#ifdef REQ_EMAIL
NCMD(ns_getkey)
{
	char *from = nick->nick;
	RegNickList *regnick;

	if(addFlood(nick,5))
		return;

	if (numargs<2) {
		sSend(":%s NOTICE %s :You must specify a nick to getkey", NickServ, from);
		return;
	}

	if (!isRoot(nick)) {
		sSend(":%s NOTICE %s :Access denied", NickServ, from);
		return;
	}

	regnick=getRegNickData(args[1]);

	if(!regnick || !(regnick->flags & NREG))
		sSend(":%s NOTICE %s :Nick %s is not registered", NickServ, from, args[1]);
        else {
		sSend(":%s NOTICE %s :The activation code for %s is %lu", NickServ, from, regnick->nick, regnick->email_key);
		sSend(":%s GNOTICE :%s used getkey on %s", myname, from, args[1]);
	}
	return;
}
#endif

/**
 * Debugging command, get password field bits from user structure
 * \deprecated Use of this command is deprecated
 */
NCMD(ns_getrealpass)
{
	char *from = nick->nick;
	RegNickList *targetNick;

	if (addFlood(nick, 5))
		return RET_KILLED;

	if (numargs < 2) {
		sSend(":%s NOTICE %s :You must specify a nick to getrealpass",
			  NickServ, from);
		return RET_SYNTAX;
	}
#ifdef ENABLE_GRPOPS
	if (isRoot(nick) == 0 && isGRPop(nick) == 0)
#else
	if (isRoot(nick) == 0)
#endif
	{
		PutReply(NickServ, nick, ERR_NOACCESS, 0, 0, 0);
		return RET_NOPERM;
	}

	sSend(":%s NOTICE %s :Warning -- this command is deprecated.",
		  NickServ, from);

	targetNick = getRegNickData(args[1]);

	if (targetNick == NULL)
		sSend(":%s NOTICE %s :Nick %s is not registered", NickServ, from,
			  args[1]);
	else if (targetNick->flags & NMARK) {
		if (targetNick->markby)
			sSend
				(":%s NOTICE %s :\2Important Note:\2 %s has been marked by %s -- you need to contact the setter before retrieving the password for the nick in question.",
				 NickServ, from, targetNick->nick, targetNick->markby);
		else
			sSend(":%s NOTICE %s :\2Important Note:\2 %s has been marked.",
				  NickServ, from, targetNick->nick);
		return RET_OK;
	}
		else if (!opFlagged(nick, OROOT | OVERRIDE)
				 && (targetNick->flags & NOSENDPASS)) {
		sSend
			(":%s NOTICE %s :Nick %s has enabled the \2NOPWSEND\2 option.",
			 NickServ, from, args[1]);
		return RET_EFAULT;
	}
		else if ((!isRoot(nick) && (targetNick->opflags & OROOT))
				 || (!isServop(nick) && (targetNick->opflags & OSERVOP))) {
		sSend
			(":%s NOTICE %s :Permission denied -- a GRPop may not GRP a user with root or servop privileges.",
			 NickServ, from);
		sSend(":%s GLOBOPS :%s!%s@%s tries to getrealpass \2%s\2.",
			  NickServ, from, nick->user, nick->host, args[1]);
		operlog->log(nick, NS_GETREALPASS, args[1], RET_FAIL);
		return RET_NOPERM;
	} else {
		if (!(targetNick->flags & NENCRYPT))
			sSend(":%s NOTICE %s :The password for %s is %s", NickServ, from,
				  targetNick->nick, targetNick->password);
		else {
			u_char *p;

			sSend(":%s NOTICE %s :The password for %s is encrypted: %s", NickServ, from,
				  targetNick->nick, md5_printable(targetNick->password));
			p = toBase64(targetNick->password, 16);
			if (p != NULL) {
				sSend(":%s NOTICE %s :In Base64 format that is: $%s", NickServ, from,
					  p);
				FREE(p);
			}
		}
		sSend(":%s GLOBOPS :%s used getrealpass on %s", myname, from,
			  args[1]);
		operlog->log(nick, NS_GETREALPASS, args[1], 0);
	}
	return RET_OK;
}

/**
 * \nscmd Getpass
 * \syntax Getpass \<nick> { options }
 * \plr Send a password change authorization key to the user's e-mail
 *      address.
 * -Mysid
 */
NCMD(ns_getpass)
{
	char buf[MAXBUF];
	char *from = nick->nick;
	RegNickList *targetNick;
	EmailMessage *email;
	const char *pwAuthKey;
	time_t doot = time(NULL);
	int key_new, key_resend, key_transfer, i, nick_arg = 1;

	key_new = key_resend = key_transfer = 0;
	srand48(time(NULL));

	if (addFlood(nick, 5))
		return RET_KILLED;

	if ((isOper(nick) == 0) && (isHelpop(nick) == 0)) {
		PutReply(NickServ, nick, ERR_NOACCESS, 0, 0, 0);
		return RET_NOPERM;
	}

	if (numargs < 2) {
		sSend(":%s NOTICE %s :You must specify a nick to getpass",
			  NickServ, from);
		return RET_SYNTAX;
	}

	if (numargs >= 3) {
		for(i = 2; i < numargs; i++)
		{
			if (*args[i] == '-')
			{
				if (!strcmp(args[i], "-new") && isOper(nick))
					key_new = 1;
				else if (!strcmp(args[i], "-resend"))
					key_resend = 1;
				else if (opFlagged(nick, OGRP) && !strcmp(args[i], "-transfer"))
					key_transfer = 1;
				else if (args[i][1] == '-') {
					break;
				}
				else
					continue;
			}
			else {
				break;
			}
		}
	}

	targetNick = getRegNickData(args[nick_arg]);

	if (targetNick == NULL) {
		sSend(":%s NOTICE %s :Nick %s is not registered", NickServ, from,
			  args[nick_arg]);
		return RET_EFAULT;
	}
	if (!key_transfer && (strncmp(targetNick->email, "(none)", 6) == 0
		|| targetNick->email[0] == 0)) {
		sSend(":%s NOTICE %s :Nick %s has not specified an e-mail address",
			  NickServ, from, args[nick_arg]);
		return RET_EFAULT;
	}
	else if (!opFlagged(nick, OVERRIDE) && !key_transfer
		 && (targetNick->flags & NOSENDPASS)) {
		sSend
		(":%s NOTICE %s :Warning: nick %s has enabled the \2NOPWSEND\2 option.",
		 NickServ, from, args[nick_arg]);
		return RET_EFAULT;
	} else if (targetNick->chpw_key && !key_new && !key_resend) {
		sSend(":%s NOTICE %s :That nickname has already had a change "
		      "key sent out.  Please specify -new or -resend.",
			  NickServ, from);
	} else if ((key_transfer || (targetNick->flags & NFORCEXFER)) && (!(targetNick->opflags & OROOT))) {
		if (!targetNick->chpw_key || !(targetNick->flags & NFORCEXFER))
		{
			targetNick->chpw_key = (u_int32_t)(lrand48());
			if (!targetNick->chpw_key)
				targetNick->chpw_key = 1;
		}

		pwAuthKey = GetAuthChKey("-forced-transfer-",
		                         PrintPass(targetNick->password, NickGetEnc(targetNick)),
		                         targetNick->timereg,
		                         targetNick->chpw_key);

		if (key_transfer)
			sSend(":%s GLOBOPS :%s used sendpass (transfer) on %s",
			      ChanServ, from, args[nick_arg]);
		else
			sSend(":%s GLOBOPS :%s used sendpass (getkey) on %s",
			      ChanServ, from, args[nick_arg]);
		sSend(":%s NOTICE %s :Transfer key for %s is %s",
		      ChanServ, from, args[nick_arg], pwAuthKey);
		targetNick->flags |= NFORCEXFER;
		operlog->log(nick, NSS_GETPASS_XFER, args[nick_arg]);
	} else if ((targetNick->flags & NFORCEXFER) && targetNick->chpw_key && !opFlagged(nick, OGRP | OOPER | OVERRIDE)) {
		sSend(":%s NOTICE %s :This nick's pw key is marked for ``nick transfer''",
		      NickServ, from);
	} else {
		email = new EmailMessage;

		if (!email) {
			sSend(":%s WALLOPS :Fatal error: ns_sendpass: out of memory",
				  NickServ);
			sSend(":%s NOTICE %s :Fatal error: ns_sendpass: out of memory",
				  NickServ, from);
			abort();
			return RET_EFAULT;
		}

		if (key_new || !key_resend || !targetNick->chpw_key) {
			targetNick->chpw_key = (u_int32_t)(lrand48());
			if (!targetNick->chpw_key)
				targetNick->chpw_key = 1;
		}

		pwAuthKey = GetAuthChKey(targetNick->email,
		                         PrintPass(targetNick->password, NickGetEnc(targetNick)),
		                         targetNick->timereg,
		                         targetNick->chpw_key);

		sSend(":%s GNOTICE :%s used sendpass%s on %s", myname, from,
			  key_resend ? " (resend)" : "", args[nick_arg]);
		if (isOper(nick) == 0)
			sSend
				(":%s NOTICE %s :The \2password change key\2 has been sent to the email address: (hidden)",
				 NickServ, from);
		else
			sSend
				(":%s NOTICE %s :The \2password change key\2 has been sent to the email address: %s",
				 NickServ, from, targetNick->email);
		sprintf(buf, "%s Password Recovery <%s@%s>", NickServ, NickServ,
				NETWORK);
		email->from = buf;
		email->to = targetNick->email;
		email->subject = "Password recovery";

		sprintf(buf, "\nThe password change key for %s is %s\n", targetNick->nick,
				pwAuthKey);
		email->body = buf;

		email->body += "This key can be used to set a new password "
		               "for your nickname.\n";
		email->body += "\n";
		sprintf(buf, "On IRC, use: /NickServ SETPASS %s %s <password>\n",
		        targetNick->nick, pwAuthKey);
		email->body += buf;
		email->body += "Or navigate to: "
		"http://astral.sorcery.net/~sortest/setpass.php?u_nick=";
		email->body += urlEncode(targetNick->nick);
		email->body += "&u_ck=";
		email->body += urlEncode(pwAuthKey);
		email->body += "\nTo choose your new password.\n\n";

		sprintf(buf, "This key was recovered by %s at %s\n",
				nick->nick, ctime(&(doot)));
		email->body += buf;

		email->send();
		email->reset();
		delete email;

		operlog->log(nick, NS_GETPASS, args[nick_arg]);
	}
	return RET_OK_DB;
}

/**
 * \nscmd Delete
 * \plr Delete a nickname record
 */
NCMD(ns_delete)
{
	char *from = nick->nick;
	RegNickList *targetNick;

	if (opFlagged(nick, OOPER | ONBANDEL) == 0) {
		PutReply(NickServ, nick, ERR_NOACCESS, 0, 0, 0);
		return RET_NOPERM;
	}

	if (numargs < 2) {
		sSend(":%s NOTICE %s :You must specify a nick", NickServ, from);
		return RET_SYNTAX;
	}

	targetNick = getRegNickData(args[1]);
	if (targetNick == NULL) {
		sSend(":%s NOTICE %s :Nickname %s is not registered",
			  NickServ, from, args[1]);
		return RET_EFAULT;
	}

	if (isRoot(nick) == 0 && (targetNick->opflags & OROOT)) {
		sSend(":%s NOTICE %s :Permission denied.", NickServ, from);
		return RET_NOPERM;
	}

	delRegNick(targetNick);
	sSend(":%s NOTICE %s :This nick has been deleted by %s."
		  " To enable you to use this nick, you will need to "
		  "change nicknames and then change back",
		  NickServ, args[1], from);

	sSend(":%s NOTICE %s :Deleted nick %s", NickServ, from, args[1]);
	sSend(":%s GLOBOPS :%s deleted nick %s", myname, from, args[1]);

	return RET_OK_DB;
}

/// Save NickServ database
/* ARGSUSED1 */
NCMD(ns_save)
{
	char *from = nick->nick;

	if (isRoot(nick) == 0) {
		PutReply(NickServ, nick, ERR_NOACCESS, 0, 0, 0);
		return RET_NOPERM;
	}
#ifdef GLOBOP_ON_SAVE
	sSend(":%s GLOBOPS :Saving database. (%s)", NickServ, from);
#else
	sSend(":%s PRIVMSG " LOGCHAN " :Saving database. (%s)", NickServ,
		  from);
#endif
	saveNickData();
	return RET_OK_DB;
}

/**
 * \nscmd List
 * \plr List matching nicknames \plr -Mysid
 */
NCMD(ns_list)
{
	char *from = nick->nick;
	RegNickList *reg;
	MaskData *umask;
	int i = 0, x = 0, bucket = 0;

	if (opFlagged(nick, OLIST) == 0) {
		PutReply(NickServ, nick, ERR_NOACCESS, 0, 0, 0);
		return RET_NOPERM;
	}

	if (numargs != 2) {
		sSend(":%s NOTICE %s :You must specify ONE address to search by",
			  NickServ, from);
		return RET_SYNTAX;
	}

	umask = make_mask();

	if (split_userhost(args[1], umask) < 0) {
		free_mask(umask);

		sSend(":%s NOTICE %s :Unable to read mask.", NickServ, from);
		return RET_SYNTAX;
	}

	if (!strcmp(umask->nick, "*") && !strcmp(umask->user, "*")
		&& !strcmp(umask->host, "*")) {
		sSend(":%s NOTICE %s :Invalid mask *!*@*.", NickServ, from);
		free_mask(umask);
		return RET_EFAULT;
	}

	for (bucket = 0; bucket < NICKHASHSIZE; bucket++) {
		reg = LIST_FIRST(&RegNickHash[bucket]);

		while (reg != NULL) {
			if (!match(umask->nick, reg->nick)
				&& !match(umask->user, reg->user)
				&& !match(umask->host, reg->host)) {
				i++;
				if (i <= 200)
					sSend(":%s NOTICE %s :%4i: %s!%s@%s",
						  NickServ, from, i, reg->nick,
						  reg->user, reg->host);
			}

			reg = LIST_NEXT(reg, rn_lst);
			x++;
		}
	}
	if (i > 200)
		sSend(":%s NOTICE %s :[Results truncated to 200 items]",
			  NickServ, from);
	sSend(":%s NOTICE %s :%i/%i matched conditions (%s!%s@%s)",
		  NickServ, from, i, x, umask->nick, umask->user, umask->host);
	sSend(":%s GLOBOPS :%s is listing %s!%s@%s (%d/%d matches)",
		  NickServ, from, umask->nick, umask->user, umask->host, i, x);
	free_mask(umask);

	return RET_OK;
}

/**
 * \nscmd Bypass
 * \plr Control ahurt bypass flags
 * \plr -Mysid
 */
NCMD(ns_bypass)
{
	RegNickList *targetnick;
	char *from;
	HashKeyVal hashEnt;

	if (!nick || !(from = nick->nick))
		return RET_FAIL;

	if (!opFlagged(nick, OAKILL) && !opFlagged(nick, OAHURT)) {
		sSend
			(":%s NOTICE %s :You do not have the necessary flags to use this"
			 " command", NickServ, from);
		return RET_NOPERM;
	}

	if (numargs < 2) {
		sSend(":%s NOTICE %s :Syntax: /msg %s BYPASS <nick> [<on|off>]",
			  NickServ, from, NickServ);
		return RET_SYNTAX;
	}

	if (isServop(nick) && !strcasecmp("allon", args[1])) {
		char mask[IRCBUF + 2] = "";

		if ((numargs <= 2) && !opFlagged(nick, OROOT | OVERRIDE) ) {
			sSend(":%s NOTICE %s :Syntax error, mask required.",
				OperServ, from);
			sSend(":%s NOTICE %s :Must use /os override "
			      "to set bypass for all nicks in db.",
			      OperServ, from);
			return RET_NOPERM;
		}

		if (numargs > 2)
		{
			if (!match(args[2], "...!..@...com")
			    || !match(args[2], "...!..@...net")
			    || !match(args[2], "...!..@...edu")
			    || !match(args[2], "...!..@...org")
			    || !match(args[2], "...!..@...my")
			    || !match(args[2], "...!..@...uk")) 
			{
		            sSend(":%s NOTICE %s :That %s is too broad.", OperServ,
					from, "bypass mask");
			    sSend(":%s GLOBOPS :%s attempts to set %s for %s", OperServ,
				        from, "bypass_allon", args[2]);
			    return RET_NOPERM;
			}
		}


		nicklog->log(nick, NS_BYPASS, args[2], LOGF_ON | LOGF_PATTERN);

		for (hashEnt = 0; hashEnt < NICKHASHSIZE; hashEnt++) {
			for (targetnick = LIST_FIRST(&RegNickHash[hashEnt]);
				 targetnick; targetnick = LIST_NEXT(targetnick, rn_lst)) {
				if (numargs >= 3) {
					snprintf(mask, IRCBUF, "%s!%s@%s", targetnick->nick,
							 targetnick->user, targetnick->host);
					if ((numargs > 2) 
						&& match(args[2], mask))
						continue;
				}
				targetnick->flags |= NBYPASS;
				sSend(":%s NOTICE %s :%s", NickServ, from,
					  targetnick->nick);
			}
		}
		sSend(":%s NOTICE %s :End of list", NickServ, from);
		return RET_OK_DB;
	}

	if (isRoot(nick) && !strcasecmp("alloff", args[1])) {
		char mask[IRCBUF + 2] = "";

                if ((numargs <= 2) && !opFlagged(nick, OROOT | OVERRIDE) ) {
                        sSend(":%s NOTICE %s :Syntax error, mask required.",
                                OperServ, from);
                        sSend(":%s NOTICE %s :Must use /os override "
                              "to clear bypass for all nicks in db.",
                              OperServ, from);
                        return RET_NOPERM;
                }

                if (numargs > 2)
                {
                        if (!match(args[2], "...!..@...com")
                            || !match(args[2], "...!..@...net")
                            || !match(args[2], "...!..@...edu")
                            || !match(args[2], "...!..@...org")
                            || !match(args[2], "...!..@...my")
                            || !match(args[2], "...!..@...uk"))
                        {
                            sSend(":%s NOTICE %s :That %s is too broad.", OperServ,
                                        from, "bypass mask");
                            sSend(":%s GLOBOPS :%s attempts to set %s for %s", OperServ,
                                        from, "bypass_allon", args[2]);
                            return RET_NOPERM;
                        }
                }


		nicklog->log(nick, NS_BYPASS, args[2], LOGF_ON | LOGF_PATTERN);

		for (hashEnt = 0; hashEnt < NICKHASHSIZE; hashEnt++) {
			for (targetnick = LIST_FIRST(&RegNickHash[hashEnt]);
				 targetnick; targetnick = LIST_NEXT(targetnick, rn_lst)) {
				if (numargs >= 3) {
					snprintf(mask, IRCBUF, "%s!%s@%s", targetnick->nick,
							 targetnick->user, targetnick->host);
					if (match(args[2], mask))
						continue;
				}
				targetnick->flags &= ~NBYPASS;
				sSend(":%s NOTICE %s :%s", NickServ, from,
					  targetnick->nick);
			}
		}
		sSend(":%s NOTICE %s :End of list", NickServ, from);
		return RET_OK_DB;
	}


	if (!strcasecmp("list", args[1])) {
		char mask[IRCBUF + 2] = "";

		nicklog->log(nick, NS_BYPASS, args[2], LOGF_SCAN | LOGF_PATTERN,
					 "LIST");

		for (hashEnt = 0; hashEnt < NICKHASHSIZE; hashEnt++) {
			for (targetnick = LIST_FIRST(&RegNickHash[hashEnt]);
				 targetnick; targetnick = LIST_NEXT(targetnick, rn_lst)) {
				if (numargs >= 3) {
					snprintf(mask, IRCBUF, "%s!%s@%s", targetnick->nick,
							 targetnick->user, targetnick->host);
					if (match(args[2], mask))
						continue;
				}
				if (!(targetnick->flags & NBYPASS))
					continue;
				sSend(":%s NOTICE %s :%s", NickServ, from,
					  targetnick->nick);
			}
		}
		sSend(":%s NOTICE %s :End of list", NickServ, from);
		return RET_OK;
	}

	if (!(targetnick = getRegNickData(args[1]))) {
		sSend(":%s NOTICE %s :%s is not registered.",
			  NickServ, from, args[1]);
		return RET_EFAULT;
	}

	if (numargs < 3)
		sSend(":%s NOTICE %s :%s can%s bypass selective bans (autohurts).",
			  NickServ, from, args[1],
			  targetnick->flags & NBYPASS ? "" : "not");
	else {
		if (!strcasecmp(args[2], "on") || !strcasecmp(args[2], "yes")) {
			sSend
				(":%s NOTICE %s :%s is now a nick that can be used to bypass selective bans.",
				 NickServ, from, args[1]);
			targetnick->flags |= NBYPASS;
			return RET_OK_DB;
		}
			else if (!strcasecmp(args[2], "off")
					 || !strcasecmp(args[2], "no")) {
			sSend
				(":%s NOTICE %s :%s is no longer a nick that can be used to bypass selective bans.",
				 NickServ, from, args[1]);
			targetnick->flags &= ~NBYPASS;
			return RET_OK_DB;
		} else {
			sSend
				(":%s NOTICE %s :Turn %s's selective ban bypass ability on or off?",
				 NickServ, from, args[1]);
			return RET_OK;
		}
	}
	return RET_OK;
}

/**
 * \nscmd Logoff
 * \plr Unidentify from all nicknames
 * \plr -Mysid
 */
NCMD(ns_logoff)
{
	char *from = nick->nick;

	clearIdentify(nick);
	if (nick->caccess > 2)
		nick->caccess = 2;
	sSend(":%s NOTICE %s :You are no longer identified with %s.", NickServ,
		  from, NickServ);
	return RET_OK;
}

#ifdef REQ_EMAIL
/**
 * \todo reimplement e-mail verification but instead of using it as
 *       a part of registration/activation as it was before --
 *       use it as a means of verifying the e-mail for enabling
 *       more features... features like vacation, a e-mail memos
 *       features.  Maybe a system where a user who remembers their
 *       e-mail address can sendpass themself.
 */
NCMD(ns_activate)
{
	char *from = nick->nick;
	/* time_t doot; */

	if (addFlood(nick, 5))
		return;

	if(numargs < 2)
	{
		sSend(":%s NOTICE %s :A validation password is required with %s ACTIVATE", NickServ, from, NickServ);
		return;
	}

	if(!nick->reg || (nick->reg->flags & NBANISH)) {
		sSend(":%s NOTICE %s :Your nick is not registered", NickServ, from);
		return;
	}

	if(atoi(args[1]) != nick->reg->email_key) {
		sSend(":%s NOTICE %s :Incorrect activation key.", NickServ, from);
		return;
	}

	if(strlen(args[2]) > PASSLEN) {
		sSend(":%s NOTICE %s :The password you specified is too long to use, please make it 15 characters or less.", NickServ, from);
                return;
	}

	if ((nick->reg->flags & NACTIVE) &&
	    !(nick->reg->flags & NDEACC))
	{
		sSend(":%s NOTICE %s :This nickname is already active.",
			NickServ, from);
		return;
	}

	doot = time(NULL);

	if (!(nick->reg->flags & NACTIVE))
	{
		nick->reg->flags |= NACTIVE;
		sSend(":%s NOTICE %s :Your nick %s is now activated.", NickServ, from, from);
#ifdef
		sSend(":%s NOTICE %s :Your MemoServ memo box has been configured",
			  NickServ, from);
#endif
	}
	else
	{
		nick->reg->flags |= NACTIVE;
		nick->reg->flags &= ~NDEACC;
		sSend(":%s NOTICE %s :Your nick has been re-activated", NickServ, from);
	}
}
#endif


#ifdef REQ_EMAIL
/** 
 *  \brief Check for possible abusive activity based on e-mail address
 *  \param nick Online user setting specified e-mail address or registering
 *  \param email Target e-mail address
 *  \param newNick TRUE if this is to check if a new registration is ok.
 */
int emailAbuseCheck(UserList *nick, const char *email, int newNick)
{
	if (newNick && !addNReg(email)) {
		sSend(":%s NOTICE %s :You have too many nicks registered.", NickServ, from);
#ifdef REGLIMITYELL
		sSend(":%s GLOBOPS :%s is trying to register too many nicks.", NickServ, from);
#endif
		return 1;
	}
}

/**
 * @brief Add registered nick to emaildb
 */
int addNReg(char *email)
{
        nRegList *tmp;
        for(tmp = firstReg;tmp;tmp=tmp->next) {
                if(!strcasecmp(email, tmp->email)) {
                        tmp->regs++;
                        if(tmp->regs > NickLimit) {
                                tmp->regs--;
                                return 0;
                        }

                }
        }

        tmp = malloc(sizeof(nRegList));
        strcpy(tmp->email, email);
        tmp->regs = 1;

        if(!firstReg) {
                firstReg = tmp;
                tmp->previous = NULL;
        }
        else {
                lastReg->next = tmp;
                tmp->previous = lastReg;
        }

        lastReg = tmp;
        tmp->next = NULL;
        return 1;
}

void delNReg(char *email) {
        nRegList *tmp;

        for(tmp=firstReg;tmp;tmp=tmp->next) {
                if(!strcasecmp(email, tmp->email)) {
                        tmp->regs--;
                        if(!tmp->regs) {
                                if(tmp->previous)
                                  tmp->previous->next = tmp->next;
                                else
                                  firstReg = tmp->next;
                                if(tmp->next)
                                  tmp->next->previous = tmp->previous;
                                else
                                  lastReg = tmp->previous;
                                sfree(tmp);
                        }
                }
        }
}
#endif

/**
 * \nscmd Setpass
 * \plr This NickServ command is used to set a nick password
 *      if supplied the proper 'password change code'.
 * \plr -Mysid
 */
NCMD(ns_setpass)
{
	const char *from = nick->nick;
	const char *pwAuthChKey = NULL;
	RegNickList *rnl;

	if (numargs < 4)
	{
		sSend(":%s NOTICE %s :Syntax: SETPASS <nick> <code> <new password>",
			NickServ, from);
		return RET_SYNTAX;
	}

	if ((rnl = getRegNickData(args[1])) == NULL)
	{
		sSend(":%s NOTICE %s :That nickname is not registered.",
		      NickServ, from);
		return RET_NOTARGET;
	}

	if (rnl->chpw_key) {
		const char *rnE = rnl->email;
		const char *rnP = PrintPass(rnl->password, NickGetEnc(rnl));

		if (rnl->flags & NFORCEXFER)
			rnE = "-forced-transfer-";
		pwAuthChKey = GetAuthChKey(rnE, rnP,
		                         rnl->timereg, rnl->chpw_key);
	}

	if (rnl->chpw_key == 0 || strcmp(pwAuthChKey, args[2]))
	{
		sSend(":%s NOTICE %s :Access Denied.",
		      NickServ, from);
		return RET_FAIL;
	}

	if (strlen(args[3]) > PASSLEN)
	{
		sSend(":%s NOTICE %s :Specified password is too long, please "
		      "choose a password of %d characters or less.",
		      NickServ, from, PASSLEN);
		return RET_EFAULT;
	}


	if (strcasecmp(args[1], args[3]) == 0 || strcasecmp(args[3], "password") == 0
	    || strlen(args[3]) < 3)
	{
		sSend(":%s NOTICE %s :You cannot use \2%s\2 as your nickname password.",
		      NickServ, from, args[3]);
		return RET_EFAULT;
	}

	sSend(":%s NOTICE %s :Ok.", NickServ, from);

	pw_enter_password(args[3], rnl->password, NickGetEnc(rnl));
	rnl->chpw_key = 0;
	rnl->flags &= ~NFORCEXFER;

	sSend(":%s NOTICE %s :Password for %s is now %s, do NOT forget it, we"
	      " are not responsible for lost passwords.",
		NickServ, from, rnl->nick, args[3]);

	return RET_OK_DB;
}

/**
 * \nscmd Setemail
 * \plr Change e-mail of a user
 * \plr +G access required.
 * \plr -Mysid
 */
NCMD(ns_setemail)
{
	char *from = nick->nick;
	RegNickList *regnick;

	if (isOper(nick) == 0) {
		sSend(":%s NOTICE %s :Unknown command %s.\r\n"
			  ":%s NOTICE %s :Please try /msg %s HELP",
			  NickServ, from, args[0], NickServ, from, NickServ);
		return RET_NOPERM;
	}

	if (opFlagged(nick, OOPER | OGRP) == 0) {
		sSend(":%s NOTICE %s :Access denied.",
			NickServ, from);
		return RET_NOPERM;
	}

	if (numargs < 3)
	{
		sSend(":%s NOTICE %s :Syntax: SETEMAIL <nick> <email>",
			NickServ, from);
		return RET_SYNTAX;
	}

	regnick = getRegNickData(args[1]);

	if (!regnick) {
		PutError(NickServ, nick, ERR_NICKNOTREG_1ARG, args[1], 0, 0);
		PutHelpInfo(NickServ, nick, "HELP");

		return RET_NOTARGET;
	}

	if ((regnick->opflags & OROOT) && (opFlagged(nick, OROOT) == 0)) {
		sSend(":%s NOTICE %s :Access denied.",
			NickServ, from);
		return RET_NOPERM;
	}

	if (strchr(args[2], '@') == NULL || strchr(args[2], '.') == NULL ||
            strlen(args[2]) > EMAILLEN)
	{
		sSend(":%s NOTICE %s :Invalid e-mail address.",	NickServ, from);
		return RET_SYNTAX;
	}

	sSend(":%s GLOBOPS :%s sets email for nick %s from \"%s\" to \"%s\"", NickServ, from, args[1],
		regnick->email, args[2]);
	operlog->log(nick, NS_SETEMAIL, regnick->nick, 0, "%s -> %s", regnick->email,
				 args[2]);
	strncpyzt(regnick->email, args[2], EMAILLEN);
	return RET_FAIL;
}

/**************************************************************************/
