/**
 * \file chanserv.c
 * \brief ChanServ Implementation
 *
 * Functions related to the manipulation of registered channel
 * data, channel op list handling, and all channel management
 * and internal state data services deals with.
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
#include "chanserv.h"
#include "nickserv.h"
#include "macro.h"
#include "queue.h"
#include "hash.h"
#include "db.h"
#include "log.h"
#include "email.h"
#include "interp.h"
#include "chantrig.h"
#include "hash/md5pw.h"

#define ARGLEN 150
#define DEFIFZERO(value, default) ((value) ? (value) : (default))

/**
 * @brief Maximum length of a topic
 */
const int TOPIC_MAX = 350;

ChanList *firstChan = NULL;		///< First listed online channel
ChanList *lastChan = NULL;		///< Last listed online channel
RegChanList *firstRegChan = NULL;	///< First listed registered channel
RegChanList *lastRegChan = NULL;	///< Last listed registered channel

const char *GetAuthChKey(const char*, const char*, time_t, u_int32_t);
const char *PrintPass(u_char pI[], char enc);
char *urlEncode(const char *);

/* void initChanData(ChanList *); */

/* all these are ONLY for channel linked list funcs */
void createGhostChannel(char *);
void deleteGhostChannel(char *);
void deleteTimedGhostChannel(char *);

static cmd_return do_chanop(services_cmd_id, UserList *,
							RegChanList * chan, const char *nick,
							int level);
static cmd_return do_chanop_del(UserList * isfrom, RegChanList * chan,
								const char *cTargetNick, int tarLevel);
static cmd_return do_chanop_add(UserList * isfrom, RegChanList * chan,
								const char *cTargetNick, int tarLevel);
static cmd_return do_chanop_list(UserList * isfrom, RegChanList * chan,
								 const char *cTargetNick, int tarLevel);

/*! \brief This is a datatype used for creating the /ChanServ SET table. */
typedef struct _cs_settbl__
{
	char *cmd;					/*!< Name of the set command */
	  cmd_return(*func) (struct _cs_settbl__ *, UserList *, RegChanList *,
						 char **, int);	/*!< Set procedure, function */
	int aclvl;					/*!< Access level required to use this SET option */
	size_t xoff;				/*!< Offset of the channel variable (for generic procedures) */
	const char *optname;		/*!< Name of the option being adjusted */
	unsigned long int optlen;	/*!< Maximum length or value of a set */
}
cs_settbl_t;

cmd_return cs_set_passwd(cs_settbl_t *, UserList *, RegChanList *, char **,
						 int);
cmd_return cs_set_memolvl(cs_settbl_t *, UserList *, RegChanList *,
						  char **, int);
cmd_return cs_set_founder(cs_settbl_t *, UserList *, RegChanList *,
						  char **, int);
cmd_return cs_set_string(cs_settbl_t *, UserList *, RegChanList *, char **,
						 int);
cmd_return cs_set_fixed(cs_settbl_t *, UserList *, RegChanList *, char **,
						int);
cmd_return cs_set_bool(cs_settbl_t *, UserList *, RegChanList *, char **,
					   int);
cmd_return cs_set_mlock(cs_settbl_t *, UserList *, RegChanList *, char **,
						int);
cmd_return cs_set_restrict(cs_settbl_t *, UserList *, RegChanList *,
						   char **, int);
cmd_return cs_set_topiclock(cs_settbl_t *, UserList *, RegChanList *,
							char **, int);
cmd_return cs_set_encrypt(cs_settbl_t *, UserList *, RegChanList *,
							char **, int);

// *INDENT-OFF*
/*! \brief ChanServ command parse table */
interp::service_cmd_t chanserv_commands[] = {
  /* string     function         Flags      L */
  { "help",		cs_help,		 0,	LOG_NO,	0,		2},
  { "info",		cs_info,		 0,	LOG_NO,	0,		5},
  { "acc*",		cs_access,		 0,	LOG_NO,	CMD_MATCH,	3},
  { "chanop",	cs_chanop,		 0,	LOG_NO,	CMD_REG,	3},
  { "aop",		cs_chanop,		 0,	LOG_NO,	CMD_REG,	3},
  { "maop",		cs_chanop,		 0,	LOG_NO,	CMD_REG,	3},
  { "msop",		cs_chanop,		 0,	LOG_NO,	CMD_REG,	3},
  { "sop",		cs_chanop,		 0,	LOG_NO,	CMD_REG,	3},
  { "founder",	cs_chanop,		 0,	LOG_NO,	CMD_REG,	3},
  { "mfounder",	cs_chanop,		 0,	LOG_NO,	CMD_REG,	3},
  { "akick",	cs_akick,		 0,	LOG_NO,	CMD_REG,	3},
  { "register",	cs_register,	 0,	LOG_NO,	CMD_REG,	20},
  { "identify",	cs_identify,	 0,	LOG_NO,	CMD_REG,	15},
  { "id",		cs_identify,	 0,	LOG_NO,	CMD_REG,	15},
  { "addop",	cs_addop,		 0,	LOG_NO,	CMD_REG,	10},
  { "addak*",	cs_addak,		 0,	LOG_NO,	CMD_REG | CMD_MATCH,	10},
  { "delop",	cs_addop,		 0,	LOG_NO,	CMD_REG,	10},
  { "mdeop",	cs_mdeop,		 0,	LOG_NO,	CMD_REG,	10},
  { "mkick",	cs_mkick,		 0,	LOG_NO,	CMD_REG,	10},
  { "wipeak*",	cs_wipeak,		 0,	LOG_NO,	CMD_REG | CMD_MATCH,	10},
  { "wipeop",	cs_wipeop,	 OOPER|ODMOD,	LOG_OK,	CMD_REG | CMD_MATCH,	10},
  { "delak*",	cs_delak,		 0,	LOG_NO,	CMD_REG | CMD_MATCH,	10},
  { "listop*",	cs_listop,		 0,	LOG_NO,	CMD_REG | CMD_MATCH,	5},
  { "listak*",	cs_listak,		 0,	LOG_NO,	CMD_REG | CMD_MATCH,	5},
  { "delete",	cs_delete,	OOPER|OCBANDEL,	LOG_NO,	CMD_REG,		0},
  { "drop",		cs_drop,		 0,	LOG_NO,	CMD_REG,	20},
  { "op",		cs_op,			 0,	LOG_NO, 0,		20},
  { "deop",		cs_deop,		 0,	LOG_NO,	CMD_REG,	20},
  { "clist",	cs_clist,		 0,	LOG_NO,	CMD_REG,	 5},
  { "banish",	cs_banish,		 0,	LOG_NO,	CMD_REG,	 0},
  { "clean",	cs_clean,		 0,	LOG_NO, CMD_REG,	 3},
  { "close",	cs_close,		 0,	LOG_NO,	CMD_REG,	 0},
  { "hold",		cs_hold,		 0,	LOG_NO,	CMD_REG,	 0},
  { "mark",		cs_mark,	 	 0,	LOG_NO,	CMD_REG,	 0},
  { "m*lock",	cs_modelock,	 0,	LOG_NO,	CMD_REG | CMD_MATCH,	 3},
  { "restrict",	cs_restrict,	 0,	LOG_NO,	CMD_REG,	 3},
  { "t*lock",	cs_topiclock,	 0,	LOG_NO,	CMD_REG | CMD_MATCH,	 3},
  { "set",		cs_set,			 0,	LOG_NO,	CMD_REG,	 3},
  { "sendpass",	cs_getpass,	 	 0,	LOG_NO,	CMD_REG,	 5},
  { "getpass",	cs_getpass,	 	 0,	LOG_NO,	CMD_REG,	 5},
  { "setpass",  cs_setpass,		 0,	LOG_NO, CMD_REG,	 5},
  { "setrealpass",  cs_setrealpass,  OOPER,	LOG_OK, CMD_REG,	 5},
#ifndef NOGRPCMD
  { "getrealpass",cs_getrealpass,	 0,	LOG_NO,	CMD_REG,	 5},
#endif
  { "save",		cs_save,	 0,	LOG_NO,	CMD_REG,	 15},
  { "unban",	cs_unban,	 0,	LOG_NO,	CMD_REG,	  5},
  { "invite",	cs_invite,	 0,	LOG_NO,	CMD_REG,	  3},
  { "list",		cs_list,	 0,	LOG_NO,	CMD_REG,	  5},
  { "whois",	cs_whois,	 0,	LOG_NO,	CMD_REG,	  5},
  { "log",	cs_log,		OOPER,	LOG_NO, CMD_REG,	  1},
  { "dmod",	cs_dmod,		OOPER|ODMOD, LOG_OK, CMD_REG,	3},
  { "trigger",	cs_trigger,	OROOT,	LOG_DB, CMD_REG,	5},
  { NULL }
};

/*! \brief Provides the conversions between chanop levels and various names */
static struct {
   int lev;
   const char *v[3];
}
oplev_table[] = {
     { 0,	{"User",		"user",		"User"		}},
     { 1,	{"MAOP(1)",		"miniop(1)",	"MiniOp(1)"	}},
     { 2,	{"MAOP(2)",		"miniop-",	"MiniOp(2)"	}},
     { MAOP,{"MAOP",		"miniop",	"MiniOp"	}},
     { 4,	{"MAOP(4)",		"miniop(4)",	"MiniOp(4)"	}},
     { AOP,	{"AOP",			"aop",		"AutoOp"	}},
     { 6,	{"AOP(6)",		"aop(6)",	"AutoOp(6)"	}},
     { 7,	{"AOP(7)",		"aop(7)",	"AutoOp(7)"	}},
     { MSOP,{"MSOP",		"msop",		"MiniSop"	}},
     { 9,	{"MSOP(9)",		"msop(9)",	"MiniSop(9)"	}},
     { SOP,	{"SOP",			"sop",		"SuperOp"	}},
     { 11,	{"SOP(11)",		"sop(11)",	"SuperOp(11)"	}},
     { 12,	{"SOP(12)",		"sop(12)",	"SuperOp(12)"	}},
     { MFOUNDER, {"MFOUNDER","mfounder","MiniFounder"	}},
     { 14,	{"MFOUNDER(14)","mfounder(14)",	"MiniFounder(14)"}},
     { FOUNDER, {"FOUNDER",	"founder",	"Founder"	}},
     { 16,	{ "UNDEFINED",	"undefined",	"Undefined"	}},
};

// *INDENT-ON*

/*------------------------------------------------------------------------*/

/* Handle bad password attempts -Mysid (JH) */
/**
 * @brief Handle a bad password attempt
 * \param nick User who failed to enter password correctly
 * \param target Pointer to channel attempt was against
 * \return 1 if the user was killed, 0 if not
 * \pre   nick is a user on IRC, target points to a listed registered
 *        channel object.
 * \post  A bad password attempt by `nick' has been recorded against `target'.
 *        If 1 was returned, then `nick' may no longer be valid or even on IRC.
 */
int BadPwChan(UserList *nick, RegChanList *target) 
{
	if (nick == NULL || target == NULL)
		return 0;

	if (nick->badpws < UCHAR_MAX)
		nick->badpws++;
	if (target->badpws < UCHAR_MAX)
		target->badpws++;

	if ((nick->badpws > CPW_TH_SENDER_1 && target->badpws > CPW_TH_TARGET_1)
           || (nick->badpws > CPW_TH_SENDER_2 && target->badpws > CPW_TH_TARGET_2)
	   || (nick->badpws > CPW_TH_SENDER_3 && target->badpws > CPW_TH_TARGET_3))
	{
		sSend(":%s GLOBOPS :Possible password guess attempt %s (%u) -> %s (%u)",
			ChanServ, nick->nick, (u_int)nick->badpws,
			target->name, (u_int)target->badpws);
	}

	if (addFlood(nick, FLOODVAL_BADPW))
		return 1;
	return 0;
}

/**
 * @brief A user successfully identified to a channel, send them badpw
 *        information since their last `login'
 */
void GoodPwChan(UserList *nick, RegChanList *target)
{
	if (nick == NULL || target == NULL)
		return;

	if (target->badpws > 0) {
		sSend(":%s NOTICE %s :%s had %d failed password attempts since last identify.",
			ChanServ, nick->nick, target->name, target->badpws);
	}

	target->badpws = 0;
}

/*------------------------------------------------------------------------*/

void kick_for_akick(const char* kicker, const char* nick, 
	            RegChanList* chan, cAkickList* record)
{
	if (!(chan->flags & CAKICKREASONS) || record == NULL
	    || record->reason[0] == '\0')
	{
		sSend
			(":%s KICK %s %s :You have been permanently banned "
			 "from this channel.", ChanServ, chan->name, nick);
	}
	else
	{
		sSend
			(":%s KICK %s %s :Autokick set by a channel "
			 "operator: (%s)", ChanServ, 
			 chan->name, nick, record->reason);
	}
}

/*------------------------------------------------------------------------*/

/* The following bunch of functions all deal with channels and registered
 * ones, ignore them unless you need to fiddle with the linked lists for
 * channels. 
 */

/** 
 *  \brief Add an online channel to the hash table and linked list
 *  \param newchan Pointer to the online channel to be added
 *  \pre  Newchan is a pointer to an initialized online channel record
 *        that has not yet been listed.  The channel name member holds
 *         the name of the channel.
 *  \post The channel record ``newchan'' is added to the online channels
 *        table (has been listed).
 */
void addChan(ChanList * newchan)
{
	HashKeyVal hashEnt = getHashKey(newchan->name) % CHANHASHSIZE;

	if (getChanData(newchan->name)) {
		FREE(newchan);
		return;
	}

	if (firstChan == NULL) {
		firstChan = newchan;
		newchan->previous = NULL;
	} else {
		lastChan->next = newchan;
		newchan->previous = lastChan;
	}
	lastChan = newchan;
	newchan->next = NULL;

	if (!ChanHash[hashEnt].chan) {
		ChanHash[hashEnt].chan = newchan;
		newchan->hashprev = NULL;
	} else {
		ChanHash[hashEnt].lastchan->hashnext = newchan;
		newchan->hashprev = ChanHash[hashEnt].lastchan;
	}

	ChanHash[hashEnt].lastchan = newchan;
	newchan->hashnext = NULL;
}

/**
 * \brief Add a registered channel to the hash table and linked list
 * \param newchan Pointer to a channel database object to add into the
 *        list
 *  \pre  Newchan is a pointer to an initialized registered channel record
 *        that is not yet listed.  The channel name member holds the name
 *        of the channel.
 *  \post The record ``newchan'' is added to the registered channels
 *        table (the channel is listed).
 */
void addRegChan(RegChanList * newchan)
{
	HashKeyVal hashEnt = getHashKey(newchan->name) % CHANHASHSIZE;

	if (getRegChanData(newchan->name)) {
		freeRegChan(newchan);
		return;
	}

	if (!firstRegChan) {
		firstRegChan = newchan;
		newchan->previous = NULL;
	} else {
		lastRegChan->next = newchan;
		newchan->previous = lastRegChan;
	}
	lastRegChan = newchan;
	newchan->next = NULL;

	if (!RegChanHash[hashEnt].chan) {
		RegChanHash[hashEnt].chan = newchan;
		newchan->hashprev = NULL;
	} else {
		RegChanHash[hashEnt].lastchan->hashnext = newchan;
		newchan->hashprev = RegChanHash[hashEnt].lastchan;
	}

	RegChanHash[hashEnt].lastchan = newchan;
	newchan->hashnext = NULL;
}

/**
 * \brief Add an online user to an online channel's user hash and linked list
 * \param channel Online channel whom a user is to be added to
 * \param newnick A pointer to a channel-nick object to add to the hash
 *                table and list of the channel's members
 * \pre  Channel is a pointer to a valid online channel record, and
 *       newnick is a pointer to a freshly-allocated cNickList (or
 *       channel member entry) record that is not in a list but
 *       has been initialized to contain the appropriate nickname.
 * \post The ``newnick'' record is stored in the ``channel'' member
 *       list.
 * \warning Once a userlist record is listed, the nickname of the record
 *          should never be altered.
 */
void addChanUser(ChanList * channel, cNickList * newnick)
{
	HashKeyVal hashEnt;
	if (newnick == NULL) {
		return;
	}

	if (channel->firstUser == NULL) {
		channel->firstUser = newnick;
		newnick->previous = NULL;
	} else {
		channel->lastUser->next = newnick;
		newnick->previous = channel->lastUser;
	}

	channel->lastUser = newnick;
	newnick->next = NULL;

	hashEnt = getHashKey(newnick->person->nick) % CHANUSERHASHSIZE;

	if (channel->users[hashEnt].item == NULL) {
		channel->users[hashEnt].item = newnick;
		newnick->hashprev = NULL;
	} else {
		channel->users[hashEnt].lastitem->hashnext = newnick;
		newnick->hashprev = channel->users[hashEnt].lastitem;
	}

	channel->users[hashEnt].lastitem = newnick;
	newnick->hashnext = NULL;

}



/**
 * \brief Add a ban item to an online channel's banlist
 * \param channel Online channel that a ban object should be added to
 * \param item Ban object to be added to ChanServ's information
 *             on a channel's banlist.
 * \pre  Channel is a pointer to a valid online channel record, and
 *       item is a pointer to a freshly-allocated cBanList (or
 *       channel ban-list entry) record that has not yet been stored
 *       in a list.
 * \post The ``item'' record is added to the ``channel'' ban
 *       list.
 */
void addChanBan(ChanList * channel, cBanList * item)
{
	if (channel->firstBan == NULL) {
		channel->firstBan = item;
		item->previous = NULL;
	} else {
		channel->lastBan->next = item;
		item->previous = channel->lastBan;
	}

	channel->lastBan = item;
	item->next = NULL;
}


/**
 * \brief Add an autokick list item to a registered channel's autokick list
 * \param channel Channel database item that autokick object should be
 *        added to
 * \param item Autokick object to be added to a registered channel
 *        object
 * \pre  Channel is a pointer to a valid REGISTERED channel record, and
 *       item is a pointer to a freshly-allocated cAkickList (or
 *       ChanServ AutoKick) item that has not yet been stored
 *       in a list.
 * \post The ``item'' record is inserted in the ``channel autokick''
 *       linked list.
 */
void addChanAkick(RegChanList * channel, cAkickList * item)
{
	if (channel->firstAkick == NULL) {
		channel->firstAkick = item;
		item->previous = NULL;
	} else {
		channel->lastAkick->next = item;
		item->previous = channel->lastAkick;
	}

	channel->lastAkick = item;
	item->next = NULL;
	channel->akicks++;
	indexAkickItems(channel);
}

/**
 * \brief Add an 'access list' item to a registered channel's operator list
 * \param channel Registered channel object of item to be added
 * \param item Object to be added to the channel access list
 * \pre  Channel is a pointer to a valid REGISTERED channel record, and
 *       item is a pointer to a freshly-allocated cAccessList (or
 *       ChanServ operator) item that has not yet been stored
 *       in a list.
 * \post The ``item'' record is inserted in the ``channel operator''
 *       linked list but has been initialized to contain the
 *       appropriate nickname.
 * \warning Once an access list record is listed, the nickname of the
 *          record should never be altered.
 */
void addChanOp(RegChanList * channel, cAccessList * item)
{
	HashKeyVal hashEnt;

	if (channel == NULL || item == NULL)
		return;

	if (channel->firstOp == NULL) {
		channel->firstOp = item;
		item->previous = NULL;
	} else {
		channel->lastOp->next = item;
		item->previous = channel->lastOp;
	}

	channel->lastOp = item;
	item->next = NULL;
	hashEnt = ( item->nickId.getHashKey() % OPHASHSIZE );

	if (channel->op[hashEnt].item == NULL) {
		channel->op[hashEnt].item = item;
		item->hashprev = NULL;
	} else {
		channel->op[hashEnt].lastitem->hashnext = item;
		item->hashprev = channel->op[hashEnt].lastitem;
	}

	channel->op[hashEnt].lastitem = item;
	item->hashnext = NULL;
	channel->ops++;
	indexOpItems(channel);
}

/**
 * \brief remove a channel from the channel list, and close it up where
 *        necessary
 * \param killme Channel to remove from services' online channels
 *        list and destroy
 * \pre   Killme is a pointer to a listed online channel record.
 * \post  The channel is unlisted, removed from the table, and the
 *        memory area is freed.
 * \warning This does free killme.
 */
void delChan(ChanList * killme)
{
	HashKeyVal hashEnt;
	ChanList *tmpchan;

	if (killme == NULL)
		return;

	if (killme->previous)
		killme->previous->next = killme->next;
	else
		firstChan = killme->next;

	if (killme->next)
		killme->next->previous = killme->previous;
	else
		lastChan = killme->previous;

	hashEnt = getHashKey(killme->name) % CHANHASHSIZE;
	for (tmpchan = ChanHash[hashEnt].chan; tmpchan;
		 tmpchan = tmpchan->hashnext) {
		if (tmpchan == killme) {
			if (killme->hashprev)
				killme->hashprev->hashnext = killme->hashnext;
			else
				ChanHash[hashEnt].chan = killme->hashnext;

			if (killme->hashnext)
				killme->hashnext->hashprev = killme->hashprev;
			else
				ChanHash[hashEnt].lastchan = killme->hashprev;
		}
	}

	FREE(killme);
}

/**
 * \brief Delete a registered channel
 *
 * Remove a channel from the registered channel list, and close it up where necessary
 * \param killme Registered channel object to be deleted
 * \pre   Killme is a pointer to a listed registered channel record.
 * \post  The registration record is unlisted, removed from the table, and the
 *        memory area is freed.
 * \warning This procedure does not search for and update the 'reg' member
 *          of the online channel record to indicate that it is no longer
 *          registered.  The caller must do this.
 * \warning This does free killme.
 */
void delRegChan(RegChanList * killme)
{
	HashKeyVal hashEnt;
	RegChanList *tmpchan;
	RegNickIdMap *ptrMap;

	if (killme == NULL)
		return;

	if (killme->previous)
		killme->previous->next = killme->next;
	else
		firstRegChan = killme->next;

	if (killme->next)
		killme->next->previous = killme->previous;
	else
		lastRegChan = killme->previous;

	hashEnt = getHashKey(killme->name) % CHANHASHSIZE;
	for (tmpchan = RegChanHash[hashEnt].chan; tmpchan;
		 tmpchan = tmpchan->hashnext) {
		if (tmpchan == killme) {
			if (killme->hashprev)
				killme->hashprev->hashnext = killme->hashnext;
			else
				RegChanHash[hashEnt].chan = killme->hashnext;

			if (killme->hashnext)
				killme->hashnext->hashprev = killme->hashprev;
			else
				RegChanHash[hashEnt].lastchan = killme->hashprev;
		}
	}

	ptrMap = killme->founderId.getIdMap();

	if (ptrMap && ptrMap->nick) {
		ptrMap->nick->chans--;
	}

	freeRegChan(killme);
}

/**
 * \brief Frees a registered channel and its data
 * \param killme Registered channel object to release from memory
 * \pre Killme points to an otherwise valid registered channel record that
 *      has been allocated from the heap but is not found in the channel
 *      list(s) or table(s).
 * \post The memory area pointed by killme has been freed
 *       and all non-null dynamic members of the killme record have
 *       been individually freed
 */
void freeRegChan(RegChanList * killme)
{
	if (killme->id.nick)
		FREE(killme->id.nick);
	if (killme->url)
		FREE(killme->url);
	if (killme->autogreet)
		FREE(killme->autogreet);
	if (killme->markby)
		FREE(killme->markby);
	FREE(killme);
}


/**
 * Now delete the user from the channel, gee is this easy or what? no. :>
 * \param channel Channel to delete from
 * \param nick User to delete
 * \param doCleanChan Clean an empty channel or no?
 * \pre Channel points to a valid, listed online channel record, and Nick
 *      points to a valid cNickList record that can be found in the
 *      proper members hash table bucket of the channel record.
 * \post The nick record is removed from the table and the memory
 *       pointed to by nick is freed.  If no member structures
 *       remain, then the entire channel is unlisted and freed if
 *       doCleanChan is true.
 */
void delChanUser(ChanList * channel, cNickList * nick, int doCleanChan)
{
	HashKeyVal hashEnt, i = 0;
	cNickList *tmpnick;

	if (nick == NULL || channel == NULL)
		return;

	if (nick->previous != NULL)
		nick->previous->next = nick->next;
	else
		channel->firstUser = nick->next;

	if (nick->next != NULL)
		nick->next->previous = nick->previous;
	else
		channel->lastUser = nick->previous;

	hashEnt = getHashKey(nick->person->nick) % CHANUSERHASHSIZE;

	for (tmpnick = channel->users[hashEnt].item; tmpnick;
		 tmpnick = tmpnick->hashnext) {
		if (tmpnick == nick) {
			if (nick->hashprev != NULL)
				nick->hashprev->hashnext = nick->hashnext;
			else
				channel->users[hashEnt].item = nick->hashnext;

			if (nick->hashnext != NULL)
				nick->hashnext->hashprev = nick->hashprev;
			else
				channel->users[hashEnt].lastitem = nick->hashprev;
		}
	}

	/* delChanUser() ought to take care of this instead of redundancy */
	if (nick->person)
		for (i = 0; i < NICKCHANHASHSIZE; i++)
			if (nick->person->chan[i] == channel)
				nick->person->chan[i] = NULL;
	FREE(nick);

	/*
	 * if there are no more users on the channel, get it out of
	 * out memory
	 */
	if (doCleanChan && channel->firstUser == NULL)
		delChan(channel);
}

/**
 * Remove a channel ban item
 * \pre Channel points to a valid, listed channel record.
 *      Item points to a valid cBanList channel ban list record
 *      that can be found in the ban list of channel.
 * \post Item is removed from the channel banlist and the
 *       memory area pointed to by item is freed.
 */
void delChanBan(ChanList * channel, cBanList * item)
{
	if (item->previous)
		item->previous->next = item->next;
	else
		channel->firstBan = item->next;

	if (item->next)
		item->next->previous = item->previous;
	else
		channel->lastBan = item->previous;

	FREE(item);
}


/**
 * Remove channel akick 
 * \pre Channel points to a valid, listed registered channel record.
 *      Item points to a valid cAkickList record that can be found
 *      in the autokick list of channel.
 * \post Item is removed from the channel autokick list and the
 *       memory area pointed to by item is freed.
 */
void delChanAkick(RegChanList * channel, cAkickList * item)
{
	if (item->previous)
		item->previous->next = item->next;
	else
		channel->firstAkick = item->next;

	if (item->next)
		item->next->previous = item->previous;
	else
		channel->lastAkick = item->previous;

	FREE(item);
	channel->akicks--;
	indexAkickItems(channel);
}

/**
 * Remove channel operator
 * \param channel Channel item to remove an operator from
 * \param item Access list entry to remove
 * \pre Channel points to a valid, listed registered channel record.
 *      Item points to a valid cAccessList record that can be found
 *      in the channel operator list of channel.
 * \post Item is removed from the channel operator list and the
 *       memory area pointed to by item is freed.
 */
void delChanOp(RegChanList * channel, cAccessList * item)
{
	HashKeyVal hashEnt;
	cAccessList *tmpnick;

	if (item->previous)
		item->previous->next = item->next;
	else
		channel->firstOp = item->next;

	if (item->next)
		item->next->previous = item->previous;
	else
		channel->lastOp = item->previous;

	hashEnt = item->nickId.getHashKey() % OPHASHSIZE;
	for (tmpnick = channel->op[hashEnt].item; tmpnick;
		 tmpnick = tmpnick->hashnext) {
		if (tmpnick == item) {
			if (item->hashprev)
				item->hashprev->hashnext = item->hashnext;
			else
				channel->op[hashEnt].item = item->hashnext;

			if (item->hashnext)
				item->hashnext->hashprev = item->hashprev;
			else
				channel->op[hashEnt].lastitem = item->hashprev;
		}
	}

	FREE(item);
	channel->ops--;
	indexOpItems(channel);
}

/**
 * \brief Find a channel in the list of online channels
 * \param Name of channel to find
 * \pre    Name points to a valid NUL-terminated character array
 *         that is also a valid channel name.
 * \return Null pointer if no online channel by that name has been listed.
 *         #ChanList object if a channel by that name is listed.
 */
ChanList *getChanData(char *name)
{
	HashKeyVal hashEnt;
	if (!name)
		return NULL;

	hashEnt = getHashKey(name) % CHANHASHSIZE;

	if (ChanHash[hashEnt].chan == NULL)
		return NULL;
	else {
		ChanList *tmpchan;

		for (tmpchan = ChanHash[hashEnt].chan; tmpchan;
			 tmpchan = tmpchan->hashnext) {
			if (!strcasecmp(tmpchan->name, name))
				return tmpchan;
		}

		return NULL;
	}
}

/**
 * \brief Find a registered channel in the list
 * \param Name of registered channel to find
 * \pre    Name points to a valid NUL-terminated character array
 *         that is also a valid channel name.
 * \return A pointer to the #RegChanList object for a registered
 *         channel record if a registered channel structure 
 *         by that name has been listed.  Otherwise a null pointer.
 */
RegChanList *getRegChanData(char *name)
{
	HashKeyVal hashEnt;
	if (name == NULL)
		return NULL;

	hashEnt = getHashKey(name) % CHANHASHSIZE;

	if (RegChanHash[hashEnt].chan == NULL)
		return NULL;
	else {
		RegChanList *tmpchan;
		for (tmpchan = RegChanHash[hashEnt].chan; tmpchan;
			 tmpchan = tmpchan->hashnext) {
			if (!strcasecmp(tmpchan->name, name))
				return tmpchan;
		}
		return NULL;
	}
}

