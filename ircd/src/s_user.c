/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_user.c (formerly ircd/s_msg.c)
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers. 
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

#ifndef lint
static  char sccsid[] = "@(#)s_user.c	2.74 2/8/94 (C) 1988 University of Oulu, \
Computing Center and Jarkko Oikarinen";
#endif


#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include "userload.h"
#include <sys/stat.h>
#ifndef _WIN32
#include <utmp.h>
#else
#include <io.h>
#endif
#include <fcntl.h>
#include "md5_iface.h"
#include "h.h"

void	send_umode_out PROTO((aClient*, aClient *, aClient *, int));
void	send_umode PROTO((aClient *, aClient *, aClient *, int, int, char *));
static	is_silenced PROTO((aClient *, aClient *));
int      check_for_ctcp(char *, char **);
int      allow_dcc(aClient *, aClient *);
static int register_user(aClient *cptr, aClient *sptr, char *nick, char *username);
aMachine *hash_find_machine(struct in_addr ip, aMachine *hptr);
aClient *find_server_const(const char *, aClient *);

/* static  Link    *is_banned PROTO((aClient *, aChannel *)); */

void perform_mask(aClient *acptr, int v);
char *tokenEncode(char *str);
char *ntokEncode(char *str, int);
char *umode_str(struct Client *cptr);

/* defines for check_ctcp results */
#define CTCP_NONE       0
#define CTCP_YES        1
#define CTCP_DCC        2
#define CTCP_DCCSEND    3

static char buf[BUFSIZE], buf2[BUFSIZE];

/*
** m_functions execute protocol messages on this server:
**
**	cptr	is always NON-NULL, pointing to a *LOCAL* client
**		structure (with an open socket connected!). This
**		identifies the physical socket where the message
**		originated (or which caused the m_function to be
**		executed--some m_functions may call others...).
**
**	sptr	is the source of the message, defined by the
**		prefix part of the message if present. If not
**		or prefix not found, then sptr==cptr.
**
**		(!IsServer(cptr)) => (cptr == sptr), because
**		prefixes are taken *only* from servers...
**
**		(IsServer(cptr))
**			(sptr == cptr) => the message didn't
**			have the prefix.
**
**			(sptr != cptr && IsServer(sptr) means
**			the prefix specified servername. (?)
**
**			(sptr != cptr && !IsServer(sptr) means
**			that message originated from a remote
**			user (not local).
**
**		combining
**
**		(!IsServer(sptr)) means that, sptr can safely
**		taken as defining the target structure of the
**		message in this server.
**
**	*Always* true (if 'parse' and others are working correct):
**
**	1)	sptr->from == cptr  (note: cptr->from == cptr)
**
**	2)	MyConnect(sptr) <=> sptr == cptr (e.g. sptr
**		*cannot* be a local connection, unless it's
**		actually cptr!). [MyConnect(x) should probably
**		be defined as (x == x->from) --msa ]
**
**	parc	number of variable parameter strings (if zero,
**		parv is allowed to be NULL)
**
**	parv	a NULL terminated list of parameter pointers,
**
**			parv[0], sender (prefix string), if not present
**				this points to an empty string.
**			parv[1]...parv[parc-1]
**				pointers to additional parameters
**			parv[parc] == NULL, *always*
**
**		note:	it is guaranteed that parv[0]..parv[parc-1] are all
**			non-NULL pointers.
*/

/*
** next_client
**	Local function to find the next matching client. The search
**	can be continued from the specified client entry. Normal
**	usage loop is:
**
**	for (x = client; x = next_client(x,mask); x = x->next)
**		HandleMatchingClient;
**	      
** aClient *next = First client to check
** char    *ch   = search string (may include wilds)
*/
aClient *next_client(aClient *next, char *ch)
{
	aClient	*tmp = next;

	next = find_client(ch, tmp);
	if (tmp && tmp->prev == next)
		return NULL;
	if (next != tmp)
		return next;
	for ( ; next; next = next->next)
	{
		if (IsService(next))
			continue;
		if (!match(ch, next->name) || !match(next->name, ch))
			break;
	}
	return next;
}

/*
** hunt_server
**
**	Do the basic thing in delivering the message (command)
**	across the relays to the specific server (server) for
**	actions.
**
**	Note:	The command is a format string and *MUST* be
**		of prefixed style (e.g. ":%s COMMAND %s ...").
**		Command can have only max 8 parameters.
**
**	server	parv[server] is the parameter identifying the
**		target server.
**
**	*WARNING*
**		parv[server] is replaced with the pointer to the
**		real servername from the matched client (I'm lazy
**		now --msa).
**
**	returns: (see #defines)
*/
int hunt_server(aClient *cptr, aClient *sptr, char *command, int server, int parc, char *parv[])
{
	aClient *acptr;

	/*
	** Assume it's me, if no server
	*/
	if (parc <= server || BadPtr(parv[server]) ||
	    match(me.name, parv[server]) == 0 ||
	    match(parv[server], me.name) == 0)
		return (HUNTED_ISME);
	/*
	** These are to pickup matches that would cause the following
	** message to go in the wrong direction while doing quick fast
	** non-matching lookups.
	*/
	if ((acptr = find_client(parv[server], NULL)))
		if (acptr->from == sptr->from && !MyConnect(acptr))
			acptr = NULL;
	if (!acptr && (acptr = find_server(parv[server], NULL)))
		if (acptr->from == sptr->from && !MyConnect(acptr))
			acptr = NULL;
	if (!acptr)
		for (acptr = client, (void)collapse(parv[server]);
		     (acptr = next_client(acptr, parv[server]));
		     acptr = acptr->next)
		    {
			if (acptr->from == sptr->from && !MyConnect(acptr))
				continue;
			/*
			 * Fix to prevent looping in case the parameter for
			 * some reason happens to match someone from the from
			 * link --jto
			 */
			if (IsRegistered(acptr) && (acptr != cptr))
				break;
		    }
	 if (acptr)
	    {
		if (IsMe(acptr) || MyClient(acptr))
			return HUNTED_ISME;
		if (match(acptr->name, parv[server]))
			parv[server] = acptr->name;
		sendto_one(acptr, command, parv[0],
			   parv[1], parv[2], parv[3], parv[4],
			   parv[5], parv[6], parv[7], parv[8]);
		return(HUNTED_PASS);
	    } 
	sendto_one(sptr, err_str(ERR_NOSUCHSERVER), me.name,
		   parv[0], parv[server]);
	return(HUNTED_NOSUCH);
    }

/*
** 'do_nick_name' ensures that the given parameter (nick) is
** really a proper string for a nickname (note, the 'nick'
** may be modified in the process...)
**
**	RETURNS the length of the final NICKNAME (0, if
**	nickname is illegal)
**
**  Nickname characters are in range
**	'A'..'}', '_', '-', '0'..'9'
**  anything outside the above set will terminate nickname.
**  In addition, the first character cannot be '-'
**  or a Digit.
**
**  Note:
**	'~'-character should be allowed, but
**	a change should be global, some confusion would
**	result if only few servers allowed it...
*/

static int do_nick_name(char *nick, int isserv)
{
      char *ch;

      if (!isserv) /* this is so services can use certain chars in nicks */
	if (*nick == '-' || isdigit(*nick)) /* first character in [0..9-] */
		return 0;

	for (ch = nick; *ch && (ch - nick) < NICKLEN; ch++)
		if (!isvalid(*ch) || isspace(*ch))
			break;

	*ch = '\0';

	return (ch - nick);
}


/*
** canonize
**
** reduce a string of duplicate list entries to contain only the unique
** items.  Unavoidably O(n^2).
*/
char *canonize(char *buffer)
{
	static	char	cbuf[BUFSIZ];
	char	*s, *t, *cp = cbuf;
	int	l = 0;
	char	*p = NULL, *p2;

	*cp = '\0';

	for (s = strtoken(&p, buffer, ","); s; s = strtoken(&p, NULL, ","))
	    {
		if (l)
		    {
			for (p2 = NULL, t = strtoken(&p2, cbuf, ","); t;
			     t = strtoken(&p2, NULL, ","))
				if (!mycmp(s, t))
					break;
				else if (p2)
					p2[-1] = ',';
		    }
		else
			t = NULL;
		if (!t)
		    {
			if (l)
				*(cp-1) = ',';
			else
				l = 1;
			(void)strcpy(cp, s);
			if (p)
				cp += (p - s);
		    }
		else if (p2)
			p2[-1] = ',';
	    }
	return cbuf;
}


void introduce_user(aClient *cptr, aClient *sptr)
{
      if (!IS_SET(cptr->cap, CAP_USERIP)) {
          sendto_one(cptr, "NICK %s %d %d %s %s %s :%s", sptr->name,
  		             sptr->hopcount+1, sptr->lastnick, sptr->user->username, sptr->user->host,
		             sptr->user->server, sptr->info);
          send_umode(cptr, sptr, sptr, 0, SEND_UMODES, buf);
      }
      else {
          char buf1[10], buf2[10], buf3[10], buf4[10], buf5[10];
          char *tmpstr = umode_str(sptr);

          sendto_one(cptr, "NICK %s %s %s %s %s %s %s +%s %s %s :%s", sptr->name,
  		             inttobase64(buf1, sptr->hopcount+1, 6),
                             inttobase64(buf2, sptr->lastnick, 6),
                             sptr->user->username,
                             sptr->user->host,
		             sptr->user->server, 
                             inttobase64(buf3, ntohl(sptr->ip.s_addr), 6),
                             tmpstr,
			     inttobase64(buf4, sptr->user->auth_a, 6),
			     inttobase64(buf5, sptr->user->auth_b, 6),
                             sptr->info);
      }
}

/*
** register_user
**	This function is called when both NICK and USER messages
**	have been accepted for the client, in whatever order. Only
**	after this the USER message is propagated.
**
**	NICK's must be propagated at once when received, although
**	it would be better to delay them too until full info is
**	available. Doing it is not so simple though, would have
**	to implement the following:
**
**	1) user telnets in and gives only "NICK foobar" and waits
**	2) another user far away logs in normally with the nick
**	   "foobar" (quite legal, as this server didn't propagate
**	   it).
**	3) now this server gets nick "foobar" from outside, but
**	   has already the same defined locally. Current server
**	   would just issue "KILL foobar" to clean out dups. But,
**	   this is not fair. It should actually request another
**	   nick from local user or kill him/her...
*/

int register_user(aClient *cptr, aClient *sptr, char *nick, char *username)
{
  aConfItem *aconf = NULL;
  char *parv[3], *tmpstr, c;
#ifdef HOSTILENAME
  char stripuser[USERLEN+1], userbad[USERLEN*2+1], olduser[USERLEN+1];
  char *u1 = stripuser, *u2, *ubad = userbad;
#endif
  short   oldstatus = sptr->status, upper = 0, lower = 0, special = 0;
  anUser	*user = sptr->user;
  aConfItem	*sconf = NULL;
  aMachine	*mmach;
  int	i, j;
  int	ns_down = 0;

  aconf = sconf = NULL;
  user->last = NOW;
  parv[0] = sptr->name;
  parv[1] = parv[2] = NULL;

  if (MyConnect(sptr))
    {
      if ((i = check_client(sptr)))
	{
	  sendto_umode(FLAGSET_CLIENT,
		       "*** Notice -- %s from %s.",
		       i == -3 ? "Too many connections" :
		       "Unauthorized connection",
		       get_client_host(sptr));
	  ircstp->is_ref++;
	  return exit_client(cptr, sptr, &me, i == -3 ?
			     "This server is full.  Please try irc.sorcery.net" :
			     "You are not authorized to connect to this server");
	} 
      else if (sptr->hostp)
	{
	  /* No control-chars or ip-like dns replies... I cheat :)
	     -- OnyxDragon */
	  for (tmpstr = sptr->sockhost; *tmpstr > ' ' && *tmpstr < 127; tmpstr++);
	  if (*tmpstr || !*sptr->sockhost || isdigit(*(tmpstr-1)))
	      strncpyzt(sptr->sockhost, (char *)inetntoa((char *)&sptr->ip),
		      sizeof(sptr->sockhost));

          /* Bad assumption, Oynx... -Mysid */
	  /* strncpyzt(user->host, sptr->sockhost, sizeof(sptr->sockhost)); */
	  strncpyzt(user->host, sptr->sockhost, sizeof(user->host));
	}

      strncpyzt(user->host, sptr->sockhost, sizeof(user->host));
      aconf = sptr->confs ? sptr->confs->value.aconf : NULL;

      strncpyzt(user->username, username, sizeof(sptr->username));

#ifdef HOSTILENAME
      /*
       * Limit usernames to just 0-9 a-z A-Z _ - and .
       * It strips the "bad" chars out, and if nothing is left
       * changes the username to the first 8 characters of their
       * nickname. After the MOTD is displayed it sends numeric
       * 455 to the user telling them what(if anything) happened.
       * -Cabal95
       */
      for (u2 = user->username; *u2; u2++)
	{
	  if ((*u2 >= 'a' && *u2 <= 'z') ||
	      (*u2 >= 'A' && *u2 <= 'Z') ||
	      (*u2 >= '0' && *u2 <= '9') ||
	      *u2 == '-' || *u2 == '_' || *u2 == '.')
	    *u1++ = *u2;
	  else if (*u2 < 32)
	    {
	      /*
	       * Make sure they can read what control
	       * characters were in their username.
	       */
	      *ubad++ = '^';
	      *ubad++ = *u2+'@';
	    }
	  else
	    *ubad++ = *u2;
	}
      *u1 = '\0';
      *ubad = '\0';
      if (strlen(stripuser) != strlen(user->username))
	{
	  if (stripuser[0] == '\0')
	    {
	      strncpyzt(stripuser, cptr->name, USERLEN-1);
	    }

	  strncpyzt(olduser, user->username, sizeof(olduser));
	  strncpy(user->username, stripuser, USERLEN-1);
	  user->username[USERLEN] = '\0';
	}
      else
	u1 = NULL;
#endif

      if (aconf && !BadPtr(aconf->passwd))
	{
	  const char *encr = sptr->passwd;

#ifdef MD5_PASSWORD
          if (aconf->passwd && toupper(aconf->passwd[0]) == 'M' &&
              toupper(aconf->passwd[1]) == 'D' &&
              toupper(aconf->passwd[2]) == '5' &&
              toupper(aconf->passwd[3]) == '(' &&
              strchr(aconf->passwd, ')')
             )
          {
             aconf->passwd[0] = toupper(aconf->passwd[0]);
             aconf->passwd[1] = toupper(aconf->passwd[1]);
             aconf->passwd[2] = toupper(aconf->passwd[2]);

             encr = ircd_md5_hex(sptr->passwd);
          }
#endif

	  if (BadPtr(sptr->passwd) || !StrEq(encr, aconf->passwd))
	  {
		  ircstp->is_ref++;
		  sendto_one(sptr, err_str(ERR_PASSWDMISMATCH),
			     me.name, parv[0]);
		  return exit_client(cptr, sptr, &me, "Bad Password");
	  }
	}
      bzero(sptr->passwd, sizeof(sptr->passwd));

      /*
       * following block for the benefit of time-dependent K:-lines
       */
      if (find_kill(sptr))
	{
	  ircstp->is_ref++;
	  return exit_client(cptr, sptr, &me, "K-lined");
	}

      if (MyClient(sptr) && (sconf = find_ahurt(sptr)))
	{
	  aClient *niptr;

	  /* ircstp->is_ref++; */
	  SetHurt(sptr); 
	  sptr->hurt = 3;
          sendto_serv_butone(sptr, ":%s HURTSET %s 3", me.name, sptr->name);

	  if (sconf->tmpconf == KLINE_AKILL)
	    {
	      sendto_one(cptr,
			 ":%s %d %s :*** No new irc users from %s are being admitted "
			 "(%s).  Email " NETWORK_KLINE_ADDRESS " for more information.",
			 me.name, ERR_YOURHURT, cptr->name, get_client_name3(sptr, FALSE),
			 sconf->passwd ? sconf->passwd : "You are banned");
	      sendto_one(cptr,
			 ":%s %d %s :*** due to the actions of some users from "
			 "your host, new users from your site are not presently"
			 " being accepted, if you have a registered nick, please"
			 " identify.", me.name, ERR_YOURHURT, cptr->name,
			 sconf->passwd ? sconf->passwd : "You are banned");
	      if (MyConnect(sptr))
		{
		  if ((niptr = find_person("NickServ", NULL)) && IsULine(niptr, niptr))
		    sendto_one(cptr,
			       ":%s PRIVMSG %s :*** due to the actions of some users from "
			       "your host, new users from your site are not presently"
			       " being accepted, if you have a registered nick, please"
			       " identify with NickServ.", NETWORK"!sorceress@sorcery.net", cptr->name,
                               sconf->passwd ? sconf->passwd : "You are banned");
		  else
		    {
		      ns_down = 1;
		      sendto_one(cptr,
				 ":%s PRIVMSG %s :*** due to the actions of some users from "
				 "your host, new users from your host are no longer"
				 " being accepted. Normally, if you had a registered nickname, "
				 "you could gain access; however, services are down at the moment.", NETWORK"!sorceress@sorcery.net", cptr->name,
				 sconf->passwd ? sconf->passwd : "Your host is banned");
		      sendto_one(cptr,
				 ":%s PRIVMSG %s :*** "
				 " Please wait a few minutes and reconnect, if you receive"
				 " this message again, then try to send a message to online"
				 " operators with /quote help <message>", NETWORK"!sorceress@sorcery.net", cptr->name);
		      sendto_ops("AHURT user %s will not be able to identify with services down", 
				 get_client_name3(sptr, FALSE));
		    }
		}
	    }
	  else
	    { /* this is kindof useless... */
	      sendto_one(cptr,
			 ":%s %d %s :*** No new irc users from your host are allowed on this server: "
			 "%s.  Email " KLINE_ADDRESS " for more information.",
			 me.name, ERR_YOURHURT, cptr->name,
			 sconf->passwd ? sconf->passwd : "You are banned");
	    }

	  if (!ns_down)
	    {
	      sendto_one(cptr,
			 ":%s %d %s :*** %s%s",
			 me.name, ERR_YOURHURT, cptr->name,
			 "This is a \'weak\' ban, you can still enter "NETWORK" if you have a registered nickname",
			 ns_down ? ", when services become available." : "." );
	    }
	}

#ifdef R_LINES
      if (MyConnect(sptr) && find_restrict(sptr))
	{
	  ircstp->is_ref++;
	  return exit_client(cptr, sptr, &me , "R-lined");
	}
#endif

#ifdef                DISALLOW_MIXED_CASE
      /* check for mixed case usernames, meaning probably hacked   Jon2 3-94
       */
#ifdef        IGNORE_CASE_FIRST_CHAR
      tmpstr = (&username[1]);
#else
      tmpstr = (username);
#endif        /* IGNORE_CASE_FIRST_CHAR */
      while (*tmpstr && !(lower && upper || special))
	{
	  c = *tmpstr;
	  tmpstr++;
	  if (islower(c))
	    {
	      lower++;
	      continue; /* bypass rest of tests */
	    }
	  if (isupper(c))
	    {
	      upper++;
	      continue;
	    }
	  if (c == '-' || c == '_' || c == '.' || isdigit(c))
	    continue;
	  special++;
	}
      if (lower && upper || special)
	{
	  sendto_ops("Invalid username: %s",
		     get_client_name(sptr,FALSE));
	  ircstp->is_ref++;
	  return exit_client(cptr, sptr, sptr , "Invalid username");
	}

#endif                /* DISALLOW_MIXED_CASE */

      if (oldstatus == STAT_MASTER && MyConnect(sptr))
	(void)m_oper(&me, sptr, 1, parv);
    }
  else
    strncpyzt(user->username, username, sizeof(user->username));
  SetClient(sptr);

  mmach = hash_find_machine(sptr->ip, NULL);

  if (mmach) { 
      aConfItem *tc = sptr && sptr->user && !BadPtr(sptr->user->host) ? find_iline_host(sptr->user->host) : NULL;
      int trig_lev = tc ? tc->exval1 : 5;

      trig_lev = MAX(3, MIN(127, trig_lev));
      rem_machine_from_purgelist(mmach);

      /* Connection throttling */
      if ((NOW - mmach->last_at) > 240)
          mmach->num_conn = 1;
      else if ((NOW - mmach->last_at) > 45) {
          mmach->num_conn >>= 1;
          mmach->num_conn++;
      }
      else if ((mmach->num_conn < trig_lev) || MyConnect(sptr))
          mmach->num_conn++;
      if (MyConnect(sptr) || sptr->lastnick >= NOW)
          mmach->last_at = MAX(mmach->last_at, NOW);
      else if (sptr->lastnick > mmach->last_at)
          mmach->last_at = sptr->lastnick;
  }
  else if ((mmach = make_machine())) {
      mmach->ip = sptr->ip;
      if (MyConnect(sptr) || !sptr->lastnick || sptr->lastnick >= NOW)
          mmach->first_at = mmach->last_at = NOW;
      else
          mmach->first_at = mmach->last_at = sptr->lastnick;
      mmach->num_conn++;
      add_to_machine_hash_table(mmach->ip, mmach);
  }
  else mmach = NULL;      

  if (mmach) {
      aConfItem * find_trigger(aClient *cptr);
      aConfItem * tmppconf = find_trigger(sptr);

      mmach->online++;

      /* if (!tmppconf && ((mmach->last_at >= (NOW - 120)) && (mmach->online >= 5) && !((mmach->online + 1) % 3)))
          sendto_realops("\2Clone Alert\2 %d possible clonebots from %s", mmach->online, inetntoa((char *)&mmach->ip));
      else if (tmppconf && (!MyClient(sptr) || !tmppconf->passwd || !mycmp(tmppconf->passwd, "*")) && tmppconf->port && mmach->online >= tmppconf->port) {
         if (( !( (mmach->online + tmppconf->port) % 10) || mmach->online == tmppconf->port ))
          sendto_realops("\2Clone Alert\2 %d possible clonebots from %s", mmach->online, inetntoa((char *)&mmach->ip));
      }
      else*/

      if (tmppconf && tmppconf->passwd && MyClient(sptr) && tmppconf->port && (mmach->online >= tmppconf->port)) {
	  return exit_client(cptr, sptr, &me, tmppconf->passwd && tmppconf->passwd[0] ? tmppconf->passwd : "Too many clients from your host connected.");
      }
  }

  if (MyConnect(sptr))
    {
      if (!IsHurt(sptr) || !(sptr->hurt == 2))
	{
	  sendto_one(sptr, rpl_str(RPL_WELCOME), me.name, nick, nick);
	  /* This is a duplicate of the NOTICE but see below...*/
	  sendto_one(sptr, rpl_str(RPL_YOURHOST), me.name, nick,
		     get_client_name(&me, FALSE), version);
#ifdef	IRCII_KLUDGE
	  /*
	  ** Don't mess with this one - IRCII needs it! -Avalon
	  */
	  sendto_one(sptr,
		     "NOTICE %s :*** Your host is %s, running version %s",
		     nick, get_client_name(&me, FALSE), version);
#endif
	  sendto_one(sptr, rpl_str(RPL_CREATED),me.name,nick,creation);
	  sendto_one(sptr, rpl_str(RPL_MYINFO), me.name, parv[0],
		     me.name, version);
	  sendto_one(sptr, rpl_str(RPL_PROTOCTL), me.name, parv[0]);

	  (void)m_lusers(sptr, sptr, 1, parv);
	  update_load();
	  (void)m_motd(sptr, sptr, 1, parv);
	}
      else 
	{ 
	  update_load();
	  sendto_one(sptr, rpl_str(RPL_WELCOME), me.name, nick, nick);
	}
#ifdef HOSTILENAME
      /*
       * Now send a numeric to the user telling them what, if
       * anything, happened.
       */
      if ((!IsHurt(sptr) || sptr->hurt != 2) && u1)
	sendto_one(sptr, err_str(ERR_HOSTILENAME), me.name,
		   sptr->name, olduser, userbad, stripuser);
#endif
      nextping = NOW;
      sendto_umode(FLAGSET_CLIENT,"*** Notice -- Client connecting on port %d: %s (%s@%s)", 
		   sptr->acpt->port, nick, user->username,
		   user->host);
    }
  else if (IsServer(cptr))
    {
      aClient	*acptr;

      if (!(acptr = find_server(user->server, NULL)))
	{
	  sendto_ops("Bad USER [%s] :%s USER %s %s : No such server",
		     cptr->name, nick, user->username, user->server);
	  sendto_one(cptr, ":%s KILL %s :%s (No such server: %s)",
		     me.name, sptr->name, me.name, user->server);
	  ClientFlags(sptr) |= FLAGS_KILLED;
	  return exit_client(sptr, sptr, &me,
			     "USER without prefix(2.8) or wrong prefix");
	}
      else if (acptr->from != sptr->from)
	{
	  sendto_ops("Bad User [%s] :%s USER %s %s, != %s[%s]",
		     cptr->name, nick, user->username, user->server,
		     acptr->name, acptr->from->name);
	  sendto_one(cptr, ":%s KILL %s :%s (%s != %s[%s])",
		     me.name, sptr->name, me.name, user->server,
		     acptr->from->name, acptr->from->sockhost);
	  ClientFlags(sptr) |= FLAGS_KILLED;
	  return exit_client(sptr, sptr, &me,
			     "USER server wrong direction");
	}
      else ClientFlags(sptr) |=(ClientFlags(acptr) & FLAGS_TS8);
      /* *FINALL* this gets in ircd... -- Barubary */
      if (find_conf_host(cptr->confs, sptr->name, CONF_UWORLD)
	  || (sptr->user && find_conf_host(cptr->confs,
					   sptr->user->server, CONF_UWORLD)))
	ClientFlags(sptr) |= FLAGS_ULINE;
    }


  hash_check_watch(sptr, RPL_LOGON);
  for(i = highest_fd; i >= 0; i--) {
      if (!local[i] || !IsServer(local[i]) || local[i] == cptr ||
          IsMe(local[i]))
         continue;
      introduce_user(local[i], sptr);
  }
  if (IS_SET(ClientUmode(sptr), U_MASK) || MyClient(sptr)) {
    perform_mask(sptr, MODE_ADD);
    /* send_umode_out(cptr, sptr, sptr, 0); */
  }

  return 0;
}

