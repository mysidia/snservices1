/**
 * \file akill.c
 * \brief OperServ (services) banlist routines
 *
 * Procedures in this file are used by OperServ and services
 * timers to manage the autokill lists - this module also
 * contains the implementation of those lists.
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

#include "services.h"
#include "operserv.h"
#include "hash.h"
#include "email.h"
#include "log.h"
#include "sipc.h"
#include "timestr.h"

/*******************************************************************/
#define NUM_AKTYPE_INDICES 4

struct _akill_mail_exclude {
	char* nick;
	int count_removes[NUM_AKTYPE_INDICES];
	int count_adds[NUM_AKTYPE_INDICES];

	LIST_ENTRY(_akill_mail_exclude) dm_lst;
};

typedef struct _akill_mail_exclude SetterExclude;

static LIST_HEAD(, _akill_mail_exclude)  mailExcludedSetters;

/*******************************************************************/

/*! \def AKREASON_LEN
 * \brief Maximum length of an autokill reason
 */

/*******************************************************************/

void saveKlineQueue();
void loadKlineQueue();

/**
 * \brief Highest akill identifying number
 * \todo Highest stamp should save to services.totals
 */
unsigned long top_akill_stamp = 0;

/*! \brief First Item in the autokill list */
static struct akill *firstBanItem = NULL;

EmailMessage kline_email;		///< E-mail to be sent to kline@
EmailString kline_enforce_buf;	///< Buffer of akill enforcement logs
EmailMessage ops_email;			///< E-mail to be sent to ops@
EmailString ops_enforce_buf;	///< Enforce buffer to be mailed to ops
int kline_email_nitems = 0;		///< Number of items in the kline queue for kline@
int ops_email_nitems = 0;		///< Number of items in the kline queue for ops@

time_t kline_e_last_time;		///< Time of last kline@ kline queue message
time_t ops_e_last_time;			///< Time of last ops@ kline queue message


/*****************************************************************************/

int IpcConnectType::sendAkills(int akillType, const char *searchText)
{
	struct akill *ak;
	char mask[NICKLEN + USERLEN + HOSTLEN + 3];
	char length[16];
	int x = 0;

	if (!firstBanItem)
		return 0;

	for (ak = firstBanItem; ak; ak = ak->next) {
		if (ak->type == akillType || ak->type == 0) {
			strcpy(mask, ak->nick);
			strcat(mask, "!");
			strcat(mask, ak->user);
			strcat(mask, "@");
			strcat(mask, ak->host);

			if (searchText && strcmp(mask, searchText) )
				continue;

			if (ak->unset)
				sprintf(length, "%luh",
						(long)(ak->unset - ak->set) / 3600);
			else
				strcpy(length, "forever");

			fWriteLn("DATA %ld %ld %s %s :%s", ak->set, ak->unset, length, mask, ak->reason);
			x++;
		}
	}
	return x;
}

/**
 * \brief Excluded the given nick from akill mailings to ops@ and kline@
 * \param nick -- The setter to be excluded, non-null zero terminated char array
 */
void makeSetterExcludedFromKlineMails(const char* nick)
{
	SetterExclude* se;
	int i;
	
	if (nick == 0 || !*nick)
		return;

	// Never list one twice
	for(se = LIST_FIRST(&mailExcludedSetters); se; se = LIST_NEXT(se, dm_lst))
	{
		if (se->nick && !str_cmp(se->nick, nick))
			return;
	}

	se = (SetterExclude*)oalloc(sizeof(SetterExclude));
	LIST_ENTRY_INIT(se, dm_lst);
	se->nick = (char *)oalloc(strlen(nick)+1);
	strcpy(se->nick, nick);

	for(i = 0; i < NUM_AKTYPE_INDICES; i++) {
		se->count_adds[i] = 0;
		se->count_removes[i] = 0;
	}

	LIST_INSERT_HEAD(&mailExcludedSetters, se, dm_lst);
}

/**
 * \brief Handles an AutoKill/AutoHurt/Ignore/... list request
 * \param from Oper requesting list
 * \param type Type of list requested (ie: #A_AKILL)
 * \pre From points to a valid NUL-terminated character array bearing
 *      the nickname of an online IRC oper.  Type is one of the akill
 *      type constants (ex: #A_AKILL, #A_IGNORE, #A_AHURT);
 */