/**
 * \brief Get the encryption status of a registered channel object
 *
 * \return $ for MD5 encrypted, @ for plaintext
 */
char ChanGetEnc(RegChanList *rcl)
{
        if (rcl)
                return ((rcl->flags & CENCRYPT) ? '$' : '@');
        return '@';
}

/**
 * \brief Get the IDENT flag status of a registered channel object
 *
 * \return 1 for IDENT enabled, 0 for disabled
 */
char ChanGetIdent(RegChanList *rcl)
{
	if (rcl)
		return ((rcl->flags & CIDENT) ? 1 : 0);
	return 0;
}

/**
 * \brief Find a user channel record in an online channel
 * \param chan Pointer to an online channel item
 * \param data Pointer to an online user item
 * \pre    Chan points to a valid, listed online channel record.
 *         Data points to a valid, listed online user record.
 * \return A null pointer if no channel user record is found for that
 *         nickname on the channel.  Else a #cNickList record, the
 *         membership object for the requested channel for the requested
 *         user.
 */
cNickList *getChanUserData(ChanList * chan, UserList * data)
{
	HashKeyVal hashEnt;
	cNickList *tmpnick;

	if (data == NULL || chan == NULL)
		return NULL;

	hashEnt = getHashKey(data->nick) % CHANUSERHASHSIZE;

	if (chan->users[hashEnt].item == NULL)
		return NULL;

	for (tmpnick = chan->users[hashEnt].item; tmpnick != NULL;
		 tmpnick = tmpnick->hashnext) if (tmpnick->person != NULL
										  && tmpnick->person == data)
			return tmpnick;

	return NULL;
}

/** \def process_op_item
 * \brief Process an access check on a chanop item
 *
 * XXX: Macro abuse
 * This macro is used by getMiscChanop to process access
 * items in a channel.
 */
#define process_op_item(x) { \
	if (x) { \
		if (checkAccessNick) \
		{ \
			strncpyzt(checkAccessNick, tmpNickName, NICKLEN); \
		} \
		if (highest < (x)->uflags) { \
			highest = (x)->uflags; \
		} \
	} \
}

/**
 * \brief Get the channel access level
 * \param chan Pointer to a registered channel item
 * \param nick Nickname of user to check access of
 * \param #TRUE if user must be identified to have access
 * \param checkAccessNick buffer to store information about 'why'
 *        a user has access to the channel
 * \pre   Chan points to a valid, listed registered channel record.
 *        Nick is a null pointer or points to a valid NUL-terminated
 *        character array.  Id has a value of 0 or zero, and
 *        checkAccessNick is a null pointer or points to a freshly-
 *        allocated memory area of size NICKLEN or greater.
 * \post  If checkAccessNick was not a null pointer and the highest level
 *        of channel access was found through a remote nickname, then that
 *        nickname is copied into the checkAccessNick memory area.
 * \return The access level of the specified nickname to a registered
 *         channel, 0 means no access, -1 means AutoKicked, any other
 *         value is that of their present chanop access.
 */
int getMiscChanOp(RegChanList * chan, char *nick, int id,
                  char *checkAccessNick, cAkickList** akickRecord)
{
	int highest = 0;
	UserList *tmp;
	cAccessList *tmpnick;
	cAkickList* matchingAk = 0;
	const char *tmpNickName;

	if (!nick || !chan)
		return 0;

	if (akickRecord)
		*akickRecord = 0;

	tmp = getNickData(nick);

    /* Channel record but not open registration? No access. */
	if (!tmp || ((chan->flags & (CBANISH | CCLOSE)) && !isOper(tmp)))
		return 0;

	/* First check for founder or chan id */
	if (isFounder(chan, tmp)) {
		if (checkAccessNick)
			strncpyzt(checkAccessNick, "founder(PW)", NICKLEN);
		return FOUNDER;
	}

	if (id) {
		/* If we have to be identified, find an exact match only */
		UserList *check = getNickData(nick);
		RegNickList *reg2;
		if (check && check->reg) {
			if (isIdentified(check, check->reg)) {
				tmpnick = getChanOpData(chan, nick);

				if (tmpnick) {
					tmpNickName = tmpnick->nickId.getNick();
					if (tmpNickName)
						process_op_item(tmpnick);
				}
			}
		}
		if (check && check->id.nick
			&& (reg2 = getRegNickData(check->id.nick))) {
			if (isIdentified(check, reg2)) {
				tmpnick = getChanOpData(chan, reg2->nick);
				if (tmpnick) {
					tmpNickName = (tmpnick->nickId.getNick());
					if (tmpNickName && (tmpnick->uflags > highest))
						process_op_item(tmpnick);
				}
			}
		}
	} else {
		/* Otherwise, search for masks to remote nicks too */
		RegNickList *reg2 =
			tmp->id.nick ? getRegNickData(tmp->id.nick) : NULL;

		if (reg2 && isIdentified(tmp, reg2)
			&& (tmpnick = getChanOpData(chan, reg2->nick))
			&& (tmpNickName = ((tmpnick->nickId).getNick())))
			process_op_item(tmpnick);
		if (isRecognized(tmp, tmp->reg)
			&& (tmpnick = getChanOpData(chan, nick))
			&& (tmpNickName = ((tmpnick->nickId).getNick()))) 
		{
			process_op_item(tmpnick);
		} else {
			for (tmpnick = chan->firstOp; tmpnick; tmpnick = tmpnick->next) {
				if (!(tmpNickName = (tmpnick->nickId).getNick()))
					continue;
				if (checkAccess
					(tmp->user, tmp->host,
					 getRegNickData(tmpNickName)) == 1)
					process_op_item(tmpnick);
			}
		}
	}


	/* Iff the don't have op access, look for an akick */
	if (!highest) {
		cAkickList *tmpak;
		char hostmask[HOSTLEN + USERLEN + NICKLEN];
		char hostmask2[HOSTLEN + USERLEN + NICKLEN];

		snprintf(hostmask, HOSTLEN + USERLEN + NICKLEN, "%s!%s@%s",
				 tmp->nick, tmp->user, tmp->host);
		snprintf(hostmask2, HOSTLEN + USERLEN + NICKLEN, "%s!%s@%s",
				 tmp->nick, tmp->user, genHostMask(tmp->host));

		for (tmpak = chan->firstAkick; tmpak; tmpak = tmpak->next) {
			if (!match(tmpak->mask, hostmask)
				|| !match(tmpak->mask, hostmask2)) {
				highest = -1;
				matchingAk = tmpak;
				
				break;
			}
		}
	}

	/* invasive edit removed -- better method in use */

    /* If forced xfer then no access other than oper using DMOD/Override */
	if ((chan->flags & CFORCEXFER) && !opFlagged(tmp, OVERRIDE))
		return 0;

	if (highest == -1 && matchingAk && akickRecord) {
		*akickRecord = matchingAk;
	}

	return highest;
#undef process_op_item
}

/**
 * \brief Get a user's chanop access to a channel
 * \pre    Chan is a null pointer or a pointer to a valid, listed registered
 *         channel record.  Nick is a null pointer or a pointer to a valid
 *         character array.
 * \return The access level of the specified nickname to the channel.
 * \par See also: getMiscChanOp()
 */
int getChanOp(RegChanList * chan, char *nick)
{
	/* This is now just a wrapper to the getMiscChanop function */
	if (!chan || !nick)
		return 0;

	return getMiscChanOp(chan, nick, (chan->flags & CIDENT) == CIDENT,
						 NULL, NULL);
}

/**
 * \brief Get a user's chanop access to a channel requiring that they be identified
 * to have any access
 * \pre    Chan is a null pointer or a pointer to a valid, listed registered
 *         channel record.  Nick is a null pointer or a pointer to a valid
 *         character array.
 * \return The access level of the specified nickname to the channel.
 *         Identification to an op list item is required for chanop access.
 * \par See also: getMiscChanop()
 */
int getChanOpId(RegChanList * chan, char *nick)
{
	/* Get Chanop level, but require identification */
	if (!chan || !nick)
		return 0;
	return getMiscChanOp(chan, nick, 1, NULL, NULL);
}

/**
 * \brief Get the chanop data structure of a nick that might be in the
 * access list.
 *
 * \param Pointer to online channel
 * \param Name of nickname to find in oplist
 * \pre    Chan is a null pointer or a pointer to a valid, listed registered
 *         channel record.  Nick is a null pointer or a pointer to a valid
 *         character array.
 * \return A null pointer if the chanop cannot be found, otherwise a pointer
 *         to the chanop access object (#cAccessList) is returned.
 */
cAccessList *getChanOpData(const RegChanList * chan, const char *nick)
{
	RegNickList *rNickPtr;
	HashKeyVal hashEnt;

	if (nick == NULL || chan == NULL)
		return NULL;

	rNickPtr = getRegNickData(nick);

	if (rNickPtr == NULL)
		return NULL;

	hashEnt = rNickPtr->regnum.getHashKey() % OPHASHSIZE;

	if (chan->op[hashEnt].item == NULL)
		return NULL;
	else {
		cAccessList *tmpnick;

		for (tmpnick = chan->op[hashEnt].item; tmpnick;
			 tmpnick = tmpnick->hashnext) {
			if (tmpnick->nickId == rNickPtr->regnum)
				return tmpnick;
		}
		return NULL;
	}
}

/**
 * \brief Search for a certain ban in a channel (What's this for?)
 * \param chan Pointer to an online channel item to look in
 * \param ban Ban to search for
 * \return NULL if the ban could not be found, or a pointer to 
 *         the appropriate #cBanList structure if it is found.
 */
cBanList *getChanBan(ChanList * chan, char *ban)
{
	cBanList *tmp;

	if (chan == NULL || ban == NULL)
		return NULL;

	for (tmp = chan->firstBan; tmp; tmp = tmp->next) {
		if (!strcasecmp(ban, tmp->ban))
			return tmp;
	}

	return NULL;
}

/**
 * \brief Search for a specific akick in a channel
 * \param chan Pointer to online channel item
 * \param akick AutoKick string to look for
 * \return Null pointer if the specified autokick could not be found,
 *         otherwise a pointer to the relevant #cAkickList object.
 */
cAkickList *getChanAkick(RegChanList * chan, char *akick)
{
	cAkickList *tmp;

	if (chan == NULL || akick == NULL)
		return NULL;

	for (tmp = chan->firstAkick; tmp; tmp = tmp->next) {
		if (!strcasecmp(akick, tmp->mask))
			return tmp;
	}

	return NULL;
}

/**
 * \brief Re-Index the akick list
 * \param chan Registered channel to re-index the autokick list of.
 * \pre Chan is a pointer to a valid, listed registered channel record.
 * \post Autokick items for the channel are renumbered (indexed) starting 
 *       at 1.
 */
void indexAkickItems(RegChanList * chan)
{
	cAkickList *tmp;
	int i = 1;
	for (tmp = chan->firstAkick; tmp; tmp = tmp->next) {
		tmp->index = i;
		i++;
	}
}

/**
 * \brief Re-Index the oplist
 * \param Registered channel to re-index chanop list of
 * \pre Chan is a pointer to a valid, listed registered channel record.
 * \post Channel operator items for the channel are renumbered (indexed)
 *       starting at 1.
 */
void indexOpItems(RegChanList * chan)
{
	cAccessList *tmp;
	int i = 1;
	for (tmp = chan->firstOp; tmp; tmp = tmp->next) {
		tmp->index = i;
		i++;
	}
}

/**
 * \brief Clear channel identification structures (un-identify)
 * \param Registered channel to clear identification data for
 * \pre Chan is a pointer to a valid, listed registered channel record.
 * \post Any identification to the channel by password is cleared.
 */
void clearChanIdent(RegChanList * chan)
{
	if (!chan)
		return;
	if (chan->id.nick)
		FREE(chan->id.nick);
	chan->id.nick = NULL;
	chan->id.idnum = RegId(0, 0);
	chan->id.timestamp = 0;
}

/**
 * \brief Determine if a user is the founder of a channel
 *
 * Is this 'nick' person the founder of the channel or no?
 * \pre Chan is a pointer to a valid, listed registered channel record.
 * \param chan Channel to check
 * \param nick Nick to check founder access of
 * \return 1 if yes, 0 if not
 */
int isFounder(RegChanList * chan, UserList * nick)
{
	UserList *tmp;

	if ((chan->flags & CFORCEXFER) && !opFlagged(nick, OVERRIDE))
		return 0;

	if (chan->id.nick && (tmp = getNickData(chan->id.nick))) {
		if (tmp->timestamp < chan->id.timestamp
			&& tmp->idnum == chan->id.idnum
			&& !strcasecmp(nick->nick, chan->id.nick)) return 1;
		else if (!strcasecmp(nick->nick, chan->id.nick))
			clearChanIdent(chan);
	} else if (chan->id.nick)
		clearChanIdent(chan);

	if (nick->reg && (chan->founderId == nick->reg->regnum)
		&& isRecognized(nick, nick->reg) && chan->facc)
		return 1;
	else
		return 0;
}

/**
 * \pre Chan is a pointer to a valid, NUL-terminated character array.
 * \brief Reports whether/not this an official SorceryNet channel like #sorcery
 * \return #TRUE if that is the case, #FALSE if not
 */
int is_sn_chan(char *ch_name)
{
	if (!ch_name || !*ch_name)
		return 0;

	/** bug XXX Use a named constant, not a string constant! */
	if (!strcasecmp(ch_name, PLUSLCHAN) || !strcasecmp(ch_name, LOGCHAN))
		return 1;
	return 0;
}

/**
 * \brief Initialize (clean) a registered channel object
 * \pre Chan is a pointer to a freshly-allocated registered channel
 *      structure that has not yet been listed and has not been used.
 * \brief Init data on a registered channel (clean hashes and pointers)
 * \param chan Registered channel object to be initialized
 */
void initRegChanData(RegChanList * chan)
{
	long i;

	chan->memolevel = 3;
	for (i = 0; i < OPHASHSIZE; i++) {
		chan->op[i].item = NULL;
		chan->op[i].lastitem = NULL;
	}
	chan->url = NULL;
}

/**
 * \pre  Chan is a pointer to a valid, online channel.
 *       Format points to a valid NUL-terminated character array,
 *       and parameters beyond it follow rules of the format string.
 * \post If the channel is not registered or the QUIET option is not
 *       set, then the given message has been sent to all channel
 *       operators.
 * \param chan Pointer to channel item
 * \param format Format string for message
 * \param ... Format arguments
 */
void sendToChanOps(ChanList * chan, char *format, ...)
{
	char buffer[IRCBUF];
	va_list stuff;

	if (!chan || (chan->reg && chan->reg->flags & CQUIET))
		return;

	va_start(stuff, format);
	vsnprintf(buffer, sizeof(buffer), format, stuff);
	va_end(stuff);

	sSend(":%s NOTICE @%s :(Ops:%s) %s", ChanServ, chan->name, chan->name,
		  buffer);
}


/** \todo make these two one function. 
 * Send a message to channel operators, ignore QUIET
 * \param chan Pointer to channel item
 * \param format Format string for message
 * \param ... Format arguments
 * \pre  Chan is a pointer to a valid, online channel.
 *       Format points to a valid NUL-terminated character array,
 *       and parameters beyond it follow rules of the format string.
 * \post An IRC message has been sent for all operators of the given
 *       channel.
 */
void sendToChanOpsAlways(ChanList * chan, char *format, ...)
{
	char buffer[IRCBUF];
	va_list stuff;

	va_start(stuff, format);
	vsnprintf(buffer, sizeof(buffer), format, stuff);
	va_end(stuff);

	sSend(":%s NOTICE @%s :(Ops:%s) %s", ChanServ, chan->name, chan->name,
		  buffer);
}

/**
 * \brief Kick and ban a user from a channel
 * 
 * Kicks a user off the specified channel and sets a ban.
 * \param chan Pointer to a channel item
 * \param nick Pointer to online nickname (target item)
 * \param format Kick message format string
 * \param ... Kick message format argument list
 * \pre  Chan points to a listed, valid online channel record.
 *       Nick points to a listed, valid online user record.
 *       Format points to a valid NUL-terminated character array,
 *       and format and the following parameters form a valid
 *       format string and set of arguments for that string.
 * \post An IRC message has been sent to kick and ban the specified
 *       online user.  This procedure does not perform the internal
 *       updates necessary to note a change.
 *
 * \warning This function can cause a channel to be deleted,
 *          'chan' should be treated as no longer valid after
 *          a call to this function -- since it is void, there
 *          is no way to test if the pointer value is still valid
 *          short of a lookup by name in the Channel hash table.
 */
void banKick(ChanList * chan, UserList * nick, char *format, ...)
{
	char user[USERLEN], host[HOSTLEN], theirmask[USERLEN + HOSTLEN + 3];
	char buffer[256];
	va_list stuff;
	cBanList *tmpban;
	cNickList *channick;

	strncpyzt(user, nick->user, USERLEN);
	strncpyzt(host, nick->host, HOSTLEN);

	mask(user, host, 1, theirmask);

	sSend(":%s MODE %s +b %s", ChanServ, chan->name, theirmask);
	tmpban = (cBanList *) oalloc(sizeof(cBanList));
	strncpyzt(tmpban->ban, theirmask, sizeof(tmpban->ban));
	addChanBan(chan, tmpban);
#ifdef CDEBUG
	sSend(":%s PRIVMSG " DEBUGCHAN " :Added ban %s on %s", ChanServ,
		  theirmask, chan->name);
#endif
	va_start(stuff, format);
	vsnprintf(buffer, sizeof(buffer), format, stuff);
	va_end(stuff);

	channick = getChanUserData(chan, nick);

	sSend(":%s KICK %s %s :%s", ChanServ, chan->name, nick->nick, buffer);
	delChanUser(chan, channick, 1);
}

/**
 * \brief Generate initial mode string
 *
 * Returns a pointer to a static buffer holding a copy of the the initial
 * mode string necessary to restore channel modes.
 *
 * \param chan Pointer to channel list item
 * \return A string for initializing a channel's modes
 */
char *initModeStr(ChanList * chan)
{
	char mode[IRCBUF];
	char parastr[IRCBUF] = "";
	static char mstr[IRCBUF];
	int l;


	mstr[0] = '\0';
	if (!chan || !chan->reg)
		return mstr;
	*parastr = '\0';

	makeModeLockStr(chan->reg, mode);
	if (mode[0] != 0) {
		if ((chan->reg->mlock & PM_L)) {
			l = strlen(parastr);
			snprintf(parastr + l, IRCBUF - l, " %ld", chan->reg->limit);
		}

		if (((chan->reg->mlock & PM_K) || (chan->reg->mlock & MM_K)) &&
                     ((l = strlen(parastr)) < IRCBUF)) 
		{
			snprintf(parastr + l, IRCBUF - l, " %s", chan->reg->key
					&& *chan->reg->key ? chan->reg->key : "*");
		}
	}
	if (*parastr)
		snprintf(mstr, IRCBUF, "%s%s", mode, parastr);
	else
		snprintf(mstr, IRCBUF, "%s", mode);
	return mstr;
}

/**
 * \brief Have ChanServ join a channel
 * \param chan Name of channel to join
 * \pre Chan points to a valid NUL-terminated character array.
 * \post An IRC message has been sent to join chanserv into the
 *       channel, and the registered channel record has been flagged
 *       as 'ghosted' if the channel is registered.
 */
void createGhostChannel(char *chan)
{
	RegChanList *channel;

	if ((channel = getRegChanData(chan)))
		channel->flags |= CCSJOIN;

	sSend(":%s JOIN %s", ChanServ, chan);
	sSend(":%s MODE %s +i 1", ChanServ, chan);
}

/**
 * \brief Remove ChanServ from an enforced channel to end the ghost period
 * \param chan Name of channel to leave
 * \pre Chan points to a valid NUL-terminated character array that
 *      contains the name of a known ghost channel.
 * \post An IRC message has been sent to PART chanserv from the
 *       channel, and the registered channel record 'ghosted' flag
 *       is removed if the channel is registered.
 */
void deleteGhostChannel(char *chan)
{
	RegChanList *channel;

	if ((channel = getRegChanData(chan)))
		channel->flags &= ~CCSJOIN;

	sSend(":%s PART %s :Ahhh, all finished here!", ChanServ, chan);
}

/**
 * \brief Timed ending of a Channel enforcement period.
 * \param chan Name of enforced channel to expire
 * \warning Function frees the 'chan' pointer
 */
void deleteTimedGhostChannel(char *chan)
{
	deleteGhostChannel(chan);
	FREE(chan);
}


/**
 * \brief Generate mode-lock string
 *
 * Fills the modelock buffer with a copy of the locked modes string
 *
 * \param chan Pointer to channel list item
 * \param modelock Buffer of at least 80 characters to hold the lock buffer
 * \pre   Chan must point to a valid, listed registered channel.
 *        Mode must point to a character memory area of at least 20 characters
 *        in length.
 * \post The memory area pointed to by modelock has received a copy
 *       of the modelock enforcement string that can be used with an
 *       IRC message to enforce channel modes.
 * \warning DON'T add modes to this procedure without first checking
 *          that all callers have a large enough buffer.
 *          Buffer is assumed 20 units long.
 */
void makeModeLockStr(RegChanList * chan, char *modelock)
{
	char *p = modelock;
	bzero(modelock, 20);

	*p++ = '+';
	if (chan->mlock & PM_I)
		*p++ = 'i';
	if (chan->mlock & PM_L)
		*p++ = 'l';
	if (chan->mlock & PM_K)
		*p++ = 'k';
	if (chan->mlock & PM_M)
		*p++ = 'm';
	if (chan->mlock & PM_N)
		*p++ = 'n';
	if (chan->mlock & PM_P)
		*p++ = 'p';
	if (chan->mlock & PM_S)
		*p++ = 's';
	if (chan->mlock & PM_T)
		*p++ = 't';
	if (chan->mlock & PM_H)
		*p++ = 'H';
	if (chan->mlock & PM_C)
		*p++ = 'c';

	if (*(p - 1) == '+')		/* If the previous char is '+' */
		p = modelock;			/* Then erase it */

	if (chan->mlock >= 257) {	/* If there is a minus mlock */
		*p++ = '-';

		if (chan->mlock & MM_I)
			*p++ = 'i';
		if (chan->mlock & MM_L)
			*p++ = 'l';
		if (chan->mlock & MM_K)
			*p++ = 'k';
		if (chan->mlock & MM_M)
			*p++ = 'm';
		if (chan->mlock & MM_N)
			*p++ = 'n';
		if (chan->mlock & MM_P)
			*p++ = 'p';
		if (chan->mlock & MM_S)
			*p++ = 's';
		if (chan->mlock & MM_T)
			*p++ = 't';
		if (chan->mlock & MM_H)
			*p++ = 'H';
		if (chan->mlock & MM_C)
			*p++ = 'c';
	}

	if (p == modelock)
		strcpy(modelock, "(none)");
	else
		*p++ = 0;
}

/**
 * \brief Add user to a channel
 *
 * Adds a user to the specified channel after a JOIN
 *
 * \param nick Pointer to an online user item
 * \param channel Name of a channel
 * \warning MAXCHANSPERUSER as used by ircd must be 
 *          match the value of NICKCHANHASH or desyncs
 *          will occur.
 */
void addUserToChan(UserList * nick, char *channel)
{
	char chan[CHANLEN], *badTemp = NULL;
	int a = 0;
	ChanList *tmp;
	cNickList *person;

	dlogEntry("addUserToChan(%s, %s)", nick, channel);

	if (nick == NULL || channel == NULL || channel[0] == '\0')
		return;

	if (*channel == ':')
		channel++;

	while (*channel) {
		bzero(chan, CHANLEN);
		a = 0;
		badTemp = NULL;

		while (*channel != ',' && *channel != 0) {
			if (a < (CHANLEN - 1))
				chan[a] = *channel;
			else if (!badTemp)
				badTemp = channel;
			a++;
			channel++;
		}

		if (badTemp && a >= CHANLEN) {
			chan[CHANLEN - 1] = '\0';
			sSend(":%s GLOBOPS :%s Joining channel > CHANLEN (%s%s)", ChanServ, nick->nick, chan, badTemp ? badTemp : "");
			continue;
		}

		chan[a] = 0;

		if (*channel)
			channel++;

		if (chan[0] == '0')
			remFromAllChans(nick);

		else if (chan[0] != '+') {

			tmp = getChanData(chan);
			if (tmp == NULL)
				tmp = (ChanList *) oalloc(sizeof(ChanList));

			if (tmp->firstUser == NULL) {
				strncpyzt(tmp->name, chan, CHANLEN);
				person = (cNickList *) oalloc(sizeof(cNickList));
				person->person = nick;
				person->op = 0;

				addChan(tmp);
				addChanUser(tmp, person);

				/** \bug this depends on maxchans */
				for (a = 0; a < NICKCHANHASHSIZE; a++) {
					if (!nick->chan[a]) {
						nick->chan[a] = tmp;
						break;
					}
				}
#ifdef CDEBUG
				sSend(":%s PRIVMSG " DEBUGCHAN
					  " :Created channel %s, added %s to it", ChanServ,
					  chan, nick->nick);
#endif
				tmp->reg = getRegChanData(chan);

				if (tmp->reg) {
					char mode[20];
					char themask[NICKLEN + USERLEN + HOSTLEN + 3];
					cAkickList* akickRecord = 0;

					a = getMiscChanOp(tmp->reg, nick->nick,
					        (tmp->reg->flags & CIDENT) == CIDENT,						
						NULL, &akickRecord);

					if (a == -1) {
						cAkickList *blah;
						char userhost[NICKLEN + USERLEN + HOSTLEN + 3];
						char userhostM[NICKLEN + USERLEN + HOSTLEN + 3];

						snprintf(userhost, sizeof(userhost),
								 "%s!%s@%s", nick->nick, nick->user,
								 nick->host);
						snprintf(userhostM, sizeof(userhostM),
								 "%s!%s@%s", nick->nick, nick->user,
								 genHostMask(nick->host));

						for (blah = tmp->reg->firstAkick; blah;
							 blah = blah->next) {
							strncpyzt(themask, blah->mask,
									  sizeof(themask));
							if (!match(blah->mask, userhost)
								|| !match(blah->mask, userhostM)) {
								cBanList *newban =
									(cBanList *) oalloc(sizeof(cBanList));

								sSend(":%s MODE %s +b %s", ChanServ,
									  tmp->reg->name, themask);
								strncpyzt(newban->ban, themask, sizeof(newban->ban));
								addChanBan(tmp, newban);
								break;
							}
						}

						createGhostChannel(tmp->name);
						timer(10, deleteTimedGhostChannel,
							  strdup(tmp->name));


						kick_for_akick(ChanServ,
								nick->nick,
								tmp->reg,
								akickRecord);
						delChanUser(tmp, person, 1);
						return;
					}

					if ((tmp->reg->flags & (CBANISH | CCLOSE))
						&& !isOper(nick)) {
						sSend(":%s MODE %s +isnt-o %s", ChanServ,
							  tmp->name, nick->nick);
						createGhostChannel(tmp->name);
						timer(30, deleteTimedGhostChannel,
							  strdup(tmp->name));
						banKick(tmp, nick,
								"this channel is closed/banished");
						return;
					}
					if (a < tmp->reg->restrictlevel && (a < MFOUNDER)) {
						createGhostChannel(tmp->name);
						timer(30, deleteTimedGhostChannel,
							  strdup(tmp->name));
						banKick(tmp, nick,
								"this channel is restricted to level %i and above",
								tmp->reg->restrictlevel);
						return;
					}
					if (!a) {
						if (tmp->reg->mlock & PM_I) {
							createGhostChannel(tmp->name);
							timer(30, deleteTimedGhostChannel,
								  strdup(tmp->name));
							banKick(tmp, nick,
									"you must be invited to join this channel.");
							return;
						}
						if ((tmp->reg->mlock & PM_K)
							&& !(tmp->reg->flags & CCSJOIN)) {
							char *p;
							createGhostChannel(tmp->name);
							timer(30, deleteTimedGhostChannel,
								  strdup(tmp->name));
							sSend(":%s MODE %s +i-o %s", ChanServ,
								  tmp->name, nick->nick);
							sSend(":%s KICK %s %s :%s", ChanServ,
								  tmp->name, nick->nick,
								  "this is a keyed channel.");
							sSend(":%s MODE %s -i", ChanServ, tmp->name);
							if ((p = initModeStr(tmp))) {
								sSend(":%s MODE %s %s", ChanServ,
									  tmp->name, p);
#ifdef IRCD_MLOCK
								sSend(":%s MLOCK %s %s", ChanServ,
									  tmp->name, p);
#endif
							}
							remUserFromChan(nick, tmp->name);
							return;
						}
					}
					if ((a < AOP) && (a >= MAOP)) {
						sSend(":%s MODE %s +v %s", ChanServ, tmp->name,
							  nick->nick);
						person->op |= CHANVOICE;
						tmp->reg->timestamp = CTime;
						if (tmp && tmp->name
							&& !strcasecmp(tmp->name, HELPOPS_CHAN)
							&& (getChanOp(tmp->reg, nick->nick) >= 4)) {
							sSend(":ChanServ MODE %s :+h", nick->nick);
							nick->oflags |= NISHELPOP;
						}
					} else if (a && a >= AOP) {
						sSend(":%s MODE %s +o %s", ChanServ, tmp->name,
							  nick->nick);
						person->op |= CHANOP;
						tmp->reg->timestamp = CTime;
						if (tmp && tmp->name
							&& !strcasecmp(tmp->name, HELPOPS_CHAN)) {
							sSend(":ChanServ MODE %s :+h", nick->nick);
							nick->oflags |= NISHELPOP;
						}
					}
					if (tmp->reg->flags & CKTOPIC)
						if (tmp->reg->topic)
							sSend(":%s TOPIC %s %s %lu :%s", ChanServ,
								  tmp->name, tmp->reg->tsetby,
								  tmp->reg->ttimestamp, tmp->reg->topic);
					if (tmp->reg->autogreet)
						sSend(":%s NOTICE %s :[%s] %s", ChanServ,
							  nick->nick, tmp->name, tmp->reg->autogreet);

					makeModeLockStr(tmp->reg, mode);

					if (mode[0] != 0) {
						char parastr[IRCBUF * 2];

						*parastr = '\0';
						if ((tmp->reg->mlock & PM_L))
							sprintf(parastr + strlen(parastr), " %ld",
									tmp->reg->limit);
						if ((tmp->reg->mlock & PM_K)
							|| (tmp->reg->mlock & MM_K)) sprintf(parastr +
																 strlen
																 (parastr),
																 " %s",
																 tmp->reg->
																 key
																 && *tmp->
																 reg->
																 key ?
																 tmp->reg->
																 key :
																 "*");
						sSend(":%s MODE %s %s%s", ChanServ, tmp->name,
							  mode, parastr);
#ifdef IRCD_MLOCK
						sSend(":%s MLOCK %s %s%s", ChanServ, tmp->name,
							  mode, parastr);
#endif
					}
				}
			} else {
				person = (cNickList *) oalloc(sizeof(cNickList));
				person->person = nick;
				person->op = 0;
				addChanUser(tmp, person);

				for (a = 0; a < NICKCHANHASHSIZE; a++) {
					if (!nick->chan[a]) {
						nick->chan[a] = tmp;
						break;
					}
				}
				if (tmp->reg) {
					char themask[NICKLEN + USERLEN + HOSTLEN + 3];
					cAkickList* akickRecord = NULL;

					a = getMiscChanOp(tmp->reg, nick->nick,
					         (tmp->reg->flags & CIDENT) == CIDENT,
						NULL, &akickRecord);

					if (a == -1) {
						cAkickList *blah;
						char userhost[NICKLEN + USERLEN + HOSTLEN + 3];
						char userhostM[NICKLEN + USERLEN + HOSTLEN + 3];

						snprintf(userhost, sizeof(userhost),
								 "%s!%s@%s", nick->nick, nick->user,
								 nick->host);
						snprintf(userhostM, sizeof(userhostM),
								 "%s!%s@%s", nick->nick, nick->user,
								 genHostMask(nick->host));

						for (blah = tmp->reg->firstAkick; blah;
							 blah = blah->next) {
							strncpyzt(themask, blah->mask,
									  sizeof(themask));
							if (!match(blah->mask, userhost)
								|| !match(blah->mask, userhostM)) {
								cBanList *newban =
									(cBanList *) oalloc(sizeof(cBanList));
								sSend(":%s MODE %s +b %s", ChanServ, chan,
									  themask);
								strncpyzt(newban->ban, themask, sizeof(newban->ban));
								addChanBan(tmp, newban);
								break;
							}
						}

						kick_for_akick(ChanServ,
								nick->nick,
								tmp->reg,
								akickRecord);
						delChanUser(tmp, person, 1);
						return;
					}
					if ((tmp->reg->flags & (CBANISH | CCLOSE))
						&& !isOper(nick)) {
						banKick(tmp, nick,
								"this channel is closed/banished");
						return;
					}
					if (a < tmp->reg->restrictlevel) {
						banKick(tmp, nick,
								"this channel is restricted to level %i+",
								tmp->reg->restrictlevel);
						return;
					}
					if ((a >= MAOP) && (a < AOP)) {
						sSend(":%s MODE %s +v %s", ChanServ, tmp->name,
							  nick->nick);
						person->op |= CHANVOICE;
						tmp->reg->timestamp = CTime;
						if (tmp && tmp->name && tmp->reg
							&& !strcasecmp(tmp->name, HELPOPS_CHAN)
							&& (getChanOp(tmp->reg, nick->nick) >= 4)) {
							sSend(":ChanServ MODE %s :+h", nick->nick);
							nick->oflags |= NISHELPOP;
						}
					} else if (a && a >= AOP) {
						sSend(":%s MODE %s +o %s", ChanServ, tmp->name,
							  nick->nick);
						person->op |= CHANOP;
						tmp->reg->timestamp = CTime;
						if (tmp && tmp->name
							&& !strcasecmp(tmp->name, HELPOPS_CHAN)) {
							sSend(":ChanServ MODE %s :+h", nick->nick);
							nick->oflags |= NISHELPOP;
						}
					}
					if (tmp->reg->autogreet)
						sSend(":%s NOTICE %s :[%s] %s", ChanServ,
							  nick->nick, tmp->name, tmp->reg->autogreet);
				}
#ifdef CDEBUG
				sSend(":%s PRIVMSG " DEBUGCHAN " :Added %s to channel %s",
					  ChanServ, nick->nick, chan);
#endif
			}
		}
	}
}

