/* $Id$ */

/*
 * Copyright (c) 1996-1997 Chip Norkus
 * Copyright (c) 1997 Max Byrd
 * Copyright (c) 1997 Greg Poma
 * Copyright (c) 2000 James Hess
 * Copyright (c) 1999 Portions Copyright
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
 * @file memoserv.c
 * @brief Memo/messaging service
 *
 * System for sending messages to offline/online registered users
 *
 * \wd \taz \greg \mysid
 * \date 2001-2002
 *
 * $Id$
 */

#include "services.h"
#include "memoserv.h"
#include "nickserv.h"
#include "chanserv.h"
#include "queue.h"
#include "macro.h"
#include "log.h"
#include "hash/md5pw.h"

// *INDENT-OFF*
MCMD(ms_help);
MCMD(ms_read);
MCMD(ms_savememo);
MCMD(ms_send);
MCMD(ms_forward);
MCMD(ms_nomemo);
MCMD(ms_clean);
MCMD(ms_delete);
MCMD(ms_list);
MCMD(ms_unsend);
MCMD(ms_mblock);

int  ms_sendMemo(char *, char *, RegNickList *, const char *, char *, UserList *);
static cmd_return ms_AddMemoBlock(UserList *, char *);
static cmd_return ms_DelMemoBlock(UserList *, char *);
char NickGetEnc(RegNickList *);

/// Nick is registered and user is identified to that nickname
#define CMD_REGIDENT (CMD_REG|CMD_IDENT)

/// Dispatch table for handling MemoServ commands
interp::service_cmd_t memoserv_commands[] = {
  /* string             function         Flags      L */
   { "help",		ms_help,	0, LOG_NO,  0, 3 },
   { "read",		ms_read,	0, LOG_NO,  CMD_REGIDENT, 7 },
   { "save",		ms_savememo,	0, LOG_NO,  CMD_REGIDENT, 7 },
   { "list",  		ms_list,	0, LOG_NO,  CMD_REGIDENT, 7 },
   { "send",		ms_send,	0, LOG_NO,  CMD_REGIDENT, 9 },
   { "sendaop",		ms_send,	0, LOG_NO,  CMD_REGIDENT, 9 },
   { "sendsop",		ms_send,	0, LOG_NO,  CMD_REGIDENT, 9 },
   { "sendvop",		ms_send,	0, LOG_NO,  CMD_REGIDENT, 9 },
   { "del", 		ms_delete,	0, LOG_NO,  CMD_REGIDENT, 5 },
   { "delete",		ms_delete,	0, LOG_NO,  CMD_REGIDENT, 5 },
   { "purge",		ms_clean,	0, LOG_NO,  CMD_REGIDENT, 5 },
   { "clean",		ms_clean,	0, LOG_NO,  CMD_REGIDENT, 5 },
   { "forward",		ms_forward,	0, LOG_NO,  CMD_REGIDENT, 5 },
   { "nomemo",  	ms_nomemo,	0, LOG_NO,  CMD_REGIDENT, 5 },
   { "unsend",		ms_unsend,	0, LOG_NO,  CMD_REGIDENT, 5 },
   { "mblock",		ms_mblock,	0, LOG_NO,  CMD_REGIDENT, 5 },
   { NULL,		NULL,		0, LOG_NO,  0, 0 }
};
// *INDENT-ON*

/*!
 * \brief Parse a MemoServ message
 * \param tmp Pointer to online user initiating the message
 * \param args Args of the message, where args[0] is the command and
 *        the extra parameters follow
 * \param numargs Highest index in the args[] array passed plus 1.
 *        so args[numargs - 1] is the highest index that can be safely
 *        accessed.
 */
void sendToMemoServ(UserList * nick, char **args, int numargs)
{
	interp::parser * cmd;

	cmd = new interp::parser(MemoServ, getOpFlags(nick),
							 memoserv_commands, args[0]);
	if (!cmd)
		return;

	if ((cmd->getCmdFlags() & CMD_REG) && !nick->reg) {
		PutReply(MemoServ, nick, ERR_SERVICE_NEEDREGNICK_1ARG, MemoServ, 0, 0);
		return;
	}

	if (nick->reg && !nick->reg->memos)
		ADD_MEMO_BOX(nick->reg);

	switch (cmd->run(nick, args, numargs)) {
	default:
		break;
	case RET_FAIL:
		PutReply(MemoServ, nick, ERR_IS_UNKNOWNCMD_1ARG, args[0], 0, 0);
		PutHelpInfo(MemoServ, nick, "HELP");
		break;
	case RET_OK_DB:
		PutReply(MemoServ, nick, RPL_DBSAVE_1ARG,
			(long)((nextMsync - CTime) / 60), 0, 0);
		break;
	}
	return;
}

/**
 * \mscmd Help
 * \syntax Help \<topic>
 */
MCMD(ms_help)
{
	help(nick->nick, MemoServ, args, numargs);
	return RET_OK;
}

/**
 * \mscmd Read
 * \syntax Read \<index number>
 * \plr Read a memo.
 */
