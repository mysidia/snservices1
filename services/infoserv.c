/**
 * \file infoserv.c
 * \brief InfoServ Implementation
 *
 * \duff \greg \mysid
 * \date 1997, 2000-2001
 *
 * $Id$
 */

/*
 * Portions Copyright (c) 2000, 2001 James Hess
 * Copyright (c) 1997 Dafydd James & Greg Poma.
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
#include "infoserv.h"
#include "nickserv.h"
#include "log.h"
#include "db.h"

// *INDENT-OFF*

/* infoserv.c
 *
 * Concept by someone I forgot who, I think Mysidia...
 * designed to send important news/messages to users
 * -DuffJ
 *
 * Yeah, that's right.
 * -Mysid
 */

/**
 * \brief Last time_t of an #InfoServ post
 */
time_t is_last_post_time = 0;

/**
 * @brief InfoServ command dispatch data
 */
interp::service_cmd_t infoserv_commands[] = {
/*  command	function	oper		log			addflood */
  { "help",	is_help, 		0,		LOG_NO, 0,	5	},
  { "read",	is_sendinfo,	0,		LOG_NO, 0,	5	},
  { "list",	is_listnews,	0,		LOG_NO, 0,	9	},
  { "post",	is_postnews,	OOPER,	LOG_NO, 0,	3	},
  { "del", 	is_delete,		OOPER,	LOG_NO, 0,	3	},
  { "save", is_save,		OOPER,	LOG_NO, 0,	3	},
  { NULL,	NULL,			0,		LOG_NO, 0,	0	}
};

/**
 * News article linked list head
 */
SomeNews *is_listhead = NULL;

/// Levels of priority for InfoServ Articles
const char *priority_names[] =
{
	"IRCops Only",
	"Informational",
	"General announcement",
	"Important information",
	"URGENT Notice",
	"Critical message",
	"Unknown",
};

char* str_dup(const char *);

// *INDENT-ON*
/*------------------------------------------------------------------------*/

/**
 * @brief Deallocate a news article
 */
void
freeNews(SomeNews *theNews)
{
	FREE(theNews->content);
	FREE(theNews->header);
	FREE(theNews);
}

/**
 * \brief Nag users signing on/identifying about unread news
 * \param Pointer to the online user to nag
 */
void
newsNag(UserList * nick)
{
	SomeNews *news;
	int count = 0, ict = 0, tupdate = 0;
	if (!nick || !nick->reg || nick->reg->is_readtime >= is_last_post_time
		|| nick->caccess < 2)
		return;
	for (news = is_listhead; news; news = news->next) {
		++ict;


		switch(news->importance)
		{
			case 0:
				if ((isOper(nick) 
				    || (nick->reg 
				        && (nick->reg->opflags & OOPER)
				       ))) 
					++count;
				break;
			case 4:
				sSend(":%s NOTICE %s :(%d) \"%s\": %s",
				      InfoServ, nick->nick, ict,
				      news->header, news->content);
				tupdate = 1;
				break;

			case 5:
				sSend(":%s PRIVMSG %s :(%d) \"%s\": %s",
				      InfoServ, nick->nick, ict,
				      news->header, news->content);
				tupdate = 1;
				break;
			default:;
				tupdate = 0;
				if (news->timestamp < nick->reg->is_readtime
					&& news->importance >= 3)
					++count;
				break;
		}
	}
	if (!count)
		return;
	if (tupdate && nick->reg)
		nick->reg->is_readtime = CTime;

	if (count == 1)
		PutReply(InfoServ, nick, RPL_IS_NEWITEM, 0, 0, 0);
	else
		PutReply(InfoServ, nick, RPL_IS_NEWITEMS_1ARG, count, 0, 0);

	PutReply(InfoServ, nick, RPL_IS_HOWTOLIST, 0, 0, 0);
}


/**
 * \brief Parse an InfoServ message
 * \param nick Pointer to online user item sending message
 * \param args Array vector of parameters of which args[0] is the name
 *             of the command and args[numargs - 1] is the last parameter
 * \param numargs Number of accessible items in the args[] array
 * \pre Nick points to a valid online user record.  Args points to
 *      a string vector with numargs elements following PRIVMSG X :Y Z.
 *      args[0] is Y each space-separated token thereafter is an element.
 * \post Command has been processed appropriately
 */
