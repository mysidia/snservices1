/*
 *   IRC - Internet Relay Chat, ircd/channel.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Co Center
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

/* -- Jto -- 09 Jul 1990
 * Bug fix
 */

/* -- Jto -- 03 Jun 1990
 * Moved m_channel() and related functions from s_msg.c to here
 * Many changes to start changing into string channels...
 */

/* -- Jto -- 24 May 1990
 * Moved is_full() from list.c
 */

#ifndef	lint
static	char sccsid[] = "@(#)channel.c	2.58 2/18/94 (C) 1990 University of Oulu, Computing\
 Center and Jarkko Oikarinen";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "channel.h"
#include "h.h"

aChannel *channel = NullChn;

static	void	add_invite PROTO((aClient *, aChannel *));
static	int	add_banid PROTO((aClient *, aChannel *, char *));
static	int	can_join PROTO((aClient *, aChannel *, char *));
static	void	channel_modes PROTO((aClient *, char *, char *, aChannel *));
static	int	check_channelmask PROTO((aClient *, aClient *, char *));
static	int	del_banid PROTO((aChannel *, char *));
static	int	find_banid PROTO((aChannel *, char *));
/*static	Link	*is_banned PROTO((aClient *, aChannel *));*/
static  int     have_ops PROTO((aChannel *));
static	int	number_of_zombies PROTO((aChannel *));
static  int     is_deopped PROTO((aClient *, aChannel *));
static	int	set_mode PROTO((aClient *, aClient *, aChannel *, int,\
			        char **, char *,char *, int *, int));
static	void	sub1_from_channel PROTO((aChannel *));

void	clean_channelname PROTO((char *));
void	del_invite PROTO((aClient *, aChannel *));

static	char	*PartFmt = ":%s PART %s";
static	char	*PartFmt2 = ":%s PART %s :%s";
/*
 * some buffers for rebuilding channel/nick lists with ,'s
 */
static	char	nickbuf[BUFSIZE], buf[BUFSIZE];
static	char	modebuf[MODEBUFLEN], parabuf[MODEBUFLEN];

/*
 * return the length (>=0) of a chain of links.
 */
static	int	list_length(lp)
Link	*lp;
{
	int	count = 0;

	for (; lp; lp = lp->next)
		count++;
	return count;
}

/*
** find_chasing
**	Find the client structure for a nick name (user) using history
**	mechanism if necessary. If the client is not found, an error
**	message (NO SUCH NICK) is generated. If the client was found
**	through the history, chasing will be 1 and otherwise 0.
*/
static	aClient *find_chasing(sptr, user, chasing)
aClient *sptr;
char	*user;
int	*chasing;
{
	aClient *who = find_client(user, (aClient *)NULL);

	if (chasing)
		*chasing = 0;
	if (who)
		return who;
	if (!(who = get_history(user, (long)KILLCHASETIMELIMIT)))
	    {
		sendto_one(sptr, err_str(ERR_NOSUCHNICK),
			   me.name, sptr->name, user);
		return NULL;
	    }
	if (chasing)
		*chasing = 1;
	return who;
}

/*
 *  Fixes a string so that the first white space found becomes an end of
 * string marker (`\-`).  returns the 'fixed' string or "*" if the string
 * was NULL length or a NULL pointer.
 */
static	char	*check_string(s)
char *s;
{
	static	char	star[2] = "*";
	char	*str = s;

	if (BadPtr(s))
		return star;

	for ( ;*s; s++)
		if (isspace(*s))
		    {
			*s = '\0';
			break;
		    }

	return (BadPtr(str)) ? star : str;
}

/*
 * create a string of form "foo!bar@fubar" given foo, bar and fubar
 * as the parameters.  If NULL, they become "*".
 */
static int IsIpMask(const char *mask)
{
  if (!mask)
    return 0;
  while (*mask)
    {
      if (isalpha(*mask))
	return 0;
      ++mask;
    }
  return 1;
}

static	char *make_nick_user_ip(cptr)
aClient *cptr;
{
  static char	namebuf[NICKLEN+USERLEN+HOSTLEN+7];
  char	 *s = namebuf;
  char *nick = cptr->name;
  char *name = cptr->user->username;
  char *host;

  if (!MyClient(cptr))
    return NULL;

  host = inetntoa((char *)&cptr->ip);
  bzero(namebuf, sizeof(namebuf));
  nick = check_string(nick);
  strncpyzt(namebuf, nick, NICKLEN + 1);
  s += strlen(s);
  *s++ = '!';
  name = check_string(name);
  /*
   * User +2 instead of +1 here because if you ban *1234567890, even
   * though 1234567890 is valid, it cuts off the '0' because ircd
   * thinks it is too many; Let users set bans this way now -Cabal95
   */
  strncpyzt(s, name, USERLEN + 2);
  s += strlen(s);
  *s++ = '@';
  host = check_string(host);
  strncpyzt(s, host, HOSTLEN + 1);
  s += strlen(s);
  *s = '\0';
  return (namebuf);
}

static	char *make_nick_user_host(nick, name, host)
char	*nick, *name, *host;
{
	static	char	namebuf[NICKLEN+USERLEN+HOSTLEN+7];
	char	*s = namebuf;

	bzero(namebuf, sizeof(namebuf));
	nick = check_string(nick);
	strncpyzt(namebuf, nick, NICKLEN + 1);
	s += strlen(s);
	*s++ = '!';
	name = check_string(name);
	/*
	 * User +2 instead of +1 here because if you ban *1234567890, even
	 * though 1234567890 is valid, it cuts off the '0' because ircd
	 * thinks it is too many; Let users set bans this way now -Cabal95
	 */
	strncpyzt(s, name, USERLEN + 2);
	s += strlen(s);
	*s++ = '@';
	host = check_string(host);
	strncpyzt(s, host, HOSTLEN + 1);
	s += strlen(s);
	*s = '\0';
	return (namebuf);
}

/*
 * Ban functions to work with mode +b
 */
/* add_banid - add an id to be banned to the channel  (belongs to cptr) */

static	int	add_banid(cptr, chptr, banid)
aClient	*cptr;
aChannel *chptr;
char	*banid;
{
	Link	*ban;
	int	cnt = 0, len = 0;

	if (MyClient(cptr))
		(void)collapse(banid);
	for (ban = chptr->banlist; ban; ban = ban->next)
	    {
		len += strlen(ban->value.ban.banstr);
		if (MyClient(cptr))
			if ((len > MAXBANLENGTH) || (++cnt >= MAXBANS))
			    {
				sendto_one(cptr, err_str(ERR_BANLISTFULL),
					   me.name, cptr->name,
					   chptr->chname, banid);
				return -1;
			    }

			else
			    {
				if (!match(ban->value.ban.banstr, banid)
				    /*||!match(banid, ban->value.ban.banstr)*/)
					return -1;
			    }
		else if (!mycmp(ban->value.ban.banstr, banid))
			return -1;
		
	    }
	ban = make_link();
	bzero((char *)ban, sizeof(Link));
	ban->flags = CHFL_BAN;
	ban->next = chptr->banlist;
	ban->value.ban.banstr = (char *)MyMalloc(strlen(banid)+1);
	(void)strcpy(ban->value.ban.banstr, banid);
	ban->value.ban.who = (char *)MyMalloc(strlen(cptr->name)+1);
	(void)strcpy(ban->value.ban.who, cptr->name);
	ban->value.ban.when = NOW;
	chptr->banlist = ban;
	return 0;
}
/*
 * del_banid - delete an id belonging to cptr
 */
static	int	del_banid(chptr, banid)
aChannel *chptr;
char	*banid;
{
	Link **ban;
        Link *tmp;

	if (!banid)
		return -1;
 	for (ban = &(chptr->banlist); *ban; ban = &((*ban)->next))
		if (mycmp(banid, (*ban)->value.ban.banstr)==0)
		    {
			tmp = *ban;
			*ban = tmp->next;
			MyFree(tmp->value.ban.banstr);
			MyFree(tmp->value.ban.who);
			free_link(tmp);
			return 0;
		    }
	return -1;
}

/*
 * find_banid - Find an exact match for a ban
 */
static	int	find_banid(chptr, banid)
aChannel *chptr;
char	*banid;
{
	Link **ban;
	Link *tmp;

	if (!banid)
		return -1;
	for (ban = &(chptr->banlist); *ban; ban = &((*ban)->next))
		if (!mycmp(banid, (*ban)->value.ban.banstr)) return 1;
	return 0;
}

/*
 * IsMember - returns 1 if a person is joined and not a zombie
 */
int	IsMember(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
        Link *lp;

	return (((lp=find_user_link(chptr->members, cptr)) &&
			!(lp->flags & CHFL_ZOMBIE))?1:0);
}

/*
 * is_banned - returns a pointer to the ban structure if banned else NULL
 */
extern	Link	*is_banned(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Link	*tmp;
	char	*s, *s_ip;
        
	if (!IsPerson(cptr))
		return NULL;

	s = make_nick_user_host(cptr->name, cptr->user->username,
				  cptr->user->host);

	for (tmp = chptr->banlist; tmp; tmp = tmp->next)
		if (match(tmp->value.ban.banstr, s) == 0 ||
                   (MyClient(cptr) && IsIpMask(tmp->value.ban.banstr) &&
                   (s_ip = make_nick_user_ip(cptr)) && !match(tmp->value.ban.banstr, s_ip ) ))
			break;
	return (tmp);
}

/*
 * adds a user to a channel by adding another link to the channels member
 * chain.
 */
static	void	add_user_to_channel(chptr, who, flags)
aChannel *chptr;
aClient *who;
int	flags;
{
	Link *ptr;

	if (who->user)
	    {
		ptr = make_link();
		ptr->value.cptr = who;
		ptr->flags = flags;
		ptr->next = chptr->members;
		chptr->members = ptr;
		chptr->users++;

		ptr = make_link();
		ptr->value.chptr = chptr;
		ptr->next = who->user->channel;
		who->user->channel = ptr;
		who->user->joined++;
	    }
}

