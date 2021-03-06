/************************************************************************
 *   IRC - Internet Relay Chat, ircd/whowas.c
 *   Copyright (C) 1990 Markku Savela
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * --- avalon --- 6th April 1992
 * rewritten to scrap linked lists and use a table of structures which
 * is referenced like a circular loop. Should be faster and more efficient.
 */

#include "ircd.h"

#include "whowas.h"

IRCD_SCCSID("@(#)whowas.c	2.16 08 Nov 1993 (C) 1988 Markku Savela");
IRCD_RCSID("$Id$");

static	aName	was[NICKNAMEHISTORYLENGTH];
static	int	ww_index = 0;

void	add_history(aClient *cptr)
{
	aName	*np;
	Link	*lp, *chlist;

        np = &was[ww_index];
        if (np->ww_user)
		free_user(np->ww_user, np->ww_online);

	for (lp = np->ww_chans; lp; lp = lp->next)
	  {
		irc_free(lp->value.wwcptr);
		free_link(lp);
	  }

	bzero(np, sizeof(aName));
	strncpyzt(np->ww_nick, cptr->name, NICKLEN + 1);
	strncpyzt(np->ww_info, cptr->info, REALLEN + 1);
	np->ww_user = cptr->user;
	np->ww_logout = NOW;
	np->ww_online = (cptr->from != NULL) ? cptr : NULL;
	np->ww_user->refcnt++;

	np->ww_chans = NULL;
	for (lp = cptr->user->channel; lp; lp = lp->next)
	  {
		chlist = make_link();
		chlist->next = np->ww_chans;
		np->ww_chans = chlist;
		chlist->value.wwcptr = irc_malloc(sizeof(wwChan));
	       	strncpyzt(chlist->value.wwcptr->chname,
			  lp->value.chptr->chname, CHANNELLEN);
		chlist->value.wwcptr->pub = (unsigned char)PubChannel(lp->value.chptr);

		if (is_chan_op(cptr, lp->value.chptr))
			chlist->value.wwcptr->ucflags |= CHFL_CHANOP;

		else if (has_voice(cptr, lp->value.chptr))
			chlist->value.wwcptr->ucflags |= CHFL_VOICE;
	  }

	ww_index++;
	if (ww_index >= NICKNAMEHISTORYLENGTH)
		ww_index = 0;
}

/*
** get_history
**      Return the current client that was using the given
**      nickname within the timelimit. Returns NULL, if no
**      one found...
*/
aClient *get_history(char *nick, time_t timelimit)
{
        aName   *wp, *wp2;
        int     i = 0;

	if (ww_index == 0)
	  wp = wp2 = &was[NICKNAMEHISTORYLENGTH - 1];
	  else
	  wp = wp2 = &was[ww_index - 1];
        timelimit = NOW-timelimit;

	do {
		if (!mycmp(nick, wp->ww_nick) && wp->ww_logout >= timelimit)
			break;
		if (wp == was)
		{
		  i = 1;
		  wp = &was[NICKNAMEHISTORYLENGTH - 1];
		}
		else
		  wp--;
	} while (wp != wp2);

	if (wp != wp2 || !i)
		return (wp->ww_online);
	return (NULL);
}

void	off_history(aClient *cptr)
{
	aName	*wp;
	int	i;

	for (i = NICKNAMEHISTORYLENGTH, wp = was; i; wp++, i--)
		if (wp->ww_online == cptr)
			wp->ww_online = NULL;
}

void	initwhowas()
{
	bzero(&was, sizeof(aName) * NICKNAMEHISTORYLENGTH);
}