/**
 * \brief Removes a user from channel(s)
 * 
 * Removes a user from channel(s) in response to a PART message
 * \param nick Pointer to online user item
 * \param channel List of channel(s) to remove user from
 */
void remUserFromChan(UserList * nick, char *channel)
{
	char chan[CHANLEN], *badTemp = NULL;
	int a;
	ChanList *tmp;
	cNickList *person;

	if (*channel == ':')
		channel++;

	while (*channel) {
		bzero(chan, CHANLEN);
		badTemp = NULL;
		a = 0;

		while (*channel != ',' && *channel != 0) {
			if (a < (CHANLEN - 1))
				chan[a] = *channel;
			else if (!badTemp)
				badTemp = channel;
			a++;
			channel++;
		}
		chan[a] = 0;
		a = 0;

		if (badTemp) {
			continue;
		}

		if (*channel)
			channel++;

		if (chan[0] != '+') {
			tmp = getChanData(chan);

			if (tmp != NULL) {
				person = getChanUserData(tmp, nick);
				if (person == NULL) {
					/* 
					 * I guess killing a user with that PART hack
					 * in here would be bad :)
					 */
					/*nDesynch(nick->nick, "PART"); */
					return;
				}

				if (tmp && (nick->oflags & NISHELPOP)
					&& !(nick->oflags & NISOPER) && tmp->name
					&& !strcasecmp(tmp->name, HELPOPS_CHAN)
					&& tmp->reg && (getChanOp(tmp->reg, nick->nick) <= 5)) {
					sSend(":ChanServ MODE %s :-h", nick->nick);
					nick->oflags &= ~NISHELPOP;
				}

				for (a = 0; a < NICKCHANHASHSIZE; a++) {
					if (nick->chan[a] == tmp) {
						nick->chan[a] = NULL;
						break;
					}
				}
				delChanUser(tmp, person, 1);
				tmp = NULL;
#ifdef CDEBUG
				sSend(":%s PRIVMSG " DEBUGCHAN " :Removed %s from %s",
					  ChanServ, nick->nick, chan);
#endif

			}
		}
	}
}

/**
 * Removes a user from all channels
 *
 * \param nick Pointer to online user item
 */
void remFromAllChans(UserList * nick)
{
	int i, a;

#ifdef CDEBUG
	sSend(":%s PRIVMSG " DEBUGCHAN " :Clearing %s from ALL channels",
		  ChanServ, nick->nick);
#endif
	if (nick == NULL)
		return;

	for (i = 0; i < NICKCHANHASHSIZE; i++) {
		if (nick->chan[i]) {
			cNickList *ctmp = getChanUserData(nick->chan[i], nick);

			if (ctmp)
				delChanUser(nick->chan[i], ctmp, 1);
			nick->chan[i] = NULL;

			/* purge any duplicates */
			for (a = i; a < NICKCHANHASHSIZE; a++)
				if (nick->chan[i] && nick->chan[a] == nick->chan[i])
					nick->chan[a] = NULL;
		}
	}
}

/**
 * Changes a user's nicknames on all channels that they have joined
 *
 * \param oldnick Pointer to the original nickname of the user
 * \param newnick Pointer to an online user item of the target nickname
 */
void changeNickOnAllChans(UserList * oldnick, UserList * newnick)
{
	int i;
	cNickList *tmpnick, *addnick;

#ifdef CDEBUG
	sSend(":%s PRIVMSG " DEBUGCHAN " :Changing nick for %s->%s", ChanServ,
		  oldnick->nick, newnick->nick);
#endif

	for (i = 0; i < NICKCHANHASHSIZE; i++) {
		if (oldnick->chan[i]) {
			addnick = (cNickList *) oalloc(sizeof(cNickList));
			tmpnick = getChanUserData(oldnick->chan[i], oldnick);
			if (!tmpnick)
				FREE(addnick);
			else {
				addnick->op = tmpnick->op;
				addnick->person = newnick;
				newnick->chan[i] = oldnick->chan[i];
				addChanUser(newnick->chan[i], addnick);
				delChanUser(oldnick->chan[i], tmpnick, 1);
			}
		} else
			newnick->chan[i] = NULL;
	}
}


/**
 * \brief Updates services' idea of what a channel's modes are
 * \param args The args[] vector of the MODE message of which args[3]
 *        is the mode string, args[0] is the setter, args[2] is the channel,
 *        and anything beyond args[3] are parameters.
 * \param numargs Total number of indices in the args[] vector passed
 */
void setChanMode(char **args, int numargs)
{
	int on = 1, onarg = 4, i = 0;
	ChanList *tmp = getChanData(args[2]);
	cBanList *tmpban;
	cNickList *tmpnick;
	char mode[20];

	if (tmp == NULL)
		return;

	if (numargs < 3)
		return;

	for (i = 0; args[3][i]; i++) {
		switch (args[3][i]) {
		case '+':
			on = 1;
			break;
		case '-':
			on = 0;
			break;

		case 'b':
			if (on) {
				tmpban = (cBanList *) oalloc(sizeof(cBanList));
				strncpyzt(tmpban->ban, args[onarg], sizeof(tmpban->ban));
				addChanBan(tmp, tmpban);
#ifdef CDEBUG
				sSend(":%s PRIVMSG " DEBUGCHAN " :Added ban %s on %s",
					  ChanServ, args[onarg], tmp->name);
#endif
			} else {
				tmpban = getChanBan(tmp, args[onarg]);
				if (tmpban) {
#ifdef CDEBUG
					sSend(":%s PRIVMSG " DEBUGCHAN
						  " :Removed ban %s on %s", ChanServ, args[onarg],
						  tmp->name);
#endif
					delChanBan(tmp, tmpban);
				}
			}
			onarg++;
			break;

		case 'i':
			if (on) {
				tmp->modes |= PM_I;
#ifdef CDEBUG
				sSend(":%s PRIVMSG " DEBUGCHAN " :Set %s +i", ChanServ,
					  tmp->name);
#endif
			} else {
				tmp->modes &= ~(PM_I);
#ifdef CDEBUG
				sSend(":%s PRIVMSG " DEBUGCHAN " :Set %s -i", ChanServ,
					  tmp->name);
#endif
			}
			break;
		case 'l':
			if (on) {
				tmp->modes |= PM_L;
#ifdef CDEBUG
				sSend(":%s PRIVMSG " DEBUGCHAN " :Set %s +l %lu", ChanServ,
					  tmp->name, tmp->reg->limit);
#endif
			} else {
				tmp->modes &= ~(PM_L);
#ifdef CDEBUG
				sSend(":%s PRIVMSG " DEBUGCHAN " :Set %s -l", ChanServ,
					  tmp->name);
#endif
			}
			break;
		case 'k':
			if (on) {
				tmp->modes |= PM_K;
#ifdef CDEBUG
				sSend(":%s PRIVMSG " DEBUGCHAN " :Set %s +k %s", ChanServ,
					  tmp->name, tmp->reg->key);
#endif
			} else {
				tmp->modes &= ~(PM_K);
#ifdef CDEBUG
				sSend(":%s PRIVMSG " DEBUGCHAN " :Set %s -k %s", ChanServ,
					  tmp->reg->name, args[onarg]);
#endif
			}
			onarg++;
			break;
		case 'm':
			if (on) {
				tmp->modes |= PM_M;
#ifdef CDEBUG
				sSend(":%s PRIVMSG " DEBUGCHAN " :Set %s +m", ChanServ,
					  tmp->name);
#endif
			} else {
				tmp->modes &= ~(PM_M);
#ifdef CDEBUG
				sSend(":%s PRIVMSG " DEBUGCHAN " :Set %s -m", ChanServ,
					  tmp->name);
#endif
			}
			break;
		case 'n':
			if (on) {
				tmp->modes |= PM_N;
#ifdef CDEBUG
				sSend(":%s PRIVMSG " DEBUGCHAN " :Set %s +n", ChanServ,
					  tmp->name);
#endif
			} else {
				tmp->modes &= ~(PM_N);
#ifdef CDEBUG
				sSend(":%s PRIVMSG " DEBUGCHAN " :Set %s -m", ChanServ,
					  tmp->name);
#endif
			}
			break;
		case 'o':
			if (on) {
				tmpnick = getChanUserData(tmp, getNickData(args[onarg]));
				if (!tmpnick) {
				}
				/* do nothing */
				else if (tmp->reg) {
					if (getChanOp(tmp->reg, args[onarg]) < 5
						&& !(tmp->reg->flags & CFORCEXFER)
						&& (tmp->reg->flags & COPGUARD
							|| tmp->firstUser == tmp->lastUser)) {
						sSend
							(":%s NOTICE %s :You are not allowed ops in %s",
							 ChanServ, args[onarg], tmp->name);
						sSend(":%s MODE %s -o %s", ChanServ, tmp->name,
							  args[onarg]);
					} else {
						tmpnick->op |= CHANOP;
#ifdef CDEBUG
						sSend(":%s PRIVMSG " DEBUGCHAN " :Oped %s in %s",
							  ChanServ, args[onarg], tmp->name);
#endif
					}
				} else {
					tmpnick->op |= CHANOP;
#ifdef CDEBUG
					sSend(":%s PRIVMSG " DEBUGCHAN " :Oped %s in %s",
						  ChanServ, args[onarg], tmp->name);
#endif
				}
			} else {
				tmpnick = getChanUserData(tmp, getNickData(args[onarg]));
				if (tmpnick) {
					if (tmp->reg) {
						int t_lev = getChanOp(tmp->reg, args[onarg]);
						int s_lev = getChanOp(tmp->reg, (args[0] /*+ 1*/));

						if ((t_lev > s_lev) && (t_lev >= AOP)
							&& tmp->reg->flags & CPROTOP)
							sSend(":%s MODE %s +o %s", ChanServ, tmp->name,
								  args[onarg]);
						else
							tmpnick->op &= ~CHANOP;
					} else
						tmpnick->op &= ~CHANOP;
#ifdef CDEBUG
					sSend(":%s PRIVMSG " DEBUGCHAN " :DeOped %s in %s",
						  ChanServ, args[onarg], tmp->name);
#endif
				}
			}
			onarg++;
			break;
		case 'p':
			if (on) {
				tmp->modes |= PM_P;
#ifdef CDEBUG
				sSend(":%s PRIVMSG " DEBUGCHAN " :Set %s +p", ChanServ,
					  tmp->name);
#endif
			} else {
				tmp->modes &= ~(PM_P);
#ifdef CDEBUG
				sSend(":%s PRIVMSG " DEBUGCHAN " :Set %s -p", ChanServ,
					  tmp->name);
#endif
			}
			break;
		case 's':
			if (on) {
				tmp->modes |= PM_S;
#ifdef CDEBUG
				sSend(":%s PRIVMSG " DEBUGCHAN " :Set %s +s", ChanServ,
					  tmp->name);
#endif
			} else {
				tmp->modes &= ~(PM_S);
#ifdef CDEBUG
				sSend(":%s PRIVMSG " DEBUGCHAN " :Set %s -s", ChanServ,
					  tmp->name);
#endif
			}
			break;
		case 't':
			if (on) {
				tmp->modes |= PM_T;
#ifdef CDEBUG
				sSend(":%s PRIVMSG " DEBUGCHAN " :Set %s +t", ChanServ,
					  tmp->name);
#endif
			} else {
				tmp->modes &= ~(PM_T);
#ifdef CDEBUG
				sSend(":%s PRIVMSG " DEBUGCHAN " :Set %s -t", ChanServ,
					  tmp->name);
#endif
			}
			break;

		case 'H':
			if (on)
				tmp->modes |= PM_H;
			else
				tmp->modes &= ~PM_H;
			break;

		case 'c':
			if (on) tmp->modes |= PM_C;
			else tmp->modes &= ~PM_C;
			break;

		case 'v':
			if (on) {
				tmpnick = getChanUserData(tmp, getNickData(args[onarg]));
				if (tmpnick) {
					tmpnick->op |= CHANVOICE;
#ifdef CDEBUG
					sSend(":%s PRIVMSG " DEBUGCHAN " :Oped %s in %s",
						  ChanServ, args[onarg], tmp->name);
#endif
				}
			} else {
				tmpnick = getChanUserData(tmp, getNickData(args[onarg]));
				if (tmpnick) {
					tmpnick->op &= ~CHANVOICE;
#ifdef CDEBUG
					sSend(":%s PRIVMSG " DEBUGCHAN " :DeOped %s in %s",
						  ChanServ, args[onarg], tmp->name);
#endif
				}
			}
			onarg++;
			break;
		}

	}

	if (tmp->reg) {
		makeModeLockStr(tmp->reg, mode);
		if (mode[0] != 0) {
			char parastr[IRCBUF * 2];

			*parastr = '\0';
			if ((tmp->reg->mlock & PM_L))
				sprintf(parastr + strlen(parastr), " %ld",
						tmp->reg->limit);
			if ((tmp->reg->mlock & PM_K) || (tmp->reg->mlock & MM_K))
				sprintf(parastr + strlen(parastr), " %s",
						(tmp->reg->key
						 && *tmp->reg->key ? tmp->reg->key : "*"));
			sSend(":%s MODE %s %s%s", ChanServ, tmp->name, mode, parastr);
#ifdef IRCD_MLOCK
			sSend(":%s MLOCK %s %s%s", ChanServ, tmp->name, mode, parastr);
#endif
		}
	}
}

/** 
 * \brief Tell ChanServ about a topic change
 *
 * Updates services' idea of what a channel's topic is. Enforces topic
 * locks.
 *
 * \param args args[] vector from a TOPIC message
 * \param numargs numargs from a TOPIC message
 * \warning Assumes TOPIC_MAX < IRCBUF
 */
void setChanTopic(char **args, int numargs)
{
	RegChanList *tmp;
	char tmptopic[IRCBUF + 1];
/*	u_int16_t    i; */

	if (numargs < 3)
		return;

	/*
	 * If this isn't a channel we care to think about too hard, ignore
	 * the channel topic.
	 */
	tmp = getRegChanData(args[2]);
	if (tmp == NULL)
		return;

	/*
	 * If someone who is too low to set the topic tries, set it back
	 * to what we want it to be.
	 */
	if ((numargs < 4) || getChanOp(tmp, args[3]) < tmp->tlocklevel) {
		if (tmp->topic)
			sSend(":%s TOPIC %s %s %lu :%s", myname, tmp->name,
				  tmp->tsetby, tmp->ttimestamp, tmp->topic);
		return;
	}

	if (tmp->topic)
		FREE(tmp->topic);
	strncpyzt(tmp->tsetby, args[3], NICKLEN);
	tmp->ttimestamp = atol(args[4]);

	/*
	 * copy in the topic.  Make certain we don't overrun
	 * the buffer.  Skip a leading : if present...  If it is not
	 * present, this isn't a valid command.
	 */
	if (*args[5] == ':')
		args[5]++;

	/*
	 * Due to our somewhat broken parsing system... the topic part
	 * isn't passed in one argument, it is passed in lots. :/
	 */
	bzero(tmptopic, TOPIC_MAX);
	parse_str(args, numargs, 5, tmptopic, TOPIC_MAX);
	tmptopic[TOPIC_MAX] = '\0';

	tmp->topic = strdup(tmptopic);
}

/**
 * \brief Print the name of an op level
 *
 * Displays the name of a chanop level
 * \param level What level to stringify
 * \param x_case Value indicates the form of the level name shown
 *        type is 0, 1, or 2: type 0 is the short form such as "AOP",
 *        type 1 is the lowercase form such as "superop", and
 *        type 2 is the uppercase form such as "MiniAop" 
 * \return The name of the specified channel op level, or "op/operator"
 *         if invalid or the default is used
 */
const char *opLevelName(int level, int x_case)
{
	static char *undef_string[3] = {
		"Op", "operator", "Operator"
	};

	if (x_case > 2 || x_case < 0)
		x_case = level = 0;
	if (level == -1 || level > FOUNDER)
		return undef_string[x_case];
	return oplev_table[level].v[x_case];
}

/**
 * \brief Converts a level name into a number
 *
 * A number is generated from a level name
 * \param name Name of the level
 * \return The chanop access level or -1 to indicate an error
 */
int opNameLevel(const char *name)
{
	char *p;
	int level = 0, j = 0;

	if (!name || !*name || strlen(name) > 15)
		return -1;
	if (!strcasecmp(name, "off") || !strcasecmp(name, "user"))
		return 0;

	if (isdigit(*name)) {
		level = atoi(name);
		if (level >= 0 && level <= FOUNDER)
			return level;
	}


	if (*name && (p = strchr(name, '(')) && *p == '(' && isdigit(p[1])) {
		if ((isdigit(p[2]) && p[3] == ')') || p[2] == ')')
			return atoi(p + 1);
	}

	for (level = 0; level <= FOUNDER; level++) {
		for (j = 0; j < 3; j++)
			if (!strcasecmp(oplev_table[level].v[j], name))
				return level;
	}
	return -1;
}

/**
 * Handle timed ChanServ database syncs
 */
void syncChanData(time_t next)
{
	nextCsync = next;
	saveChanData(firstRegChan);
}

/*------------------------------------------------------------------------*/

/*--------------------------------------------------------------------*/

int ValidChannelName(const char* nmtoken)
{
	const char *p;

	if (!nmtoken || !*nmtoken)
		return 0;
	if ((nmtoken[0] != '#' && nmtoken[0] != '+') || !nmtoken[1])
		return 0;
	for(p = nmtoken; *p; p++) {
		if (!iscntrl(*p) && !isspace(*p) && (unsigned char)*p != 0xA0)
			continue;
		return 0;
	}
	return 1;
}

/*--------------------------------------------------------------------*/
/**
 * Channel Expirations 
 */

void expireChans(char *arg)
{
	time_t timestart;
	time_t timeend;
	RegChanList *regchan;
	RegChanList *next;
	ChanList *chan;
	/*RegNickList   *nick; */
	char backup[50];
	int i;

	arg = arg;					/* shut up gcc */

	i = 0;
	mostchans = 0;
	timestart = time(NULL);

	strftime(backup, 49, "chanserv/backups/chanserv.db%d%m%Y",
			 localtime(&timestart));
	rename("chanserv/chanserv.db", backup);
	saveChanData(firstRegChan);

	for (regchan = firstRegChan; regchan; regchan = next) {
		next = regchan->next;	/* save this, since regchan might die */
		mostchans++;

		if ((timestart - regchan->timestamp) >= CHANDROPTIME) {
			if ((regchan->flags & (CHOLD | CBANISH)))
				continue;
#if 0
			printf("Dropping channel %s: %ld %ld %ld\n", regchan->name,
				   timestart, regchan->timestamp,
				   timestart - regchan->timestamp);
#endif

			chan = getChanData(regchan->name);
			if (chan != NULL)
				chan->reg = NULL;
			chanlog->log(NULL, CSE_EXPIRE, regchan->name);
			delRegChan(regchan);
			i++;
		}
	}

	timeend = time(NULL);
	sSend(":%s GLOBOPS :ChanServ EXPIRE(%d/%lu) %ld seconds", ChanServ, i,
		  mostchans, (timeend - timestart));
	timer((int)(2.5 * 3600), (void (*)(char *))expireChans, NULL);
}

/*------------------------------------------------------------------------*/

/* -------------------------------------------------------------------- */
/* The ChanServ command parser                                   -Mysid */
/* -------------------------------------------------------------------- */

/**
 * @brief Parse a ChanServ command.
 */
void sendToChanServ(UserList * nick, char **args, int numargs)
{
	char *from = nick->nick;
	interp::parser * cmd;

	/* small "hack" to make services work in either /cs #channel cmd
	 * or /cs cmd #channel way */
	char crud[IRCBUF];

	if (index(args[0], '#')) {
		counterOldCSFmt++;
		strncpyzt(crud, args[0], IRCBUF);
		strcpy(args[0], args[1]);
		strcpy(args[1], crud);
	}

	cmd =
		new interp::parser(ChanServ, getOpFlags(nick), chanserv_commands,
						   args[0]);
	if (!cmd)
		return;

	if ((cmd->getCmdFlags() & CMD_REG) && !nick->reg) {
		sSend
			(":%s NOTICE %s :You nick must be registered to use that command.",
			 ChanServ, from);
		return;
	}

	switch (cmd->run(nick, args, numargs)) {
	default:
		break;
	case RET_FAIL:
		sSend(":%s NOTICE %s :Unknown command %s.\r\n"
			  ":%s NOTICE %s :Please try /msg %s HELP", ChanServ, from,
			  args[0], ChanServ, from, ChanServ);
		break;
	case RET_OK_DB:
		sSend(":%s NOTICE %s :Next database synch(save) in %ld minutes.",
			  ChanServ, nick->nick, (long)((nextCsync - CTime) / 60));
		break;
	}
}

/*--------------------------------------------------------------------*/

/**
 * \cscmd Help
 * \syntax ChanServ HELP [\<topic\>]
 * \plr View ChanServ help on a topic
 */
CCMD(cs_help)
{
	help(nick->nick, ChanServ, args, numargs);
	return RET_OK;
}

/*--------------------------------------------------------------------*/

/**
 * \cscmd Chanop
 * \syntax ChanServ CHANOP \<#channel\> add \<nick\> \<level\>
 * \syntax ChanServ CHANOP \<#channel\> del \<item\>
 * \syntax ChanServ CHANOP \<#channel\> list {\<option\> \<space\> ...}
 * \plr Maintain a channel op list
 */
CCMD(cs_chanop)
{
	const char *from = nick->nick;
	const char *cmdName = "op", *cTargNick = NULL;
	int targLevel = -1, i;
	int is_add = 0, is_del = 0, is_chanop = 0;
	char cmd_x[25] = "";
	RegChanList *chan;

	struct
	{
		const char *name;
		int lev;
	} op_alias_table[] = {
		{ "maop", MAOP }, { "aop", AOP },
		{ "msop", MSOP}, { "sop", SOP },
		{ "mfounder", MFOUNDER}, { "founder", FOUNDER },
		{ NULL }
	};

	cmd_x[0] = '\0';

	if (numargs >= 1 && args[0] && *args[0]) {
		for (i = 0; args[0][i] && i < 25; i++)
			cmd_x[i] = toupper(args[0][i]);
		cmd_x[i] = '\0';
	}

        if (cmd_x[0] == '-' && strcmp(cmd_x, "-ops"))
            strcpy(cmd_x, "chanop");

	if (numargs < 3) {
		sSend(":%s NOTICE %s :Usage: %s ADD <nick>", ChanServ, from,
			  cmd_x);
		sSend(":%s NOTICE %s :       %s DEL <nick | index#>", ChanServ,
			  from, cmd_x);
		sSend(":%s NOTICE %s :       %s LIST", ChanServ, from, cmd_x);
		sSend
			(":%s NOTICE %s :Type `/msg %s HELP %s', for more information.",
			 ChanServ, from, ChanServ, cmd_x);
		return RET_SYNTAX;
	}

	if (!(chan = getRegChanData(args[1]))) {
		sSend(":%s NOTICE %s :%s is not a registered channel.", ChanServ,
			  from, args[1]);
		return RET_NOTARGET;
	}

	if (!(tolower(args[0][0]) == 'c' && !strcasecmp(args[0] + 1, "hanop"))) {
		for (i = 0; op_alias_table[i].name; i++)
			if (op_alias_table[i].name[0] == tolower(*args[0])
				&& !strcasecmp(1 + op_alias_table[i].name, 1 + args[0]))
				break;
		if (op_alias_table[i].name) {
			targLevel = op_alias_table[i].lev;
			cmdName = op_alias_table[i].name;
		}
	} else
		is_chanop = 1;

	if ((is_add = !strcasecmp(args[2], "add"))
		|| (is_del = !strncasecmp(args[2], "del", 3))) {
		if (numargs < 3) {
			sSend(":%s NOTICE %s :%s who?", ChanServ, from,
				  tolower(*args[2]) == 'd' ? "delete" : "add");
			return RET_SYNTAX;
		}

		cTargNick = args[3];
		if (is_add) {
			if ((numargs >= 5)) {
				int newTargLevel = opNameLevel(args[4]);

				if (newTargLevel < 1 || newTargLevel > FOUNDER)
					newTargLevel = -1;

				if (!is_chanop) {
					if ((newTargLevel > targLevel)
						|| (newTargLevel < MAOP - 1)
						|| (newTargLevel <= AOP && targLevel == MSOP)
						|| (newTargLevel < MSOP && targLevel == SOP)
						|| (newTargLevel <= SOP && targLevel == MFOUNDER)
						) {
						sSend
							(":%s NOTICE %s :Error: Invalid level for %s command.",
							 ChanServ, from, cmdName);
						sSend
							(":%s NOTICE %s :Type `/msg %s HELP %s', for more information.",
							 ChanServ, from, ChanServ, cmd_x);
						return RET_SYNTAX;
					}
				}
				targLevel = newTargLevel;
			}
			return do_chanop(CS_ADDOP, nick, chan, cTargNick, targLevel);
		} else
			return do_chanop(CS_DELOP, nick, chan, cTargNick, targLevel);
	} else if (!strcasecmp(args[2], "list")) {
		if ((numargs >= 4))
			cTargNick = args[3];
		return do_chanop(CS_LISTOP, nick, chan, cTargNick, targLevel);
	} else {
		sSend(":%s NOTICE %s :Unknown %s command %s.\r"
			  ":%s NOTICE %s :/msg %s HELP for assistance", ChanServ, from,
			  cmdName, args[2], ChanServ, from, ChanServ);
		return RET_SYNTAX;
	}

	return RET_OK;
}