void listAkills(char *from, char type)
{
	struct akill *ak;
	char mask[NICKLEN + USERLEN + HOSTLEN + 3];
	char length[16] = {};
	struct tm *t;

	if (!firstBanItem) {
		sSend(":%s NOTICE %s :There are no autokills/ignores.", OperServ,
			  from);
		return;
	}
	sSend(":%s NOTICE %s :%-14s %-8s %-20s %-10s %s", OperServ, from,
		  "SetTime", "Length", "Mask", "Set By", "Reason");
	for (ak = firstBanItem; ak; ak = ak->next) {
		if (ak->type == type || ak->type == 0) {
			strcpy(mask, ak->nick);
			strcat(mask, "!");
			strcat(mask, ak->user);
			strcat(mask, "@");
			strcat(mask, ak->host);

			t = localtime(&(ak->set));

			if (ak->unset) 
			{
				TimeLengthString tls(ak->unset - ak->set);

				ASSERT((ak->unset - ak->set) >= 0);

				if (tls.isValid() == false) 
				{
					sprintf(length, "%luh",
							(long)(ak->unset - ak->set) / 3600);
				}
				else
				{
					tls.asString(length, sizeof(length) - 1, true, false, false);
				}
			}
			else
				strcpy(length, "forever");

			sSend
				(":%s NOTICE %s :%02d/%02d/%02d-%02d:%02d %-8s %-20s %-10s %s",
				 OperServ, from, t->tm_mon + 1, t->tm_mday,
				 (t->tm_year % 100), t->tm_hour, t->tm_min, length, mask,
				 ak->setby, ak->reason);
		}
	}
	sSend(":%s NOTICE %s :End of list.", OperServ, from);
}

/**
 * \brief Returns #TRUE if a user is akilled
 * \param nick The user's nickname
 * \param user The users's username
 * \param host The user's hostname
 * \pre Nick, user, and host each point to separate, valid
 *      NUL-terminated character arrays.  Nick is a valid
 *      IRC nickname, user is a valid IRC username, and host
 *      is a valid IRC hostname.
 */
int isAKilled(char *nick, char *user, char *host)
{
	struct akill *ak;

	for (ak = firstBanItem; ak; ak = ak->next) {
		if (!match(ak->nick, nick) && !match(ak->user, user)
			&& !match(ak->host, host) && (ak->type == A_AKILL))
			return 1;
	}
	return 0;
}


struct akill* getAkill(char *nick, char *user, char *host)
{
	struct akill *ak;

	for (ak = firstBanItem; ak; ak = ak->next) {
		if (!match(ak->nick, nick) && !match(ak->user, user)
			&& !match(ak->host, host) && (ak->type == A_AKILL))
			return ak;
	}
	return (struct akill*)0;
}

long getAkillId(struct akill* ak)
{
	return ak->id;
}

struct akill* getAhurt(char *nick, char *user, char *host)
{
	struct akill *ak;

	for (ak = firstBanItem; ak; ak = ak->next) {
		if (!match(ak->nick, nick) && !match(ak->user, user)
			&& !match(ak->host, host) && (ak->type == A_AHURT))
			return ak;
	}
	return (struct akill*)0;
}

char* getAkReason(struct akill *ak)
{
	return ak->reason;
}

/** \return A reason string if the user described is akilled, otherwise
 *          a null pointer.
 * \param nick The user's nickname
 * \param user The users's username
 * \param host The user's hostname
 * \pre  Nick, user, and host each point to separate, valid
 *       NUL-terminated character arrays.  Nick is a valid
 *       IRC nickname, user is a valid IRC username, and host
 *       is a valid IRC hostname.
 * \post If the user is found, then if appropriate an
 *       AKILL or AHURT message is sent.
 */
char* applyAkill(char* nick, char* user, char* host, struct akill* ak)
{
	char buf[1024];

/*	if (!match(ak->nick, nick) && !match(ak->user, user)
		&& !match(ak->host, host)) */
	{
		char length[16];

		/* if the nick is wildcarded, reset the akill */
		if (!strcmp(ak->nick, "*")) {
			if (ak->unset)
				sprintf(length, "%luh",
					(long)(ak->unset - ak->set) / 3600);
			else
				strcpy(length, "forever");

		if (ak->type == A_AKILL)
			sSend
				(":%s AKILL %s %s :[#%.6x] Autokilled for %s [%s]",
				 myname, ak->host, ak->user, ak->id, length,
				 ak->reason);
		else if (ak->type == A_AHURT)
			sSend(":%s AHURT %s %s :[#%.6x] Autohurt for %s [%s]",
				  myname, ak->host, ak->user, ak->id, length,
				  ak->reason);
		}

		if (ak->type == A_AKILL)
			sprintf(buf,
				"[#%.6x] Autokill %s!%s@%s enforced on user %s!%s@%s at %s\n",
				ak->id, ak->nick, ak->user, ak->host, nick, user,
				host, ctime(&CTime));
		else if (ak->type == A_AHURT)
			sprintf(buf,
				"[#%.6x] Autohurt %s!%s@%s enforced on user %s!%s@%s at %s\n",
				ak->id, ak->nick, ak->user, ak->host, nick, user,
				host, ctime(&CTime));
#ifdef AKILLMAILTO
		kline_enforce_buf += buf;
		kline_email_nitems++;
#endif

#ifdef OPSMAILTO
		ops_enforce_buf += buf;
		ops_email_nitems++;
#endif

	if (ak->type == A_AKILL)
			return ak->reason;
	}

	return NULL;
}

/**
 * \brief Returns #TRUE if a user is autohurt
 * \param nick The user's nickname
 * \param user The users's username
 * \param host The user's hostname
 * \pre Nick, user, and host each point to separate
 *      NUL-terminated character arrays.  Respectively,
 *      a valid nickname, a valid username, and a valid IRC
 *      hostname.
 */