void	remove_user_from_channel(sptr, chptr)
aClient *sptr;
aChannel *chptr;
{
	Link	**curr;
	Link	*tmp, *lp = chptr->members;

	for (; lp && (lp->flags & CHFL_ZOMBIE || lp->value.cptr==sptr);
	    lp=lp->next);
	for (;;)
	{
	  for (curr = &chptr->members; (tmp = *curr); curr = &tmp->next)
		  if (tmp->value.cptr == sptr)
		      {
			  *curr = tmp->next;
			  free_link(tmp);
			  break;
		      }
	  for (curr = &sptr->user->channel; (tmp = *curr); curr = &tmp->next)
		  if (tmp->value.chptr == chptr)
		      {
			  *curr = tmp->next;
			  free_link(tmp);
			  break;
		      }
	  sptr->user->joined--;
	  if (lp) break;
	  if (chptr->members) sptr = chptr->members->value.cptr;
	  else break;
	  sub1_from_channel(chptr);
	}
	sub1_from_channel(chptr);
}


static	int	have_ops(chptr)
aChannel *chptr;
{
	Link	*lp;

	if (chptr)
        {
	  lp=chptr->members;
	  while (lp)
	  {
	    if (lp->flags & CHFL_CHANOP) return(1);
	    lp = lp->next;
	  }
        }
	return 0;
}

int	is_chan_op(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Link	*lp;

	if (chptr)
		if (lp = find_user_link(chptr->members, cptr))
			return (lp->flags & CHFL_CHANOP);

	return 0;
}

/* This was the original function that allowed mode hacking - not
   anymore. -- Barubary */
static	int	is_deopped(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	if (!IsPerson(cptr)) return 0;
	return !is_chan_op(cptr, chptr);
}

int	is_zombie(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Link	*lp;

	if (chptr)
		if ((lp = find_user_link(chptr->members, cptr)))
			return (lp->flags & CHFL_ZOMBIE);

	return 0;
}

int	has_voice(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Link	*lp;

	if (chptr)
		if ((lp = find_user_link(chptr->members, cptr)) &&
		    !(lp->flags & CHFL_ZOMBIE))
			return (lp->flags & CHFL_VOICE);

	return 0;
}

int	can_send(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Link	*lp;
	int	member;

	member = IsMember(cptr, chptr);
	lp = find_user_link(chptr->members, cptr);

	if (chptr->mode.mode & MODE_MODERATED &&
	    (!lp || !(lp->flags & (CHFL_CHANOP|CHFL_VOICE)) ||
	    (lp->flags & CHFL_ZOMBIE)))
			return (MODE_MODERATED);

	if (chptr->mode.mode & MODE_NOPRIVMSGS && !member)
		return (MODE_NOPRIVMSGS);

if ((!lp || !(lp->flags & (CHFL_CHANOP|CHFL_VOICE)) ||
    (lp->flags & CHFL_ZOMBIE)) && MyClient(cptr) &&
    is_banned(cptr, chptr))
	return (MODE_BAN);

	return 0;
}

aChannel *find_channel(chname, chptr)
char	*chname;
aChannel *chptr;
{
	return hash_find_channel(chname, chptr);
}

/*
 * write the "simple" list of channel modes for channel chptr onto buffer mbuf
 * with the parameters in pbuf.
 */
static	void	channel_modes(cptr, mbuf, pbuf, chptr)
aClient	*cptr;
char	*mbuf, *pbuf;
aChannel *chptr;
{
	*mbuf++ = '+';
	if (chptr->mode.mode & MODE_SECRET)
		*mbuf++ = 's';
	else if (chptr->mode.mode & MODE_PRIVATE)
		*mbuf++ = 'p';
	if (chptr->mode.mode & MODE_MODERATED)
		*mbuf++ = 'm';
	if (chptr->mode.mode & MODE_TOPICLIMIT)
		*mbuf++ = 't';
	if (chptr->mode.mode & MODE_INVITEONLY)
		*mbuf++ = 'i';
	if (chptr->mode.mode & MODE_NOPRIVMSGS)
		*mbuf++ = 'n';
	if (chptr->mode.limit)
	    {
		*mbuf++ = 'l';
		if (IsMember(cptr, chptr) || IsServer(cptr) || IsULine(cptr,cptr))
			(void)sprintf(pbuf, "%d ", chptr->mode.limit);
	    }
	if (*chptr->mode.key)
	    {
		*mbuf++ = 'k';
		if (IsMember(cptr, chptr) || IsServer(cptr) || IsULine(cptr,cptr))
			(void)strcat(pbuf, chptr->mode.key);
	    }
	*mbuf++ = '\0';
	return;
}

/*
 * m_opmode
 * force a channel to change modes with Operator privileges
 * parv[0] = sender
 * parv[1] = channel
 * parv[2] = modes
 */
int     m_opmode(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int     parc;
char	*parv[];
{
    static char tmp[MODEBUFLEN];
    int badop, sendts;
    aChannel *chptr;

     if (check_registered(cptr) || !IsPerson(sptr))
        return 0;
      if (!IsPrivileged(cptr) || !IsAnOper(sptr))
      {
         sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
         return 0;
      }
      if ( parc < 2 )
      {
           sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
                      me.name, parv[0], "MODE");
           return 0;
       }
      if (MyClient(sptr) && ( !IsOper(sptr) || !OPCanModeHack(sptr)))
      {
         sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
	 return 0;
      }
      chptr = find_channel(parv[1], NullChn);
      if (chptr == NullChn)
      {
           sendto_one(sptr, err_str(ERR_NOSUCHNICK),
                      me.name, parv[0], parv[1]);
           return 0;
      }
      sptr->flags&=~FLAGS_TS8;
      clean_channelname(parv[1]);
      if (check_channelmask(sptr, cptr, parv[1]))
           return 0;
      sendts = set_mode(cptr, sptr, chptr, parc - 2, parv + 2,
               modebuf, parabuf, &badop, 1);
      sendto_realops("%s[%s]!%s@%s Forced Mode Change of channel %s: \2%s %s\2",
                  sptr->name, MyClient(sptr) ? me.name :
                  sptr->user->server ? sptr->user->server : "",
                  sptr->user->username, sptr->user->host, 
                  chptr->chname, modebuf, parabuf);
      tolog(LOG_OPER, "%s[%s]!%s@%s Forced Mode Change of channel %s: %s %s",
                  sptr->name, MyClient(sptr) ? me.name :
                  sptr->user->server ? sptr->user->server : "",
                  sptr->user->username, sptr->user->host, 
                  chptr->chname, modebuf, parabuf);
      sendto_match_servs(chptr, cptr, ":%s OPMODE %s %s %s",
                  parv[0], chptr->chname, modebuf, parabuf);
      sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s",
                  parv[0], chptr->chname, modebuf, parabuf);
      return 0;
}


static	int send_mode_list(cptr, chname, creationtime, top, mask, flag)
aClient	*cptr;
Link	*top;
int	mask;
char	flag, *chname;
time_t	creationtime;
{
	Link	*lp;
	char	*cp, *name;
	int	count = 0, send = 0, sent = 0;

	cp = modebuf + strlen(modebuf);
	if (*parabuf)	/* mode +l or +k xx */
		count = 1;
	for (lp = top; lp; lp = lp->next)
	    {
		if (!(lp->flags & mask))
			continue;
		if (mask == CHFL_BAN)
			name = lp->value.ban.banstr;
		else
			name = lp->value.cptr->name;
		if (strlen(parabuf) + strlen(name) + 11 < (size_t) MODEBUFLEN)
		    {
			(void)strcat(parabuf, " ");
			(void)strcat(parabuf, name);
			count++;
			*cp++ = flag;
			*cp = '\0';
		    }
		else if (*parabuf)
			send = 1;
		if (count == 6)
			send = 1;
		if (send)
		    {
		       /* cptr is always a server! So we send creationtimes */
			sendmodeto_one(cptr, me.name, chname, modebuf,
				   parabuf, creationtime);
                        sent = 1;
			send = 0;
			*parabuf = '\0';
			cp = modebuf;
			*cp++ = '+';
			if (count != 6)
			    {
				(void)strcpy(parabuf, name);
				*cp++ = flag;
			    }
			count = 0;
			*cp = '\0';
		    }
	    }
     return sent;
}

/*
 * send "cptr" a full list of the modes for channel chptr.
 */
void	send_channel_modes(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
        int sent;
	if (*chptr->chname != '#')
		return;

	*modebuf = *parabuf = '\0';
	channel_modes(cptr, modebuf, parabuf, chptr);

	sent=send_mode_list(cptr, chptr->chname, chptr->creationtime,
	    chptr->members, CHFL_CHANOP, 'o');
	if (!sent && chptr->creationtime)
	  sendto_one(cptr, ":%s MODE %s %s %s %lu", me.name,
	      chptr->chname, modebuf, parabuf, chptr->creationtime);
	else if (modebuf[1] || *parabuf)
	  sendmodeto_one(cptr, me.name,
	      chptr->chname, modebuf, parabuf, chptr->creationtime);

	*parabuf = '\0';
	*modebuf = '+';
	modebuf[1] = '\0';
	(void)send_mode_list(cptr, chptr->chname,chptr->creationtime,
	    chptr->banlist, CHFL_BAN, 'b');
	if (modebuf[1] || *parabuf)
	  sendmodeto_one(cptr, me.name, chptr->chname, modebuf,
	      parabuf, chptr->creationtime);

	*parabuf = '\0';
	*modebuf = '+';
	modebuf[1] = '\0';
	(void)send_mode_list(cptr, chptr->chname,chptr->creationtime,
	    chptr->members, CHFL_VOICE, 'v');
	if (modebuf[1] || *parabuf)
	  sendmodeto_one(cptr, me.name, chptr->chname, modebuf,
	      parabuf, chptr->creationtime);
}

/*
 * m_mode
 * parv[0] - sender
 * parv[1] - channel
 */