/*--------------------------------------------------------------------*/

/**
 * \cscmd Akick
 * \syntax Akick \<channel\> list
 * \plr View a channel autokick list.
 *
 * \syntax Akick \<channel\> add \<mask\> [\<reason\>]
 * \plr Add an autokick for the specified mask of the form nick!user\@host.
 * Wildcards * and ? are meaningful.
 *
 * \syntax Akick \<channel\> del \<item\>
 * \plr Remove a channel autokick
 */
CCMD(cs_akick)
{
	const char *from = nick->nick;
	char theCmd[256];
	int i = 3;

	if (numargs < 2) {
		sSend(":%s NOTICE %s :Channel name required", ChanServ, from);

		return RET_SYNTAX;
	}

	if (numargs < 3) {
		sSend(":%s NOTICE %s :Not enough arguments to command", ChanServ,
			  from);
		return RET_SYNTAX;
	}

	strncpyzt(theCmd, args[2], sizeof(theCmd));

	while (i < numargs) {
		strncpyzt(args[i - 1], args[i], ARGLEN);
		i++;
	}
	numargs--;

	if (!strcasecmp(theCmd, "add"))
		return cs_addak(nick, args, numargs);
	else if (!match("del*", theCmd))
		return cs_delak(nick, args, numargs);
	else if (!strcasecmp(theCmd, "list"))
		return cs_listak(nick, args, numargs);
	else {
		sSend(":%s NOTICE %s :Unknown akick command %s.\r"
			  ":%s NOTICE %s :/msg %s HELP for assistance", ChanServ, from,
			  args[2], ChanServ, from, ChanServ);
		return RET_SYNTAX;
	}
	return RET_OK;
}

/*--------------------------------------------------------------------*/

/**
 * \cscmd Mdeop
 * \syntax Mdeop \<channel\>
 *
 * \plr Deop all ops of lower access in a channel.  An operator with +D can
 * use override mdeop, in this case all channel operators are deopped.
 * Regardless, the channel must be registered for mdeop to work.
 * Requires MAOP access.
 */
CCMD(cs_mdeop)
{
	char *from = nick->nick;
	char accNick[NICKLEN+1];
	ChanList *chan;
	cNickList *tmp;
	int aclvl;

	if (numargs < 2) {
		sSend(":%s NOTICE %s :You must specify a channel", ChanServ, from);
		return RET_SYNTAX;
	}

	chan = getChanData(args[1]);
	if (!chan) {
		sSend(":%s NOTICE %s :%s is not currently active", ChanServ, from,
			  args[1]);
		return RET_NOTARGET;
	}

	chan->reg = getRegChanData(args[1]);
	if (!chan->reg) {
		sSend(":%s NOTICE %s :%s is not registered", ChanServ, from,
			  args[1]);
		return RET_NOTARGET;
	}

	aclvl = getMiscChanOp(chan->reg, from, ChanGetIdent(chan->reg), accNick, NULL);
	if (opFlagged(nick, OVERRIDE | OSERVOP)
		|| opFlagged(nick, OVERRIDE | ODMOD)) aclvl = FOUNDER + 1;

	if (MAOP > aclvl) {
		sSend
			(":%s NOTICE %s :You must be at least level %i in the channel op list to MDEOP.",
			 ChanServ, from, MAOP);
		return RET_NOPERM;
	}
	sendToChanOpsAlways(chan, "%s!%s is using command MDEOP on %s", accNick, from,
						chan->name);
	for (tmp = chan->firstUser; tmp; tmp = tmp->next) {
		if (aclvl > getChanOp(chan->reg, tmp->person->nick)) {
			sSend(":%s MODE %s -o %s", ChanServ, chan->name,
				  tmp->person->nick);
			tmp->op = 0;
		} else
			sSend
				(":%s NOTICE %s :Your access is not high enough to deop %s",
				 ChanServ, from, tmp->person->nick);
	}
	return RET_OK;
}

/*--------------------------------------------------------------------*/

/**
 * \cscmd Mkick
 * \syntax Mkick \<channel\> { | "-o" }
 *
 * \plr Kicks all users out of a channel and blocks re-entry until all
 * users have been removed from the channel and a delay has passed.
 * An oper can supply the "-o" option to invoke this without channel
 * access. Requires SOP access.
 */
CCMD(cs_mkick)
{
	char *from = nick->nick;
	char accNick[NICKLEN+1];
	ChanList *chan;
	cNickList *tmp;
	cNickList *next;
	int aclvl;

	if (numargs < 2) {
		sSend(":%s NOTICE %s :You must specify a channel", ChanServ, from);
		return RET_SYNTAX;
	}

	chan = getChanData(args[1]);
	if (chan == NULL) {
		sSend(":%s NOTICE %s :%s is not currently active", ChanServ, from,
			  args[1]);
		return RET_NOTARGET;
	}

	if (chan->reg == NULL) {
		sSend(":%s NOTICE %s :%s is not registered", ChanServ, from,
			  args[1]);
		return RET_NOTARGET;
	}

	aclvl = getMiscChanOp(chan->reg, from, 1, accNick, NULL);

	if ((SOP > aclvl) && (!isOper(nick))) {
		sSend
			(":%s NOTICE %s :You must be at least level %i in the channel op list to MKICK.",
			 ChanServ, from, SOP);
		return RET_NOPERM;
	}

	if (isOper(nick) && (SOP > aclvl)) {
		if (numargs > 2) {
			if (strncmp(args[2], "-o", 2) == 0) {
				sSend
					(":%s GLOBOPS :%s is using MKICK on %s (without SOP level)",
					 ChanServ, from, chan->name);
			} else {
				sSend
					(":%s NOTICE %s :You are an oper but not a channel op, to override MKICK limitations, use -o",
					 ChanServ, from);
				return RET_FAIL;
			}
		} else {
			sSend
				(":%s NOTICE %s :You are an oper but not a channel op, to override MKICK limitations, use -o",
				 ChanServ, from);
			return RET_FAIL;
		}
	}

	createGhostChannel(chan->name);
	sSend(":%s MODE %s +b *!*@*", ChanServ, chan->name);
	timer(10, deleteTimedGhostChannel, strdup(chan->name));

	sendToChanOpsAlways(chan, "%s!%s is using MKICK on %s", accNick, nick->nick, from, chan->name);

	for (tmp = chan->firstUser; tmp; tmp = next) {
		next = tmp->next;

		sSend(":%s KICK %s %s", ChanServ, chan->name, tmp->person->nick);
		delChanUser(chan, tmp, 1);
	}
	return RET_OK;
}

/*--------------------------------------------------------------------*/

/**
 * @brief Print results of a /Chanserv Info to a user.
 *
 * ( Revamped to use tables -Mysid )
 */
void sendChannelInfo(UserList *nick, RegChanList *chan, int fTerse)
{
	char *from = nick->nick;
	char buf[IRCBUF];
	char timejunk[80];
	char modestring[20];
	const char *tmpcChar;
	unsigned int i;
	int hidden_details = 0;
	RegNickList *founderInfo;

	struct { 
		flag_t flags;
		const char *brief, *alt;
		const char *line;
	}
	csflagOptTab[] = {
		{ CIDENT,   "ident", "Ident",  "Ident. Ops must identify to their nick before being recognised by ChanServ" }, 
		{ COPGUARD, "opguard", "SecuredOps", "Op Guard. Only users in ChanServ's op list can have ops in the channel" }, 
		{ CKTOPIC,  "keeptopic", "\"Sticky\" Topics", "Preserve Topic. ChanServ will remember and re-set the last topic when the channel is empty" }, 
		{ CPROTOP,  "protect ops", NULL, "Protected Ops. Ops cannot be deopped by another op with lower access" }, 
		{ CQUIET,   "quiet", "Quiet", "Quiet Changes. ChanServ will not inform channel ops when channel options are changed" }, 
		{ CHOLD,    "\2held\2", "held", "Held. This channel will not expire." }, 
	};

	if ((chan->mlock & (PM_S)) || (chan->flags & (CBANISH)))
		hidden_details = 2; /* 2 means hide details */
	else if ((chan->mlock & (PM_I)) || (chan->mlock & (PM_K)))
		hidden_details = 1; /* 1 means hide autogreet */

	if (hidden_details) {
		if (isOper(nick) || 
		    getMiscChanOp(chan, nick->nick, 1, NULL, NULL) >= MAOP)
		hidden_details = 0;
	}

	if ((chan->flags & CBANISH))
		sSend(":%s NOTICE %s :%s is BANISHED.", ChanServ, from, chan->name);
	else if ((chan->flags & CCLOSE))
		sSend(":%s NOTICE %s :\2This channel is CLOSED\2", ChanServ, from);
	else
	{
		founderInfo = chan->founderId.getNickItem();

		switch(fTerse)
		{
			case 2:
				sSend(":%s NOTICE %s :\2***\2 Info on %s",
					  ChanServ, from, chan->name);		
				break;
			case 1:
				sSend(":%s NOTICE %s :Information on %s (terse):",
					  ChanServ, from, chan->name);
				break;
			case 0: default:

			sSend(":%s NOTICE %s :Information on %s:",
				  ChanServ, from, chan->name);
				break;
		}

		if (founderInfo != NULL)
		{
#if defined(NETNICK) && defined(NETNICKFOUNDERLINE)
			if (strcasecmp(founderInfo->nick, NETNICK) == 0)
			{
				sSend(":%s NOTICE %s :Founder    : %s", ChanServ, from, NETNICKFOUNDERLINE);
			}
			else
#endif
				sSend(":%s NOTICE %s :Founder    : %s (%s@%s)", ChanServ, from, founderInfo->nick, founderInfo->user, regnick_ugethost(nick, founderInfo));
		}
		else if ((tmpcChar = chan->founderId.getNick()) != NULL) {
			if (*tmpcChar != '\0' && *tmpcChar != '*')
				sSend(":%s NOTICE %s :Founder    : %s", ChanServ, from, tmpcChar);
		}

		if (chan->desc[0] != '\0')
			sSend(":%s NOTICE %s :Description: %s", ChanServ, from,
				  chan->desc);

		//// This is private, possibly //////////////////////////////
		if (!hidden_details && chan->autogreet != NULL)
			sSend(":%s NOTICE %s :Autogreet  : %s", ChanServ, from,
				  chan->autogreet);

		if ((hidden_details < 2) && chan->topic) {
			sSend(":%s NOTICE %s :Topic      : %s (%s)", ChanServ, from,
				  chan->topic, chan->tsetby);
		}
		if (chan->url)
			sSend(":%s NOTICE %s :Url        : %s", ChanServ, from, chan->url);
		buf[0] = '\0';

		if (!fTerse)
			sSend(":%s NOTICE %s :Options:", ChanServ, from);

		for(i = 0; i < sizeof(csflagOptTab) / sizeof(csflagOptTab[0]); i++)
		{
			if ((chan->flags & csflagOptTab[i].flags) == csflagOptTab[i].flags)
			{
				if (fTerse)
				{
					if (buf[0] != '\0')
						strcat(buf, ", ");
					if (fTerse == 2 && csflagOptTab[i].alt)
						strcat(buf, csflagOptTab[i].alt);
					else
						strcat(buf, csflagOptTab[i].brief);
				}
				else
					sSend(":%s NOTICE %s :%s", ChanServ, from, csflagOptTab[i].line);

			}
		}

		if (fTerse)
		{
			if (fTerse == 2) {				
				if (chan->restrictlevel)
					sprintf(buf + strlen(buf), "%sRestricted(%d)",
					        buf[0] != '\0' ? ", " : "", chan->restrictlevel);
				if (chan->tlocklevel > AOP)
					sprintf(buf + strlen(buf), "%sTopic Lock(%d)",
					        buf[0] != '\0' ? ", " : "", chan->tlocklevel);				
			}
			sSend(":%s NOTICE %s :Options    : %s", ChanServ, from, buf);
			if (chan->memolevel > 0)
				sSend(":%s NOTICE %s :Memo Level : %s", ChanServ, from, opLevelName(chan->memolevel, 0));
		}

		makeModeLockStr(chan, modestring);

		if (fTerse) 
		{
			if (fTerse != 2)
			{
				if (*modestring != '\0')
					sSend(":%s NOTICE %s :Mode Locked to: \002%s\002", ChanServ, from,
					      modestring);
				sSend(":%s NOTICE %s :Topic Lock: Level %2i+, Restricted: Level %2i+",
				 ChanServ, from, chan->tlocklevel, chan->restrictlevel);
			}
			else {
				if (*modestring != '\0')
					sSend(":%s NOTICE %s :Mode Lock  : %s", ChanServ, from,
					  modestring);
			}
		}
		else
		{
			if (*modestring == '\0')
				strcpy(modestring, "(none)");

			sSend
				(":%s NOTICE %s :Mode Locked to: \002%s\002 = These modes will be enforced on the channel",
				 ChanServ, from, modestring);

			sSend(":%s NOTICE %s :Topic Lock: \002Level %i+\002 = %s",
				  ChanServ, from, chan->tlocklevel,
				  (chan->tlocklevel == 0) ? "Any channel operator can change the topic" :
				  "Only operators with this access level or higher can change the topic");
			sSend(":%s NOTICE %s :Restrict Lock: \002Level %i+\002 = %s",
				  ChanServ, from, chan->restrictlevel,
				  (chan->restrictlevel == 0) ? "Anyone can join this channel" :
				  "Only operators with this access level or higher can join");
		}

		if (strftime(timejunk, 80, "%a %Y-%b-%d %T %Z",
				 localtime(&chan->timereg)) > 0)
		{
			if (fTerse != 2)
				sSend(":%s NOTICE %s :Channel registered at: %s", ChanServ, from,
					  timejunk);
			else
				sSend(":%s NOTICE %s :Registered : %s", ChanServ, from,
					  timejunk);
		}

		if (strftime(timejunk, 80, "%a %Y-%b-%d %T %Z",
				 localtime(&chan->timestamp)) > 0)
		{
			if (fTerse != 2)
				sSend(":%s NOTICE %s :Channel last used: %s", ChanServ, from,
					  timejunk);
			else
				sSend(":%s NOTICE %s :Last Used  : %s", ChanServ, from,
					  timejunk);
		}
	}

	if (isOper(nick)) {
		sSend(":%s NOTICE %s :+ Oper Info: %i Ops   %i Akicks", ChanServ, from,
			  chan->ops, chan->akicks);

		if (chan->flags & CMARK) {
			sSend(":%s NOTICE %s :+ Channel MARKED%s%s.", ChanServ, from,
				  chan->markby ? " by " : "",
				  chan->markby ? chan->markby : "");
		}
	}

	/*if (numargs > 2 && args[2])
	   tzapply(DEF_TIMEZONE); */

	if (fTerse != 2)
		sSend(":%s NOTICE %s :End of information on %s", ChanServ, from,
			  chan->name);
	else
		sSend(":%s NOTICE %s :\2***\2 End of info", ChanServ, from);
}

/**
 * \cscmd Info
 * \syntax Info { options } \<channel\>
 *
 * \plr Get information on a channel.
 * Options include:
 * - -verbose
 * - -terse
 *
 * -Mysid
 */
CCMD(cs_info)
{
	char *from = nick->nick;
	RegChanList *chan;
	int i, fTerse;

	if (addFlood(nick, 5))
		return RET_KILLED;

	fTerse = (nick->reg && (nick->reg->flags & NTERSE));

	for(i = 1; i < numargs; i++) {
		if (args[i][0] != '-')
			break;
		if (strcmp(args[i], "-long") == 0 || strcmp(args[i], "-verbose") == 0)
			fTerse = 0;
		else if (strcmp(args[i], "-terse") == 0 || strcmp(args[i], "-short") == 0)
			fTerse = 1;
		else if (strcmp(args[i], "-2") == 0)
			fTerse = 2;
	}

	if (i >= numargs) {
		sSend(":%s NOTICE %s :You must specify a channel", ChanServ, from);
		return RET_SYNTAX;
	}

	chan = getRegChanData(args[i]);

	if (!chan) {
		sSend(":%s NOTICE %s :%s is not registered", ChanServ, from,
			  args[i]);
		return RET_NOTARGET;
	}

	/*if (chan && numargs > 2 && args[2])
	   tzapply(args[2]); */

	sendChannelInfo(nick, chan, fTerse);

	return RET_OK;
}

/*--------------------------------------------------------------------*/

/**
 * \cscmd Access
 * \syntax Access \<channel\> [\<nickname\>]
 */
/**
 * @brief Get access level [?of target] on channel
 *
 * Improved to always indicate nick used to get access unless
 * not the same as the present nick. -Mysid
 */
CCMD(cs_access)
{
	char *from = nick->nick;
	char checkAccessNick[NICKLEN + 1];
	RegChanList *tmp;
	UserList *tmpnick;
	RegNickList *tmprnick;
	int level;

	if (numargs < 2) {
		sSend(":%s NOTICE %s :You must specify at least a channel",
			  ChanServ, from);
		return RET_SYNTAX;
	}

	tmp = getRegChanData(args[1]);

	if (!tmp) {
		sSend(":%s NOTICE %s :%s is not registered", ChanServ, from,
			  args[1]);
		return RET_NOTARGET;
	}

	if (numargs >= 3)
		if (getNickData(args[2]) == NULL) {
			sSend(":%s NOTICE %s :%s is not online", ChanServ, from,
				  args[2]);
			return RET_NOTARGET;
		}

	level =
		getMiscChanOp(tmp, (numargs >= 3) ? args[2] : nick->nick,
					  ChanGetIdent(tmp), checkAccessNick, NULL);
	tmpnick = getNickData((numargs >= 3) ? args[2] : nick->nick);
	tmprnick = getRegNickData(checkAccessNick);

	if (level == FOUNDER && *checkAccessNick == '*')
		sSend(":%s NOTICE %s :%s %s identified as the founder of %s",
			  ChanServ, from, (numargs >= 3) ? args[2] : "You",
			  (numargs >= 3) ? "is" : "are", tmp->name);
	else if (level < 1)
		sSend(":%s NOTICE %s :%s %s access level %i on %s", ChanServ, from,
			  (numargs >= 3) ? args[2] : "You",
			  (numargs >= 3) ? "has" : "have", level, tmp->name);
	else if (tmprnick && tmpnick && isIdentified(tmpnick, tmprnick))
		sSend
			(":%s NOTICE %s :%s %s access level %i on %s via identification to %s",
			 ChanServ, from, (numargs >= 3) ? args[2] : "You",
			 (numargs >= 3) ? "has" : "have", level, tmp->name,
			 checkAccessNick);
	else
		sSend
			(":%s NOTICE %s :%s %s access level %i on %s from access list of %s",
			 ChanServ, from, (numargs >= 3) ? args[2] : "You",
			 (numargs >= 3) ? "has" : "have", level, tmp->name,
			 checkAccessNick);
	return RET_OK;
}

/*--------------------------------------------------------------------*/

/**
 * \cscmd Register 
 * \syntax Register #\<channel\> \<password\> {\<description\>}
 */
CCMD(cs_register)
{
	char *from = nick->nick;
	char descbuf[IRCBUF];
	char pw_reason[IRCBUF];
	cNickList *tmp;
	cAccessList *founder;
	ChanList *chan;
	RegChanList *reg;
	int override = 0;

	if (numargs < 4) {
		sSend
			(":%s NOTICE %s :You must specify a channel, a password, and a description for that channel to register it",
			 ChanServ, from);
		sSend
			(":%s NOTICE %s :See \2/msg %s HELP REGISTER\2 for more information",
			 ChanServ, from, ChanServ);
		return RET_SYNTAX;
	}

	chan = getChanData(args[1]);
	tmp = getChanUserData(chan, nick);

	if (ValidChannelName(args[1]) == 0) {
		sSend(":%s NOTICE %s :Invalid channel name, %s.", ChanServ, from,
			  args[1]);
		return RET_EFAULT;
	}

	if (nick == NULL || nick->reg == NULL || isIdentified(nick, nick->reg) == 0) {
		sSend
			(":%s NOTICE %s :You must be identified to register a channel",
			 NickServ, from);
		return RET_FAIL;
	}

	if (chan != NULL && chan->reg != NULL) 
	{
		sSend(":%s NOTICE %s :%s is already registered", ChanServ, from,
			  args[1]);
		return RET_FAIL;
	}

	if (is_sn_chan(args[1]) != 0 && opFlagged(nick, OOPER | OVERRIDE) == 0) 
	{
		sSend(":%s NOTICE %s :This channel is reserved for " NETWORK ".",
			  ChanServ, from);
		return RET_NOPERM;
	}

        if (isPasswordAcceptable(args[2], pw_reason) == 0) 
	{
                sSend(":%s NOTICE %s :Sorry, %s isn't a password that you can use.",
                        ChanServ, from, args[1]);
                sSend(":%s NOTICE %s :%s", ChanServ, from, pw_reason);
                return RET_EFAULT;
        }

	if (PASSLEN < strlen(args[2])) {
		sSend
			(":%s NOTICE %s :Your password is too long, it must be %d characters or less",
			 ChanServ, from, PASSLEN);
		return RET_EFAULT;
	}

#if CHANDESCBUF < IRCBUF
	parse_str(args, numargs, 3, descbuf, CHANDESCBUF + 1);
#else
	parse_str(args, numargs, 3, descbuf, IRCBUF);
#endif

	if (strlen(descbuf) >= CHANDESCBUF) {
		sSend(":%s NOTICE %s :Your description is too long, it must be %d characters or less.",
			 ChanServ, from, CHANDESCBUF - 1);
		return RET_EFAULT;
	}

	if (nick->reg->chans >= ChanLimit && !opFlagged(nick, OVERRIDE)
            && !(nick->reg->opflags & OROOT)) {
		sSend(":%s NOTICE %s :You have registered too many channels",
			  ChanServ, from);
#ifdef REGLIMITYELL
		sSend(":%s GLOBOPS :%s is trying to register too many channels",
			  ChanServ, from);
#endif
		return RET_FAIL;
	}

	if (!chan || !tmp || !tmp->op) {
		if (!opFlagged(nick, OVERRIDE | OSERVOP)
			&& !opFlagged(nick, OVERRIDE | ODMOD)) {
			sSend
				(":%s NOTICE %s :You must be both inside, and oped, in the channel you wish to register",
				 ChanServ, from);
			return RET_FAIL;
		}
	}

	if ((override = opFlagged(nick, OVERRIDE))) {
		sSend(":%s NOTICE %s :Override registration ok.", ChanServ, from);
		sSend(":%s GLOBOPS :%s used override register on channel %s",
			  ChanServ, from, args[1]);
	}

	nick->reg->chans++;
	reg = (RegChanList *) oalloc(sizeof(RegChanList));
	initRegChanData(reg);
	if (chan != 0) {
		chan->reg = reg;
	}
	strcpy(reg->name, chan ? chan->name : args[1]);
	reg->founderId = nick->reg->regnum;
	reg->flags |= CENCRYPT;
	pw_enter_password(args[2], reg->password, ChanGetEnc(reg));

	reg->timereg = CTime;
	reg->timestamp = CTime;
	if (chan != 0) {
		sendToChanOpsAlways(chan, "%s just registered %s", from, args[1]);
	}
	sSend
		(":%s NOTICE %s :%s is now registered under your nick: \002%s\002",
		 ChanServ, from, args[1], from);
	sSend(":%s NOTICE %s :You now have level 15 access.", ChanServ, from);
	sSend(":%s NOTICE %s :Your password is \002%s\002 DO NOT FORGET IT",
		  ChanServ, from, args[2]);
	sSend(":%s NOTICE %s :We are NOT responsible for lost passwords.",
		  ChanServ, from);
	sSend(":%s NOTICE %s :Your channel is modelocked +tn-k", ChanServ,
		  from);
	sSend(":%s NOTICE %s :And topic preservation is set on", ChanServ,
		  from);
	sSend
		(":%s NOTICE %s :For information on channel maintenance, /msg %s HELP",
		 ChanServ, from, ChanServ);
	founder = (cAccessList *) oalloc(sizeof(cAccessList));
	founder->nickId = nick->reg->regnum;
	founder->uflags = FOUNDER;

	addChanOp(reg, founder);
	reg->flags |= CKTOPIC;
	reg->flags |= CIDENT;
	reg->mlock |= PM_N;
	reg->mlock |= PM_T;
	reg->mlock |= MM_H;
	reg->mlock |= MM_K;
	reg->facc = 1;
	addRegChan(reg);
	mostchans++;
	chanlog->log(nick, CS_REGISTER, chan ? chan->name : args[1], 0,
				 override ? "(override)" : "");
	strncpyzt(reg->desc, descbuf, CHANDESCBUF);
	return RET_OK_DB;
}

/*--------------------------------------------------------------------*/

/**
 * \cscmd Identify
 * \syntax Identify \<channel\> \<password\>
 */
/**
 * Identify for founder access
 *
 * Improvements, support for identity using a nick other than
 * listed in /cs info (remote id.) -Mysid
 */
CCMD(cs_identify)
{
	char *from = nick->nick;
	RegChanList *chan;
	cAccessList *newitem;

	if (numargs < 3) {
		sSend
			(":%s NOTICE %s :You must specify a channel and password to identify with",
			 ChanServ, from);
		return RET_SYNTAX;
	}
	chan = getRegChanData(args[1]);

	if (!chan) {
		sSend(":%s NOTICE %s :The channel is not registered.", ChanServ,
			  from);
		return RET_NOTARGET;
	}

	if ((chan->flags & (CBANISH | CCLOSE)) && !isRoot(nick)) {
		sSend(":%s NOTICE %s :You cannot identify to that channel because it is closed or banished.",
			 ChanServ, from);
		return RET_FAIL;
	}

	if ((chan->flags & CFORCEXFER)) {
                sSend(":%s NOTICE %s :An operator has disabled the current "
                      "channel password for a forced change of founder.",
                      ChanServ, from);
		return RET_FAIL;
	}

        if (isMD5Key(args[2]))
        {
              struct auth_data auth_info[] = {{
                      nick->auth_cookie,
                      nick->idnum.getHashKey(),
                      2
              }};

            if (!Valid_md5key(args[2], auth_info, chan->name, chan->password, ChanGetEnc(chan)))
            {
                sSend(":%s NOTICE %s :Invalid MD5 key.", NickServ, nick->nick);
                nick->auth_cookie = 0;
                if (BadPwChan(nick, chan))
                    return RET_KILLED;

                 return RET_BADPW;
            }

            /* Valid */
	    nick->auth_cookie = 0;
        }
	else if (!Valid_pw(args[2], chan->password, ChanGetEnc(chan))) {
		sSend(":%s NOTICE %s :Invalid password", ChanServ, from);

		if (!(chan->flags & CFORCEXFER)) {
			if (BadPwChan(nick, chan))
				return RET_KILLED;
		}

		return RET_BADPW;
	}

/// XXX should check nick->caccess maybe?
	if (nick->reg && nick->reg->regnum == chan->founderId)
		chan->facc = 1;
	else {
		if (chan->id.nick)
			FREE(chan->id.nick);
		chan->id.nick = (char *)oalloc(strlen(nick->nick) + 1);
		chan->id.timestamp = CTime;
		chan->id.idnum = nick->idnum;
		strcpy(chan->id.nick, nick->nick);
		chan->facc = 0;
	}
	chan->timestamp = CTime;
	sSend(":%s NOTICE %s :You are now the identified founder of %s",
		  ChanServ, from, chan->name);
	/* If a founder identifies, but is not in the ops list, that's generally a bad thing. */

	if (!chan->facc)
		return RET_OK;

	if (nick->reg && nick->caccess > 1)
	{
		newitem = getChanOpData(chan, from);

		if (newitem == NULL) {
			newitem = (cAccessList *) oalloc(sizeof(cAccessList));
			newitem->uflags = FOUNDER;
			newitem->nickId = nick->reg->regnum;
			addChanOp(chan, newitem);
		} else if (newitem->uflags < FOUNDER)
			newitem->uflags = FOUNDER;
	}

	GoodPwChan(nick, chan);
	chan->chpw_key = 0;

	return RET_OK;
}

/*--------------------------------------------------------------------*/