int isAHurt(char *nick, char *user, char *host)
{
#ifdef ENABLE_AHURT
	struct akill *ak;

	for (ak = firstBanItem; ak; ak = ak->next) {
		if (!match(ak->nick, nick) && !match(ak->user, user)
			&& !match(ak->host, host) && (ak->type == A_AHURT))
			return 1;
	}
#endif
	return 0;
}

/**
 * \brief Returns #TRUE if a user is ignored
 * \param nick The user's nickname
 * \param user The users's username
 * \param host The user's hostname
 * \pre Nick, user, and host each point to separate
 *      NUL-terminated character arrays.  Respectively,
 *      a valid nickname, a valid username, and a valid IRC
 *      hostname.
 */
int isIgnored(char *nick, char *user, char *host)
{
	struct akill *ak;

	for (ak = firstBanItem; ak; ak = ak->next) {
		if (!match(ak->nick, nick) && !match(ak->user, user)
			&& !match(ak->host, host) && (ak->type == A_IGNORE))
			return 1;
	}
	return 0;
}


/** \brief Apply a new autokill/ban to online users
 *  \param Pointer to an akill item (struct akill) to be applied
 *  \pre   Ak is a valid, listed autokill object.
 *  \post  IRC kill messages have been sent for all users
 *         that are effected by this autokill.  Similarly
 *         HURTSETS for all new ahurt users.  A remUser call
 *         is made for each user killed, so one or more online
 *         user structures may no longer be valid.
 */
void checkAkillAllUsers(struct akill *ak)
{
	HashKeyVal hashEnt;
	char ak_pattern[NICKLEN + USERLEN + HOSTLEN + 25];
	char user_pattern[NICKLEN + USERLEN + HOSTLEN + 25];
	char buf[MAXBUF];
	UserList *tmpnick, *tmpnext;

	if (!ak || (ak->type != A_AKILL && ak->type != A_AHURT))
		return;
#ifndef IRCD_HURTSET
	if (ak->type == A_AHURT)
		return;
#endif
	if (!ak->nick || !ak->user || !ak->host)
		return;

	snprintf(ak_pattern, sizeof(ak_pattern), "%s!%s@%s", ak->nick,
			 ak->user, ak->host);

	for (hashEnt = 0; hashEnt < NICKHASHSIZE; hashEnt++)
		for (tmpnick = LIST_FIRST(&UserHash[hashEnt]); tmpnick != NULL;
			 tmpnick = tmpnext) {
			tmpnext = LIST_NEXT(tmpnick, ul_lst);
			if (tmpnick->oflags & NISOPER)
				continue;
			if (ak->type == A_AHURT
				&& (tmpnick->caccess >= 2 && tmpnick->reg
					&& ((tmpnick->reg->flags & NBYPASS)
						|| (tmpnick->reg->opflags))))
				 continue;

			snprintf(user_pattern, sizeof(user_pattern), "%s!%s@%s",
					 tmpnick->nick, tmpnick->user, tmpnick->host);
			if (match(ak_pattern, user_pattern))
				continue;

			if (ak->type == A_AHURT) 
			{
				/* sSend(":%s HURTSET %s 3 :AutoHurt User", OperServ,
					  tmpnick->nick); */
				/* tmpnick->oflags |= NISAHURT; */
			}
			if (ak->type == A_AKILL) {
				char inick[NICKLEN + 1];
				strncpyzt(inick, tmpnick->nick, sizeof(inick));
				sSend(":%s KILL %s :%s!%s (AKilled: %s)", services[1].name,
					  tmpnick->nick, services[1].host, services[1].name,
					  ak->reason);

				sprintf(buf,
						"[#%.6x] Autokill %s!%s@%s enforced on existing user %s!%s@%s at %s\n",
						ak->id,
						ak->nick, ak->user, ak->host, tmpnick->nick,
						tmpnick->user, tmpnick->host, ctime(&CTime));
#ifdef AKILLMAILTO
				kline_enforce_buf += buf;
				kline_email_nitems++;
#endif

#ifdef OPSMAILTO
				ops_enforce_buf += buf;
				ops_email_nitems++;
#endif

/* XXX: Where did this remUser come from ? Note to check rev 1.69<->1.70 */
				remUser(inick, 1);

				if (strcmp(ak->nick, "*")) {
					addGhost(inick);
					timer(30, delTimedGhost, strdup(inick));
				}
				continue;
			}
		}
}

/**
 * \brief Addakill adds an item to an OperServ autokill/ignore/autohurt/other
 *  list
 * \return 0 if successful, -1 if failed
 * \param length Duration (in seconds) to leave item in the list, 0 for
 *        permanent
 * \param mask This is the mask to be affected by the entry
 * \param by This is the nickname of the user that was responsible for 
 *        creating the entry (OperServ for automatic)
 * \param type This is the entry type (A_AKILL, A_AHURT, A_IGNORE)
 * \param reason This is the reason the entry needs to be set
 * \pre   Length is an integer between 0 and INT_MAX.
 *        Mask is a pointer to a valid NUL-terminated character array.
 *        By is a pointer to a valid, NUL-terminated character array
 *        that holds a valid IRC nickname.
 *        Type is one of the A_xx constants for autokills as defined
 *        in operserv.h (ex: #A_AKILL).  
 *        Reason is a pointer to a valid, non-zero, NUL-terminated
 *        character array.
 * \post  An autokill record is added.  One or more users might have
 *        been killed or autohurt.  One or more nickname structures
 *        may have been removed from the online users list and their
 *        memory freed.
 */