MCMD(ms_read)
{
	char *from = nick->nick;
	int i;
	int idx;
	MemoList *tmp;
	struct tm *tm;
	char temp[30];
	RegNickList *sender;

	if (numargs < 2) {
		PutReply(MemoServ, nick, ERR_NEEDMEMONUM_1ARG, "read", 0, 0);
		return RET_SYNTAX;
	}

	idx = 0;
	if (!strcasecmp(args[1], "all")) {
		for (tmp = LIST_FIRST(&nick->reg->memos->mb_memos);
			 tmp; tmp = LIST_NEXT(tmp, ml_lst)) {
			idx++;
			PutReply(MemoServ, nick, RPL_MEMO_HEADER1_3ARG,
				  idx, tmp->from, tmp->to);
			tm = localtime(&tmp->sent);
			strftime(temp, 30, "%a %Y-%b-%d %T %Z", tm);
			sSend(":%s NOTICE %s :Sent at: %-30s  Flagged %c%c%c%c%c",
				  MemoServ, from, temp,
				  (tmp->flags & MEMO_UNREAD ? '*' : ' '),
				  (tmp->flags & MEMO_DELETE ? 'D' : ' '),
				  (tmp->flags & MEMO_SAVE ? 'S' : ' '),
				  (tmp->flags & MEMO_FWD ? 'f' : ' '),
				  (tmp->flags & MEMO_REPLY ? 'r' : ' '));
			sSend(":%s NOTICE %s :%s", MemoServ, from, tmp->memotxt);
			if (tmp->flags & MEMO_UNREAD) {
				tmp->flags &= ~MEMO_UNREAD;
				sender = getRegNickData(tmp->from);
				if (sender)
					LIST_REMOVE(tmp, ml_sent);
			}
		}
		return RET_OK;
	}

	i = atoi(args[1]);

	if (i > nick->reg->memos->memocnt || i <= 0) {
		PutReply(MemoServ, nick, ERR_NOSUCH_MEMO_1ARG, i, 0, 0);
		return RET_NOTARGET;
	}

	idx = 0;
	for (tmp = LIST_FIRST(&nick->reg->memos->mb_memos);
		 tmp; tmp = LIST_NEXT(tmp, ml_lst)) {
		idx++;
		if (idx == i)
			break;
	}

	if (tmp == NULL) {
		sSend(":%s NOTICE %s :%s internal error",
			  MemoServ, from, MemoServ);
		return RET_FAIL;
	}

	/* Memo (XXX) from YYYY to ZZZZ  */
	PutReply(MemoServ, nick, RPL_MEMO_HEADER1_3ARG, idx, tmp->from, tmp->to);
	tm = localtime(&tmp->sent);
	strftime(temp, 30, "%a %Y-%b-%d %T %Z", tm);
	sSend(":%s NOTICE %s :Sent at: %-30s  Flagged %c%c%c%c%c",
		  MemoServ, from, temp,
		  (tmp->flags & MEMO_UNREAD ? '*' : ' '),
		  (tmp->flags & MEMO_DELETE ? 'D' : ' '),
		  (tmp->flags & MEMO_SAVE ? 'S' : ' '),
		  (tmp->flags & MEMO_FWD ? 'f' : ' '),
		  (tmp->flags & MEMO_REPLY ? 'r' : ' '));
	sSend(":%s NOTICE %s :%s", MemoServ, from, tmp->memotxt);

	if (tmp->flags & MEMO_UNREAD) {
		tmp->flags &= ~MEMO_UNREAD;
		sender = getRegNickData(tmp->from);
		if (sender)
			LIST_REMOVE(tmp, ml_sent);
	}
	return RET_OK;
}

/**
 * \mscmd Save
 * \syntax Save \<index number>
 * \plr Save a memo.
 */
MCMD(ms_savememo)
{
	char *from = nick->nick;
	int i, idx;
	MemoList *tmp;

	if (numargs < 2) {
		PutReply(MemoServ, nick, ERR_NEEDMEMONUM_1ARG, "save", 0, 0);
		return RET_SYNTAX;
	}

	idx = 0;
	i = atoi(args[1]);

	if (i > nick->reg->memos->memocnt || i <= 0) {
		PutReply(MemoServ, nick, ERR_NOSUCH_MEMO_1ARG, i, 0, 0);
		return RET_NOTARGET;
	}

	for (tmp = LIST_FIRST(&nick->reg->memos->mb_memos);
		 tmp; tmp = LIST_NEXT(tmp, ml_lst)) {
		idx++;
		if (idx == i)
			break;
	}

	if (tmp == NULL) {
		sSend(":%s NOTICE %s :%s internal error",
			  MemoServ, from, MemoServ);
		return RET_FAIL;
	}

	tmp->flags |= MEMO_SAVE;

	PutReply(MemoServ, nick, RPL_MEMO_SAVED_1ARG, idx, 0, 0);
	return RET_OK_DB;
}

/**
 * \mscmd List
 * \syntax List
 * \plr Show listing of received memos
 */
