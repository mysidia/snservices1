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

#include "ircd.h"

#include "channel.h"
#include "hash.h"
#include "msg.h"

#include "ircd/channel.h"

IRCD_SCCSID("@(#)channel.c	2.58 2/18/94 (C) 1990 University of Oulu, Computing Center and Jarkko Oikarinen");
IRCD_RCSID("$Id$");

aChannel *channel = NullChn;

static	void	add_invite(aClient *, aChannel *);
static	int	add_banid(aClient *, aChannel *, char *);
static	int	can_join(aClient *, aChannel *, char *);
static	void	channel_modes(aClient *, char *, char *, aChannel *);
static	int	check_channelmask(aClient *, aClient *, char *);
static	int	del_banid(aChannel *, char *);
static	int	find_banid(aChannel *, char *);
/*static	Link	*is_banned(aClient *, aChannel *);*/
static  int     have_ops(aChannel *);
static	int	set_mode(aClient *, aClient *, aChannel *, int,\
			 char **, char *,char *, int *, int *, int);
static	void	sub1_from_channel(aChannel *);
static	void	set_topic(aChannel *, const char *, const int);
char *genHostMask(char *host);


void	clean_channelname(char *);
void	del_invite(aClient *, aChannel *);

void mlock_buf(aChannel *chptr, char *buf);
int legalize_mode(aChannel *chptr, char **curr);

static	char	*PartFmt = ":%s PART %s";
static	char	*PartFmt2 = ":%s PART %s :%s";
/*
 * some buffers for rebuilding channel/nick lists with ,'s
 */
static	char	nickbuf[BUFSIZE], buf[BUFSIZE];
static	char	modebuf[MODEBUFLEN], parabuf[MODEBUFLEN], parabufK[MODEBUFLEN*2];
static  char    mlockbuf[MODEBUFLEN];

static	int	chan_flags[] = {
	MODE_PRIVATE,    'p', 
	MODE_SECRET,     's',
	MODE_MODERATED,  'm',
	MODE_NOPRIVMSGS, 'n',
	MODE_TOPICLIMIT, 't', 
	MODE_INVITEONLY, 'i',
	MODE_VOICE,      'v',
	MODE_KEY,        'k',
	MODE_SHOWHOST,   'H',
	MODE_NOCOLORS,   'c',
	0x0, 0x0 
};

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
  char *s;
  
  if (!mask)
    return 0;

  s = strchr(mask, '@');

  if ( s )
	mask = s;
  
  if (strchr(mask, ':'))
	  return 1;

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

  host = address_tostring(cptr->sock->raddr, 0);
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