void
sendToInfoServ(UserList * nick, char **args, int numargs)
{
	interp::parser * cmd;

	cmd =
		new interp::parser(InfoServ, getOpFlags(nick), infoserv_commands,
						   args[0]);
	if (!cmd)
		return;

	switch (cmd->run(nick, args, numargs)) {
	default:
		break;
	case RET_FAIL:
		PutReply(InfoServ, nick, ERR_IS_UNKNOWNCMD_1ARG, args[0], 0, 0);
		PutHelpInfo(InfoServ, nick, "HELP");
		break;
	}

	delete cmd;
}

/*------------------------------------------------------------------------*/

/**
 * @brief Help system entry point
 */
ICMD(is_help)
{
	help(nick->nick, InfoServ, args, numargs);
	return RET_OK;
}

/**
 * @brief Save all articles
 */
ICMD(is_save)
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
	saveInfoData();
	return RET_OK;
}


/**
 * @brief /InfoServ READ
 */
ICMD(is_sendinfo)
{
	char *from = nick->nick, *p, *str;
	SomeNews *sn;
	int i, which;
	parse_t lineSplit;

	sn = is_listhead;

	if (numargs < 2) {
		PutReply(InfoServ, nick, ERR_IS_NEEDPARAM, 0, 0, 0);
		PutHelpInfo(InfoServ, nick, "HELP READ");
	}

	if (isdigit(*args[1]) == 0) {
		PutReply(InfoServ, nick, ERR_IS_NEEDNUM, 0, 0, 0);
		return RET_SYNTAX;
	}

	which = atoi(args[1]);

	for (i = 1; sn != 0; sn = sn->next, i++)
		if (i == which &&
		    (isOper(nick) ||
		     (nick->reg && (nick->reg->opflags & OOPER)) ||
		      sn->importance != 0)) 
		{
			str = str_dup(sn->content);
			if ( parse_init(&lineSplit, str) != 0 )
				continue;

			sSend(":%s NOTICE %s :#%d (%s) :: %s", InfoServ, from, i, sn->from, ctime(&sn->timestamp));
			sSend(":%s NOTICE %s :Subject: %s", InfoServ, from, sn->header);
			sSend(":%s NOTICE %s :Priority: %s", InfoServ, from, priority_names[sn->importance]);
			sSend(":%s NOTICE %s :+++", InfoServ, from);
			lineSplit.delim = '\n';
			while((p = parse_getarg(&lineSplit)))
				sSend(":%s NOTICE %s :%s", InfoServ, from, (p && *p) ? p : " ");
			parse_cleanup(&lineSplit);
			FREE(str);

			return RET_OK;
		}
	PutReply(InfoServ, nick, ERR_IS_NOARTICLE_ARG1, atoi(args[1]), 0, 0);
	return RET_NOTARGET;
}

/**
 * /IS listnews
 */
/* ARGSUSED1 */
ICMD(is_listnews)
{

	SomeNews *blah;
	int i = 1;
	struct tm *time;
	char timestr[80];

	blah = is_listhead;

	if (nick->reg)
		nick->reg->is_readtime = CTime;

	if (blah == NULL) {
		PutReply(InfoServ, nick, ERR_IS_NOARTICLES, 0, 0, 0);
		return RET_OK;
	}

	sSend(":%s NOTICE %s :List of articles", InfoServ, nick->nick);
	sSend
		(":%s NOTICE %s :Number  Header              Sender              Time posted",
		 InfoServ, nick->nick);

	for (; blah != 0; blah = blah->next, i++) {
		if (blah->importance == 0 && !isOper(nick)
			&& (!nick->reg || nick->caccess < 2
				|| !(nick->reg->opflags & OOPER))) continue;

		time = localtime(&blah->timestamp);
		strftime(timestr, 80, "%a %D %T %Z", time);

		sSend(":%s NOTICE %s :%-2d %-20s : %-20s :: %s", InfoServ,
			  nick->nick, i, blah->from, timestr, blah->header);
	}

	PutReply(InfoServ, nick, RPL_IS_END_OF_LIST, 0, 0, 0);
	PutReply(InfoServ, nick, RPL_IS_READ_HELP, 0, 0, 0);
	return RET_OK;
}


/**
 * is_postnews() - Sun Jan 4th, 1998 DuffJ
 * args[0] = "post"
 * args[1] = importance level (1 - 5)
 * args[2] = one-word header. 
 * args[3] = content, up to max limit (NEWSCONTENTLEN)
 */
