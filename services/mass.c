 /*************************************************************************
 * Mass Advertising detection functions.
 *
 *  Copyright (c) 2000 James Hess
 *  All rights reserved.
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
 */
/**
 * @file mass.c
 * @brief Ad detection
 *
 * Procedures to help rid a network of unauthorized mass ads!
 *
 * \mysid
 * \date 2001-2002
 *
 * $Id$
 */

 /*  changes:
  *    [7/2/1998] Obsolete ->status stuff removed            
  *
  *
  */
#include "services.h"
#include "log.h"
#include "nickserv.h"
#include "mass.h"

char *massAdClones[5] = {};
void insert_ad(aMassAd * insert_this);
const char s_MassBot[] = "MassServ";
LIST_HEAD(, _mass_ad) masslist;

/**
 * @param source Sender nickname
 * @param buf Message text
 * @param which Value indicating which client received the message
 * @brief handle an incoming message to a MassServ fake client
 */
void detect_mass(UserList *nl_from, char *buf, int which)
{
	aMassAd *booya;
	aMassAd *f1, *f2, *f1o, *f2o;

	expire_ads();

	if (nl_from != 0) {
		f1o = f1 = find_ad(nl_from, 0);
		f2o = f2 = find_ad(nl_from, 1);
	} 
	else
		return;

	if (nl_from && ((!f1 && which == 0) || (!f2 && which == 1))) {
		booya = (aMassAd *) oalloc(sizeof(aMassAd));
		booya->sender = nl_from;
		booya->utc = time(NULL);
		booya->rcvd_by = which;
		insert_ad(booya);
		if (!booya->sender)
			logDump(corelog,
					"[***] bug: booya->sender is NULL in detect_mass()");
	}

	if (f1 == NULL)
		f1 = find_ad(nl_from, 0);
	if (f2 == NULL)
		f2 = find_ad(nl_from, 1);

	if (f1 != NULL && f2 != NULL) {
		sSend(":%s GLOBOPS :\2Mass Ad:\2 (%s) %s",
			  MassServ, nl_from->nick, buf);
		flush_ad(nl_from);
		// sSend(":%s SVSKILL %s :killed ([services] (Mass msg'ing))", source);
	} else if (!f1o && !f2o)
		sSend(":%s GLOBOPS :(Debug) \2Possible Mass Ad:\2 (%s) %s", MassServ,
			  nl_from->nick, buf);
	return;
}

/**
 * @brief Cancel mass ad cache from a user
 * @param u User to flush out
 * @param rcv -1 = cancel all, or cancel where ad->rcvd_by == rcv
 */
aMassAd *
find_ad(UserList * u, int rcv)
{
	aMassAd *search;

	if ((search = LIST_FIRST(&masslist)) == NULL)
		return NULL;
	while (search) {
		if (search->sender == u && (search->rcvd_by == rcv || rcv == -1))
			return search;
		search = LIST_NEXT(search, ma_lst);
	}
	return NULL;
}

/**
 * @brief Insert a new ad structure into a user mass list
 */
void
insert_ad(aMassAd * insert_this)
{
	LIST_INSERT_HEAD(&masslist, insert_this, ma_lst);
}

/**
 *    executed on every db expire to flush out mass ads
 *    older than 5 minutes
 */
void
expire_ads(void)
{
	aMassAd *iter, *iter_next;
	time_t present = time(NULL);

	iter = LIST_FIRST(&masslist);
	if (iter == NULL)
		return;

	while (iter) {
		iter_next = LIST_NEXT(iter, ma_lst);

		if ((present - iter->utc) > (15 * 60)) {
			//       wallops(NULL, "forgetting about ad from %s", iter->sender->nick);
			remove_ad(iter);
		}
		iter = iter_next;
	}
}


/**
 *    flush_ad(<user>):  called when a user quits IRC
 *                       so we dont leave a dangling ->sender pointer
 */
void
flush_ad(UserList * foo)
{
	aMassAd *iter, *iter_next;
	iter = NULL;

	iter = LIST_FIRST(&masslist);

	if (iter == NULL)
		return;
	while (iter) {
		iter_next = LIST_NEXT(iter, ma_lst);

		if (iter->sender == foo || !foo)
			remove_ad((iter));
		iter = iter_next;
	}
}

/**
 *      quickly remove a massad from the list and free it.
 */
void
remove_ad(aMassAd * zap)
{
	if (zap == NULL)
		return;

	LIST_REMOVE(zap, ma_lst);
	FREE(zap);
}


/**
 *      unlink a mass ad entry from the masslist without freeing the memory
 *      better keep a pointer to it unless you wanna just memleak...
 */
void
unlink_ad(aMassAd * zap)
{
	LIST_REMOVE(zap, ma_lst);
}



/**
 * used by ranstring
 */