static	char *bmake_nick_user_host(namebuf, nick, name, host)
char	*namebuf, *nick, *name, *host;
{
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

static	char *make_nick_user_host(nick, name, host)
char	*nick, *name, *host;
{
	static	char	namebuf[NICKLEN+USERLEN+HOSTLEN+7];

	return bmake_nick_user_host(namebuf, nick, name, host);
}

void check_bquiet(aChannel *chptr, aClient *sptr, int add)
{
	int bantype = 0;
	Link *lp;

	if (!chptr)
		return;

	for (lp = chptr->members; lp; lp = lp->next) {
		if (!MyClient(lp->value.cptr))
			continue;
		if (sptr && sptr != lp->value.cptr)
			continue;
		if ((add && (lp->flags & CHFL_BQUIET)) ||
		    (!add && !(lp->flags & CHFL_BQUIET)))
			continue;
		if (is_banned(lp->value.cptr, chptr, &bantype)) {
			if (bantype & BAN_BQUIET)
				lp->flags |= CHFL_BQUIET;
			else
				lp->flags &= ~CHFL_BQUIET;
		}
		else
			lp->flags &= ~CHFL_BQUIET;
	}
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
	ban->value.ban.banstr = irc_malloc(strlen(banid)+1);
	(void)strcpy(ban->value.ban.banstr, banid);
	ban->value.ban.who = irc_malloc(strlen(cptr->name)+1);
	(void)strcpy(ban->value.ban.who, cptr->name);
	ban->value.ban.when = NOW;
	chptr->banlist = ban;
	check_bquiet(chptr, NULL, 1);

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
	Link *lp;
        int bantype = 0;

	if (!banid)
		return -1;
 	for (ban = &(chptr->banlist); *ban; ban = &((*ban)->next))
		if (mycmp(banid, (*ban)->value.ban.banstr)==0)
		    {
			tmp = *ban;
			*ban = tmp->next;
			irc_free(tmp->value.ban.banstr);
			irc_free(tmp->value.ban.who);
			free_link(tmp);
			for (lp = chptr->members; lp; lp = lp->next) {
			     if (!(lp->flags & CHFL_BQUIET))
	 		        continue;
		             if (!is_banned(lp->value.cptr, chptr, &bantype)
                                 || !(bantype & BAN_BQUIET))
		 	         lp->flags &= ~CHFL_BQUIET;
			}

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

	if (!banid)
		return -1;
	for (ban = &(chptr->banlist); *ban; ban = &((*ban)->next))
		if (!mycmp(banid, (*ban)->value.ban.banstr)) return 1;
	return 0;
}

/*
 * index_left_part( text, substring, int *pointer ) :
 *    Tries to find 'substring' at the beginning of text.
 *       if successful returns the index 1 after the end, else 0
 */
int index_left_part(const char* text, const char* substring)
{
	int i = 0;

	for(; *substring != '\0'; substring++) {
		if (*text == '\0') {
			return 0;
		}

		if (irc_tolower(*text) != irc_tolower(*substring))  {
			return 0;
		}
		text++;
		i++;
	}

	return i;
}

/*
 * IsMember - returns 1 if a person has joined the given channel
 */
int	IsMember(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
        Link *lp;

	return (((lp=find_user_link(chptr->members, cptr)))?1:0);
}

int BanRuleMatch(const char *text, aClient *cptr, int *result,
                 const char *nuh, const char *nuhmask,
                 const char *sip)
{
	int pattern_start = 0, negate_rule = 0;
	int ban_flags = (result ? *result : BAN_STD);

	if (!text || !cptr || *text != '$')
	    return 0;
        text++;

	if (irc_tolower(*text) == '!' && text[1] != '\0') {
		negate_rule = 1;
		text++;
	}

	if (irc_tolower(*text) == 'q' && text[1] == ':') {
	    ban_flags = BAN_BQUIET;
	    pattern_start = 2;
	}
	else if (irc_tolower(*text) == 'm')
	    ban_flags = BAN_MASK;
	else if (irc_tolower(*text) == 'r' && text[1] == ':') {
	    ban_flags = BAN_REQUIRE;
	    pattern_start = 2;
	}
        else if (irc_tolower(*text) == 'g' && text[1] == ':') {
            ban_flags = BAN_GECOS;
            pattern_start = 2;
        }
        else if (((pattern_start = index_left_part(text, "RR:")) > 0)
	          && !IsRegNick(cptr)) {
            ban_flags = BAN_REGONLY;
        }
        else if (((pattern_start = index_left_part(text, "RV:")) > 0)
	          && !IsVerNick(cptr)) {
	    ban_flags = BAN_VERONLY;
        }	
        else return 0;


	/* Ban flags +q,  NV, and NR  match nick!user@host  like any other ban */
	if ((ban_flags == BAN_BQUIET || ban_flags == BAN_VERONLY || ban_flags == BAN_REGONLY)
	      && pattern_start) {
	    if (  text[pattern_start] != '\0' &&
                  (match(text+pattern_start, nuh)) &&
                  (!nuhmask || match(text+pattern_start, nuhmask)) &&
                  (!sip || match(text+pattern_start, sip))
               )
               return 0;
	}

	if (ban_flags == BAN_REQUIRE && pattern_start) {
	    aChannel *ctmp = find_channel((char *)(text+pattern_start), (aChannel *)0);
	    Link *lp;

	    lp = ctmp ? find_user_link(ctmp->members, cptr) : (Link *)0;

	    //if ((lp && !negate_rule) || (!lp && negate_rule))
	    // <--> (lp XOR negate_rule)
	    if ((lp == 0) != (negate_rule == 0))
		return 0;
	}

	if (ban_flags == BAN_GECOS && pattern_start) {
		if (match(text+pattern_start, cptr->info))
			return 0;
	}

	if (ban_flags == BAN_MASK) {
	    if (IsMasked(cptr))
	        ban_flags |= BAN_STD;
	    else
	        return 0;
	}

	if (result)
	    *result = ban_flags;
	return 1;
}

/*
 * is_banned - returns a pointer to the ban structure if banned else NULL
 */
extern	Link	*is_banned(cptr, chptr, bantype)
aClient *cptr;
aChannel *chptr;
int *bantype;
{
	Link	*tmp, *tmpp;
	char    nuh[NICKLEN+USERLEN+HOSTLEN+7];
	char    nuhmask[NICKLEN+USERLEN+HOSTLEN+7];
	char	*s_ip = NULL;
	int	typetmp = 0, garbageint;

	if (!bantype)
	    bantype = &garbageint;
        
	if (!IsPerson(cptr))
		return NULL;
	*bantype = 0;
	tmpp = NULL;

	bmake_nick_user_host(nuh, cptr->name, cptr->user->username,
			     cptr->user->host);
	bmake_nick_user_host(nuhmask, cptr->name, cptr->user->username,
			     genHostMask(cptr->user->host));

	for (tmp = chptr->banlist; tmp; tmp = tmp->next)
        {
		if (*tmp->value.ban.banstr == '%')
		{
			int is_an_ipmask = (MyClient(cptr) && 
				IsIpMask(tmp->value.ban.banstr+1)) ? 1 : 0;

			if ( !is_an_ipmask ) {
				if (match(tmp->value.ban.banstr, nuh) == 0)
				    break;
			}
			else if ((s_ip = make_nick_user_ip(cptr)) &&
				 !match(tmp->value.ban.banstr+1, s_ip )) {
			   break;
			}
			continue;
		}

		if (match(tmp->value.ban.banstr, nuhmask) == 0)
			break;

		/* important! ordinary bans take precedence */
		if (*tmp->value.ban.banstr == '$' && BanRuleMatch(tmp->value.ban.banstr, cptr, &typetmp, nuh, nuhmask, s_ip)) {
		    *bantype |= typetmp;
		    tmpp = tmp;
		}
        }
	if (tmp)
	    *bantype |= BAN_STD;

	return (tmp ? tmp : tmpp);
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
	Link	**curr, *tmp;

	/* Remove the user from the channel */
	for (curr = &chptr->members; (tmp = *curr); curr = &tmp->next)
	  {
		if (tmp->value.cptr == sptr)
		  {
			*curr = tmp->next;
			free_link(tmp);
			sub1_from_channel(chptr);
			break;
		  }
	  }

	/* Remove the channel from the user's channel list */
	for (curr = &sptr->user->channel; (tmp = *curr); curr = &tmp->next)
	  {
		if (tmp->value.chptr == chptr)
		  {
			*curr = tmp->next;
			free_link(tmp);
			sptr->user->joined--;
			break;
		  }
	  }
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
		if ((lp = find_user_link(chptr->members, cptr)))
			return (lp->flags & CHFL_CHANOP);

	return 0;
}

int	has_voice(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Link	*lp;

	if (chptr)
		if ((lp = find_user_link(chptr->members, cptr)))
			return (lp->flags & CHFL_VOICE);

	return 0;
}

int	can_send(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	Link	*lp;
	int	member;

	lp = find_user_link(chptr->members, cptr);
	member = lp ? 1 : 0;

	if (chptr->mode.mode & MODE_MODERATED &&
	    (!lp || !(lp->flags & (CHFL_CHANOP|CHFL_VOICE))))
			return (MODE_MODERATED);

	if (chptr->mode.mode & MODE_NOPRIVMSGS && !member)
		return (MODE_NOPRIVMSGS);

	if (!lp) {
		int bantype_tmp;

		if (is_banned(cptr, chptr, &bantype_tmp))
			return MODE_BAN;
		return 0;
        }

	if (MyClient(cptr))
	{
		if (lp->flags & (CHFL_CHANOP|CHFL_VOICE))
			return 0;
		if ((lp->flags & CHFL_BQUIET))
			return (MODE_BAN);
	}

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
	if (chptr->mode.mode & MODE_SHOWHOST)
		*mbuf++ = 'H';
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
	if (chptr->mode.mode & MODE_NOCOLORS)
		*mbuf++ = 'c';

	if (chptr->mode.limit)
	    {
		*mbuf++ = 'l';
		if (IsMember(cptr, chptr) || IsServer(cptr) || IsULine(cptr,cptr))
			(void)sprintf(pbuf, "%d ", chptr->mode.limit);
	    }
	if (*chptr->mode.key)
	    {
		Link *lp;
		*mbuf++ = 'k';

		lp = find_user_link(chptr->members, cptr);
		if ((lp && (lp->flags & CHFL_CHANOP)) || IsServer(cptr) || IsULine(cptr, cptr))
			(void)strcat(pbuf, chptr->mode.key);
		else
			(void)strcat(pbuf, "*");
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
int
m_opmode(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int     parc;
char	*parv[];
{
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
      ClientFlags(sptr) &=~FLAGS_TS8;
      clean_channelname(parv[1]);
      if (check_channelmask(sptr, cptr, parv[1]))
           return 0;
      sendts = set_mode(cptr, sptr, chptr, parc - 2, parv + 2,
               modebuf, parabuf, &badop, NULL, 1);
      if (BadPtr(modebuf))
          return 0;
      sendto_realops("%s[%s]!%s@%s forced mode change of channel %s: \2%s %s\2",
                  sptr->name, MyClient(sptr) ? me.name :
                  sptr->user->server ? sptr->user->server : "",
                  sptr->user->username, sptr->user->host, 
                  chptr->chname, modebuf, parabuf);
      tolog(LOG_OPER, "%s[%s]!%s@%s forced mode change of channel %s: %s %s",
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
			if (strchr(name, ' '))
				count = 6;
			else
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
	if (*chptr->chname == '&')
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

int
m_mode(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	static char tmp[MODEBUFLEN];
	int badop, sendts, keyslot = -1, i, j, k;
	aChannel *chptr;

	if (check_registered(sptr))
		return 0;

	/* Now, try to find the channel in question */
	if (parc > 1) {
		chptr = find_channel(parv[1], NullChn);
		if (chptr == NullChn)
			return m_umode(cptr, sptr, parc, parv);
	} else {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "MODE");
 	 	return 0;
	}

	ClientFlags(sptr) &=~FLAGS_TS8;

	clean_channelname(parv[1]);
	if (check_channelmask(sptr, cptr, parv[1]))
		return 0;

	if (parc < 3) {
		*modebuf = *parabuf = '\0';
		modebuf[1] = '\0';
		channel_modes(sptr, modebuf, parabuf, chptr);
		sendto_one(sptr, rpl_str(RPL_CHANNELMODEIS), me.name, parv[0],
			   chptr->chname, modebuf, parabuf);
                if (chptr->mode.mlock_on || chptr->mode.mlock_off) {
			mlock_buf(chptr, mlockbuf);
 			sendto_one(sptr, rpl_str(RPL_CHANNELMLOCKIS),
				   me.name, parv[0],
				   chptr->chname, mlockbuf);
		}
		sendto_one(sptr, rpl_str(RPL_CREATIONTIME), me.name, parv[0],
			   chptr->chname, chptr->creationtime);
		return 0;
	}

	if (!(sendts = set_mode(cptr, sptr, chptr, parc - 2, parv + 2,
				modebuf, parabuf, &badop, &keyslot, 0))
	    && !IsULine(cptr,sptr)) {
		sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
			   me.name, parv[0], chptr->chname);
		return 0;
	}

	if ((badop>=2) && !IsULine(cptr,sptr)) {
		int i=3;
		*tmp='\0';
		while (i < parc) {
			strcat(tmp, " ");
			strcat(tmp, parv[i++]);
		}
	}

	if (strlen(modebuf) > (size_t)1 || sendts > 0) {
		if (badop!=2 && strlen(modebuf) > (size_t)1) {
			/*
			 * BROKEN ? Possibly makes a bad assumption (+/-k)
			 * must always be by itself
			 */
			if (!strchr(modebuf, 'k') || keyslot == -1)
				sendto_channel_butserv(chptr, sptr,
						       ":%s MODE %s %s %s",
						       parv[0], chptr->chname,
						       modebuf, parabuf);
			else {
				parabufK[0] = '\0';
	      
				for(i = j = k = 0; parabuf[i]; i++) {
					if (parabuf[i] == ' ' || i == 0) {
						if (i != 0)
							parabufK[k++] = ' ';

						if ((++j) == keyslot)
							parabufK[k++] = '*';
						while(parabuf[i] == ' '
						      && parabuf[i+1] == ' ')
							++i;
						continue;
					}

					if (j != keyslot) 
						parabufK[k++] = parabuf[i];
				}
				parabufK[k++] = '\0';
	      
				sendto_channel_butserv(chptr, sptr,
						       ":%s MODE %s %s %s",
						       parv[0], chptr->chname,
						       modebuf, parabufK);
			}
		}

		/* We send a creationtime of 0, to mark it as a hack --Run */
		if ((IsServer(sptr) && (badop==2 || sendts > 0)) ||
		    IsULine(cptr,sptr)) {
			if (*modebuf == '\0')
				strcpy(modebuf,"+");
			if (badop==2)
				badop = 2;
			else
				sendto_match_servs(chptr, cptr,
						   ":%s MODE %s %s %s %lu",
						   parv[0], chptr->chname,
						   modebuf, parabuf,
						   (badop == 4) ? (time_t)0 : chptr->creationtime);
		} else
			sendto_match_servs(chptr, cptr, ":%s MODE %s %s %s",
					   parv[0], chptr->chname, modebuf,
					   parabuf);
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

char *pretty_mask(mask, limit_rules)
char *mask;
int limit_rules;
{
  char *cp, *lead;
  char *user;
  char *host;

  if (mask && *mask == '$' && mask[1] != '\0') {
      if (!limit_rules)
          return mask;
      lead = mask+1;

      if (*lead == '!' && lead[1] != '\0')
	  lead++;

      switch(*lead) {
	      case 'R': case 'r': case 'g':
	      case 'q': case 'm':
		      return mask;
	      default:;
      }
      
      mask[1] = '?';
      return mask;
  }

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

int get_mode_chfl(char p)
{
	switch(p)
	{
		case 'o':  return MODE_CHANOP;
		case 'v':  return MODE_VOICE;
		default:   return 0;
	}
}

int get_chanop_letters(int lp_flags, char *buf)
{
	char *orig = buf;

	if (lp_flags & CHFL_CHANOP)
		*(buf++) = 'o';
	if (lp_flags & CHFL_VOICE)
		*(buf++) = 'v';
/*	if (lp_flags & CHFL_BQUIET)
		*(buf++) = 'q';*/
	*(buf++) = '\0';

	return ((buf - orig) - 1);
}


/*
 * Check and try to apply the channel modes passed in the parv array for
 * the client ccptr to channel chptr.  The resultant changes are printed
 * into mbuf and pbuf (if any) and applied to the channel.
 */
static	int	set_mode(cptr, sptr, chptr, parc, parv, mbuf, pbuf, badop, keyslot, modehack)
aClient *cptr, *sptr;
aChannel *chptr;
int	parc, *badop;
char	*parv[], *mbuf, *pbuf;
int	*keyslot;
int	modehack;
{
	static	Link	chops[MAXMODEPARAMS];
	Link	*lp;
	char	*curr = parv[0], *cp = NULL;
	int	*ip;
	Link    *member, *tmp = NULL;
	u_int	whatt = MODE_ADD, bwhatt = 0;
	int	limitset = 0, chasing = 0, bounce;
	int	nusers = 0, new, len, blen, keychange = 0, opcnt = 0, banlsent = 0;
	int     doesdeop = 0, doesop = 0, hacknotice = 0, change, gotts = 0;
	int     cis_member = 0, cis_chop = 0; 
	int	orig_parc = parc;
	aClient *who;
	Mode	*mode, oldm;
	static  char bmodebuf[MODEBUFLEN], bparambuf[MODEBUFLEN], numeric[16];
        char    *bmbuf = bmodebuf, *bpbuf = bparambuf;
	time_t	newtime = (time_t)0;
	aConfItem *aconf;

	if (keyslot)
		*keyslot = -1;

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
#if 1
	cis_member = IsMember(sptr, chptr);
	cis_chop = (cis_member == 0) ? 0 : is_chan_op(sptr, chptr);

	if ((IsServer(cptr) == 0 && IsULine(cptr,sptr) == 0 
	     && parc == 1 
	     && modehack == 0
	     && curr == parv[0]
	     && (  
		(*curr == 'b' && curr[1] == '\0') || 
		((*curr == '+' || *curr == '-') && curr[1] == 'b' 
		    && curr[2] == '\0')
	     ) 
	     && cis_member != 0 && cis_chop == 0))
		{
#if 0
		/* XXX: This was and is dead code, --parv is not == 0*/
		    if (--parv == 0) /* XXX MLG Was <= */
			{
			/* NOTREACHED */
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
#endif
		}
	else if (cis_chop == 0 && IsServer(cptr) == 0 &&
		 IsULine(cptr, sptr) == 0 && modehack == 0)
		return 0;
#endif

	new = mode->mode;

        if (MyClient(sptr) && !IsULine(cptr, sptr) && !modehack)
            (void)legalize_mode(chptr, &curr);

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
				lp->flags = get_mode_chfl(*curr);
				lp->flags |= MODE_ADD;
			    }
			else if (whatt == MODE_DEL)
			    {
				lp = &chops[opcnt++];
				lp->value.cptr = who;
				doesdeop = 1; /* Also when -v */
				lp->flags = get_mode_chfl(*curr);
				lp->flags |= MODE_DEL;
			    }
			if (*curr == 'o')
			  doesop=1;
			break;
		case 'k':
			if (--parc <= 0)
				break;
			if (keyslot && (*keyslot == -1))
				*keyslot = orig_parc - parc;
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
					new &= ~MODE_SHOWHOST;
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
				while ((lp = chptr->invites))
					del_invite(lp->value.cptr, chptr);
		default:
			for (ip = chan_flags; *ip; ip += 2)
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

					if ((new & MODE_KEY) || chptr->mode.key[0] != '\0')
						new &= ~MODE_SHOWHOST;
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
	for (ip = chan_flags; *ip; ip += 2)
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

	for (ip = chan_flags; *ip; ip += 2)
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
		char	c = 0;
		u_int prev_whatt = 0;

		for (; i < opcnt; i++)
		    {
			lp = &chops[i];
			/*
			 * make sure we have correct mode change sign
			 */
			if (whatt != (lp->flags & (MODE_ADD|MODE_DEL))) {
				if (lp->flags & MODE_ADD) {
					*mbuf++ = '+';
					prev_whatt = whatt;
					whatt = MODE_ADD;
				} else {
					*mbuf++ = '-';
					prev_whatt = whatt;
					whatt = MODE_DEL;
				}
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
				cp = pretty_mask(lp->value.cp, (MyClient(sptr)
						 && !MyOper(sptr)) ? 1 : 0);
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
				      blen ++;
				    }
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
					    (((whatt & MODE_ADD) && !add_banid(sptr, chptr, cp))
					     || ((whatt & MODE_DEL) && !del_banid(chptr, cp)))) { 
						*mbuf++ = c;
						(void)strcat(pbuf, cp);
						len += strlen(cp);
						(void)strcat(pbuf, " ");
						len++;
					}
				} else {
					if (whatt & MODE_ADD) {
						if (!find_banid(chptr, cp)) {
							if (bwhatt != MODE_DEL)
								*bmbuf++ = '-';
							*bmbuf++ = c;
							strcat(bpbuf, cp);
							blen += strlen(cp);
							strcat(bpbuf, " ");
							blen++;
						}
					} else {
				  if (find_banid(chptr, cp)) {
				  if (bwhatt != MODE_ADD)
					*bmbuf++ = '+';
				  *bmbuf++ = c;
				  strcat(bpbuf, cp);
				  blen += strlen(cp);
				  strcat(bpbuf, " ");
				  blen++;
				  }
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
	int invited = 0, bantype = 0;


       for (lp = sptr->user->invited; lp; lp = lp->next)
               if (lp->value.chptr == chptr) {
                       invited = 1;
                       break;
               }

        if (invited || IsULine(sptr, sptr))
            return 0;

	if (chptr->mode.mode & MODE_INVITEONLY)
	        return (ERR_INVITEONLYCHAN);

	if (*chptr->mode.key && (BadPtr(key) || mycmp(chptr->mode.key, key)))
		return (ERR_BADCHANNELKEY);

	if (IsSet(ClientUmode(sptr), U_FULLMASK) &&
	    IsSet(chptr->mode.mode, MODE_SHOWHOST) && !IsAnOper(sptr))
	    return ERR_NOMASKCHAN;

	if (is_banned(sptr, chptr, &bantype)) {
           if (IsSet(bantype, BAN_BLOCK | BAN_GECOS))
               return (ERR_BANNEDFROMCHAN);
           if (IsSet(bantype, BAN_RBLOCK | BAN_MASK))
               return (ERR_BANRULE);
           if (IsSet(bantype, BAN_REQUIRE))
               return (ERR_BANREQUIRE);
           if (IsSet(bantype, BAN_VERONLY))
	       return (ERR_NEEDVERNICK);	   
	   if (IsSet(bantype, BAN_REGONLY))
	       return (ERR_NEEDREGGEDNICK);
	}

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
		if (*cn == '\007' || *cn == ' ' || *cn == ',' ||
		    ((unsigned char)*cn) == 160 || *cn == '\033' ||
		    *cn == '\t' || *cn == '\003'  || *cn == '\017' ||
		    *cn == '\026' || *cn == '\037' || *cn == '\002' ||
		    *cn == '\001' || *cn == '\b')
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
		chptr = irc_malloc(sizeof(aChannel) + len);
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
			irc_free(tmp->value.ban.banstr);
			irc_free(tmp->value.ban.who);
			free_link(tmp);
		}
		if (chptr->prevch)
			chptr->prevch->nextch = chptr->nextch;
		else
			channel = chptr->nextch;
		if (chptr->nextch)
			chptr->nextch->prevch = chptr->prevch;
		(void)del_from_channel_hash_table(chptr->chname, chptr);
		irc_free(chptr);
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
	int	i, flags = 0;
	char	*p = NULL, *p2 = NULL;

	if (check_registered_user(sptr))
		return 0;

#if defined(NOSPOOF) && defined(REQ_VERSION_RESPONSE)
	if (MyClient(sptr) && !IsUserVersionKnown(sptr) && parc >= 2) {
			sendto_one(sptr,
				":%s %d %s %s :Sorry, cannot join channel. (Client hasn't responded to version check, try typing /join %s  again in a moment)",
                                   me.name, ERR_CHANNELISFULL, parv[0], parv[1], parv[1]);
			return 0;
	}
#endif

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "JOIN");
		return 0;
	    }

	if (IsMasked(sptr))
		SET_BIT(ClientUmode(sptr), U_FULLMASK);
	*jbuf = '\0';

	/*
	** Rebuild list of channels joined to be the actual result of the
	** JOIN.  Note that "JOIN 0" is the destructive problem.
	*/
	for (i = 0, name = strtok_r(parv[1], ",", &p); name;
	     name = strtok_r(NULL, ",", &p))
	    {
		clean_channelname(name);
		if (check_channelmask(sptr, cptr, name)==-1)
			continue;
		if (*name == '&' && !MyConnect(sptr))
			continue;

		if (MyClient(sptr) && (parc >= 2))
		{
			/* XXX: name != parv[1], double-check that */
			if (*name == '0' && !MyOper(sptr) && (name != parv[1] || (*(name + 1) && *(name + 1) != ',')))
			{
				*jbuf = '\0';
				i = 0;
				continue;
			}
		}


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
		key = strtok_r(parv[2], ",", &p2);
	parv[2] = NULL;	/* for m_names call later, parv[parc] must == NULL */
	for (name = strtok_r(jbuf, ",", &p); name;
	     key = (key) ? strtok_r(NULL, ",", &p2) : NULL,
	     name = strtok_r(NULL, ",", &p))
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
			if (!IsModelessChannel(name) && (OPCanHelpOp(sptr) || !IsSystemChannel(name)))
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
			/* if (MyConnect(sptr) && !IsAnOper(sptr) && key
			    && *key && (!myncmp(key, "unmask",6))) */
		    }
		    
		    if (!IsAnOper(sptr) && key && *key && (!myncmp(key, "unmask",6))) {
			if (key[6] == ':' || key[6] == '\0')
			{
				REMOVE_BIT(ClientUmode(sptr), U_FULLMASK);
				if (key[6])
					key += 7;
			}
		    }
		chptr = get_channel(sptr, name, CREATE);
                if (chptr && (lp=find_user_link(chptr->members, sptr)))
			continue;

		if (!MyConnect(sptr))
			flags = CHFL_DEOPPED;

		if (ClientFlags(sptr) & FLAGS_TS8)
			flags|=CHFL_SERVOPOK;

		if (!chptr ||
		    (MyConnect(sptr) && (i = can_join(sptr, chptr, key))))
		    {
			switch(i) {
			default:
			/*sendto_one(sptr,
				   ":%s %d %s %s :Sorry, cannot join channel.",
				   me.name, i, parv[0], name);
                        break;*/
			case ERR_NOMASKCHAN:
			case ERR_BANRULE:
			case ERR_BANREQUIRE:
			sendto_one(sptr, err_str(i),
				      me.name, parv[0], name, name);
                        break;
			}
			continue;
		    }

		/*
		**  Complete user entry to the new channel (if any)
		*/
		add_user_to_channel(chptr, sptr, flags);

		if (chptr && MyClient(sptr))
			check_bquiet(chptr, sptr, 1);

		/*
		** notify all other users on the new channel
		*/
		if (chptr)
		{
			if (IsSet(ClientUmode(sptr), U_FULLMASK))
				sendto_channel_butserv(chptr, sptr, ":%s JOIN :%s", parv[0], name);
			else
				sendto_channel_butserv_unmask(chptr, sptr, ":%s JOIN :%s", parv[0], name);
                }

		if (!IsMasked(sptr) || IsSet(ClientUmode(sptr), U_FULLMASK))
			sendto_match_servs(chptr, cptr, ":%s JOIN :%s", parv[0], name);
		else
                        sendto_match_servs(chptr, cptr, ":%s JOIN %s unmask", parv[0], name);

		if (IsMasked(sptr)) {
			SET_BIT(ClientUmode(sptr), U_FULLMASK);
		}


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
	char	*p = NULL, *stripped = NULL, *name;
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

	for (; (name = strtok_r(parv[1], ",", &p)); parv[1] = NULL)
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

		if (!IsMember(sptr, chptr))
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
		 *  Remove user from the old channel (if any)
		 */

		/* ...without a parting comment */
                if (parc < 3)
		  {
			sendto_match_servs(chptr, cptr, PartFmt, parv[0],
					   name);
			sendto_channel_butserv(chptr, sptr, PartFmt, parv[0],
					       name);
		  }

		/* ...with a parting comment */
		else
		  {
			sendto_match_servs(chptr, cptr, PartFmt2, parv[0],
					   name, comment);

			if (!stripped && chptr->mode.mode & MODE_NOCOLORS &&
			    msg_has_colors(comment))
				stripped = strip_colors(comment);

			sendto_channel_butserv(chptr, sptr, PartFmt2, parv[0],
					       name, (stripped) ? stripped :
					       comment);
		  }

		remove_user_from_channel(sptr, chptr);
	    }

	irc_free(stripped);
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
	int	chasing = 0, i, num;
	char	*comment, *name, *p = NULL, *user, *p2 = NULL;
	char	buf2[BUFSIZE];
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

	for (; (name = strtok_r(parv[1], ",", &p)); parv[1] = NULL)
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
		for (; (user = strtok_r(parv[2], ",", &p2)); parv[2] = NULL)
		    {   if (!(who = find_chasing(sptr, user, &chasing)))
				continue; /* No such user left! */
			if (((lp = find_user_link(chptr->members, who))) ||
			    IsServer(sptr))
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
				num = get_chanop_letters(lp->flags, temp);

				*buf2 = '\0';

				for(i = 0; i < num; i++) {
					strcat(buf2, who->name);
					if (i+1 < num)
						strcat(buf2, " ");
				}

				/*
				 * \XXX: depends on a tolerable NICKLEN
				 * that's not good...
				 */
				sendto_one(cptr, ":%s MODE %s %s %s %lu",
					me.name, chptr->chname, buf, buf2,
					chptr->creationtime);
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

	for (; (name = strtok_r(parv[1], ",", &p)); parv[1] = NULL)
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
				set_topic(chptr, topic, 0);
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
		else if (((chptr->mode.mode & MODE_TOPICLIMIT) == 0
			  || is_chan_op(sptr, chptr))
			 || (IsULine(cptr,sptr) && topic)) {
			/* setting a topic */
			set_topic(chptr, topic, 1);
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
** set_topic
**	chptr - ptr. to target channel
**	topic - topic to set
**	check - check for colors? (0 - no, anything else - yes)
*/
static void
set_topic(aChannel *chptr, const char *topic, const int check)
{
	char *stripped = NULL;

	if ((chptr->mode.mode & MODE_NOCOLORS) && msg_has_colors(topic) &&
	    check)
		topic = stripped = strip_colors(topic);
		
	strncpyzt(chptr->topic, topic, sizeof(chptr->topic));

	if (check)
	  irc_free(stripped);
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
	if (MyConnect(acptr)) {
		if (chptr && sptr->user && (is_chan_op(sptr, chptr) || IsULine(cptr,sptr)))
			add_invite(acptr, chptr);
                if (!IsULine(cptr, sptr))
                    sendto_channelops_butone(NULL, &me, chptr, ":%s NOTICE @%s :%s invited %s into channel %s.",
                                             me.name, chptr->chname, sptr->name, acptr->name, chptr->chname);
        }

	sendto_prefix_one(acptr, sptr, ":%s INVITE %s :%s",parv[0],
			  acptr->name, ((chptr) ? (chptr->chname) : parv[2]));
	return 0;
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

	char li_flag = 0;
	LOpts  *lopt;
	short  int  showall = 0;
	Link   *yeslist = NULL, *nolist = NULL, *listptr;
	short  usermin = 0, usermax = -1;
	time_t currenttime = time(NULL);
	time_t chantimemin = 0, topictimemin = 0;
	time_t chantimemax, topictimemax;
	char **orig_parv = parv;
	int orig_parc = parc;

	static char *usage[] = {
	    "   Usage: /raw LIST options (on mirc) or /quote LIST options (ircII)",
	    "",
	    "If you don't include any options, the default is to send you the",
	    "entire unfiltered list of channels. Below are the options you can",
	    "use, and what channels LIST will return when you use them.",
	    ">number  List channels with more than <number> people.",
	    "<number  List channels with less than <number> people.",
	    "C>number List channels created between now and <number> minutes ago.",
	    "C<number List channels created earlier than <number> minutes ago.",
	    "T>number List channels whose topics are older than <number> minutes",
	    "         (Ie, they have not changed in the last <number> minutes.",
	    "T<number List channels whose topics are not older than <number> minutes.",
	    "*mask*   List channels that match *mask*",
	    "!*mask*  List channels that do not match *mask*",
	    NULL
	};

	/* None of that unregistered LIST stuff.  -- Barubary */
	if (check_registered(sptr))
		 return 0;
	if (cptr != sptr) {
		sendto_one(sptr, err_str(ERR_NOTREGISTERED), me.name, "*");
		return 0;
	}


	/*
	 * I'm making the assumption it won't take over a day to transmit
	 * the list... -Rak
	 */
	chantimemax = topictimemax = currenttime + 86400;

        if ((parc == 2) && (!strcasecmp(parv[1], "?"))) {
            char **ptr = usage;
    
            for (; *ptr; ptr++)
                 sendto_one(sptr, rpl_str(RPL_COMMANDSYNTAX), me.name,
                            cptr->name, *ptr);
            return 0;
        }


	/*
	 * A list is already in process, for now we just interrupt the
	 * current listing, perhaps later we can allow stacked ones...
	 *  -Donwulff (Not that it's hard or anything, but I don't see
	 *             much use for it, beyond flooding)
	 */

	if(cptr->lopt) {
	   free_str_list(cptr->lopt->yeslist);
	   free_str_list(cptr->lopt->nolist);
	   irc_free(cptr->lopt);
	   cptr->lopt = NULL;
	   sendto_one(sptr, rpl_str(RPL_LISTEND), me.name, cptr->name);

	   /* Interrupted list, penalize 10 seconds */
	   if(!IsPrivileged(sptr))
	      sptr->since+=10;
	   return 0;
	}

	sendto_one(sptr, rpl_str(RPL_LISTSTART), me.name, parv[0]);


	/* LIST with no arguments */
	if (parc < 2 || BadPtr(parv[1]))
	    {
		lopt = irc_malloc(sizeof(LOpts));
		if (!lopt)
			return 0;

		memset(lopt, 0, sizeof(LOpts));

		lopt->next = (LOpts *) NULL;
		lopt->yeslist = lopt->nolist = (Link *) NULL;
		lopt->usermin = 0;
		lopt->usermax = -1;
		lopt->chantimemax = lopt->topictimemax = currenttime + 86400;
		cptr->lopt = lopt;

		if (IsSendable(cptr))
			send_list(cptr, 64);
		return 0;
	    }

    /*
     * General idea: We don't need parv[0], since we can get that
     * information from cptr.name. So, let's parse each element of
     * parv[], setting pointer parv to the element being parsed.
     */
    while (--parc) {
        parv += 1;
        if (BadPtr(parv)) /* Sanity check! */
            continue;

        name = strtok_r(*parv, ",", &p);

        while (name) {
          switch (*name) {
            case '>':
                showall = 1;
                usermin = strtol(++name, (char **) 0, 10);
                break;

            case '<':
                showall = 1;
                usermax = strtol(++name, (char **) 0, 10);
                break;

            case 't':
            case 'T':
                showall = 1;
                switch (*++name) {
                    case '>':
                        topictimemax = currenttime - 60 *
                                       strtol(++name, (char **) 0, 10);
                        break;

                    case '<':
                        topictimemin = currenttime - 60 *
                                       strtol(++name, (char **) 0, 10);
                        break;

                    case '\0':
                        topictimemin = 1;
                        break;

                    default:
                        sendto_one(sptr, err_str(ERR_LISTSYNTAX),
                                   me.name, cptr->name);
                        free_str_list(yeslist);
                        free_str_list(nolist);
                        sendto_one(sptr, rpl_str(RPL_LISTEND),
                                   me.name, cptr->name);

                        return 0;
                }
                break;

                case 'c':
                case 'C':
                    showall = 1;
                    switch (*++name) {
                        case '>':
                            chantimemin = currenttime - 60 *
                                          strtol(++name, (char **) 0, 10);
                            break;

                        case '<':
                            chantimemax = currenttime - 60 *
                                          strtol(++name, (char **) 0, 10);
                            break;

                        default:
                            sendto_one(sptr, err_str(ERR_LISTSYNTAX),
                                       me.name, cptr->name);
                            free_str_list(yeslist);
                            free_str_list(nolist);
                            sendto_one(sptr, rpl_str(RPL_LISTEND),
                                       me.name, cptr->name);
                            return 0;
                    }
                    break;

		case 's':
			if (0 && IsOper(sptr) && MyOper(sptr) && OPCanModeHack(sptr)
                            && OPCanGlobOps(sptr))
				li_flag |= 1;
			break;

                default: /* A channel or channel mask */

                    /*
                     * new syntax: !channelmask will tell ircd to ignore
                     * any channels matching that mask, and then
                     * channelmask will tell ircd to send us a list of
                     * channels only masking channelmask. Note: Specifying
                     * a channel without wildcards will return that
                     * channel even if any of the !channelmask masks
                     * matches it.
                     */

                    if (*name == '!') {
                        showall = 1;
                        listptr = make_link();
                        listptr->next = nolist;
                        listptr->value.cp = irc_strdup(name+1);
                        nolist = listptr;
                    }
                    else if (strchr(name, '*') || strchr(name, '?')) {
                        showall = 1;
                        listptr = make_link();
                        listptr->next = yeslist;
                        listptr->value.cp = irc_strdup(name);
                        yeslist = listptr;
                    }
                    else {
                        chptr = find_channel(name, NullChn);
                        if (chptr && ShowChannel(sptr, chptr))
                            sendto_one(sptr, rpl_str(RPL_LIST),
                                       me.name, cptr->name,
                                       ShowChannel(sptr,chptr) ? name : "*",
                                       chptr->users,
                                       chptr->topic);
                    }
          } /* switch (*name) */
        name = strtok_r(NULL, ",", &p);
        } /* while(name) */
    } /* while(--parc) */

    if (!showall || (chantimemin > currenttime) ||
         (topictimemin > currenttime)) {
        free_str_list(yeslist);
        free_str_list(nolist);
        sendto_one(sptr, rpl_str(RPL_LISTEND), me.name, cptr->name);

        return 0;
    }

    lopt = irc_malloc(sizeof(LOpts));

    lopt->showall = 0;
    lopt->next = NULL;
    lopt->yeslist = yeslist;
    lopt->nolist = nolist;
    lopt->starthash = 0;
    lopt->usermin = usermin;
    lopt->usermax = usermax;
    lopt->currenttime = currenttime;
    lopt->chantimemin = chantimemin;
    lopt->chantimemax = chantimemax;
    lopt->topictimemin = topictimemin;
    lopt->topictimemax = topictimemax;
    lopt->flag = li_flag;
    cptr->lopt = lopt;

    if (IsPerson(cptr) && li_flag) {
        static char buf[2048];
        int i = 0, count = 0;

        *buf = '\0';
        for(i = 1; i < orig_parc; i++) {
            count += snprintf(buf + count, 80, "%s", orig_parv[i]);
            if ((count + 512) >= 767)
                break;
            if (i+1 < orig_parc) {
                 strcat(buf+count, " ");
                 count++;
            }
        }

        /* Any listing of +s channels is subject to auditing/logging by */
        /* all servers -Mysid */

        sendto_serv_butone(&me, ":%s LOG :%s!%s@%s Listing invisible channels (%s)",
              me.name, sptr->name, sptr->user->username, sptr->user->host, buf);
        tolog(LOG_NET, "%s[%s]!%s@%s Listing invisible channels (%s)", sptr->name,
              MyClient(sptr) ? me.name : sptr->user->server,
              sptr->user->username, sptr->user->host, buf);
    }
    else lopt->flag &= ~(0x1);

    send_list(cptr, 64);
    return 0;
}



/*
 * send_list
 *
 * The function which sends
 * The function which sends the actual /list output back to the user.
 * Operates by stepping through the hashtable, sending the entries back if
 * they match the criteria.
 * cptr = Local client to send the output back to.
 * numsend = Number (roughly) of lines to send back. Once this number has
 * been exceeded, send_list will finish with the current hash bucket,
 * and record that number as the number to start next time send_list
 * is called for this user. So, this function will almost always send
 * back more lines than specified by numsend (though not by much,
 * assuming CHANNELHASHSIZE is was well picked). So be conservative
 * if altering numsend };> -Rak
 */
void    send_list(cptr, numsend)
aClient *cptr;
int     numsend;
{
	int hashptr;
	aChannel    *chptr;
	int cis_member = 0;
	char *stripped = NULL;

#define l cptr->lopt /* lazy shortcut */

    for (hashptr = l->starthash; hashptr < CHANNELHASHSIZE; hashptr++)
      {
        if (numsend > 0)
        for (chptr = (aChannel *)hash_get_chan_bucket(hashptr);
             chptr; chptr = chptr->hnextch) {
            cis_member = 0;

            if (!cptr->user || (SecretChannel(chptr) && !(l->flag & 0x1) && !(cis_member = IsMember(cptr, chptr))))
                continue;
            if (!l->showall && ((chptr->users <= l->usermin) ||
                ((l->usermax == -1)?0:(chptr->users >= l->usermax)) ||
                ((chptr->creationtime||1) <= l->chantimemin) ||
                (chptr->topic_time < l->topictimemin) ||
                (chptr->creationtime >= l->chantimemax) ||
                (chptr->topic_time > l->topictimemax)))
                continue;
            /* For now, just extend to topics as well. Use patterns starting
             * with # to stick to searching channel names only. -Donwulff
             */

            if (MyOper(cptr)) {
                if (l->nolist &&
                    (find_str_match_link(&(l->nolist), chptr->chname) ||
                     find_str_match_link(&(l->nolist), chptr->topic)))
                     continue;

                if (l->yeslist &&
                    (!find_str_match_link(&(l->yeslist), chptr->chname) &&
                     !find_str_match_link(&(l->yeslist), chptr->topic)))
                    continue;
            } else {
                if (l->nolist && (find_str_match_link(&(l->nolist), chptr->chname)))
                    continue;
                if (l->yeslist && (!find_str_match_link(&(l->yeslist), chptr->chname)))
                    continue;
            }

            if (!cis_member && ((l->flag & 0x1) || ShowChannel(cptr, chptr)))
                cis_member = 1;

	    if (msg_has_colors(chptr->topic))
		stripped = strip_colors(chptr->topic);

	    sendto_one(cptr, rpl_str(RPL_LIST), me.name, cptr->name,
		       cis_member ? chptr->chname : "*",
		       chptr->users,
		       cis_member ? ((stripped) ? stripped : chptr->topic) :
		       "");

	    irc_free(stripped);
	    stripped = NULL;
        }

        if ((numsend < 1) && (++hashptr < CHANNELHASHSIZE)) {
            l->starthash = hashptr;
            return;
        }
    }

    sendto_one(cptr, rpl_str(RPL_LISTEND), me.name, cptr->name);
    free_str_list(l->yeslist);
    free_str_list(l->nolist);
    irc_free(l);
    l = NULL;

    /* List finished, penalize by 10 seconds -Donwulff */
    if (!IsPrivileged(cptr))
        cptr->since+=10;
#undef l

    return;
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
		        if (lp->flags & CHFL_CHANOP)
				(void)strcat(buf, "@");
			else if (lp->flags & CHFL_VOICE)
				(void)strcat(buf, "+");
			(void)strncat(buf, c2ptr->name, NICKLEN);
			idx += strlen(c2ptr->name) + 2;
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

		if (!IsPerson(c2ptr) || (sptr!=c2ptr && IsInvisible(c2ptr)))
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
		idx += strlen(c2ptr->name) + 2;
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

/*
** m_mlock
**	parv[0] = sender prefix
**	parv[1] = channel
**	parv[2] = channel modes
**	parv[3] = timestamp
*/
int
m_mlock(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	char *modes = (parc > 2) ? parv[2] : NULL;
	aChannel *chptr;
	char buf[1024];
	int i = 0, j = 0;
	int what = 1;

	if (parc < 3) {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "MLOCK");
		return 0;
	}
	if (!(chptr = find_channel(parv[1], NULL))) {
		sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL),
			   me.name, parv[0], parv[1]);
		return 0;
	}

	/*
	 * For now at least, opers can't do /mlock on global chans except 
	 * for test purposes
	 */
#ifdef IRCD_TEST
	if (MyClient(sptr) && (!IsOper(sptr) || !OPCanModeHack(sptr)))
#else
	if (MyClient(sptr) && !IsULine(cptr, sptr) && 
	    (!IsOper(sptr) || !OPCanModeHack(sptr) || *parv[1] != '&') )
#endif
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}

	chptr->mode.mlock_on = 0;
	chptr->mode.mlock_off = 0;

	for (i = 0 ; modes[i]; i++) {
		switch(modes[i]) {
		case '+':
			what = 1;
			break;
		case '*':
			what = 0;
			break;
		case '-':
			what = -1;
			break;
		default:
			if (!isalpha(modes[i]))
				break;
			for (j = 0; chan_flags[j]; j += 2)
				if (modes[i] == (char)chan_flags[j+1])
					break;
			if (chan_flags[j]) {
				REMOVE_BIT(chptr->mode.mlock_on, chan_flags[j]);
				REMOVE_BIT(chptr->mode.mlock_off, chan_flags[j]);
				if (what > 0)
					SET_BIT(chptr->mode.mlock_on, chan_flags[j]);
				else if (what < 0)
					SET_BIT(chptr->mode.mlock_off, chan_flags[j]);
			}
		}
	}

	mlock_buf(chptr, buf);
	sendto_match_servs(chptr, cptr, ":%s MLOCK %s %s :%s",
			   parv[0], parv[1], buf, parc > 3 ? parv[3] : "");
	/* send out op messages if it wasn't done by a ulined client */
	if (!IsULine(cptr, sptr) && (!IsServer(sptr) || IsPerson(sptr))) { 
		if (!IsPerson(sptr) || !sptr->user)
			tolog(LOG_NET, "%s set ircd mlock for %s to %s",
			      parv[0], parv[1], buf);
		else
			tolog(LOG_NET,
			      "%s[%s]!%s@%s set ircd mlock for %s to %s", 
			      sptr->name,  MyClient(sptr) ? me.name : sptr->user->server ? sptr->user->server : "<null>",
			      sptr->user->username, sptr->user->host, parv[1], buf);
		if (MyClient(sptr))
			sendto_one(sptr, ":%s NOTICE %s :Enforced modes for %s are now: \2%s\2.", me.name, parv[0], parv[1], buf);
	}
	return 0;
}

/* fill a buffer with a channel's modelock information */
void mlock_buf(aChannel *chptr, char *buf)
{
   int i = 0, j = 0, x = 0;

   x = 0;
   buf[0] = '\0';
   for (j = 0, i = 0; chan_flags[j]; j += 2)
        if (IsSet(chptr->mode.mlock_on, chan_flags[j]))
        {
            if (!*buf) buf[x++] = '+';
            buf[x++] = chan_flags[j+1];
        }
   for (j = 0; chan_flags[j]; j += 2)
        if (IsSet(chptr->mode.mlock_off, chan_flags[j]))
        {
            if (!i) {i = 1; buf[x++] = '-';}
            buf[x++] = chan_flags[j+1];
        }
   buf[x++] = '\0';
}

/* make a mode legal under a channel's modelock */
int legalize_mode(aChannel *chptr, char **curr)
{
    static char buf[2048] = "";
    char *s;
    int i = 0, j = 0, x = 0, whatt = 1, bset = 1;
    int retval = 0;
    
    s = *curr;

    for (i = 0 ; s[i]; i++)
    {
        switch(s[i])
        {
           case '+': whatt =  1;  break;
           case '-': whatt = -1;  break;
           case 'o':
                   if ((!IsSet(chptr->mode.mlock_on, MODE_CHANOP)  || whatt > 0) &&
                       (!IsSet(chptr->mode.mlock_off, MODE_CHANOP) || whatt < 0))
                   {                       
                       if (bset != whatt)
                       {
                           if (whatt < 0) buf[x++] = '-';
                           else           buf[x++] = '+';
                           bset = whatt;
                       }
                       buf[x++] = s[i];
                  }
                  else retval = 1;
           break;
           default:
               for (j = 0; chan_flags[j]; j += 2)
                   if (s[i] == (char)chan_flags[j+1])
                       break;
               {
                   if (!chan_flags[j] || ((!IsSet(chptr->mode.mlock_on, chan_flags[j])  || whatt > 0) &&
                       (!IsSet(chptr->mode.mlock_off, chan_flags[j]) || whatt < 0)))
                   {                       
                       if (bset != whatt)
                       {
                           if (whatt < 0) buf[x++] = '-';
                           else           buf[x++] = '+';
                           bset = whatt;
                       }
                       if (chan_flags[j])
                           buf[x++] = chan_flags[j+1];
                       else
                           buf[x++] = s[i];
                   }
                   else retval = 1;
               }
           break;
        }
    }
    buf[x++] = '\0';
    /* *curr = buf; */
    strcpy(*curr, buf);
    return retval;
}

void
sendmodeto_one(aClient *cptr, char *from, char *name, char *mode,
	       char *param, time_t creationtime)
{
	if ((IsServer(cptr) && DoesOp(mode) && creationtime) ||
	    IsULine(cptr, cptr))
		sendto_one(cptr,":%s MODE %s %s %s %lu",
			   from, name, mode, param, creationtime);
	else
		sendto_one(cptr,":%s MODE %s %s %s", from, name, mode, param);
}