int	m_mode(cptr, sptr, parc, parv)
aClient *cptr;
aClient *sptr;
int	parc;
char	*parv[];
{
	static char tmp[MODEBUFLEN];
	int badop, sendts;
	aChannel *chptr;

	if (check_registered(sptr))
		return 0;

	/* Now, try to find the channel in question */
	if (parc > 1)
	    {
		chptr = find_channel(parv[1], NullChn);
		if (chptr == NullChn)
			return m_umode(cptr, sptr, parc, parv);
	    }
	else
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "MODE");
 	 	return 0;
	    }

	ClientFlags(sptr) &=~FLAGS_TS8;

	clean_channelname(parv[1]);
	if (check_channelmask(sptr, cptr, parv[1]))
		return 0;

	if (parc < 3)
	    {
		*modebuf = *parabuf = '\0';
		modebuf[1] = '\0';
		channel_modes(sptr, modebuf, parabuf, chptr);
		sendto_one(sptr, rpl_str(RPL_CHANNELMODEIS), me.name, parv[0],
			   chptr->chname, modebuf, parabuf);
		sendto_one(sptr, rpl_str(RPL_CREATIONTIME), me.name, parv[0],
				 chptr->chname, chptr->creationtime);
		return 0;
	    }

	if (!(sendts = set_mode(cptr, sptr, chptr, parc - 2, parv + 2,
	    modebuf, parabuf, &badop, 0)) && !IsULine(cptr,sptr))
	    {
		sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
			   me.name, parv[0], chptr->chname);
		return 0;
	    }

	if ((badop>=2) && !IsULine(cptr,sptr))
	{
	  int i=3;
	  *tmp='\0';
	  while (i < parc)
	  { strcat(tmp, " ");
	    strcat(tmp, parv[i++]); }
/* I hope I've got the fix *right* this time.  --Russell
	  sendto_ops("%sHACK(%d): %s MODE %s %s%s [%lu]",
	      (badop==3)?"BOUNCE or ":"", badop,
	      parv[0],parv[1],parv[2],tmp,chptr->creationtime);*/
	}

	if (strlen(modebuf) > (size_t)1 || sendts > 0)
	{   if (badop!=2 && strlen(modebuf) > (size_t)1)
	      sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s",
	          parv[0], chptr->chname, modebuf, parabuf);
            /* We send a creationtime of 0, to mark it as a hack --Run */
	    if ((IsServer(sptr) && (badop==2 || sendts > 0)) ||
		 IsULine(cptr,sptr))
	    { if (*modebuf == '\0') strcpy(modebuf,"+");
	      if (badop==2)
		badop = 2;
/*	        sendto_serv_butone(cptr, ":%s WALLOPS :HACK: %s MODE %s %s%s",
	            me.name,parv[0],parv[1],parv[2],tmp);*/
	      else
	        sendto_match_servs(chptr, cptr, ":%s MODE %s %s %s %lu",
		    parv[0], chptr->chname, modebuf, parabuf,
		    (badop==4)?(time_t)0:chptr->creationtime); }
            else
	      sendto_match_servs(chptr, cptr, ":%s MODE %s %s %s",
	          parv[0], chptr->chname, modebuf, parabuf);
	}
	return 0;
}

int DoesOp(modebuf)
char *modebuf;
{
  modebuf--; /* Is it possible that a mode starts with o and not +o ? */
  while (*++modebuf) if (*modebuf=='o' || *modebuf=='v') return(1);
  return 0;
}

int sendmodeto_one(cptr, from, name, mode, param, creationtime)
aClient *cptr;
char *from,*name,*mode,*param;
time_t creationtime;
{
	if ((IsServer(cptr) && DoesOp(mode) && creationtime) ||
 		IsULine(cptr,cptr))
	  sendto_one(cptr,":%s MODE %s %s %s %lu",
	      from, name, mode, param, creationtime);
	else
	  sendto_one(cptr,":%s MODE %s %s %s",
	      from, name, mode, param);
}

char *pretty_mask(mask)
char *mask;
{
  char *cp;
  char *user;
  char *host;

  if ((user = index((cp = mask), '!')))
    *user++ = '\0';
  if ((host = rindex(user ? user : cp, '@')))
    {
      *host++ = '\0';
      if (!user)
	return make_nick_user_host(NULL, cp, host);
    }
  else
    if (!user && index(cp, '.'))
      return make_nick_user_host(NULL, NULL, cp);

  return make_nick_user_host(cp, user, host);
}

/*
 * Check and try to apply the channel modes passed in the parv array for
 * the client ccptr to channel chptr.  The resultant changes are printed
 * into mbuf and pbuf (if any) and applied to the channel.
 */