/**
 * \cscmd Addop
 * \syntax Addop #\<channel> \<nick> [\<level>]
 *
 * \plr Interface for adding channel operators
 */
CCMD(cs_addop)
{
	char *from = nick->nick;
	RegChanList *chan;
	services_cmd_id thisCmd = CS_ADDOP;
	int targLevel;

	if (tolower(*args[0]) == 'd')
		thisCmd = CS_DELOP;

	if (numargs < 3) {
		if (thisCmd == CS_ADDOP)
			sSend(":%s NOTICE %s :You must specify both a channel to add "
				  "to, and a nick for ADDOP.", ChanServ, from);
		else
			sSend
				(":%s NOTICE %s :You must specify both a channel to delete "
				 "from, and a nickname to remove.", ChanServ, from);
		return RET_SYNTAX;
	}

	chan = getRegChanData(args[1]);
	if (chan == NULL) {
		sSend(":%s NOTICE %s :%s channel is not registered.", ChanServ,
			  from, args[1]);
		return RET_NOTARGET;
	}

	targLevel = (numargs < 4) ? -1 : opNameLevel(args[3]);

	if ((thisCmd == CS_ADDOP) && targLevel <= 0)
		targLevel = AOP;

	return do_chanop(thisCmd, nick, chan, args[2], targLevel);
}

/**
 * \brief Handle one of the services channel oplist editing commands
 *        such as '/ChanServ AOP'
 * \param services_cmd_id Command interface chosen, #CS_ADDOP, #CS_DELOP, or #CS_LISTOP
 * \param isfrom User sending the command
 * \param chan Pointer to registered chnanel record to be effected
 * \param cTargetNick Nickname to be effected
 * \param tarLevel Access level target [to add, remove from, etc]
 */
static cmd_return do_chanop(services_cmd_id cmd,
                            UserList * isfrom,
                            RegChanList * chan,
                            const char *cTargetNick,
                            int tarLevel
)
{
	int mylevel;

	if (!isfrom || !chan || (!cTargetNick && cmd != CS_LISTOP))
		return RET_FAIL;
	mylevel = getChanOp(chan, isfrom->nick);

        if (isFounder(chan, isfrom))
		mylevel = FOUNDER;

	if ((AOP > mylevel) && !opFlagged(isfrom, OOPER | OVERRIDE)) {
		sSend
			(":%s NOTICE %s :Your access is too low to modify the channel oplist.",
			 ChanServ, isfrom->nick);
		return RET_NOPERM;
	}

	if (chan->ops >= ChanMaxOps(chan) && (cmd == CS_ADDOP)
		&& !opFlagged(isfrom, OVERRIDE)) {
		sSend(":%s NOTICE %s :%s has too many ops", ChanServ, isfrom->nick,
			  chan->name);
		sSend(":%s GLOBOPS :%s tries to add too many ops to %s.", ChanServ, isfrom->nick, chan->name);

		if (addFlood(isfrom, 10))
			return RET_KILLED;

		return RET_FAIL;
	}

	if (!opFlagged(isfrom, OVERRIDE | ODMOD | OOPER)
		&& getChanOpId(chan, isfrom->nick) < AOP) {
		sSend
			(":%s NOTICE %s :You must identify to modify channel op lists.",
			 ChanServ, isfrom->nick);
		return RET_NOPERM;
	}

	switch (cmd) {
	default:
		break;
	case CS_ADDOP:
		return do_chanop_add(isfrom, chan, cTargetNick, tarLevel);
		break;
	case CS_DELOP:
		return do_chanop_del(isfrom, chan, cTargetNick, tarLevel);
		break;
	case CS_LISTOP:
		return do_chanop_list(isfrom, chan, cTargetNick, tarLevel);
		break;
	}

	return RET_FAIL;
}

/**
 * \brief Handle one of the services channel oplist ADD commands
 *        such as '/ChanServ AOP <channel> ADD'
 * \param isfrom User sending the command
 * \param chan Pointer to registered chnanel record to be effected
 * \param cTargetNick Nickname to be added
 * \param tarLevel Access level to add op at
 */
static cmd_return do_chanop_add(UserList * isfrom, 
                                RegChanList * chan,
                                const char *cTargetNick, 
                                int tarLevel
)
{
	const char *from = isfrom->nick;
	char accNick[NICKLEN+1];
	cAccessList *targetEntry, *anop;
	RegNickList *tarRegNick;
	int currentlevel;
	int maxEdit = FOUNDER;

	int mylevel = getMiscChanOp(chan, isfrom->nick, ChanGetIdent(chan), accNick, NULL);

	if ((mylevel < FOUNDER) && isOper(isfrom) && cTargetNick
		&& opFlagged(isfrom, ODMOD | OVERRIDE)) {
		sSend(":%s GLOBOPS :%s using override add on %s",
			  ChanServ, isfrom->nick, chan->name);
		mylevel = FOUNDER + 1;
	}
	targetEntry = getChanOpData(chan, cTargetNick);
	tarRegNick = getRegNickData(cTargetNick);

	if (tarRegNick == NULL) {
		sSend
			(":%s NOTICE %s :The nickname %s is not registered, nicknames must be registered to add as ops",
			 ChanServ, from, cTargetNick);
		return RET_NOTARGET;
	}

	if (mylevel <= AOP)
		maxEdit = 0;
	else if (mylevel < MSOP)
		maxEdit = 1;
	else if (mylevel < SOP)
		maxEdit = MAOP;
	else if (mylevel < FOUNDER)
		maxEdit = mylevel - 1;
	else
		maxEdit = mylevel;

	if (targetEntry) {
		if (tarLevel == -1) {
			sSend(":%s NOTICE %s :%s is already in the channel %s "
				  "list, and you did not specify a valid level to change"
				  " them to.", ChanServ, isfrom->nick,
				  opLevelName(tarLevel, 0), cTargetNick);
			return RET_FAIL;
		}

		if (tarLevel < 1 || tarLevel > FOUNDER) {
			sSend
				(":%s NOTICE %s :The level you specified (%d) is not a valid level",
				 ChanServ, from, tarLevel);
			return RET_EFAULT;
		}

		if ((anop = getChanOpData(chan, cTargetNick)))
			currentlevel = anop->uflags;
		else
			currentlevel = 0;

		if ((!opFlagged(isfrom, ODMOD | OVERRIDE)
				&& !isFounder(chan, isfrom)) && mylevel != FOUNDER) {
			if (maxEdit >= mylevel)
				maxEdit = mylevel - 1;

			if (mylevel < tarLevel || maxEdit < tarLevel) {
				sSend
					(":%s NOTICE %s :A%s %s cannot promote an operator to level %s.",
					 ChanServ, from, mylevel == MAOP ? "n" : "",
					 opLevelName(mylevel, 2), opLevelName(tarLevel, 2));
				return RET_NOPERM;
			}

			if (maxEdit < currentlevel || mylevel < currentlevel) {
				sSend
					(":%s NOTICE %s :A%s %s cannot modify the access level of a%s %s.",
					 ChanServ, from, mylevel == 5 ? "n" : "",
					 opLevelName(mylevel, 2),
					 currentlevel == MAOP ? "n" : "",
					 opLevelName(currentlevel, 2));
				return RET_NOPERM;
			}
		}

		/* It seems this person can modify the level to whatever level
		 * was specified */
		sSend(":%s NOTICE %s :%s was %s from %s to %s.", ChanServ, from,
			  cTargetNick,
			  ((tarLevel > targetEntry->uflags) ? "promoted" : "demoted"),
			  opLevelName(targetEntry->uflags, 2), opLevelName(tarLevel,
															   2));

		targetEntry->uflags = tarLevel;
		return RET_OK_DB;
	}

	if (tarLevel == -1)
		tarLevel = MAOP;

	if ((!opFlagged(isfrom, ODMOD | OVERRIDE) && !isFounder(chan, isfrom)
             && mylevel != FOUNDER)) {
		if (maxEdit >= FOUNDER)
			maxEdit = FOUNDER - 1;
	}

	if (tarLevel > maxEdit) {
		sSend
			(":%s NOTICE %s :Permission denied -- you cannot add ops at levels higher than %s (%d).",
			 ChanServ, from, opLevelName(maxEdit, 2), maxEdit);
		return RET_NOPERM;
	}

	if ((tarRegNick->flags & NOADDOP) && 
             (!isOper(isfrom) ||
                (!opFlagged(isfrom, OVERRIDE | OSERVOP)
		&& !opFlagged(isfrom, OVERRIDE | ODMOD))) ) {
		sSend
			(":%s NOTICE %s :The nickname %s has set a feature which disallows the adding of this nickname to an oplist.",
			 ChanServ, from, cTargetNick);
		return RET_NOPERM;
	}

	targetEntry = (cAccessList *) oalloc(sizeof(cAccessList));

	if (tarLevel < 1 || tarLevel > FOUNDER) {
		sSend
			(":%s NOTICE %s :The level you specified (%d) is not a valid level",
			 ChanServ, from, tarLevel);
		FREE(targetEntry);
		return RET_EFAULT;
	}

	if (!tarLevel)
		targetEntry->uflags = AOP;
	else
		targetEntry->uflags = tarLevel;

	targetEntry->nickId = tarRegNick->regnum;
	sSend(":%s NOTICE %s :Added op(%i) %s(%i)", ChanServ, from,
		  chan->ops + 1, cTargetNick, targetEntry->uflags);
	sendToChanOps(getChanData(chan->name), "%s!%s added op %s(%i)",
	                          accNick, from, cTargetNick,
	                          targetEntry->uflags);
	addChanOp(chan, targetEntry);
	return RET_OK_DB;
}

/**
 * \brief Handle one of the services channel oplist DEL commands
 *        such as '/ChanServ AOP <channel> DEL'
 * \param isfrom User sending the command
 * \param chan Pointer to registered chnanel record to be effected
 * \param cTargetNick Nickname to be deleted
 * \param tarLevel Access level to delete ops at
 */
static cmd_return do_chanop_del(UserList * isfrom, RegChanList * chan,
                                const char *cTargetNick, int tarLevel)
{
	char *from = isfrom->nick;
	cAccessList *delop;
	const char *tmpNick;
	int a, x;
	int maxEdit = FOUNDER;

	if ((x = atoi(cTargetNick))) {
		for (delop = chan->firstOp; delop; delop = delop->next) {
			if (tarLevel != delop->uflags && tarLevel != -1)
				continue;
			if (delop->index == x)
				break;
		}
		if (delop == NULL) {
			sSend
				(":%s NOTICE %s :%s is not an index in the %s list for %s",
				 ChanServ, from, cTargetNick,
				 tarLevel == -1 ? "ops" : opLevelName(tarLevel, 2),
				 chan->name);
			return RET_NOTARGET;
		}
	} else {
			delop = getChanOpData(chan, cTargetNick);
			if (delop && tarLevel != -1 && tarLevel != delop->uflags)
				delop = NULL;
	}

	if (delop == NULL) {
		sSend(":%s NOTICE %s :%s is not in the %s list for %s", ChanServ,
			  from, cTargetNick,
			  tarLevel == -1 ? "ops" : opLevelName(tarLevel, 2),
			  chan->name);
		return RET_NOTARGET;
	}

	a = getChanOp(chan, from);

	if (opFlagged(isfrom, OOPER | ODMOD | OVERRIDE)) {
		sSend(":%s GLOBOPS :%s using directmod on %s (delop %s [%d])",
			  ChanServ, isfrom->nick, chan->name, cTargetNick, tarLevel);
		a = FOUNDER;
	}

	if (a < AOP)
		maxEdit = 0;
	else if (a < MSOP)
		maxEdit = 1;
	else if (a < SOP)
		maxEdit = MAOP;
	else if (a < FOUNDER)
		maxEdit = a - 1;
	else
		maxEdit = a;

	if ((delop->uflags > maxEdit) && (!isfrom->reg || delop->nickId != isfrom->reg->regnum)) {
/***=======
*	if((newitem->uflags >= MFOUNDER && FOUNDER > mylevel)
*	   || (newitem->uflags >= MSOP && MFOUNDER > mylevel)
*	   || ((newitem->uflags > (mylevel - 5)) && MFOUNDER > mylevel)
*	   || (mylevel < MSOP)) {
**>>>>>>> 1.230.2.23*/
		sSend(":%s NOTICE %s :Access denied", ChanServ, from);
		return RET_NOPERM;
	}

	tmpNick = delop->nickId.getNick();

	if (tmpNick)
	{
		sendToChanOps(getChanData(chan->name), "%s deleted op %s(%i)", from,
					  tmpNick, delop->uflags);
		sSend(":%s NOTICE %s :Deleted op %s on %s", ChanServ, from,
			  tmpNick, chan->name);
	}
	else {
		sendToChanOps(getChanData(chan->name), "%s deleted op %s(%i)", from,
					  "*deactivated*", delop->uflags);
	}

	delChanOp(chan, delop);
	return RET_OK_DB;
}


/**
 * \brief Handle one of the services channel oplist LIST commands
 *        such as '/ChanServ AOP <channel> LIST'
 * \param isfrom User sending the command
 * \param chan Pointer to registered chnanel record
 * \param cTargetNick Nickname or pattern to search for
 * \param tarLevel Access level to list channel operators at
 */
static cmd_return do_chanop_list(UserList * isfrom, RegChanList * chan,
								 const char *cTargetNick, int tarLevel)
{
	cAccessList *tmp;
	const char *from = isfrom->nick;
	const char *tmpName;
	RegNickList *tmpOpRnl;
	MaskData *tmpMask = NULL;
	int i = 0, x = 0;

	if (cTargetNick) 
	{
			tmpMask = make_mask();

			if (split_userhost(cTargetNick, tmpMask) < 0)
			{
				free_mask(tmpMask);
				tmpMask = NULL;
			}
	}

	if (tarLevel == -1)
	{
		sSend(":%s NOTICE %s :   %-3s Listing For %s", ChanServ, from,
			  "Op", chan->name);
		sSend(":%s NOTICE %s :(Index) (Level) (Nick)", ChanServ, from);
	}
	else {
		sSend(":%s NOTICE %s :%s Search results for channel %s", ChanServ, from,
			  opLevelName(tarLevel, 2), chan->name);
	}



	for (tmp = chan->firstOp; tmp; tmp = tmp->next, i++) {
		if (!(tarLevel == -1 || tmp->uflags == tarLevel))
			continue;
		tmpOpRnl = (tmp->nickId).getNickItem();
		tmpName  = tmpOpRnl ? tmpOpRnl->nick : "*deactivated*";

		if (cTargetNick && match(cTargetNick, tmpName))
		{
			if (tmpMask == NULL || tmpOpRnl == NULL)
				continue;

			if (match(tmpMask->nick, tmpOpRnl->nick) ||
			    match(tmpMask->user, tmpOpRnl->user) ||
			    match(tmpMask->host, regnick_ugethost(isfrom, tmpOpRnl, 0)))
				continue;
		}

		if (tarLevel == -1)
		{
			sSend(":%s NOTICE %s :(%-3i) (%-4i)  (%-30s)", ChanServ, from,
			  tmp->index, tmp->uflags, tmpName);
		}
		else 
		{
			if (tmpOpRnl)
				sSend(":%s NOTICE %s :%4i %s (%s@%s)", ChanServ, from,
				  tmp->index, tmpName, tmpOpRnl->user, regnick_ugethost(isfrom, tmpOpRnl));
			else
				sSend(":%s NOTICE %s :%4i %s", ChanServ, from,
				  tmp->index, tmpName);
		}


		x++;
	}

	if (tarLevel == -1) {
		sSend(":%s NOTICE %s :%i op elements searched", ChanServ, from, i);
		sSend(":%s NOTICE %s :%i matched given conditions", ChanServ, from, x);
	}
	else {
		sSend(":%s NOTICE %s :[%i match(es) found]", ChanServ, from, x);
	}

	if (tmpMask) {
		free_mask(tmpMask);
	}

	return RET_OK;
}

/*--------------------------------------------------------------------*/

/**
 * @brief Clean out bad chanops
 */
int cleanChopList(RegChanList *chan)
{
	cAccessList *delbad, *delbad_next;
	int removedBad = 0;

	for(delbad = chan->firstOp; delbad; delbad = delbad_next) {
		delbad_next = delbad->next;

		if (!delbad->nickId.getNick()) {
			delChanOp(chan, delbad);
			++removedBad;
		}
	}

	return removedBad;
}

/**
 * \cscmd Clean
 * \syntax Clean \<channel>
 */
CCMD(cs_clean)
{
	char *from = nick->nick;
	RegChanList *chan;
	int removedBad = 0, aclvl;

	if (numargs < 2) {
		sSend(":%s NOTICE %s :You must specify a channel", ChanServ, from);
		return RET_SYNTAX;
	}

	chan = getRegChanData(args[1]);

	if (chan == NULL) {
		sSend(":%s NOTICE %s :%s is not registered", ChanServ, from,
			  args[1]);
		return RET_NOTARGET;
	}

	aclvl = getChanOp(chan, from);

	if ((MSOP > aclvl) && (!isOper(nick))) {
		sSend
			(":%s NOTICE %s :You must be at least level %i in the channel op list to CLEAN.",
			 ChanServ, from, MSOP);
		return RET_NOPERM;
	}

	removedBad = cleanChopList(chan);

	if (removedBad)
	{
		sSend(":%s NOTICE %s :Removed %d chanop entries that were no longer valid.",
			 ChanServ, from, removedBad);
		return RET_OK_DB;
	}
	else
	{
		sSend(":%s NOTICE %s :No bad op entries found.",
			 ChanServ, from);
		return RET_OK_DB;
	}
}

/**
 * \cscmd Addak
 * \syntax Addak \<channel> {\<nick> | \<nick!user\@host>} [\<reason>]
 */
CCMD(cs_addak)
{
	char *from = nick->nick;
	cAkickList *newitem;
	RegChanList *chan;
	int i;

	if (numargs < 3) {
		sSend
			(":%s NOTICE %s :You must specify both a channel to add to, and a mask for ADDAK",
			 ChanServ, from);
		return RET_SYNTAX;
	}

	chan = getRegChanData(args[1]);
	if (!chan) {
		sSend
			(":%s NOTICE %s :The channel must be registered to allow addition/removal of akicks",
			 ChanServ, from);
		return RET_FAIL;
	}

	if (MSOP > getChanOp(chan, from)
		&& (!opFlagged(nick, OVERRIDE | ODMOD | OOPER))) {
		sSend(":%s NOTICE %s :Your access is too low to add akicks",
			  ChanServ, from);
		return RET_NOPERM;
	}

	if (chan->akicks >= (int)ChanMaxAkicks(chan) && (!opFlagged(nick, OVERRIDE))) {
		sSend(":%s NOTICE %s :%s has too many akicks", ChanServ, from,
			  chan->name);
		sSend(":%s GLOBOPS :%s tries to add too many akicks to %s.", ChanServ, from, chan->name);

		if (addFlood(nick, 10))
			return RET_KILLED;
		return RET_FAIL;
	}
	newitem = getChanAkick(chan, args[2]);
	if (newitem) {
		sSend(":%s NOTICE %s :%s is already in the akick list", ChanServ,
			  from, args[2]);
		return RET_FAIL;
	}
	if (!index(args[2], '!') || !index(args[2], '@')) {
		sSend(":%s NOTICE %s :Invalid AKick mask %s", ChanServ, from,
			  args[2]);
		return RET_EFAULT;
	}

	newitem = (cAkickList *) oalloc(sizeof(cAkickList));
	strncpyzt(newitem->mask, args[2], USERLEN + HOSTLEN + 2);
	snprintf(newitem->reason, 51 + NICKLEN, "%s:", from);
	for (i = 3;
		 i < numargs
		 && (strlen(newitem->reason) + strlen(args[i]) + 2) <= NICKLEN + 51; i++) {
		strcat(newitem->reason, " ");
		strcat(newitem->reason, args[i]);
	}
	newitem->added = CTime;
	addChanAkick(chan, newitem);
	sendToChanOps(getChanData(chan->name), "%s added akick %s (%s)", from,
				  newitem->mask, newitem->reason);
	sSend(":%s NOTICE %s :Added Akick %s to %s", ChanServ, from,
		  newitem->mask, chan->name);
	return RET_OK_DB;
}

/**
 * \cscmd Wipeak
 * \syntax Wipeak \<channel>
 */
CCMD(cs_wipeak)
{
	char *from = nick->nick;
	RegChanList *chan;
	cAkickList *curr_akick;
	cAkickList *next_akick;


	if (numargs < 2) {
		sSend(":%s NOTICE %s :You must specify a channel.", ChanServ,
			  from);
		return RET_SYNTAX;
	}

	chan = getRegChanData(args[1]);

	if (chan == NULL) {
		sSend(":%s NOTICE %s :The channel is not registered", ChanServ,
			  from);
		return RET_NOTARGET;
	}

	if (!opFlagged(nick, OVERRIDE | ODMOD)
		&& (MFOUNDER > getChanOp(chan, from))) {
		sSend(":%s NOTICE %s :Access denied", ChanServ, from);
		return RET_NOPERM;
	}

	for (curr_akick = chan->firstAkick; curr_akick != (cAkickList *) NULL;
		 curr_akick = next_akick) {
		next_akick = curr_akick->next;
		delChanAkick(chan, curr_akick);
	}

	sendToChanOps(getChanData(chan->name), "%s wiped the akick list.",
				  from);
	sSend(":%s NOTICE %s :All Akicks removed.", ChanServ, from);
	return RET_OK_DB;
}


/**
 * \cscmd Wipeop
 * \syntax Wipeop \<channel>
 */
CCMD(cs_wipeop)
{
	char *from = nick->nick;
	RegChanList *chan;
	cAccessList *curr_op;
	cAccessList *next_op;


	if (numargs < 2) {
		sSend(":%s NOTICE %s :You must specify a channel.", ChanServ,
			  from);
		return RET_SYNTAX;
	}

	chan = getRegChanData(args[1]);

	if (!opFlagged(nick, OVERRIDE | ODMOD)) {
		sSend(":%s NOTICE %s :Permission Denied -- this command is only available through DMOD and when using override mode.", ChanServ, from);
		return RET_NOPERM;
	}

	if (chan == NULL) {
		sSend(":%s NOTICE %s :The channel is not registered", ChanServ,
			  from);
		return RET_NOTARGET;
	}

	for (curr_op = chan->firstOp; curr_op != (cAccessList *) NULL;
		 curr_op = next_op) {
		next_op = curr_op->next;
		delChanOp(chan, curr_op);
	}

	sSend(":%s NOTICE %s :All chanops removed.", ChanServ, from);
	return RET_OK_DB;
}

/**
 * \cscmd Delak
 * \syntax Delak \<channel> \<mask>
 */
CCMD(cs_delak)
{
	char *from = nick->nick;
	RegChanList *chan;
	cAkickList *delak;
	int x;

	if (numargs < 3) {
		sSend
			(":%s NOTICE %s :You must specify a channel and the AKick to delete",
			 ChanServ, from);
		return RET_SYNTAX;
	}

	chan = getRegChanData(args[1]);

	if (chan == NULL) {
		sSend(":%s NOTICE %s :The channel is not registered", ChanServ,
			  from);
		return RET_NOTARGET;
	}

	if ((x = atoi(args[2]))) {
		for (delak = chan->firstAkick; delak; delak = delak->next) {
			if (delak->index == x)
				break;
		}
	} else
		delak = getChanAkick(chan, args[2]);

	if (delak == NULL) {
		sSend(":%s NOTICE %s :%s is not in the akick list for %s",
			  ChanServ, from, args[2], chan->name);
		return RET_NOTARGET;
	}

	if (MSOP > getChanOp(chan, from) && !opFlagged(nick, OVERRIDE | ODMOD)) {
		sSend(":%s NOTICE %s :Access denied", ChanServ, from);
		return RET_NOPERM;
	}

	sendToChanOps(getChanData(chan->name), "%s deleted akick %s", from,
				  delak->mask);
	sSend(":%s NOTICE %s :Removed AKick %s on %s", ChanServ, from,
		  delak->mask, chan->name);
	delChanAkick(chan, delak);
	return RET_OK_DB;
}

/*--------------------------------------------------------------------*/

/**
 * \cscmd Listop
 * \syntax Listop \<channel> { option \<argument> }
 */
CCMD(cs_listop)
{
	char *from = nick->nick;
	int levelsearch = 0;
	char namesearch[NICKLEN];

	RegChanList *tmpchan;

	bzero(namesearch, NICKLEN);

	if (numargs == 1) {
		sSend(":%s NOTICE %s :You must specify a channel", ChanServ, from);
		return RET_SYNTAX;
	}

	if (numargs == 2) {
		levelsearch = -1;
		namesearch[0] = 0;
	}
	if (numargs == 3) {
		sSend(":%s NOTICE %s :Improper command format", ChanServ, from);
		return RET_SYNTAX;
	}
	if (numargs == 4) {
		if (!strcasecmp(args[2], "-l"))
			levelsearch = atoi(args[3]);
		else if (!strcasecmp(args[2], "-n"))
			strncpyzt(namesearch, args[3], NICKLEN);
		else {
			sSend(":%s NOTICE %s :Unknown search parameter", ChanServ,
				  from);
			return RET_SYNTAX;
		}
	}

	tmpchan = getRegChanData(args[1]);

	if ((tmpchan == NULL)) {
		sSend(":%s NOTICE %s :%s is not registered", ChanServ, from,
			  args[1]);
		return RET_NOTARGET;
	}

	if ((getChanOp(tmpchan, from) < 1) && !isOper(nick)) {
		sSend(":%s NOTICE %s :Access denied", ChanServ, from);
		return RET_NOPERM;
	}

	return do_chanop_list(nick, tmpchan, namesearch[0] ? namesearch : NULL,
						  levelsearch);
}

/*--------------------------------------------------------------------*/

/**
 * \cscmd Listak
 * \syntax Listak \<channel>
 */
CCMD(cs_listak)
{
	char *from = nick->nick;
	char aktime[35];
	RegChanList *tmpchan;
	int i = 0, x = 0;
	char namesearch[NICKLEN + USERLEN + HOSTLEN + 5];
	cAkickList *tmp;

	bzero(namesearch, NICKLEN + USERLEN + HOSTLEN);
	aktime[0] = '\0';

	if (numargs == 1) {
		sSend(":%s NOTICE %s :You must specify a channel", ChanServ, from);
		return RET_SYNTAX;
	}

	if (numargs == 2)
		namesearch[0] = 0;

	if (numargs == 3) {
		sSend(":%s NOTICE %s :Improper command format", ChanServ, from);
		return RET_SYNTAX;
	}

	if (numargs == 4) {
		if (!strcasecmp(args[2], "-n")) {
			strncpyzt(namesearch, args[3], NICKLEN + USERLEN + HOSTLEN);
			namesearch[sizeof(namesearch) - 1] = '\0';
		} else {
			sSend(":%s NOTICE %s :Unknown search parameter", ChanServ,
				  from);
			return RET_SYNTAX;
		}
	}

	tmpchan = getRegChanData(args[1]);

	if (tmpchan == NULL) {
		sSend(":%s NOTICE %s :%s is not registered", ChanServ, from,
			  args[1]);
		return RET_NOTARGET;
	}

	if ((getChanOp(tmpchan, from) < MAOP) && !isOper(nick)) {
		sSend(":%s NOTICE %s :Access denied", ChanServ, from);
		return RET_NOPERM;
	}

	sSend(":%s NOTICE %s :Akick Listing For %s", ChanServ, from, args[1]);
	sSend(":%s NOTICE %s :(Index) (Mask)", ChanServ, from);
	sSend(":%s NOTICE %s :(Time [GMT])     (Reason)", ChanServ, from);

	for (tmp = tmpchan->firstAkick; tmp; tmp = tmp->next, i++) {
		if (namesearch[0]) {
			if (!match(namesearch, tmp->mask)) {
				strftime(aktime, sizeof(aktime), "%a %Y-%b-%d %T",
						 gmtime(&tmp->added));
				aktime[sizeof(aktime) - 1] = '\0';
				sSend(":%s NOTICE %s :(%-3i) (%-s)", ChanServ, from,
					  tmp->index, tmp->mask);
				sSend(":%s NOTICE %s :(%-20s) (%-s)", ChanServ, from,
					  aktime, tmp->reason);
				x++;
				if ((x >= 15) && !isOper(nick) && (x % 3) == 0) {
					if (nick->floodlevel.GetLev() >= 66) {
						sSend(":%s NOTICE %s :Command cancelled (flood protection).", ChanServ, nick->nick);
						sSend(":%s NOTICE %s :Please wait at least 30 seconds before sending services further commands.", ChanServ, nick->nick);
						return RET_FAIL;
					}
					if (addFlood(nick, 1))
						return RET_FAIL;
				}
			}
		} else {
			strftime(aktime, sizeof(aktime), "%a %Y-%b-%d %T",
					 gmtime(&tmp->added));
			aktime[sizeof(aktime) - 1] = '\0';
			sSend(":%s NOTICE %s :(%-3i) (%-s)", ChanServ, from,
				  tmp->index, tmp->mask);
			sSend(":%s NOTICE %s :(%-20s) (%-s)", ChanServ, from, aktime,
				  tmp->reason);
			x++;
			if ((x >= 15) && !isOper(nick) && (x % 3) == 0) {
				if (nick->floodlevel.GetLev() >= 66) {
					sSend(":%s NOTICE %s :Command cancelled (flood protection).", ChanServ, nick->nick);
					sSend(":%s NOTICE %s :Please wait at least 30 seconds before sending services further commands.", ChanServ, nick->nick);
					return RET_FAIL;
				}
				if (addFlood(nick, 1))
					return RET_FAIL;
			}
		}
	}

	sSend(":%s NOTICE %s :%i akick elements searched", ChanServ, from, i);
	sSend(":%s NOTICE %s :%i matched given conditions", ChanServ, from, x);
	return RET_OK;
}

