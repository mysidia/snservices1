/**
 * \file gameserv.c
 * \brief GameServ implementation
 *
 * \qube
 * \date 1999
 *
 * $Id$
 */

/*
 * Copyright (c) 1999 James Mulcahy
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
#include "log.h"
#include "interp.h"

/**
 * @brief Dispatch table for GameServ commands.
 */
interp::service_cmd_t gameserv_commands[] = {
	/* string             function         Flags      L */
	{	"help", gs_help, 0, LOG_NO, 0, 1},
	{	"roll", gs_roll, 0, LOG_NO, 0, 1},
	{	NULL	},
};

/*------------------------------------------------------------------------*/
/* Message handler */

/**
 * \brief Parse a GameServ message
 * \param tmp Pointer to online user initiating the msesage
 * \param args Args of the message, where args[0] is the command and
 *        the extra parameters follow
 * \param numargs Highest index in the args[] array passed plus 1.
 *        so args[numargs - 1] is the highest index that can be safely
 *        accessed.
 */
void
sendToGameServ(UserList * nick, char **args, int numargs)
{
	char *from = nick->nick;
	interp::parser * cmd;

	if (!nick)
		return;

	cmd =
		new interp::parser(GameServ, getOpFlags(nick), gameserv_commands,
						   args[0]);
	if (!cmd)
		return;

	switch (cmd->run(nick, args, numargs)) {
	default:
		break;
	case RET_FAIL:
		sSend(":%s NOTICE %s :Unknown command %s.\r\n"
			  ":%s NOTICE %s :Please try /msg %s HELP", GameServ, from,
			  args[0], GameServ, from, GameServ);
		break;
	}

	delete cmd;
}

/*------------------------------------------------------------------------*/
/* Utility functions */

/*!
 * \fn int dice(int num, int sides)
 * \brief rolls some dice and returns the sum of the rolls
 *
 * \param num The number of dice to roll
 * \param sides The number of sides on the dice
 * \return Result of rolling dice D sides
 *
 * \sideffect Changes the random seed in use for generation of
 * numbers
 */
int
dice(int dice, int sides)
{
	int res = 0;

	/* Don't bother randomizing if it's always zero */
	if (dice < 1 || sides < 1)
		return 0;

	/* Roll the dice, sum up the results */
	while (dice-- > 0) {
		res += 1+(int)(((float)sides * rand()) / (RAND_MAX + 1.0));
		/* res += ((random() % sides) + 1); */
	}

	return res;
}


/*------------------------------------------------------------------------*/
/* Message functions*/

/**
 * @brief /msg GameServ help
 */
GCMD(gs_help)
{
	char *from = nick->nick;

	help(from, GameServ, args, numargs);
	return RET_OK;
}



/**
 * [Frequency] # [Number of Dice] D [Number of sides] +/- [Modifier]
 *                                  ^^^^^^^^^^^^^^^^^
 * Only the number of sides is mandatory.
 */