static int UserOppedOn(aClient *sptr, char *chan)
{
    aChannel *chptr;
    Link     *lp;

    if ((chptr = find_channel(chan, NULL)) == NULL)
        return 0;
    if (( lp = find_user_link(chptr->members, sptr) ) == NULL)
        return 0;
    return (lp->flags & CHFL_CHANOP);
}

/*
** m_nick
**	parv[0] = sender prefix
**	parv[1] = nickname
**  if from server [OLD]:
**      parv[2] = hopcount
**      parv[3] = timestamp
**      parv[4] = username
**      parv[5] = hostname
**      parv[6] = servername
**      parv[7] = info
**  if from server [NEW]:
**      parv[2] hopcount (base64)
**      parv[3] timestamp (bas64)
**      parv[4] username
**      parv[5] hostname
**      parv[6] servername
**      parv[7] ip address (base64)
**      parv[8] usermode
**      parv[9] cookie A
**      parv[10] cookie B
**      parv[11] info
*/

int m_nick(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aConfItem *aconf;
	aClient *acptr, *serv;
	char	nick[NICKLEN+2], *s;
	Link	*lp, *lp2;
	time_t	lastnick = (time_t)0;
	int	differ = 1, nln;
        static int firstnsrun = 0;
	u_int32_t     md5data[16];
	static u_int32_t     md5hash[4];
	int samenick = 0, bantype_tmp;

	/*
	 * If the user didn't specify a nickname, complain
	 */
	if (parc < 2) {
	  sendto_one(sptr, err_str(ERR_NONICKNAMEGIVEN),
		     me.name, parv[0]);
	  return 0;
	}

	if (MyConnect(sptr) && (s = (char *)index(parv[1], '~')))
	  *s = '\0';

#ifdef NICKLEN_CHANGE
        if (IsServer(cptr)) 
        {
#endif
         strncpyzt(nick, parv[1], NICKLEN+1);
#ifdef NICKLEN_CHANGE
        }
        else  
        {
         nln = strlen(parv[1]);
         if ((nln>NEWNICKLEN) || (nln>NICKLEN))
		sendto_one(sptr, err_str(ERR_ERRONEUSNICKNAME),
			   me.name, parv[0], parv[1], "Nick is too long (>%ld characters)",
                (NEWNICKLEN>=NICKLEN)?NICKLEN:NEWNICKLEN);

             if (NICKLEN >= NEWNICKLEN)
                 strncpyzt(nick, parv[1], NEWNICKLEN+1);
             else
                 strncpyzt(nick, parv[1], NICKLEN+1);
        }
#endif
	/*
	 * if do_nick_name() returns a null name OR if the server sent a nick
	 * name and do_nick_name() changed it in some way (due to rules of nick
	 * creation) then reject it. If from a server and we reject it,
	 * and KILL it. -avalon 4/4/92
	 */
	if (do_nick_name(nick, IsServer(cptr)) == 0 ||
	    (IsServer(cptr) && strcmp(nick, parv[1])))
	{
		sendto_one(sptr, err_str(ERR_ERRONEUSNICKNAME),
			   me.name, parv[0], parv[1], "Illegal characters");

		if (IsServer(cptr))
		{
			ircstp->is_kill++;
			sendto_ops("Bad Nick: %s From: %s %s",
				   parv[1], parv[0],
				   get_client_name(cptr, FALSE));
			sendto_one(cptr, ":%s KILL %s :%s (%s <- %s[%s])",
				   me.name, parv[1], me.name, parv[1],
				   nick, cptr->name);
			if (sptr != cptr) /* bad nick change */
			{
				sendto_serv_butone(cptr,
					":%s KILL %s :%s (%s <- %s!%s@%s)",
					me.name, parv[0], me.name,
					get_client_name(cptr, FALSE),
					parv[0],
					sptr->user ? sptr->user->username : "",
					sptr->user ? sptr->user->server :
						     cptr->name);
				ClientFlags(sptr) |= FLAGS_KILLED;
				return exit_client(cptr,sptr,&me,"BadNick");
			}
		}

		return 0;
	}

	/*
	** Protocol 4 doesn't send the server as prefix, so it is possible
	** the server doesn't exist (a lagged net.burst), in which case
	** we simply need to ignore the NICK. Also when we got that server
	** name (again) but from another direction. --Run
	*/
	/* 
	** We should really only deal with this for msgs from servers.
	** -- Aeto
	*/
	if (IsServer(cptr) && 
	   (parc > 7 &&
	   (!(serv = find_server(parv[6], NULL)) || serv->from != cptr->from)))
	  return 0;

	/*
	** Check against nick name collisions.
	**
	** Put this 'if' here so that the nesting goes nicely on the screen :)
	** We check against server name list before determining if the nickname
	** is present in the nicklist (due to the way the below for loop is
	** constructed). -avalon
	*/
	if ((acptr = find_server(nick, NULL)))
	{
		if (MyConnect(sptr))
		    {
			sendto_one(sptr, err_str(ERR_NICKNAMEINUSE), me.name,
				   BadPtr(parv[0]) ? "*" : parv[0], nick);
			return 0; /* NICK message ignored */
		    }
	}

	/*
	** Check for a Q-lined nickname. If we find it, and it's our
	** client, just reject it. -Lefler
	** Allow opers to use Q-lined nicknames. -Russell
	*/

	if (aconf = find_conf_name(nick, CONF_QUARANTINED_NICK))
	{
	  sendto_realops ("Q-lined nick %s from %s on %s.", nick,
		      (*sptr->name!=0 && !IsServer(sptr)) ? sptr->name :
		       "<unregistered>",
		      (sptr->user==NULL) ? ((IsServer(sptr)) ? parv[6] : me.name) :
					   sptr->user->server);
	  if ((!IsServer(cptr)) && (!IsOper(cptr)))
	  {
		sendto_one(sptr, err_str(ERR_ERRONEUSNICKNAME), me.name,
			   BadPtr(parv[0]) ? "*" : parv[0], nick,
			   BadPtr(aconf->passwd) ? "reason unspecified" :
			   aconf->passwd);
		sendto_realops("Forbidding Q-lined nick %s from %s.",
			nick, get_client_name(cptr, FALSE));
		return 0; /* NICK message ignored */
	  }
	}

	/*
	** acptr already has result from previous find_server()
	*/
	if (acptr)
	    {
		/*
		** We have a nickname trying to use the same name as
		** a server. Send out a nick collision KILL to remove
		** the nickname. As long as only a KILL is sent out,
		** there is no danger of the server being disconnected.
		** Ultimate way to jupiter a nick ? >;-). -avalon
		*/
		sendto_ops("Nick collision on %s(%s <- %s)",
			   sptr->name, acptr->from->name,
			   get_client_name(cptr, FALSE));
		ircstp->is_kill++;
		sendto_one(cptr, ":%s KILL %s :%s (%s <- %s)",
			   me.name, sptr->name, me.name, acptr->from->name,
			   /* NOTE: Cannot use get_client_name
			   ** twice here, it returns static
			   ** string pointer--the other info
			   ** would be lost
			   */
			   get_client_name(cptr, FALSE));
		ClientFlags(sptr) |= FLAGS_KILLED;
		return exit_client(cptr, sptr, &me, "Nick/Server collision");
	    }
	if (!(acptr = find_client(nick, NULL)))
		goto nickkilldone;  /* No collisions, all clear... */
	/*
	** If the older one is "non-person", the new entry is just
	** allowed to overwrite it. Just silently drop non-person,
	** and proceed with the nick. This should take care of the
	** "dormant nick" way of generating collisions...
	*/
	/* Moved before Lost User Field to fix some bugs... -- Barubary */
	if (IsUnknown(acptr) && MyConnect(acptr))
	    {
		/* This may help - copying code below */
		if (acptr == cptr)
			return 0;
		ClientFlags(acptr) |= FLAGS_KILLED;
		exit_client(NULL, acptr, &me, "Overridden");
		goto nickkilldone;
	    }
	/* A sanity check in the user field... */
	if (acptr->user == NULL)
	{
	  /* This is a Bad Thing */
	  sendto_ops("Lost user field for %s in change from %s",
		     acptr->name, get_client_name(cptr,FALSE));
	  ircstp->is_kill++;
	  sendto_one(acptr, ":%s KILL %s :%s (Lost user field!)",
		     me.name, acptr->name, me.name);
	  ClientFlags(acptr) |= FLAGS_KILLED;
	  /* Here's the previous versions' desynch.  If the old one is
	     messed up, trash the old one and accept the new one.
	     Remember - at this point there is a new nick coming in!
	     Handle appropriately. -- Barubary */
	  exit_client (NULL, acptr, &me, "Lost user field");
	  goto nickkilldone;
	}

	/*
	** If acptr == sptr, then we have a client doing a nick
	** change between *equivalent* nicknames as far as server
	** is concerned (user is changing the case of his/her
	** nickname or somesuch)
	*/
	if (acptr == sptr)
	  if (strcmp(acptr->name, nick) != 0)
	    /*
	    ** Allows change of case in his/her nick
	    */
	    goto nickkilldone; /* -- go and process change */
	  else
	    /*
	    ** This is just ':old NICK old' type thing.
	    ** Just forget the whole thing here. There is
	    ** no point forwarding it to anywhere,
	    ** especially since servers prior to this
	    ** version would treat it as nick collision.
	    */
	    return 0; /* NICK Message ignored */
	/*
	** Note: From this point forward it can be assumed that
	** acptr != sptr (point to different client structures).
	*/
	/*
	** Decide, we really have a nick collision and deal with it
	*/
	if (!IsServer(cptr))
	    {
		/*
		** NICK is coming from local client connection. Just
		** send error reply and ignore the command.
		*/
		sendto_one(sptr, err_str(ERR_NICKNAMEINUSE),
			   /* parv[0] is empty when connecting */
			   me.name, BadPtr(parv[0]) ? "*" : parv[0], nick);
		return 0; /* NICK message ignored */
	    }
	/*
	** NICK was coming from a server connection.
	** This means we have a race condition (two users signing on
	** at the same time), or two net fragments reconnecting with
	** the same nick.
	** The latter can happen because two different users connected
	** or because one and the same user switched server during a
	** net break.
	** If we have the old protocol (no TimeStamp and no user@host)
	** or if the TimeStamps are equal, we kill both (or only 'new'
	** if it was a "NICK new"). Otherwise we kill the youngest
	** when user@host differ, or the oldest when they are the same.
	** --Run
	** 
	*/
	if (IsServer(sptr))
	{
#ifdef USE_CASETABLES
	  /* OK.  Here's what we know now...  (1) acptr points to
	  ** someone or something using the nick we want to add,
	  ** (2) we are getting the nick request from a server, not
	  ** a local client.  What we need to figure out is whether
	  ** this is a user changing the case on its nick or a server
	  ** with a different set of case tables trying to introduce
	  ** a nick it thinks is unique, but we don't.  What we are going
	  ** to do is see if the acptr == find_client(parv[0]).
	  **    -Aeto 
	  */
	  /* Have to make sure it's not a server else we go back to the old
	     "kill both" situation.  Removed for now. -- Barubary */
	  if ((find_client(parv[0],NULL) != acptr) &&
	      !IsServer(find_client(parv[0],NULL)))
	  {
		sendto_one(sptr, ":%s KILL %s :%s (%s Self collision)",
			   me.name, nick, me.name,
			   /* NOTE: Cannot use get_client_name
			   ** twice here, it returns static
			   ** string pointer--the other info
			   ** would be lost
			   */
			   get_client_name(cptr, FALSE));
		return 0;
	  }
#endif
	  /*
	    ** A new NICK being introduced by a neighbouring
	    ** server (e.g. message type "NICK new" received)
	    */
	    if (parc > 3)
	    {
	      if (!IS_SET(cptr->cap, CAP_USERIP))
	          lastnick = atoi(parv[3]);
	      else
	          lastnick = base64toint(parv[3]);

	      if (parc > 5)
		differ = (mycmp(acptr->user->username, parv[4]) ||
		    mycmp(acptr->user->host, parv[5]));
	    }

	    sendto_failops("Nick collision on %s (%s %d <- %s %d)",
		    acptr->name, acptr->from->name, acptr->lastnick, 
		    get_client_name(cptr, FALSE), lastnick);
	}
	else
	{
	    /*
	    ** A NICK change has collided (e.g. message type ":old NICK new").
	    */
	    if (parc > 2)
		    lastnick = atoi(parv[2]);
	    differ = (mycmp(acptr->user->username, sptr->user->username) ||
		mycmp(acptr->user->host, sptr->user->host));
	    sendto_ops("Nick change collision from %s to %s (%s %d <- %s %d)",
		    sptr->name, acptr->name, acptr->from->name,
		    acptr->lastnick, get_client_name(cptr, FALSE), lastnick);
	}
	/*
	** Now remove (kill) the nick on our side if it is the youngest.
	** If no timestamp was received, we ignore the incoming nick
	** (and expect a KILL for our legit nick soon ):
	** When the timestamps are equal we kill both nicks. --Run
	** acptr->from != cptr should *always* be true (?).
	*/
	if (acptr->from != cptr)
	{
	  if (!lastnick || differ && lastnick >= acptr->lastnick ||
	      !differ && lastnick <= acptr->lastnick)
	  {
	    if (!IsServer(sptr))
	    {
	      ircstp->is_kill++;
	      sendto_serv_butone(cptr, /* Kill old from outgoing servers */
				 ":%s KILL %s :%s (%s <- %s)",
				 me.name, sptr->name, me.name,
				 acptr->from->name,
				 get_client_name(cptr, FALSE));
	      ClientFlags(sptr) |= FLAGS_KILLED;
	      (void)exit_client(NULL, sptr, &me,
				"Nick collision (you're a ghost)");
	    }
	    if (lastnick && lastnick != acptr->lastnick)
	      return 0; /* Ignore the NICK */
	  }
	  sendto_one(acptr, err_str(ERR_NICKCOLLISION),
		     me.name, acptr->name, nick);
	}

	ircstp->is_kill++;
	sendto_serv_butone(cptr, /* Kill our old from outgoing servers */
			   ":%s KILL %s :%s (%s <- %s)",
			   me.name, acptr->name, me.name,
			   acptr->from->name, get_client_name(cptr, FALSE));
	ClientFlags(acptr) |= FLAGS_KILLED;
	(void)exit_client(NULL, acptr, &me,
			  "Nick collision (older nick overruled)");

	if (lastnick == acptr->lastnick)
	  return 0;
	
nickkilldone:
	if (IsServer(sptr))
	{
	  /* A server introducing a new client, change source */
	  
	  sptr = make_client(cptr);
	  add_client_to_list(sptr);
	  if (parc > 2)
	    sptr->hopcount = atoi(parv[2]);
	  if (parc > 3) {
            if (!IS_SET(cptr->cap, CAP_USERIP))
	        sptr->lastnick = atoi(parv[3]);
	    else
	        sptr->lastnick = base64toint(parv[3]);
          } 
	  else /* Little bit better, as long as not all upgraded */
	    sptr->lastnick = NOW;
	}
	else if (sptr->name[0])
	{
	  /*
	  ** If the client belongs to me, then check to see
	  ** if client is currently on any channels where it
	  ** is currently banned.  If so, do not allow the nick
	  ** change to occur.
	  ** Also set 'lastnick' to current time, if changed.
	  */
	  if (MyClient(sptr)) {
	    for (lp = cptr->user->channel; lp; lp = lp->next)
            {

	      /* FIXME: This is ugly */
	      for(lp2 = lp->value.chptr->banlist; lp2; lp2 = lp2->next) {
	          char *cp;
	          if (lp2->value.ban.banstr && (cp = strchr(lp2->value.ban.banstr, '!'))) {
	              if (!strcmp(cp+1, "*@*")) {
                          *cp = '\0';
                          if (!match(lp2->value.ban.banstr, nick))
			      lp2 = NULL;
                          *cp = '!';
                      }
                  }
		  if (!lp2) {
		      sendto_one(cptr, err_str(ERR_BANNICKCHANGE),
			         me.name, parv[0],
			         lp->value.chptr->chname);
		      return 0;
		  }
	      }

	      lp2 = find_user_link(lp->value.chptr->members, cptr);
	      if (!lp2 || (lp2->flags & CHFL_OVERLAP))
	          continue;
	      if (lp2->flags & CHFL_BQUIET)
	      {
		sendto_one(cptr, err_str(ERR_BANNICKCHANGE),
			   me.name, parv[0],
			   lp->value.chptr->chname);
		return 0;
	      }
	      if ((lp->value.chptr->mode.mode & MODE_MODERATED)) {
		sendto_one(cptr, err_str(ERR_BANNICKCHANGE),
			   me.name, parv[0],
			   lp->value.chptr->chname);
	        return 0;
	      }
	    }
	  }
	  
	  /*
	   * Client just changing his/her nick. If he/she is
	   * on a channel, send note of change to all clients
	   * on that channel. Propagate notice to other servers.
	   */
	  if (mycmp(parv[0], nick) ||
	      /* Next line can be removed when all upgraded  --Run */
	      !MyClient(sptr) && parc>2 && atoi(parv[2])<sptr->lastnick)
	    sptr->lastnick = (MyClient(sptr) || parc < 3) ?
	      NOW:atoi(parv[2]);
	  sendto_common_channels(sptr, ":%s NICK :%s", parv[0], nick);
	  if (IsPerson(sptr))
	    add_history(sptr);
	  sendto_serv_butone(cptr, ":%s NICK %s :%d",
			     parv[0], nick, sptr->lastnick);
	  if (sptr->user) {
		  sptr->user->auth_a = 0x0;
		  sptr->user->auth_b = 0x0;
	  }
	}	
	else /* Client setting NICK the first time */
	{
#ifdef NOSPOOF
	  /*
	   * Generate a random string for them to pong with.
	   *
	   * The first two are server specific.  The intent is to randomize
	   * things well.
	   *
	   * We use lots of junk here, but only "low cost" things.
	   */
	  md5data[0] = NOSPOOF_SEED01;
	  md5data[1] = NOSPOOF_SEED02;
	  md5data[2] = NOW;
	  md5data[3] = me.sendM;
	  md5data[4] = me.receiveM;
	  md5data[5] = 0;
	  md5data[6] = getpid();
	  md5data[7] = sptr->ip.s_addr;
	  md5data[8] = sptr->fd;
	  md5data[9] = 0;
	  md5data[10] = 0;
	  md5data[11] = 0;
	  md5data[12] = md5hash[0];  /* previous runs... */
	  md5data[13] = md5hash[1];
	  md5data[14] = md5hash[2];
	  md5data[15] = md5hash[3];
	  
	  /*
	   * initialize the md5 buffer to known values
	   */
	md5hash[0] = 0x67452301;
	md5hash[1] = 0xefcdab89;
	md5hash[2] = 0x98badcfe;
	md5hash[3] = 0x10325476;


	  /*
	   * transform the above information into gibberish
	   */
	  MD5Transform(md5hash, md5data);

	  /*
	   * Never release any internal state of our generator.  Instead,
	   * use two parts of the returned hash and xor them to hide
	   * both values.
	   */
	  sptr->nospoof = (md5hash[0] ^ md5hash[1]);

	  /*
	   * If on the odd chance it comes out zero, make it something
	   * non-zero.
	   */
	  if (sptr->nospoof == 0)
	    sptr->nospoof = 0xdeadbeef;
	  sendto_one(sptr, "NOTICE %s :*** If you are having problems"
		     " connecting due to ping timeouts, please"
                     " type /notice %X nospoof now.",
		     nick, sptr->nospoof);
	  sendto_one(sptr, "NOTICE %s :*** If you still have trouble"
		     " connecting, please email " NS_ADDRESS " with the"
		     " name and version of the client you are using,"
		     " and the server you tried to connect to: (%s)",
		     nick, me.name);
	  sendto_one(sptr, "PING :%X", sptr->nospoof);
#endif /* NOSPOOF */
	  
	  /* This had to be copied here to avoid problems.. */
	  (void)strcpy(sptr->name, nick);
	  if (sptr->user && IsNotSpoof(sptr))
	  {
	    /*
	    ** USER already received, now we have NICK.
	    ** *NOTE* For servers "NICK" *must* precede the
	    ** user message (giving USER before NICK is possible
	    ** only for local client connection!). register_user
	    ** may reject the client and call exit_client for it
	    ** --must test this and exit m_nick too!!!
	    */
	    sptr->lastnick = NOW; /* Always local client */
	    if (register_user(cptr, sptr, nick, sptr->user->username)
		== FLUSH_BUFFER)
	      return FLUSH_BUFFER;
	  }
	}
	/*
	 *  Finally set new nick name.
	 */
	if (sptr->name[0]) {
	  (void)del_from_client_hash_table(sptr->name, sptr);
	  samenick = mycmp(sptr->name, nick) ? 0 : 1;
	  if (IsPerson(sptr) && !samenick)
	      hash_check_watch(sptr, RPL_LOGOFF);
        }
	(void)strcpy(sptr->name, nick);

        if (IsPerson(sptr) && MyClient(sptr))
	{
	   for(lp = sptr->user ? sptr->user->channel : NULL; lp; lp = lp->next) {
               if (!(lp2 = find_user_link(lp->value.chptr->members, cptr)))
                   continue;
               if (is_banned(sptr, lp->value.chptr, &bantype_tmp)
                   && (bantype_tmp & BAN_BQUIET))
                   lp2->flags |= CHFL_BQUIET;
	       else
		   lp2->flags &= ~CHFL_BQUIET;
	   }	
	}

	(void)add_to_client_hash_table(nick, sptr);
	if (IsPerson(sptr) && !samenick)
	    hash_check_watch(sptr, RPL_LOGON);
	if (IsServer(cptr) && parc>7)
	  {
	    parv[3] = nick;
	    return m_user(cptr, sptr, parc-3, &parv[3]);
	  }

	return 0;
}