static	int	set_mode(cptr, sptr, chptr, parc, parv, mbuf, pbuf, badop, modehack)
aClient *cptr, *sptr;
aChannel *chptr;
int	parc, *badop;
char	*parv[], *mbuf, *pbuf;
int	modehack;
{
	static	Link	chops[MAXMODEPARAMS];
	static	int	flags[] = {
				MODE_PRIVATE,    'p', MODE_SECRET,     's',
				MODE_MODERATED,  'm', MODE_NOPRIVMSGS, 'n',
				MODE_TOPICLIMIT, 't', MODE_INVITEONLY, 'i',
				MODE_VOICE,      'v', MODE_KEY,        'k',
				0x0, 0x0 };

	Link	*lp;
	char	*curr = parv[0], *cp;
	int	*ip;
	Link    *member, *tmp = NULL;
	u_int	whatt = MODE_ADD, bwhatt = 0;
	int	limitset = 0, chasing = 0, bounce;
	int	nusers, new, len, blen, keychange = 0, opcnt = 0, banlsent = 0;
	int     doesdeop = 0, doesop = 0, hacknotice = 0, change, gotts = 0;
	char	fm = '\0';
	aClient *who;
	Mode	*mode, oldm;
	char	chase_mode[3];
	static  char bmodebuf[MODEBUFLEN], bparambuf[MODEBUFLEN], numeric[16];
        char    *bmbuf = bmodebuf, *bpbuf = bparambuf, *mbufp = mbuf;
	time_t	newtime = (time_t)0;
	aConfItem *aconf;

	*mbuf=*pbuf=*bmbuf=*bpbuf='\0';
	*badop=0;
	if (parc < 1)
		return 0;

	mode = &(chptr->mode);
	bcopy((char *)mode, (char *)&oldm, sizeof(Mode));
        /* Mode is accepted when sptr is a channel operator
	 * but also when the mode is received from a server. --Run
	 */

/*
 * This was modified to allow non-chanops to see bans..perhaps not as clean
 * as other versions, but it works. Basically, if a person who's not a chanop
 * calls set_mode() with a /mode <some parameter here> command, this function
 * checks to see if that parameter is +b, -b, or just b. If it is one of
 * these, and there are NO other parameters (i.e. no multiple mode changes),
 * then it'll display the banlist. Else, it just returns "You're not chanop."
 * Enjoy. --dalvenjah, dalvenja@rahul.net
 */

	if (!(IsServer(cptr) || IsULine(cptr,sptr) || is_chan_op(sptr, chptr) || modehack))
		{
			if ( ((*curr=='b' && (strlen(parv[0])==1 && parc==1))
			      || ((*curr=='+') && (*(curr+1)=='b') && (strlen(parv[0])==2 && parc==1))
			      || ((*curr=='-') && (*(curr+1)=='b') && (strlen(parv[0])==2 && parc==1)))
			    && (!is_chan_op(sptr, chptr)))
			    {
				    if (--parv == 0) /* XXX MLG Was <= */
					{
					for (lp=chptr->banlist;lp;lp=lp->next)
						sendto_one(cptr,
							   rpl_str(RPL_BANLIST),
							   me.name, cptr->name,
							   chptr->chname,
							   lp->value.ban.banstr,
							   lp->value.ban.who,
							   lp->value.ban.when);
						sendto_one(cptr,
							   rpl_str(RPL_ENDOFBANLIST),
							   me.name, cptr->name,
							   chptr->chname);
					}
			    }
			else
			{
				return 0;
			}
		}

	new = mode->mode;

	while (curr && *curr)
	    {
		switch (*curr)
		{
		case '+':
			whatt = MODE_ADD;
			break;
		case '-':
			whatt = MODE_DEL;
			break;
		case 'o' :
		case 'v' :
			if (--parc <= 0)
				break;
			parv++;
			*parv = check_string(*parv);
			if (MyClient(sptr) && opcnt >= MAXMODEPARAMS)
				break;
			/*
			 * Check for nickname changes and try to follow these
			 * to make sure the right client is affected by the
			 * mode change.
			 */
			if (!(who = find_chasing(sptr, parv[0], &chasing)))
				break;
	  		if (!(member = find_user_link(chptr->members,who)))
			    {
	    			sendto_one(cptr, err_str(ERR_USERNOTINCHANNEL),
					   me.name, cptr->name,
					   parv[0], chptr->chname);
				break;
			    }
			if (whatt == MODE_ADD)
			    {
				lp = &chops[opcnt++];
				lp->value.cptr = who;
				if ((IsServer(sptr) &&
				    (!(ClientFlags(who) & FLAGS_TS8) ||
				    ((*curr == 'o') && !(member->flags &
				    (CHFL_SERVOPOK|CHFL_CHANOP))) ||
				    who->from != sptr->from)) ||
				    IsULine(cptr,sptr) || modehack)
				  *badop=((member->flags & CHFL_DEOPPED) &&
				      (*curr == 'o'))?2:3;
				lp->flags = (*curr == 'o') ? MODE_CHANOP:
					                     MODE_VOICE;
				lp->flags |= MODE_ADD;
			    }
			else if (whatt == MODE_DEL)
			    {
				lp = &chops[opcnt++];
				lp->value.cptr = who;
				doesdeop = 1; /* Also when -v */
				lp->flags = (*curr == 'o') ? MODE_CHANOP:
							     MODE_VOICE;
				lp->flags |= MODE_DEL;
			    }
			if (*curr == 'o')
			  doesop=1;
			break;
		case 'k':
			if (--parc <= 0)
				break;
			parv++;
			/* check now so we eat the parameter if present */
			if (keychange)
				break;
			*parv = check_string(*parv);
			{
				u_char	*s1,*s2;

				for (s1 = s2 = (u_char *)*parv; *s2; s2++)
				  if ((*s1 = *s2 & 0x7f) > (u_char)32 &&
				      *s1 != ':') s1++;
				*s1 = '\0';
			}
			if (MyClient(sptr) && opcnt >= MAXMODEPARAMS)
				break;
			if (whatt == MODE_ADD)
			    {
				if (*mode->key && !IsServer(cptr) &&
					!IsULine(cptr,sptr) && !modehack)
					sendto_one(cptr, err_str(ERR_KEYSET),
						   me.name, cptr->name,
						   chptr->chname);
				else if (!*mode->key || IsServer(cptr) ||
					IsULine(cptr,sptr) || modehack)
				    {
					lp = &chops[opcnt++];
					lp->value.cp = *parv;
					if (strlen(lp->value.cp) >
					    (size_t) KEYLEN)
						lp->value.cp[KEYLEN] = '\0';
					lp->flags = MODE_KEY|MODE_ADD;
					keychange = 1;
				    }
			    }
			else if (whatt == MODE_DEL)
			    {
				if (mycmp(mode->key, *parv) == 0 ||
				    IsServer(cptr) || IsULine(cptr,sptr) || modehack)
				    {
					lp = &chops[opcnt++];
					lp->value.cp = mode->key;
					lp->flags = MODE_KEY|MODE_DEL;
					keychange = 1;
				    }
			    }
			break;
		case 'b':
			if (--parc <= 0)
			    {
                                if (banlsent) /* Only send it once */
                                  break;
				for (lp = chptr->banlist; lp; lp = lp->next)
					sendto_one(cptr, rpl_str(RPL_BANLIST),
					     me.name, cptr->name,
						   chptr->chname,
						   lp->value.ban.banstr,
						   lp->value.ban.who,
						   lp->value.ban.when);
				sendto_one(cptr, rpl_str(RPL_ENDOFBANLIST),
					   me.name, cptr->name, chptr->chname);
                                banlsent = 1;
				break;
			    }
			parv++;
			if (BadPtr(*parv))
				break;
			if (MyClient(sptr) && opcnt >= MAXMODEPARAMS)
				break;
			if (whatt == MODE_ADD)
			    {
				lp = &chops[opcnt++];
				lp->value.cp = *parv;
				lp->flags = MODE_ADD|MODE_BAN;
			    }
			else if (whatt == MODE_DEL)
			    {
				lp = &chops[opcnt++];
				lp->value.cp = *parv;
				lp->flags = MODE_DEL|MODE_BAN;
			    }
			break;
		case 'l':
			/*
			 * limit 'l' to only *1* change per mode command but
			 * eat up others.
			 */
			if (limitset)
			    {
				if (whatt == MODE_ADD && --parc > 0)
					parv++;
				break;
			    }
			if (whatt == MODE_DEL)
			    {
				limitset = 1;
				nusers = 0;
				break;
			    }
			if (--parc > 0)
			    {
				if (BadPtr(*parv))
					break;
				if (MyClient(sptr) && opcnt >= MAXMODEPARAMS)
					break;
				if (!(nusers = atoi(*++parv)))
					continue;
				lp = &chops[opcnt++];
				lp->flags = MODE_ADD|MODE_LIMIT;
				limitset = 1;
				break;
			    }
			sendto_one(cptr, err_str(ERR_NEEDMOREPARAMS),
					   me.name, cptr->name, "MODE +l");
			break;
		case 'i' : /* falls through for default case */
			if (whatt == MODE_DEL)
				while (lp = chptr->invites)
					del_invite(lp->value.cptr, chptr);
		default:
			for (ip = flags; *ip; ip += 2)
				if (*(ip+1) == *curr)
					break;

			if (*ip)
			    {
				if (whatt == MODE_ADD)
				    {
					if (*ip == MODE_PRIVATE)
						new &= ~MODE_SECRET;
					else if (*ip == MODE_SECRET)
						new &= ~MODE_PRIVATE;
					new |= *ip;
				    }
				else
					new &= ~*ip;
			    }
			else if (!IsServer(cptr) && !IsULine(cptr,sptr))
				sendto_one(cptr, err_str(ERR_UNKNOWNMODE),
					    me.name, cptr->name, *curr);
			break;
		}
		curr++;
		/*
		 * Make sure mode strings such as "+m +t +p +i" are parsed
		 * fully.
		 */
		if (!*curr && parc > 0)
		    {
			curr = *++parv;
			parc--;
			/* If this was from a server, and it is the last
			 * parameter and it starts with a digit, it must
			 * be the creationtime.  --Run
			 */
			if (IsServer(sptr) || IsULine(cptr,sptr))
			{ if (parc==1 && isdigit(*curr))
			  { 
			    newtime=atoi(curr);
			    gotts=1;
			    if (newtime == 0)
			    { *badop=2;
			      hacknotice = 1; }
			    else if (newtime > chptr->creationtime)
			    { /* It is a net-break ride if we have ops.
			       * bounce modes if we have ops.
			       * --Run
			       */
			      if (doesdeop) *badop=2;
			      else if (chptr->creationtime==0 ||
			      	  !have_ops(chptr))
			      { if (chptr->creationtime && doesop)
				  ;
/*			          sendto_ops("NET.RIDE on opless %s from %s",
				      chptr->chname,sptr->name);*/
				if (chptr->creationtime == 0 || doesop)
				  chptr->creationtime=newtime;
				*badop=0; }
			      /* Bounce: */
			      else *badop=1;
			    }
			    else if (doesdeop && newtime < chptr->creationtime)
			      *badop=2;
			    /* A legal *badop can occur when two
			     * people join simultaneously a channel,
			     * Allow for 10 min of lag (and thus hacking
			     * on channels younger then 10 min) --Run
			     */
			    else if (*badop==0 ||
				chptr->creationtime > (NOW-(time_t)600))
			    { if (doesop || !have_ops(chptr))
			        chptr->creationtime=newtime;
		              *badop=0; }
			    break;
			  } }
                        else *badop=0;
		    }
	    } /* end of while loop for MODE processing */

	if (doesop && newtime==0 && (IsServer(sptr) || IsULine(cptr,sptr) || modehack))
	  *badop=2;

	if (*badop>=2 &&
	    (aconf = find_conf_host(cptr->confs, sptr->name, CONF_UWORLD)))
	  *badop=4;
	if (*badop>=2 && (IsULine(cptr,sptr) || modehack))
	  *badop=4;

	/* Fixes channel hacking.  Problem before was that it didn't bounce
	   unless user was deopped by server.  Now modes from *all*
	   non-chanops are bounced.  -- Barubary */
	if (!IsServer(sptr) && !IsULine(cptr, sptr) &&
		!is_chan_op(sptr, chptr)) bounce = 1;
	else bounce = (*badop==1 || *badop==2)?1:0;
	if (IsULine(cptr,sptr) || modehack) bounce = 0;

        whatt = 0;
	for (ip = flags; *ip; ip += 2)
		if ((*ip & new) && !(*ip & oldm.mode))
		    {
			if (bounce)
			{ if (bwhatt != MODE_DEL)
			  { *bmbuf++ = '-';
			    bwhatt = MODE_DEL; }
			  *bmbuf++ = *(ip+1); }
			else
			{ if (whatt != MODE_ADD)
			  { *mbuf++ = '+';
			    whatt = MODE_ADD; }
			  mode->mode |= *ip;
			  *mbuf++ = *(ip+1); }
		    }

	for (ip = flags; *ip; ip += 2)
		if ((*ip & oldm.mode) && !(*ip & new))
		    {
			if (bounce)
			{ if (bwhatt != MODE_ADD)
			  { *bmbuf++ = '+';
			    bwhatt = MODE_ADD; }
			  *bmbuf++ = *(ip+1); }
                        else
			{ if (whatt != MODE_DEL)
			  { *mbuf++ = '-';
			    whatt = MODE_DEL; }
			  mode->mode &= ~*ip;
			  *mbuf++ = *(ip+1); }
		    }

	blen = 0;
	if (limitset && !nusers && mode->limit)
	    {
		if (bounce)
		{ if (bwhatt != MODE_ADD)
		  { *bmbuf++ = '+';
		    bwhatt = MODE_ADD; }
                  *bmbuf++ = 'l';
		  (void)sprintf(numeric, "%-15d", mode->limit);
		  if ((cp = index(numeric, ' '))) *cp = '\0';
		  (void)strcat(bpbuf, numeric);
		  blen += strlen(numeric);
		  (void)strcat(bpbuf, " ");
		}
		else
		{ if (whatt != MODE_DEL)
		  { *mbuf++ = '-';
		    whatt = MODE_DEL; }
		  mode->mode &= ~MODE_LIMIT;
		  mode->limit = 0;
		  *mbuf++ = 'l'; }
	    }
	/*
	 * Reconstruct "+bkov" chain.
	 */
	if (opcnt)
	    {
		int	i = 0;
		char	c;
		char	*user, *host;
		u_int prev_whatt;

		for (; i < opcnt; i++)
		    {
			lp = &chops[i];
			/*
			 * make sure we have correct mode change sign
			 */
			if (whatt != (lp->flags & (MODE_ADD|MODE_DEL)))
				if (lp->flags & MODE_ADD)
				    {
					*mbuf++ = '+';
					prev_whatt = whatt;
					whatt = MODE_ADD;
				    }
				else
				    {
					*mbuf++ = '-';
					prev_whatt = whatt;
					whatt = MODE_DEL;
				    }
			len = strlen(pbuf);
			/*
			 * get c as the mode char and tmp as a pointer to
			 * the parameter for this mode change.
			 */
			switch(lp->flags & MODE_WPARAS)
			{
			case MODE_CHANOP :
				c = 'o';
				cp = lp->value.cptr->name;
				break;
			case MODE_VOICE :
				c = 'v';
				cp = lp->value.cptr->name;
				break;
			case MODE_BAN :
                         /* I made this a bit more user-friendly (tm):
			  * nick = nick!*@*
			  * nick!user = nick!user@*
			  * user@host = *!user@host
			  * host.name = *!*@host.name    --Run
			  */
				c = 'b';
				cp = pretty_mask(lp->value.cp);
				break;
			case MODE_KEY :
				c = 'k';
				cp = lp->value.cp;
				break;
			case MODE_LIMIT :
				c = 'l';
				(void)sprintf(numeric, "%-15d", nusers);
				if ((cp = index(numeric, ' ')))
					*cp = '\0';
				cp = numeric;
				break;
			}

			if (len + strlen(cp) + 12 > (size_t) MODEBUFLEN)
				break;
			switch(lp->flags & MODE_WPARAS)
			{
			case MODE_KEY :
				if (strlen(cp) > (size_t) KEYLEN)
					*(cp+KEYLEN) = '\0';
				if ((whatt == MODE_ADD && (*mode->key=='\0' ||
				    mycmp(mode->key,cp)!=0)) ||
				    (whatt == MODE_DEL && (*mode->key!='\0')))
				{ if (bounce)
				  { if (*mode->key=='\0')
				    { if (bwhatt != MODE_DEL)
				      { *bmbuf++ = '-';
				        bwhatt = MODE_DEL; }
				      (void)strcat(bpbuf, cp);
				      blen += strlen(cp);
				      (void)strcat(bpbuf, " ");
				      blen++; }
                                    else
				    { if (bwhatt != MODE_ADD)
				      { *bmbuf++ = '+';
					bwhatt = MODE_ADD; }
				      (void)strcat(bpbuf, mode->key);
				      blen += strlen(mode->key);
				      (void)strcat(bpbuf, " ");
				      blen++; }
				    *bmbuf++ = c;
				    mbuf--;
				    if (*mbuf!='+' && *mbuf!='-') mbuf++;
				    else whatt = prev_whatt; }
				  else
				  { *mbuf++ = c;
				    (void)strcat(pbuf, cp);
				    len += strlen(cp);
				    (void)strcat(pbuf, " ");
				    len++;
				    if (whatt == MODE_ADD)
				      strncpyzt(mode->key, cp,
					  sizeof(mode->key));
				    else *mode->key = '\0'; } }
				break;
			case MODE_LIMIT :
				if (nusers && nusers != mode->limit)
				{ if (bounce)
				  { if (mode->limit == 0)
				    { if (bwhatt != MODE_DEL)
				      { *bmbuf++ = '-';
				        bwhatt = MODE_DEL; } }
                                    else
				    { if (bwhatt != MODE_ADD)
				      { *bmbuf++ = '+';
					bwhatt = MODE_ADD; }
				      (void)sprintf(numeric, "%-15d",
					  mode->limit);
				      if ((cp = index(numeric, ' ')))
					*cp = '\0';
				      (void)strcat(bpbuf, numeric);
				      blen += strlen(numeric);
				      (void)strcat(bpbuf, " ");
				      blen++;
				    }
				    *bmbuf++ = c;
				    mbuf--;
				    if (*mbuf!='+' && *mbuf!='-') mbuf++;
				    else whatt = prev_whatt; }
				  else
				  { *mbuf++ = c;
				    (void)strcat(pbuf, cp);
				    len += strlen(cp);
				    (void)strcat(pbuf, " ");
				    len++;
				    mode->limit = nusers; } }
				break;
			case MODE_CHANOP :
			case MODE_VOICE :
				tmp = find_user_link(chptr->members,
				    lp->value.cptr);
				if (lp->flags & MODE_ADD)
				{ change=(~tmp->flags) & CHFL_OVERLAP &
				      lp->flags;
				  if (change && bounce)
				  { if (lp->flags & MODE_CHANOP)
				      tmp->flags |= CHFL_DEOPPED;
				    if (bwhatt != MODE_DEL)
				    { *bmbuf++ = '-';
				      bwhatt = MODE_DEL; }
				    *bmbuf++ = c;
				    (void)strcat(bpbuf, lp->value.cptr->name);
				    blen += strlen(lp->value.cptr->name);
				    (void)strcat(bpbuf, " ");
				    blen++;
				    change=0; }
				  else if (change)
				  { tmp->flags |= lp->flags & CHFL_OVERLAP;
				    if (lp->flags & MODE_CHANOP)
				    { tmp->flags &= ~CHFL_DEOPPED;
				      if (IsServer(sptr) || IsULine(cptr,sptr))
					tmp->flags &= ~CHFL_SERVOPOK; } } }
				else
				{ change=tmp->flags & CHFL_OVERLAP & lp->flags;
				  if (change && bounce)
				  { if (lp->flags & MODE_CHANOP)
				      tmp->flags &= ~CHFL_DEOPPED;
				    if (bwhatt != MODE_ADD)
				    { *bmbuf++ = '+';
				      bwhatt = MODE_ADD; }
				    *bmbuf++ = c;
				    (void)strcat(bpbuf, lp->value.cptr->name);
				    blen += strlen(lp->value.cptr->name);
				    (void)strcat(bpbuf, " ");
				    blen++;
				    change=0; }
				  else
				  { tmp->flags &= ~change;
				    if ((change & MODE_CHANOP) &&
					(IsServer(sptr) || IsULine(cptr,sptr) || modehack))
				      tmp->flags |= CHFL_DEOPPED; } }
				if (change || *badop==2 || *badop==4)
				{ *mbuf++ = c;
				  (void)strcat(pbuf, cp);
				  len += strlen(cp);
				  (void)strcat(pbuf, " ");
				  len++; }
				else
				{ mbuf--;
				  if (*mbuf!='+' && *mbuf!='-') mbuf++;
				  else whatt = prev_whatt; }
				break;
			case MODE_BAN :
/* Only bans aren't bounced, it makes no sense to bounce last second
 * bans while propagating bans done before the net.rejoin. The reason
 * why I don't bounce net.rejoin bans is because it is too much
 * work to take care of too long strings adding the necessary TS to
 * net.burst bans -- RunLazy
 * We do have to check for *badop==2 now, we don't want HACKs to take
 * effect.
 */
/* Not anymore.  They're not bounced from servers/ulines, but they *are*
   bounced if from a user without chanops. -- Barubary */
				if (IsServer(sptr) || IsULine(cptr, sptr)
					|| !bounce)
				{
				if (*badop!=2 &&
				    ((whatt & MODE_ADD) &&
				     !add_banid(sptr, chptr, cp) ||
				     (whatt & MODE_DEL) &&
				     !del_banid(chptr, cp)))
				{ 
				  *mbuf++ = c;
				  (void)strcat(pbuf, cp);
				  len += strlen(cp);
				  (void)strcat(pbuf, " ");
				  len++;
				}
				} else {
				if (whatt & MODE_ADD)
				{
				  if (!find_banid(chptr, cp)) {
				  if (bwhatt != MODE_DEL)
					*bmbuf++ = '-';
				  *bmbuf++ = c;
				  strcat(bpbuf, cp);
				  blen += strlen(cp);
				  strcat(bpbuf, " ");
				  blen++; }
				}
				else
				{
				  if (find_banid(chptr, cp)) {
				  if (bwhatt != MODE_ADD)
					*bmbuf++ = '+';
				  *bmbuf++ = c;
				  strcat(bpbuf, cp);
				  blen += strlen(cp);
				  strcat(bpbuf, " ");
				  blen++; }
				}
				}
				break;
			}
		    } /* for (; i < opcnt; i++) */
	    } /* if (opcnt) */

	*mbuf++ = '\0';
	*bmbuf++ = '\0';

/* Bounce here */
	if (!hacknotice && *bmodebuf && chptr->creationtime)
	  sendto_one(cptr,":%s MODE %s %s %s %lu",
	      me.name, chptr->chname, bmodebuf, bparambuf,
	      *badop==2?(time_t)0:chptr->creationtime);

	return gotts?1:-1;
}