GCMD(gs_roll)
{
	char *from = nick->nick;
	char *to = nick->nick;
	char *string = args[1];
	int rolls = 1;
	int num_dice = 1;
	int num_sides = 1;
	int mod = 0;
	int multiplier = 1;			/* +1 or -1 */
	int counter = 0;
	int i, l = 0, *last_arg = &num_sides;
	int result = 0;
	char dicebuf[IRCBUF], rollbuf[IRCBUF];
	ChanList *chan;

	if (numargs < 2) {
		sSend(":%s NOTICE %s :Not enough parameters.\r\n"
			  ":%s NOTICE %s :/msg %s HELP %s for assistance.", GameServ,
			  nick->nick, GameServ, nick->nick, GameServ, args[0]);
		return RET_SYNTAX;
	}

	if (numargs == 3) {
		chan = getChanData(args[1]);
		if (!chan || !chan->reg) {
			sSend(":%s NOTICE %s :%s: No such registered channel is open.",
				  GameServ, from, args[1]);
			return RET_NOTARGET;
		}
		if (!getChanUserData(chan, nick)) {
			sSend(":%s NOTICE %s :%s: You are not on that channel.",
				  GameServ, from, args[1]);
			return RET_EFAULT;
		}
		if (getChanOp(chan->reg, nick->nick) < 4 && !isRoot(nick)) {
			sSend
				(":%s NOTICE %s :%s: You must be a channel operator to do this.",
				 GameServ, from, args[1]);
			return RET_EFAULT;
		}
		to = args[1];
		string = args[2];
	}

	/* c#bda+d
	 * *
	 * * c is optional.
	 * * b is optional and defaults to one.
	 * * a is any integer between 1 and 100 (and %)
	 * * d is optional, preceded by + or -.
	 * *
	 * * Negative numbers are allowed when modified by c.  
	 * *
	 * * c is the number of separate outputs to give. ie, 3#1d6 might return: 2, 6, 5
	 * * b is the number of dice to roll, but not output: 3d6 might return: 13
	 */
	for (i = 0, l = 0; string[i] && (i < 30); i++)
		switch (tolower(string[i])) {
		case '#':
			if (isdigit(*(string + i - l)))
				rolls = atoi(string + i - l);
			last_arg = &num_dice;
			l = 0;
			break;
		case 'd':
			if (isdigit(*(string + i - l)))
				num_dice = atoi(string + i - l);
			last_arg = &num_sides;
			l = 0;
			break;
		case '%':
			num_sides = 100;
			last_arg = NULL;
			l = 0;
			break;
		case '-':
		case '+':
			if (string[i] == '-')
				multiplier = -1;
			else
				multiplier = 1;
			if (isdigit(*(string + i - l)))
				num_sides = atoi(string + i - l);
			last_arg = &mod;
			l = 0;
			break;
		default:
			l++;
			break;
		}
	if (last_arg != NULL && l) {
		if (last_arg == &mod) {
			char *p = (string + i - l);

			if (*p == '-' || *p == '+')
				p++;
			if (isdigit(*p))
				*last_arg = multiplier * atoi(p);
		} else
			*last_arg = atoi(string + i - l);
	}

	if (*to == '#' && rolls > 10) {
		sSend(":%s NOTICE %s :Limited to 10 rolls for channels.", GameServ,
			  from);
		rolls = 10;
	}

	if (rolls > 100)
		rolls = 100;
	if (num_dice > 256)
		num_dice = 256;
	if (num_sides > 256)
		num_sides = 256;
	if (mod > 10000)
		mod = 10000;
	if (mod < -10000)
		mod = (-10000);

	/* Seed the rand() with the current time value in microsecs
	 * if we can't, then use secs */

	dicebuf[0] = rollbuf[0] = '\0';

	for (i = 0, counter = 0; i < rolls; i++) {
		result = (dice(num_dice, num_sides) + mod);
		if (*dicebuf)
			strcpy(rollbuf, ", ");
		else
			rollbuf[0] = '\0';
		sprintf(rollbuf + strlen(rollbuf), "%2d", result);
		strcat(dicebuf, rollbuf);
		if (counter++ >= 5 || strlen(dicebuf) >= (sizeof(IRCBUF) - 15)) {
			if (nick->floodlevel.GetLev() < (.75 * MAXFLOODLEVEL))
				if (addFlood(nick, *to == '#' ? 5 : 1))
					return RET_KILLED;
			counter = 0;
			if (*to == '#')
				sSend(":%s PRIVMSG %s :Result (%s) (%d#%dd%d%c%d): %s",
					  GameServ, to, from, rolls, num_dice, num_sides,
					  mod >= 0 ? '+' : '-', abs(mod), dicebuf);
			else
				sSend(":%s NOTICE %s :Result (%d#%dd%d%c%d): %s", GameServ,
					  to, rolls, num_dice, num_sides, mod >= 0 ? '+' : '-',
					  abs(mod), dicebuf);
			dicebuf[0] = '\0';
		}
	}
	if (counter) {
		if (*to == '#')
			sSend(":%s PRIVMSG %s :Result (%s) (%d#%dd%d%c%d): %s",
				  GameServ, to, from, rolls, num_dice, num_sides,
				  mod >= 0 ? '+' : '-', abs(mod), dicebuf);
		else
			sSend(":%s NOTICE %s :Result (%d#%dd%d%c%d): %s", GameServ, to,
				  rolls, num_dice, num_sides, mod >= 0 ? '+' : '-',
				  abs(mod), dicebuf);
	}
	return RET_OK;
}