int addakill(long length, char *mask, char *by, char type, char *reason)
{
	struct akill *ak;
	char *akmask;
	int i, j;
	char temp[75] = {};

	ak = (struct akill *)oalloc(sizeof(struct akill));
	akmask = strdup(mask);

	ak->nick = akmask;

	j = strlen(akmask);
	for (i = 0; i < j; i++) {
		if (akmask[i] == '!') {
			akmask[i] = 0;
			ak->user = &(akmask[i + 1]);
		}
		if (akmask[i] == '@') {
			akmask[i] = 0;
			ak->host = &(akmask[i + 1]);
		}
	}
	if (firstBanItem)
		firstBanItem->prev = ak;
	ak->next = firstBanItem;
	ak->prev = NULL;
	firstBanItem = ak;

	strcpy(ak->setby, by);

	ak->set = time(NULL);
	if (length == 0)
		ak->unset = 0;
	else
		ak->unset = ak->set + length;

	ak->type = type;
	ak->id = ++top_akill_stamp;
	strncpyzt(ak->reason, reason, AKREASON_LEN);

	if (length == 0)
		strcpy(temp, "forever.");
	else {
		TimeLengthString tls(length);

		if (tls.isValid() == false)
			snprintf(temp, 16, "%d hours.", (int)length / 3600);
		else
			tls.asString(temp, sizeof(temp) - 1, true, true, false);
	}

	sSend(":%s GLOBOPS :%s set %s for %s for %s [%s]", OperServ, by,
		  aktype_str(type, 0), mask, temp, reason);
	if (type == A_AKILL && !strcmp(ak->nick, "*"))
		sSend(":%s AKILL %s %s :Autokilled [#%.6x] for %s [%s]", myname,
			  ak->host, ak->user, ak->id, temp, ak->reason);
	else if (type == A_AHURT && !strcmp(ak->nick, "*"))
		sSend(":%s AHURT %s %s :Autohurt [#%.6x] for %s [%s]", myname,
			  ak->host, ak->user, ak->id, temp, ak->reason);

	if (ak->unset)
		ak->tid = timer(length, autoremoveakill, strdup(mask));
	else
		ak->tid = 0;
#if defined(AKILLMAILTO) || defined(OPSMAILTO)
	queueakill(mask, ak->setby, temp, reason, ak->set, type, ak->id, 1);
#endif
	if (type == A_AHURT || type == A_AKILL)
		checkAkillAllUsers(ak);
	return 0;
}

#ifndef AKILLMAILTO
/*void timed_akill_queue(char *)
{
	return;
}*/
#endif

// Used by queueAkill and kline mailing only
int akill_flag_toindex(int type_flag)
{
	if (type_flag & A_AKILL)
		return 0;
	if (type_flag & A_AHURT)
		return 2;
	if (type_flag & A_IGNORE)
		return 1;
	return 3;
}

// Used by queueAkill and kline mailing only
int akill_index_toflag(int index)
{
	switch(index)
	{
		case 0: return A_AKILL;
		case 1: return A_IGNORE;
		case 2: return A_AHURT;
		default: return A_AKILL;
	}
}


/**
 * \brief Adds information about an autokill to the body of the kline queue
 *  that is periodically mailed to the AKILLMAILTO address
 * \param mask This is the mask of the item to be queued
 * \param length This is a string representing the duration
 * \param setby This is a string representing the nickname who created the item
 * \param reason This is a string representing the reason the entry was created
 * \param time This is the time at which the list entry was added (UTC)
 * \param type This is the type of entry (A_AKILL, for example)
 * \param added If #TRUE, then an entry has been added.  If FALSE, then an entry
 *        has been removed.
 */
void queueakill(char *mask, char *setby, char *length, char *reason,
				time_t time, int type, int id, int added)
{
	static char buf[8192];
	SetterExclude* se, *excluded = 0;

	if (added) {
		sprintf(buf,
				"[#%.6x] %.*s was %.*s by %.*s for %.*s\nReason: %.*s\n"
				"Set At: %.*s\n", id, 255, mask, 255, aktype_str(type, 1),
				NICKLEN, setby, 255, length, 255, reason, 255,
				ctime(&(time)));
	} else {
		sprintf(buf, "[#%.6x] %.*s removed %.*s %.*s at %.*s\n", id,
				NICKLEN, setby, 255, aktype_str(type, 0),
				NICKLEN + USERLEN + HOSTLEN + 25, mask, 255,
				ctime(&(time)));
	}

	for(se = LIST_FIRST(&mailExcludedSetters); se; se = LIST_NEXT(se, dm_lst))
	{
		if (se->nick && !str_cmp(se->nick, setby))
		{
			excluded = se;
			break;
		}	
	}

#ifdef AKILLMAILTO
	kline_email.body.add(buf);
	kline_email_nitems++;
#endif

#ifdef OPSMAILTO	
	if (excluded == 0)
	{
		ops_email.body.add(buf);
		ops_email_nitems++;
	}
	else {		
		int x = akill_flag_toindex(type);

		if (x >= 0 && x < NUM_AKTYPE_INDICES) 
		{
			if (added) {
				excluded->count_adds[x]++;
			}
			else {
				excluded->count_removes[x]++;
			}	
		}
		ops_email_nitems++;
	}
#endif	
}


