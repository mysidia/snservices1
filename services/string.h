/**
 * \file string.h
 * \brief String declarations and reply settings
 *
 * \mysid
 * \date August, 2001
 *
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

#ifndef __string_h__
#define __string_h__

/**
 * \brief Send out an error message
 */
#define PutError(cs, ct, rn, n1, n2, n3) \
        reply(cs, ct, rn, n1, n2, n3)

/**
 * \brief Send out an ordinary message
 */
#define PutReply(cs, ct, rn, n1, n2, n3) \
        reply(cs, ct, rn, n1, n2, n3)

/**
 * \brief Send out an info/help response 
 *  ex: "see /xxxxserv help for more info"
 */
#define PutHelpInfo(cs, ct, n1) \
        reply(cs, ct, RPL_MSGFORHELP, cs, n1, 0)

struct _userlist;

void reply(const char *cService, struct _userlist *cTo, int replyNum, ...);

typedef enum
{
   _dummy_item_hst_
} help_show_type;

/**
 * \brief Reply/Error/message codes
 */
typedef enum
{
/*00*/	ERR_NEEDREGNICK_1ARG,
	ERR_NICKNOTREG_1ARG,
	ERR_NICKBANISHED_1ARG,
	ERR_NOAHURTBYPASS,
	ERR_BADPW,
/**/	ERR_NOACCESS,
	ERR_NOTIDENTIFIED,
	ERR_AKILLSYNTAX_1ARG,
	RPL_AKILLHELP_2ARG,

	RPL_SWITCHIS_ARG2,
	RPL_SWITCHNOW_ARG2,
	RPL_AHURTBYPASS,
/**/	RPL_BYPASSISLOGGED,
	RPL_IDENTIFYOK_NOARG,
	RPL_IDENTIFYOK_NICKARG,
	RPL_CHKEY_DEAD,
	RPL_MSGFORHELP,
	RPL_MASKHELP,

/**/	RPL_AUTH_TYPES,
	RPL_AUTH_SEED,
	RPL_AUTH_OK_0ARG,
	RPL_AUTH_OK_1ARG,
	RPL_AUTH_NORESPONSE,
/**/	ERR_AUTH_CHAL,
	ERR_AUTH_NOTREGISTERED_2ARG,
	ERR_AUTH_BAD_1ARG,
	ERR_AUTH_NOTYPE,

	RPL_INFONLINE_ID,
	RPL_INFONLINE_NOID,
	RPL_NS_BANISH,
	RPL_NS_ENDINFO,

	RPL_IS_GREET_1ARG,
	RPL_IS_COPYRIGHT_1ARG,
	RPL_IS_NEWITEM,
	RPL_IS_NEWITEMS_1ARG,
	RPL_IS_HOWTOLIST,
	ERR_IS_UNKNOWNCMD_1ARG,
	ERR_IS_NEEDPARAM,
	ERR_IS_NEEDNUM,
	ERR_IS_NOARTICLE_ARG1,
	ERR_IS_NOARTICLES,
	RPL_IS_END_OF_LIST,
	RPL_IS_READ_HELP,
	ERR_IS_NEEDIMPORTANCE,
	ERR_IS_TOOLONG,
	RPL_IS_POSTED,
	RPL_IS_DELETED_1ARG,
	ERR_CS_INVALIDCHAN_1ARG,
	ERR_CHANNOTREG_1ARG,
	ERR_INVALID_TRIGVAR,

	ERR_SERVICE_NEEDREGNICK_1ARG,
	RPL_DBSAVE_1ARG,
	ERR_NEEDMEMONUM_1ARG,
	RPL_MEMO_HEADER1_3ARG,
	ERR_NOSUCH_MEMO_1ARG,
	RPL_MEMO_SAVED_1ARG,
	ERR_NOMEMOS,
	RPL_MS_LIST_HEAD1,
	RPL_MS_LIST_HEAD2,
	RPL_MS_LIST_FOOT,
	ERR_MS_NEEDNICKCHANMEMO,
	ERR_NOTREG_1ARG,
	ERR_MS_NOACCESS_1ARG,
	ERR_MS_TOOLONG_2ARG,
	RPL_MS_ALLDELETED_1ARG,
	RPL_MS_DELETED_1ARG,
	ERR_MS_DEL_SPECIFYMEMO,
	ERR_MS_BADINDEXNUMBER,
	RPL_MS_CLEAN_2ARG,
	ERR_MS_NOTFORWARD,
	RPL_MS_FORWARD_OFF,
	ERR_MS_FORWARD_SYNTAX_1ARG,
	ERR_BADPW_NICK_1ARG,
	RPL_MS_FORWARD_ON_1ARG,
	ERR_MS_NOMEMO_NEEDONOFF,
	RPL_MS_NOMEMO_ON,
	RPL_MS_NOMEMO_OFF,
	ERR_MS_NOMEMO_BADPARAM,
	RPL_MS_UNSEND_HEAD1,
	RPL_MS_UNSEND_HEAD2,
	RPL_MS_UNSENT_ALL,
	RPL_MS_UNSENT_1ARG,
	ERR_MS_UNSEND_NOSUCH_1ARG,
	RPL_MS_MEMOWAITING,
	RPL_MS_MEMOWAITING_1ARG,
	RPL_MS_MEMO2_2ARG,
	RPL_MS_MEMO3_2ARG,
	RPL_MS_MEMO4n_2ARG,
	ERR_MS_MBLOCK_NONE,
	RPL_MS_MBLOCK_HEAD,
	ERR_MS_MBLOCK_BADPARAM_1ARG,
	ERR_MS_MBLOCK_NOCHANGE,
	ERR_MS_MBLOCK_NOSUCH_1ARG,
	RPL_MS_MBLOCK_DELETED_1ARG,
	RPL_MS_MBLOCK_ADDED_1ARG,
	ERR_MS_MBLOCK_ALREADY_1ARG,
	ERR_MS_MBLOCK_TOOMANY,

	MAX_REPLY_STRING_NUM
} reply_type;