/*--------------------------------------------------------------------*/

/**
 * \cscmd Drop
 * \syntax Drop \<channel> \<password>
 */
CCMD(cs_drop)
{
	RegChanList *tmp;
	ChanList *chan;
	char *from = nick->nick;

	if (numargs < 3) {
		sSend(":%s NOTICE %s :You must specify a channel and password",
			  ChanServ, from);
		return RET_SYNTAX;
	}

	tmp = getRegChanData(args[1]);

	if (tmp == NULL) {
		sSend(":%s NOTICE %s :The channel is not registered", ChanServ,
			  from);
		return RET_NOTARGET;
	}

	if ((tmp->flags & (CBANISH | CCLOSE)) && !isRoot(nick)) {
		sSend(":%s NOTICE %s :You cannot drop that channel because it is closed or banished.",
			 ChanServ, from);
		return RET_EFAULT;
	}

	if ((tmp->flags & (CMARK | CHOLD)) && !isRoot(nick)) {
		sSend(":%s NOTICE %s :Sorry, dropping this channel is not allowed.",
				ChanServ, from);

		sSend(":%s NOTICE %s :You may contact a services operator for assistance. "
				      "see /motd %s",
				ChanServ, from, InfoServ);
		chanlog->log(nick, CS_DROP, args[1], LOGF_FAIL);
		return RET_EFAULT;
	}

	if (isFounder(tmp, nick) == 0) {
		sSend(":%s NOTICE %s :Access denied", ChanServ, from);
		return RET_NOPERM;
	}

	if (!Valid_pw(args[2], tmp->password, ChanGetEnc(tmp))) {
		sSend(":%s NOTICE %s :Incorrect password, this has been logged",
			  ChanServ, from);
		chanlog->log(nick, CS_DROP, args[1], LOGF_BADPW);

		if (BadPwChan(nick, tmp))
			return RET_KILLED;

		return RET_BADPW;
	}

	if ((chan = getChanData(tmp->name)))
		sendToChanOps(chan, "%s just dropped %s", from, args[1]);
	sSend
		(":%s NOTICE %s :Channel registration for %s has been discontinued",
		 ChanServ, from, args[1]);
	chan = getChanData(args[1]);
	if (chan)
		chan->reg = NULL;
	chanlog->log(nick, CS_DROP, args[1], 0);

	GoodPwChan(nick, tmp);
	delRegChan(tmp);
	return RET_OK_DB;
}

/*--------------------------------------------------------------------*/

/**
 * \cscmd Op
 * \syntax Op \<channel> [\<nick>]
 * \todo Support multinick /cs op perhaps?
 */
CCMD(cs_op)
{
	char *from = nick->nick;
	int a, mylevel;
	RegChanList *tmp;
	ChanList *chan;
	cNickList *tmpnick;

	if (numargs < 2) {
		sSend(":%s NOTICE %s :You must specify a channel", ChanServ, from);
		return RET_SYNTAX;
	}

	tmp = getRegChanData(args[1]);
	chan = getChanData(args[1]);
	if (chan == NULL) {
		sSend(":%s NOTICE %s :%s: No such channel", ChanServ, from,
			  args[1]);
		return RET_NOTARGET;
	}
	if (tmp == NULL) {
		sSend
			(":%s NOTICE %s :Cannot receive ops on non registered channel",
			 ChanServ, from);
		return RET_FAIL;
	}

	if (numargs == 3)
		tmpnick = getChanUserData(chan, getNickData(args[2]));
	else
		tmpnick = getChanUserData(chan, nick);

	if (tmpnick == NULL) {
		sSend(":%s NOTICE %s :%s is not in %s", ChanServ, from,
			  (numargs == 3) ? args[2] : from, args[1]);
		return RET_FAIL;
	}

	a = getChanOp(tmp, (numargs == 3) ? args[2] : from);

	if (a == 0 || ((mylevel = getChanOp(tmp, from)) < MAOP)) {
		sSend(":%s NOTICE %s :Access denied", ChanServ, from);
		return RET_NOPERM;
	}

	if ((mylevel < MFOUNDER) && (mylevel <= AOP) && (mylevel < a)) {
		sSend
			(":%s NOTICE %s :Permission denied -- a(n) %s cannot use this command on a(n) %s.",
			 ChanServ, from, opLevelName(mylevel, 0), opLevelName(a, 0));
		return RET_NOPERM;
	}

	if (getChanData(tmp->name)
		&& strcmp(from, (numargs == 3) ? args[2] : from)) {
		if (mylevel < AOP) {
			sSend(":%s NOTICE %s :Access Denied", ChanServ, from);
			return RET_NOPERM;
		}
		sendToChanOpsAlways(getChanData(tmp->name),
							"%s used \002OP\002 on %s", from,
							(numargs == 3) ? args[2] : from);
	}

	if (mylevel < AOP || a < AOP) {
		sSend(":%s MODE %s +v %s", ChanServ, args[1],
			  (numargs == 3) ? args[2] : from);
		tmpnick->op |= CHANVOICE;
		tmp->timestamp = CTime;


		if (tmp && tmp->name && !strcasecmp(tmp->name, HELPOPS_CHAN)
			&& (getChanOp(tmp, nick->nick) >= 4) && (numargs < 3)) {
			sSend(":ChanServ MODE %s :+h", from);
			nick->oflags |= NISHELPOP;
		}

		return RET_OK;
	} else {
		sSend(":%s MODE %s +o %s", ChanServ, args[1],
			  (numargs == 3) ? args[2] : from);
		tmpnick->op |= CHANOP;
		tmp->timestamp = CTime;

		if (tmp && tmp->name && tmp && !strcasecmp(tmp->name, HELPOPS_CHAN)
			&& !strcasecmp(from, (numargs >= 3) ? args[2] : from)
			&& (getChanOp(tmp, nick->nick) >= 4) && (numargs < 3)) {
			sSend(":ChanServ MODE %s :+h", from);
			nick->oflags |= NISHELPOP;
		}

		return RET_OK;
	}
	return RET_OK;
}

/*--------------------------------------------------------------------*/

/**
 * \cscmd Deop
 * \syntax Deop \<channel> \<nick>
 */
CCMD(cs_deop)
{
	char *from = nick->nick;
	UserList *target;
	ChanList *chan;
	cNickList *deopto;
	int mylevel, victlevel, self_deop = 0;

	if (numargs < 3) {
		sSend(":%s NOTICE %s :You must specify a channel and user to deop",
			  ChanServ, from);
		return RET_SYNTAX;
	}

	chan = getChanData(args[1]);

	if (chan == NULL) {
		sSend(":%s NOTICE %s :%s: No such channel", ChanServ, from,
			  args[1]);
		return RET_NOTARGET;
	}

	if (chan->reg == NULL) {
		sSend(":%s NOTICE %s :%s: Is not registered", ChanServ, from,
			  args[1]);
		return RET_NOTARGET;
	}

	deopto = getChanUserData(chan, getNickData(args[2]));
	target = getNickData((numargs >= 3) ? args[2] : from);

	if (deopto == NULL) {
		sSend(":%s NOTICE %s :%s is not in %s", ChanServ, from, args[2],
			  args[1]);
		return RET_NOTARGET;
	}

	if ((mylevel = getChanOp(chan->reg, from)) < MAOP) {
		sSend(":%s NOTICE %s :Access denied", ChanServ, from);
		return RET_NOPERM;
	}

	victlevel = getChanOp(chan->reg, args[2]);

	if (!strcmp(from, (numargs == 3) ? args[2] : from))
		self_deop = 1;

	if ((victlevel > mylevel) || ((mylevel <= MAOP) && !self_deop)) {
		sSend
			(":%s NOTICE %s :Permission denied -- a level %d operator may not deop another level %d operator.",
			 ChanServ, from, mylevel, victlevel);
		return RET_NOPERM;
	}

	if (chan && chan->reg && target && (target->oflags & NISHELPOP)
		&& !(target->oflags & NISOPER) && chan->name
		&& !strcasecmp(chan->name, HELPOPS_CHAN)
		&& (getChanOp(chan->reg, target->nick) <= 5)) {
		sSend(":ChanServ MODE %s :-h", target->nick);
		target->oflags &= ~NISHELPOP;
	}

	sendToChanOpsAlways(getChanData(chan->reg->name),
						"%s used \002DEOP\002 on %s", from, args[2]);
	sSend(":%s MODE %s -ov %s %s", ChanServ, args[1], args[2], args[2]);
	deopto->op &= ~CHANOP;
	return RET_OK;
}

/*--------------------------------------------------------------------*/

/**
 * \cscmd Banish
 * \syntax Banish \<channel> ("" | "On" | "Yes" )
 * \plr Banish a channel.
 * \syntax Banish \<channel> ( "Off" | "No" )
 * \plr Unbanish a channel.
 */
CCMD(cs_banish)
{
	char *from = nick->nick;
	ChanList *ochan;
	RegChanList *chan;

	if (!isServop(nick) && !opFlagged(nick, OOPER | OCBANDEL)) {
		sSend(":%s NOTICE %s :Access denied", ChanServ, from);
		return RET_NOPERM;
	}

	if (numargs < 2) {
		sSend(":%s NOTICE %s :Must supply at least one argument", ChanServ,
			  from);
		return RET_SYNTAX;
	}

	if (!isRoot(nick) && is_sn_chan(args[1])) {
		sSend(":%s NOTICE %s :That channel cannot be banished (magic channel).", ChanServ,
			  from);
		return RET_EFAULT;
	}

	chan = getRegChanData(args[1]);
	if ((*args[1] != '#' && *args[1] != '+')) {
		sSend(":%s NOTICE %s :Invalid channel name.", ChanServ, from);
		return RET_EFAULT;
	}

	if (numargs < 3) {
		ochan = getChanData(args[1]);
		if (!chan) {
			chan = (RegChanList *) oalloc(sizeof(RegChanList));
			initRegChanData(chan);

			strncpyzt(chan->name, args[1], CHANLEN);

			addRegChan(chan);
			mostchans++;
			if (ochan)
				ochan->reg = chan;
			chan->founderId = RegId(0, 1);
			chan->password[0] = '\0';

			/* strcpy(chan->password, "\2\2\33\377\2\2"); */
			chan->timereg = chan->timestamp = CTime;
			chan->flags |= CBANISH|CENCRYPT;
			chan->mlock = (PM_N | PM_T | MM_K);
		}
		chan->flags |= CBANISH;

		sSend(":%s NOTICE %s :%s is now \2BANISHED\2.", ChanServ, from,
			  args[1]);
		sSend(":%s GLOBOPS :%s set banish on %s.", ChanServ, from,
			  args[1]);
		chanlog->log(nick, CS_BANISH, chan->name);
		return RET_OK_DB;
	}

	if (!strcasecmp(args[2], "on") || !strcasecmp(args[2], "yes")
		|| !strcasecmp(args[2], "banish")) {
		sSend(":%s NOTICE %s :%s is now \2BANISHED\2.", ChanServ, from,
			  args[1]);
		sSend(":%s GLOBOPS :%s set banish on %s.", ChanServ, from,
			  args[1]);
		chan->flags |= CBANISH;
		chanlog->log(nick, CS_BANISH, chan->name, LOGF_ON);
		return RET_OK_DB;
	} else if (!strcasecmp(args[2], "off") || !strcasecmp(args[2], "no")) {
		sSend(":%s NOTICE %s :%s is now \2not banished\2.", ChanServ, from,
			  args[1]);
		chan->flags &= ~CBANISH;
		chanlog->log(nick, CS_BANISH, chan->name, LOGF_OFF);
		return RET_OK_DB;
	} else {
		sSend(":%s NOTICE %s :Unrecognized usage.", ChanServ, from);
		sSend(":%s NOTICE %s :BANISH <#channel> [<on|off>]", ChanServ,
			  from);
		return RET_SYNTAX;
	}
	return RET_OK;
}


/*--------------------------------------------------------------------*/

/**
 * \cscmd Close
 * \syntax Close \<channel>
 * \syntax Close \<channel> close
 * \syntax Close \<channel> reopen
 * \plr Close down/reopen a channel
 * -Mysid
 */
CCMD(cs_close)
{
	char *from = nick->nick;
	RegChanList *chan;

	if (!isServop(nick) && !opFlagged(nick, OOPER | OCBANDEL)) {
		sSend(":%s NOTICE %s :Access denied", ChanServ, from);
		return RET_NOPERM;
	}

	if (numargs < 2) {
		sSend(":%s NOTICE %s :Must supply at least one argument", ChanServ,
			  from);
		return RET_SYNTAX;
	}

	chan = getRegChanData(args[1]);
	if (chan == NULL) {
		sSend(":%s NOTICE %s :Channel not found", ChanServ, from);
		return RET_NOTARGET;
	}

	if (numargs < 3) {
		sSend(":%s NOTICE %s :%s is \2%s\2.", ChanServ, from, args[1],
			  chan->flags & CHOLD ? "Closed" : "Not closed");
		return RET_OK;
	}

	if (!strcasecmp(args[2], "on") || !strcasecmp(args[2], "yes")
		|| !strcasecmp(args[2], "close")) {
		sSend(":%s NOTICE %s :%s is now \2CLOSED\2.", ChanServ, from,
			  args[1]);
		sSend(":%s GLOBOPS :%s closed channel %s.", ChanServ, from,
			  args[1]);
		chan->flags |= CCLOSE;
		chanlog->log(nick, CS_CLOSE, chan->name, LOGF_ON);

		return RET_OK_DB;
	} else if (!strcasecmp(args[2], "off") || !strcasecmp(args[2], "no")) {
		sSend(":%s NOTICE %s :%s is now \2OPEN\2.", ChanServ, from,
			  args[1]);
		chan->flags &= ~CCLOSE;
		chanlog->log(nick, CS_CLOSE, chan->name, LOGF_OFF);

		return RET_OK_DB;
	} else {
		sSend(":%s NOTICE %s :Unrecognized usage.", ChanServ, from);
		sSend(":%s NOTICE %s :CLOSE <#channel> [<on|off>]", ChanServ,
			  from);
		return RET_OK_DB;
	}
}

/*--------------------------------------------------------------------*/

/**
 * \cscmd Hold
 * \syntax Hold \<channel> {"" | "On" | "Off"}
 */
/**
 * @brief Hold/unhold a channel from expiration
 * \smysid
 */
CCMD(cs_hold)
{
	char *from = nick->nick;
	RegChanList *chan;

	if (!isRoot(nick)) {
		sSend(":%s NOTICE %s :Access denied", ChanServ, from);
		return RET_NOPERM;
	}

	if (numargs < 2) {
		sSend(":%s NOTICE %s :Must supply at least one argument", ChanServ,
			  from);
		return RET_SYNTAX;
	}

	chan = getRegChanData(args[1]);
	if (chan == NULL) {
		sSend(":%s NOTICE %s :Channel not found", ChanServ, from);
		return RET_NOTARGET;
	}
	if (numargs < 3) {
		sSend(":%s NOTICE %s :%s is \2%s\2.", ChanServ, from, args[1],
			  chan->flags & CHOLD ? "Held" : "Not Held");
		return RET_OK;
	}
	if (!strcasecmp(args[2], "on") || !strcasecmp(args[2], "yes")) {
		sSend(":%s NOTICE %s :%s is now \2Held\2.", ChanServ, from,
			  args[1]);
		chan->flags |= CHOLD;
		chanlog->log(nick, CS_HOLD, chan->name, LOGF_ON);
		return RET_OK_DB;
	} else if (!strcasecmp(args[2], "off") || !strcasecmp(args[2], "no")) {
		sSend(":%s NOTICE %s :%s is now \2Held\2.", ChanServ, from,
			  args[1]);
		chan->flags &= ~CHOLD;
		chanlog->log(nick, CS_HOLD, chan->name, LOGF_OFF);
		return RET_OK_DB;
	} else {
		sSend(":%s NOTICE %s :Unrecognized usage.", ChanServ, from);
		sSend(":%s NOTICE %s :HOLD <#channel> <on|off>", ChanServ, from);
		return RET_SYNTAX;
	}
	return RET_OK;
}

/*--------------------------------------------------------------------*/

/**
 * \cscmd Mark
 * \syntax Mark \<channel>
 * \plr View mark flag
 *
 * \syntax Mark \<channel> on
 * \plr Mark a channel
 *
 * \syntax Close \<channel> off
 * \plr Unmark a channel
 *
 * \plr Grp is blocked for a marked channel.  The expectation
 * is that if a channel is marked, an oper assisting with a lost
 * password should first check the services logs.  In general,
 * any mark should have a /cs log message indicating its reason
 * and purpose.
 * \plr -Mysid
 */
/**
 * @brief Mark/unmark a channel (ServOp command)
 * \smysid
 */
CCMD(cs_mark)
{
	char *from = nick->nick;
	RegChanList *chan;

	if (!isServop(nick)) {
		sSend(":%s NOTICE %s :Access denied", ChanServ, from);
		return RET_NOPERM;
	}

	if (numargs < 2) {
		sSend(":%s NOTICE %s :Must supply at least one argument", ChanServ,
			  from);
		return RET_SYNTAX;
	}

	chan = getRegChanData(args[1]);
	if (chan == NULL) {
		sSend(":%s NOTICE %s :Channel not found", ChanServ, from);
		return RET_NOTARGET;
	}
	if (numargs < 3) {
		sSend(":%s NOTICE %s :%s is \2%s\2.", ChanServ, from, args[1],
			  chan->flags & CMARK ? "Marked" : "Not Marked");
		return RET_OK;
	}
	if (!strcasecmp(args[2], "on") || !strcasecmp(args[2], "yes")) {
		if (chan->flags & CMARK) {
			if (chan->markby)
				sSend(":%s NOTICE %s :%s has already been marked by %s.",
					  ChanServ, from, args[1], chan->markby);
			else
				sSend(":%s NOTICE %s :%s has already been marked.",
					  ChanServ, from, args[1]);
			return RET_OK;
		}
		sSend(":%s NOTICE %s :%s is now \2Marked\2.", ChanServ, from,
			  args[1]);
		chan->flags |= CMARK;
		if (chan->markby)
			FREE(chan->markby);
		chan->markby = strdup(nick->nick);
		chanlog->log(nick, CS_MARK, chan->name, LOGF_ON);
		return RET_OK_DB;
	} else if (!strcasecmp(args[2], "off") || !strcasecmp(args[2], "no")) {
		sSend(":%s NOTICE %s :%s is now \2Unmarked\2.", ChanServ, from,
			  args[1]);
		chan->flags &= ~CMARK;
		chanlog->log(nick, CS_MARK, chan->name, LOGF_OFF);
		if (chan->markby)
			FREE(chan->markby);
		chan->markby = NULL;
		return RET_OK_DB;
	} else {
		sSend(":%s NOTICE %s :Unrecognized usage.", ChanServ, from);
		sSend(":%s NOTICE %s :MARK <#channel> <on|off>", ChanServ, from);
		return RET_SYNTAX;
	}
	return RET_OK;
}

/*--------------------------------------------------------------------*/

/**
 * \cscmd Clist
 * \plr Channel members list (/CS CLIST #<channel>)
 * +L services access required.
 * \plr -Mysid
 */
CCMD(cs_clist)
{
	char *from = nick->nick;
	char tmpch[IRCBUF] = "";
	ChanList *chan;
	cNickList *tmp;
	UserList *tnick;
	int c;

	if (numargs < 2) {
		sSend(":%s NOTICE %s :Must specify a channel name.", ChanServ,
			  from);
		return RET_SYNTAX;
	}

	chan = getChanData(args[1]);

	if (!opFlagged(nick, OOPER) && (!chan || !chan->reg || (getChanOpId(chan->reg, from) < SOP))) {
		sSend(":%s NOTICE %s :Access denied", ChanServ, from);
		return RET_NOPERM;
	}

	tnick = getNickData(args[1]);

	if (chan == NULL) {
		if (!tnick || !isRoot(nick)) {
			sSend(":%s NOTICE %s :Channel not found", ChanServ, from);
			return RET_NOTARGET;
		}

		/* Another desync detection mechanism */
		else {
			cNickList *cu;
			int i;

			/* Note: This is online data not database data */
			sSend(":%s NOTICE %s :*** %s [%s@%s]", ChanServ, from,
				  tnick->nick, tnick->user, tnick->host);
			sSend(":%s NOTICE %s :ID: %ld, TS: %ld, LT: %ld", ChanServ,
				  from, tnick->idnum.getHashKey(), tnick->timestamp, tnick->floodlevel.GetLev());
			sSend(":%s NOTICE %s :OF: %d, FL: %d, CA: %d", ChanServ, from,
				  tnick->oflags, tnick->floodlevel.GetLev(), tnick->caccess);

			for (i = 0; i < NICKCHANHASHSIZE; i++) {
				if (tnick->chan[i]) {
					if ((cu = getChanUserData(tnick->chan[i], tnick))) {
						if ((cu->op & CHANVOICE))
							strcat(tmpch, "+");
						if ((cu->op & CHANOP))
							strcat(tmpch, "@");
					}
					strncat(tmpch, tnick->chan[i]->name, CHANLEN);
					strcat(tmpch, "  ");
					if (strlen(tmpch) > 80) {
						sSend(":%s NOTICE %s :%s", ChanServ, from, tmpch);
						tmpch[0] = '\0';
					}
				}
			}
			if (*tmpch != '\0')
				sSend(":%s NOTICE %s :%s", ChanServ, from, tmpch);
			return RET_OK;
		}
	}


	if (!opFlagged(nick, OVERRIDE | OROOT))
		if (chan->modes & (PM_S | PM_I)) {
			for (tmp = chan->firstUser; tmp; tmp = tmp->next)
				if (tmp->person == nick)
					break;
			if (!tmp) {
				if (!opFlagged(nick, OROOT))
					sSend(":%s NOTICE %s :Channel not found", ChanServ,
						  from);
				else {
					sSend
						(":%s NOTICE %s :That channel is secret (+s) or private (+p)",
						 ChanServ, from);
					sSend
						(":%s NOTICE %s :Use the %s OVERRIDE command to override.",
						 ChanServ, from, OperServ);
				}
				return RET_FAIL;
			}
		}

	//for (tmp = chan->firstUser; tmp; tmp = tmp->next) {
	//	sSend(":%s NOTICE %s :%s -> %s (%i)", ChanServ, from, chan->name,
	//			  tmp->person->nick, tmp->op);
	//
	//}

	tmpch[0] = '\0';
	c = 0;

	sSend(":%s NOTICE %s :Channel membership for \2%s\2:", ChanServ, from, chan->name);
	for (tmp = chan->firstUser; tmp; tmp = tmp->next) {
		c += sprintf(tmpch+c, "%.1s%.1s%.1s%-18.30s",
		             *tmpch ? " " : "",
		             tmp->op & CHANVOICE ? "+" : "",
		             tmp->op & CHANOP ? "@" : "",
		             tmp->person->nick);
		if (c >= 80) {
			sSend(":%s NOTICE %s :%s", ChanServ, from, tmpch);
			c = 0;
			tmpch[0] = '\0';
		}	
	}

	if (tmpch[0] != '\0')
		sSend(":%s NOTICE %s :%s", ChanServ, from, tmpch);
	sSend(":%s NOTICE %s :End of channel member list.", ChanServ, from);
	return RET_OK;
}

/*--------------------------------------------------------------------*/

/**
 * \cscmd Whois
 * \plr List channels a user is on
 * -Mysid
 */
CCMD(cs_whois)
{
	char *from = nick->nick;
	UserList *tnick;
	char tmpch[(CHANLEN + 4) * NICKCHANHASHSIZE] = "";
	cNickList *c;
	int i;

	if (!opFlagged(nick, OOPER | OSERVOP)) {
		sSend(":%s NOTICE %s :Access denied", ChanServ, from);
		return RET_NOPERM;
	}

	if (numargs != 2) {
		sSend(":%s NOTICE %s :Must supply at least one argument", ChanServ,
			  from);
		return RET_SYNTAX;
	}

	tnick = getNickData(args[1]);

	if (!tnick) {
		sSend(":%s NOTICE %s :Nick not found", ChanServ, from);
		return RET_NOTARGET;
	}

	sSend(":%s GLOBOPS :%s uses CWHOIS on %s", ChanServ, from, args[1]);
	operlog->log(nick, CS_WHOIS, fullhost1(tnick));

	sSend(":%s NOTICE %s :*** %s [%s@%s]", ChanServ, from, tnick->nick,
		  tnick->user, tnick->host);
	for (i = 0; i < NICKCHANHASHSIZE; i++) {
		if (tnick->chan[i]) {
			if ((c = getChanUserData(tnick->chan[i], tnick))) {
				if ((c->op & CHANVOICE))
					strcat(tmpch, "+");
				if ((c->op & CHANOP))
					strcat(tmpch, "@");
			}
			strncat(tmpch, tnick->chan[i]->name, CHANLEN);
			strcat(tmpch, "  ");
			if (strlen(tmpch) > 80) {
				sSend(":%s NOTICE %s :%s", ChanServ, from, tmpch);
				tmpch[0] = '\0';
			}
		}
	}
	if (tmpch[0])
		sSend(":%s NOTICE %s :%s", ChanServ, from, tmpch);
	return RET_OK;
}

/*--------------------------------------------------------------------*/

/**
 * \cscmd Restrict
 * \plr Change channel restrict level
 */
CCMD(cs_restrict)
{
	char *from = nick->nick;
	RegChanList *regchan;

	if (numargs < 2) {
		sSend(":%s NOTICE %s :Too few arguments to RESTRICT command",
			  ChanServ, from);
		return RET_SYNTAX;
	}

	regchan = getRegChanData(args[1]);

	if (!regchan) {
		sSend(":%s NOTICE %s :%s is not registered", ChanServ, from,
			  args[1]);
		return RET_NOTARGET;
	}

	return cs_set_restrict(NULL, nick, regchan, args, numargs);
}

/*--------------------------------------------------------------------*/

/**
 * \cscmd Topiclock
 * \plr Change channel topiclock
 */
CCMD(cs_topiclock)
{
	char *from = nick->nick;
	RegChanList *regchan;

	if (numargs < 2) {
		sSend(":%s NOTICE %s :Too few arguments to TOPICLOCK command",
			  ChanServ, from);
		return RET_SYNTAX;
	}

	regchan = getRegChanData(args[1]);

	if (!regchan) {
		sSend(":%s NOTICE %s :%s is not registered", ChanServ, from,
			  args[1]);
		return RET_NOTARGET;
	}

	return cs_set_topiclock(NULL, nick, regchan, args, numargs);
}

/*--------------------------------------------------------------------*/

/**
 * \cscmd Mlock
 * \plr Change channel modelock
 */
CCMD(cs_modelock)
{
	char *from = nick->nick;
	RegChanList *regchan;

	if (numargs < 3) {
		sSend(":%s NOTICE %s :Too few arguments to MODELOCK command",
			  ChanServ, from);
		sSend
			(":%s NOTICE %s :For usage information, type \2/CHANSERV HELP MLOCK\2",
			 ChanServ, from);
		return RET_SYNTAX;
	}

	regchan = getRegChanData(args[1]);

	if (!regchan) {
		sSend(":%s NOTICE %s :%s is not registered", ChanServ, from,
			  args[1]);
		return RET_NOTARGET;
	}

	if (MFOUNDER > getChanOpId(regchan, from)) {
		sSend(":%s NOTICE %s :Access denied", ChanServ, from);
		return RET_NOPERM;
	}

	return cs_set_mlock(NULL, nick, regchan, args, numargs);
}

/*--------------------------------------------------------------------*/

/** \def CSO
 *  \brief /ChanServ Set related macro
 *  This is a hack to allow for generalized ChanServ set types
 *  such as cs_set_bool and cs_set_string.
 */
#define CSO(q)	(((size_t) &((RegChanList *)0)->q))

/**
 * \cscmd Set
 * \plr Change channel configuration options
 * -Mysid
 */