/**
 * \brief Returns a string indicating which type of OperServ banlist item we have
 * \param type Represents the type of list item (such as #A_AKILL)
 * \param which Indicates which string to print, '0' is the standard form
 * such as 'autokill', '1' is the capitalized form such as 'Autokill',
 * and '2' is the past-tense verb form such as 'autokilled'.  No other value
 * for which is allowed.
 *
 * \par Returns a string representing the type of list item
 */
const char *aktype_str(int type, int which)
{
	int i = 0;
	struct akill_type_string_data
	{
		int type;
		char *simple;
		char *usimple;
		char *action;
	} akill_strings[] =
	{
		{
		A_AKILL, "autokill", "Autokill", "autokilled"}
		, {
		A_IGNORE, "ignore", "Ignore", "ignored"}
		, {
		A_AHURT, "autohurt", "Autohurt", "autohurt"}
		, {
		0x0, NULL, NULL, NULL}
	};

	for (i = 0; akill_strings[i].simple; i++) {
		if (akill_strings[i].type == type)
			switch (which) {
			default:
			case 0:
				return akill_strings[i].simple;
			case 1:
				return akill_strings[i].action;
			case 2:
				return akill_strings[i].usimple;
			}
	}

	return ("banitem");
}


/**
 * \brief Sends out services kline queue e-mail messages when the time arrives
 * for them to be sent.
 */
void timed_akill_queue(char *)
{
	char buf1[1024], buf2[1024];
	SetterExclude* se = LIST_FIRST(&mailExcludedSetters);
	int i, l;

#ifdef AKILLMAILTO
	if (
		((kline_email_nitems > 0)
		 && ((CTime - kline_e_last_time) >= 60 * 60 * 12))
		|| (kline_email_nitems >= 100)) {
		sprintf(buf1, "\"Operator Services - Kline\" <%s@%s>", OperServ,
				NETWORK);

		kline_email.to = AKILLMAILTO;
		kline_email.from = buf1;
		kline_email.subject = "Queued Kline list";
		if (kline_enforce_buf.length() > 0) {
			kline_email.body.add("");
			kline_email.body.add(kline_enforce_buf.get_string());
			kline_enforce_buf.set_string(NULL);
		}
		kline_email.send();
		kline_email.reset();
		kline_email_nitems = 0;
		kline_e_last_time = time(NULL);
		saveKlineQueue();
	} else if (!kline_email_nitems)
		kline_e_last_time = time(NULL);
#endif

#if defined(OPSMAILTO)
	if ((ops_email_nitems > 0)
		&& ((CTime - ops_e_last_time) >= 60 * 60 * 24 * 3)) {
		sprintf(buf1, "\"Operator Services - Kline\" <%s@%s>", OperServ,
				NETWORK);

		ops_email.to = OPSMAILTO;
		ops_email.from = buf1;
		ops_email.subject = "Kline log";
		if (se)
		{
			ops_email.body.add("");
			ops_email.body.add("Excluded Setters/Remover Activity: \n");
			ops_email.body.add("Nick               Ak+     Ig+     Ah+"
					   "     ?      Ak-     Ig-      Ah-  --\n");
			for(; se; se = LIST_NEXT(se, dm_lst))
			{
				if (!se->nick) {
					continue;
				}
				l = sprintf(buf2,      "%-18.20s", se->nick);

				for(i = 0 ; i < NUM_AKTYPE_INDICES; i++) {
					l += sprintf(buf2 + l, " %-7d", se->count_adds[i]);

					se->count_adds[i] = 0;
				}

				for(i = 0 ; i < NUM_AKTYPE_INDICES; i++) {
					l += sprintf(buf2 + l, " %-7d", se->count_removes[i]);
					se->count_removes[i] = 0;
				}
				
				ops_email.body.add(buf2);
				ops_email.body.add("\n");
			}
			ops_email.body.add("\n");
		}
		if (ops_enforce_buf.length() > 0) {
			ops_email.body.add("");
			ops_email.body.add(kline_enforce_buf.get_string());
			ops_enforce_buf.set_string(NULL);
		}
		ops_email.send();
		ops_email.reset();
		ops_email_nitems = 0;
		ops_e_last_time = time(NULL);
		saveKlineQueue();
	} else if (!ops_email_nitems)
		ops_e_last_time = time(NULL);
#endif

#if defined(AKILLMAILTO) || defined(OPSMAILTO)
	timer(60 * 60, timed_akill_queue, NULL);
#endif
}