struct reply_string {
	char *string;
	int numargs;
	int show_help;
};

#ifdef __string_cc__
/**
 * \brief Message strings
 */
struct reply_string reply_table[] =
{
/*00*/	{"The nickname you are using, %s, is not registered.",
         1, 0},
	{"The nickname %s is not registered.", 1},
	{"Sorry, but the nickname %s is currently banished.", 1},
	{"\2NOTE\2 The nickname you are identifying to does not have "
	 "the select ban bypass permission needed to nullify your banned "
	 "status.", 0},
	{"Incorrect password."},
/**/	{"Access denied"},
	{"You must be identified with NickServ to use this command.", 0},
	{"Syntax: %s <time/-> <nick!user@host> <reason>", 1},
	{"Improper format.  Try /msg %s help %s", 2},

	{"Your \2%s\2 switch is currently \2%s\2", 2},
	{"Your \2%s\2 switch is now \2%s\2", 2},
	{"You are now identified to a registered nick and are thereby "
	 " bypassing the select ban on your site.", 0},
/**/	{"Note that this has been logged, and any abuse of the network from "
	 "the nickname may result in removal of your bypass access.", 0},

	{"Password accepted -- you are now identified", 0},
	{"Password accepted for nick \2%s\2.", 1},
	{"Warning: SENDPASS had been used to mail you a password recovery "
	 "key.  Since you have identified, that key is no longer valid.", 1},
	{"Try /msg %s %s for more information", 1},
	{"See '/msg %s HELP ACCESS' and '/msg %s HELP ADDMASK' for information on host-based recognition.", 2},

/**/	{"200 MD5 PLAIN"},
	{"205 S/MD5 1.0 %x:%x"},
	{"210 - Authentication accepted -- you are now identified.", 0},
	{"210 - Authentication accepted for nick \2%s\2.", 1},
	{"215 - Missing response."},
/**/	{"300 - You need to ask me for a challenge first."},
	{"500 - The %s %s is not registered.", 2},
	{"500 - Invalid authentication for nick %s.", 1},
	{"510 - That authentication type is not available."},