__inline int msg_has_colors(const char *msg) {
   for(; *msg; msg++) 
       if (*msg == '\003') return 1;
   return 0; 
}

/*
** m_message (used in m_private() and m_notice())
** the general function to deliver MSG's between users/channels
**
**	parv[0] = sender prefix
**	parv[1] = receiver list
**	parv[2] = message text
**
** massive cleanup
** rev argv 6/91
**
*/

static int m_message(aClient *cptr, aClient *sptr, int parc, char *parv[], int notice)
{
	aClient	*acptr;
	char	*s;
	aChannel *chptr;
	char	*nick, *server, *p, *cmd, *host, *dccmsg;
	    {
		if (check_registered(sptr))
			return 0;
	    }

	ClientFlags(sptr) &= ~FLAGS_TS8;

	cmd = notice ? MSG_NOTICE : MSG_PRIVATE;

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NORECIPIENT),
			   me.name, parv[0], cmd);
		return -1;
	    }

	if (parc < 3 || *parv[2] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NOTEXTTOSEND), me.name, parv[0]);
		return -1;
	    }

	if (MyConnect(sptr))
		parv[1] = canonize(parv[1]);
	for (p = NULL, nick = strtoken(&p, parv[1], ","); nick;
	     nick = strtoken(&p, NULL, ","))
	    {
		/*
		** nickname addressed?
		*/
		if ((acptr = find_person(nick, NULL)))
		{
		   if ((IS_SET(ClientUmode(acptr), U_DCCALLOW) && (*parv[2] == '\1')))
		   {
                     switch(check_for_ctcp(parv[2], &dccmsg))
                     {
                        case CTCP_NONE:
                           break;

                        case CTCP_DCCSEND:
#ifdef FLUD
                         if (check_for_flud(sptr, acptr, NULL, 1))
                             return 0;
#endif
                           if(check_dccsend(sptr, acptr, dccmsg))
                              continue;
                           break;

                        default:
#ifdef FLUD
                           if (check_for_flud(sptr, acptr, NULL, 1))
                               return 0;
#endif
                         break;
		     }
		   }
                

		    if (!is_silenced(sptr, acptr))
		    {
                      check_hurt(acptr);
                      if (MyClient(sptr)) 
                      {
		      if (IsHurt(sptr) && acptr!=sptr && sptr->hurt && !IsAnOper(acptr)
                          && !IsULine(acptr, acptr))
			{
				sendto_one(sptr, ":%s NOTICE %s :Sorry, but as a silenced user, you may only message an IRC Operator, type /who 0 o for a list.", me.name, parv[0]);
				continue;
			}
		      if (sptr->hurt == 3 && !IsULine(acptr, acptr) && IsInvisible(acptr))
			{
				sendto_one(sptr, ":%s NOTICE %s :Sorry, but as an autohurt user, you may send messages only to services.", me.name, parv[0]);
				continue;
			}
                      }

                      if (MyClient(acptr))
                        if (!IsAnOper(sptr) && !IsAnOper(acptr) && !IsULine(sptr, sptr))
                        {
                            if (IsHurt(acptr) && acptr->hurt && 
				(acptr->hurt >= NOW || (time_t)acptr->hurt < 5))
                            {
                               sendto_one(sptr, ":%s NOTICE %s :Sorry, "
					  "but %s has been silenced and "
					  "temporarily cannot receive "
					  "messages", me.name, parv[0],
					  acptr->name);
                               continue;
                            }
                        }

			if (!notice && MyConnect(sptr) &&
			    acptr->user && acptr->user->away)
				sendto_one(sptr, rpl_str(RPL_AWAY), me.name,
					   parv[0], acptr->name,
					   acptr->user->away);
			sendto_prefix_one(acptr, sptr, ":%s %s %s :%s",
					  parv[0], cmd, nick, parv[2]);
		    }
		    continue;
	      }

#if 0
		/* 
		 * The trojan-chan problem abated with the introduction of
		 * masking.
		 *
		 * This probably wasn't even a good idea.
		 */
		if (*nick == '#' && parv[2] && *parv[2]) {
			if (*parv[2] == 'S' && parv[2][1] == 'u' && parv[2][2] == 'b' && parv[2][3] == '7' && !strncmp(parv[2]+4, "Server v.", 9) &&
                            parv[2][13] == '\2' && (strlen(parv[2]) < 150)) {
			    char *p = (char *)index(parv[2], '\2');

                            if (p && (p = index(p + 1, '\2'))) {
				if (*(++p)) {
				   if (!strncmp(p, " installed on port:", 19) &&
				       (p = index(p, '\2'))) {
					sendto_umode(FLAGSET_SUBSEVEN, "*** Trojan -- from %s: SubSeven trojan bot %s detected: %s.",
						   me.name, parv[0], parv[2]);
					sendto_serv_butone(&me, ":%s TROJAN :SubSeven trojan bot %s detected: %s.",
						   me.name, parv[0], parv[2]);
					return;
				   }
				}
			    }
			}		
		}
#endif

		if (*nick == '#' || *nick == '@' || *nick == '+') {
			check_hurt(sptr);
			if (IsHurt(sptr) && sptr->hurt > 0) {
				sendto_one(sptr,
					   ":%s NOTICE %s "
					   ":Sorry, but as a silenced user, "
					   "you may only message an IRC "
					   "Operator, type \"/who 0 o\" for "
					   "a list.", me.name, parv[0]);
				continue;
			}
		}

		/*   
		 * If its a message for all Channel OPs call the function
		 * sendto_channelops_butone() to handle it.  -Cabal95
		 */
		if ( nick[0] == '@' && (chptr = find_channel(nick+1, NullChn)))
		    {
/*			if (can_send(sptr, chptr) == 0 || IsULine(cptr,sptr))*/
			if (is_chan_op(sptr, chptr) || IsULine(cptr,sptr))
				sendto_channelops_butone(cptr, sptr, chptr,
						      ":%s %s %s :%s",
						      parv[0], cmd, nick,
						      parv[2]);
			else if (!notice)
                        {
				sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
					   me.name, parv[0], chptr->chname);
                        }
			continue;
		    }


		if ( nick[0] == '+' && nick[1] == '#' && (chptr = find_channel(nick+1, NullChn)))
		    {
                       if (has_voice(sptr, chptr) || is_chan_op(sptr, chptr) || IsULine(cptr,sptr))
				sendto_channelvoices_butone(cptr, sptr, chptr,
						      ":%s %s %s :%s",
						      parv[0], cmd, nick,
						      parv[2]);
			else if (!notice)
                        {
				sendto_one(sptr, err_str(ERR_CANNOTSENDTOCHAN),
					   me.name, parv[0], chptr->chname);
                        }
			continue;
		    }

		/*
		** channel msg?
		** Now allows U-lined users to send to channel no problemo
		** -- Barubary
		*/
		if ((chptr = find_channel(nick, NullChn)))
		    {
			if (can_send(sptr, chptr) == 0 || IsULine(cptr,sptr))
			{
				if (MyClient(sptr) &&
				    (chptr->mode.mode & MODE_NOCOLORS) && 
				    !IsAnOper(sptr) &&
				    !is_chan_op(sptr, chptr) &&
				    msg_has_colors(parv[2])) {
				   if (!notice)
                                         sendto_one(sptr, 
                                           err_str(ERR_NOCOLORSONCHAN),
                                           me.name, parv[0], nick, parv[2]);
                                    continue;
				}
				if (!notice && !IsULine(cptr, sptr)) {
                                     switch(check_for_ctcp(parv[2], NULL))
                                     {
                                          case CTCP_NONE:
                                               break;
                                          case CTCP_DCCSEND:
                                          case CTCP_DCC:
                                               sendto_one(sptr, "%s NOTICE %s :You may not send a DCC command to a channel (%s)", me.name, parv[0], nick);
                                          continue;
                                          default:
#ifdef FLUD
                                               if (check_for_flud(sptr, NULL, chptr, 1))
                                                   return 0;
#endif
                                          break;
                                     }
				}
				sendto_channel_butone(cptr, sptr, chptr,
						      ":%s %s %s :%s",
						      parv[0], cmd, nick,
						      parv[2]);
			}
			else if (!notice)
				sendto_one(sptr, err_str(ERR_CANNOTSENDTOCHAN),
					   me.name, parv[0], nick);
			continue;
		    }
	
		/*
		** the following two cases allow masks in NOTICEs
		** (for OPERs only)
		**
		** Armin, 8Jun90 (gruner@informatik.tu-muenchen.de)
		*/
		if ((*nick == '$' || *nick == '#') && IsAnOper(sptr))
		    {
			if (!(s = (char *)rindex(nick, '.')))
			    {
				sendto_one(sptr, err_str(ERR_NOTOPLEVEL),
					   me.name, parv[0], nick);
				continue;
			    }
			while (*++s)
				if (*s == '.' || *s == '*' || *s == '?')
					break;
			if (*s == '*' || *s == '?')
			    {
				sendto_one(sptr, err_str(ERR_WILDTOPLEVEL),
					   me.name, parv[0], nick);
				continue;
			    }
			sendto_match_butone(IsServer(cptr) ? cptr : NULL, 
					    sptr, nick + 1,
					    (*nick == '#') ? MATCH_HOST :
							     MATCH_SERVER,
					    ":%s %s %s :%s", parv[0],
					    cmd, nick, parv[2]);
			continue;
		    }
	
		/*
		** user[%host]@server addressed?
		*/


		server = (char *)index(nick, '@');
		acptr = NULL;
		if (server)
		    acptr = find_server(server + 1, NULL);		

		if (!acptr && server && *server) {
			if (server[1] == '\0' || !myncmp(server+1, "services", 8))
			    acptr = find_server_const(SERVICES_NAME, NULL);
			else
			    acptr = NULL;
		}
		
		if ((acptr && server))
		    {
			int count = 0;

			/*
			** Not destined for a user on me :-(
			*/
			if (acptr && !IsMe(acptr))
			    {
				sendto_one(acptr,":%s %s %s :%s", parv[0],
					   cmd, nick, parv[2]);
				continue;
			    }
			*server = '\0';
			if ((host = (char *)index(nick, '%')))
				*host++ = '\0';

			/*
			** Look for users which match the destination host
			** (no host == wildcard) and if one and one only is
			** found connected to me, deliver message!
			*/
			acptr = find_userhost(nick, host, NULL, &count);
			if (acptr && !IsULine(acptr, acptr))
				acptr = NULL;
			if (server)
				*server = '@';
			if (host)
				*--host = '%';
			if (acptr)
			    {
				if (count == 1)
					sendto_prefix_one(acptr, sptr,
							  ":%s %s %s :%s",
					 		  parv[0], cmd,
							  nick, parv[2]);
				else if (!notice)
					sendto_one(sptr,
						   err_str(ERR_TOOMANYTARGETS),
						   me.name, parv[0], nick);
			    }
			if (acptr)
				continue;
		    }
		sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name,
			   parv[0], nick);
            }
    return 0;
}

/*
** m_private
**	parv[0] = sender prefix
**	parv[1] = receiver list
**	parv[2] = message text
*/

int m_private(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	return m_message(cptr, sptr, parc, parv, 0);
}

/*
** m_notice
**	parv[0] = sender prefix
**	parv[1] = receiver list
**	parv[2] = notice text
*/

int m_notice(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	if (!IsRegistered(cptr) && (cptr->name[0]) && !IsNotSpoof(cptr))
	{
		if (BadPtr(parv[1])) return 0;
#ifdef NOSPOOF
		if (strtoul(parv[1], NULL, 16) != cptr->nospoof)
			goto temp;
		sptr->nospoof = 0;
#endif
		if (sptr->user && sptr->name[0])
			return register_user(cptr, sptr, sptr->name,
				sptr->user->username);
		return 0;
	}
	temp:
	return m_message(cptr, sptr, parc, parv, 1);
}

static void do_who(aClient *sptr, aClient *acptr, aChannel *repchan)
{
	char	status[5];
	int	i = 0;

	if (acptr->user->away)
		status[i++] = 'G';
	else
		status[i++] = 'H';
	if (IsAnOper(acptr))
		status[i++] = '*';
	else if (IsInvisible(acptr) && sptr != acptr && IsAnOper(sptr))
		status[i++] = '%';
	if (repchan && is_chan_op(acptr, repchan))
		status[i++] = '@';
	else if (repchan && has_voice(acptr, repchan))
		status[i++] = '+';
	else if (repchan && is_zombie(acptr, repchan))
		status[i++] = '!';
	status[i] = '\0';
	sendto_one(sptr, rpl_str(RPL_WHOREPLY), me.name, sptr->name,
		   (repchan) ? (repchan->chname) : "*", acptr->user->username,
		   UGETHOST(sptr, acptr->user), acptr->user->server, acptr->name,
		   status, acptr->hopcount, acptr->info);
}


/*
** m_who
**	parv[0] = sender prefix
**	parv[1] = nickname mask list
**	parv[2] = additional selection flag, only 'o' for now.
*/
int m_who(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient *acptr;
	char	*mask = parc > 1 ? parv[1] : NULL;
	Link	*lp, *mem_lp;
	aChannel *chptr, *mychannel;
	char	*channame = NULL, *s;
	int	oper = parc > 2 ? (*parv[2] == 'o' ): 0; /* Show OPERS only */
	int	member;
        char everyone[5] = "0";

	if (check_registered_user(sptr))
		return 0;
	if (IsHurt(sptr) && sptr->hurt)
	{
	      if (sptr->hurt == 3)
		{
			sendto_one(sptr, ":%s NOTICE %s :Sorry, but as an autohurt user, you cannot use the /who command.", me.name, parv[0]);
			sendto_one(sptr, rpl_str(RPL_ENDOFWHO), me.name, parv[0],
				   BadPtr(mask) ?  "*" : mask);
			return;
		}
		if (BadPtr(mask) || !oper)
                {
                        sendto_one(sptr, ":%s NOTICE %s :NOTE: as a hurt user you can only list irc operators", me.name, parv[0]);
                        mask = everyone;
                        oper = 1;
                }
	}

	if (!BadPtr(mask))
	    {
		if ((s = (char *)index(mask, ',')))
		    {
			parv[1] = ++s;
			(void)m_who(cptr, sptr, parc, parv);
		    }
		clean_channelname(mask);
	    }

	mychannel = NullChn;
	if (sptr->user)
		if ((lp = sptr->user->channel))
			mychannel = lp->value.chptr;

	/* Allow use of m_who without registering */
	
	/*
	**  Following code is some ugly hacking to preserve the
	**  functions of the old implementation. (Also, people
	**  will complain when they try to use masks like "12tes*"
	**  and get people on channel 12 ;) --msa
	*/
	if (!mask || *mask == '\0')
		mask = NULL;
	else if (mask[1] == '\0' && mask[0] == '*')
	    {
		mask = NULL;
		if (mychannel)
			channame = mychannel->chname;
	    }
	else if (mask[1] == '\0' && mask[0] == '0') /* "WHO 0" for irc.el */
		mask = NULL;
	else
		channame = mask;
	(void)collapse(mask);

	if (IsChannelName(channame))
	    {
		/*
		 * List all users on a given channel
		 */
		chptr = find_channel(channame, NULL);
		if (chptr)
		  {
		    mem_lp = find_user_link(chptr->members, sptr);
		    member = mem_lp ? 1 : 0;

                    if (!IsAnOper(sptr) && IsPlusChan(chptr) && mem_lp
                        && !(mem_lp->flags & (CHFL_CHANOP)))
                             ;
		    else if (member || !SecretChannel(chptr))
			for (lp = chptr->members; lp; lp = lp->next)
			    {
				if (oper && !IsAnOper(lp->value.cptr))
					continue;
				if (lp->value.cptr!=sptr &&
				    (lp->flags & CHFL_ZOMBIE))
					continue;
				if (lp->value.cptr!=sptr && IsInvisible(lp->value.cptr) && !member)
					continue;
				do_who(sptr, lp->value.cptr, chptr);
			    }
		  }
	    }
	else for (acptr = client; acptr; acptr = acptr->next)
	    {
		aChannel *ch2ptr = NULL;
		int	showperson, isinvis;

		if (!IsPerson(acptr))
			continue;
		if (oper && !IsAnOper(acptr))
			continue;
		showperson = 0;
		/*
		 * Show user if they are on the same channel, or not
		 * invisible and on a non secret channel (if any).
		 * Do this before brute force match on all relevant fields
		 * since these are less cpu intensive (I hope :-) and should
		 * provide better/more shortcuts - avalon
		 */
		isinvis = acptr!=sptr && IsInvisible(acptr) && !IsAnOper(sptr);
		for (lp = acptr->user->channel; lp; lp = lp->next)
		    {
			chptr = lp->value.chptr;
			member = IsMember(sptr, chptr);
			if (isinvis && !member)
				continue;
			if (is_zombie(acptr, chptr))
				continue;
			if (IsAnOper(sptr)) showperson = 1;
			if (member || (!isinvis && 
				ShowChannel(sptr, chptr)))
			    {
				ch2ptr = chptr;
				showperson = 1;
				break;
			    }
			if (HiddenChannel(chptr) && !SecretChannel(chptr) &&
			    !isinvis)
				showperson = 1;
		    }
		if (!acptr->user->channel && !isinvis)
			showperson = 1;
		/*
		** This is brute force solution, not efficient...? ;( 
		** Show entry, if no mask or any of the fields match
		** the mask. --msa
		*/
		if (showperson &&
		    (!mask ||
		     match(mask, acptr->name) == 0 ||
		     match(mask, acptr->user->username) == 0 ||
		     match(mask, UGETHOST(sptr, acptr->user)) == 0 ||
		     match(mask, acptr->user->server) == 0 ||
		     match(mask, acptr->info) == 0))
			do_who(sptr, acptr, ch2ptr);
	    }
	sendto_one(sptr, rpl_str(RPL_ENDOFWHO), me.name, parv[0],
		   BadPtr(mask) ?  "*" : mask);
	return 0;
}