/**
 * \brief Handles a user attempting to remove an akill
 * \return 0 if successful, -1 if failed
 * \param from Nicname attempting to remove akill
 * \param mask Mask that removal of is attempted
 */
int removeAkill(char *from, char *mask)
{
	int i, j;
	struct akill *ak;
	char *nick, *user = NULL, *host = NULL, *oldmask;
	time_t temp;

	oldmask = strdup(mask);

	j = strlen(mask);
	nick = mask;
	for (i = 0; i < j; i++) {
		if (mask[i] == '!') {
			mask[i] = 0;
			user = &(mask[i + 1]);
		}
		if (mask[i] == '@') {
			mask[i] = 0;
			host = &(mask[i + 1]);
		}
	}

	for (ak = firstBanItem; ak; ak = ak->next) {
		if (!strcmp(host, ak->host) && !strcmp(user, ak->user)
			&& !strcmp(nick, ak->nick)) {
			if (strcasecmp(from, OperServ) && strcasecmp(from, ak->setby)) {
				UserList *fromnl = getNickData(from);
				if (fromnl && !opFlagged(fromnl, OAKILL)) {
					sSend
						(":%s NOTICE %s :Permission denied -- that akill was set by %s.",
						 OperServ, from, ak->setby);
					if (oldmask)
						FREE(oldmask);
					return -1;
				}
			}
			sSend(":%s GLOBOPS :%s is removing %s %s.", OperServ, from,
				  aktype_str(ak->type, 0), oldmask);
			temp = time(NULL);

#if defined(AKILLMAILTO) || defined(OPSMAILTO)
			queueakill(oldmask, from, NULL, NULL, temp, ak->type, ak->id,
					   0);
#endif

			if (ak->prev)
				ak->prev->next = ak->next;
			if (ak->next)
				ak->next->prev = ak->prev;
			if (ak == firstBanItem)
				firstBanItem = ak->next;
			if (ak->type == A_AKILL && !strcmp(nick, "*"))
				sSend(":%s RAKILL %s %s", myname, ak->host, ak->user);
			if (ak->type == A_AHURT && !strcmp(nick, "*"))
				sSend(":%s RAHURT %s %s", myname, ak->host, ak->user);
			FREE(ak->nick);
/*
 * Fun fact:  If OperServ is removing the autokill, the timer is up, and
 * it shouldn't be canceled, or two timers will be cancelled...
 */
			/*! \bug This looks ugly */
			if (strcmp(from, OperServ) == 0);
			else {
				sSend(":%s NOTICE %s :%s %s removed", OperServ, from,
					  aktype_str(ak->type, 0), oldmask);
				cancel_timer(ak->tid);
			}
			FREE(ak);
			FREE(oldmask);
			return 0;
		}
	}

	if (strcmp(from, OperServ)) {
		sSend(":%s NOTICE %s :Autokill/AutoHurt/Ignore %s not found",
			  OperServ, from, oldmask);
	}
	FREE(oldmask);
	return -1;
}

/**
 * \brief Handles a user attempting to remove an OperServ banlist item
 * \return 0 if successful, -1 if failed
 * \param from Nicname attempting to remove akill
 * \param mask Mask that removal of is attempted
 * \param type Type of entry removal is being attempted of (such as #A_AKILL)
 * \param restrict [UNUSED]
 */
int removeAkillType(char *from, char *mask, int type, int restrict)
{
	int i, j;
	struct akill *ak;
	char *nick, *user = NULL, *host = NULL, *oldmask;
	time_t temp;

	oldmask = strdup(mask);

	j = strlen(mask);
	nick = mask;
	for (i = 0; i < j; i++) {
		if (mask[i] == '!') {
			mask[i] = 0;
			user = &(mask[i + 1]);
		}
		if (mask[i] == '@') {
			mask[i] = 0;
			host = &(mask[i + 1]);
		}
	}

	for (ak = firstBanItem; ak; ak = ak->next) {
		if (ak->type == type || !type)
			if (!strcmp(host, ak->host) && !strcmp(user, ak->user)
				&& !strcmp(nick, ak->nick)) {


				if (strcasecmp(from, OperServ) && strcasecmp(from, ak->setby)) {
					UserList *fromnl = getNickData(from);
					if (fromnl && !opFlagged(fromnl, OAKILL)) {
						sSend
							(":%s NOTICE %s :Permission denied -- that akill was set by %s.",
							 OperServ, from, ak->setby);
						continue;
					}
				}


				sSend(":%s GLOBOPS :%s is removing %s %s.", OperServ, from,
					  aktype_str(ak->type, 0), oldmask);
				temp = time(NULL);

#if defined(AKILLMAILTO) || defined(OPSMAILTO)
				queueakill(oldmask, from, NULL, NULL, temp, ak->type,
						   ak->id, 0);
#endif
				if (ak->prev)
					ak->prev->next = ak->next;
				if (ak->next)
					ak->next->prev = ak->prev;
				if (ak == firstBanItem)
					firstBanItem = ak->next;
				if (ak->type == A_AKILL && !strcmp(nick, "*"))
					sSend(":%s RAKILL %s %s", myname, ak->host, ak->user);
				if (ak->type == A_AHURT && !strcmp(nick, "*"))
					sSend(":%s RAHURT %s %s", myname, ak->host, ak->user);

	  /**
        * \bug You say "If OperServ is removing the kill", but you just
        *      strcmp() != 0'ed against OperServ... what's the deal?
        *      Oh yeah.. you cancel too, bad comment?
        */
				if (strcmp(from, OperServ)) {
					sSend(":%s NOTICE %s :%s %s removed", OperServ, from,
						  aktype_str(ak->type, 0), oldmask);
					cancel_timer(ak->tid);	/* If OperServ is removing the kill, timer is up&shouldn't be canceled */
				}
				FREE(ak);
				FREE(oldmask);
				return 0;
			}
	}
	if (strcmp(from, OperServ)) {
		sSend(":%s NOTICE %s :%s %s not found", OperServ, from,
			  aktype_str(type, 2), oldmask);
	}
	return -1;
}