MCMD(ms_list)
{
	char *from = nick->nick;
	MemoList *tmp;
	struct tm *tm;
	int idx;
	char temp[30];
	char search[NICKLEN + 2];

	idx = 0;

	memset(search, 0, NICKLEN + 2);
	if (numargs >= 2)
		strncpyzt(search, args[1], NICKLEN);

	if (LIST_FIRST(&nick->reg->memos->mb_memos) == NULL) {
		PutReply(MemoServ, nick, ERR_NOMEMOS, 0, 0, 0);
		return RET_OK;
	}

	PutReply(MemoServ, nick, RPL_MS_LIST_HEAD1, 0, 0, 0);
	PutReply(MemoServ, nick, RPL_MS_LIST_HEAD2, 0, 0, 0);

	for (tmp = LIST_FIRST(&nick->reg->memos->mb_memos);
		 tmp; tmp = LIST_NEXT(tmp, ml_lst)) {
		idx++;
		if (search[0]) {
			if (!match(search, tmp->from)) {
				tm = localtime(&tmp->sent);
				strftime(temp, 30, "%a %Y-%b-%d %T %Z", tm);
				sSend(":%s NOTICE %s :%3u %-26s %c%c%c%c%c %s",
					  MemoServ, from, idx, temp,
					  (tmp->flags & MEMO_UNREAD ? '*' : ' '),
					  (tmp->flags & MEMO_DELETE ? 'D' : ' '),
					  (tmp->flags & MEMO_SAVE ? 'S' : ' '),
					  (tmp->flags & MEMO_FWD ? 'f' : ' '),
					  (tmp->flags & MEMO_REPLY ? 'r' : ' '), tmp->from);
			}
		} else {
			tm = localtime(&tmp->sent);
			strftime(temp, 30, "%a %Y-%b-%d %T %Z", tm);
			sSend(":%s NOTICE %s :%3u %-26s %c%c%c%c%c %s",
				  MemoServ, from, idx, temp,
				  (tmp->flags & MEMO_UNREAD ? '*' : ' '),
				  (tmp->flags & MEMO_DELETE ? 'D' : ' '),
				  (tmp->flags & MEMO_SAVE ? 'S' : ' '),
				  (tmp->flags & MEMO_FWD ? 'f' : ' '),
				  (tmp->flags & MEMO_REPLY ? 'r' : ' '), tmp->from);
		}
	}
	PutReply(MemoServ, nick, RPL_MS_LIST_FOOT, 0, 0, 0);
	return RET_OK;
}

/// User-interface for memo sending
cmd_return ui_ms_send(UserList *nick, int numargs, char *args[], int level)
{
	char *from = nick->nick, *memo;
	RegNickList *sendto;
	RegChanList *chan;
	cAccessList *tmp;
	const char *tmpName;
	int i, len, fIsChannel;

	sendto = NULL;
	chan = NULL;

	if (numargs < 3) {
		PutReply(MemoServ, nick, ERR_MS_NEEDNICKCHANMEMO, 0, 0, 0);
		return RET_SYNTAX;
	}

	if (args[1][0] == '#')
		fIsChannel = 1;
	else
	{
		fIsChannel = 0;
		sendto = getRegNickData(args[1]);
		if (sendto == NULL) {
			PutReply(MemoServ, nick, ERR_NOTREG_1ARG, args[1], 0, 0);
			return RET_NOTARGET;
		}
	}


	/*
	 * If sending to a channel, make certain it exists, and they are
	 * a channel operator of the correct level
	 */
	if (fIsChannel) {
		chan = getRegChanData(args[1]);
		if (chan == NULL) {
			PutReply(MemoServ, nick, ERR_CHANNOTREG_1ARG, args[1], 0, 0);
			return RET_NOTARGET;
		}
		if ((getChanOp(chan, from) < chan->memolevel)) {
			PutReply(MemoServ, nick, ERR_MS_NOACCESS_1ARG, chan->name, 0, 0);
			return RET_NOPERM;
		}
	}

	/*
	 * Count up the total length of the memo they intend to send.  If
	 * it is going to be too long, complain.
	 */
	len = strlen(args[2]);
	if (3 < numargs)
		len++;
	for (i = 3; i < numargs; i++) {
		len += strlen(args[i]);	
		if (i+1 < numargs)
			len++; /* plus the trailing space */
	}
	len++;						/* The terminating null */

	if (len >= MEMOLEN) {
		PutReply(MemoServ, nick, ERR_MS_TOOLONG_2ARG, len, MEMOLEN - 1, 0);
		return RET_EFAULT;
	}

	/*
	 * generate the memo string
	 */
	memo = (char *)oalloc(len+1);
	strcpy(memo, args[2]);		/* strcpy is safe */
	for (i = 3; i < numargs; i++) {
		strcat(memo, " ");		/* strcat is safe */
		strcat(memo, args[i]);	/* strcat is safe */
	}

	/*
	 * send the thing (user)
	 */
	if (fIsChannel == 0) {
		if (ms_sendMemo(from, memo, sendto, args[1], NULL, nick) == -1)
		{
			if (memo)
				FREE(memo);
			return RET_FAIL;
		}
		return RET_OK_DB;
	}


	/*
	 * send the thing (channel)
	 */
	for (tmp = chan->firstOp; tmp != NULL; tmp = tmp->next) {
		char *memocpy;

		if (tmp->uflags < level)
			continue;

		tmpName = tmp->nickId.getNick();

		if (tmpName == NULL)
			continue;

		sendto = getRegNickData(tmpName);

		if (sendto == NULL || memo == NULL)
			continue;

		memocpy = strdup(memo);	/*! \bug XXX what a hack...  */

		if (ms_sendMemo(from, memocpy, sendto, tmpName, chan->name, nick)
	        == -1 && memocpy)
			FREE(memocpy);
	}

	FREE(memo);
	return RET_OK_DB;
}

/**
 * \mscmd Send
 * \syntax Send \<nickname> \<message>
 * \plr Send a memo containing the message \e message to the
 *      registered user \e nickname.
 * \note \e message is limited to MEMOLEN (default 350) characters.
 */