CCMD(cs_set)
{
	char *from = nick->nick;
	RegChanList *chan;
	ChanList *tmp;
	cs_settbl_t *cmd;

	cs_settbl_t cs_setcmd[] = {
		/* string       function */
		{"opguard", cs_set_bool, FOUNDER, CSO(flags), "op guard",
		 COPGUARD},
		{"protop", cs_set_bool, FOUNDER, CSO(flags), "protop", CPROTOP},
		{"keeptopic", cs_set_bool, FOUNDER, CSO(flags), "topic holding",
		 CKTOPIC},
		{"ident", cs_set_bool, FOUNDER, CSO(flags), "ident", CIDENT},
		{"quiet", cs_set_bool, FOUNDER + 1, CSO(flags), "quiet changes",
		 CQUIET},
		{"gs_roll", cs_set_bool, FOUNDER, CSO(flags), "+v gameserv roll", CGAMESERV},
		{"akickreasons", cs_set_bool, FOUNDER, CSO(flags), "use akick reasons", CAKICKREASONS},
		{"passw*d", cs_set_passwd, FOUNDER + 1},
		{"memo*", cs_set_memolvl, FOUNDER},
		{"desc*", cs_set_fixed, FOUNDER, CSO(desc), "description",
		 CHANDESCBUF},
		{"founder", cs_set_founder, 0, 0},
		{"autogreet", cs_set_string, FOUNDER, CSO(autogreet), NULL, 255},
		{"url", cs_set_string, FOUNDER, CSO(url), NULL, 255},
		{"mlock", cs_set_mlock, FOUNDER, 0},
		{"restrict", cs_set_restrict, MFOUNDER, 0},
		{"topiclock", cs_set_topiclock, MFOUNDER, 0},
		{"encrypt", cs_set_encrypt, 0, 0},
		{NULL}
	};

	if (numargs < 2) {
		sSend
			(":%s NOTICE %s :You must specify arguments after the SET command",
			 ChanServ, from);
		return RET_SYNTAX;
	}

	chan = getRegChanData(args[1]);
	tmp = getChanData(args[1]);

	if (chan == NULL) {
		sSend(":%s NOTICE %s :%s is not registered", ChanServ, from,
			  args[1]);
		return RET_NOTARGET;
	}

	if (!isOper(nick) && (chan->flags & CFORCEXFER)) {
		sSend(":%s NOTICE %s :services for %s have been temporarily "
		      "suspended.", ChanServ, from,
			  args[1]);
		return RET_NOTARGET;
	}

	if (tmp && (tmp->reg != chan)) {	/* Sanity check */
		logDump(corelog,
				"WARNING! tmp->reg != getRegChanData(tmp->name) (%s) (%s)",
				tmp->name, chan->name);
		tmp->reg = chan;
	}

	if (isOper(nick) && opFlagged(nick, ODMOD | OVERRIDE)) {
		sSend(":%s GLOBOPS :%s using directmod on %s with set '%s'",
			  ChanServ, nick->nick, chan->name, args[2]);
	}

	for (cmd = cs_setcmd; cmd->cmd != NULL; cmd++) {
		if (!match(cmd->cmd, args[2])) {


			if ((cmd->aclvl == (FOUNDER + 3))
				&& !opFlagged(nick, OOPER | ODMOD | OVERRIDE)) {
				sSend(":%s NOTICE %s :Option not available.\r\n"
					  ":%s NOTICE %s :Please try /msg %s HELP", ChanServ,
					  from, ChanServ, from, ChanServ);
				return RET_NOPERM;
			} else if ((cmd->aclvl == (FOUNDER + 2)) && !isRoot(nick))
				continue;
			else if ((cmd->aclvl == (FOUNDER + 1))
					 && !opFlagged(nick, OOPER | ODMOD | OVERRIDE)) {
				if (!isFounder(chan, nick)) {
					sSend
						(":%s NOTICE %s :You must identify as channel founder with the channel password to alter the value of that setting.",
						 ChanServ, from);
					return RET_NOPERM;
				}
			} else if (!opFlagged(nick, OOPER | ODMOD | OVERRIDE)
					   && (cmd->aclvl > getChanOpId(chan, from))) {
				sSend(":%s NOTICE %s :Access denied.", ChanServ, from);
				return RET_NOPERM;
			}

			return cmd->func(cmd, nick, chan, args + 1, numargs - 1);
		}
	}

	sSend(":%s NOTICE %s :Unknown SET item.\r\n"
		  ":%s NOTICE %s :Please try /msg %s HELP", ChanServ, from,
		  ChanServ, from, ChanServ);
	return RET_EFAULT;
}

/**
 * Generic: /CS SET <channel> <string option> <string|*>
 */
cmd_return cs_set_string(cs_settbl_t * cmd, UserList * nick,
						 RegChanList * regchan, char **args, int numargs)
{
	char *from = nick->nick, **str_target;
	char tmpbuf[IRCBUF + 1];
	int i;

	if (!regchan)
		return RET_OK;
	str_target = (char **)(((regchan->name) + (cmd->xoff)));
	*tmpbuf = '\0';

	if (numargs == 3 && !strcasecmp(args[2], "*")) {
		sSend(":%s NOTICE %s :The %s of %s is now unspecified.", ChanServ,
			  from, cmd->optname ? cmd->optname : cmd->cmd, regchan->name);
		if (str_target)
			FREE((*str_target));
		(*str_target) = NULL;
		return RET_OK_DB;
	}

	if (numargs >= 3) {
		for (i = 2; i < numargs; i++) {
			if (strlen(tmpbuf) + strlen(args[i]) >= cmd->optlen)
				break;
			strcat(tmpbuf, args[i]);
			if (i + 1 < numargs)
				strcat(tmpbuf, " ");
		}

		if (i < numargs && !*tmpbuf) {
			sSend(":%s NOTICE %s :Text line too long (must be %lu or shorter).",
				 ChanServ, from, cmd->optlen);
			return RET_EFAULT;
		}

		if ((*str_target))
			FREE((*str_target));
		(*str_target) = NULL;

		(*str_target) = (char *)oalloc(strlen(tmpbuf) + 1);
		strcpy((*str_target), tmpbuf);
		sSend(":%s NOTICE %s :The %s of %s is now: %s", ChanServ, from,
			  cmd->optname ? cmd->optname : cmd->cmd, regchan->name,
			  (*str_target));
		return RET_OK_DB;
	}

	if (*str_target)
		sSend(":%s NOTICE %s :The %s of %s is: %s", ChanServ, from,
			  cmd->optname ? cmd->optname : cmd->cmd, regchan->name,
			  (*str_target));
	else
		sSend(":%s NOTICE %s :The %s of %s is empty.", ChanServ, from,
			  cmd->optname ? cmd->optname : cmd->cmd, regchan->name);
	return RET_OK;
}

/**
 * @brief Generic set for a string of fixed length
 */
cmd_return cs_set_fixed(cs_settbl_t * cmd, UserList * nick,
						RegChanList * regchan, char **args, int numargs)
{
	char *from = nick->nick, *str_target;
	int i;

	if (!regchan || !cmd->optlen)
		return RET_FAIL;
	str_target = (char *)((regchan->name) + (cmd->xoff));

	if (numargs == 3 && !strcasecmp(args[2], "*") && (!cmd->optname || strcmp(cmd->optname, "description"))) {
		sSend(":%s NOTICE %s :The %s of %s is now unspecified.", ChanServ,
			  from, cmd->optname ? cmd->optname : cmd->cmd, regchan->name);
		(*str_target) = '\0';
		return RET_OK_DB;
	}

	if (numargs >= 3) {
		(*str_target) = '\0';

		for (i = 2; i < numargs; i++) {
			if (strlen((str_target)) + strlen(args[i]) >= cmd->optlen)
				break;
			strcat((str_target), args[i]);
			if (i + 1 < numargs)
				strcat((str_target), " ");
		}

		if (i < numargs && !*str_target)
			sSend(":%s NOTICE %s :Text line too long (must be %lu or shorter).",
				 ChanServ, from, cmd->optlen);
		sSend(":%s NOTICE %s :The %s of %s is now: %s", ChanServ, from,
			  cmd->optname ? cmd->optname : cmd->cmd, regchan->name,
			  str_target);
		return RET_OK_DB;
	}
	sSend(":%s NOTICE %s :The %s of %s is: %s", ChanServ, from,
		  cmd->optname ? cmd->optname : cmd->cmd, regchan->name,
		  (str_target));
	return RET_OK;
}

/**
 * @brief Generic set for an on|off flag setting
 */
cmd_return cs_set_bool(cs_settbl_t * cmd, UserList * nick,
					   RegChanList * regchan, char **args, int numargs)
{
	long *long_target;
	char *from = nick->nick;
	ChanList *chan;

	if (!regchan || !cmd->optlen)
		return RET_FAIL;
	long_target = (long *)(((regchan->name) + (cmd->xoff)));
	chan = getChanData(regchan->name);

	if (numargs < 3) {
		sSend(":%s NOTICE %s :For %s, %s is currently %s.", ChanServ, from,
			  regchan->name, cmd->optname ? cmd->optname : cmd->cmd,
			  ((*long_target) & cmd->optlen) ? "ON" : "OFF");
		return RET_OK;
	}

	if (!strcasecmp(args[2], "ON")) {
		(*long_target) |= cmd->optlen;
		sSend(":%s NOTICE %s :For %s, %s is now \2on\2.", ChanServ, from,
			  regchan->name, cmd->optname ? cmd->optname : cmd->cmd);
		if (chan)
			sendToChanOps(chan, "%s set %s on.", from,
						  cmd->optname ? cmd->optname : cmd->cmd);
		return RET_OK_DB;
	} else if (!strcasecmp(args[2], "OFF")) {
		(*long_target) &= ~cmd->optlen;
		sSend(":%s NOTICE %s :For %s, %s is now \2off\2.", ChanServ, from,
			  regchan->name, cmd->optname ? cmd->optname : cmd->cmd);
		if (chan)
			sendToChanOps(chan, "%s set %s off.", from,
						  cmd->optname ? cmd->optname : cmd->cmd);
		return RET_OK_DB;
	} else {
		sSend(":%s NOTICE %s :For %s, %s is currently %s.", ChanServ, from,
			  regchan->name, cmd->optname ? cmd->optname : cmd->cmd,
			  ((*long_target) & cmd->optlen) ? "ON" : "OFF");
		return RET_OK;
	}
	return RET_OK;
}

/**
 * Set for a channel restrict level
 */
cmd_return cs_set_restrict(cs_settbl_t * cmd, UserList * nick,
						   RegChanList * regchan, char **args, int numargs)
{
	char *from = nick->nick;
	ChanList *tmpchan;
	int reslevel;

	if (regchan == NULL) {
		sSend(":%s NOTICE %s :%s is not registered", ChanServ, from,
			  args[1]);
		return RET_NOTARGET;
	}

	tmpchan = getChanData(regchan->name);

	if (numargs < 3) {
		if (regchan->restrictlevel)
			sSend
				(":%s NOTICE %s :Access to \2%s\2 currently restricted to %s.",
				 ChanServ, from, regchan->name,
				 opLevelName(regchan->restrictlevel, 2));
		else
			sSend
				(":%s NOTICE %s :Restricted mode for \2%s\2 is currently \2OFF\2.",
				 ChanServ, from, regchan->name);
		return RET_SYNTAX;
	}

	if (MFOUNDER > getChanOp(regchan, from)) {
		sSend(":%s NOTICE %s :Access denied", ChanServ, from);
		return RET_NOPERM;
	}

	reslevel = opNameLevel(args[2]);

	if (reslevel == -1) {
		sSend(":%s NOTICE %s :No such level.", ChanServ, from);
		return RET_NOPERM;
	}

	if (reslevel > getChanOp(regchan, from)) {
		sSend(":%s NOTICE %s :Access denied", ChanServ, from);
		return RET_NOPERM;
	}

	regchan->restrictlevel = reslevel;

	if (tmpchan)
		sendToChanOps(tmpchan, "%s has set restriction to %i+ levels",
					  from, reslevel);
	if (reslevel)
		sSend(":%s NOTICE %s :%s is now restricted to %s level.", ChanServ,
			  from, regchan->name, opLevelName(reslevel, 2));
	else
		sSend(":%s NOTICE %s :%s is no longer in restricted mode.",
			  ChanServ, from, regchan->name);
	return RET_OK_DB;
}

/**
 * Set for channel topiclock level
 */
cmd_return cs_set_topiclock(cs_settbl_t * cmd, UserList * nick,
							RegChanList * regchan, char **args,
							int numargs)
{
	char *from = nick->nick;
	ChanList *tmpchan;
	int reslevel;

	if (regchan == NULL) {
		sSend(":%s NOTICE %s :%s is not registered", ChanServ, from,
			  args[1]);
		return RET_NOTARGET;
	}

	tmpchan = getChanData(regchan->name);

	if (numargs < 3) {
		if (regchan->tlocklevel)
			sSend
				(":%s NOTICE %s :Topic changing for \2%s\2 currently restricted to %s.",
				 ChanServ, from, regchan->name,
				 opLevelName(regchan->restrictlevel, 2));
		else
			sSend
				(":%s NOTICE %s :The topic restriction for \2%s\2 is currently \2OFF\2.",
				 ChanServ, from, regchan->name);
		return RET_SYNTAX;
	}

	if (MFOUNDER > getChanOp(regchan, from)) {
		sSend(":%s NOTICE %s :Access denied", ChanServ, from);
		return RET_NOPERM;
	}

	reslevel = opNameLevel(args[2]);

	if (reslevel == -1) {
		sSend(":%s NOTICE %s :No such level.", ChanServ, from);
		return RET_NOPERM;
	}

	if (reslevel > getChanOp(regchan, from)) {
		sSend(":%s NOTICE %s :Access denied", ChanServ, from);
		return RET_NOPERM;
	}

	regchan->tlocklevel = reslevel;

	if (tmpchan)
		sendToChanOps(tmpchan, "%s has set topiclock to %i+ levels", from,
					  reslevel);
	if (reslevel)
		sSend(":%s NOTICE %s :Topic restriction for %s is now %s+.",
			  ChanServ, from, regchan->name, opLevelName(reslevel, 2));
	else
		sSend(":%s NOTICE %s :Topic for %s is no longer restricted.",
			  ChanServ, from, regchan->name);
	return RET_OK_DB;
}

/**
 * Encrypt/decrypt a channel password
 */
cmd_return cs_set_encrypt(cs_settbl_t * cmd, UserList * nick,
							RegChanList * regchan, char **args,
							int numargs)
{
	char *from = nick->nick;
	int do_enc = 0;

	if (numargs < 3) {
		sSend(":%s NOTICE %s :Password for %s is %scurrently encrypted.",
			ChanServ, from, regchan->name, (regchan->flags & CENCRYPT) ? "" : "not ");
		return RET_OK_DB;
	}

	if (!str_cmp(args[2], "ON") || !str_cmp(args[2], "ENCRYPT") ||
	    !str_cmp(args[2], "YES"))
		do_enc = 1;
	else if (!str_cmp(args[2], "OFF") || !str_cmp(args[2], "UNENCRYPT") ||
		 !str_cmp(args[2], "NO"))
		do_enc = 0;
	else {
		sSend(":%s NOTICE %s :Invalid setting.  Must be ON or OFF.",
		      ChanServ, from);
	}

	if (numargs < 4) {
		sSend(":%s NOTICE %s :Password required to change this option.",
			ChanServ, from);
		return RET_OK_DB;
	}

	if (!Valid_pw(args[3], regchan->password, ChanGetEnc(regchan))) {
		PutReply(ChanServ, nick, ERR_BADPW, 0, 0, 0);
		if (BadPwChan(nick, regchan))
			return RET_KILLED;
		return RET_BADPW;
	}
	if (do_enc) {
		sSend(":%s NOTICE %s :Password for %s is now encrypted "
		      "within services.", ChanServ, from, regchan->name);
		GoodPwChan(nick, regchan);
		regchan->flags |= CENCRYPT;
	}
	else {
		sSend(":%s NOTICE %s :Password for %s is no longer "
		      "encrypted within services.",
			ChanServ, from, args[1]);
		regchan->flags &= ~CENCRYPT;
	}

	pw_enter_password(args[3], regchan->password, ChanGetEnc(regchan));

	return RET_OK_DB;
}

/**
 * Set a channel mode lock
 */
cmd_return cs_set_mlock(cs_settbl_t * cmd, UserList * nick,
						RegChanList * regchan, char **args, int numargs)
{
	struct
	{
		char character;
		int plus, minus;
	} mlock_table[] =
	{
		{
		'i', PM_I, MM_I}
		, {
		'm', PM_M, MM_M}
		, {
		'n', PM_N, MM_N}
		, {
		's', PM_S, MM_S}
		, {
		'p', PM_P, MM_P}
		, {
		't', PM_T, MM_T}
		, {
		'H', PM_H, MM_H}
		, {
		'c', PM_C, MM_C}
		, {
		'\0', 0, 0}
	};

	int onargs = 3;
	int on = 1;
	int reset = 0;
	u_int i, j, klen;
	char mode[20], parastr[IRCBUF * 2];
	char *from = nick->nick;

	ChanList *tmpchan;

	if (!regchan)
		return RET_FAIL;
	tmpchan = getChanData(regchan->name);

	if (regchan == NULL) {
		sSend(":%s NOTICE %s :%s is not registered", ChanServ, from,
			  args[1]);
		return RET_FAIL;
	}

	if (numargs < 3) {
		makeModeLockStr(regchan, mode);
		sSend
			(":%s NOTICE %s :The modelock string for %s is currently \2%s\2",
			 ChanServ, from, regchan->name, mode);
		return RET_OK;
	}

	if (MFOUNDER > getChanOp(regchan, from)) {
		sSend(":%s NOTICE %s :Access denied", ChanServ, from);
		return RET_NOPERM;
	}

	klen = strlen(args[2]);

	for (i = 0; i < klen; i++) {
		switch (args[2][i]) {
		default:
			for (j = 0; mlock_table[j].character; j++)
				if (mlock_table[j].character == args[2][i])
					break;
			if (mlock_table[j].character) {
				if (on) {
					regchan->mlock |= mlock_table[j].plus;
					regchan->mlock &= ~mlock_table[j].minus;
				} else {
					regchan->mlock |= mlock_table[j].minus;
					regchan->mlock &= ~mlock_table[j].plus;
				}
			}
			break;
		case '=':
			if (!reset) {
				regchan->mlock = 0;
				reset = 1;
				break;
			}
		case '+':
			on = 1;
			break;
		case '-':
			on = 0;
			break;
		case 'k':
			if (on) {
				if (numargs < onargs + 1)
					sSend(":%s NOTICE %s :+k not set, no key specified",
						  ChanServ, from);
				else {
					strncpyzt(regchan->key, args[onargs], KEYLEN-1);
					regchan->mlock |= PM_K;
					regchan->mlock &= ~MM_K;
					onargs++;
				}
			} else {
				regchan->mlock |= MM_K;
				regchan->mlock &= ~PM_K;
			}
			break;
		case 'l':
			if (on) {
				if (numargs < onargs + 1)
					sSend(":%s NOTICE %s :+l not set, no key specified",
						  ChanServ, from);
				else {
					regchan->limit = atoi(args[onargs]);
					regchan->mlock |= PM_L;
					regchan->mlock &= ~MM_L;
					onargs++;
				}
			} else {
				regchan->mlock |= MM_L;
				regchan->mlock &= ~PM_L;
			}
			break;
		}
	}

	makeModeLockStr(regchan, mode);
	sendToChanOps(getChanData(regchan->name), "%s set modelock %s", from,
				  mode);
	*parastr = '\0';
	if ((regchan->mlock & PM_L))
		sprintf(parastr + strlen(parastr), " %ld", regchan->limit);
	if ((regchan->mlock & PM_K) || (regchan->mlock & MM_K))
		sprintf(parastr + strlen(parastr), " %s", regchan->key
				&& *regchan->key ? regchan->key : "*");
	sSend(":%s MODE %s %s%s", ChanServ, regchan->name, mode, parastr);
#ifdef IRCD_MLOCK
	sSend(":%s MLOCK %s %s%s", ChanServ, regchan->name, mode, parastr);
#endif

	sSend(":%s NOTICE %s :Mode for %s is now locked to %s", ChanServ, from,
		  regchan->name, mode);
	return RET_OK_DB;
}

/**
 * Change a channel password
 */
/*ARGSUSED3*/
cmd_return cs_set_passwd(cs_settbl_t * cmd, UserList * nick,
						 RegChanList * regchan, char **args, int numargs)
{
	char *from = nick->nick;
	char pw_reason[IRCBUF];

	if (numargs < 3) {
		sSend
			(":%s NOTICE %s :Current password for %s cannot be shown for security reasons.",
			 ChanServ, from, regchan->name);
		sSend
			(":%s NOTICE %s :To change the password, type \2/CHANSERV SET <CHANNEL> PASSWORD <NEWPASS>\2",
			 ChanServ, from);
		sSend
			(":%s NOTICE %s :Or type \2/CHANSERV HELP SET PASSWORD\2 for more information",
			 ChanServ, from);
		return RET_OK;
	}

	if (!isFounder(regchan, nick))
		sSend
			(":%s NOTICE %s :You must identify as channel founder to change the channel password",
			 ChanServ, from);
	else {

                if (isPasswordAcceptable(args[2], pw_reason) == 0) {
                    sSend(":%s NOTICE %s :Sorry, %s isn't a password that you can use.",
                        ChanServ, from, args[1]);
                    sSend(":%s NOTICE %s :%s", ChanServ, from, pw_reason);
                    return RET_EFAULT;
                }

		/*! \bug 4, 15 should be symbolized */

		if (4 > strlen(args[2]) || 15 < strlen(args[2])) {
			sSend
				(":%s NOTICE %s :Please choose a password between 4 and 15 characters long",
				 ChanServ, from);
			return RET_EFAULT;
		} else {
			pw_enter_password(args[2], regchan->password, ChanGetEnc(regchan));
			clearChanIdent(regchan);

			sSend(":%s NOTICE %s :Password for %s is now %s", ChanServ,
				  from, regchan->name, args[2]);
			return RET_OK_DB;
		}
	}
	return RET_OK;
}

/**
 * Change channel founder  SET <CHANNEL> FOUNDER <PASSWORD>
 * [User must be identified to channel first]
 */
/*ARGSUSED3*/
cmd_return cs_set_founder(cs_settbl_t * cmd, UserList * nick,
						  RegChanList * regchan, char **args, int numargs)
{
	char *from = nick->nick;
	ChanList *chan = getChanData(regchan->name);

	if (numargs < 3) {
		const char *tmpName = regchan->founderId.getNick();

		if (tmpName == NULL)
			tmpName = "(none)";

		sSend(":%s NOTICE %s :Current founder of \2%s\2 is \2%s\2\2",
			  ChanServ, from, regchan->name, tmpName);
		return RET_OK;
	}

	if (numargs < 3) {
		sSend(":%s NOTICE %s :To set yourself as the founder, you "
			  "must specify the channel password.", ChanServ, from);
		return RET_SYNTAX;
	}

	if (!nick->reg) {
		sSend(":%s NOTICE %s :Your nickname, %s, is not registered.", ChanServ, from,
			  from);
		return RET_FAIL;
	}

	if (Valid_pw(args[2], regchan->password, ChanGetEnc(regchan))) {
		RegNickList *founderInfo;

		founderInfo = regchan->founderId.getNickItem();

		if (founderInfo != NULL)
			founderInfo->chans--;
		regchan->founderId = nick->reg->regnum;
		nick->reg->chans++;

		if (chan)
			sendToChanOps(chan, "The founder of %s is now %s",
						  regchan->name, from);
		sSend(":%s NOTICE %s :You are now the founder of %s", ChanServ,
			  from, regchan->name);
		GoodPwChan(nick, regchan);
		chanlog->log(nick, CS_SET_FOUNDER, chan->name, LOGF_OK);

		return RET_OK_DB;
	}
	if (chan)
		sendToChanOps(chan, "%s failed a SET FOUNDER attempt for %s", from,
					  chan->name);
	sSend(":%s NOTICE %s :Incorrect password", ChanServ, from);
	if (BadPwChan(nick, regchan))
		return RET_KILLED;
	chanlog->logw(nick, CS_SET_FOUNDER, chan->name, LOGF_BADPW);

	return RET_BADPW;
}

/**
 * Set for channel memo level
 */
/*ARGSUSED3*/
cmd_return cs_set_memolvl(cs_settbl_t * cmd, UserList * nick,
						  RegChanList * regchan, char **args, int numargs)
{
	char *from = nick->nick;
	int i;

	if (numargs < 3) {
		sSend(":%s NOTICE %s :Current memo level for \2%s\2 is \2%d\2.",
			  ChanServ, from, regchan->name, regchan->memolevel);
		return RET_SYNTAX;
	}

	i = atoi(args[2]);

	if (!((i >= MAOP) && (i <= FOUNDER))) {
		sSend(":%s NOTICE %s :You need to specify a valid level.",
			  ChanServ, from);
		return RET_SYNTAX;
	}

	regchan->memolevel = atoi(args[2]);
	sSend(":%s NOTICE %s :Channel memolevel is now %s", ChanServ, from,
		  args[2]);
	return RET_OK_DB;
}

/**
 * \cscmd Save
 * \plr This is a services root command.
 */
CCMD(cs_save)
{
	char *from = nick->nick;

	if (!isRoot(nick)) {
		sSend(":%s NOTICE %s :Access denied.", ChanServ, from);
		return RET_NOPERM;
	}

#ifdef GLOBOP_ON_SAVE
	sSend(":%s GLOBOPS :Saving database. (%s)", ChanServ, from);
#else
	sSend(":%s PRIVMSG " LOGCHAN " :Saving database. (%s)", ChanServ,
		  from);
#endif
	saveChanData(firstRegChan);
	return RET_OK_DB;
}

/*--------------------------------------------------------------------*/
/* Password retrievals */

/**
 * \cscmd Sendpass
 * \syntax sendpass \<channel> { options }
 * \plr Mail a channel password change authorization key to the e-mail address
 *      on the founder nickname.
 * \plr Options:
 *      - -transfer for transferring a channel to another user (+G required)
 *        authorization code is sent to the oper and founder access cannot
 *        be gained to the channel until the code is entered with setpass.
 *      - -resend re-sends the active pw auth change code
 *      - -new voids the old pw change auth code, makes a new one, and mails it
 * \bug Not mark-aware: an -override-mark option should be required
 *      to getpass a marked thing.
 */
CCMD(cs_getpass)
{
	char *from = nick->nick;
	const char *pwAuthKey;
	time_t doot = time(NULL);
	char buf[MAXBUF];
	int key_new, key_resend, key_transfer, i;
	RegNickList *regnick;
	RegChanList *chan;
	EmailMessage *email = NULL;

	key_new = key_resend = key_transfer = 0;

	if (numargs < 2) {
		sSend(":%s NOTICE %s :Must supply a channel name", ChanServ, from);
		return RET_SYNTAX;
	}

	/**
	 * \todo This stuff should be done using ``getopt_long_only''.
	 * Parse ChanServ getpass options
	 */
	if (numargs >= 3)
	{
		for(i = 2; i < numargs; i++)
		{
			if (*args[i] == '-')
			{
				if (!strcmp(args[i], "-new"))
					key_new = 1;
				else if (!strcmp(args[i], "-resend"))
					key_resend = 1;
				else if (!strcmp(args[i], "-transfer") &&
				         opFlagged(nick, OGRP) &&
					 !is_sn_chan(args[1]))
					key_transfer = 1;
				else if (args[i][1] == '-')
					break;
			}
			else
				continue;
		}
	}

	if (key_resend && key_new)
		key_resend = key_new = 0;

	if (isOper(nick) == 0) {
		sSend(":%s NOTICE %s :You are not currently an IRC Operator",
			  ChanServ, from);
		return RET_NOPERM;
	}

	chan = getRegChanData(args[1]);

	if (chan == NULL) {
		sSend(":%s NOTICE %s :%s is not registered", ChanServ, from,
			  args[1]);
		return RET_NOTARGET;
	}

	if ((chan->flags & CMARK) && !opFlagged(nick, OVERRIDE)) {
		sSend(":%s NOTICE %s :%s is marked: use /OperServ override chanserv getpass ...  to override", ChanServ, from, chan->name);
		return RET_FAIL;
	}

	regnick = chan->founderId.getNickItem();

	if (regnick == NULL) {
		sSend(":%s NOTICE %s :There is no founder on record for "
		      "the channel %s.",
			 ChanServ, from, args[1]);
		return RET_FAIL;
	}

	if (!key_transfer && (!regnick->email[0] || !strncasecmp(regnick->email, "(none)", 6))) {
		sSend
			(":%s NOTICE %s :The founder, %s, did not specify an e-mail address",
			 NickServ, from, regnick->nick);
		return RET_FAIL;
	}

	if (!key_transfer)
	{
		email = new EmailMessage;

		if (!email) {
			sSend(":%s WALLOPS :cs_getpass: fatal error: out of memory",
				  ChanServ);
			abort();
			return RET_EFAULT;
		}
	}

	if (!key_resend && !key_new && chan->chpw_key)
	{
		sSend(":%s NOTICE %s :A password change key has already been "
		      "issued for %s. Please specify -new or -resend.",
		      ChanServ, from, chan->name);
		return RET_FAIL;
	}
	else if (key_transfer || (chan->flags & CFORCEXFER)) {
		if (!chan->chpw_key || !key_resend)
			chan->chpw_key = lrand48();
		if (!chan->chpw_key)
			chan->chpw_key = 1;
		pwAuthKey = GetAuthChKey("-forced-transfer-",
					 PrintPass(chan->password, ChanGetEnc(chan)),
					 chan->timereg,
					 chan->chpw_key);
		if (key_transfer)
			sSend(":%s GLOBOPS :%s used getpass (transfer) on %s", ChanServ, from, chan->name);
		else 
			sSend(":%s GLOBOPS :%s used getpass (getkey) on %s", ChanServ, from, chan->name);
		sSend(":%s NOTICE %s :The change auth key for %s is: %s",
		      ChanServ, from, chan->name, pwAuthKey);
		operlog->log(nick, CSS_GETPASS_XFER, args[1]);
		chan->flags |= CFORCEXFER;
		clearChanIdent(chan);

		return RET_OK_DB;
	}
	else if ((chan->flags & CFORCEXFER) && !opFlagged(nick, OGRP | OOPER | OVERRIDE))
	{
		sSend(":%s NOTICE %s :This chan's pw key was marked ``channel transfer''",
		      ChanServ, from);
		return RET_FAIL;
	}


	if (!key_resend || key_new)
		key_new = key_resend = 0;

	sSend(":%s GLOBOPS :%s used sendpass on %s", myname, from, chan->name);
	sSend
		(":%s NOTICE %s :The \2password change key\2 has been sent to the email address: "
		 "%s", ChanServ, from, regnick->email);
	snprintf(buf, sizeof(buf), "%s Password Recovery <%s@%s>", ChanServ,
			 ChanServ, NETWORK);

	if (!chan->chpw_key || !key_resend || key_new) {
		chan->chpw_key = lrand48();
	}
	if (!chan->chpw_key)
		chan->chpw_key = 1;
	pwAuthKey = GetAuthChKey(regnick->email, PrintPass(chan->password, ChanGetEnc(chan)), chan->timereg,
	                         chan->chpw_key);

	email->from = buf;
	email->subject = "Password";
	email->to = regnick->email;
	sprintf(buf,
			"\nThe password change key for %.*s is %.*s\n"
			"\nType /CHANSERV SETPASS %.*s %.*s <password>\n",
			CHANLEN, chan->name,
			255, pwAuthKey,
			CHANLEN, chan->name,
			255, pwAuthKey
	);
	email->body = buf;


	email->body += "Or navigate to: "
		"http://astral.sorcery.net/~sortest/setpass.php?u_chan=";
	email->body += urlEncode(chan->name);
	email->body += "&u_ck=";
	email->body += urlEncode(pwAuthKey);
	email->body += "\nTo set a new channel password.\n\n";

	sprintf(buf,	"Retrieved by %.*s at %s",
			NICKLEN, from, ctime(&(doot)));
	email->body += buf;
	email->send();
	email->reset();
	operlog->log(nick, CS_GETPASS, args[1]);

	delete email;

	return RET_OK_DB;
}