/**
 * \brief Handles automatic removal of expired akills by OperServ
 * \param mask Mask to be removed
 * 
 * \par Warning: this function calls free() on the mask passed
 */
void autoremoveakill(char *mask)
{
	removeAkill(OperServ, mask);
	FREE(mask);
}

/**
 * \brief Saves the akill database
 * \par Note that ignores, autohurts, ... are saved as well
 */
void saveakills()
{
	FILE *aksave;
	struct akill *ak;

	saveKlineQueue();

	aksave = fopen("operserv/akill.db", "w");

	for (ak = firstBanItem; ak != NULL; ak = ak->next) {
		fputs("-\n", aksave);
		fprintf(aksave, "#ID %d\n", ak->id);
		fputs(ak->nick, aksave);
		fputs("\n", aksave);
		fputs(ak->user, aksave);
		fputs("\n", aksave);
		fputs(ak->host, aksave);
		fputs("\n", aksave);
		fprintf(aksave, "%lu %lu\n", (long)ak->set, (long)ak->unset);
		fputs(ak->setby, aksave);
		fputs("\n", aksave);
		fputs(ak->reason, aksave);
		fputs("\n", aksave);
		fprintf(aksave, "%d\n", ak->type);
	}
	fclose(aksave);
	return;
}

/**
 * \brief Loads the queued e-mail pieces to be sent to the AKILLMAILTO
 * address
 * \bug FIXME: Blank lines that were supposed to be in the message
 *      are not getting through services restarts!!
 */
void loadKlineQueue()
{
	FILE *fp = fopen("operserv/kline_queue.db", "r");
	char buf[1024];
	EmailMessage *email;
	int lines = 0;

	if (!fp)
		return;
	if (fgets(buf, 255, fp)) {
		kline_e_last_time = 0;
		ops_e_last_time = 0;
		sscanf(buf, "%ld:%ld", &kline_e_last_time, &ops_e_last_time);
	} else {
		fclose(fp);
		return;
	}

	email = &kline_email;

	while (fgets(buf, 255, fp)) {
		if (buf[0] && buf[0] != '\n' && strchr(buf, '\n')
                    && buf[1] == '\n') {
			buf[strlen(buf) - 1] = 0;
		}

		if (*buf == 'F')
			email->from.add(buf + 1);
		else if (*buf == 'T') {
			email->to.add(buf + 1);
		} else if (*buf == 'S')
			email->subject.add(buf + 1);
		else if (*buf == 'B') {
			email->body.add(buf + 1);
			lines++;
		} else if (*buf == 'E') {
			if (lines) {
				if (email == &kline_email)
					kline_email_nitems++;
				else if (email == &ops_email)
					ops_email_nitems++;
			}

			if (email == &ops_email)
				break;
			email = &ops_email;
			lines = 0;
		}
	}

	while (fgets(buf, 255, fp)) {
		if (buf[0] && buf[0] != '\n' && strchr(buf, '\n')
                    && buf[1] == '\n') {
			buf[strlen(buf) - 1] = 0;
		}

		if (*buf == 'X')
			kline_enforce_buf.add(buf + 1);
		else if (*buf == 'O')
			ops_enforce_buf.add(buf + 1);
        }

	if (fclose(fp) < 0) {
		perror("fclose");
	}
}

/**
 * \brief Splits a string across \n and prints each line starting
 * with a prefix character
 * \param fp File to print to
 * \param pre Prefix character
 * \param str String to print
 */
void parseFprint(FILE * fp, char pre, const char *str)
{
	char buf[8192], *s, *a;

	if (!fp || !str)
		return;
	strncpyzt(buf, str, sizeof(buf));
	buf[sizeof(buf) - 1] = '\0';

	for (s = a = buf; *s; s++) {
		if (*s == '\n') {
			*s = '\0';
			fprintf(fp, "%c%s\n", pre, a);
			*s = '\n';
			a = s + 1;
		}
	}
	if (a[0] && a[0] != '\n');
	fprintf(fp, "%c%s\n", pre, a);
}

/**
 * \brief Keep the kline queue stored to disk before it's mailed
 */