MCMD(ms_send) {
    int ulev_send = 0;

	if (numargs < 1)
		return RET_FAIL;

	if (!str_cmp(args[0], "sendaop"))
		ulev_send = AOP;
	else if (!str_cmp(args[0], "sendsop"))
		ulev_send = MSOP;
	else if (!str_cmp(args[0], "sendvop"))
		ulev_send = MAOP;

	return ui_ms_send(nick, numargs, args, ulev_send);
}

/// /ms sendsop message handler
MCMD(ms_sendsop) {
	return ui_ms_send(nick, numargs, args, MSOP);
}

/**
 * \brief Sends a memo out, can fail according to NOMEMO, MBLOCK, Max memos,
 *        and other restrictions.
 * \param sender Nickname of that who sent the message
 * \param memo Message text
 * \param sendto Pointer to registered nickname of recipient
 * \param to Recipient nickname
 * \param senderptr Pointer to online user item of memo sender
 * \return -1 on failure, 0 on success
 * \warning The value of the 'memo' pointer is stored on the sent memo structure
 */
int
ms_sendMemo(char *sender, char *memo, RegNickList * sendto,
			const char *to, char *tochan, UserList * senderptr)
{
	MemoList *newmemo;
	UserList *nick = NULL;
	RegNickList *reg_sender;
	MemoBlock *mblock;

	/**
	 * make certain the nickname we are sending to is registered.
	 * \bug XXX Why isn't this done before we ever get here?
	 */
	if (sendto == NULL) {
		sSend(":%s NOTICE %s :%s is not registered.",
			  MemoServ, sender, to);
		return -1;
	}

	reg_sender = getRegNickData(sender);

	/*
	 * if the memo is NULL, we are in deep trouble
	 */
	if (memo == NULL) {
		sSend(":%s GLOBOP :Invalid call to send a memo from %s",
			  MemoServ, sender);
		return -1;
	}

	/*
	 * If the receiver has no memo box, create one.  We will check
	 * this again if we switch to a new recepient later...
	 */
	if (sendto->memos == NULL)
		ADD_MEMO_BOX(sendto);

	/*
	 * allocate a new memo entry, and fill in the details
	 */
	newmemo = (MemoList *) oalloc(sizeof(MemoList));
	strcpy(newmemo->from, sender);
	newmemo->sent = CTime;
	newmemo->flags |= MEMO_UNREAD;
	if (tochan == NULL)
		strcpy(newmemo->to, to);
	else
		strcpy(newmemo->to, tochan);
	newmemo->memotxt = memo;	/* allocated by caller */
	newmemo->realto = sendto;

	/*
	 * If the user has memos forwarded, send it there instead
	 */
	if (((sendto->memos->flags & MFORWARDED) == MFORWARDED)
		&& (sendto->memos->forward != NULL)) {
		if (senderptr && isOper(senderptr))
			sSend(":%s NOTICE %s :Forwarding memo from %s to %s",
				  MemoServ, sender, sendto->nick,
				  sendto->memos->forward->nick);
		newmemo->flags |= MEMO_FWD;
		sendto = sendto->memos->forward;
		if (sendto->memos == NULL)	/* gotta check _again_ */
			ADD_MEMO_BOX(sendto);

	}

	if (sender)
		nick = getNickData(sender);

	/*
	 * Make certain that user can receive memos
	 */
	if ((sendto->memos->flags & MNOMEMO)
		&& (!nick || !opFlagged(nick, OVERRIDE))) {
		sSend(":%s NOTICE %s :%s does not allow memos to be sent to them",
			  MemoServ, sender, sendto->nick);
		FREE(newmemo);
		return -1;
	}

	/*
	 * Make sure user isn't being blocked
	 */
	if (reg_sender)
	{
		mblock = getMemoBlockData(sendto->memos, reg_sender);

		if (mblock != NULL) {
			sSend
				(":%s NOTICE %s :Unable to send memo to recipient %s. This user has chosen to block your memos.",
				 MemoServ, sender, sendto->nick);
			FREE(newmemo);
			return -1;
		}
	}

	/*
	 * Check to see if the user has too many memos.  If so, this one
	 * cannot be sent.
	 */
	if (sendto->memos->memocnt >= sendto->memos->max
		&& !opFlagged(nick, OVERRIDE | OSERVOP)) {
		sSend(":%s NOTICE %s :%s has received too many memos.", MemoServ,
			  sender, sendto->nick);
		FREE(newmemo);
		return -1;
	}

	/*
	 * Add the memo to the user's memo list, and if they are online,
	 * let them know they have a new memo waiting.
	 */
	LIST_ENTRY_INIT(newmemo, ml_lst);
	LIST_INSERT_HEAD(&sendto->memos->mb_memos, newmemo, ml_lst);
	sendto->memos->memocnt++;
	if (getNickData(sendto->nick))
		sSend(":%s NOTICE %s :You have a new memo from %s.",
			  MemoServ, to, sender);
	sSend(":%s NOTICE %s :Memo has been sent to %s",
		  MemoServ, sender, sendto->nick);
	if (reg_sender)
		LIST_INSERT_HEAD(&reg_sender->memos->mb_sent, newmemo, ml_sent);
	return 0;
}