static	int	can_join(sptr, chptr, key)
aClient	*sptr;
aChannel *chptr;
char	*key;
{
	Link	*lp;

	if (is_banned(sptr, chptr))
		return (ERR_BANNEDFROMCHAN);
	if (chptr->mode.mode & MODE_INVITEONLY)
	    {
		for (lp = sptr->user->invited; lp; lp = lp->next)
			if (lp->value.chptr == chptr)
				break;
		if (!lp)
			return (ERR_INVITEONLYCHAN);
	    }

	if (*chptr->mode.key && (BadPtr(key) || mycmp(chptr->mode.key, key)))
		return (ERR_BADCHANNELKEY);

	if (chptr->mode.limit && chptr->users >= chptr->mode.limit)
		return (ERR_CHANNELISFULL);

	return 0;
}

/*
** Remove bells and commas from channel name
*/

void	clean_channelname(cn)
char *cn;
{
	for (; *cn; cn++)
		if (*cn == '\007' || *cn == ' ' || *cn == ',')
		    {
			*cn = '\0';
			return;
		    }
}

/*
** Return -1 if mask is present and doesnt match our server name.
*/
static	int	check_channelmask(sptr, cptr, chname)
aClient	*sptr, *cptr;
char	*chname;
{
	char	*s;

	if (*chname == '&' && IsServer(cptr))
		return -1;
	s = rindex(chname, ':');
	if (!s)
		return 0;

	s++;
	if (match(s, me.name) || (IsServer(cptr) && match(s, cptr->name)))
	    {
		if (MyClient(sptr))
			sendto_one(sptr, err_str(ERR_BADCHANMASK),
				   me.name, sptr->name, chname);
		return -1;
	    }
	return 0;
}

/*
**  Get Channel block for i (and allocate a new channel
**  block, if it didn't exists before).
*/
static	aChannel *get_channel(cptr, chname, flag)
aClient *cptr;
char	*chname;
int	flag;
{
	aChannel *chptr;
	int	len;

	if (BadPtr(chname))
		return NULL;

	len = strlen(chname);
	if (MyClient(cptr) && len > CHANNELLEN)
	    {
		len = CHANNELLEN;
		*(chname+CHANNELLEN) = '\0';
	    }
	if ((chptr = find_channel(chname, (aChannel *)NULL)))
		return (chptr);
	if (flag == CREATE)
	    {
		chptr = (aChannel *)MyMalloc(sizeof(aChannel) + len);
		bzero((char *)chptr, sizeof(aChannel));
		strncpyzt(chptr->chname, chname, len+1);
		if (channel)
			channel->prevch = chptr;
		chptr->prevch = NULL;
		chptr->nextch = channel;
		chptr->creationtime = MyClient(cptr)?NOW:(time_t)0;
		channel = chptr;
		(void)add_to_channel_hash_table(chname, chptr);
	    }
	return chptr;
    }