ICMD(is_postnews)
{
	SomeNews *newnews = 0;
	SomeNews *working = 0;
	char *from = nick->nick;
	char content[IRCBUF + 200];
	int i = 0;

	if (numargs < 4) {
		PutReply(InfoServ, nick, ERR_IS_NEEDPARAM, 0, 0, 0);
		PutHelpInfo(InfoServ, nick, "HELP POST");
		return RET_SYNTAX;
	}

	if (!isdigit(*args[1])) {
		PutReply(InfoServ, nick, ERR_IS_NEEDIMPORTANCE, 0, 0, 0);
		for(i = 0; i <= 5; i++)
			sSend(":%s NOTICE %s :%-2d  - %s",
			 InfoServ, from, i, priority_names[i]);
		return RET_SYNTAX;
	}

	/* Compile the content into a string, and a length string */
	content[0] = 0;				/* Make sure that strcat() starts at the beginning */
	parse_str(args, numargs, 3, content, IRCBUF);

	/* refuse to post if an item will be truncated. - Liam
	 */
	if (strlen(content) >= NEWSCONTENTLEN) {
		PutReply(InfoServ, nick, ERR_IS_TOOLONG, 0, 0, 0);
		sSend(":%s NOTICE %s :>>%s", InfoServ, from, &content[NEWSCONTENTLEN]);
		return RET_SYNTAX;
	}

	/* if is_listhead is null, allocate mem for it
	 * and make that the new entry
	 */
	if (is_listhead == 0)
		newnews = is_listhead = (SomeNews *) oalloc(sizeof(SomeNews));
	else {						/* find the end */
		working = is_listhead;

		while (working->next != 0)
			working = working->next;

		newnews = working->next = (SomeNews *) oalloc(sizeof(SomeNews));
	}


	/* OK, let's fill up the news item structure
	 * with all relevant data
	 */
	strncpyzt(newnews->from, from, NICKLEN);	/* Sanity */
	newnews->importance = atoi(args[1]);
	if (newnews->importance > 0 && !opFlagged(nick, OINFOPOST))
		newnews->importance = 0;
	if (newnews->importance > 3 && !opFlagged(nick, OSERVOP))
		newnews->importance = 3;
	if (newnews->importance > 4 && !opFlagged(nick, OROOT))
		newnews->importance = 4;
	if (newnews->importance > 5)
		newnews->importance = 5;

	newnews->header = strdup(args[2]);
	newnews->timestamp = CTime;
	newnews->content = strdup(content);
	if (!newnews->content)
		abort();
	is_last_post_time = CTime;

	PutReply(InfoServ, nick, RPL_IS_POSTED, 0, 0, 0);

	sSend(":%s GLOBOPS :New article posted to database by %s, header: %s",
		  InfoServ, newnews->from, newnews->header);
	if (newnews->importance <= 2);
	else if (newnews->importance <= 3)
		sSend
			(":%s NOTICE $*.sorcery.net :A new article flagged important has been posted to my database, type \2/msg infoserv list\2 to view InfoServ articles.",
			 InfoServ);
	else if (newnews->importance <= 4)
		sSend
			(":%s NOTICE $*.sorcery.net :[\2Global Notice\2] (%s) %s [\2Please do not respond\2]",
			 InfoServ, newnews->header, newnews->content);
	else
		sSend
			(":%s PRIVMSG $*.sorcery.net :[\2Global Notice\2] (%s) %s [\2Please do not respond\2]",
			 InfoServ, newnews->header, newnews->content);
	return RET_OK_DB;
}

/**
 * @brief Delete an article
 */
ICMD(is_delete)
{

	SomeNews *news, *news2, *news3 = 0;
	int i = 1;

	if (numargs < 2) {
		PutReply(InfoServ, nick, ERR_IS_NEEDPARAM, 0, 0, 0);
		PutHelpInfo(InfoServ, nick, "HELP DELETE");
		return RET_SYNTAX;
	}

	if (isdigit(*args[1]) == 0) {
		PutReply(InfoServ, nick, ERR_IS_NEEDNUM, 0, 0, 0);
		return RET_SYNTAX;
	}

	for (news = is_listhead; news; news = news->next, i++)
		if (i == atoi(args[1])) {
			news2 = news->next;
			if (news3 == 0)
				is_listhead = news->next;
			else
				news3->next = news2;
			freeNews(news);
			news = news2;
			PutReply(InfoServ, nick, RPL_IS_DELETED_1ARG, i, 0, 0);
			return RET_OK_DB;
		} else
			news3 = news;

	PutReply(InfoServ, nick, ERR_IS_NOARTICLE_ARG1, i, 0, 0);
	return RET_NOTARGET;
}

/*------------------------------------------------------------------------*/

/* $Id$ */