/*
 * This function checks to see if a CTCP message (other than ACTION) is
 * contained in the passed string.  This might seem easier than I am
 * doing it, but a CTCP message can be changed together, even after a
 * normal message.
 *
 * If the message is found, and it's a DCC message, pass it back in
 * *dccptr.
 *
 *
 * Unfortunately, this makes for a bit of extra processing in the
 * server.
 */

int check_for_ctcp(char *str, char **dccptr)
{
   char       *p = str;

   while ((p = index(p, 1)) != NULL)
   {
      if (myncmp(++p, "DCC", 3) == 0)
      {
         if(dccptr)
            *dccptr = p;
         if(myncmp(p+3, " SEND", 5) == 0)
            return CTCP_DCCSEND;
         else
            return CTCP_DCC;
      }
      if (myncmp(++p, "ACTION", 6) != 0)
         return CTCP_YES;
      if ((p = index(p, 1)) == NULL)
         return CTCP_NONE;
      if(!(*(++p)))
         break;;
   }
   return CTCP_NONE;
}


/*
 * Shadowfax's FLUD code
 */

#ifdef FLUD

void
announce_fluder(aClient *fluder,        /*
                                         * fluder, client being fluded
                                         */
                aClient *cptr,
                aChannel *chptr,        /*
                                         * channel being fluded
                                         */
                int type)
{                               /*
                                 * for future use
                                 */
   char       *fludee;


   if (cptr)
      fludee = cptr->name;
   else
      fludee = chptr->chname;

   sendto_umode(UMODE_FLOOD, "Flooder %s [%s@%s] on %s target: %s",
             fluder->name, fluder->user->username, fluder->user->host,
                  fluder->user->server, fludee);
}

/*
 * This is really just a "convenience" function.  I can only keep three
 * or * four levels of pointer dereferencing straight in my head.  This
 * remove * an entry in a fluders list.  Use this when working on a
 * fludees list :)
 */
struct fludbot *
remove_fluder_reference(struct fludbot **fluders,
                        aClient *fluder)
{
   struct fludbot *current, *prev, *next;

   prev = NULL;
   current = *fluders;
   while (current) {
      next = current->next;
      if (current->fluder == fluder) {
         if (prev)
            prev->next = next;
         else
            *fluders = next;

         BlockHeapFree(free_fludbots, current);
      }
      else
         prev = current;
      current = next;
   }

   return (*fluders);
}

/*
 * Another function to unravel my mind.
 */
Link       *
remove_fludee_reference(Link **fludees, void *fludee)
{
   Link       *current, *prev, *next;

   prev = NULL;
   current = *fludees;
   while (current) {
      next = current->next;
      if (current->value.cptr == (aClient *) fludee) {
         if (prev)
            prev->next = next;
         else
            *fludees = next;

         BlockHeapFree(free_Links, current);
      }
      else
         prev = current;
      current = next;
   }

   return (*fludees);
}

int
check_for_fludblock(aClient *fluder,    /*
                                         * fluder being fluded
                                         */
                    aClient *cptr,      /*
                                         * client being fluded
                                         */
                    aChannel *chptr,    /*
                                         * channel being fluded
                                         */
                    int type)
{                               /*
                                 * for future use
                                 */
   time_t      now;
   int         blocking;

   /*
    * If it's disabled, we don't need to process all of this
    */
   if (flud_block == 0)
      return 0;

   /*
    * It's either got to be a client or a channel being fluded
    */
   if ((cptr == NULL) && (chptr == NULL))
      return 0;

   if (cptr && !MyFludConnect(cptr)) {
      sendto_ops("check_for_fludblock() called for non-local client");
      return 0;
   }

   /*
    * Are we blocking fluds at this moment?
    */
   time(&now);
   if (cptr)
      blocking = (cptr->fludblock > (now - flud_block));
   else
      blocking = (chptr->fludblock > (now - flud_block));

   return (blocking);
}

int
check_for_flud(aClient *fluder, /*
                                 * fluder, client being fluded
                                 */
               aClient *cptr,
               aChannel *chptr, /*
                                 * channel being fluded
                                 */
               int type)
{                               /*
                                 * for future use
                                 */
   time_t      now;
   struct fludbot *current, *prev, *next;
   int         blocking, count, found;
   Link       *newfludee;

   /*
    * If it's disabled, we don't need to process all of this
    */
   if (flud_block == 0)
          return 0;

   /*
    * It's either got to be a client or a channel being fluded
    */
   if ((cptr == NULL) && (chptr == NULL))
          return 0;

   if (cptr && !MyFludConnect(cptr)) {
      sendto_ops("check_for_flud() called for non-local client");
      return 0;
   }

   /*
    * Are we blocking fluds at this moment?
    */
   time(&now);
   if (cptr)
          blocking = (cptr->fludblock > (now - flud_block));
   else
          blocking = (chptr->fludblock > (now - flud_block));


   /*
    * Collect the Garbage
    */
   if (!blocking) {
      if (cptr)
                  current = cptr->fluders;
      else
                  current = chptr->fluders;
      prev = NULL;
      while (current) {
                        next = current->next;
                        if (current->last_msg < (now - flud_time)) {
                                if (cptr)
                                  remove_fludee_reference(&current->fluder->fludees, (void *) cptr);
				else
                                  remove_fludee_reference(&current->fluder->fludees,  (void *) chptr);

                                if (prev)
                                  prev->next = current->next;
                                else if (cptr)
                                  cptr->fluders = current->next;
                                else
                                  chptr->fluders = current->next;
                                BlockHeapFree(free_fludbots, current);
                        }

                          prev = current;
                        current = next;
      }
   }
   /*
    * Find or create the structure for the fluder, and update the
    * counter * and last_msg members.  Also make a running total count
    */
   if (cptr)
          current = cptr->fluders;
   else
          current = chptr->fluders;
   count = found = 0;
   while (current) {
      if (current->fluder == fluder) {
                        current->last_msg = now;
                        current->count++;
                        found = 1;
      }
      if (current->first_msg < (now - flud_time))
                  count++;
      else
                  count += current->count;
      current = current->next;
   }
   if (!found) {
                if ((current = BlockHeapALLOC(free_fludbots, struct fludbot)) != NULL) {
                        current->fluder = fluder;
                        current->count = 1;
                        current->first_msg = now;
                        current->last_msg = now;
                        if (cptr) {
                                current->next = cptr->fluders;
                                cptr->fluders = current;
                        }
                        else {
                                current->next = chptr->fluders;
                                chptr->fluders = current;
                        }

                        count++;
                        if ((newfludee = BlockHeapALLOC(free_Links, Link)) != NULL) {
                                if (cptr) {
                                        newfludee->flags = 0;
                                        newfludee->value.cptr = cptr;
                                }
                                else {
                                        newfludee->flags = 1;
                                        newfludee->value.chptr = chptr;
                                }
                                newfludee->next = fluder->fludees;
                                fluder->fludees = newfludee;
                        }
                        else
                          outofmemory();
                        /*
                         * If we are already blocking now, we should go ahead * and
                         * announce the new arrival
                         */
                        if (blocking)
                          announce_fluder(fluder, cptr, chptr, type);
                }
                else
                  outofmemory();
   }
   /*
    * Okay, if we are not blocking, we need to decide if it's time to *
    * begin doing so.  We already have a count of messages received in *
    * the last flud_time seconds
    */

   if (!blocking && (count > flud_num)) {
      blocking = 1;
      ircstp->is_flud++;
      /*
       * if we are going to say anything to the fludee, now is the *
       * time to mention it to them.
       */
      if (cptr)
                  sendto_one(cptr,
                                                 ":%s NOTICE %s :*** Notice -- Server flood protection activated for %s",
                                                 me.name, cptr->name, cptr->name);
      else
                  sendto_channel_butserv(chptr, &me, ":%s NOTICE %s :*** Notice -- Server flood protection activated for %s",
         me.name, chptr->chname, chptr->chname);

      /*
       * Here we should go back through the existing list of * fluders
       * and announce that they were part of the game as * well.
       */
      if (cptr)
                  current = cptr->fluders;
      else
                  current = chptr->fluders;
      while (current) {
                        announce_fluder(current->fluder, cptr, chptr, type);
                        current = current->next;
      }
   }
   /*
    * update blocking timestamp, since we received a/another CTCP
    * message
    */
   if (blocking) {
      if (cptr)
                  cptr->fludblock = now;
      else
                  chptr->fludblock = now;
   }

   return (blocking);
}

void
free_fluders(aClient *cptr, aChannel *chptr)
{
   struct fludbot *fluders, *next;

   if ((cptr == NULL) && (chptr == NULL)) {
      sendto_ops("free_fluders(NULL, NULL)");
      return;
   }

   if (cptr && !MyFludConnect(cptr))

   if (cptr)
      fluders = cptr->fluders;
   else
      fluders = chptr->fluders;

   while (fluders) {
      next = fluders->next;

      if (cptr)
         remove_fludee_reference(&fluders->fluder->fludees, (void *) cptr);
      else
         remove_fludee_reference(&fluders->fluder->fludees, (void *) chptr);

      BlockHeapFree(free_fludbots, fluders);
      fluders = next;
   }
}

void
free_fludees(aClient *badguy)
{
   Link       *fludees, *next;

   if (badguy == NULL) {
      sendto_ops("free_fludees(NULL)");
      return;
   }
   fludees = badguy->fludees;
   while (fludees) {
      next = fludees->next;

      if (fludees->flags)
         remove_fluder_reference(&fludees->value.chptr->fluders, badguy);
      else {
         if (!MyFludConnect(fludees->value.cptr))
            sendto_ops("free_fludees() encountered non-local client");
         else
            remove_fluder_reference(&fludees->value.cptr->fluders, badguy);
      }

      BlockHeapFree(free_Links, fludees);
      fludees = next;
   }
}
#endif /*
        * FLUD
        */




/*
** m_whois
**	parv[0] = sender prefix
**	parv[1] = nickname masklist
*/
int m_whois(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	static anUser UnknownUser =
	    {
		NULL,	/* nextu */
		NULL,	/* channel */
		NULL,   /* invited */
		NULL,	/* silence */
		NULL,   /* dccallow */
		NULL,	/* away */
		NULL,   /* mask */
		0,	/* last */
		1,      /* refcount */
		0,	/* joined */
		"<Unknown>",	/* username */
		"<Unknown>",	/* host */
		"<Unknown>"	/* server */
	    };
	Link	*lp, *lp2;
	anUser	*user;
	aClient *acptr, *a2cptr;
	aChannel *chptr;
	char	*nick, *tmp, *name, *temp;
	char	*p = NULL;
	int	found, len, mlen, t;

	if (check_registered_user(sptr))
		return 0;

    	if (parc < 2)
	    {
		sendto_one(sptr, err_str(ERR_NONICKNAMEGIVEN),
			   me.name, parv[0]);
		return 0;
	    }

	if (parc > 2)
	    {
		if (hunt_server(cptr,sptr,":%s WHOIS %s :%s", 1,parc,parv) !=
		    HUNTED_ISME)
			return 0;
		parv[1] = parv[2];
	    }

	for (tmp = parv[1]; (nick = strtoken(&p, tmp, ",")); tmp = NULL)
	    {
		int	invis, showperson, member, wilds;

		found = 0;
		(void)collapse(nick);
		wilds = (index(nick, '?') || index(nick, '*'));
		if (wilds && IsServer(cptr)) continue;
		for (acptr = client; (acptr = next_client(acptr, nick));
		     acptr = acptr->next)
		    {
			if (IsServer(acptr))
				continue;
			/*
			 * I'm always last :-) and acptr->next == NULL!!
			 */
			if (IsMe(acptr))
				break;
			/*
			 * 'Rules' established for sending a WHOIS reply:
			 *
			 * - only allow a remote client to get replies for
			 *   local clients if wildcards are being used;
			 *
			 * - if wildcards are being used dont send a reply if
			 *   the querier isnt any common channels and the
			 *   client in question is invisible and wildcards are
			 *   in use (allow exact matches only);
			 *
			 * - only send replies about common or public channels
			 *   the target user(s) are on;
			 */
			if (!MyConnect(sptr) && !MyConnect(acptr) && wilds)
				continue;
			user = acptr->user ? acptr->user : &UnknownUser;
			name = (!*acptr->name) ? "?" : acptr->name;

			invis = acptr!=sptr && IsInvisible(acptr);
			member = (user->channel) ? 1 : 0;
			showperson = (wilds && !invis && !member) || !wilds;
			for (lp = user->channel; lp; lp = lp->next)
			    {
				chptr = lp->value.chptr;
				member = IsMember(sptr, chptr);
				if (invis && !member)
					continue;
				if (is_zombie(acptr, chptr))
					continue;
				if (member || (!invis && PubChannel(chptr)))
				    {
					showperson = 1;
					break;
				    }
				if (!invis && HiddenChannel(chptr) &&
				    !SecretChannel(chptr))
					showperson = 1;
			    }
			if (!showperson)
				continue;

			a2cptr = find_server(user->server, NULL);

			/* RPL_WHOISUSER used to be at this point */
			found = 1;
			mlen = strlen(me.name) + strlen(parv[0]) + 6 +
				strlen(name);
			for (len = 0, *buf = '\0', lp = user->channel; lp;
			     lp = lp->next)
			    {
				chptr = lp->value.chptr;
				lp2 = find_user_link(chptr->members, sptr);

				if ((PubChannel(chptr) || lp2) &&
				    (acptr==sptr || !is_zombie(acptr, chptr)))
				    {
					if (len + strlen(chptr->chname)
                                            > (size_t) BUFSIZE - 4 - mlen)
					    {
						sendto_one(sptr,
							   ":%s %d %s %s :%s",
							   me.name,
							   RPL_WHOISCHANNELS,
							   parv[0], name, buf);
						*buf = '\0';
						len = 0;
					    }
					if (is_chan_op(acptr, chptr))
						*(buf + len++) = '@';
					else if (has_voice(acptr, chptr))
						*(buf + len++) = '+';
 					else if (is_zombie(acptr, chptr))
						*(buf + len++) = '!';
					if (len)
						*(buf + len) = '\0';
					(void)strcpy(buf + len, chptr->chname);
					len += strlen(chptr->chname);
					(void)strcat(buf + len, " ");
					len++;
				    }
			    }

			sendto_one(sptr, rpl_str(RPL_WHOISUSER), me.name,
				   parv[0], name,
				   user->username, UGETHOST(sptr, user), acptr->info);

			if (buf[0] != '\0')
				sendto_one(sptr, rpl_str(RPL_WHOISCHANNELS),
					   me.name, parv[0], name, buf);

			sendto_one(sptr, rpl_str(RPL_WHOISSERVER),
				   me.name, parv[0], name, user->server,
				   a2cptr?a2cptr->info:"*Not On This Net*");

			if (user->away)
				sendto_one(sptr, rpl_str(RPL_AWAY), me.name,
					   parv[0], name, user->away);
			if (acptr->user && acptr->user->mask && (sptr == acptr || IsAnOper(sptr)))
				sendto_one(sptr, rpl_str(RPL_WHOISMASKED),
					   me.name, parv[0], name, acptr->user->mask);

			if (IsAnOper(acptr))
				sendto_one(sptr, rpl_str(RPL_WHOISOPERATOR),
					   me.name, parv[0], name);

			if (IsHelpOp(acptr) && IsPerson(acptr) && !acptr->user->away
                            && !IS_SET(ClientUmode(acptr), U_INVISIBLE))
				sendto_one(sptr, rpl_str(RPL_WHOISHELPOP),
					   me.name, parv[0], name);


			if (IsHurt(acptr) && !check_hurt(acptr))
			{
				sendto_one(sptr, rpl_str(RPL_WHOISHURT),
					   me.name, parv[0], name);
			}
			if (acptr->user && MyConnect(acptr))
				sendto_one(sptr, rpl_str(RPL_WHOISIDLE),
					   me.name, parv[0], name,
					   NOW - user->last,
					   acptr->firsttime);
		    }
		if (!found)
			sendto_one(sptr, err_str(ERR_NOSUCHNICK),
				   me.name, parv[0], nick);
		if (p)
			p[-1] = ',';
	    }
	sendto_one(sptr, rpl_str(RPL_ENDOFWHOIS), me.name, parv[0], parv[1]);

	return 0;
}

/*
** m_user
**	parv[0] = sender prefix
**	parv[1] = username (login name, account)
**	parv[2] = client host name (used only from other servers)
**	parv[3] = server host name (used only from other servers)
**	parv[4] = users real name info
**
** NEW protocol:
**      parv[0] nickname
**      parv[1] username
**      parv[2] hostname
**      parv[3] server host name
**      parv[4] ip address (base64)
**      parv[5] usermode
**      parv[6] cookie A
**      parv[7] cookie B
**      parv[8] info
*/
int m_user(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
#define	UFLAGS	(U_DCCALLOW)
	char	*username, *host, *server, *realname;
	anUser	*user;
 
	if (IsServer(cptr) && !IsUnknown(sptr))
		return 0;

	/* Stop clients putting '@' into their username */
	if (parc > 2 && (username = (char *)index(parv[1],'@')))
		*username = '\0'; 

	if (parc < 5 || *parv[1] == '\0' || *parv[2] == '\0' ||
	    *parv[3] == '\0' || *parv[4] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
	    		   me.name, parv[0], "USER");
		if (IsServer(cptr))
			sendto_ops("bad USER param count for %s from %s",
				   parv[0], get_client_name(cptr, FALSE));
		else
			return 0;
	    }

	/* Copy parameters into better documenting variables */

	username = (parc < 2 || BadPtr(parv[1])) ? "<bad-boy>" : parv[1];
	host     = (parc < 3 || BadPtr(parv[2])) ? "<nohost>" : parv[2];
	server   = (parc < 4 || BadPtr(parv[3])) ? "<noserver>" : parv[3];
	if (!IsServer(cptr) || !IS_SET(cptr->cap, CAP_USERIP))
		realname = (parc < 5 || BadPtr(parv[4])) ? "<bad-realname>" : parv[4];
	else
		realname = (parc < 9 || BadPtr(parv[8])) ? "<bad-realname>" : parv[8];

 	user = make_user(sptr);

	if ((parc >= 9) && IsServer(cptr) && IS_SET(cptr->cap, CAP_USERIP)) {
	    unsigned long parse_umode(char *string);
            long u_modes = parse_umode(parv[5]);

	    sptr->ip.s_addr = htonl(base64toint(parv[4]));
	    ClientUmode(sptr) = u_modes;
	    user->auth_a = base64toint(parv[6]);
	    user->auth_b = base64toint(parv[7]);
	}
	else if (!MyConnect(sptr))
		sptr->ip.s_addr = 0;

	if (!MyConnect(sptr))
	    {
		strncpyzt(user->server, server, sizeof(user->server));
		strncpyzt(user->host, host, sizeof(user->host));
		goto user_finish;
	    }

	if (!IsUnknown(sptr))
	    {
		sendto_one(sptr, err_str(ERR_ALREADYREGISTRED),
			   me.name, parv[0]);
		return 0;
	    }
#ifndef	NO_DEFAULT_INVISIBLE
	ClientUmode(sptr) |= U_INVISIBLE;
#endif

	/* sptr->flags |= (UFLAGS & atoi(host)); 
             call me crazy, but isn't &'ing host stuff a bit weird ? */
	ClientUmode(sptr) |= (UFLAGS); 
	strncpyzt(user->host, host, sizeof(user->host));
	strncpyzt(user->server, me.name, sizeof(user->server));

user_finish:
	strncpyzt(sptr->info, realname, sizeof(sptr->info));
	if (sptr->name[0] && (IsServer(cptr) ? 1 : IsNotSpoof(sptr)))
	/* NICK and no-spoof already received, now we have USER... */
		return register_user(cptr, sptr, sptr->name, username);
	else
		strncpyzt(sptr->user->username, username, USERLEN+1);
	return 0;
}