/*
** m_whowas
**	parv[0] = sender prefix
**	parv[1] = nickname queried
*/
int	m_whowas(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aName	*wp, *wp2 = NULL;
	int	j = 0;
	anUser	*up = NULL;
	char	*chname = NULL;
	aChannel *chptr = NULL;
	Link	*lp;
	int	max = -1, len = 0, mlen = 0;
	char	*p, *nick, *s, buf[BUFSIZE];

 	if (parc < 2)
	    {
		sendto_one(sptr, err_str(ERR_NONICKNAMEGIVEN),
			   me.name, parv[0]);
		return 0;
	    }
	if (parc > 2)
		max = atoi(parv[2]);
	if (parc > 3)
		if (hunt_server(cptr,sptr,":%s WHOWAS %s %s :%s", 3,parc,parv))
			return 0;

	for (s = parv[1]; (nick = strtok_r(s, ",", &p)); s = NULL)
	    {
		wp = wp2 = &was[ww_index - 1];

		do {
			if (wp < was)
				wp = &was[NICKNAMEHISTORYLENGTH - 1];
			if (mycmp(nick, wp->ww_nick) == 0)
			    {
				up = wp->ww_user;
				sendto_one(sptr, rpl_str(RPL_WHOWASUSER),
					   me.name, parv[0], wp->ww_nick,
					   up->username,
					   UGETHOST(sptr, up), wp->ww_info);

				mlen = strlen(me.name) + strlen(parv[0]) + 10 +
				  strlen(wp->ww_nick);

				for (*buf = '\0', len = 0, lp = wp->ww_chans; lp; lp = lp->next)
				  {
					chname = lp->value.wwcptr->chname;

					if (lp->value.wwcptr->pub ||
					    ((chptr = find_channel(lp->value.wwcptr->chname, NULL)) &&
					     IsMember(sptr, chptr)))
					  {
						if (len + strlen(lp->value.wwcptr->chname) > (size_t) BUFSIZE - mlen)
						  {
							sendto_one(sptr,
								   ":%s %d %s %s :%s",
								   me.name,
								   RPL_WHOISCHANNELS,
								   parv[0], wp->ww_nick, buf);
							*buf = '\0';
							len = 0;
						  }

						if (lp->value.wwcptr->ucflags & CHFL_CHANOP)
							buf[len++] = '@';

						else if (lp->value.wwcptr->ucflags & CHFL_VOICE)
							buf[len++] = '+';

						if (len)
							buf[len] = '\0';

						strcpy(&buf[len], lp->value.wwcptr->chname);
						len += strlen(lp->value.wwcptr->chname);
						strcpy(&buf[len], " ");
						len++;
					  }
				  }

				if (*buf != '\0')
					sendto_one(sptr,
						   rpl_str(RPL_WHOISCHANNELS),
						   me.name, parv[0],
						   wp->ww_nick,
						   buf);

				sendto_one(sptr, rpl_str(RPL_WHOISSERVER),
					   me.name, parv[0], wp->ww_nick,
					   up->server, myctime(wp->ww_logout));
				if (up->away)
					sendto_one(sptr, rpl_str(RPL_AWAY),
						   me.name, parv[0],
						   wp->ww_nick, up->away);
				if (up->mask && ((wp->ww_online && sptr == wp->ww_online) || IsAnOper(sptr)))
					sendto_one(sptr, rpl_str(RPL_WHOISMASKED),
						   me.name, parv[0], wp->ww_nick, up->mask);
				if (!BadPtr(up->sup_version)) {
					sendto_one(sptr, rpl_str(RPL_WHOISVERSION),
							me.name, parv[0],
							wp->ww_nick, 
							up->sup_version);
				}
				j++;
			    }
			if (max > 0 && j >= max)
				break;
			wp--;
		} while (wp != wp2);

		if (up == NULL)
			sendto_one(sptr, err_str(ERR_WASNOSUCHNICK),
				   me.name, parv[0], nick);
		up=NULL;

		if (p)
			p[-1] = ',';
	    }
	sendto_one(sptr, rpl_str(RPL_ENDOFWHOWAS), me.name, parv[0], parv[1]);
	return 0;
    }


void	count_whowas_memory(int *wwu, int *wwa, u_long *wwam)
{
	anUser	*tmp;
	int	i, j;
	int	u = 0, a = 0;
	u_long	am = 0;

	for (i = 0; i < NICKNAMEHISTORYLENGTH; i++)
		if ((tmp = was[i].ww_user))
			if (!was[i].ww_online)
			    {
				for (j = 0; j < i; j++)
					if (was[j].ww_user == tmp)
						break;
				if (j < i)
					continue;
				u++;
				if (tmp->away)
				    {
					a++;
					am += (strlen(tmp->away)+1);
				    }
			    }
	*wwu = u;
	*wwa = a;
	*wwam = am;
}