	{"%s is online and identified. (Acc %d)"},
	{"%s is online but not identified. (Acc %d)"},
	{"%s is banished and cannot be used."},
	{"End of Info"},
	{"%s"},
	{"Services version %s (c) Chip Norkus, Max Byrd, Greg Poma, Michael Graff, James Hess, and Dafydd James 1996-2001"},
	{"A news item was posted since you last read the list, type "
	 "\2/msg infoserv list\2 if you wish to examine the list of "
	 "articles."},
	{"%d news items have been posted since you last read the list, "
	 "type \2/msg infoserv list\2 if you wish to examine the list of"
	 " articles."},
	{"\2/INFOSERV LIST\2 to see the list of news articles. "
	 " Type \2/INFOSERV HELP\2 for more information."},
	{"Unknown command %s."},
	{"Not enough parameters."},
	{"Please specify an article *number*"},
	{"There is no article number %d."},
	{"There are no articles in the database right now."},
	{"End of list"},
	{"Type \2/msg infoserv read <number>\2 to view an article."},
	{"Please specify a numeric importance level (0-5)."},
	{"Posting too long:"},
	{"Article posted."},
	{"News article %d has been deleted."},

	{"Invalid channel name, %s."},
	{"%s is not registered."},
	{"Invalid trigger variable."},
/**/
	{"You nick must be registered to use %s."},
	{"Next database synch(save) in %ld minutes."},
	{"You must specify a memo to %s."},
	{"Memo (%3i) from: %s to: %s"},
	{"No such memo (%i)"},
	{"Memo %3i saved."},
	{"You have no memos."},
	{"Memos:"},
	{"  # Time Sent                  Flags From"},
	{"End of Memo List"},
	{"You must specify a nick/channel and memo to send."},
	{"%s is not registered."},
	{"You do not have access to send memos to %s"},
	{"Your memo (%d) was too long.  All memos must be %d characters or shorter."},
	{"All%s memos marked as deleted."},
	{"Memo %i marked as deleted."},
	{"You must specify a specific memo, or \"all\", \"read\""},
	{"Please use a proper index number"},
	{"%u of %u memos cleared from your MemoBox"},
	{"You're not currently auto-forwarding memos"},
	{"Your memos are no longer auto-forwarded"},
	{"Your ``%s'' request did not fit any valid syntax."},
	{"Incorrect password for nick %s"},
	{"Your memos are now forwarded to %s automatically."},
	{"You must specify either ON or OFF. See \"/msg %s help nomemo\" for details."},
	{"You will no longer receive memos."},
	{"You can now receive memos again."},
	{"Invalid parameter. /msg %s help nomemo for help"},
	{"Memos from you:"},
	{"  # Time Sent                  Flags To"},
	{"All sent memos deleted."},
	{"Removed memo %d."},
	{"Memo %d does not exist."},
	{"You have 1 memo waiting."},
	{"You have %u memos waiting."},
	{"%3u unread.   %3u saved."},
	{"%3u forward.  %3u replies."},
	{"%3u marked as deleted.  To clear these:  /msg %s purge"},
	{"You have no memo blocks."},
	{"Now listing blocked memo senders:"},
	{"Invalid parameter. /msg %s help mblock for help."},
	{"No changes made to memo sender block list."},
	{"You do not have a memo block in place for %s"},
	{"Memo block removed for %s. You may now receive memos from this user again."},
	{"New memo block in place for %s. You will no longer receive memos from this user."},
	{"You already have a memo block in place for %s"},
	{"Unable to comply. You have too many Memo Blocks in place already. Consider \"/msg MemoServ NOMEMO ON\""}
};
#else
extern struct reply_string reply_table[];
#endif

const char* get_reply(reply_type reply);

#endif /* __string_h__ */