static	void	add_invite(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Link	*inv, **tmp;

	del_invite(cptr, chptr);
	/*
	 * delete last link in chain if the list is max length
	 */
	if (list_length(cptr->user->invited) >= MAXCHANNELSPERUSER)
	    {
/*		This forgets the channel side of invitation     -Vesa
		inv = cptr->user->invited;
		cptr->user->invited = inv->next;
		free_link(inv);
*/
		del_invite(cptr, cptr->user->invited->value.chptr);
 
	    }
	/*
	 * add client to channel invite list
	 */
	inv = make_link();
	inv->value.cptr = cptr;
	inv->next = chptr->invites;
	chptr->invites = inv;
	/*
	 * add channel to the end of the client invite list
	 */
	for (tmp = &(cptr->user->invited); *tmp; tmp = &((*tmp)->next))
		;
	inv = make_link();
	inv->value.chptr = chptr;
	inv->next = NULL;
	(*tmp) = inv;
}

/*
 * Delete Invite block from channel invite list and client invite list
 */
void	del_invite(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Link	**inv, *tmp;

	for (inv = &(chptr->invites); (tmp = *inv); inv = &tmp->next)
		if (tmp->value.cptr == cptr)
		    {
			*inv = tmp->next;
			free_link(tmp);
			break;
		    }

	for (inv = &(cptr->user->invited); (tmp = *inv); inv = &tmp->next)
		if (tmp->value.chptr == chptr)
		    {
			*inv = tmp->next;
			free_link(tmp);
			break;
		    }
}

/*
**  Subtract one user from channel i (and free channel
**  block, if channel became empty).
*/
static	void	sub1_from_channel(chptr)
aChannel *chptr;
{
	Link *tmp;
	Link	*obtmp;

	if (--chptr->users <= 0)
	    {
		/*
		 * Now, find all invite links from channel structure
		 */
		while ((tmp = chptr->invites))
			del_invite(tmp->value.cptr, chptr);

		while (chptr->banlist)
		{
			tmp = chptr->banlist;
			chptr->banlist = tmp->next;
			MyFree(tmp->value.ban.banstr);
			MyFree(tmp->value.ban.who);
			free_link(tmp);
		}
		if (chptr->prevch)
			chptr->prevch->nextch = chptr->nextch;
		else
			channel = chptr->nextch;
		if (chptr->nextch)
			chptr->nextch->prevch = chptr->prevch;
		(void)del_from_channel_hash_table(chptr->chname, chptr);
		MyFree((char *)chptr);
	    }
}

/*
** m_join
**	parv[0] = sender prefix
**	parv[1] = channel
**	parv[2] = channel password (key)
*/
int	m_join(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	static	char	jbuf[BUFSIZE];
	Link	*lp;
	aChannel *chptr;
	char	*name, *key = NULL;
	int	i, flags = 0, zombie = 0;
	char	*p = NULL, *p2 = NULL;

	if (check_registered_user(sptr))
		return 0;

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "JOIN");
		return 0;
	    }

	*jbuf = '\0';
	/*
	** Rebuild list of channels joined to be the actual result of the
	** JOIN.  Note that "JOIN 0" is the destructive problem.
	*/
	for (i = 0, name = strtoken(&p, parv[1], ","); name;
	     name = strtoken(&p, NULL, ","))
	    {
		clean_channelname(name);
		if (check_channelmask(sptr, cptr, name)==-1)
			continue;
		if (*name == '&' && !MyConnect(sptr))
			continue;
		if (*name == '0' && !atoi(name))
		    {
		    	(void)strcpy(jbuf, "0");
		        i = 1;
		        continue;
		    }
		else if (!IsChannelName(name))
		    {
			if (MyClient(sptr))
				sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL),
					   me.name, parv[0], name);
			continue;
		    }
		if (*jbuf)
			(void)strcat(jbuf, ",");
		(void)strncat(jbuf, name, sizeof(jbuf) - i - 1);
		i += strlen(name)+1;
	    }
	(void)strcpy(parv[1], jbuf);

	p = NULL;
	if (parv[2])
		key = strtoken(&p2, parv[2], ",");
	parv[2] = NULL;	/* for m_names call later, parv[parc] must == NULL */
	for (name = strtoken(&p, jbuf, ","); name;
	     key = (key) ? strtoken(&p2, NULL, ",") : NULL,
	     name = strtoken(&p, NULL, ","))
	    {
		/*
		** JOIN 0 sends out a part for all channels a user
		** has joined.
		*/
		if (*name == '0' && !atoi(name))
		    {
			while ((lp = sptr->user->channel))
			    {
				chptr = lp->value.chptr;
				if (!is_zombie(sptr, chptr))
				  sendto_channel_butserv(chptr, sptr, PartFmt,
				      parv[0], chptr->chname);
				remove_user_from_channel(sptr, chptr);
			    }
			sendto_serv_butone(cptr, ":%s JOIN 0", parv[0]);
			continue;
		    }

		if (MyConnect(sptr))
		    {
			/*
			** local client is first to enter previously nonexistant
			** channel so make them (rightfully) the Channel
			** Operator.
			*/
			if (!IsModelessChannel(name))
			    flags = (ChannelExists(name)) ? CHFL_DEOPPED :
							    CHFL_CHANOP;
			else
			    flags = CHFL_DEOPPED;

			if (sptr->user->joined >= MAXCHANNELSPERUSER)
			    {
				sendto_one(sptr, err_str(ERR_TOOMANYCHANNELS),
					   me.name, parv[0], name);
				return 0;
			    }
		    }
		chptr = get_channel(sptr, name, CREATE);
                if (chptr && (lp=find_user_link(chptr->members, sptr)))
		{ if (lp->flags & CHFL_ZOMBIE)
		  { zombie = 1;
		    flags = lp->flags & (CHFL_DEOPPED|CHFL_SERVOPOK);
		    remove_user_from_channel(sptr, chptr);
		    chptr = get_channel(sptr, name, CREATE); }
		  else continue; }
		if (!zombie)
		{ if (!MyConnect(sptr)) flags = CHFL_DEOPPED;
		  if (ClientFlags(sptr) & FLAGS_TS8) flags|=CHFL_SERVOPOK; }
		if (!chptr ||
		    (MyConnect(sptr) && (i = can_join(sptr, chptr, key))))
		    {
			sendto_one(sptr,
				   ":%s %d %s %s :Sorry, cannot join channel.",
				   me.name, i, parv[0], name);
			continue;
		    }

		/*
		**  Complete user entry to the new channel (if any)
		*/
		add_user_to_channel(chptr, sptr, flags);
		/*
		** notify all other users on the new channel
		*/
		sendto_channel_butserv(chptr, sptr, ":%s JOIN :%s",
					parv[0], name);
		sendto_match_servs(chptr, cptr, ":%s JOIN :%s", parv[0], name);

		if (MyClient(sptr))
		    {
		        /*
			** Make a (temporal) creationtime, if someone joins
			** during a net.reconnect : between remote join and
			** the mode with TS. --Run
			*/
		        if (chptr->creationtime == 0)
		        { chptr->creationtime = NOW;
			  sendto_match_servs(chptr, cptr, ":%s MODE %s + %lu",
			      me.name, name, chptr->creationtime);
			}
			del_invite(sptr, chptr);
			if (flags & CHFL_CHANOP)
				sendto_match_servs(chptr, cptr,
				  ":%s MODE %s +o %s %lu",
				  me.name, name, parv[0], chptr->creationtime);
			if (chptr->topic[0] != '\0') {
				sendto_one(sptr, rpl_str(RPL_TOPIC), me.name,
					   parv[0], name, chptr->topic);
				sendto_one(sptr, rpl_str(RPL_TOPICWHOTIME),
					    me.name, parv[0], name,
					    chptr->topic_nick,
					    chptr->topic_time);
			      }
			parv[1] = name;
			(void)m_names(cptr, sptr, 2, parv);
		    }
	    }
	return 0;
}

/*
** m_part
**	parv[0] = sender prefix
**	parv[1] = channel
**	parv[2] = comment (added by Lefler)
*/
int	m_part(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aChannel *chptr;
	Link	*lp;
	char	*p = NULL, *name;
	char *comment = (parc > 2 && parv[2]) ? parv[2] : NULL;

	if (check_registered_user(sptr))
		return 0;

        ClientFlags(sptr) &=~FLAGS_TS8;

	if (parc < 2 || parv[1][0] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "PART");
		return 0;
	    }

#ifdef	V28PlusOnly
	*buf = '\0';
#endif

	for (; (name = strtoken(&p, parv[1], ",")); parv[1] = NULL)
	    {
		chptr = get_channel(sptr, name, 0);
		if (!chptr)
		    {
			sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL),
				   me.name, parv[0], name);
			continue;
		    }
		if (check_channelmask(sptr, cptr, name))
			continue;
		/* Do not use IsMember here: zombies must be able to part too */
		if (!(lp=find_user_link(chptr->members, sptr)))
		    {
			/* Normal to get get when our client did a kick
			** for a remote client (who sends back a PART),
			** so check for remote client or not --Run
			*/
			if (MyClient(sptr))
			  sendto_one(sptr, err_str(ERR_NOTONCHANNEL),
		    	      me.name, parv[0], name);
			continue;
		    }
		/*
		**  Remove user from the old channel (if any)
		*/
#ifdef	V28PlusOnly
		if (*buf)
			(void)strcat(buf, ",");
		(void)strcat(buf, name);
#else
                if (parc < 3)
		  sendto_match_servs(chptr, cptr, PartFmt, parv[0], name);
		else
		  sendto_match_servs(chptr, cptr, PartFmt2, parv[0], name, comment);
#endif
		if (!(lp->flags & CHFL_ZOMBIE)) {
                if (parc < 3)
		  sendto_channel_butserv(chptr, sptr, PartFmt, parv[0], name);
		else
		  sendto_channel_butserv(chptr, sptr, PartFmt2, parv[0], name, comment);
                } else if (MyClient(sptr)) {
		  if (parc < 3)
		    sendto_one(sptr, PartFmt, parv[0], name);
		  else
		    sendto_one(sptr, PartFmt2, parv[0], name, comment);
                }
		remove_user_from_channel(sptr, chptr);
	    }
#ifdef	V28PlusOnly
	if (*buf)
		sendto_serv_butone(cptr, PartFmt, parv[0], buf);
#endif
	return 0;
    }