/*
** m_quit
**	parv[0] = sender prefix
**	parv[1] = comment
*/
int	m_quit(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
        char *comment = (parc > 1 && parv[1]) ? parv[1] : cptr->name;
        char reason[512+2] = "";

        if (comment && !(*comment))
             comment = parv[0];

        if (MyClient(sptr) && comment && *comment)
        { 
           strcat(reason,"Quit: "); 
           strncat(reason, comment, TOPICLEN);
           comment = &reason[0]; 
        }

	if (strlen(comment) > (size_t) TOPICLEN)
		comment[TOPICLEN] = '\0';
	return IsServer(sptr) ? 0 : exit_client(cptr, sptr, sptr, comment);
}


/*
** m_hurtset
**      parv[0] = sender prefix
**      parv[1] = hurt victim
**      parv[2] = hurt time
**      parv[3] = reason
*/
int m_hurtset(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
   aClient *acptr;

   if (!IsServer(cptr))
       return 0;
   if (parc < 3)
       return;
   if (!( acptr = find_person(parv[1], NULL)))
       return;
   if (!MyConnect(acptr)) {
       sendto_serv_butone(cptr, ":%s HURTSET %s %s", parv[0], parv[1], parv[2]);
       return;
   }
   SET_BIT(ClientFlags(acptr), FLAGS_HURT);
   set_hurt(acptr, parv[0], 0);
   if (isdigit(*parv[2]))
       acptr->hurt = atoi(parv[2]);
   else if (*parv[2] == '-')
   {
       remove_hurt(acptr);
       acptr->hurt = 0;
   }
   return 0;	
}

/*
** m_hurt
**      parv[0] = sender prefix
**      parv[1] = hurt victim
**      parv[2] = hurt time
**      parv[3] = reason
*/
int m_hurt(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	char	*user, *path, *nick, *host, *p, *ph, *pav[6]; 
	char	ufield[USERLEN+5] = "", hurt_list[1024+10] = "";
	aClient	*acptr, *bcptr;
	time_t  hurttime=time(NULL);
	int	chasing, hurtl, hurt2, hurtct, multiple, buffleft, need_comma = 0;
        int     addht=0;
	int	i = 0, l = 0, tinf = 0;
        static int startstat=0;
	
	addht=0;
	need_comma = hurtct = multiple = chasing = 0;
	buffleft=200; /* nothing too large... */

#define SIL_FMT ":%s NOTICE %s :ERROR: Silencing link: %s (%s%s (%s))"
	if ( ( !IsPrivileged(cptr) || (MyClient(cptr) && IsPerson(cptr) && 
	     !IsAnOper(cptr) ) ) )
	   {
	        sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
	        return 0;
	    }
	
	if (check_registered(sptr))   
	        return 0;
	
	if (parc == 2)
	{ /* special case: report the user's hurt time */
	      nick = parv[1];
              if (acptr = find_person(parv[1], NULL))
		{
			if (hunt_server(cptr,sptr,":%s HURT %s", 1,parc,parv) !=
			      HUNTED_ISME)
			            return 0;
                        else
                        {
			    if (acptr->hurt == 0 || (acptr->hurt < NOW && acptr->hurt > 5)) 
			              remove_hurt(acptr);
			    if (!IsHurt(acptr))
				sendto_one(sptr, ":%s NOTICE %s :User %s is not hurt.", me.name, parv[0], acptr->name);
			    else
			    {
			      if (acptr->hurt > 5 || acptr->hurt < 1)
				sendto_one(sptr, ":%s NOTICE %s :User %s will be hurt for %lu more seconds.", me.name, parv[0], acptr->name, acptr->hurt - NOW);
			      else if (acptr->hurt > 0 && acptr->hurt < 5)
				sendto_one(sptr, ":%s NOTICE %s :User %s will be hurt until an oper invokes HEAL.", me.name, parv[0], acptr->name);
			    }
			}
                       return 0;
		}
		else
		{
			sendto_one(sptr, err_str(ERR_NOSUCHNICK),
				me.name, parv[0], parv[1]);
				return 0;		
		}
		return 0;		
	} else if (parc < 2 && MyClient(sptr) && IsAnOper(sptr))
	{
		sendto_one(sptr, ":%s NOTICE %s :\02HURT usage\02", me.name, parv[0]);
		sendto_one(sptr, ":%s NOTICE %s :1) /hurt <nick> <duration> <reason>", me.name, parv[0]);
		sendto_one(sptr, ":%s NOTICE %s :2) /hurt #<machinename> <duration> <reason>", me.name, parv[0]);
		sendto_one(sptr, ":%s NOTICE %s :3) /hurt {nick1,nick2,...} <duration> <reason>", me.name, parv[0]);
		sendto_one(sptr, ":%s NOTICE %s :4) /hurt <nick>      [gets information about a user's hurt status]", me.name, parv[0]);
		sendto_one(sptr, ":%s NOTICE %s :--------------------------------------------", me.name, parv[0]);
		sendto_one(sptr, ":%s NOTICE %s :ex: /hurt badnick 1m15s flooding ", me.name, parv[0]);
		sendto_one(sptr, ":%s NOTICE %s :    /hurt #ppp5.bad.org 5m floodingclones", me.name, parv[0]);
		sendto_one(sptr, ":%s NOTICE %s :    /hurt clone1,clone2 30s clones", me.name, parv[0]);
		sendto_one(sptr, ":%s NOTICE %s :END", me.name, parv[0]);
		return 0;
	}

	if (parc < 4 || *parv[1] == '\0')
	{
	     sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
	               me.name, parv[0], "HURT");
	     return 0;
	}

	if (isdigit(*parv[2])) /* evil hack to support 2h1m5s format. */
	{
		 l = strlen(parv[2]);
		 tinf = 0;
		 addht = 0;
		 hurtl = 0;
		 if (l > 20) l = 20;
                 hurtl = 0; /* at this point in time, hurtl is used as a number buffer */
		 for(i=0;i<l;i++)
		 {
			if (isdigit(parv[2][i]))
			{
				if (hurtl) addht += hurtl;
				hurtl = atol(&parv[2][i]);
				while(isdigit(parv[2][i]))
					i++;
				i--;
				continue;
			}
			else
			{
			 if (tinf++ < 3) /* only allow 3 symbolic letters to be processed per command */
			    switch(parv[2][i])
			    {
				case 's': case 'S':
					addht += hurtl;
					hurtl = 0;
					break;
				case 'm': case 'M':
					addht += hurtl*60;
					hurtl = 0;
					break;
				case 'h': case 'H':
					addht += hurtl*60*60;
					hurtl = 0;
					break;
				default:
					addht += hurtl;
					hurtl = 0;
					break;
			    }
			}
		 }

		 if (hurtl) addht += hurtl;
		 /*if (addht == 0) addht = 1;*/
                 hurtl = addht;
	}
	else
	{
	    sendto_one(sptr,"NOTICE %s :HURTTIME must be a _numeric_ value",sptr->name);
	    return 0;
	}
#if 0 /*  enable -true- permanent hurting.
       *  if a remote host sends 30000 everything's fine so no risk of desync.
       *  if we send 0, they'll interpret it as 30000 though.
       */
             if (addht==0) {addht=30000;}
#endif
                

#ifdef DEBUGMODE
		Debug((11, "HURT: %s %s %s (%s)", parv[0],parv[1], parv[2], parv[3] ));
#endif
	user = parv[1];
	path = parv[2]; /* Either defined or NULL (parc >= 2!!) */
	
	if (index(user,','))
	    multiple = 1;
	else
	    multiple = 0;
	
	for (p = NULL, hurtct = 0, nick = strtoken(&p, user, ","); nick &&
	    ((IsServer(cptr)&&hurtct<50)||hurtct <= MAXHURTS);
	    nick = strtoken(&p, NULL, ","), hurtct++)
	{
	    if (!nick) break;
	    if (!multiple)
	    if (IsPrivileged(sptr) && (index(nick,'#') == nick) && *(nick+1))
	    {
	        bcptr = acptr = NULL;
	        if (ph = index((nick+1),'@'))
	        {
	            strncpy(ufield, nick+1, ((ph - (nick+1)) < USERLEN ? (ph - (nick+1)) : (USERLEN)));
	            ufield[(ph - (nick+1) < USERLEN) ? ph - (nick+1) : USERLEN] = '\0';
	        } else
	        ufield[0] = '\0';
	        host = nick + (ufield[0] ? 2 : 1 )  + strlen(ufield);
	       if (MyConnect(sptr) && !IsServer(sptr) && addht > 30000 || addht < 0)
	      { /* > 30000 will be possible if all servers are ok with it */
	          sendto_one(sptr,"NOTICE %s :Hurt times may not exceed 30000 and must be at least 0",sptr->name);
	          return 0;
	      }

            for (bcptr = client; bcptr; bcptr = bcptr->next)
            {
                if (!bcptr) break;
                if (!IsPerson(bcptr)) 
                   continue;
                if (mycmp(bcptr->user->host, host) == 0 
                    && (ufield[0] == '\0'
                    || (match(ufield, bcptr->user->username) == 0) ) )
                {
                    if (MyClient(bcptr) && !IsAnOper(bcptr))
                    {
                       bcptr->hurt = hurttime+addht;
                       if (!IsServer(sptr)) set_hurt(bcptr, parv[0], 0);
                       else SetHurt(bcptr);
                       sendto_one(bcptr, SIL_FMT, me.name, bcptr->name,
                              get_client_name(bcptr, FALSE), 
                              (MyClient(sptr)&&MyClient(bcptr)?"Local hurt by ":"hurt: "),
                              sptr->name,
                              parv[3] );


                    }
                      /*         m_hurt() no longer called here recursively.   */

                }
	   }
	if (addht)
        sendto_realops("Received Hurt Message for %s from %s (%ld seconds) (%s)",
                    parv[1], parv[0], addht, parv[3]);   
	else
        sendto_realops("Received Hurt Message for %s from %s (forever) (%s)",
                    parv[1], parv[0], parv[3]);   
        sendto_serv_butone(cptr,":%s HURT %s %ld :%s",
                          parv[0],parv[1],addht,parv[3]);

			/* function to insert temp conf/data for hurt goes here */
            acptr = NULL;
             return 0;
          }

             acptr=find_client(nick,NULL);

          if (!(acptr))
          {
            if (!(acptr = get_history(nick, (long)KILLCHASETIMELIMIT )))
            {
                if (IsPerson(sptr))
                {
                     sendto_one(sptr, err_str(ERR_NOSUCHNICK),
                                me.name, parv[0], nick);
                }
                /*return(0);*/
                continue;
            }
            else
              chasing = 1;
          }  
	if (!acptr || !sptr || !cptr)  
	    continue;


         /*  this is up to the admin, but if they want,
          *  locops can't hurt non-local clients under the
          *  same rules as kill;>
          */
#ifdef STRICT_LOCOPS
        if ( sptr == cptr && IsPerson(sptr) && !IsOper(sptr) 
             && !MyClient(acptr) )
        {
                sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
                continue;
        }
#endif
	/* 1> stop local opers from using /hurt on an oper
	   2> stop remote locops [people whose O's arent propogated] from using 
	     /hurt on an oper local to this server, no matter what. */

	if ((MyClient(sptr)||(!IsOper(sptr)&&MyClient(acptr))) &&
	    IsOper(acptr) && acptr!=sptr)
	{
			sendto_one(sptr, ":%s NOTICE %s :opers cant /hurt opers.",
					 me.name, parv[0]);
			sendto_realops("Refused hurt for oper %s from user %s[%s@%s]", 
				   acptr->name, sptr->name,
				   sptr->user?sptr->user->username:"<null>",
				   sptr->user?sptr->user->host:"<null>");
		continue;
	}

    if (IsServer(acptr) || IsMe(acptr))
    {
             /* don't allow opers to hurt servers */
          if (MyClient(sptr))
                sendto_one(sptr, err_str(ERR_CANTKILLSERVER),
                           me.name, parv[0]);
                continue;
    }


    if (!MyClient(acptr) || !MyClient(sptr))
    {
                   buffleft  = buffleft - (strlen(acptr->name)+1);
                   if (buffleft>0) {  
                        if (!need_comma) need_comma=1;
                        else
                        strcat(hurt_list, ",");
                        strcat(hurt_list, acptr->name);
                      
                   } else { 
                       buffleft=200; 
                       need_comma=0;
                     sendto_serv_butone(cptr,":%s HURT %s %s :%s",
                               parv[0],hurt_list,parv[2],parv[3]);
                        hurt_list[0]='\0';
                      strcpy(hurt_list, acptr->name);
                   }
    }

    if ((addht > 30000 && MyClient(sptr) && IsPerson(sptr))  || addht < 0 )
   {
      sendto_one(cptr,"NOTICE %s :Too Much Hurt Time: Cannot Raise hurt time to more than 30000 seconds or less than 0",cptr->name);
      continue;
   }



    if (chasing) /* do chasing for hurts */
    {
        sendto_one(sptr,"NOTICE %s :Hurt for %s changed to %s",
                   sptr->name,nick,acptr->name);
    }     

     if (IsHurt(acptr)) /* make sure it hasn't -already- expired */
     {
        if (acptr->hurt > 5 && acptr->hurt <= NOW)
		remove_hurt(acptr);
     }

     if (!IsHurt(acptr))
     {
      if (addht)
        sendto_ops("Received Hurt Message for %s from %s (%ld seconds) (%s)",   
        acptr->name,sptr->name,addht,parv[3]);
      else
        sendto_ops("Received Hurt Message for %s from %s (permanent) (%s)",   
        acptr->name,sptr->name,parv[3]);

			/* function to insert temp conf/data for hurt goes here */
    }
    else
       {
	if (!(acptr->hurt > 0 && acptr->hurt < 5)) /* make it appear right... */
		acptr->hurt = NOW;
       if (addht)
        sendto_realops("Raising Hurt time for %s from %s (+%ld seconds) (%s)",    acptr->name,sptr->name,addht,parv[3]);
       else
        sendto_realops("Raising Hurt time for %s from %s (permanent) (%s)",   
        acptr->name,sptr->name,parv[3]);
        if (IsAnOper(sptr)) sendto_one(sptr, ":%s NOTICE %s :User %s hurt time raised from %lu to %lu.",  me.name, sptr->name, acptr->name, acptr->hurt - NOW, acptr->hurt+addht - NOW);
       }

            
    if ( MyClient(acptr) )
    {   
          if ( !IsHurt(acptr) )
          {
              /*sendto_one(acptr, "NOTICE %s :%s has SILENCED you: (%s)",acptr->name,parv[0],parv[3]);*/
              sendto_one(acptr,SIL_FMT, me.name, acptr->name,
                         get_client_name(acptr, FALSE), 
                         (MyClient(sptr)&&MyClient(acptr)?"Local hurt by ":"hurt: "),
                         sptr->name,
                         parv[3] );

              sendto_one(acptr, err_str(ERR_YOURHURT), me.name,
                                      acptr->name);
          }
            
     if (!IsServer(acptr)) set_hurt(acptr, parv[0], 0);
                    else   SetHurt(acptr);

     if (acptr->hurt < (NOW)) 
     {
         hurt2 = acptr->hurt;
             if (addht != 0)
                 acptr->hurt = NOW + addht;
             else
                 acptr->hurt = 1 /*+ (24*60*60)*/ ;
     } 
     else
     {
                       /* . o O ( hurt twice ? ;> )*/
             if (addht != 0)
                 acptr->hurt += addht;
             else
                 acptr->hurt = 1;
     }

   }
 }

         if (hurt_list[0] && startstat==0)
            sendto_serv_butone(cptr,":%s HURT %s %ld :%s",
                               parv[0],hurt_list,addht,parv[3]);

}        




/*
** m_heal() - reverse hurt early
**    parv[0] = sender prefix
**    parv[1] = person to heal
*/
int m_heal(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
        aClient *acptr;      

        if ( ( !IsPrivileged(cptr) || (MyClient(cptr) && IsPerson(cptr) && 
               !IsAnOper(cptr) ) ) )
            {
                sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
                return(0);
            }

        if (parc < 2 || *parv[1] == '\0')
            {
                sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
                           me.name, parv[0], "HEAL");
                return(0);
            }


        if (check_registered(sptr))
                return 0;

#ifdef DEBUGMODE
		Debug((11, "HEAL: %s %s", parv[0],parv[1] ));
#endif
        
          if (!(acptr = find_client(parv[1], NULL)))
          {
                     sendto_one(sptr, err_str(ERR_NOSUCHNICK),
                                me.name, parv[0], parv[1]);
               return(0);
          }

            /*if (IsPerson(sptr))*/
                sendto_realops("Received HEAL message for %s from %s",
                               acptr->name,sptr->name);
            if (MyClient(acptr))
            {
              if (MyClient(sptr) && (!IsHurt(acptr) || !(acptr->hurt)))
              {
                  sendto_one(sptr,"NOTICE %s :%s is not hurt!",
                  sptr->name,acptr->name);
                  return(0);
              } 
              /*
               * they are hurt and mine, so now to heal them.
               */
              if (IsPerson(sptr))  
               sendto_one(sptr,":%s NOTICE %s :%s is no longer hurt", me.name,
                          sptr->name, acptr->name);
               sendto_one(acptr,":%s NOTICE %s :%s Has removed your hurt status and restored your command access.", me.name, 
                          acptr->name, sptr->name);
               remove_hurt(acptr);
               acptr->hurt = 0;  
            }   
            else
            if (!MyClient(acptr) && IsPrivileged(cptr))
            {
              sendto_serv_butone(cptr,":%s HEAL %s", parv[0], parv[1]);
            }
}