/**
 * \mscmd Delete
 * \syntax Delete \<index number>
 * \plr Mark a memo deleted (Use /MS clean to erase permanently)
 */
/// Delete a memo (message handler)
MCMD(ms_delete)
{
	MemoList *tmp;
	MemoList *tmp_next;
	int i;
	int idx;

	if (numargs < 2) {
		PutReply(MemoServ, nick, ERR_MS_DEL_SPECIFYMEMO, 0, 0, 0);
		return RET_SYNTAX;
	}

	if (nick->reg->memos->memocnt == 0) {
		PutReply(MemoServ, nick, ERR_NOMEMOS, 0, 0, 0);
		return RET_OK;
	}

	if (!strcasecmp(args[1], "all")) {
		tmp = LIST_FIRST(&nick->reg->memos->mb_memos);
		while (tmp) {
			tmp_next = LIST_NEXT(tmp, ml_lst);
			tmp->flags |= MEMO_DELETE;
			if (tmp->flags & MEMO_UNREAD)
				LIST_REMOVE(tmp, ml_sent);
			tmp = tmp_next;
		}
		PutReply(MemoServ, nick, RPL_MS_ALLDELETED_1ARG, "", 0, 0);
		return RET_OK_DB;
	}

	if (!strcasecmp(args[1], "new")) {
		tmp = LIST_FIRST(&nick->reg->memos->mb_memos);
		while (tmp) {
			tmp_next = LIST_NEXT(tmp, ml_lst);
			if (tmp->flags & MEMO_UNREAD) {
				tmp->flags |= MEMO_DELETE;
				LIST_REMOVE(tmp, ml_sent);
			}
			tmp = tmp_next;
		}

		PutReply(MemoServ, nick, RPL_MS_ALLDELETED_1ARG, " unread", 0, 0);
		return RET_OK_DB;
	}

	if (!strcasecmp(args[1], "old") || !strcasecmp(args[1], "read")) {
		tmp = LIST_FIRST(&nick->reg->memos->mb_memos);
		while (tmp) {
			tmp_next = LIST_NEXT(tmp, ml_lst);
			if (!(tmp->flags & MEMO_UNREAD))
				tmp->flags |= MEMO_DELETE;

			tmp = tmp_next;
		}

		PutReply(MemoServ, nick, RPL_MS_ALLDELETED_1ARG, " read", 0, 0);
		return RET_OK_DB;
	}

	i = atoi(args[1]);

	if (i <= 0 || i > nick->reg->memos->memocnt) {
		PutReply(MemoServ, nick, ERR_MS_BADINDEXNUMBER, 0, 0, 0);
		return RET_SYNTAX;
	}

	idx = 0;
	for (tmp = LIST_FIRST(&nick->reg->memos->mb_memos);
		 tmp; tmp = LIST_NEXT(tmp, ml_lst)) {
		idx++;
		if (idx == i) {
			tmp->flags |= MEMO_DELETE;
			if (tmp->flags & MEMO_UNREAD)
				LIST_REMOVE(tmp, ml_sent);
			break;
		}
	}
	PutReply(MemoServ, nick, RPL_MS_DELETED_1ARG, i, 0, 0);
	return RET_OK_DB;
}

/**
 * \mscmd Clean
 * \syntax Clean
 * \plr Expunges memos that are marked deleted.
 */
/* ARGSUSED1 */
MCMD(ms_clean)
{
	MemoList *tmp;
	MemoList *tmp_next;
	u_int i;
	u_int x;

	i = x = 0;

	tmp = LIST_FIRST(&nick->reg->memos->mb_memos);
	while (tmp != NULL) {
		tmp_next = LIST_NEXT(tmp, ml_lst);
		x++;
		if (tmp->flags & MEMO_DELETE) {
			delMemo(nick->reg->memos, tmp);
			i++;
		}
		tmp = tmp_next;
	}

	PutReply(MemoServ, nick, RPL_MS_CLEAN_2ARG, i, x, 0);
	return RET_OK_DB;
}

/**
 * \mscmd Forward
 * \syntax Forward {[\<nick> \<password>]}
 * \plr Forward all memos to another registered user.
 * \plr If no target nick is specified, then forwarding is disabled.
 * \note Memos are not forwarded if they were received through a nick
 *       forwarding to a nick with forward enabled.
 */
/// Forward future memos to another nick
MCMD(ms_forward)
{
	RegNickList *from, *to;

	if (!nick->reg) {
		PutReply(MemoServ, nick, ERR_NEEDREGNICK_1ARG, nick->nick, 0, 0);
		return RET_FAIL;
	}

	from = nick->reg;

	if (numargs <= 1) {
		if (!from->memos)
			ADD_MEMO_BOX(from);

		if (!from->memos->forward) {
			PutReply(MemoServ, nick, ERR_MS_NOTFORWARD, 0, 0, 0);
			return RET_FAIL;
		}

		from->memos->forward = 0;
		from->memos->flags &= ~MFORWARDED;
		PutReply(MemoServ, nick, RPL_MS_FORWARD_OFF, 0, 0, 0);
		return RET_OK_DB;
	}

	if (numargs == 2) {
		PutReply(MemoServ, nick, ERR_MS_FORWARD_SYNTAX_1ARG, "forward", 0, 0);
		PutHelpInfo(MemoServ, nick, "HELP FORWARD");
		return RET_SYNTAX;
	}

	to = getRegNickData(args[1]);
	if (to == NULL) {
		PutReply(MemoServ, nick, ERR_NICKNOTREG_1ARG, args[1], 0, 0);
		return RET_NOTARGET;
	}

	if (Valid_pw(args[2], to->password, NickGetEnc(to)) == 0) {
		PutReply(MemoServ, nick, ERR_BADPW_NICK_1ARG, to->nick, 0, 0);
		return RET_BADPW;
	}

	if (from->memos == NULL)
		ADD_MEMO_BOX(from);

	from->memos->forward = to;
	from->memos->flags |= MFORWARDED;

	PutReply(MemoServ, nick, RPL_MS_FORWARD_ON_1ARG, args[1], 0, 0);
	return RET_OK_DB;
}