/*--------------------------------------------------------------------*/
/* Debug command: Get the value of the password struct */

/**
 * \cscmd Getrealpass
 * \plr This command is slated for removal and now only exists to debug
 *      the new implementation of passwords.  -Mysid
 * \warning Do not use.
 */
CCMD(cs_getrealpass)
{
	char *from = nick->nick;
	RegChanList *chan;


	if (!isRoot(nick)) {
		sSend(":%s NOTICE %s :Access denied", ChanServ, from);
		return RET_NOPERM;
	}

	if (!opFlagged(nick, OROOT | OVERRIDE)) {
		sSend(":%s NOTICE %s :Restricted -- getrealpass is a debugging and administrative command not for password recovery.",
			 ChanServ, from);
		return RET_NOPERM;
	}

	if (numargs != 2) {
		sSend(":%s NOTICE %s :Must supply a channel name", ChanServ, from);
		return RET_SYNTAX;
	}

	chan = getRegChanData(args[1]);

	if (chan == NULL) {
		sSend(":%s NOTICE %s :%s is not registered", ChanServ, from,
			  args[1]);
		return RET_NOTARGET;
	}

	sSend(":%s GLOBOPS :%s used getrealpass on %s", ChanServ, from,
		  chan->name);
	if (!(chan->flags & CENCRYPT))
		sSend(":%s NOTICE %s :The password for %s is %s", ChanServ, from,
			  args[1], chan->password);
	else {
		u_char *p = toBase64(chan->password, 16);

		sSend(":%s NOTICE %s :The password for %s is encrypted: %s", ChanServ, from,
			  args[1], md5_printable(chan->password));
		if (p) {
			sSend(":%s NOTICE %s :Base64 representation: $%s", ChanServ, from,
				 p);
			FREE(p);
		}
	}
	operlog->log(nick, CS_GETREALPASS, args[1]);
	return RET_OK;
}

/*--------------------------------------------------------------------*/

/**
 * \cscmd Invite
 *
 * \syntax Invite \<channel>
 * \plr When a user sends this request for a channel they have recognized
 *      AOP+ status on, ChanServ will invite them (this goes through bans,
 *      +i, +l, etc).  Opers with the +a flag can use Invite with override,
 *      also.
 * \note Override invite works even if the channel is not registered,
 *       otherwise it must be registered.
 */
CCMD(cs_invite)
{
	char *from = nick->nick;
	int override = 0;
	RegChanList *chan;

	override = opFlagged(nick, OOPER | OVERRIDE) ? 1 : 0;

	if (numargs < 2) {
		sSend(":%s NOTICE %s :You must specify a channel.", ChanServ,
			  from);
		return RET_SYNTAX;
	}

	chan = getRegChanData(args[1]);

	if (override && chan == NULL) {
		sSend(":%s INVITE %s %s", ChanServ, from, args[1]);
		if (override)
			sSend(":%s GLOBOPS :%s uses override invite (%s)", ChanServ,
				  from, args[1]);
		return RET_OK;
	}

	if (chan == NULL) {
		sSend(":%s NOTICE %s :%s isn't registered", ChanServ, from,
			  args[1]);
		return RET_NOTARGET;
	}

	if ((getChanOp(chan, from) < chan->restrictlevel)) {
		sSend(":%s NOTICE %s :%s is currently restricted to chanop "
			  "levels %d (%s+) and up.", ChanServ, from, chan->name,
			  chan->restrictlevel, opLevelName(chan->restrictlevel, 0));
		return RET_NOPERM;
	}

	if (1 > getChanOp(chan, from) && !override) {
		sSend(":%s NOTICE %s :Access denied", ChanServ, from);
		return RET_NOPERM;
	} else {
		sSend(":%s NOTICE %s :Giving you an invitation for %s...",
			  ChanServ, from, chan->name);
		sSend(":%s INVITE %s %s", ChanServ, from, chan->name);

		if (!getChanData(chan->name))
			sSend(":%s NOTICE %s :Error: %s is empty.", ChanServ, from,
				  chan->name);

		if (override && chan)
			sSend(":%s GLOBOPS :%s uses override invite (%s)", ChanServ,
				  from, chan->name);
	}

	return RET_OK;
}

/*--------------------------------------------------------------------*/

/**
 * \cscmd Unban
 * \syntax Unban \<channel> ( "me" | "all" | \<pattern> )
 * \plr Level 1+ op access required for unban me, level MSOP access required
 * for other options.
 *
 * \syntax Unban \<channel>
 * \plr Alias for "Unban \<channel\> me"
 * \note Does not work with override, use /OS mode.
 */
CCMD(cs_unban)
{
	char *from = nick->nick;
	RegChanList *chan;
	ChanList *tmp;
	cBanList *tmpban;
	cBanList *next;
	int a, domodes = 0;
	char mode[(USERLEN + HOSTLEN + HOSTLEN) * 6 + 5] = "";
	char userhost[NICKLEN + USERLEN + HOSTLEN];
	char userhostM[NICKLEN + USERLEN + HOSTLEN];

	tmp = getChanData(args[1]);
	chan = getRegChanData(args[1]);

	if (tmp == NULL || chan == NULL) {
		sSend(":%s NOTICE %s :The channel must be registered, and in use.",
			  ChanServ, from);
		return RET_NOTARGET;
	}
	a = getChanOp(chan, from);

	if (a < AOP) {
		sSend(":%s NOTICE %s :You do not have sufficient access to %s",
			  ChanServ, from, args[1]);
		if (a > 0)
			sSend
				(":%s NOTICE %s :try \2/msg ChanServ INVITE %s\2 instead.",
				 ChanServ, from, args[1]);
		return RET_NOPERM;
	}

	if ((numargs < 3) || !strcasecmp(args[2], "me")
		|| !strcasecmp(args[2], from)) {
		snprintf(userhost, sizeof(userhost), "%s!%s@%s", nick->nick,
				 nick->user, nick->host);
		snprintf(userhostM, sizeof(userhost), "%s!%s@%s", nick->nick,
				 nick->user, genHostMask(nick->host));
		for (tmpban = tmp->firstBan; tmpban; tmpban = next) {
			next = tmpban->next;

			if (match(tmpban->ban, userhost) == 0
				|| match(tmpban->ban, userhostM) == 0) {
				strcat(mode, tmpban->ban);
				strcat(mode, " ");
				domodes++;
				sSend(":%s NOTICE %s :You are no longer banned on %s (%s)",
					  ChanServ, from, args[1], tmpban->ban);
				delChanBan(tmp, tmpban);
			}
			if (domodes > 0 && (domodes == 6 || next == NULL)) {
				sSend(":%s NOTICE %s :Removed bans: %s", ChanServ, from,
					  mode);
				sSend(":%s MODE %s -bbbbbb %s", ChanServ, args[1], mode);
				domodes = 0;
				mode[0] = 0;
			}
		}
	} else if (!strcasecmp(args[2], "all") || !strcasecmp(args[2], "*")) {
		if (a < MSOP)
			sSend(":%s NOTICE %s :Access denied", ChanServ, from);
		else {
			for (tmpban = tmp->firstBan; tmpban; tmpban = next) {
				next = tmpban->next;

				strcat(mode, tmpban->ban);
				strcat(mode, " ");
				domodes++;
				if (domodes == 6 || next == NULL) {
					sSend(":%s NOTICE %s :Removed bans: %s", ChanServ,
						  from, mode);
					sSend(":%s MODE %s -bbbbbb %s", ChanServ, args[1],
						  mode);
					domodes = 0;
					mode[0] = 0;
				}
				delChanBan(tmp, tmpban);

			}
		}
	} else {
		if (a < MSOP) {
			sSend(":%s NOTICE %s :Access denied", ChanServ, from);
		} else {
			for (tmpban = tmp->firstBan; tmpban; tmpban = next) {
				next = tmpban->next;

				if (!match(args[2], tmpban->ban)) {
					strcat(mode, tmpban->ban);
					strcat(mode, " ");
					domodes++;
					delChanBan(tmp, tmpban);
				}
				if (domodes > 0 && (domodes == 6 || next == NULL)) {
					sSend(":%s NOTICE %s :Removed bans: %s", ChanServ,
						  from, mode);
					sSend(":%s MODE %s -bbbbbb %s", ChanServ, args[1],
						  mode);
					domodes = 0;
					mode[0] = 0;
				}
			}
		}
	}
	return RET_OK;
}

/*--------------------------------------------------------------------*/

/**
 * \cscmd List
 * \syntax List { Option } \<search>
 * \plr List all channels matching the query.  By default \<search> is a
 * channel name mask: wildcards ? and * may be used.
 *
 * \plr "-f" may be given as an option, \<search> will then be for matching
 * against founder nickname
 *
 * \plr +L services access required.
 */
CCMD(cs_list)
{
	char *from = nick->nick;
	int i, x;
	RegChanList *reg;
	RegNickList *rnl = NULL;
	short byfounder = 0;

	i = x = 0;

	if (!isServop(nick) && !opFlagged(nick, OOPER | OLIST)) {
		sSend(":%s NOTICE %s :Permission denied.", ChanServ, from);
		return RET_NOPERM;
	}

	if (numargs < 2) {
		sSend(":%s NOTICE %s :You must specify a mask or -f", ChanServ,
			  from);
		return RET_SYNTAX;
	}

	if (strncmp(args[1], "-f", 2) == 0)
		if (numargs < 3) {
			sSend(":%s NOTICE %s :You must specify a founder nick with -f",
				  ChanServ, from);
			return RET_SYNTAX;
		} else {
			byfounder = 1;
			rnl = getRegNickData(args[2]);
		}

	for (reg = firstRegChan; reg; reg = reg->next) {
		if (byfounder == 1) {
			if (rnl && rnl->regnum == reg->founderId) {
				i++;
				sSend(":%s NOTICE %s :%4i: %s %s (%s)", ChanServ, from, i,
					  reg->name, rnl->nick, reg->desc);
			}
		} else {
			if (!match(args[1], reg->name)) {
				const char *tmpNick = reg->founderId.getNick();

				if (tmpNick == NULL)
					tmpNick = "(none)";

				i++;
				sSend(":%s NOTICE %s :%4i: %s %s (%s)", ChanServ, from, i,
					  reg->name, tmpNick, reg->desc);
			}
		}
		x++;
	}

	sSend(":%s NOTICE %s :%i/%i matched conditions (%s)", ChanServ, from,
		  i, x, (byfounder == 1) ? args[2] : args[1]);
	if (byfounder == 1)
		sSend(":%s GLOBOPS :%s is listing founder nick %s (%d/%d matches)", ChanServ, from,
			  args[2], i, x);
	else
		sSend(":%s GLOBOPS :%s is listing %s (%d/%d matches)", ChanServ, from, args[1], i, x);
	return RET_OK;
}

/*--------------------------------------------------------------------*/

/**
 * \cscmd Delete
 * \syntax Delete \<channel>
 *
 * \plr +C services access required.
 */
CCMD(cs_delete)
{
	char *from = nick->nick;
	int i, x;
	RegChanList *reg;
	ChanList *chan;

	i = x = 0;

	if (!isServop(nick) && !opFlagged(nick, OOPER | OCBANDEL)) {
		sSend(":%s NOTICE %s :Permission denied.", ChanServ, from);
		return RET_NOPERM;
	}

	if (numargs < 2) {
		sSend(":%s NOTICE %s :You must specify a channel to delete.",
			  ChanServ, from);
		return RET_SYNTAX;
	}


	if (is_sn_chan(args[1]) && !opFlagged(nick, OOPER | OSERVOP | OVERRIDE)) {
		sSend(":%s NOTICE %s :You can't delete that chan!", ChanServ,
			  from);
		sSend(":%s GLOBOPS :%s tried to use DROP command on %s", ChanServ,
			  from, args[1]);
		chanlog->log(nick, CS_DELETE, args[1], LOGF_FAIL | LOGF_OPER);
		return RET_EFAULT;
	}


	if (!(reg = getRegChanData(args[1]))) {
		sSend(":%s NOTICE %s :That channel is not registered.", ChanServ,
			  from);
		return RET_NOTARGET;
	}

	if (addFlood(nick, 25))
		return RET_KILLED;

	if ((reg->flags & CHOLD) && !opFlagged(nick, OROOT)) {
		sSend
			(":%s NOTICE %s :That is a held channel.",
			 ChanServ, from);
		sSend(":%s GLOBOPS :%s tried to use DROP command on %s (held channel)", ChanServ,
			  from, args[1]);
		chanlog->log(nick, CS_DELETE, args[1], LOGF_FAIL | LOGF_OPER);
		return RET_EFAULT;
	}

	if ((reg->flags & CMARK) && !opFlagged(nick, OROOT)) {
		sSend
			(":%s NOTICE %s :That is a marked channel.",
			 ChanServ, from);
		sSend(":%s GLOBOPS :%s tried to use DROP command on %s (marked channel)", ChanServ,
				from, args[1]);
		chanlog->log(nick, CS_DELETE, args[1], LOGF_FAIL | LOGF_OPER);
		return RET_OK_DB;
	}

	sSend(":%s GLOBOPS :%s used DROP command on %s", ChanServ, from,
		  args[1]);

	chanlog->log(nick, CS_DELETE, args[1], LOGF_OPER);
	if ((chan = getChanData(args[1])))
		chan->reg = NULL;
	delRegChan(reg);
	return RET_OK_DB;
}

/*--------------------------------------------------------------------*/

/**
 * \cscmd Log
 * \syntax Log \<message>
 *
 * \plr Write a message to the ChanServ logs.
 * Oper access required.
 */
CCMD(cs_log)
{
	char *from = nick->nick;
	char log_str[IRCBUF + 1];

	if (addFlood(nick, 5))
		return RET_KILLED;

	if (isOper(nick) == 0) {
		sSend(":%s NOTICE %s :Access denied", ChanServ, from);
		return RET_NOPERM;
	}

	if (numargs < 2) {
		sSend(":%s NOTICE %s :Must supply at least one argument", ChanServ,
			  from);
		return RET_SYNTAX;
	}

	bzero(log_str, IRCBUF);
	parse_str(args, numargs, 1, log_str, IRCBUF);

	if (strlen(log_str) < 2) {
		sSend(":%s NOTICE %s :Logline too short.", ChanServ, from);
		return RET_EFAULT;
	}
	if (strlen(log_str) >= (IRCBUF - 1)) {
		sSend(":%s NOTICE %s :Logline too long.", ChanServ, from);
		return RET_EFAULT;
	}

	sSend(":%s NOTICE %s :Ok, logged.", ChanServ, from);
	chanlog->log(nick, CS_LOG, NULL, 0, "%s", log_str);
	return RET_OK_DB;
}

/*--------------------------------------------------------------------*/

/**
 * \cscmd Setpass
 * \syntax Setpass \<channel> \<code> \<new password>
 *
 * \plr Command used to change a channel password using a key mailed
 * by services.
 */
CCMD(cs_setpass)
{
	const char *from = nick->nick;
	const char *pwAuthChKey;
	RegChanList *rcl;
	RegNickList *rfl;

	if (numargs < 4)
	{
		sSend(":%s NOTICE %s :Syntax: SETPASS <channel> <code> <new password>",
			ChanServ, from);
		return RET_SYNTAX;
	}

	if ((rcl = getRegChanData(args[1])) == NULL)
	{
		sSend(":%s NOTICE %s :That channel is not registered.",
		      NickServ, from);
		return RET_NOTARGET;
	}

	rfl = rcl->founderId.getNickItem();

	if (rfl && rcl->chpw_key) {
		if (rcl->flags & CFORCEXFER)
			pwAuthChKey = GetAuthChKey("-forced-transfer-", PrintPass(rcl->password, ChanGetEnc(rcl)),
			                         rcl->timereg, rcl->chpw_key);
		else
			pwAuthChKey = GetAuthChKey(rfl->email, PrintPass(rcl->password, ChanGetEnc(rcl)),
			                         rcl->timereg, rcl->chpw_key);
	}
	else
		pwAuthChKey = NULL;

	if (pwAuthChKey == NULL || strcmp(pwAuthChKey, args[2]))
	{
		sSend(":%s NOTICE %s :Access Denied.",
		      ChanServ, from);
		return RET_FAIL;
	}

	if (strlen(args[3]) > PASSLEN)
	{
		sSend(":%s NOTICE %s :Specified password is too long, please "
		      "choose a password of %d characters or less.",
		      ChanServ, from, PASSLEN);
		return RET_EFAULT;
	}


	if (strcasecmp(args[1], args[3]) == 0 || strcasecmp(args[3], "password") == 0
	    || strlen(args[3]) < 3)
	{
		sSend(":%s NOTICE %s :You cannot use \2%s\2 as your channel password.",
		      ChanServ, from, args[3]);
		return RET_EFAULT;
	}

	sSend(":%s NOTICE %s :Ok.", ChanServ, from);

	rcl->flags |= CENCRYPT;
	rcl->chpw_key = 0;
	rcl->flags &= ~CFORCEXFER;
	pw_enter_password(args[3], rcl->password, ChanGetEnc(rcl));
	clearChanIdent(rcl);

	sSend(":%s NOTICE %s :The password for %s is now %s, do NOT forget it, we"
	      " are not responsible for lost passwords.",
		ChanServ, from, rcl->name, args[3]);

	return RET_OK_DB;
}

/**
 * @internal unfinished
 */
NCMD(cs_setrealpass)
{
	char* from = nick->nick;
	RegChanList* chn;

	if (!opFlagged(nick, OOPER | OGRP)) {
		sSend(":%s NOTICE %s :Permission denied -- this command is "
		      "provided for PWops/GRPops.", ChanServ, from);

		return RET_NOPERM;
	}

	if (numargs < 2) {
		sSend(":%s NOTICE %s :Syntax Error: Channel name missing.",
			ChanServ, from);
		return RET_SYNTAX;
	}

	if (numargs < 3) {
		sSend(":%s NOTICE %s :Syntax Error: Password missing.",
			ChanServ, from);
		return RET_SYNTAX;
	}

	chn = getRegChanData(args[1]);

	if (chn == NULL) {
		sSend(":%s NOTICE %s :Setrealpass error: channel is not registered.",
			ChanServ, from);
		return RET_NOTARGET;
	}

	if ((!isRoot(nick) || !opFlagged(nick, OVERRIDE)) && is_sn_chan(args[1])) {
		sSend(":%s NOTICE %s :Permission denied: magic channel.",
			ChanServ, from);
		return RET_NOPERM;
	}

	pw_enter_password(args[2], chn->password, ChanGetEnc(chn));
	sSend(":%s NOTICE %s :New password installed.",
			ChanServ, from);

	return RET_OK_DB;
}

/*--------------------------------------------------------------------*/

/**
 * \cscmd Trigger
 * \syntax Trigger
 * \plr Show system defaults
 *
 * \syntax Trigger list
 * \plr List channel triggers
 *
 * \syntax Trigger \<channel>
 * \plr Show trigger levels for channel.
 *
 * \syntax Trigger \<channel> DEFAULTS
 * \plr Reset trigger levels for channel to the default values.
 *
 * \syntax Trigger \<channel> MaxAkicks \<number>
 * \plr Change the number akicks limit for the channel
 *
 * \syntax Trigger \<channel> MaxOps \<number>
 * \plr Change the number ops maximum for the channel
 *
 */
CCMD(cs_trigger)
{
	// COMMAND_REQUIRE_REGNICK(nick);
	// COMMAND_REQUIRE_OPER(nick);
	// COMMAND_REQUIRE_PERMISSIONS(nick, OSERVOP);
	ChanTrigger* trg;
	char* from = nick->nick;

	if (!isOper(nick) || !isRoot(nick)) {
		PutReply(ChanServ, nick, ERR_NOACCESS, 0, 0, 0);
		return RET_NOPERM;
	}

	/**
	 * @sect cmd_cs_trigger
	 * Command Syntax:
	 *  a. TRIGGER
	 *          Show system defaults
	 *  b. TRIGGER LIST
	 *          List triggers
	 *  c. TRIGGER <channel>
	 *          Show trigger levels
	 *  d. TRIGGER <channel> DEFAULTS
	 *          Remove all triggered data
	 *  e. TRIGGER <channel> MaxAkicks <number>
	 *          Change max number akicks
	 *  f. TRIGGER <channel> MaxOps <number>
	 *          Change max number chanops
	 */

	if (numargs == 1) {
		// Show system defaults
		sSend(":%s NOTICE %s :Default Values:", ChanServ, nick->nick);
		sSend(":%s NOTICE %s :MaxOps(%d), MaxAkicks(%d)", ChanServ, nick->nick, OpLimit, AkickLimit);
		return RET_OK;
	}

	if (numargs < 2) {
		sSend(":%s NOTICE %s :Syntax: TRIGGER [<channel> [<item> <value>]]", ChanServ, nick->nick);
		sSend(":%s NOTICE %s :        TRIGGER <channel> DEFAULTS", ChanServ, nick->nick);
		return RET_SYNTAX;
	}//$$ XXX


	if (!str_cmp(args[1], "LIST")) {
		long hashEnt;
		// List all triggers

		sSend(":%s NOTICE %s :%-30s %-3s %-3s", ChanServ, from, "Channel", "Ops", "Aks");

		for(hashEnt = 0; hashEnt < CHANTRIGHASHSIZE; hashEnt++)
			for(trg = LIST_FIRST(&ChanTrigHash[hashEnt]); trg;
			    trg = LIST_NEXT(trg, cn_lst))
			{
				sSend(":%s NOTICE %s :%-30s %-3d %-3d", ChanServ,
					from, trg->chan_name, DEFIFZERO(trg->op_trigger, OpLimit), DEFIFZERO(trg->ak_trigger, AkickLimit));
			}
		return RET_OK;
	}

	if (ValidChannelName(args[1]) == 0) { // Invalid channel name error
		PutReply(ChanServ, nick, ERR_CS_INVALIDCHAN_1ARG, args[1], 0, 0);
		return RET_EFAULT;
	}

	if (getRegChanData(args[1]) == 0) { // Channel not registered error
		PutReply(ChanServ, nick, ERR_CHANNOTREG_1ARG, args[1], 0, 0);
		return RET_NOTARGET;
	}

	trg = FindChannelTrigger(args[1]);

	if (numargs == 2) {
		// Show trigger information for channel
		sSend(":%s NOTICE %s :%s:", ChanServ, nick->nick, args[1]);

                if (trg)
			sSend(":%s NOTICE %s :MaxOps(%d)  MaxAkicks(%d)", ChanServ, nick->nick,
			      DEFIFZERO(trg->op_trigger, OpLimit), DEFIFZERO(trg->ak_trigger, AkickLimit));
		else
			sSend(":%s NOTICE %s :MaxOps(%d)  MaxAkicks(%d)", ChanServ, nick->nick,
			      OpLimit, AkickLimit);
		return RET_OK;
	}

    {
	int k = -1;

	if (!str_cmp(args[2], "MaxOps"))
		k = 0;
	else if (!str_cmp(args[2], "MaxAkicks"))
		k = 1;

	if (k == -1 && (numargs != 4 || (numargs > 3 && args[3] && !isdigit(args[3][0])))) {
		if (!str_cmp(args[2], "DEFAULTS")) {
			// Delete the trigger info for this chan,
			// reverting it to default limits/state.
			// Log the change
			// Send a globops, maybe

			if (trg) {
				DelChannelTrigger(trg);
				FreeChannelTrigger(trg);

				return RET_OK_DB;
			}
			return RET_NOTARGET;
		}
		// Syntax error, must specify trigger level
		return RET_SYNTAX;
	}

	if (k == -1) { // Error, no such trigger variable
		PutReply(ChanServ, nick, ERR_INVALID_TRIGVAR, 0, 0, 0);
		return RET_SYNTAX;
	}

	if (!trg) {
		trg = MakeChannelTrigger(args[1]);
		AddChannelTrigger(trg);
	}

	if (k == 0)
		trg->op_trigger = atoi(args[3]);
	if (k == 1)
		trg->ak_trigger = atoi(args[3]);

	// Add a trigger for the channel name if it doesn't exist,
	// make the appropriate changes

	// Log the change
	// Send globops
	return RET_OK_DB;
    }

	return RET_FAIL;
}

/*--------------------------------------------------------------------*/

CCMD(cs_dmod)
{
	RegChanList *chan;
        typedef enum { DMOD_UNDEFINED, DMOD_HELP, DMOD_TOPIC,
                       DMOD_FOUNDER } dModItem;
        dModItem tar = DMOD_UNDEFINED;
	char* from = nick->nick;
	int i;

	if (!isOper(nick) || !opFlagged(nick, ODMOD)) {
		PutReply(ChanServ, nick, ERR_NOACCESS, 0, 0, 0);
		return RET_NOPERM;
	}

        #define DMOD_KEY(nm, va) else if (strcmp(args[i], nm) == 0) {tar=(va); i++; break;}

	for(i = 1; i < numargs; i++) {
		if (args[i][0] != '-')
			break;

                if (0);
		DMOD_KEY("-help", DMOD_HELP)
		DMOD_KEY("-topic", DMOD_TOPIC)
		DMOD_KEY("-founder", DMOD_FOUNDER)
	}

	if (tar == DMOD_UNDEFINED) {
		sSend(":%s NOTICE %s :An action is required.  See \2/cs help dmod\2  for more information.", ChanServ, from);
		return RET_SYNTAX;
	}

	if (i >= numargs) {
		sSend(":%s NOTICE %s :This command requires a channel.", ChanServ, from);
		return RET_SYNTAX;
	}

	if (i + 1 >= numargs) {
		sSend(":%s NOTICE %s :To what value?", ChanServ, from);
		return RET_SYNTAX;
	}

	chan = getRegChanData(args[i]);
	if (!chan) {
		sSend(":%s NOTICE %s :%s is not registered", ChanServ, from, args[1]);
		return RET_NOTARGET;
	}

	if (is_sn_chan(chan->name) && !opFlagged(nick, OOPER | OROOT | OVERRIDE)) {
		sSend(":%s NOTICE %s :Please contact a services root, to use dmod on a "
			" held channel.", ChanServ, from);
		chanlog->log(nick, CS_DMOD, chan->name, LOGF_FAIL | LOGF_OPER);
		return RET_NOPERM;
	}

	if (!isRoot(nick) && (chan->flags & (CHOLD|CMARK))) {
		sSend(":%s NOTICE %s :Error: %s is held or marked.", ChanServ, from, args[1]);
		return RET_NOPERM;
	}

	rshift_argv(args, i, numargs);
	numargs -= (i);

	switch( tar ) {
		case DMOD_FOUNDER :
			{
				RegNickList* reg = getRegNickData(args[1]);

				if (reg == NULL) {
					PutReply(ChanServ, nick, ERR_NICKNOTREG_1ARG,
						args[1], 0, 0);
					return RET_NOTARGET;
				}

				chan->founderId = reg->regnum;

				sSend(":%s NOTICE %s :%s now founder of %s.",
				      ChanServ, from, reg->nick, chan->name);
				return RET_OK_DB;
			}
		case DMOD_TOPIC:
			{
				char buf[TOPIC_MAX+1];
				parse_str(args, numargs, 1, buf, TOPIC_MAX);
				if (strcmp(buf, "*") == 0)
					buf[0] = '\0';
				if (chan->topic)
					FREE(chan->topic);
				chan->topic = static_cast<char *>(oalloc(strlen(buf)));
				strcpy(chan->topic, buf);
				strncpyzt(chan->tsetby, nick->nick, NICKLEN);
				return RET_OK_DB;
			}
			break;
		default:;
			break;
	}

	return RET_EFAULT;
}

/*--------------------------------------------------------------------*/

/* $Id$ */