/*
** m_kill
**	parv[0] = sender prefix
**      parv[1] = kill victim(s) - comma separated list
**	parv[2] = kill path
*/
int m_kill(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
        static anUser UnknownUser =
            {
                NULL,   /* nextu */
                NULL,   /* channel */
                NULL,   /* invited */
                NULL,   /* silence */
	        NULL,   /* dccallow */
                NULL,   /* away */
		NULL,   /* mask */
                0,      /* last */
                1,      /* refcount */
                0,      /* joined */
                "<Unknown>",    /* username */
                "<Unknown>",    /* host */
                "<Unknown>"     /* server */
            };
	aClient *acptr;
        anUser  *auser;
	char	*inpath = get_client_name(cptr,FALSE);
	char	*user, *path, *killer, *nick, *p, *s;
	int	chasing = 0, kcount = 0;

        if (check_registered(sptr))
                return 0;

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "KILL");
		return 0;
	    }

	user = parv[1];
	path = parv[2]; /* Either defined or NULL (parc >= 2!!) */

	if (!IsPrivileged(cptr))
	    {
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	    }
	if (IsAnOper(cptr))
	    {
		if (BadPtr(path))
		    {
			sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
				   me.name, parv[0], "KILL");
			return 0;
		    }
		if (strlen(path) > (size_t) TOPICLEN)
			path[TOPICLEN] = '\0';
	    }


       if (MyClient(sptr))
               user = canonize(user);

       for (p = NULL, nick = strtoken(&p, user, ","); nick;
               nick = strtoken(&p, NULL, ","))
       {
             if (!nick) break;

              chasing = 0;

       if (!(acptr = find_client(nick, NULL)))
            {
		/*
		** If the user has recently changed nick, we automaticly
		** rewrite the KILL for this new nickname--this keeps
		** servers in synch when nick change and kill collide
		*/
		if (!(acptr = get_history(nick, (long)KILLCHASETIMELIMIT)))
		    {
			sendto_one(sptr, err_str(ERR_NOSUCHNICK),
				   me.name, parv[0], nick);
			continue;
		    }
		sendto_one(sptr,":%s NOTICE %s :KILL changed from %s to %s",
			   me.name, parv[0], nick, acptr->name);
		chasing = 1;
	    }
	if ((!MyConnect(acptr) && MyClient(cptr) && !OPCanGKill(cptr)) ||
	    (MyConnect(acptr) && MyClient(cptr) && !OPCanLKill(cptr)))
	    {
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		continue;
	    }
	if (IsServer(acptr) || IsMe(acptr))
	    {
		sendto_one(sptr, err_str(ERR_CANTKILLSERVER),
			   me.name, parv[0]);
		continue;
	    }

       /* From here on, the kill is probably going to be successful. */

       kcount++;

       if (!IsServer(sptr) && (kcount > MAXKILLS))
       {
         sendto_one(sptr,":%s NOTICE %s :Too many targets, kill list was truncated. Maximum is %d.",me.name, parv[0], MAXKILLS);
         break;
       }



	if (!IsServer(cptr))
	    {
		/*
		** The kill originates from this server, initialize path.
		** (In which case the 'path' may contain user suplied
		** explanation ...or some nasty comment, sigh... >;-)
		**
		**	...!operhost!oper
		**	...!operhost!oper (comment)
		*/
		inpath = cptr->sockhost;
		if (IsPerson(cptr) && IsMasked(cptr) && cptr->user->mask)
		     inpath = cptr->user->mask;

                  if (kcount < 2)
		    if (!BadPtr(path))
		    {
			(void)sprintf(buf, "%s%s (%s)",
				cptr->name, IsOper(sptr) ? "" : "(L)", path);
			path = buf;
		    }
		else
			path = cptr->name;
	    }
	else if (BadPtr(path))
		 path = "*no-path*"; /* Bogus server sending??? */
	/*
	** Notify all *local* opers about the KILL (this includes the one
	** originating the kill, if from this server--the special numeric
	** reply message is not generated anymore).
	**
	** Note: "acptr->name" is used instead of "user" because we may
	**	 have changed the target because of the nickname change.
	*/

       auser = acptr->user ? acptr->user : &UnknownUser;


	if (index(parv[0], '.'))
	{
		/*sendto_flag(U_KILLS, "*** Notice -- Received KILL message for %s. From %s Path: %s!%s",
			acptr->name, parv[0], inpath, path);*/
               sendto_umode(U_KILLS|U_OPER, "*** Notice -- Received KILL message for %s!%s@%s from %s Path: %s!%s",
                       acptr->name, auser->username, auser->host,
                       parv[0], inpath, path);
               sendto_umode_except(U_KILLS, U_OPER, "*** Notice -- Received KILL message for %s. From %s Path: %s!%s",
                       acptr->name, parv[0], inpath, path);

		if (((MyClient(sptr)||MyClient(acptr)) && sptr && IsPerson(sptr) &&
                    sptr->user->server && acptr->user && acptr->user->server) && !IsULine(cptr, sptr))
                {
			tolog(LOG_OPER, "Kill mesage from %s!%s@%s on %s -> %s!%s@%s on %s [%s]",
                        sptr->name, sptr->user->username,
                        sptr->user->host, sptr->user->server,
                        acptr->name, acptr->user->username, acptr->user->server, path);
		}
	}
	else
	{
		/*sendto_ops("Received KILL message for %s. From %s Path: %s!%s",
			acptr->name, parv[0], inpath, path);*/
               sendto_umode(U_SERVNOTICE | U_OPER, "*** Notice -- Received KILL message for %s!%s@%s from %s Path: %s!%s",
                       acptr->name, auser->username, auser->host,
                       parv[0], inpath, path);
               sendto_umode_except(U_SERVNOTICE, U_OPER, "*** Notice -- Received KILL message for %s. From %s Path: %s!%s",
                       acptr->name, parv[0], inpath, path);

		if (((MyClient(acptr)||MyClient(sptr)) && sptr && IsPerson(sptr) && sptr->user->server &&
                    acptr->user && acptr->user->server) && !IsULine(cptr, sptr))
                {
			tolog(LOG_OPER, "Kill mesage from %s!%s@%s on %s -> %s!%s@%s on %s [%s]",
			sptr->name, sptr->user->username,
			sptr->user->host, sptr->user->server,
			acptr->name, acptr->user->username, acptr->user->host,
			acptr->user->server, path);
		}
	}

#if defined(USE_SYSLOG) && defined(SYSLOG_KILL)
	if (IsOper(sptr))
		syslog(LOG_DEBUG,"KILL From %s For %s Path %s!%s",
			parv[0], acptr->name, inpath, path);
#endif
	/*
	** And pass on the message to other servers. Note, that if KILL
	** was changed, the message has to be sent to all links, also
	** back.
	** Suicide kills are NOT passed on --SRB
	*/
	if (!MyConnect(acptr) || !MyConnect(sptr) || !IsAnOper(sptr))
	    {
		sendto_serv_butone(cptr, ":%s KILL %s :%s!%s",
				   parv[0], acptr->name, inpath, path);
		if (chasing && IsServer(cptr))
			sendto_one(cptr, ":%s KILL %s :%s!%s",
				   me.name, acptr->name, inpath, path);
		ClientFlags(acptr) |= FLAGS_KILLED;
	    }

	/*
	** Tell the victim she/he has been zapped, but *only* if
	** the victim is on current server--no sense in sending the
	** notification chasing the above kill, it won't get far
	** anyway (as this user don't exist there any more either)
	*/
	if (MyConnect(acptr))
		sendto_prefix_one(acptr, sptr,":%s KILL %s :%s!%s",
				  parv[0], acptr->name, inpath, path);
	/*
	** Set FLAGS_KILLED. This prevents exit_one_client from sending
	** the unnecessary QUIT for this. (This flag should never be
	** set in any other place)
	*/
	if (MyConnect(acptr) && MyConnect(sptr) && IsAnOper(sptr))
		(void)sprintf(buf2, "Local kill by %s (%s)", sptr->name,
			BadPtr(parv[2]) ? sptr->name : parv[2]);
	else
	    {
		if ((killer = index(path, ' ')))
		    {
			while (*killer && *killer != '!')
				killer--;
			if (!*killer)
				killer = path;
			else
				killer++;
		    }
		else
			killer = path;
		(void)sprintf(buf2, "Killed (%s)", killer);
	    }
          if (exit_client(cptr, acptr, sptr, buf2) == FLUSH_BUFFER)
               return FLUSH_BUFFER;
       }
       return 0;

}

/***********************************************************************
 * m_away() - Added 14 Dec 1988 by jto. 
 *            Not currently really working, I don't like this
 *            call at all...
 *
 *            ...trying to make it work. I don't like it either,
 *	      but perhaps it's worth the load it causes to net.
 *	      This requires flooding of the whole net like NICK,
 *	      USER, MODE, etc messages...  --msa
 ***********************************************************************/

/*
** m_away
**	parv[0] = sender prefix
**	parv[1] = away message
*/
int m_away(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	char	*away, *awy2 = parv[1];

	if (check_registered_user(sptr))
		return 0;

	away = sptr->user->away;

	if (parc < 2 || !*awy2)
	    {
		/* Marking as not away */

		if (away)
		    {
			MyFree(away);
			sptr->user->away = NULL;
		    }
		sendto_serv_butone(cptr, ":%s AWAY", parv[0]);
		if (MyConnect(sptr))
			sendto_one(sptr, rpl_str(RPL_UNAWAY),
				   me.name, parv[0]);
		return 0;
	    }

	/* Marking as away */

	if (strlen(awy2) > (size_t) TOPICLEN)
		awy2[TOPICLEN] = '\0';
	sendto_serv_butone(cptr, ":%s AWAY :%s", parv[0], awy2);

	if (away)
		away = (char *)MyRealloc(away, strlen(awy2)+1);
	else
		away = (char *)MyMalloc(strlen(awy2)+1);

	sptr->user->away = away;
	(void)strcpy(away, awy2);
	if (MyConnect(sptr))
		sendto_one(sptr, rpl_str(RPL_NOWAWAY), me.name, parv[0]);
	return 0;
}

/*
** m_ping
**	parv[0] = sender prefix
**	parv[1] = origin
**	parv[2] = destination
*/
int m_ping(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient *acptr;
	char	*origin, *destination;

        if (check_registered(sptr))
                return 0;
 
 	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NOORIGIN), me.name, parv[0]);
		return 0;
	    }
	origin = parv[1];
	destination = parv[2]; /* Will get NULL or pointer (parc >= 2!!) */

	acptr = find_client(origin, NULL);
	if (!acptr)
		acptr = find_server(origin, NULL);
	if (acptr && acptr != sptr)
		origin = cptr->name;
	if (!BadPtr(destination) && mycmp(destination, me.name) != 0)
	    {
		if ((acptr = find_server(destination, NULL)))
			sendto_one(acptr,":%s PING %s :%s", parv[0],
				   origin, destination);
	    	else
		    {
			sendto_one(sptr, err_str(ERR_NOSUCHSERVER),
				   me.name, parv[0], destination);
			return 0;
		    }
	    }
	else
		sendto_one(sptr,":%s PONG %s :%s", me.name,
			   (destination) ? destination : me.name, origin);
	return 0;
    }

#ifdef NOSPOOF
/*
** m_nospoof - allows clients to respond to no spoofing patch
**	parv[0] = prefix
**	parv[1] = code
*/
int m_nospoof(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
unsigned long result;

	if (IsNotSpoof(cptr)) return 0;
	if (IsRegistered(cptr)) return 0;
	if (!*sptr->name) return 0;
	if (BadPtr(parv[1])) goto temp;
	result = strtoul(parv[1], NULL, 16);
	/* Accept code in second parameter (ircserv) */
	if (result != sptr->nospoof)
	{
		if (BadPtr(parv[2])) goto temp;
		result = strtoul(parv[2], NULL, 16);
		if (result != sptr->nospoof) goto temp;
	}
	sptr->nospoof = 0;
	if (sptr->user && sptr->name[0])
		return register_user(cptr, sptr, sptr->name,
			sptr->user->username);
	return 0;
	temp:
	/* Homer compatibility */
       sendto_one(cptr, ":%X!nospoof@%s PRIVMSG %s :%cVERSION%c",
               cptr->nospoof, me.name, cptr->name, (char) 1, (char) 1);

	return 0;
}
#endif /* NOSPOOF */

/*
** m_pong
**	parv[0] = sender prefix
**	parv[1] = origin
**	parv[2] = destination
*/
int	m_pong(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient *acptr;
	char	*origin, *destination;

#ifdef NOSPOOF
	if (!IsRegistered(cptr))
		return m_nospoof(cptr, sptr, parc, parv);
#endif

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NOORIGIN), me.name, parv[0]);
		return 0;
	    }

	origin = parv[1];
	destination = parv[2];
	ClientFlags(cptr) &= ~FLAGS_PINGSENT;
	ClientFlags(sptr) &= ~FLAGS_PINGSENT;

	if (!BadPtr(destination) && mycmp(destination, me.name) != 0)
	    {
		if ((acptr = find_client(destination, NULL)) ||
		    (acptr = find_server(destination, NULL)))
		{
			if (!IsServer(cptr) && !IsServer(acptr))
			{
				sendto_one(sptr, err_str(ERR_NOSUCHSERVER),
					me.name, parv[0], destination);
				return 0;
			}
			else
			sendto_one(acptr,":%s PONG %s %s",
				   parv[0], origin, destination);
		}
		else
		    {
			sendto_one(sptr, err_str(ERR_NOSUCHSERVER),
				   me.name, parv[0], destination);
			return 0;
		    }
	    }
#ifdef	DEBUGMODE
	else
		Debug((DEBUG_NOTICE, "PONG: %s %s", origin,
		      destination ? destination : "*"));
#endif
	return 0;
    }

/*
** m_oper
**	parv[0] = sender prefix
**	parv[1] = oper name
**	parv[2] = oper password
*/
int	m_oper(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aConfItem *aconf;
	char	*name, *password;
	const char *encr;
#ifdef CRYPT_OPER_PASSWORD
	char	salt[3];
#ifndef USE_DES
	extern	char *crypt();
#endif
#endif /* CRYPT_OPER_PASSWORD */

	if (check_registered_user(sptr))
		return 0;

	name = parc > 1 ? parv[1] : NULL;
	password = parc > 2 ? parv[2] : NULL;

	if (!IsServer(cptr) && (BadPtr(name) || BadPtr(password)))
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "OPER");
		return 0;
	    }
	
	/* if message arrived from server, trust it, and set to oper */
	    
	if ((IsServer(cptr) || IsMe(cptr)) && !IsOper(sptr))
	    {
                SetOper(sptr);
		sendto_serv_butone(cptr, ":%s MODE %s :+o", parv[0], parv[0]);
		if (IsMe(cptr))
			sendto_one(sptr, rpl_str(RPL_YOUREOPER),
				   me.name, parv[0]);
		return 0;
	    }
	else if (IsOper(sptr))
	    {
		if (MyConnect(sptr))
			sendto_one(sptr, rpl_str(RPL_YOUREOPER),
				   me.name, parv[0]);
		return 0;
	    }

	/*
         * check against both IP and hostname in O:lines for a matching hostname.
	 * NB. Due to my removal of ident, and the confusion between sptr->username
	 * and sptr->user->username, I changed this to sptr->user->username. Hope
	 * it doesn't break anything - DuffJ
	 */
	if (!(aconf = find_conf_exact(name, sptr->user->username, sptr->sockhost, CONF_OPS)) &&
	    !(aconf = find_conf_exact(name, sptr->user->username, inetntoa((char *)&cptr->ip), CONF_OPS)))
	    {
		sendto_one(sptr, err_str(ERR_NOOPERHOST), me.name, parv[0]);
                if (IsHurt(sptr) && MyConnect(sptr))
                {  
                   sptr->since += 15; 
		   det_confs_butmask(sptr, 0);
		   sptr->lasttime = 0;
		   SET_BIT(ClientFlags(cptr), FLAGS_PINGSENT);
                }  else sptr->since += 2;

                sendto_realops("Failed OPER attempt by %s (%s@%s)",
                  parv[0], sptr->user->username, sptr->sockhost);
                sendto_serv_butone(&me,"GLOBOPS :Failed OPER attempt by %s (%s@%s)",
                  parv[0], sptr->user->username, sptr->sockhost);
		sptr->since += 7;
		return 0;
	    }
#ifdef CRYPT_OPER_PASSWORD
        /* use first two chars of the password they send in as salt */

        /* passwd may be NULL. Head it off at the pass... */
        salt[0] = '\0';
        if (password && aconf->passwd)
	    {
        	salt[0] = aconf->passwd[0];
		salt[1] = aconf->passwd[1];
		salt[2] = '\0';
		encr = crypt(password, salt);
	    }
	else
		encr = "";
#else
	encr = password;
#endif  /* CRYPT_OPER_PASSWORD */

#ifdef MD5_PASSWORD
        if (aconf->passwd && toupper(aconf->passwd[0]) == 'M' &&
            toupper(aconf->passwd[1]) == 'D' && 
            toupper(aconf->passwd[2]) == '5' &&
            toupper(aconf->passwd[3]) == '(' &&
            strchr(aconf->passwd, ')')
           )
        {
           aconf->passwd[0] = toupper(aconf->passwd[0]);
           aconf->passwd[1] = toupper(aconf->passwd[1]);
           aconf->passwd[2] = toupper(aconf->passwd[2]);

           encr = ircd_md5_hex(password);
        }
#endif

	if ((aconf->status & CONF_OPS) &&
	    StrEq(encr, aconf->passwd) && !attach_conf(sptr, aconf))
	    {
		int old = (ClientUmode(sptr) & ALL_UMODES);
		char *s;

		s = index(aconf->host, '@');
		*s++ = '\0';
		if (!(aconf->port & OFLAG_ISGLOBAL))
			SetLocOp(sptr);
		else
			SetOper(sptr);
		sptr->oflag = aconf->port;
		*--s =  '@';
                remove_hurt(sptr);
		sendto_ops("%s (%s@%s) is now operator (%c)", parv[0],
			   sptr->user->username, 
                           IsMasked(sptr) && sptr->user->mask ? sptr->user->mask : sptr->user->host,
			   IsOper(sptr) ? 'O' : 'o');
/*                sendto_serv_butone("%s (%s@%s) is now 
			   operator (%c)", parv[0],
                           sptr->user->username, sptr->user->host,
                           IsOper(sptr) ? 'O' : 'o');*/
		ClientUmode(sptr) |= (U_SERVNOTICE|U_WALLOP|U_FAILOP|U_FLOOD|U_LOG|U_TROJAN);
		send_umode_out(cptr, sptr, sptr, old);
 		sendto_one(sptr, rpl_str(RPL_YOUREOPER), me.name, parv[0]);
#if !defined(CRYPT_OPER_PASSWORD) && (defined(FNAME_OPERLOG) ||\
    (defined(USE_SYSLOG) && defined(SYSLOG_OPER)))
		encr = "";
#endif
                tolog(LOG_OPER, "OPER (%s) (%s) by (%s!%s@%s)",
				      name, encr,
				      parv[0], sptr->user->username,
				      sptr->sockhost);

	    }
	else
	    {
		(void)detach_conf(sptr, aconf);
		sendto_one(sptr,err_str(ERR_PASSWDMISMATCH),me.name, parv[0]);
                if (IsHurt(sptr) && MyConnect(sptr))
                {
			sptr->since += 15;
			sptr->lasttime = 0;
			SET_BIT(ClientFlags(cptr), FLAGS_PINGSENT);
                }  else sptr->since += 2;


#ifndef	SHOW_PASSWORD
#ifdef  FAILOPER_WARN
		sendto_one(sptr,":%s NOTICE :Your attempt has been logged.",me.name);
#endif
                  sendto_realops("Failed OPER attempt by %s (%s@%s) using UID %s [NOPASSWORD]",
                   parv[0], sptr->user->username, sptr->sockhost, name);
#else
		  sendto_realops("Failed OPER attempt by %s (%s@%s) [%s]",
		   parv[0], sptr->user->username, sptr->sockhost,password);
#endif
		  sendto_serv_butone(&me, ":%s GLOBOPS :Failed OPER attempt by %s (%s@%s) using UID %s [---]",
		    me.name, parv[0], sptr->user->username, sptr->sockhost, name);
		sptr->since += 7;
                if (IsPerson(sptr))
                 tolog(LOG_OPER, "FAILED OPER (%s) (%s) by (%s!%s@%s)\n",
                                      name, encr,
                                      parv[0], sptr->user->username,
                                      sptr->sockhost);
	    }
	return 0;
    }

/***************************************************************************
 * m_pass() - Added Sat, 4 March 1989
 ***************************************************************************/

/*
** m_pass
**	parv[0] = sender prefix
**	parv[1] = password
*/
int	m_pass(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	char *password = parc > 1 ? parv[1] : NULL;

	if (BadPtr(password))
	    {
		sendto_one(cptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "PASS");
		return 0;
	    }
	if (!MyConnect(sptr) || (!IsUnknown(cptr) && !IsHandshake(cptr)))
	    {
		sendto_one(cptr, err_str(ERR_ALREADYREGISTRED),
			   me.name, parv[0]);
		return 0;
	    }
	strncpyzt(cptr->passwd, password, sizeof(cptr->passwd));
	return 0;
}

/*
 * m_userhost added by Darren Reed 13/8/91 to aid clients and reduce
 * the need for complicated requests like WHOIS. It returns user/host
 * information only (no spurious AWAY labels or channels).
 */
int	m_userhost(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
        int catsize;
	char	*p = NULL;
	aClient	*acptr;
	char	*s;
	char    *curpos;
	int	resid;

	if (check_registered(sptr))
		return 0;

	if (parc > 2)
		(void)m_userhost(cptr, sptr, parc-1, parv+1);

	if (parc < 2)
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "USERHOST");
		return 0;
	    }

	/*
	 * use curpos to keep track of where we are in the output buffer,
	 * and use resid to keep track of the remaining space in the
	 * buffer
	 */
	curpos = buf;
	curpos += sprintf(curpos, rpl_str(RPL_USERHOST), me.name, parv[0]);
	resid = sizeof(buf) - (curpos - buf) - 1;  /* remaining space */

	/*
	 * for each user found, print an entry if it fits.
	 */
	for (s = strtoken(&p, parv[1], " "); s;
	     s = strtoken(&p, (char *)NULL, " "))
	  if ((acptr = find_person(s, NULL))) {
	    catsize = strlen(acptr->name)
	      + (IsAnOper(acptr) ? 1 : 0)
	      + 3 + strlen(acptr->user->username)
	      + strlen(UGETHOST(sptr, acptr->user)) + 1;
	    if (catsize <= resid) {
	      curpos += sprintf(curpos, "%s%s=%c%s@%s ",
				acptr->name,
				IsAnOper(acptr) ? "*" : "",
				(acptr->user->away) ? '-' : '+',
				acptr->user->username,
				UGETHOST(sptr, acptr->user));
	      resid -= catsize;
	    }
	  }
	
	/*
	 * because of some trickery here, we might have the string end in
	 * "...:" or "foo " (note the trailing space)
	 * If we have a trailing space, nuke it here.
	 */
	curpos--;
	if (*curpos != ':')
	  *curpos = '\0';
	sendto_one(sptr, "%s", buf);
	return 0;
}

/*
 * m_ison added by Darren Reed 13/8/91 to act as an efficent user indicator
 * with respect to cpu/bandwidth used. Implemented for NOTIFY feature in
 * clients. Designed to reduce number of whois requests. Can process
 * nicknames in batches as long as the maximum buffer length.
 *
 * format:
 * ISON :nicklist
 */

int     m_ison(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
        char    namebuf[USERLEN+HOSTLEN+4];
        aClient *acptr;
        char    *s, **pav = parv, *user;
        int     len;
        char    *p = NULL;
 
        if (check_registered(sptr))
                return 0;
 
        if (parc < 2)
	  {
                sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
                           me.name, parv[0], "ISON");
                return 0;
	  }
 
        (void)sprintf(buf, rpl_str(RPL_ISON), me.name, *parv);
        len = strlen(buf);
 
        for (s = strtoken(&p, *++pav, " "); s; s = strtoken(&p, NULL, " "))
	  {
                if(user=index(s, '!')) *user++='\0';
                if ((acptr = find_person(s, NULL)))
		  {
		    if (user) {
                                strcpy(namebuf, acptr->user->username);
                                strcat(namebuf, "@");
                                strcat(namebuf, UGETHOST(sptr, acptr->user));
                                if(match(user, namebuf))
                                        continue;
                                *--user='!';
		    }
 
                        (void)strncat(buf, s, sizeof(buf) - len);
                        len += strlen(s);
                        (void)strncat(buf, " ", sizeof(buf) - len);
                        len++;
		  }
	  }
        sendto_one(sptr, "%s", buf);
        return 0;
}



static int user_modes[]	     = { U_OPER, 'o',
				 U_LOCOP, 'O',
				 U_INVISIBLE, 'i',
				 U_WALLOP, 'w',
				 U_FAILOP, 'g',
				 U_HELPOP, 'h',
				 U_SERVNOTICE, 's',
				 U_KILLS, 'k',
				 U_CLIENT, 'c',
				 U_FLOOD, 'f',
				 U_LOG, 'l',
				 U_MASK, 'm',
/*				 U_REGNICK, 'r',*/
				 U_DCCALLOW, 'd',
				 U_TROJAN, 't',
				 0, 0 };


/*
 * Build umode string for BURST command
 * --Run
 *
 * Stolen from ircu, adapted for SorceryNet
 * -Mysid
 */