/**
 * \mscmd Nomemo
 * \syntax Nomemo ("on" | "off")
 * \plr Enable or disable blocking of incoming memos.
 */
MCMD(ms_nomemo)
{
	if (!nick->reg) {
		PutReply(MemoServ, nick, ERR_NEEDREGNICK_1ARG, nick->nick, 0, 0);
		return RET_FAIL;
	}

	if (numargs < 2) {
		PutReply(MemoServ, nick, ERR_MS_NOMEMO_NEEDONOFF, 0, 0, 0);
		return RET_SYNTAX;
	}

	if (!strcasecmp(args[1], "on")) {
		nick->reg->memos->flags |= MNOMEMO;
		PutReply(MemoServ, nick, RPL_MS_NOMEMO_ON, 0, 0, 0);
	} else if (!strcasecmp(args[1], "off")) {
		nick->reg->memos->flags &= ~MNOMEMO;
		PutReply(MemoServ, nick, RPL_MS_NOMEMO_OFF, 0, 0, 0);
	} else {
		PutReply(MemoServ, nick, ERR_MS_NOMEMO_BADPARAM, 0, 0, 0);
		return RET_SYNTAX;
	}
	return RET_OK_DB;
}

/**
 * \mscmd Unsend
 * \syntax Unsend
 * \plr List memos that can be unsent
 *
 * \mscmd Unsend (\<index number> | "all")
 * \plr Unsend a memo (index into the 'unsend' list)
 *
 * \note A memo cannot be unsent once a recipient has read it.
 */
MCMD(ms_unsend)
{
	RegNickList *to;
	RegNickList *from;
	int idx = 0;
	struct tm *tm;
	MemoList *memos;
	MemoList *memos_next;
	char temp[30];
	int i;

	from = nick->reg;

	if (numargs < 2) {
		PutReply(MemoServ, nick, RPL_MS_UNSEND_HEAD1, 0, 0, 0);
		PutReply(MemoServ, nick, RPL_MS_UNSEND_HEAD2, 0, 0, 0);

		for (memos = LIST_FIRST(&from->memos->mb_sent);
			 memos; memos = LIST_NEXT(memos, ml_sent)) {
			idx++;
			tm = localtime(&memos->sent);
			strftime(temp, 30, "%a %Y-%b-%d %T %Z", tm);
			sSend(":%s NOTICE %s :%3u %-26s %c%c%c%c%c %s",
				  MemoServ, from->nick, idx, temp,
				  (memos->flags & MEMO_UNREAD ? '*' : ' '),
				  (memos->flags & MEMO_DELETE ? 'D' : ' '),
				  (memos->flags & MEMO_SAVE ? 'S' : ' '),
				  (memos->flags & MEMO_FWD ? 'f' : ' '),
				  (memos->flags & MEMO_REPLY ? 'r' : ' '),
				  memos->realto->nick);
		}

		return RET_OK;
	}

	if (!strcasecmp(args[1], "all")) {
		memos = LIST_FIRST(&from->memos->mb_sent);
		while (memos != NULL) {
			memos_next = LIST_NEXT(memos, ml_sent);
			to = memos->realto;
			if (!to)
				return RET_OK_DB;
			LIST_REMOVE(memos, ml_sent);
			delMemo(to->memos, memos);
			memos = memos_next;
		}
		PutReply(MemoServ, nick, RPL_MS_UNSENT_ALL, 0, 0, 0);
		return RET_OK_DB;
	}

	i = atoi(args[1]);
	idx = 0;
	for (memos = LIST_FIRST(&from->memos->mb_sent);
		 memos; memos = LIST_NEXT(memos, ml_sent)) {
		idx++;
		if (i == idx) {
			to = memos->realto;
			if (!to)
				return RET_OK;
			PutReply(MemoServ, nick, RPL_MS_UNSENT_1ARG, i, 0, 0);
			LIST_REMOVE(memos, ml_sent);
			delMemo(to->memos, memos);
			return RET_OK_DB;
		}
	}

	PutReply(MemoServ, nick, ERR_MS_UNSEND_NOSUCH_1ARG, i, 0, 0);
	return RET_FAIL;
}

/**
 * \brief cleanMemos() - clean a user's memobox of deleted entries
 * \param nick Pointer to an online user item
 */
void cleanMemos(UserList * nick)
{
	MemoList *tmp;
	MemoList *tmp_next;

	if (nick->reg->memos == NULL)
		return;

	if (nick->reg->memos->flags & MSELFCLEAN) {
		tmp = LIST_FIRST(&nick->reg->memos->mb_memos);
		while (tmp != NULL) {
			tmp_next = LIST_NEXT(tmp, ml_lst);
			if (tmp->flags & MEMO_DELETE)
				delMemo(nick->reg->memos, tmp);
			tmp = tmp_next;
		}
	}
}