/*
** m_kick
**	parv[0] = sender prefix
**	parv[1] = channel
**	parv[2] = client to kick
**	parv[3] = kick comment
*/
int	m_kick(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aClient *who;
	aChannel *chptr;
	int	chasing = 0;
	char	*comment, *name, *p = NULL, *user, *p2 = NULL;
	Link *lp,*lp2;

	if (check_registered(sptr))
		return 0;

	ClientFlags(sptr) &=~FLAGS_TS8;

	if (parc < 3 || *parv[1] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "KICK");
		return 0;
	    }

        if (IsServer(sptr) && !IsULine(cptr, sptr))
	  sendto_ops("HACK: KICK from %s for %s %s", parv[0], parv[1], parv[2]);

	comment = (BadPtr(parv[3])) ? parv[0] : parv[3];
	if (strlen(comment) > (size_t) TOPICLEN)
		comment[TOPICLEN] = '\0';

	*nickbuf = *buf = '\0';

	for (; (name = strtoken(&p, parv[1], ",")); parv[1] = NULL)
	    {
		chptr = get_channel(sptr, name, !CREATE);
		if (!chptr)
		    {
			sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL),
				   me.name, parv[0], name);
			continue;
		    }
		if (check_channelmask(sptr, cptr, name))
			continue;
		if (!IsServer(cptr) && !IsULine(cptr,sptr) && !is_chan_op(sptr, chptr))
		    {
			sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
				   me.name, parv[0], chptr->chname);
			continue;
		    }

	        lp2=find_user_link(chptr->members, sptr);
		for (; (user = strtoken(&p2, parv[2], ",")); parv[2] = NULL)
		    {   if (!(who = find_chasing(sptr, user, &chasing)))
				continue; /* No such user left! */
			if ((lp=find_user_link(chptr->members, who)) &&
			    !(lp->flags & CHFL_ZOMBIE) || IsServer(sptr))
	                {
			  /* Bounce all KICKs from a non-chanop unless it
			     would cause a fake direction -- Barubary */
			  if ((who->from != cptr) && !IsServer(sptr) &&
			     !IsULine(cptr, sptr) && !is_chan_op(sptr, chptr))
/*			  if (who->from!=cptr &&
			     ((lp2 && (lp2->flags & CHFL_DEOPPED)) ||
			     (!lp2 && IsPerson(sptr))) && !IsULine(cptr,sptr))*/
			  {
			  /* Bounce here:
			   * cptr must be a server (or cptr==sptr and
			   * sptr->flags can't have DEOPPED set
			   * when CHANOP is set).
			   */
			    sendto_one(cptr,":%s JOIN %s",who->name,name);
			    /* Slight change here - the MODE is sent only
			       once even if overlapping -- Barubary */
			    if (lp->flags & CHFL_OVERLAP)
			    {
				char *temp = buf;
				*temp++ = '+';
				if (lp->flags & CHFL_CHANOP) *temp++ = 'o';
				if (lp->flags & CHFL_VOICE) *temp++ = 'v';
				*temp = 0;
				sendto_one(cptr, ":%s MODE %s %s %s %s %lu",
				me.name, chptr->chname, buf, who->name,
				((lp->flags & CHFL_OVERLAP) == CHFL_OVERLAP)
				? who->name : "", chptr->creationtime);
			    }
			/*  if (lp->flags & CHFL_CHANOP)
			      sendmodeto_one(cptr, me.name, name, "+o",
			          who->name, chptr->creationtime);
			    if (lp->flags & CHFL_VOICE)
			      sendmodeto_one(cptr, me.name, name, "+v",
			      who->name, chptr->creationtime); */
			  }
			  else
			  {
			    if (lp)
			      sendto_channel_butserv(chptr, sptr,
			          ":%s KICK %s %s :%s", parv[0],
			          name, who->name, comment);
			    sendto_match_servs(chptr, cptr,
					       ":%s KICK %s %s :%s",
					       parv[0], name,
					       who->name, comment);
			    /* Finally, zombies totally removed -- Barubary */
			    if (lp)
			    {
			      if (MyClient(who))
			      { sendto_match_servs(chptr, NULL,
				      PartFmt, who->name, name);
			        remove_user_from_channel(who, chptr); }
			      else
			      {
			          remove_user_from_channel(who, chptr); }
			    }
			  }
			}
			else if (MyClient(sptr))
			  sendto_one(sptr, err_str(ERR_USERNOTINCHANNEL),
					   me.name, parv[0], user, name);
			if (!IsServer(cptr) || !IsULine(cptr,sptr))
			  break;
		    } /* loop on parv[2] */
		if (!IsServer(cptr) || !IsULine(cptr,sptr))
		  break;
	    } /* loop on parv[1] */

	return (0);
}

int	count_channels(sptr)
aClient	*sptr;
{
  aChannel	*chptr;
  int	count = 0;

  for (chptr = channel; chptr; chptr = chptr->nextch)
    count++;
  return (count);
}

/*
** m_topic
**	parv[0] = sender prefix
**	parv[1] = topic text
**
**	For servers using TS: (Lefler)
**	parv[0] = sender prefix
**	parv[1] = channel list
**	parv[2] = topic nickname
**	parv[3] = topic time
**	parv[4] = topic text
*/
int	m_topic(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aChannel *chptr = NullChn;
	char	*topic = NULL, *name, *p = NULL, *tnick = NULL;
	time_t	ttime = 0;
	
	if (check_registered(sptr))
		return 0;

	if (parc < 2)
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "TOPIC");
		return 0;
	    }

	for (; (name = strtoken(&p, parv[1], ",")); parv[1] = NULL)
	    {
		if (parc > 1 && IsChannelName(name))
		    {
			chptr = find_channel(name, NullChn);
			if (!chptr || (!IsMember(sptr, chptr) &&
			    !IsServer(sptr) && !IsULine(cptr,sptr)))
			    {
				sendto_one(sptr, err_str(ERR_NOTONCHANNEL),
					   me.name, parv[0], name);
				continue;
			    }
			if (parc > 2)
				topic = parv[2];
			if (parc > 4) {
				tnick = parv[2];
				ttime = atol(parv[3]);
				topic = parv[4];
			}
		    }

		if (!chptr)
		    {
			sendto_one(sptr, rpl_str(RPL_NOTOPIC),
				   me.name, parv[0], name);
			return 0;
		    }

		if (check_channelmask(sptr, cptr, name))
			continue;
	
		if (!topic)  /* only asking  for topic  */
		    {
			if (chptr->topic[0] == '\0')
			sendto_one(sptr, rpl_str(RPL_NOTOPIC),
				   me.name, parv[0], chptr->chname);
			else {
				sendto_one(sptr, rpl_str(RPL_TOPIC),
					   me.name, parv[0],
					   chptr->chname, chptr->topic);
				sendto_one(sptr, rpl_str(RPL_TOPICWHOTIME),
					   me.name, parv[0], chptr->chname,
					   chptr->topic_nick,
					   chptr->topic_time);
		    } 
		    } 
		else if (ttime && topic && (IsServer(sptr) || IsULine(cptr,sptr)))
		    {
			if (!chptr->topic_time || ttime < chptr->topic_time)
			   {
				/* setting a topic */
				strncpyzt(chptr->topic, topic, sizeof(chptr->topic));
				strcpy(chptr->topic_nick, tnick);
				chptr->topic_time = ttime;
				sendto_match_servs(chptr, cptr,":%s TOPIC %s %s %lu :%s",
					   parv[0], chptr->chname,
					   chptr->topic_nick, chptr->topic_time,
					   chptr->topic);
				sendto_channel_butserv(chptr, sptr, ":%s TOPIC %s :%s (%s)",
					       parv[0], chptr->chname,
					       chptr->topic, chptr->topic_nick);
			   }
		    }
		else if (((chptr->mode.mode & MODE_TOPICLIMIT) == 0 ||
			  is_chan_op(sptr, chptr)) || IsULine(cptr,sptr)
			  && topic)
		    {
			/* setting a topic */
			strncpyzt(chptr->topic, topic, sizeof(chptr->topic));
			strcpy(chptr->topic_nick, sptr->name);
                        if (ttime && IsServer(cptr))
			  chptr->topic_time = ttime;
			else
			  chptr->topic_time = NOW;
			sendto_match_servs(chptr, cptr,":%s TOPIC %s %s %lu :%s",
					   parv[0], chptr->chname, parv[0],
					   chptr->topic_time, chptr->topic);
			sendto_channel_butserv(chptr, sptr, ":%s TOPIC %s :%s",
					       parv[0],
					       chptr->chname, chptr->topic);
		    }
		else
		      sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
				 me.name, parv[0], chptr->chname);
	    }
	return 0;
    }

/*
** m_invite
**	parv[0] - sender prefix
**	parv[1] - user to invite
**	parv[2] - channel number
*/
int	m_invite(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aClient *acptr;
	aChannel *chptr;

	if (check_registered_user(sptr))
		return 0;

	if (parc < 3 || *parv[1] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "INVITE");
		return -1;
	    }

	if (!(acptr = find_person(parv[1], (aClient *)NULL)))
	    {
		sendto_one(sptr, err_str(ERR_NOSUCHNICK),
			   me.name, parv[0], parv[1]);
		return 0;
	    }
	clean_channelname(parv[2]);
	if (check_channelmask(sptr, cptr, parv[2]))
		return 0;
	if (!(chptr = find_channel(parv[2], NullChn)))
	    {
		sendto_one(sptr, err_str(ERR_NOTONCHANNEL),
			   me.name, parv[0], parv[2]);
		return -1;
	    }

	if (chptr && !IsMember(sptr, chptr) && !IsULine(cptr,sptr))
	    {
		sendto_one(sptr, err_str(ERR_NOTONCHANNEL),
			   me.name, parv[0], parv[2]);
		return -1;
	    }

	if (IsMember(acptr, chptr))
	    {
		sendto_one(sptr, err_str(ERR_USERONCHANNEL),
			   me.name, parv[0], parv[1], parv[2]);
		return 0;
	    }
	if (chptr && (chptr->mode.mode & MODE_INVITEONLY))
	    {
		if (!is_chan_op(sptr, chptr) && !IsULine(cptr,sptr))
		    {
			sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
				   me.name, parv[0], chptr->chname);
			return -1;
		    }
		else if (!IsMember(sptr, chptr) && !IsULine(cptr,sptr))
		    {
			sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
				   me.name, parv[0],
				   ((chptr) ? (chptr->chname) : parv[2]));
			return -1;
		    }
	    }

	if (MyConnect(sptr))
	    {
		sendto_one(sptr, rpl_str(RPL_INVITING), me.name, parv[0],
			   acptr->name, ((chptr) ? (chptr->chname) : parv[2]));
		if (acptr->user->away)
			sendto_one(sptr, rpl_str(RPL_AWAY), me.name, parv[0],
				   acptr->name, acptr->user->away);
	    }
	if (MyConnect(acptr))
		if (chptr && (chptr->mode.mode & MODE_INVITEONLY) &&
		    sptr->user && (is_chan_op(sptr, chptr) || IsULine(cptr,sptr)))
			add_invite(acptr, chptr);
	sendto_prefix_one(acptr, sptr, ":%s INVITE %s :%s",parv[0],
			  acptr->name, ((chptr) ? (chptr->chname) : parv[2]));
	return 0;
    }