char
ranchar(int typ)
{
	char aVal = 0;
	int rval = 0;
	/* ircnames */
	static char alphatab1[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcefghijklmnopqrstuvwxyz";
	/* nicks */
	static char alphatab2[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcefghijklmnopqrstuvwxyz_-[]\\^123456789";
	/* hostnames */
	static char alphatab3[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcefghijklmnopqrstuvwxyz0123456789-";

	/* struct timeval tv_v;*/
	struct timezone tz_v;

	tz_v.tz_minuteswest = 0;
	tz_v.tz_dsttime = 0;
	rval = rand();

	switch (typ) {
	case 0:
		aVal = alphatab1[rval % (strlen(alphatab1) - 1)];
		break;
	case 1:
		aVal = alphatab2[rval % (strlen(alphatab2) - 1)];
		break;
	case 2:
		aVal = alphatab3[rval % (strlen(alphatab3) - 1)];
		break;
	default:
		aVal = alphatab1[rval % (strlen(alphatab1) - 1)];
		break;
	}
	return aVal;
}

/**
 *     this is fun (generate a random string)
 *     to be used for massmsg detectors ircname/nicknames/hosts
 *        0 = random nick
 *        1 = random ircname
 *        2 = random user
 *        3 = random host
 */
char *
ranstring(char *inbuf, int maxlen, int typ)
{
	char *orig;
	int dolen = 0, minlen = 0, i = 0;
	/* struct timeval tv_v; */

	orig = inbuf;
	*inbuf = 0;

	switch (typ) {
	case 0:
		minlen = 4;
		break;
	case 1:
		minlen = 5;
		break;
	case 2:
		minlen = 3;
		break;
	case 3:
		minlen = 10;
		break;
	default:
		minlen = 1;
		break;
	}

	if (maxlen < 0 || minlen < 0 || minlen > maxlen) {
//		wallops(NULL, "[***] ranstring() bug: maxlen=%d minlen=%d", maxlen,
//				minlen);
		return NULL;
	}

	if (maxlen - minlen > 0)
		dolen = MIN((rand() % (maxlen - minlen)) + minlen, maxlen);
	else
		dolen = maxlen;

	switch (typ) {
	case 0:
	case 1:
	case 2:
	default:
		for (i = 0; i < dolen; i++) {
			if (inbuf < (orig + 1) || (typ < 1))
				*inbuf = ranchar(0);
			else
				*inbuf = ranchar(1);
			inbuf++;
		}
		*inbuf++ = 0;
		break;
	case 3:					/* oh fun, a hostname :/ */
		if (dolen < 10)
			return NULL;
		for (i = 0; i < (dolen - 4); i++) {
			switch (i) {
			case 4:
				*inbuf++ = '.';
				break;
			case 7:
				*inbuf++ = '.';
				break;
			default:

				*inbuf = ranchar(2);
				inbuf++;
				break;
			}
		}
		*inbuf++ = '.';
		*inbuf++ = 'c';
		*inbuf++ = 'o';
		*inbuf++ = 'm';
		*inbuf++ = 0;
		break;
	}
	return orig;

}

/**
 * @brief Make a fake mass client
 */
char *
make_clone()
{
	static char rand_nick[NICKLEN + 1];
	char rand_user[USERLEN + 1];
	char host[HOSTLEN + 1], blip1[6], blip2[5];
	char rand_ircname[50];
	int fails = 0;

	(void)ranstring(rand_nick, NICKLEN - 1, 0);
	(void)ranstring(rand_user, USERLEN - 1, 2);
	(void)ranstring(rand_ircname, sizeof(rand_ircname) - 1, 1);
	(void)ranstring(blip1, sizeof(blip1) - 1, 0);
	(void)ranstring(blip2, sizeof(blip2) - 1, 1);

	snprintf(host, sizeof(host), "services-%x.%s-sorcery-massdetect.net",
		(time(0)/30)%1000, blip1);


	if (getNickData(rand_nick) || getRegNickData(rand_nick)) {
		while ((getNickData(rand_nick) || getRegNickData(rand_nick)) && fails < 50) {
			(void)ranstring(rand_nick, NICKLEN - 1, 0);
			if (fails++ > 20)
				return NULL;
			else if (fails > 1 && fails < 3)
				sSend("GLOBOPS :[***] make_clone(MassServ): nick %s already exists",
						 rand_nick);
		}
	}

	addUser(rand_nick, rand_user, host, rand_ircname, "+m");
	return rand_nick;
}

/**
 * @brief Maintain the advert detection bots' presence and expire old
 *        advert information periodically
 */
void adCloneMaintenance()
{
	char *y;
	int k;

	if (massAdClones[0] == 0 && massAdClones[1] == 0) {
		if ((y = make_clone())) {
			massAdClones[0] = strdup(y);
			if ((y = make_clone()))
				massAdClones[1] = strdup(y);
		}
		massAdClones[2] = massAdClones[3] = massAdClones[4] = NULL;
	}
	else {
		if (massAdClones[0])
		{
			sSend(":%s QUIT :%s", massAdClones[0], massAdClones[0]);
			massAdClones[2] = massAdClones[0];
			if ((y = make_clone()))
				massAdClones[0] = strdup(y);
		}

		if (massAdClones[1])
		{
			sSend(":%s QUIT :%s", massAdClones[1], massAdClones[1]);
			massAdClones[3] = massAdClones[1];
			if ((y = make_clone()))
				massAdClones[1] = strdup(y);
		}

		for(k = 2; k < 4; k++)
		{
			if (massAdClones[k]) {
				FREE(massAdClones[k]);
				massAdClones[k] = 0;
			}
		}
	}
}

/**
 * @brief Check if a message is addressed to an ad bot
 * @return 0=no, 1=yes
 */
int adCheck(UserList *nl_from, char *nm_target, char *args[], int numargs)
{
	int k = 0;
	char txt_message[IRCBUF];

	if (MassServ == NULL)
		return 0;

	if (nl_from == 0 || args == 0 || numargs < 1 || args[0]==0 || nm_target == 0)
		return 0;
	parse_str(args, numargs, 0, txt_message, IRCBUF);

	for(k = 0; k < 2; k++)
	{
		if (massAdClones[k] && strcasecmp(nm_target, massAdClones[k]) == 0)
		{
			detect_mass(nl_from, txt_message, k);
			return 1;
		}
	}
	return 0;
}

/**
 * @brief Timed maintenance of the ad detection system, every 5 minutes
 */
void timed_advert_maint(char *a)
{
	if (MassServ != NULL) { /* Disable clones if MassServ off */
		timer(60*5, timed_advert_maint, NULL);
		expire_ads();
		adCloneMaintenance();
	}
}

/*************************************************************************/