/**
 * \brief Deletes a user's memo
 * \param mbox Pointer to a user memobox
 * \param memo Pointer to a user memobox entry item (memo)
 */
void delMemo(MemoBox * mbox, MemoList * memo)
{
	LIST_REMOVE(memo, ml_lst);
	mbox->memocnt--;
	FREE(memo->memotxt);
	FREE(memo);
}

/**
 * \brief Generates user memo stats for display during signon
 * \param nick Pointer to an online user item
 */
void checkMemos(UserList * nick)
{
	MemoList *tmp;
	char *from;
	int fwd = 0;
	int del = 0;
	int save = 0;
	int reply_ct = 0;
	int unread = 0;
	int nummemos = 0;

	/* If the user isn't, if the user isn't registered, or if the user
	 * has no memo box
	 */
	if (nick == NULL)
		return;
	if (nick->reg == NULL)
		return;
	if (nick->reg->memos == NULL)
		return;

	/* Make sure the user is identified, or matches an access list */
	if (!checkAccess(nick->user, nick->host, nick->reg)
		&& !isIdentified(nick, nick->reg))
		return;

	/* Just to make things easier, from = the sending user */
	from = nick->nick;

	/* Step through the list of memos, generate stats for the user */
	for (tmp = LIST_FIRST(&nick->reg->memos->mb_memos);
		 tmp; tmp = LIST_NEXT(tmp, ml_lst)) {
		nummemos++;
		if (tmp->flags & MEMO_UNREAD)
			unread++;
		if (tmp->flags & MEMO_SAVE)
			save++;
		if (tmp->flags & MEMO_FWD)
			fwd++;
		if (tmp->flags & MEMO_DELETE)
			del++;
		if (tmp->flags & MEMO_REPLY)
			reply_ct++;
	}

	if (nummemos > 0) {
		if (nummemos == 1)
			PutReply(MemoServ, nick, RPL_MS_MEMOWAITING, 0, 0, 0);
		else
			PutReply(MemoServ, nick, RPL_MS_MEMOWAITING_1ARG, nummemos, 0, 0);
		if (unread != 0 || save != 0)
			PutReply(MemoServ, nick, RPL_MS_MEMO2_2ARG, unread, save, 0);
		if (fwd != 0 || reply_ct != 0)
			PutReply(MemoServ, nick, RPL_MS_MEMO3_2ARG, fwd, reply_ct, 0);
		if (del != 0)
			PutReply(MemoServ, nick, RPL_MS_MEMO4n_2ARG, del, MemoServ, 0);
	}
}

/**
 * \brief Get information about a memo block
 * \param box Pointer to a memo box item
 * \param text Nickname to search for a block against
 */
MemoBlock *getMemoBlockData(MemoBox * box, RegNickList *senderRnl)
{
	MemoBlock *tmp;

	if (box == NULL || senderRnl == NULL)
		return NULL;

	for (tmp = box->firstMblock; tmp; tmp = tmp->next) {
		if (tmp->blockId == senderRnl->regnum)
			return tmp;
	}
	return NULL;
}

/**
 * \mscmd Mblock
 * \syntax Mblock
 * \plr List memo blocks
 * \syntax Mblock +\<nick>
 * \plr Block memos from a registered nickname
 * \syntax Mblock -\<nick>
 * \plr Remove a memo block
 * \todo Make mblock lists use services NickIds
 */
/// Block further memos from a registered nickname
MCMD(ms_mblock)
{
	MemoBlock *list;
	const char *tmpNickName;


	if (!nick)
		return RET_FAIL;

	if (!nick->reg) {
		PutReply(MemoServ, nick, ERR_NEEDREGNICK_1ARG, nick->nick, 0, 0);
		return RET_FAIL;
	}

	if ((!strcasecmp(args[1], "LIST")) || (numargs < 2)) {
		if (!nick->reg->memos->firstMblock) {
			PutReply(MemoServ, nick, ERR_MS_MBLOCK_NONE, 0, 0, 0);
			return RET_OK;
		}

		PutReply(MemoServ, nick, RPL_MS_MBLOCK_HEAD, 0, 0, 0);
		for (list = nick->reg->memos->firstMblock; list; list = list->next)
		{
			tmpNickName = list->blockId.getNick();

			if (tmpNickName)
				sSend(":%s NOTICE %s :%s", MemoServ, nick->nick,
					  tmpNickName);
		}
		return RET_OK;
	}

	if (!strcasecmp(args[1], "ADD") && numargs != 3)
		return ms_AddMemoBlock(nick, args[2]);
	else if (*args[1] == '+')
		return ms_AddMemoBlock(nick, args[1] + 1);
	else if (!strcasecmp(args[1], "DEL") && numargs != 3)
		return ms_DelMemoBlock(nick, args[2]);
	else if (*args[1] == '-')
		return ms_DelMemoBlock(nick, args[1] + 1);
	else
#ifndef STRICT_MBLOCK_SYNTAX
		return ms_AddMemoBlock(nick, args[1]);
#else
		PutReply(MemoServ, nick, ERR_MS_MBLOCK_BADPARAM, MemoServ, 0, 0);
#endif

	return RET_OK;
}