static int number_of_zombies(chptr)
aChannel *chptr;
{
  Link *lp;
  int count = 0;

  for (lp=chptr->members; lp; lp=lp->next)
    if (lp->flags & CHFL_ZOMBIE)
      count++;

  return count;	
}

/*
** m_list
**      parv[0] = sender prefix
**      parv[1] = channel
*/
int	m_list(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aChannel *chptr;
	char	*name, *p = NULL;

	/* None of that unregistered LIST stuff.  -- Barubary */
	if (check_registered(sptr)) return 0;

	sendto_one(sptr, rpl_str(RPL_LISTSTART), me.name, parv[0]);

	if (parc < 2 || BadPtr(parv[1]))
	    {
		for (chptr = channel; chptr; chptr = chptr->nextch)
		    {
			if (!sptr->user ||
			    (SecretChannel(chptr) && !IsMember(sptr, chptr)))
				continue;
			/* Don't know if remote LISTs are possible, but
			   it's protected against anyways */
			/* Remote LISTs only see & channels -- Barubary */
			if ((cptr != sptr) && (*chptr->chname != '&'))
				continue;
			sendto_one(sptr, rpl_str(RPL_LIST), me.name, parv[0],
				   ShowChannel(sptr, chptr)?chptr->chname:"*",
				   chptr->users - number_of_zombies(chptr),
				   ShowChannel(sptr, chptr)?chptr->topic:"");
		    }
		sendto_one(sptr, rpl_str(RPL_LISTEND), me.name, parv[0]);
		return 0;
	    }

	if (hunt_server(cptr, sptr, ":%s LIST %s %s", 2, parc, parv))
		return 0;

	for (; (name = strtoken(&p, parv[1], ",")); parv[1] = NULL)
	    {
		chptr = find_channel(name, NullChn);
		if (chptr && ShowChannel(sptr, chptr) && sptr->user)
			sendto_one(sptr, rpl_str(RPL_LIST), me.name, parv[0],
				   ShowChannel(sptr,chptr) ? name : "*",
				   chptr->users - number_of_zombies(chptr),
				   chptr->topic);
	     }
	sendto_one(sptr, rpl_str(RPL_LISTEND), me.name, parv[0]);
	return 0;
}


/************************************************************************
 * m_names() - Added by Jto 27 Apr 1989
 ************************************************************************/

/*
** m_names
**	parv[0] = sender prefix
**	parv[1] = channel
*/
int	m_names(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{ 
	aChannel *chptr;
	aClient *c2ptr;
	Link	*lp;
	aChannel *ch2ptr = NULL;
	int	idx, flag, len, mlen, x;
	char	*s, *para = parc > 1 ? parv[1] : NULL;

	if (check_registered(sptr)) return 0;

	if (parc > 1 &&
	    hunt_server(cptr, sptr, ":%s NAMES %s %s", 2, parc, parv))
		return 0;

	mlen = strlen(me.name) + 10;

	/* Only 10 requests per NAMES allowed remotely -- Barubary */
	if (!BadPtr(para) && (cptr != sptr))
	for (x = 0, s = para; *s; s++)
		if (*s == ',')
		if (++x == 10)
		{
			*s = 0;
			break;
		}

	if (BadPtr(para) && IsServer(cptr))
	{
	sendto_one(cptr, rpl_str(RPL_ENDOFNAMES), me.name, parv[0], "*");
	return 0;
	}

	if (!BadPtr(para))
	    {
		s = index(para, ',');
		if (s)
		    {
			parv[1] = ++s;
			(void)m_names(cptr, sptr, parc, parv);
		    }
		clean_channelname(para);
		ch2ptr = find_channel(para, (aChannel *)NULL);
	    }

	*buf = '\0';

	/* Allow NAMES without registering
	 *
	 * First, do all visible channels (public and the one user self is)
	 */

	for (chptr = channel; chptr; chptr = chptr->nextch)
	    {
		if ((chptr != ch2ptr) && !BadPtr(para))
			continue; /* -- wanted a specific channel */
		if (!MyConnect(sptr) && BadPtr(para))
			continue;
		if (!ShowChannel(sptr, chptr))
			continue; /* -- users on this are not listed */

		/* Find users on same channel (defined by chptr) */

		(void)strcpy(buf, "* ");
		len = strlen(chptr->chname);
		(void)strcpy(buf + 2, chptr->chname);
		(void)strcpy(buf + 2 + len, " :");

		if (PubChannel(chptr))
			*buf = '=';
		else if (SecretChannel(chptr))
			*buf = '@';
		idx = len + 4;
		flag = 1;
		for (lp = chptr->members; lp; lp = lp->next)
		    {
			c2ptr = lp->value.cptr;
			if (sptr!=c2ptr && IsInvisible(c2ptr) &&
			  !IsMember(sptr,chptr))
				continue;
			if (lp->flags & CHFL_ZOMBIE)
			{ if (lp->value.cptr!=sptr)
				continue;
			  else
				(void)strcat(buf, "!"); }
		        else if (lp->flags & CHFL_CHANOP)
				(void)strcat(buf, "@");
			else if (lp->flags & CHFL_VOICE)
				(void)strcat(buf, "+");
			(void)strncat(buf, c2ptr->name, NICKLEN);
			idx += strlen(c2ptr->name) + 1;
			flag = 1;
			(void)strcat(buf," ");
			if (mlen + idx + NICKLEN > BUFSIZE - 2)
			    {
				sendto_one(sptr, rpl_str(RPL_NAMREPLY),
					   me.name, parv[0], buf);
				(void)strncpy(buf, "* ", 3);
				(void)strncpy(buf+2, chptr->chname, len + 1);
				(void)strcat(buf, " :");
				if (PubChannel(chptr))
					*buf = '=';
				else if (SecretChannel(chptr))
					*buf = '@';
				idx = len + 4;
				flag = 0;
			    }
		    }
		if (flag)
			sendto_one(sptr, rpl_str(RPL_NAMREPLY),
				   me.name, parv[0], buf);
	    }
	if (!BadPtr(para))
	    {
		sendto_one(sptr, rpl_str(RPL_ENDOFNAMES), me.name, parv[0],
			   para);
		return(1);
	    }

	/* Second, do all non-public, non-secret channels in one big sweep */

	(void)strncpy(buf, "* * :", 6);
	idx = 5;
	flag = 0;
	for (c2ptr = client; c2ptr; c2ptr = c2ptr->next)
	    {
  	        aChannel *ch3ptr;
		int	showflag = 0, secret = 0;

		if (!IsPerson(c2ptr) || sptr!=c2ptr && IsInvisible(c2ptr))
			continue;
		lp = c2ptr->user->channel;
		/*
		 * dont show a client if they are on a secret channel or
		 * they are on a channel sptr is on since they have already
		 * been show earlier. -avalon
		 */
		while (lp)
		    {
			ch3ptr = lp->value.chptr;
			if (PubChannel(ch3ptr) || IsMember(sptr, ch3ptr))
				showflag = 1;
			if (SecretChannel(ch3ptr))
				secret = 1;
			lp = lp->next;
		    }
		if (showflag) /* have we already shown them ? */
			continue;
		if (secret) /* on any secret channels ? */
			continue;
		(void)strncat(buf, c2ptr->name, NICKLEN);
		idx += strlen(c2ptr->name) + 1;
		(void)strcat(buf," ");
		flag = 1;
		if (mlen + idx + NICKLEN > BUFSIZE - 2)
		    {
			sendto_one(sptr, rpl_str(RPL_NAMREPLY),
				   me.name, parv[0], buf);
			(void)strncpy(buf, "* * :", 6);
			idx = 5;
			flag = 0;
		    }
	    }
	if (flag)
		sendto_one(sptr, rpl_str(RPL_NAMREPLY), me.name, parv[0], buf);
	sendto_one(sptr, rpl_str(RPL_ENDOFNAMES), me.name, parv[0], "*");
	return(1);
    }


void	send_user_joins(cptr, user)
aClient	*cptr, *user;
{
	Link	*lp;
	aChannel *chptr;
	int	cnt = 0, len = 0, clen;
	char	 *mask;

	*buf = ':';
	(void)strcpy(buf+1, user->name);
	(void)strcat(buf, " JOIN ");
	len = strlen(user->name) + 7;

	for (lp = user->user->channel; lp; lp = lp->next)
	    {
		chptr = lp->value.chptr;
		if ((mask = index(chptr->chname, ':')))
			if (match(++mask, cptr->name))
				continue;
		if (*chptr->chname == '&')
			continue;
		if (is_zombie(user, chptr))
			continue;
		clen = strlen(chptr->chname);
		if (clen + 1 + len > BUFSIZE - 3)
		    {
			if (cnt)
			{
				buf[len-1]='\0';
				sendto_one(cptr, "%s", buf);
			}
			*buf = ':';
			(void)strcpy(buf+1, user->name);
			(void)strcat(buf, " JOIN ");
			len = strlen(user->name) + 7;
			cnt = 0;
		    }
		(void)strcpy(buf + len, chptr->chname);
		cnt++;
		len += clen;
		if (lp->next)
		    {
			len++;
			(void)strcat(buf, ",");
		    }
	    }
	if (*buf && cnt)
		sendto_one(cptr, "%s", buf);

	return;
}