char *umode_str(struct Client *cptr)
{
  static char umodeBuf[256];
  char* m = umodeBuf;                /* Maximum string size: "owidg\0" */
  int   i;
  int   c_flags;

  c_flags = (ClientUmode(cptr) & SEND_UMODES);        /* cleaning up the original code */

  for (i = 0; user_modes[i]; i += 2) {
    if ( (c_flags & user_modes[i]))
      *m++ = user_modes[i+1];
  }
  *m = '\0';

  return umodeBuf;                /* Note: static buffer, gets
                                   overwritten by send_umode() */
}


unsigned long parse_umode(char *string)
{
  unsigned long c_flags = 0;
  int i = 0;

  for(; string && *string; string++) {
      for (i = 0; user_modes[i]; i += 2) {
         if (*string == user_modes[i+1]) {
             c_flags |= user_modes[i];
             break;
         }
      }
  }

  return c_flags;
}


/*
 * m_umode() added 15/10/91 By Darren Reed.
 * parv[0] - sender
 * parv[1] - username to change mode for
 * parv[2] - modes to change
 */
int	m_umode(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  int	flag;
  int	*s, y;
  char	**p, *m;
  aClient	*acptr;
  int	what, setflags, realflags;
  int     uline = IsULine(cptr,sptr);
  int     useduline = 0;
  int     hackwarn = 0;

  useduline = uline;

  /*         uline = 0;
   *         useduline = 0;
   */
  if (check_registered(sptr) && !IsServer(sptr))
    return 0;

  what = MODE_ADD;

  if (parc < 2)
  {
      sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		 me.name, parv[0], "MODE");
      return 0;
  }

  if (!(acptr = find_person(parv[1], NULL)))
  {
      if (MyConnect(sptr))
	sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL),
		   me.name, parv[0], parv[1]);
      return 0;
  }

  if (sptr == acptr)
    useduline = 0;

  if ((!uline || IsServer(acptr))  && (IsServer(sptr) || sptr != acptr))
  {
      if (IsServer(cptr))
	sendto_ops_butone(NULL, &me,
			  ":%s WALLOPS :MODE for User %s From %s!%s",
			  me.name, parv[1],
			  get_client_name(cptr, FALSE), sptr->name);
      else
	sendto_one(sptr, err_str(ERR_USERSDONTMATCH),
		   me.name, parv[0]);
      return 0;
  }
 
  if (parc < 3)
  {
      m = buf;
      *m++ = '+';
      for (s = user_modes; (flag = *s) && (m - buf < BUFSIZE - 4);
	   s += 2)
	if (ClientUmode(acptr) & flag)
	  *m++ = (char)(*(s+1));
      *m = '\0';
      sendto_one(sptr, rpl_str(RPL_UMODEIS),
		 me.name, parv[0], buf);
      return 0;
  }

  /* find flags already set for user */
  setflags = 0;
  for (s = user_modes; (flag = *s); s += 2)
    if (ClientUmode(acptr) & flag)
      setflags |= flag;
  realflags = ClientUmode(acptr);

  /*
   * parse mode change string(s)
   */
  for (p = &parv[2]; p && *p; p++ )
    for (m = *p; *m; m++)
      switch(*m)
      {
	case '+' :
	  what = MODE_ADD;
	  break;
	case '-' :
	  what = MODE_DEL;
	  break;	
	  /* we may not get these,
	   * but they shouldnt be in default
	   */
	case ' ' :
	case '\n' :
	case '\r' :
	case '\t' :
	  break;
	default :
	  for (s = user_modes; (flag = *s); s += 2)
	    if (*m == (char)(*(s+1)))
	      {
		if (what == MODE_ADD)
		  ClientUmode(acptr) |= flag;
		else
		  ClientUmode(acptr) &= ~flag;	
		break;
	      }
	  if (flag == 0 && MyConnect(acptr))
	    sendto_one(acptr,
		       err_str(ERR_UMODEUNKNOWNFLAG),
		       me.name, parv[0]);
	  break;
      }


  if (uline && useduline)
  {
           
      if ((setflags & U_OPER) && !IsOper(acptr))
      {
	  SetOper(acptr);
	  hackwarn = 1;
      }
      if (!(setflags & U_OPER) && IsOper(acptr))
      {
	  ClearOper(acptr);
	  hackwarn = 1;
      }

      if (MyClient(acptr))
      {

	  if ((!(setflags & U_LOCOP) && !(setflags & U_OPER)) && 
	      IsAnOper(acptr))
	  {
	      hackwarn=1;
	      ClearOper(acptr);
	      ClearLocOp(acptr);
	  }

	  if ( (setflags & U_LOCOP) && !IsAnOper(acptr))
	  {
	      ClientUmode(acptr) |= U_LOCOP;
	      hackwarn=1;
	  }

	  /* hackwarn for anything besides +/- h */
#define ULINE_CHANGE (U_HELPOP|U_MASK)
	  if (!hackwarn)
	  {
	      if((realflags & ~(ULINE_CHANGE)) == (ClientUmode(acptr) & ~(ULINE_CHANGE)))
		hackwarn = 0;
	      else 
	      {
		  /*hackwarn = 2;*/
	      }
	  }
      }


      if (hackwarn == 1) 
      {
	  sendto_ops("HACK! %s[%s] Just tried to change usermode of %s to %s",sptr->name, sptr->user ? sptr->user->server : "" ,acptr->name,parv[2]);
	  sendto_serv_butone(cptr, "GLOBOPS :HACK! %s[%s] Just tried to change usermode of %s to %s",sptr->name,sptr->user ? sptr->user->server : "", acptr->name, parv[2]);
      }
      else if (hackwarn == 2)
      {
	  sendto_ops("HACK! %s[%s] set usermode of %s to %s",sptr->name,sptr->user ? sptr->user->server : "",acptr->name,parv[2]);
	  sendto_serv_butone(cptr, "GLOBOPS :HACK! %s[%s] set usermode of %s to %s",sptr->name,sptr->user ? sptr->user->server : "",acptr->name,parv[2]);
      }

  }

  /*if (!uline)*/
  if (uline == 0)
  {
      /* we'll need to make a bigger deal out of it if a U-lined server
       *  tries...
       */

      /*
       * stop users making themselves operators too easily
       */
      if (!IS_SET(setflags, U_OPER) && IsOper(sptr) && !IsServer(cptr))
	ClearOper(sptr);
      if (!(setflags & U_LOCOP) && IsLocOp(sptr) && !IsServer(cptr))
	ClientUmode(sptr) &= ~U_LOCOP;

      /*
       *  Let only operators set HelpOp
       * Helpops get all /quote help <mess> globals -Donwulff
       * yeh, but dont -h someone if they're already +h -Mysid
       */
      if (!IS_SET(setflags, U_HELPOP) && MyClient(sptr) && IsHelpOp(sptr) && !OPCanHelpOp(sptr))
          if (!UserOppedOn(sptr, HELPOP_CHAN)) {
 		ClearHelpOp(sptr);
                sptr->since++;
          }
      /*
       * Let only operators set FloodF, ClientF; also
       * remove those flags if they've gone -o/-O.
       *  FloodF sends notices about possible flooding -Cabal95
       *  ClientF sends notices about clients connecting or exiting
       */
      if (!IsAnOper(sptr) && !IsServer(cptr))
      {
	  if (IsClientF(sptr))
	    ClearClientF(sptr);
	  if (IsFloodF(sptr))
	    ClearFloodF(sptr);
          if (IsLogMode(sptr))
            ClearLogMode(sptr);
          if (IsTrojan(sptr))
            ClearTrojan(sptr);
      }
      /*
       * New oper access flags - Only let them set certian usermodes on
       * themselves IF they have access to set that specific mode in their
       * O:Line.
       */
      if (MyClient(sptr) && IsAnOper(sptr))
      {
	  if (!IS_SET(setflags, U_CLIENT) && IsClientF(sptr) && !OPCanUModeC(sptr))
	    ClearClientF(sptr);
	  if (!IS_SET(setflags, U_FLOOD) && IsFloodF(sptr) && !OPCanUModeF(sptr))
	    ClearFloodF(sptr);  
      }

      if (!IS_SET(ClientUmode(sptr), U_MASK))
	  REMOVE_BIT(ClientUmode(sptr), U_FULLMASK);
      else
	  SET_BIT(ClientUmode(sptr), U_FULLMASK);

      /*
       * If I understand what this code is doing correctly...
       *   If the user WAS an operator and has now set themselves -o/-O
       *   then remove their access, d'oh!
       * In order to allow opers to do stuff like go +o, +h, -o and
       * remain +h, I moved this code below those checks. It should be
       * O.K. The above code just does normal access flag checks. This
       * only changes the operflag access level.  -Cabal95
       */
      if ((setflags & (U_OPER|U_LOCOP)) && !IsAnOper(sptr) &&
	  MyConnect(sptr))
      {
	  det_confs_butmask(sptr, CONF_CLIENT & ~CONF_OPS);
	  sptr->oflag = 0;
      }
  }

  if (!(setflags & U_MASK) && IsMasked(acptr))
      perform_mask(acptr, MODE_ADD);
  if ((setflags & U_MASK) && !IsMasked(acptr))
      perform_mask(acptr, MODE_DEL);

  /*
   * compare new flags with old flags and send string which
   * will cause servers to update correctly.
   */
  send_umode_out(cptr, sptr, acptr, setflags);

  return 0;
}

void perform_mask(aClient *acptr, int v)
{
    if (!acptr->user)
        return;

    if (v == MODE_DEL)
    {
        if (acptr->user->mask)
            MyFree(acptr->user->mask);
        acptr->user->mask = NULL;
        ClearMasked(acptr);
        return;
    }
    if (v == MODE_ADD) 
    {
        if (acptr->user->mask)
            MyFree(acptr->user->mask);
        DupString(acptr->user->mask, genHostMask(acptr->user->host));
        SetMasked(acptr);
        return;
    }
}

char *genHostMask(char *host)
{
    char tok1[USERLEN+HOSTLEN+255];
    char tok2[USERLEN+HOSTLEN+255], *p, *q;
    static char fin[USERLEN+HOSTLEN+255];
    char fintmp[USERLEN+HOSTLEN+255];
    int i, fIp = TRUE, hlen;

    if (!host || !index(host, '.'))
        return (host ? host : "");
    for (i = 0; host[i]; i++)
         if ((host[i] < '0' || host[i] > '9') && host[i] != '.')
         {
             fIp = FALSE; 
             break;
         }

    *tok1 = *tok2 = '\0';
    hlen = strlen(host);

    /* It's an ipv4 address in dot format: last two tokens are encoded */
    if (fIp && hlen <= 15)
    {
        if (( p = rindex(host, '.') )) {
            *p = '\0';
            strcpy(tok1, host);
            strcpy(tok2, p + 1);
            *p = '.';
        }
        if (( p = rindex(tok1, '.') )) {
            strcpy(fintmp, ntokEncode(p+1, hlen));
            *p = '\0';
        }

        sprintf(fin, "%s.%s.", ntokEncode(tok2, hlen), fintmp);
        sprintf(fin + strlen(fin), "%s-imsk", ntokEncode(tok1, hlen));
        return fin;
    }

    /* It's a resolved hostname, hash the first token */
    if (( p = index(host, '.') )) {
        *p = '\0';
        strcpy(tok1, host);
        strcpy(fin, p + 1);
        *p = '.';
    }

    /* Then separately hash the domain */
    if (( p = rindex(fin, '.') )) {
        --p;
        while (p > fin && *(p-1) != '.') p--;
    }
    if (p && (q = rindex(fin, '.'))) {
        i = (unsigned char)*p;
        *p = '\0';
        *q = '\0';
        strcat(tok2, fin);
        *p = (unsigned char)i;
        if (*p == '.')
            strcat(tok2, tokenEncode(p+1));
        else 
            strcat(tok2, tokenEncode(p));
        *q = '.';
        strcat(tok2, q);
    }
    else
     strcpy(tok2, fin);
    strcpy(tok1, tokenEncode(tok1));
    snprintf(fin, HOSTLEN, "%s.%s-hmsk", tok1, tok2);

    return fin;
}


char *tokenEncode(char *str)
{
   static char strn[HOSTLEN + 255];
   unsigned char *p;
   unsigned long m, v;

   for ( p = str, m = 0, v = 0x55555; *p; p++ )
        v = (31 * v) + (*p);
   sprintf(strn, "%x", v);
   return strn;
}

/*
 * -> Base on 100million in the most gruesome way imaginable and map it
 * into 65 characters, in a peculiar way, the heck? *snick* 
 *
 * -Mysid 
 */

char *EEx1(u_int32_t value)
{
   static char result[100];
   char *x_output_map[] = {
        "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
        "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
        "-", "_", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "/",
        "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
        "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "\0"
   };
   #define X_OUTPUT_MAP_SZ 65
   #define X_BASE_ENCODING 2147483647
   int i = 0, x = 99;
   u_int32_t rem;

   for(i = 0; i < 100; i++)
       result[i] = '\0';

   while(value > 0) {
        rem = (value % X_BASE_ENCODING);
        value = (int) value / X_BASE_ENCODING;
        if ((rem >= X_BASE_ENCODING))
            break;
        result[x--] = *x_output_map[((unsigned char)rem) % X_OUTPUT_MAP_SZ];
        if (x < 0 || value < 1)
            break;
   }

   if (x < 99)
       for(i = 0; !result[i]; i++) ;
   else i = 0;

   return (result + i);
   #undef X_OUTPUT_MAP_SZ
   #undef X_BASE_ENCODING
}

char *ntokEncode(char *str, int ss)
{
   static char strn[HOSTLEN + 255];
   u_int32_t     md5data[16];
   u_int32_t     str_adj[16];
   static u_int32_t     md5hash[4];
   int x;

   if (str) {
       for(x = 0; x < 16 && str[x]; x++)
           str_adj[x] = str[x];
       for(; x < 16; x++)
           str_adj[x] = 0;
   }

   md5data[0]  = mask_seed1 + str_adj[0];
   md5data[1]  = 0x12345678 + str_adj[1];
   md5data[2]  = mask_seed2 + str_adj[2];
   md5data[3]  = 0xdeadbeef + str_adj[3];
   md5data[4]  = mask_seed3 + str_adj[4];
   md5data[5]  = mask_seed4 + str_adj[5];
   md5data[6]  = 0x87654321 + str_adj[6];
   md5data[7]  = md5data[1] + str_adj[7];
   if (str_adj[15] == 0)
       str_adj[15] = (str ? ss : 0);

   for(x = 1; x < 16; x++)
       md5data[x]  = (md5data[(x % 4) + 2] + str_adj[x]);

   /* These values are part of the MD5 message digest algorithm, */
   /* don't edit them. */
   md5hash[0] = 0x67452301;
   md5hash[1] = 0xefcdab89;
   md5hash[2] = 0x98badcfe;
   md5hash[3] = 0x10325476;
   MD5Transform(md5hash, md5data);

   strn[0] = '\0';

   /*for(x = 0; x < 4; x++)
        md5hash[x] >>= 12;*/

   for(x = 0; x < 4; x++) {
       sprintf(strn + strlen(strn), "%s", EEx1(md5hash[x]));
   }
   return strn;
}
	
/*
 * send the MODE string for user (user) to connection cptr
 * -avalon
 */
void send_umode(aClient *cptr, aClient *sptr, aClient *acptr, int old, int sendmask, char *umode_buf)
{
	int	*s, flag;
	char	*m;
	int	what = MODE_NULL;

	/*
	 * build a string in umode_buf to represent the change in the user's
	 * mode between the new (sptr->flag) and 'old'.
	 */
	m = umode_buf;
	*m = '\0';
	for (s = user_modes; (flag = *s); s += 2)
	    {
		if (MyClient(acptr) && !(flag & sendmask))
			continue;
		if ((flag & old) && !(ClientUmode(acptr) & flag))
		    {
			if (what == MODE_DEL)
				*m++ = *(s+1);
			else
			    {
				what = MODE_DEL;
				*m++ = '-';
				*m++ = *(s+1);
			    }
		    }
		else if (!(flag & old) && (ClientUmode(acptr) & flag))
		    {
			if (what == MODE_ADD)
				*m++ = *(s+1);
			else
			    {
				what = MODE_ADD;
				*m++ = '+';
				*m++ = *(s+1);
			    }
		    }
	    }
	*m = '\0';
	if (*umode_buf && cptr)
		sendto_one(cptr, ":%s MODE %s :%s",
			   sptr->name, acptr->name, umode_buf);
}

/*
 * added Sat Jul 25 07:30:42 EST 1992
 */
void send_umode_out(aClient *cptr, aClient *sptr, aClient *actptr, int old)
{
	int     i;
	aClient *acptr;

        if (sptr == actptr)
	   send_umode(NULL, sptr, actptr, old, SEND_UMODES, buf);
        else
	   send_umode(NULL, sptr, actptr, old, ALL_UMODES, buf);
# ifdef NPATH
        check_command((long)4, ":%s MODE %s :%s", sptr->name, actptr->name, buf);
/*        check_command((long)4, ":%s MODE %s :%s", sptr->name, sptr->name, buf); */
# endif

          /*if (*buf && actptr != sptr)*/
            if (buf && (!MyClient(sptr) && MyClient(actptr)) )
            {
        	send_umode(NULL, sptr, actptr, old, ALL_UMODES, buf);
                    if (*buf)
			 sendto_one(actptr, ":%s MODE %s :%s",
				   sptr->name, actptr->name, buf);
            }

	for (i = highest_fd; i >= 0; i--)
		if ((acptr = local[i]) && IsServer(acptr) &&
		    (acptr != cptr) && (acptr != sptr) && *buf)
			sendto_one(acptr, ":%s MODE %s :%s",
				   sptr->name, actptr->name, buf);

	if (cptr && MyClient(cptr))
		send_umode(cptr, sptr, actptr, old, ALL_UMODES, buf);
}

/***********************************************************************
 * m_silence() - Added 19 May 1994 by Run. 
 *
 ***********************************************************************/

/*
 * is_silenced : Does the actual check wether sptr is allowed
 *               to send a message to acptr.
 *               Both must be registered persons.
 * If sptr is silenced by acptr, his message should not be propagated,
 * but more over, if this is detected on a server not local to sptr
 * the SILENCE mask is sent upstream.
 */
static int is_silenced(aClient *sptr, aClient *acptr)
{
	Link *lp;
	anUser *user;
	static char sender[HOSTLEN+NICKLEN+USERLEN+5];
	static char sendermsk[HOSTLEN+NICKLEN+USERLEN+5];

	if (!(acptr->user) || !(lp = acptr->user->silence) ||
	    !(user = sptr->user))
		return 0;

	sprintf(sender,"%s!%s@%s",sptr->name,user->username, user->host);
	sprintf(sendermsk,"%s!%s@%s",sptr->name,user->username, genHostMask(user->host));

	for (; lp; lp = lp->next) {
		if (lp->value.cp[0] == '$' && IsMasked(sptr) && !IsOper(sptr)
		     && tolower(lp->value.cp[1]) == 'm') {
			if (!MyConnect(sptr)) {
				sendto_one(sptr->from,
					   ":%s SILENCE %s :%s",acptr->name,
					   sptr->name, lp->value.cp);
				lp->flags = 1;
			}
			return 1;
		}
		if (!match(lp->value.cp, sender) || !match(lp->value.cp, sendermsk)) {
			if (!MyConnect(sptr)) {
				sendto_one(sptr->from,
					   ":%s SILENCE %s :%s",acptr->name,
					   sptr->name, lp->value.cp);
				lp->flags = 1;
			}
			return 1;
		}
	}

	return 0;
}

int del_silence(aClient *sptr, char *mask)
{

  Link **lp;
  Link *tmp;

  for (lp = &(sptr->user->silence); *lp; lp = &((*lp)->next))
    if (mycmp(mask, (*lp)->value.cp)==0)
    { tmp = *lp;
      *lp = tmp->next;
      MyFree(tmp->value.cp);
      free_link(tmp);
      return 0; }
  return -1;
}

static int add_silence(aClient *sptr, char *mask)
{

  Link *lp;
  int cnt = 0, len = 0;

  for (lp = sptr->user->silence; lp; lp = lp->next)
  { len += strlen(lp->value.cp);
    if (MyClient(sptr))
      if ((len > MAXSILELENGTH) || (++cnt >= MAXSILES))
      { sendto_one(sptr, err_str(ERR_SILELISTFULL), me.name, sptr->name, mask);
	return -1; }
      else
      { if (!match(lp->value.cp, mask))
	  return -1; }
    else if (!mycmp(lp->value.cp, mask))
      return -1;
  }
  lp = make_link();
  bzero((char *)lp, sizeof(Link));
  lp->next = sptr->user->silence;
  lp->value.cp = (char *)MyMalloc(strlen(mask)+1);
  (void)strcpy(lp->value.cp, mask);
  sptr->user->silence = lp;
  return 0;
}