/**
 * \brief Delete a memo block
 * \param nick Pointer to an online nick item
 * \param text Nickname to remove from a memo block list
 */
static cmd_return ms_DelMemoBlock(UserList * nick, char *text)
{
	char *from = nick->nick;
	MemoBlock *olditem = NULL;
	RegNickList *rnl, *rnlTarget;

	if (!nick || !text) {
		logDump(corelog, "ms_delmemoblock: %s%s is NULL",
				!nick ? " nick" : "", !text ? " text" : "");
		return RET_FAIL;
	}

	if (!(rnl = nick->reg)) {
		PutReply(MemoServ, nick, ERR_NICKNOTREG_1ARG, from, 0, 0);
		PutReply(MemoServ, nick, ERR_MS_MBLOCK_NOCHANGE, 0, 0, 0);

		return RET_FAIL;
	}

	if ((rnlTarget = getRegNickData(text)) != NULL)
		olditem = getMemoBlockData(rnl->memos, rnlTarget);

	if (rnlTarget == NULL || olditem == NULL) {
		PutReply(MemoServ, nick, ERR_MS_MBLOCK_NOSUCH_1ARG, text, 0, 0);

		return RET_FAIL;
	}
	delMemoBlock(rnl->memos, olditem);

	PutReply(MemoServ, nick, RPL_MS_MBLOCK_DELETED_1ARG, text, 0, 0);
	return RET_OK_DB;
}

/**
 * \brief Create a memo block
 * \param nick Pointer to an online nick item
 * \param text Nickname to add to a memo block list
 */
static cmd_return ms_AddMemoBlock(UserList * nick, char *text)
{
	char *from = nick->nick;
	MemoBlock *newitem;
	RegNickList *rnl, *targetRnl;
	int count_mblocks = 0;

	if (!nick || !text) {
		logDump(corelog, "ms_addmemoblock: %s%s is NULL",
				!nick ? " nick" : "", !text ? " text" : "");
		return RET_FAIL;
	}


	if (!(rnl = nick->reg) || !rnl->memos) {
		PutReply(MemoServ, nick, ERR_NEEDREGNICK_1ARG, from, 0, 0);
		PutReply(MemoServ, nick, ERR_MS_MBLOCK_NOCHANGE, 0, 0, 0);

		return RET_FAIL;
	}
	if ((targetRnl = getRegNickData(text)) == NULL) {
		PutReply(MemoServ, nick, ERR_NICKNOTREG_1ARG, text, 0, 0);
		PutReply(MemoServ, nick, ERR_MS_MBLOCK_NOCHANGE, 0, 0, 0);

		return RET_NOTARGET;
	}
	for (newitem = nick->reg->memos->firstMblock; newitem;
		 newitem = newitem->next) count_mblocks++;
	if (count_mblocks >= MS_MAX_MBLOCK) {
		PutReply(MemoServ, nick, ERR_MS_MBLOCK_TOOMANY, 0, 0, 0);

		return RET_FAIL;
	}
	if (getMemoBlockData(rnl->memos, targetRnl)) {
		PutReply(MemoServ, nick, ERR_MS_MBLOCK_ALREADY_1ARG, text, 0, 0);

		return RET_FAIL;
	}

	newitem = (MemoBlock *) oalloc(sizeof(MemoBlock));
	newitem->blockId = targetRnl->regnum;
	addMemoBlock(rnl->memos, newitem);

	PutReply(MemoServ, nick, RPL_MS_MBLOCK_ADDED_1ARG, text, 0, 0);
	return RET_OK_DB;
}

/**
 * \brief Destroy a memo block record
 * \param box Pointer to a memo box item
 * \param zap Pointer to a memo block item to be deleted
 * \warning After execution, 'zap' is no longer a valid pointer value
 */
void delMemoBlock(MemoBox * box, MemoBlock * zap)
{
	MemoBlock *mblock, *tmp, *mnext;

	if (!box || !zap || !box->firstMblock)
		return;
	for (tmp = NULL, mblock = box->firstMblock; mblock; mblock = mnext) {
		mnext = mblock->next;
		if (zap == mblock) {
			if (tmp)
				tmp->next = mnext;
			else
				box->firstMblock = mnext;
			FREE(zap);
			return;
		} else
			tmp = mblock;
	}
}

/**
 * \brief Add a new memo block record to a list
 * \param box Pointer to a memo box item
 * \param zap Pointer to a memo block item to be added to the list
 */
void addMemoBlock(MemoBox * box, MemoBlock * mblock)
{
	MemoBlock *tmp;
	if (!box || !mblock)
		return;
	for (tmp = box->firstMblock; tmp; tmp = tmp->next)
		if (tmp == mblock)
			return;
	mblock->next = box->firstMblock;
	box->firstMblock = mblock;
}

/// Should a memo expire or no?
int ShouldMemoExpire(MemoList *memo, int vacationPlus)
{
	time_t x;

	if (memo->flags & MEMO_DELETE)
		return 1;
	if (memo->flags & MEMO_SAVE)
		return 0;

	if ((memo->flags & MEMO_UNREAD) == 0)
		x = MEMODROPTIME;
	else if (vacationPlus == 0)
		x = NICKDROPTIME;
	else
		x = NICKDROPTIME * 3;

	if ((time(NULL) - memo->sent) >= x)
		return 1;
	return 0;
}