void saveKlineQueue()
{
	FILE *fp = fopen("operserv/kline_queue.db", "w");
	EmailMessage *email;
	int i = 0;

	if (!fp)
		return;

	fprintf(fp, "%ld:%ld\n", kline_e_last_time, ops_e_last_time);

	for(i = 0; i < 2; i++)
	{
		switch(i) {
			case 1: email = &ops_email; break;
			default: email = &kline_email; break;
		}

		parseFprint(fp, 'F', email->from.get_string());
		parseFprint(fp, 'T', email->to.get_string());
		parseFprint(fp, 'S', email->subject.get_string());
		parseFprint(fp, 'B', email->body.get_string());
		fprintf(fp, "E\n");
	}

	if (kline_enforce_buf.length() > 0)
		parseFprint(fp, 'X', kline_enforce_buf.get_string());

	if (ops_enforce_buf.length() > 0)
		parseFprint(fp, 'O', ops_enforce_buf.get_string());

	if (fclose(fp) < 0) {
		perror("fclose");
	}
}

/**
 * \brief Load the akill/kline databases
 */
void loadakills()
{
	FILE *akload;
	struct akill *ak;
	char buffer[256], mask[NICKLEN + USERLEN + HOSTLEN + 2], temp[16];
	int i = 0, t, expired = 0, count = 0, start;
	long length;

	start = time(NULL);

	loadKlineQueue();

	akload = fopen("operserv/akill.db", "r");

	if (akload == NULL) {
		sSend(":%s GLOBOPS :Autokill EXPIRE(0/0) in %ld seconds", OperServ,
			  (long)(time(NULL) - start));
		return;
	}

	while (sfgets(buffer, 256, akload)) {
/*
 * Check for sync 
 */
		++i;

		if (strcmp(buffer, "-") != 0) {
			sSend("%s GLOBOPS :Sync error in akill.db near line %d",
				  OperServ, i);
			fprintf(corelog, "Sync error in akill.db near line %d", i);
			return;
		}
		ak = (struct akill *)oalloc(sizeof(struct akill));

		sfgets(buffer, 256, akload);
		++i;

		if (*buffer != '#') {
			ak->nick = strdup(buffer);
			ak->id = ++top_akill_stamp;
		} else {
			if (!strncmp(buffer, "#ID ", 4)) {
				ak->id = atoi(buffer + 4);
				if (((unsigned)ak->id) > top_akill_stamp)
					top_akill_stamp = ak->id;
			} else
				ak->id = ++top_akill_stamp;
			sfgets(buffer, 256, akload);
			++i;
			ak->nick = strdup(buffer);
		}
		sfgets(buffer, 256, akload);
		++i;
		ak->user = strdup(buffer);
		sfgets(buffer, 256, akload);
		++i;
		ak->host = strdup(buffer);
		fscanf(akload, "%lu %lu\n", (long *)&(ak->set),
			   (long *)&(ak->unset));
		++i;
		sfgets(ak->setby, NICKLEN, akload);
		++i;
		sfgets(buffer, 255, akload);
		++i;
		strncpyzt(ak->reason, buffer, AKREASON_LEN);
		fscanf(akload, "%d\n", &t);
		++i;

		ak->type = t;
		ak->tid = 0;

		strcpy(mask, ak->nick);
		strcat(mask, "!");
		strcat(mask, ak->user);
		strcat(mask, "@");
		strcat(mask, ak->host);

		count++;
		length = ak->unset - ak->set;

		if (ak->unset)
			sprintf(temp, "%d hours", (int)(length / 3600));
		else
			strcpy(temp, "forever");

		if ((start < ak->unset) || (ak->unset == 0)) {
			if (ak->unset)
				ak->tid =
					timer((ak->unset - start), autoremoveakill,
						  strdup(mask));
			if (strcmp(ak->nick, "*") == 0) {
				if (ak->type == A_AKILL && !strcmp(ak->nick, "*"))
					sSend(":%s AKILL %s %s :Autokilled by %s for %s [%s]",
						  myname, ak->host, ak->user, ak->setby, temp,
						  ak->reason);
				if (ak->type == A_AHURT && !strcmp(ak->nick, "*"))
					sSend(":%s AHURT %s %s :AutoHurt for %s [%s]", myname,
						  ak->host, ak->user, temp, ak->reason);
			}
			ak->next = firstBanItem;
			ak->prev = NULL;
			if (ak->next)
				ak->next->prev = ak;
			firstBanItem = ak;
		} else {
			/*
			 * It's expired! 
			 */
			if (ak->type == A_AKILL)
				sSend(":%s RAKILL %s %s", myname, ak->host, ak->user);
			if (ak->type == A_AHURT)
				sSend(":%s RAHURT %s %s", myname, ak->host, ak->user);
			expired++;
			FREE(ak->nick);
			FREE(ak->user);
			FREE(ak->host);
			FREE(ak);
		}
	}
	sSend(":%s GLOBOPS :Autokill EXPIRE(%d/%d) in %ld seconds", OperServ,
		  expired, count, (long)(time(NULL) - start));
	if (top_akill_stamp)
		top_akill_stamp += 10;
}