/*
** m_silence
**	parv[0] = sender prefix
** From local client:
**	parv[1] = mask (NULL sends the list)
** From remote client:
**	parv[1] = nick that must be silenced
**      parv[2] = mask
*/
int m_silence(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  Link *lp;
  aClient *acptr;
  char c, *cp, *user, *host;

  if (check_registered_user(sptr)) return 0;

  if (MyClient(sptr))
  {
    acptr = sptr;
    if (parc < 2 || *parv[1]=='\0' || (acptr = find_person(parv[1], NULL)))
    { if (!(acptr->user)) return 0;
      for (lp = acptr->user->silence; lp; lp = lp->next)
	sendto_one(sptr, rpl_str(RPL_SILELIST), me.name,
	    sptr->name, acptr->name, lp->value.cp);
      sendto_one(sptr, rpl_str(RPL_ENDOFSILELIST), me.name, acptr->name);
      return 0; }
    cp = parv[1];
    c = *cp;
    if (c=='-' || c=='+') cp++;
    else if (!(index(cp, '@') || index(cp, '.') ||
	index(cp, '!') || index(cp, '*')))
    { sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, parv[0], parv[1]);
      return -1; }
    else c = '+';
    cp = pretty_mask(cp, 0);
    if ((c=='-' && !del_silence(sptr,cp)) ||
        (c!='-' && !add_silence(sptr,cp)))
    { sendto_prefix_one(sptr, sptr, ":%s SILENCE %c%s", parv[0], c, cp);
      if (c=='-')
	sendto_serv_butone(NULL, ":%s SILENCE * -%s", sptr->name, cp);
    }
  }
  else if (parc < 3 || *parv[2]=='\0')
  {
    sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "SILENCE");
    return -1;
  }
  else if ((c = *parv[2])=='-' || (acptr = find_person(parv[1], NULL)))
  {
    if (c=='-')
    { if (!del_silence(sptr,parv[2]+1))
	sendto_serv_butone(cptr, ":%s SILENCE %s :%s",
	    parv[0], parv[1], parv[2]); }
    else
    { (void)add_silence(sptr,parv[2]);
      if (!MyClient(acptr))
        sendto_one(acptr, ":%s SILENCE %s :%s",
            parv[0], parv[1], parv[2]); }
  }
  else
  {
    sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, parv[0], parv[1]);
    return -1;
  }
  return 0;
}

/*
** m_cnick
**	parv[0] = sender prefix
**	parv[1] = nick to change from
** 	parv[2] = nick to change to
*/
int m_cnick(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
   aClient *acptr, *acptr2;
   char newnick[NICKLEN+2];
   char *tonick[3];
   int ts = 0;

  if(!IsServer(sptr)) {
    return 0;
  }
  
  if(parc < 3) {
    sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "CNICK");
    return 0;
  }

  acptr = find_client(parv[1], NULL);
  if(acptr == NULL) { /* Who? */
    sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, parv[0], parv[1]);
    return 0;
  } 

  if(IsServer(acptr) || IsMe(acptr)) { /* Can't change server's name */
    return 0;
  }

  if(!IsServer(sptr)) {
     return 0;
  }

  if(parc < 3) {
     sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "CNICK");
     return 0;
  }

  if (strlen(parv[2]) > NICKLEN) return 0;
  acptr = find_client(parv[1], NULL);

   if(acptr == NULL) { /* Who? */
     sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, parv[0], parv[1]);
     return 0;
   } 

   if(IsServer(acptr) || IsMe(acptr)) { /* Can't change servers nick */
     return 0;
   }

   acptr2 = find_client(parv[2], NULL);
   if(acptr2 != NULL) { /* Nick is in use */
      sendto_one(sptr, err_str(ERR_NICKNAMEINUSE), me.name, parv[0], parv[2]);
      return 0;
   }
   (void)strncpy(newnick, parv[2], NICKLEN);
   if ( *parv[2] == '\0'|| do_nick_name(newnick, 1) == 0 || 
        strcmp(newnick, parv[2]))
   {
           sendto_serv_butone(&me, ":%s GLOBOPS :%s sent an INVALID m_cnick() event for %s %s was an invalid nick", me.name, sptr->name, acptr->name, parv[1]);
           return 0;
   }

   if ((parc < 4 ) || !isdigit(*parv[3]))
       ts = NOW;
   else
       ts = atol(parv[3]);
 /* at this point the CNICK must -NOT- fail, even kill clients using the
    nick if necessary... */
   sendto_common_channels(acptr, ":%s NICK :%s", parv[1], parv[2]);
   (void)add_history(acptr);
   if(!MyConnect(acptr)) {
      sendto_one(acptr, ":%s CNICK %s %s %ld", parv[0], parv[1], parv[2], ts);
      return 0;
   }
   if (IsPerson(acptr))
   (void)add_history(acptr);
   sendto_serv_butone(NULL, ":%s NICK %s :%i",
                      parv[1], parv[2], ts);
   (void)del_from_client_hash_table(acptr->name, acptr);
   hash_check_watch(acptr, RPL_LOGOFF);
   (void)strncpy(acptr->name, parv[2], NICKLEN);
   (void)add_to_client_hash_table(parv[2], acptr);
   hash_check_watch(acptr, RPL_LOGON);

  /*tonick[0] = parv[1];  tonick[1] = parv[2];  tonick[2] = NULL;
    return m_nick(acptr, acptr, 2, tonick);*/
  return 0;  
}

/**********************************************************************/

/* Base64 Encode -- Deriviative work of Public-domain code by John Walker  */
/* -Mysid                                                                  */
#ifndef byte
#define byte signed char
#endif

static byte dtable[256];              /* Encode / decode table */
char *b64Encode(char *evalue, int xlen) {
  static char buf3[2048];
  int i = 0, x = 0, hiteof = FALSE;

  if (xlen > 512) { 
      return "<ERROR>";
  }

  for (i = 0; i < 26; i++) {
       dtable[i] = 'A' + i;
       dtable[26 + i] = 'a' + i;
  }
  for (i = 0; i < 10; i++) {
       dtable[52 + i] = '0' + i;
  }
  dtable[62] = '+';
  dtable[63] = '/';

  *buf3 = '\0';
  while (!hiteof) {
      byte igroup[3], ogroup[4];
      int c, n;

      igroup[0] = igroup[1] = igroup[2] = 0;
      for (n = 0; n < 3; n++) {
          if ((evalue[x] == '\0' && xlen < 1) || (x >= xlen)) {
              hiteof = TRUE;
              break;
          }
          c = evalue[x++];
          igroup[n] = (byte) c;
      }
      if (n > 0) {
            ogroup[0] = dtable[igroup[0] >> 2];
            ogroup[1] = dtable[((igroup[0] & 3) << 4) | (igroup[1] >> 4)];
            ogroup[2] = dtable[((igroup[1] & 0xF) << 2) | (igroup[2] >> 6)];
            ogroup[3] = dtable[igroup[2] & 0x3F];
            if (n < 3) {
                ogroup[3] = '=';
                if (n < 2) {
                    ogroup[2] = '=';
                }
            }
            for (i = 0; i < 4; i++) {
                sprintf(buf3, "%s%c", buf3, ogroup[i]);
            }
        }
    }
    return buf3;
}

char *b64Decode(char *value) {
    int i, x = 0;
    static char buf3[2048] = "";

    memset(buf3, 0, sizeof(buf3));
    for (i = 0; i < 255; i++) {
        dtable[i] = 0x80;
    }
    for (i = 'A'; i <= 'Z'; i++) {
        dtable[i] = 0 + (i - 'A');
    }
    for (i = 'a'; i <= 'z'; i++) {
        dtable[i] = 26 + (i - 'a');
    }
    for (i = '0'; i <= '9'; i++) {
        dtable[i] = 52 + (i - '0');
    }
    dtable['+'] = 62;
    dtable['/'] = 63;
    dtable['='] = 0;
    while (TRUE) {
        byte a[4], b[4], o[3];

        for (i = 0; i < 4; i++) {
            int c = value[x++];

            if (c == '\0') {
                if ((i > 0)) {
                    fprintf(stderr, "Input file incomplete.\n");
                    exit(1);
                }
                return buf3;
            }
            if (dtable[c] & 0x80) {
                /* Ignoring errors: discard invalid character. */
                i--;
                continue;
            }
            a[i] = (byte) c;
            b[i] = (byte) dtable[c];
        }
        o[0] = (b[0] << 2) | (b[1] >> 4);
        o[1] = (b[1] << 4) | (b[2] >> 2);
        o[2] = (b[2] << 6) | b[3];
        i = a[2] == '=' ? 1 : (a[3] == '=' ? 2 : 3);
        strncat(buf3, o, i);
        if (i < 3) {
            return buf3;
        }
    }
    return buf3;
}



/**********************************************************************/

char *exploits_2char[] = { "js", "pl", NULL };
char *exploits_3char[] = { "exe", "com", "bat", "dll", "ini", "vbs", "pif", "mrc", "scr", "doc", "xls", "lnk", "shs", NULL };

int dcc_warntable(char *fn) {
   int i = 0, len;
   static int init_x = 0;

   static struct ccw_data {
      char *fn;   /* Filename */
      int l;      /* Length */
   }
   cc_warn_table[] = {
        {"script.ini"},	{"life_stages.txt.shs"},        {"links.vbs"},
        {"buny.exe"},   {"setup.exe"},                  {"patch.exe"},
        {"server.exe"}, {"dwsetup.exe"},                {"movie.avi.com"},
        {"mrsmart.exe"},{"patch.exe"},                  {"mirc"},
        {NULL}
   };

  if (!init_x) {
      init_x = 1;
      for(i = 0; cc_warn_table[i].fn; i++) {
          cc_warn_table[i].l = strlen(cc_warn_table[i].fn);
      }
  } 

   if (!fn || !*fn)	
       return 0;
  len = strlen(fn);
   for(i = 0; cc_warn_table[i].fn; i++) {
       if (cc_warn_table[i].l > len)
           continue;
       if (*cc_warn_table[i].fn != tolower(*fn)) 
           continue;
       if (!myncmp(fn, cc_warn_table[i].fn, cc_warn_table[i].l))
           return 1;
   }
   return 0;
}

int check_dccsend(aClient *from, aClient *to, char *msg)
{
   /* we already know that msg will consist of "DCC SEND" so we can skip to the end */
   char *filename = msg + 8;
   char *ext;
   char **farray;
   int arraysz;
   int len = 0, extlen = 0, i;

   /* people can send themselves stuff all the like..
    * opers need to be able to send cleaner files
    * sanity checks..
    */

   if(from == to || !IsPerson(from) || IsAnOper(from) || !MyClient(to))
      return 0;

   while(*filename == ' ')
      filename++;

   if(!(*filename)) return 0;

   while(*(filename + len) != ' ')
   {
      if(!(*(filename + len))) break;
      len++;
   }

   for(ext = filename + len;; ext--)
   {
      if(ext == filename)
          return 0;

      if(*ext == '.')
      {
         ext++;
         extlen--;
         break;
      }
      extlen++;
   }

   switch(extlen)
   {
      case 2:
         farray = exploits_2char;
         arraysz = 2;
         break;

      case 3:
         farray = exploits_3char;
         arraysz = 3;
         break;

      /* no executable file here.. */
      default:
         return 0;
   }

   for(i = 0; farray[i]; i++)
   {
      if(myncmp(farray[i], ext, arraysz) == 0)
         break;
   }

   if(farray[i] == NULL)
      return 0;

   if(!allow_dcc(to, from))
   {
      char tmpext[8];
      char tmpfn[128];
      Link *tlp, *flp;
      aChannel *chptr = NULL;

      strncpy(tmpext, ext, extlen);
      tmpext[extlen] = '\0';

      if(len > 127)
         len = 127;
      strncpy(tmpfn, filename, len);
      tmpfn[len] = '\0';

      /* use notices!
       *   server notices are hard to script around.
       *   server notices are not ignored by clients.
       */

      sendto_one(from, ":%s NOTICE %s :The user %s is not accepting DCC sends of filetype *.%s from you."
                       " Your file %s was not sent.",
         me.name, from->name, to->name, tmpext, tmpfn);

      sendto_one(to, ":%s NOTICE %s :%s (%s@%s) has attempted to send you a file named %s, which was blocked.",
         me.name, to->name, from->name, from->user->username, UGETHOST(to, from->user), tmpfn);

      /* Build and encode the DCCOK data */
      do {
         char buffer[1024], *fval = filename;
         u_int32_t addr_val, size_val;
         u_int16_t port_val;
         int j = 0;

         memset(buffer, 0, 1024);
         for(;*fval && (j < 32);) {
             if ((isspace(*fval) || !*fval || (!isalnum(*fval) && !ispunct(*fval))))
                 break;
             buffer[j++] = (*(fval++));
         }
         if (j >= 32 || *fval != ' ')
              break;
         buffer[j++] = ' ';
         if (!isdigit(*(++fval)))
             break;
         addr_val = strtoul(fval, &fval, 10);
         if (*fval != ' ')
             break;
         if (!isdigit(*(++fval)))
             break;
         port_val = strtoul(fval, &fval, 10);
         if (*fval != ' ' && *fval != '\001')
             break;
         if (isdigit(*(fval))) {
             size_val = strtoul(fval, &fval, 10);
         }
         else size_val = 0;

         if (1) {
             int k = 0, ii, n_mask = 0x000000ff, sp = j, l;

             for (ii = 0; ii < 4; ii++) {
                 for (k = 0, l = 0; (ii != 1) ? (k < 4) : (k < 2); k++, l++) {
                    switch(ii) {
                       default: break;

                       /* Write out the bits for the address, port, and size */
                       case 0: buffer[j++] = (addr_val & (n_mask << (k * 8))) >> (k * 8); break;
                       case 1: buffer[j++] = (port_val & (n_mask << (k * 8))) >> (k * 8); break;
                       case 2: buffer[j++] = (size_val & (n_mask << (k * 8))) >> (k * 8); break;

                       /* Write out a checksum */
                       case 3: buffer[j++] = buffer[sp+l] ^ buffer[sp+l+1];
                               l++;
                               break;
                    }
                 }
             }
	 }

         sendto_one(to, ":%s NOTICE %s :Type: `` /QUOTE DCCOK %s %s '' to override the system and allow the DCC transfer."
                        "", me.name, to->name, from->name, b64Encode(buffer, j));
         fval++;
      } while(0);

      if(!SeenDCCNotice(to))
      {
         SetDCCNotice(to);

         sendto_one(to, ":%s NOTICE %s :Many files sent of this type are malicious virii and trojan horses."
                        " In order to reduce the spread of trojans, we are blocking DCC sends of these types of"
                        " files by default.", me.name, to->name);
         sendto_one(to, ":%s NOTICE %s :If you trust %s, and want him/her to send you this file, you may obtain"
                        " more information on using the dccallow system by typing /dccallow help",
                        me.name, to->name, from->name, to->name);
      }

      for(tlp = to->user->channel; tlp && !chptr; tlp = tlp->next)
      {
         for(flp = from->user->channel; flp && !chptr; flp = flp->next)
         {
            if(tlp->value.chptr == flp->value.chptr)
               chptr = tlp->value.chptr;
         }
      }

      if(chptr)
         /* sendto_umode(U_FLOOD, "*** Notice -- %s (%s@%s) sending forbidden filetyped file %s to %s (channel %s)", from->name,
            from->user->username, from->user->host, tmpfn, to->name, chptr->chname);*/
           ;
      else {
          if (dcc_warntable(tmpfn))
             sendto_umode(FLAGSET_DCCALLOW, "*** Notice -- %s (%s@%s) sending 'specific bad file' %s to %s hit dccallow block.", from->name,
                 from->user->username, from->user->host, tmpfn, to->name);
      }
      return 1;
    }

    return 0;
}

int add_dccallow(aClient *sptr, aClient *optr)
{
   Link *lp;
   int cnt = 0;

   for(lp = sptr->user->dccallow; lp; lp = lp->next)
   {
      if(lp->flags != DCC_LINK_ME)
         continue;
      if(++cnt >= MAXDCCALLOW)
      {
         sendto_one(sptr, err_str(ERR_TOOMANYDCC), me.name, sptr->name, optr->name, MAXDCCALLOW);
         return 0;
      }
      else if(lp->value.cptr == optr)
      {
         /* silently return */
         return 0;
      }
   }

   lp = make_link();
   lp->value.cptr = optr;
   lp->flags = DCC_LINK_ME;
   lp->next = sptr->user->dccallow;
   sptr->user->dccallow = lp;

   lp = make_link();
   lp->value.cptr = sptr;
   lp->flags = DCC_LINK_REMOTE;
   lp->next = optr->user->dccallow;
   optr->user->dccallow = lp;

   sendto_one(sptr, rpl_str(RPL_DCCSTATUS), me.name, sptr->name, optr->name, "added to");
   return 0;
}

int del_dccallow(aClient *sptr, aClient *optr)
{
   Link **lpp, *lp;
   int found = 0;

   for (lpp = &(sptr->user->dccallow); *lpp; lpp=&((*lpp)->next))
   {
      if((*lpp)->flags != DCC_LINK_ME)
         continue;

      if((*lpp)->value.cptr == optr)
      {
         lp = *lpp;
         *lpp = lp->next;
         free_link(lp);
         found++;
         break;
      }
   }

   if(!found)
   {
      sendto_one(sptr, ":%s %d %s :%s is not in your DCC allow list", me.name, RPL_DCCINFO, sptr->name, optr->name);
      return 0;
   }

   for (found = 0, lpp = &(optr->user->dccallow); *lpp; lpp=&((*lpp)->next))
   {
      if((*lpp)->flags != DCC_LINK_REMOTE)
         continue;

      if((*lpp)->value.cptr == sptr)
      {
         lp = *lpp;
         *lpp = lp->next;
         free_link(lp);
         found++;
         break;
      }
   }

   /*if(!found)
      sendto_realops_lev(DEBUG_LEV, "%s was in dccallowme list of %s but not in dccallowrem list!",
                         optr->name, sptr->name);*/

   sendto_one(sptr, rpl_str(RPL_DCCSTATUS), me.name, sptr->name, optr->name, "removed from");

   return 0;
}

int allow_dcc(aClient *to, aClient *from)
{
   Link *lp;

   if (!IS_SET(ClientUmode(to), U_DCCALLOW))
       return 1;

   for(lp = to->user->dccallow; lp; lp = lp->next)
   {
      if(lp->flags == DCC_LINK_ME && lp->value.cptr == from)
         return 1;
   }
   return 0;
}


int m_dccallow(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
   Link *lp;
   char *p, *s;
   char *cn;
   aClient *acptr, *lastcptr = NULL;
   int didlist = 0, didhelp = 0, didanything = 0;
   char **ptr;
   static char *dcc_help[] = {
      "/DCCALLOW [<+|->nick[,<+|->nick, ...]] [list] [help]",
      "You may allow DCCs of filetypes which are otherwise blocked by the IRC server",
      "by specifying a DCC allow for the user you want to receive files from.",
      "For instance, to allow the user bob to send you file.exe, you would type:",
      "/dccallow +bob",
      "and bob would then be able to send you files. bob will have to resend the file",
      "if the server gave him an error message before you added him to your allow list.",
      "/dccallow -bob",
      "Will do the exact opposite, removing him from your dcc allow list.",
      "/dccallow list",
      "Will list the users currently on your dcc allow list.",
      "",
      "/mode <your nick> -d",
      "Will disable dcc blocking.",
      NULL };

   if(!MyClient(sptr)) return 0; /* don't accept dccallows from servers or clients that aren't mine.. */

   if(parc < 2)
   {
      sendto_one(sptr, ":%s NOTICE %s :No command specified for DCCALLOW. Type /dccallow help for more information.",
                 me.name, sptr->name);
      return 0;
   }

   for (p = NULL, s = strtoken(&p, parv[1], ", "); s; s = strtoken(&p, NULL, ", "))
   {
      if(*s == '+')
      {
         didanything++;
         cn = s + 1;
         if(*cn == '\0')
            continue;

         acptr = find_person(cn, NULL);

         if(acptr == sptr) continue;

         if(!acptr)
         {
            sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, sptr->name, cn);
            continue;
         }

         add_dccallow(sptr, acptr);
      }
      else if(*s == '-')
      {
         didanything++;
         cn = s + 1;
         if(*cn == '\0')
            continue;

         acptr = find_person(cn, NULL);

         if(acptr == sptr) continue;

         if(!acptr)
         {
            sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, sptr->name, cn);
            continue;
         }
         del_dccallow(sptr, acptr);
      }
      else
      {
         if(!didlist && myncmp(s, "list", 4) == 0)
         {
            didanything++;
            didlist++;

            sendto_one(sptr, ":%s %d %s :The following users are on your dcc allow list:",
                       me.name, RPL_DCCINFO, sptr->name);
            for(lp = sptr->user->dccallow; lp; lp = lp->next)
            {
               if(lp->flags == DCC_LINK_REMOTE)
                  continue;
               sendto_one(sptr, ":%s %d %s :%s (%s@%s)", me.name, RPL_DCCLIST, sptr->name,
                          lp->value.cptr->name, lp->value.cptr->user->username, lp->value.cptr->user->host);
            }
            sendto_one(sptr, rpl_str(RPL_ENDOFDCCLIST), me.name, sptr->name, s);         
         }
         else if(!didhelp && myncmp(s, "help", 4) == 0)
         {
            didanything++;
            didhelp++;
            for(ptr = dcc_help; *ptr; ptr++)
               sendto_one(sptr, ":%s %d %s :%s", me.name, RPL_DCCINFO, sptr->name, ptr && *ptr ? *ptr : "");
            sendto_one(sptr, rpl_str(RPL_ENDOFDCCLIST), me.name, sptr->name, s);
         }
      }
   }

   if(!didanything)
   {
      sendto_one(sptr, ":%s NOTICE %s :Invalid syntax for DCCALLOW. Type /dccallow help for more information.",
                 me.name, sptr->name);
      return 0;
   }

   return 0;
}

int m_dccok(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
     if (!MyClient(sptr)) {
         return;
     }
}
