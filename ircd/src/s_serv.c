/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_serv.c (formerly ircd/s_msg.c)
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
static  char sccsid[] = "@(#)s_serv.c	2.55 2/7/94 (C) 1988 University of Oulu, \
Computing Center and Jarkko Oikarinen";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include "userload.h"
#include "patchlevel.h"
#if defined(PCS) || defined(AIX) || defined(DYNIXPTX) || defined(SVR3)
#include <time.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>
#ifndef _WIN32
#include <utmp.h>
#else
#include <io.h>
#endif
#include "md5_iface.h"
#include "h.h"

static	char	buf[BUFSIZE];
extern int oper_access[];

int     max_connection_count = 1, max_client_count = 1;
u_int32_t mask_seed1 = 0x5555142, 
	  mask_seed2 = 0xf435131,
	  mask_seed3 = 0x7332114,
	  mask_seed4 = MASK_SEED;

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
** m_version
**	parv[0] = sender prefix
**	parv[1] = remote server
*/
int	m_version(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	extern	char	serveropts[];

	if (check_registered(sptr))
		return 0;

	if (hunt_server(cptr,sptr,":%s VERSION :%s",1,parc,parv)==HUNTED_ISME)
		sendto_one(sptr, rpl_str(RPL_VERSION), me.name,
			   parv[0], version, debugmode, me.name, serveropts
#ifdef USE_CASETABLES
			   , casetable
#endif
			   );
	return 0;
}

/*
** m_squit
**	parv[0] = sender prefix
**	parv[1] = server name
**	parv[parc-1] = comment
*/
int	m_squit(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aConfItem *aconf;
	char	*server;
	aClient	*acptr;
	char	*comment = (parc > 2 && parv[parc-1]) ?
	             parv[parc-1] : cptr->name;

         if (check_registered(sptr))
                 return 0;

	if (!IsPrivileged(sptr))
	    {
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	    }

	if (parc > 1)
	    {
		server = parv[1];
		/*
		** To accomodate host masking, a squit for a masked server
		** name is expanded if the incoming mask is the same as
		** the server name for that link to the name of link.
		*/
		while ((*server == '*') && IsServer(cptr))
		    {
			aconf = cptr->serv->nline;
			if (!aconf)
				break;
			if (!mycmp(server, my_name_for_link(me.name, aconf)))
				server = cptr->name;
			break; /* WARNING is normal here */
		    }
		/*
		** The following allows wild cards in SQUIT. Only usefull
		** when the command is issued by an oper.
		*/
		for (acptr = client; (acptr = next_client(acptr, server));
		     acptr = acptr->next)
			if (IsServer(acptr) || IsMe(acptr))
				break;
		if (acptr && IsMe(acptr))
		    {
			acptr = cptr;
			server = cptr->sockhost;
		    }
	    }
	else
	    {
		/*
		** This is actually protocol error. But, well, closing
		** the link is very proper answer to that...
		*/
		server = cptr->sockhost;
		acptr = cptr;
	    }

	/*
	** SQUIT semantics is tricky, be careful...
	**
	** The old (irc2.2PL1 and earlier) code just cleans away the
	** server client from the links (because it is never true
	** "cptr == acptr".
	**
	** This logic here works the same way until "SQUIT host" hits
	** the server having the target "host" as local link. Then it
	** will do a real cleanup spewing SQUIT's and QUIT's to all
	** directions, also to the link from which the orinal SQUIT
	** came, generating one unnecessary "SQUIT host" back to that
	** link.
	**
	** One may think that this could be implemented like
	** "hunt_server" (e.g. just pass on "SQUIT" without doing
	** nothing until the server having the link as local is
	** reached). Unfortunately this wouldn't work in the real life,
	** because either target may be unreachable or may not comply
	** with the request. In either case it would leave target in
	** links--no command to clear it away. So, it's better just
	** clean out while going forward, just to be sure.
	**
	** ...of course, even better cleanout would be to QUIT/SQUIT
	** dependant users/servers already on the way out, but
	** currently there is not enough information about remote
	** clients to do this...   --msa
	*/
	if (!acptr)
	    {
		sendto_one(sptr, err_str(ERR_NOSUCHSERVER),
			   me.name, parv[0], server);
		return 0;
	    }
	if (MyClient(sptr) && ((!OPCanGRoute(sptr) && !MyConnect(acptr)) ||
			       (!OPCanLRoute(sptr) && MyConnect(acptr))))
	    {
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	    }
	/*
	**  Notify all opers, if my local link is remotely squitted
	*/
	if (MyConnect(acptr) && !IsAnOper(cptr))
	  {
            sendto_ops("Received SQUIT %s from %s (%s)",
                acptr->name, get_client_name(sptr,FALSE), comment);
	    sendto_serv_butone(&me, ":%s GNOTICE :Received SQUIT %s from %s (%s)",
		me.name, server, get_client_name(sptr,FALSE), comment);
#if defined(USE_SYSLOG) && defined(SYSLOG_SQUIT)
	    syslog(LOG_DEBUG,"SQUIT From %s : %s (%s)",
		   parv[0], server, comment);
#endif
	  }
	else if (MyConnect(acptr)) {
		sendto_ops("Received SQUIT %s from %s (%s)",
			   acptr->name, get_client_name(sptr,FALSE), comment);
		sendto_serv_butone(&me, ":%s GNOTICE :Received SQUIT %s from %s (%s)",
			   me.name, acptr->name, get_client_name(sptr,FALSE), comment);
	}
	if (IsAnOper(sptr)) {
		/*
		 * It was manually /squit'ed by a human being(we hope),
		 * there is a very good chance they don't want us to
		 * reconnect right away.  -Cabal95
		 */
		ClientFlags(acptr) |= FLAGS_SQUIT;
	}

	return exit_client(cptr, acptr, sptr, comment);
    }

/*
** m_server
**	parv[0] = sender prefix
**	parv[1] = servername
**	parv[2] = serverinfo/hopcount
**      parv[3] = serverinfo
*/
int	m_server(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	char	*ch;
	int	i;
	char	info[REALLEN+1], *inpath, *host;
	const char *encr;
	aClient *acptr, *bcptr;
	aConfItem *aconf, *cconf;
	int	hop;

	info[0] = '\0';
	inpath = get_client_name(cptr,FALSE);
	if (parc < 2 || *parv[1] == '\0')
	    {
			sendto_one(cptr,"ERROR :No servername");
			return 0;
	    }
	hop = 0;
	host = parv[1];
	if (parc > 3 && atoi(parv[2]))
	    {
		hop = atoi(parv[2]);
		(void)strncpy(info, parv[3], REALLEN);
		info[REALLEN] = '\0';
	    }
	else if (parc > 2)
	    {
		(void)strncpy(info, parv[2], REALLEN);
		if (parc > 3 && ((i = strlen(info)) < (REALLEN-2)))
		    {
				(void)strcat(info, " ");
				(void)strncat(info, parv[3], REALLEN - i - 2);
				info[REALLEN] = '\0';
		    }
	    }
	/*
	** Check for "FRENCH " infection ;-) (actually this should
	** be replaced with routine to check the hostname syntax in
	** general). [ This check is still needed, even after the parse
	** is fixed, because someone can send "SERVER :foo bar " ].
	** Also, changed to check other "difficult" characters, now
	** that parse lets all through... --msa
	*/
	if (strlen(host) > HOSTLEN)
		host[HOSTLEN] = '\0';
	for (ch = host; *ch; ch++)
		if (*ch <= ' ' || *ch > '~')
			break;
	if (*ch || !index(host, '.'))
	    {
		sendto_one(sptr,"ERROR :Bogus server name (%s)", sptr->name, host);
		sendto_ops("Bogus server name (%s) from %s", host,
			   get_client_name(cptr, TRUE));
		sptr->since += 7;
		return 0;
	    }

	if (IsPerson(cptr))
	    {
		/*
		** A local link that has been identified as a USER
		** tries something fishy... ;-)
		*/
		sendto_one(cptr, err_str(ERR_ALREADYREGISTRED),
			   me.name, parv[0]);
		sendto_one(cptr, ":%s NOTICE %s :Sorry, but your IRC "
			"program doesn't appear to support changing "
			"servers.", me.name, cptr->name);
		sendto_ops("User %s trying to become a server %s",
			   get_client_name(cptr, TRUE),host);
		sptr->since += 7;
		return 0;
	    }
	/* *WHEN* can it be that "cptr != sptr" ????? --msa */
	/* When SERVER command (like now) has prefix. -avalon */
	
	/* Get a pre-peek at the password... */
	if (IsUnknown(cptr))  {
	  aconf = find_conf_servern(host);
	  if (!aconf) {
	    sendto_one(cptr, "ERROR :No Access (No N line) %s", inpath);
	    sendto_realops("Access denied (No N line) %s", inpath);
	    sendto_umode_except(U_SERVNOTICE, U_OPER, "*** Notice -- Access denied (No N line) %s", cptr->name);
	    return exit_client(cptr, cptr, cptr, "No N line");
	  }
#ifdef CRYPT_LINK_PASSWORD
	  /* use first two chars of the password they send in as salt */
	  
	  /* passwd may be NULL. Head it off at the pass... */
	  if(*cptr->passwd)
	    {
	      char    salt[3];
#ifndef USE_DES
	      extern  char *crypt();
#endif
	      
	      salt[0]=aconf->passwd[0];
	      salt[1]=aconf->passwd[1];
	      salt[2]='\0';
	      encr = crypt(cptr->passwd, salt);
	    }
	  else
	    encr = "";
#else
	  encr = cptr->passwd;
#endif  /* CRYPT_LINK_PASSWORD */

#ifdef MD5_PASSWORD
        if (*cptr->passwd && aconf->passwd && toupper(aconf->passwd[0]) == 'M' &&
            toupper(aconf->passwd[1]) == 'D' &&
            toupper(aconf->passwd[2]) == '5' &&
            toupper(aconf->passwd[3]) == '(' &&
            strchr(aconf->passwd, ')')
           )
        {
           aconf->passwd[0] = toupper(aconf->passwd[0]);
           aconf->passwd[1] = toupper(aconf->passwd[1]);
           aconf->passwd[2] = toupper(aconf->passwd[2]);

           encr = ircd_md5_hex(cptr->passwd);
        }
#endif

	  if (*aconf->passwd && !StrEq(aconf->passwd, encr))
	    {
	      sendto_one(cptr, "ERROR :No Access (passwd mismatch) %s",
			 inpath);
	      sendto_realops("Access denied (passwd mismatch) %s", inpath);
	      sendto_umode_except(U_SERVNOTICE, U_OPER, "*** Notice -- Access denied (passwd mismatch) %s", cptr->name);
	      return exit_client(cptr, cptr, cptr, "Bad Password");
	    }
	/*	bzero(cptr->passwd, sizeof(cptr->passwd));*/
	}


	if ((acptr = find_name(host, NULL)))
	    {
                aClient *ocptr;

		/*
		** This link is trying feed me a server that I already have
		** access through another path -- multiple paths not accepted
		** currently, kill this link immeatedly!!
		**
		** Rather than KILL the link which introduced it, KILL the
		** youngest of the two links. -avalon
		*/
		acptr = acptr->from;
		ocptr = (cptr->firsttime > acptr->firsttime) ? acptr : cptr;
		acptr = (cptr->firsttime > acptr->firsttime) ? cptr : acptr;
		sendto_one(acptr,"ERROR :Server %s already exists from %s", 
                host ? host : "<noserver>",
                (ocptr->from ? ocptr->from->name : "<nobody>"));

		sendto_ops("Link %s cancelled, server %s"
                           " already exists from %s",
			   get_client_name(acptr, TRUE), host,
                           (ocptr->from ? ocptr->from->name : "<nobody>"));
		return exit_client(acptr, acptr, acptr, "Server Exists");
	    }
	if ((acptr = find_client(host, NULL)))
	    {
		/*
		** Server trying to use the same name as a person. Would
		** cause a fair bit of confusion. Enough to make it hellish
		** for a while and servers to send stuff to the wrong place.
		*/
		sendto_one(cptr,"ERROR :Nickname %s already exists!", host);
                sendto_realops("Link %s cancelled: Server/nick collision on %s",
                           inpath, host);
                sendto_umode_except(U_SERVNOTICE, U_OPER, "Link %s cancelled: Server/nick collision on %s",
                           cptr->name, host);
		sendto_serv_butone(&me, ":%s GLOBOPS : Link %s cancelled: Server/nick collision on %s",
			   parv[0], inpath, host);
		return exit_client(cptr, cptr, cptr, "Nick as Server");
	    }

	if (IsServer(cptr))
	    {
		/*
		** Server is informing about a new server behind
		** this link. Create REMOTE server structure,
		** add it to list and propagate word to my other
		** server links...
		*/
		if (parc == 1 || info[0] == '\0')
		    {
	  		sendto_one(cptr,
				   "ERROR :No server info specified for %s",
				   host);
	  		return 0;
		    }

		/*
		** See if the newly found server is behind a guaranteed
		** leaf (L-line). If so, close the link.
		*/
		if ((aconf = find_conf_host(cptr->confs, host, CONF_LEAF)) &&
		    (!aconf->port || (hop > aconf->port)))
		    {
	      		sendto_ops("Leaf-only link %s->%s - Closing",
				   get_client_name(cptr,  TRUE),
				   aconf->host ? aconf->host : "*");
	      		sendto_one(cptr, "ERROR :Leaf-only link, sorry.");
      			return exit_client(cptr, cptr, cptr, "Leaf Only");
		    }
		/*
		**
		*/
		if (!(aconf = find_conf_host(cptr->confs, host, CONF_HUB)) ||
		    (aconf->port && (hop > aconf->port)) )
		    {
			sendto_ops("Non-Hub link %s introduced %s(%s).",
				   get_client_name(cptr,  TRUE), host,
				   aconf ? (aconf->host ? aconf->host : "*") :
				   "!");
			return exit_client(cptr, cptr, cptr,
					   "Too many servers");
		    }
		/*
		** See if the newly found server has a Q line for it in
		** our conf. If it does, lose the link that brought it
		** into our network. Format:
		**
		** Q:<unused>:<reason>:<servername>
		**
		** Example:  Q:*:for the hell of it:eris.Berkeley.EDU
		*/
		if ((aconf = find_conf_name(host, CONF_QUARANTINED_SERVER)))
		    {
			sendto_ops_butone(NULL, &me,
				":%s WALLOPS * :%s brought in %s, %s %s",
				me.name, get_client_name(cptr,FALSE),
				host, "closing link because",
				BadPtr(aconf->passwd) ? "reason unspecified" :
				aconf->passwd);

			sendto_one(cptr,
				   "ERROR :%s is not welcome: %s. %s",
				   host, BadPtr(aconf->passwd) ?
				   "reason unspecified" : aconf->passwd,
				   "Try another network");

			return exit_client(cptr, cptr, cptr, "Q-Lined Server");
		    }

		acptr = make_client(cptr);
		(void)make_server(acptr);
		acptr->hopcount = hop;
		strncpyzt(acptr->name, host, sizeof(acptr->name));
		strncpyzt(acptr->info, info, sizeof(acptr->info));
		strncpyzt(acptr->serv->up, parv[0], sizeof(acptr->serv->up));
		SetServer(acptr);
		ClientFlags(acptr) |=FLAGS_TS8;
		add_client_to_list(acptr);
		(void)add_to_client_hash_table(acptr->name, acptr);
		/*
		** Old sendto_serv_but_one() call removed because we now
		** need to send different names to different servers
		** (domain name matching)
		*/
		for (i = 0; i <= highest_fd; i++)
		    {
			if (!(bcptr = local[i]) || !IsServer(bcptr) ||
			    bcptr == cptr || IsMe(bcptr))
				continue;
			if (!(aconf = bcptr->serv->nline))
			    {
				sendto_ops("Lost N-line for %s on %s. Closing",
					   get_client_name(cptr, TRUE), host);
				return exit_client(cptr, cptr, cptr,
						   "Lost N line");
			    }
			if (match(my_name_for_link(me.name, aconf),
				    acptr->name) == 0)
				continue;
			sendto_one(bcptr, ":%s SERVER %s %d :%s",
				   parv[0], acptr->name, hop+1, acptr->info);
		    }
		/* Check for U-line status -- Barubary */
		if (find_conf_host(cptr->confs, acptr->name, CONF_UWORLD))
			ClientFlags(acptr) |= FLAGS_ULINE;
		return 0;
	    }

	if (!IsUnknown(cptr) && !IsHandshake(cptr))
		return 0;
	/*
	** A local link that is still in undefined state wants
	** to be a SERVER. Check if this is allowed and change
	** status accordingly...
	*/
	strncpyzt(cptr->name, host, sizeof(cptr->name));
	strncpyzt(cptr->info, info[0] ? info:me.name, sizeof(cptr->info));
	cptr->hopcount = hop;

	/* check connection rules */
	for (cconf = conf; cconf; cconf = cconf->next)
	  if ((cconf->status == CONF_CRULEALL) &&
	      (match(cconf->host, host) == 0))
	    if (crule_eval (cconf->passwd))
	      {
		ircstp->is_ref++;
		sendto_ops("Refused connection from %s.",
			   get_client_host(cptr));
		return exit_client(cptr, cptr, cptr,
				   "Disallowed by connection rule");
	      }

	switch (check_server_init(cptr))
	{
	  case 0 : /* Server is OK to connect to network */
	    return m_server_estab(cptr);
	  default :
	    ircstp->is_ref++;
	    sendto_ops("Received unauthorized connection from %s.", get_client_host(cptr));
	    sendto_serv_butone(&me, ":%s GLOBOPS :Recieved unauthorized connection from %s.",
			       parv[0], get_client_host(cptr));
	    return exit_client(cptr, cptr, cptr, "No C/N conf lines");
	}

}

int	m_server_estab(aClient *cptr)
{
	aClient   *acptr;
	aConfItem *aconf, *bconf;
	const char *encr;
	char      *inpath, *host, *s;
	int       split, i;

	inpath = get_client_name(cptr,TRUE); /* "refresh" inpath with host */
	split = mycmp(cptr->name, cptr->sockhost);
	host = cptr->name;

	current_load_data.conn_count++;
	update_load();

	/* Look for the N:line, dump the connection if it's not there */
	if (!(aconf = find_conf(cptr->confs, host, CONF_NOCONNECT_SERVER)))
	    {
		ircstp->is_ref++;
		sendto_one(cptr, "ERROR :Access denied. No N line for server %s", inpath);
		sendto_realops("Access denied. No N line for server %s", inpath);
		sendto_umode_except(U_SERVNOTICE, U_OPER, "*** Notice -- Access denied. No N line for server %s", cptr->name);
		return exit_client(cptr, cptr, cptr, "No N line for server");
	    }
	/*
	 * We have an N:line, now look for the C:line
	 * and dump the connection if it's not there
	 */
	if (!(bconf = find_conf(cptr->confs, host, CONF_CONNECT_SERVER)))
	    {
		ircstp->is_ref++;
		sendto_one(cptr, "ERROR :Only N (no C) field for server %s", inpath);
		sendto_realops("Only N (no C) field for server %s", inpath);
		sendto_umode_except(U_SERVNOTICE, U_OPER, "*** Notice -- Only N (no C) field for server %s", cptr->name);
		return exit_client(cptr, cptr, cptr, "No C line for server");
	    }

#ifdef CRYPT_LINK_PASSWORD
	/* use first two chars of the password they send in as salt */

	/* passwd may be NULL. Head it off at the pass... */
	if(*cptr->passwd)
	    {
		char    salt[3];
		extern  char *crypt();

		salt[0]=aconf->passwd[0];
		salt[1]=aconf->passwd[1];
		salt[2]='\0';
		encr = crypt(cptr->passwd, salt);
	    }
	else
		encr = "";
#else
	encr = cptr->passwd;
#endif  /* CRYPT_LINK_PASSWORD */

#ifdef MD5_PASSWORD
        if (*cptr->passwd && aconf->passwd && toupper(aconf->passwd[0]) == 'M' &&
            toupper(aconf->passwd[1]) == 'D' &&
            toupper(aconf->passwd[2]) == '5' &&
            toupper(aconf->passwd[3]) == '(' &&
            strchr(aconf->passwd, ')')
           )
        {
           aconf->passwd[0] = toupper(aconf->passwd[0]);
           aconf->passwd[1] = toupper(aconf->passwd[1]);
           aconf->passwd[2] = toupper(aconf->passwd[2]);

           encr = ircd_md5_hex(cptr->passwd);
        }
#endif

	/* Eek, password doesn't match */
	if (*aconf->passwd && !StrEq(aconf->passwd, encr))
	    {
		ircstp->is_ref++;
		sendto_one(cptr, "ERROR :No Access (passwd mismatch) %s", inpath);
		sendto_realops("Access denied (passwd mismatch) %s", inpath);
		sendto_umode_except(U_SERVNOTICE, U_OPER, "*** Notice -- Access denied (passwd mismatch) %s", cptr->name);
		return exit_client(cptr, cptr, cptr, "Bad Password");
	    }
	bzero(cptr->passwd, sizeof(cptr->passwd));

#ifndef	HUB
	/*
	 * A leaf can never have more than one server connection,
	 * so check existing clients and discard the new connection
	 * if a server already exists.
	 */
	for (i = 0; i <= highest_fd; i++)
		if (local[i] && IsServer(local[i]))
		    {
			ircstp->is_ref++;
			sendto_one(cptr, "ERROR :I'm a leaf not a hub");
			return exit_client(cptr, cptr, cptr, "I'm a leaf");
		    }
#endif
	if (IsUnknown(cptr))
	    {
		if (bconf->passwd[0])
			sendto_one(cptr,"PASS :%s",bconf->passwd);
		/*
		 * Pass my info to the new server
		 */
		sendto_one(cptr, "CAPAB :VER=%x SEED=%lu NTIME=%ld", SORIRC_VERSION, mask_seed4, time(NULL));
		sendto_one(cptr, "SERVER %s 1 :%s",
			   my_name_for_link(me.name, aconf), 
			   (me.info[0]) ? (me.info) : "IRCers United");	
	    }
	else
	    {
		s = (char *)index(aconf->host, '@');
		*s = '\0'; /* should never be NULL */
		Debug((DEBUG_INFO, "Check Usernames [%s]vs[%s]",
			aconf->host, cptr->username));
		if (match(aconf->host, cptr->username))
		    {
			*s = '@';
			ircstp->is_ref++;
			sendto_ops("Username mismatch [%s]v[%s] : %s",
				   aconf->host, cptr->username,
				   get_client_name(cptr, TRUE));
			sendto_one(cptr, "ERROR :No Username Match");
			return exit_client(cptr, cptr, cptr, "Bad User");
		    }
		*s = '@';
	    }

	det_confs_butmask(cptr, CONF_LEAF|CONF_HUB|CONF_NOCONNECT_SERVER|CONF_UWORLD);
	/*
	** *WARNING*
	** 	In the following code in place of plain server's
	**	name we send what is returned by get_client_name
	**	which may add the "sockhost" after the name. It's
	**	*very* *important* that there is a SPACE between
	**	the name and sockhost (if present). The receiving
	**	server will start the information field from this
	**	first blank and thus puts the sockhost into info.
	**	...a bit tricky, but you have been warned, besides
	**	code is more neat this way...  --msa
	*/
	SetServer(cptr);
	ClientFlags(cptr) |= FLAGS_TS8;
	nextping = NOW;
#ifdef HUB
	sendto_serv_butone(&me, ":%s GLOBOPS :Link with %s established.",
		me.name, inpath);
#endif
	sendto_realops("Link with %s established.", inpath);
	sendto_umode_except(U_SERVNOTICE, U_OPER, "*** Notice -- Link with %s established.", cptr->name);
	/* Insert here */
	(void)add_to_client_hash_table(cptr->name, cptr);
	/* doesnt duplicate cptr->serv if allocted this struct already */
	(void)make_server(cptr);
	(void)strcpy(cptr->serv->up, me.name);
	cptr->serv->nline = aconf;
       if (find_conf_host(cptr->confs, cptr->name, CONF_UWORLD))
                       cptr->flags |= FLAGS_ULINE;

	/*
	** Old sendto_serv_but_one() call removed because we now
	** need to send different names to different servers
	** (domain name matching) Send new server to other servers.
	*/
	for (i = 0; i <= highest_fd; i++) 
	    {
		if (!(acptr = local[i]) || !IsServer(acptr) ||
		    acptr == cptr || IsMe(acptr))
			continue;
		if ((aconf = acptr->serv->nline) &&
		    !match(my_name_for_link(me.name, aconf), cptr->name))
			continue;
		if (split) {
			sendto_one(acptr,":%s SERVER %s 2 :[%s] %s",
				   me.name, cptr->name,
				   cptr->sockhost, cptr->info);
                }
		else
			sendto_one(acptr,":%s SERVER %s 2 :%s",
				   me.name, cptr->name, cptr->info);
	    }

	/*
	** Pass on my client information to the new server
	**
	** First, pass only servers (idea is that if the link gets
	** cancelled beacause the server was already there,
	** there are no NICK's to be cancelled...). Of course,
	** if cancellation occurs, all this info is sent anyway,
	** and I guess the link dies when a read is attempted...? --msa
	** 
	** Note: Link cancellation to occur at this point means
	** that at least two servers from my fragment are building
	** up connection this other fragment at the same time, it's
	** a race condition, not the normal way of operation...
	**
	** ALSO NOTE: using the get_client_name for server names--
	**	see previous *WARNING*!!! (Also, original inpath
	**	is destroyed...)
	*/

	aconf = cptr->serv->nline;
	for (acptr = &me; acptr; acptr = acptr->prev)
	    {
		/* acptr->from == acptr for acptr == cptr */
		if (acptr->from == cptr)
			continue;
		if (IsServer(acptr))
		    {
			if (match(my_name_for_link(me.name, aconf),
				    acptr->name) == 0)
				continue;
			split = (MyConnect(acptr) &&
				 mycmp(acptr->name, acptr->sockhost));
			if (split)
				sendto_one(cptr, ":%s SERVER %s %d :[%s] %s",
		   			   acptr->serv->up, acptr->name,
					   acptr->hopcount+1,
		   			   acptr->sockhost, acptr->info);
			else
				sendto_one(cptr, ":%s SERVER %s %d :%s",
		   			   acptr->serv->up, acptr->name,
					   acptr->hopcount+1, acptr->info);
		    }
	    }

	update_time();
	for (acptr = &me; acptr; acptr = acptr->prev)
	    {
		/* acptr->from == acptr for acptr == cptr */
		if (acptr->from == cptr)
			continue;
		if (IsPerson(acptr))
		    {
			/*
			** IsPerson(x) is true only when IsClient(x) is true.
			** These are only true when *BOTH* NICK and USER have
			** been received. -avalon
			** Apparently USER command was forgotten... -Donwulff
			*/
			/*sendto_one(cptr,"NICK %s %d %d %s %s %s :%s",
			  acptr->name, acptr->hopcount + 1, acptr->lastnick,
			  acptr->user->username, acptr->user->host,
			  acptr->user->server, acptr->info);
			  send_umode(cptr, acptr, acptr, 0, SEND_UMODES, buf);
			*/
			introduce_user(cptr, acptr);
			if (acptr->user->away)
				sendto_one(cptr,":%s AWAY :%s", acptr->name,
				   acptr->user->away);
			if (IsHurt(acptr))
				sendto_one(cptr,":%s HURTSET %s %ld", me.name,
				   acptr->name, acptr->hurt);

			send_user_joins(cptr, acptr);
		    }
		else if (IsService(acptr))
		    {
			sendto_one(cptr,"NICK %s :%d",
				   acptr->name, acptr->hopcount + 1);
			sendto_one(cptr,":%s SERVICE * * :%s",
				   acptr->name, acptr->info);
		    }
	    }
	/*
	** Last, pass all channels plus statuses
	*/
	{
		aChannel *chptr;
		for (chptr = channel; chptr; chptr = chptr->nextch)
		   {
			send_channel_modes(cptr, chptr);
			if (chptr->topic_time)
				sendto_one(cptr,":%s TOPIC %s %s %lu :%s",
					   me.name, chptr->chname, chptr->topic_nick,
					   chptr->topic_time, chptr->topic);
		   }
	}

        /* HelpOp ignores */
        if (h_nignores && h_ignores)
            for (i = 0 ; i < h_nignores; i++)
                 sendto_one(cptr, ":%s HELP :+ignore %s", me.name, h_ignores[i]);
	return 0;
}

/*
** m_info
**	parv[0] = sender prefix
**	parv[1] = servername
*/
int	m_info(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	char **text = infotext;

	if (check_registered(sptr))
		return 0;

	if (hunt_server(cptr,sptr,":%s INFO :%s",1,parc,parv) == HUNTED_ISME)
	    {
		while (*text)
			sendto_one(sptr, rpl_str(RPL_INFO),
				   me.name, parv[0], *text++);

		sendto_one(sptr, rpl_str(RPL_INFO), me.name, parv[0], "");
		sendto_one(sptr,
			   ":%s %d %s :Birth Date: %s, compile # %s",
			   me.name, RPL_INFO, parv[0], creation, generation);
		sendto_one(sptr, ":%s %d %s :On-line since %s",
			   me.name, RPL_INFO, parv[0],
			   myctime(me.firsttime));
		sendto_one(sptr, rpl_str(RPL_ENDOFINFO), me.name, parv[0]);
	    }

    return 0;
}

/*
** m_links
**	parv[0] = sender prefix
**	parv[1] = servername mask
** or
**	parv[0] = sender prefix
**	parv[1] = server to query 
**      parv[2] = servername mask
*/
int	m_links(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	char *mask;
	aClient *acptr;

	if (check_registered_user(sptr))
		return 0;
    
	if (parc > 2)
	    {
		if (hunt_server(cptr, sptr, ":%s LINKS %s :%s", 1, parc, parv)
				!= HUNTED_ISME)
			return 0;
		mask = parv[2];
	    }
	else
		mask = parc < 2 ? NULL : parv[1];

	for (acptr = client, (void)collapse(mask); acptr; acptr = acptr->next) 
	    {
		if (!IsServer(acptr) && !IsMe(acptr))
			continue;
		if (!BadPtr(mask) && match(mask, acptr->name))
			continue;
		if (!IsOper(sptr) && acptr->info[0] == '[') {
			int i, ippfx;

			for(i = 1, ippfx = 0; acptr->info[i] && (i < 17); i++) {
			    if (acptr->info[i] == ']' && (i >= 7)) {
				ippfx = 1;
			        break;
			    }
			    if (acptr->info[i] != '.' && !isdigit(acptr->info[i]))
				break;
			}

			sendto_one(sptr, rpl_str(RPL_LINKS),
				   me.name, parv[0], acptr->name, acptr->serv->up,
				   acptr->hopcount, (acptr->info[0] ? ((ippfx == 1) ? acptr->info + i : acptr->info) :
				   "(Unknown Location)"));
		}
		else
		sendto_one(sptr, rpl_str(RPL_LINKS),
			   me.name, parv[0], acptr->name, acptr->serv->up,
			   acptr->hopcount, (acptr->info[0] ? acptr->info :
			   "(Unknown Location)"));
	    }

	sendto_one(sptr, rpl_str(RPL_ENDOFLINKS), me.name, parv[0],
		   BadPtr(mask) ? "*" : mask);
	return 0;
}


/*
** m_stats
**	parv[0] = sender prefix
**	parv[1] = statistics selector (defaults to Message frequency)
**	parv[2] = server name (current server defaulted, if omitted)
**
**	Currently supported are:
**		M = Message frequency (the old stat behaviour)
**		L = Local Link statistics
**              C = Report C and N configuration lines
*/
/*
** m_stats/stats_conf
**    Report N/C-configuration lines from this server. This could
**    report other configuration lines too, but converting the
**    status back to "char" is a bit akward--not worth the code
**    it needs...
**
**    Note:   The info is reported in the order the server uses
**            it--not reversed as in ircd.conf!
*/

static int report_array[19][3] = {
		{ CONF_CONNECT_SERVER,    RPL_STATSCLINE, 'C'},
		{ CONF_NOCONNECT_SERVER,  RPL_STATSNLINE, 'N'},
		{ CONF_CLIENT,            RPL_STATSILINE, 'I'},
		{ CONF_AHURT,		  RPL_STATSKLINE, 'h'},
		{ CONF_KILL,              RPL_STATSKLINE, 'K'},
		{ CONF_ZAP,		  RPL_STATSKLINE, 'Z'},
		{ CONF_QUARANTINED_SERVER,RPL_STATSQLINE, 'q'},
		{ CONF_QUARANTINED_NICK,  RPL_STATSQLINE, 'Q'},
		{ CONF_LEAF,		  RPL_STATSLLINE, 'L'},
		{ CONF_OPERATOR,	  RPL_STATSOLINE, 'O'},
		{ CONF_HUB,		  RPL_STATSHLINE, 'H'},
		{ CONF_LOCOP,		  RPL_STATSOLINE, 'o'},
		{ CONF_CRULEALL,	  RPL_STATSDLINE, 'D'},
		{ CONF_CRULEAUTO,	  RPL_STATSDLINE, 'd'},
		{ CONF_SERVICE,		  RPL_STATSGENLINE, 'S'},
		{ CONF_UWORLD,		  RPL_STATSULINE, 'U'},
		{ CONF_MISSING,		  RPL_STATSXLINE, 'X'},
		{ CONF_TRIGGER,		  RPL_STATSGENLINE, 't'},
		{ 0, 0, 0 }
};

static	void	report_configured_links(aClient *sptr, int mask)
{
	static	char	null[] = "<NULL>";
	aConfItem *tmp;
	int	*p, port, tmpmask;
	char	c, *host, *pass, *name, *pt;
        char oflag_list[BUFSIZE+5];
        unsigned        long    int     oflagset                 = 0;
        int     *s;
	
	tmpmask = (mask == CONF_MISSING) ? CONF_CONNECT_SERVER : mask;

	for (tmp = conf; tmp; tmp = tmp->next)
		if (tmp->status & tmpmask)
		    {
			for (p = &report_array[0][0]; *p; p += 3)
				if (*p == tmp->status)
					break;
			if (!*p)
				continue;
			c = (char)*(p+2);
			host = BadPtr(tmp->host) ? null : tmp->host;
			pass = BadPtr(tmp->passwd) ? null : tmp->passwd;
			name = BadPtr(tmp->name) ? null : tmp->name;
			port = (int)tmp->port;


			if (tmp->status == CONF_KILL &&
			    tmp->tmpconf == KLINE_AKILL && !IsAnOper(sptr))
			    continue;

                        if (tmp->status == CONF_OPERATOR ||
                            tmp->status == CONF_LOCOP    ||
                            p[1] == RPL_STATSOLINE)
                        {
                             oflagset = 0;
                             oflag_list[0] = '\0';
                             pt = (char *) &oflag_list;
        
                        while(1) /* fake loop to use as an escape... */
                        {
                           if (tmp->port & OFLAG_ADMIN)
                           {
                              *pt++ = 'A';
                              *pt++ = 'O';
                              oflagset |= (OFLAG_ADMIN|OFLAG_GLOBAL);
                              break;
                           }
        
                           if ((tmp->port & OFLAG_GLOBAL) == OFLAG_GLOBAL)
                           {
                                *pt++ = 'O';
                              oflagset |= (OFLAG_GLOBAL);
                              if (tmp->port == OFLAG_GLOBAL) break;
                           }
                           else                                                         
                              if ((tmp->port & OFLAG_LOCAL) == OFLAG_LOCAL)
                              {
                                *pt++ = 'o';
                                oflagset |= (OFLAG_LOCAL);
                                if (tmp->port == OFLAG_LOCAL) break;
                              }
                          break;
                       }
                             for ( s = (int *) &oper_access[10] ; *s ; s += 2 )
                             { 
                                    if (*(s+1) == '*') {
                                          continue;
                                     }                               
                                  if ( ((tmp->port & *s) == *s))
                                  {
                                     if (!(oflagset & *s))
                                       *pt++ = *(s+1);
                                     else
                                       oflagset |= *s;
                                    
                                  }
                             }

                                  *pt++ = '\0';
                                     
                              /*  RPL_STATSOLINE, "%c %s * %s %s %d", */
				if (IsOper(sptr))
	                                sendto_one(sptr, rpl_str(p[1]), me.name,
	                                           sptr->name, c,  host,
	                                           name, oflag_list,
	                                           get_conf_class(tmp) );
				else
	                                sendto_one(sptr, rpl_str(p[1]), me.name,
	                                           sptr->name, c,  "*",
	                                           name, oflag_list,
	                                           get_conf_class(tmp) );
                                 continue;   
                        }
                                                                






			/*
			 * On K line the passwd contents can be
			 * displayed on STATS reply. 	-Vesa
			 */
			/* Same with Z-lines and q/Q-lines -- Barubary */
                        if ((tmp->status & CONF_SHOWPASS))
			{
/* These mods are to tell the difference between the different kinds
 * of klines.  the only effect it has is in the display.  --Russell
 */
/* Now translates spaces to _'s to show comments in klines -- Barubary */
				char *temp;
				if (!pass) strcpy(buf, "<NULL>");
				else {
					strcpy(buf, pass);
					for (temp = buf; *temp; temp++)
					if (*temp == ' ') *temp = '_';
				}
				/* semicolon intentional -- Barubary */
				if (tmp->status == CONF_QUARANTINED_NICK);
				/* Hide password for servers -- Barubary */
				else if (tmp->status & CONF_QUARANTINE)
					strcpy(buf, "*");
				else {
/* This wasn't documented before - comments aren't displayed for akills
   because they are all the same. -- Barubary */
				if (tmp->tmpconf == KLINE_AKILL 
                                         && !(tmp->status == CONF_AHURT))
					strcpy(buf, "*");
				/* KLINE_PERM == 0 - watch out when doing
				   Z-lines. -- Barubary */
				if (tmp->status == CONF_KILL)
                                {
                                   if (tmp->tmpconf == KLINE_PERM) c = 'K';
                                   if (tmp->tmpconf == KLINE_TEMP) c = 'k';
                                   if (tmp->tmpconf == KLINE_AKILL) c = 'A';
                                }
                                 else if (tmp->status == CONF_KILL)
                                 {
                                   if (tmp->tmpconf == KLINE_PERM) c = 'Z';
                                   if (tmp->tmpconf == KLINE_TEMP) c = 'z';
                                   if (tmp->tmpconf == KLINE_AKILL) c = 'S';
                                 }
				}
				sendto_one(sptr, rpl_str(p[1]), me.name,
					   sptr->name, c,  host,
					   buf, name, port,
					   get_conf_class(tmp));
			}
			/* connect rules are classless */
			else if (tmp->status & CONF_CRULE)
				sendto_one(sptr, rpl_str(p[1]), me.name,
					   sptr->name, c, host, name);
			/* Only display on X if server is missing */
			else if (mask == CONF_MISSING) {
			    if (!find_server(name, NULL))
				sendto_one(sptr, rpl_str(RPL_STATSXLINE), me.name,
					   sptr->name, name, port);
			}
			else {
			   if (IsOper(sptr) || (tmp->status != CONF_NOCONNECT_SERVER 
                               && tmp->status != CONF_CONNECT_SERVER
			       && tmp->status != CONF_CLIENT))
				sendto_one(sptr, rpl_str(p[1]), me.name,
					   sptr->name, c, host, name, port,
					   get_conf_class(tmp));
			   else
				sendto_one(sptr, rpl_str(p[1]), me.name,
					   sptr->name, c, "*", name, port,
					   get_conf_class(tmp));
			}
		    }
	return;
}

/* Used to blank out ports -- Barubary */
char *get_client_name2(aClient *acptr, int showports)
{
	char *pointer = get_client_name(acptr, TRUE);

	if (!pointer) return NULL;
	if (showports) return pointer;
	if (!strrchr(pointer, '.')) return NULL;
	strcpy((char *)strrchr(pointer, '.'), ".0]");

	return pointer;
}


char *get_client_name3(aClient *acptr, int showports)
{
	char *pointer = get_client_name(acptr, TRUE), *p2;

	if (!pointer) return NULL;
	if (showports) return pointer;
	if (!(p2 = (char *)strrchr(pointer, '.'))) return NULL;
	strcpy(p2, "]");

	return pointer;
}

char *get_client_namp(char *buf, aClient *acptr, int showports, int mask)
{
	char *pointer = get_client_name_mask(acptr, TRUE, showports, mask);

	return buf ? strcpy(buf, pointer) : pointer;
}


int	m_stats(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
      	static	char	Sformat[]  = ":%s %d %s SendQ SendM SendBytes RcveM RcveBytes Open_since :Idle";
	static	char	Lformat[]  = ":%s %d %s %s %u %u %u %u %u %u :%u";
	struct	Message	*mptr;
	aClient	*acptr;
	char	stat = parc > 1 ? parv[1][0] : '\0';
	int	i;
	int	doall = 0, wilds = 0, showports	= IsAnOper(sptr);
	char	*name;
	char	buf[HOSTLEN + NICKLEN + 255];

	if (check_registered(sptr))
		return 0;

	if (hunt_server(cptr,sptr,":%s STATS %s :%s",2,parc,parv)!=HUNTED_ISME)
		return 0;

	if (parc > 2)
	    {
		name = parv[2];
		if (!mycmp(name, me.name))
			doall = 2;
		else if (match(name, me.name) == 0)
			doall = 1;
		if (index(name, '*') || index(name, '?'))
			wilds = 1;
	    }
	else
		name = me.name;

	switch (stat)
	{
	case 'L' : case 'l' :
		/*
		 * send info about connections which match, or all if the
		 * mask matches me.name.  Only restrictions are on those who
		 * are invisible not being visible to 'foreigners' who use
		 * a wild card based search to list it.
		 */
	  	sendto_one(sptr, Sformat, me.name, RPL_STATSLINKINFO, parv[0]);
		if (IsServer(cptr))
			doall = wilds = 0;
		for (i = 0; i <= highest_fd; i++)
		    {
			if (!(acptr = local[i]))
				continue;
			if (IsInvisible(acptr) && (doall || wilds) &&
			    !(MyConnect(sptr) && IsOper(sptr)) &&
			    !IsAnOper(acptr) && (acptr != sptr))
				continue;
			if (!doall && wilds && match(name, acptr->name))
				continue;
			if (!(doall || wilds) && mycmp(name, acptr->name) &&
			    !IsServer(acptr))
				continue;
			if (!(doall || wilds) && parc > 2 && IsServer(acptr) &&
			    !IsServer(cptr))
				continue;
			#define CanSeeReal(sptr, acptr) \
				((sptr==acptr) || IsOper(sptr) || (!IsListening(acptr) && !IsMe(acptr) && !IsServer(acptr) && !IsMasked(acptr)))
			get_client_namp(buf, acptr, isupper(stat) ? showports : FALSE, !CanSeeReal(sptr, acptr));
			sendto_one(sptr, Lformat, me.name,
				   RPL_STATSLINKINFO, parv[0],
				   buf,
				   (int)DBufLength(&acptr->sendQ),
				   (int)acptr->sendM, (int)acptr->sendK,
				   (int)acptr->receiveM, (int)acptr->receiveK,
				   NOW - acptr->firsttime,
				   (acptr->user && MyConnect(acptr)) ?
				    NOW - acptr->user->last : 0);
		    }
		break;
	case 'C' : case 'c' :
                report_configured_links(sptr, CONF_CONNECT_SERVER|
					CONF_NOCONNECT_SERVER);
		break;
	case 'H' : case 'h':
                report_configured_links(sptr, CONF_HUB|CONF_LEAF);
		break;
	case 'I' : case 'i' :
		report_configured_links(sptr, CONF_CLIENT);
		break;
	case 'K' : case 'k' :
		if (!IsAnOper(sptr))
			report_configured_links(sptr, CONF_KILL);
		else
			report_configured_links(sptr, CONF_KILL|CONF_ZAP|CONF_AHURT);
		break;
	case 'M' : case 'm' :
		for (mptr = msgtab; mptr->cmd; mptr++)
			if (mptr->count)
				sendto_one(sptr, rpl_str(RPL_STATSCOMMANDS),
					   me.name, parv[0], mptr->cmd,
					   mptr->count, mptr->bytes);
		break;
	case 'o' : case 'O' :
		report_configured_links(sptr, CONF_OPS);
		break;
	case 'Q' : case 'q' :
		report_configured_links(sptr, CONF_QUARANTINE);
		break;
	case 'R' : case 'r' :
#ifdef DEBUGMODE
		send_usage(sptr,parv[0]);
#endif
		break;
	case 'D' :
		report_configured_links(sptr, CONF_CRULEALL);
		break;
	case 'd' :
		report_configured_links(sptr, CONF_CRULE);
		break;
	case 'S' : case 's' :
		report_configured_links(sptr, CONF_SERVICE);
		break;
	case 't' :
		if (IsOper(sptr))
			report_configured_links(sptr, CONF_TRIGGER);
		break;
	case 'T' :
		tstats(sptr, parv[0]);
		break;
	case 'U' :
                report_configured_links(sptr, CONF_UWORLD);
		break;
	case 'u' :
	    {
		time_t now;

		now = NOW - me.since;
		sendto_one(sptr, rpl_str(RPL_STATSUPTIME), me.name, parv[0],
			   now/86400, (now/3600)%24, (now/60)%60, now%60);
                sendto_one(sptr, rpl_str(RPL_STATSCONN), me.name, parv[0],
			   max_connection_count, max_client_count);
		break;
	    }
        case 'W' : case 'w' :
                calc_load(sptr, parv[0]);
                break;
	case 'X' : case 'x' :
		report_configured_links(sptr, CONF_MISSING);
		break;
	case 'Y' : case 'y' :
		report_classes(sptr);
		break;
	case 'Z' : case 'z' :
		count_memory(sptr, parv[0]);
		break;
	default :
		stat = '*';
		break;
	}
	sendto_one(sptr, rpl_str(RPL_ENDOFSTATS), me.name, parv[0], stat);
	return 0;
    }


/*
** Note: At least at protocol level ERROR has only one parameter,
** although this is called internally from other functions
** --msa
**
**	parv[0] = sender prefix
**	parv[*] = parameters
*/
int	m_error(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	char	*para;

	para = (parc > 1 && *parv[1] != '\0') ? parv[1] : "<>";

	Debug((DEBUG_ERROR,"Received ERROR message from %s: %s",
	      sptr->name, para));
	/*
	** Ignore error messages generated by normal user clients
	** (because ill-behaving user clients would flood opers
	** screen otherwise). Pass ERROR's from other sources to
	** the local operator...
	*/
	if (IsPerson(cptr) || IsUnknown(cptr) || IsService(cptr))
		return 0;
	if (cptr == sptr) {
		sendto_serv_butone(&me, ":%s GLOBOPS :ERROR from %s -- %s",
			   me.name, get_client_name(cptr, FALSE), para);
		sendto_realops("ERROR :from %s -- %s",
			   get_client_name(cptr, FALSE), para);
	}
	else {
                sendto_serv_butone(&me, ":%s GLOBOPS :ERROR from %s via %s -- %s",
                           me.name, sptr->name, get_client_name(cptr, FALSE), para);
		sendto_realops("ERROR :from %s via %s -- %s", sptr->name,
			   get_client_name(cptr,FALSE), para);
	}
	return 0;
    }

/*
** m_help (help/write to +h currently online) -Donwulff
**	parv[0] = sender prefix
**	parv[1] = optional message text
*/
int	m_help(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	int i;
        char    *message, *pv[4];

         if (check_registered(sptr))
                 return 0;
  
	message = parc > 1 ? parv[1] : NULL;

#if 0
	if (BadPtr(message) && MyClient(cptr))
	{
	      if (parse_help(sptr, parv[0], "INDEX") < 0)
	           sendto_one(sptr, ":%s NOTICE %s :!ERROR! Help Database is Missing!", me.name, parv[0]);
	      return 0;
	}
#endif

	if (BadPtr(message) || !mycmp(message, "msgtab"))
	{
 	   for (i = 0; msgtab[i].cmd; i++)
               if ((!IsHurt(sptr)||(msgtab[i].while_hurt>0))&&!(msgtab[i].flags & MF_H))
		   sendto_one(sptr,":%s NOTICE %s :%s",
			      me.name, parv[0], msgtab[i].cmd);
	   return 0;
	}
        if (!mycmp(message, "ignore") && (IsHelpOp(sptr) || IsOper(sptr) || IsServer(sptr)))
        {
            if (!h_nignores)
                  sendto_one(sptr, ":%s NOTICE %s :There are no help ignores.", me.name, parv[0]);
            for ( i = 0 ; i < h_nignores; i++ )
                  sendto_one(sptr, ":%s NOTICE %s :\2%2d\2. %s", me.name, parv[0], i+1, h_ignores[i]);
            return(0);
        }
        if (!myncmp(message, "+ignore ", 7) && (IsHelpOp(sptr) || IsOper(sptr) || IsServer(sptr)))
        {
            if (BadPtr(message+7) || BadPtr(message+8))
            {
                sendto_one(sptr, ":%s NOTICE %s :Ignore WHAT?", me.name, parv[0]);
                return(0);
            }
            if (!match(message+8, ".!......@...")     || 
                !match(message+8, ".!......@.....")   || 
                !match(message+8, ".!......@...net")  ||
                !match(message+8, ".!......@...com")  ||
                !match(message+8, ".!......@...org"))
            {
                sendto_one(sptr, ":%s NOTICE %s :That is not a legal ignore HelpOp mask.", me.name, parv[0]);
                if (IsServer(cptr))
                    sendto_serv_butone(cptr, ":%s HELP %s", parv[0], message);
                return (0);
            }
            helpop_ignore(message + 8);
            if (!IsServer(cptr))
                sendto_one(sptr, ":%s NOTICE %s :Ignore for \2%s\2 set.", me.name, parv[0], message+8);
            else if (IsServer(sptr))
            {
                sendto_serv_butone(cptr, ":%s HELP %s", parv[0], message);
                return (0); /* Don't send helpops for Net.burst */
            }
        }
        if (!myncmp(message, "-ignore ", 7) && (IsHelpOp(sptr) || IsOper(sptr) || IsServer(sptr)))
        {
            for ( i = 0 ; i < h_nignores; i++ )
                  if (!mycmp(message+8, h_ignores[i]))
                      break;
            if ( i >= h_nignores )
            {
                  if (MyClient(sptr))
                      sendto_one(sptr, ":%s NOTICE %s :No such ignore.", me.name, parv[0]);
                  return (0);
            }
            if (!IsServer(cptr))
                sendto_one(sptr, ":%s NOTICE %s :Ignore for \2%s\2 removed.", me.name, parv[0], h_ignores[i]);
            helpop_unignore(i);
        }
        if ((!myncmp(message, "-ignore ", 7) || !myncmp(message, "+ignore ", 7)
            || !mycmp(message, "ignore")) && !(IsOper(sptr) || IsHelpOp(sptr) || IsServer(sptr) ))
        {
            sendto_one(sptr, ":%s NOTICE %s :Permission denied -- You must be a helpop to edit help system ignores.", me.name, parv[0]);
            return (0);
        }

#if 0
	if (!mycmp(message, "topics"))
	{
                 int shown = 0;
		 char tstr[8192] = ""; 
		 sendto_one(sptr, ":%s NOTICE %s :--- Help Topics ---", me.name, sptr->name);
		 #define FLUSH_TSTR { \
			if (tstr[0]) \
				sendto_one(sptr, ":%s NOTICE %s :%s", me.name, sptr->name, tstr); \
				tstr[0] = '\0'; \
				    }
		for (i = 0; i<nhtopics; i++)
		{
			if ((++shown)%2 == 0)
			    FLUSH_TSTR;
			sprintf(tstr, "%s          %-20s", tstr, htopics[i]->command);
		}
		FLUSH_TSTR;
		sendto_one(sptr, ":%s NOTICE %s :--- End of List ---", me.name, sptr->name);
		#undef FLUSH_TSTR
		return 0;
	}
#endif        

/* Drags along from wallops code... I'm not sure what it's supposed to do, 
   at least it won't do that gracefully, whatever it is it does - but
   checking whether or not it's a person _is_ good... -Donwulff */

	if (!IsServer(sptr) && MyConnect(sptr) && !IsPerson(sptr))
	    {
		check_registered(sptr);
		return 0;
	    }

/*	if (IsServer(sptr) || IsHelpOp(sptr)) {
 *		sendto_serv_butone(IsServer(cptr) ? cptr : NULL,
 *				   ":%s HELP %s", parv[0], message);
 *		sendto_helpops("from %s (HelpOp): %s", parv[0], message);
 *	}
 */

        if (IsServer(cptr) /*|| IsHelpOp(sptr)*/ ) {
                sendto_serv_butone(IsServer(cptr) ? cptr : NULL,
                                   ":%s HELP %s", parv[0], message);
                sendto_helpops("from %s%s: %s", parv[0], IsHelpOp(sptr) ? " (HelpOp)" : "", message);
        } else if (message && *message == '!') {
               if (BadPtr(message+1))
                   message = "!index";
               parse_help(sptr, parv[0], message);
               return 0;
        } else if ((!BadPtr(message) && *message == '?') || IsHelpOp(sptr)) {
                int adj = (message && *message == '?' ? 1 : 0);

                if (!IsHelpOp(sptr) && helpop_ignored(sptr))
                {
                    sendto_one(sptr, ":%s NOTICE %s :You can't do that.", me.name, parv[0]);
                    return(0);
                }
                else if (BadPtr(message+adj))
                {
                    sendto_one(sptr, ":%s NOTICE %s :Nothing to send.", me.name, parv[0]);
                    return(0);
                }
                else if (!IsHelpOp(sptr))
                    sendto_one(sptr, ":%s NOTICE %s :Your message has been forwarded to the HelpOps.", me.name, parv[0]);
                sendto_helpops("from %s%s: %s", parv[0], IsHelpOp(sptr) ? " (HelpOp)" : MyClient(sptr) ? " (Local)" : "", message+adj);
                sendto_serv_butone(cptr, ":%s HELP %s", parv[0], message+adj);
        } else if (MyClient(sptr) && parse_help(sptr, parv[0], message) > 0) {
        /*} else if (MyClient(sptr) && index(message, '?') && parse_help(sptr, parv[0], (char *)interpret_question(message)) > 0) {*/
        } else if (!nohelp_message(sptr)) {
        } else if (MyConnect(sptr)) {
                sendto_serv_butone(IsServer(cptr) ? cptr : NULL,
                                   ":%s HELP %s", parv[0], message);
                sendto_helpops("from %s (Local): %s", parv[0], message);
        } else
        {
                if (!IsHelpOp(sptr))
                    sendto_helpops("from %s: %s", parv[0], message);
                else
                    sendto_helpops("from %s (HelpOp): %s", parv[0], message);
                sendto_serv_butone(cptr, ":%s HELP %s", parv[0], message);
        }

	return 0;
}

/*
 * parv[0] = sender
 * parv[1] = host/server mask.
 * parv[2] = server to query
 */
int	 m_lusers(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	int	s_count = 0, c_count = 0, u_count = 0, i_count = 0;
	int	o_count = 0, m_client = 0, m_client_local = 0, m_server = 0;
        char mydom_mask[HOSTLEN + 1];
	aClient *acptr;

	if (check_registered_user(sptr))
		return 0;

	if (parc > 2)
		if(hunt_server(cptr, sptr, ":%s LUSERS %s :%s", 2, parc, parv)
				!= HUNTED_ISME)
			return 0;

        mydom_mask[0] = '*';
        strncpy(&mydom_mask[1], DOMAINNAME, HOSTLEN - 1);

	(void)collapse(parv[1]);
	for (acptr = client; acptr; acptr = acptr->next)
	    {
		if (parc>1)
			if (!IsServer(acptr) && acptr->user)
			    {
				if (match(parv[1], acptr->user->server))
					continue;
			    }
			else
	      			if (match(parv[1], acptr->name))
					continue;

		switch (acptr->status)
		{
		case STAT_SERVER:
			if (MyConnect(acptr))
				m_server++;
		case STAT_ME:
			s_count++;
			break;
		case STAT_CLIENT:
			if (IsOper(acptr))
	        		o_count++;
			if (MyConnect(acptr)) {
		  		m_client++;
				if (match(mydom_mask, acptr->sockhost) == 0)
				  m_client_local++;
			      }
			if (!IsInvisible(acptr))
				c_count++;
			else
				i_count++;
			break;
		default:
			u_count++;
			break;
	 	}
	     }
	sendto_one(sptr, rpl_str(RPL_LUSERCLIENT), me.name, parv[0],
		   c_count, i_count, s_count);
	if (o_count)
		sendto_one(sptr, rpl_str(RPL_LUSEROP),
			   me.name, parv[0], o_count);
	if (u_count > 0)
		sendto_one(sptr, rpl_str(RPL_LUSERUNKNOWN),
			   me.name, parv[0], u_count);
	if ((c_count = count_channels(sptr))>0)
		sendto_one(sptr, rpl_str(RPL_LUSERCHANNELS),
			   me.name, parv[0], count_channels(sptr));
	sendto_one(sptr, rpl_str(RPL_LUSERME),
		   me.name, parv[0], m_client, m_server);
        sendto_one(sptr,
	       ":%s NOTICE %s :Highest connection count: %d (%d clients)",
               me.name, parv[0], max_connection_count, max_client_count);
        if (m_client > max_client_count)
          max_client_count = m_client;
        if ((m_client + m_server) > max_connection_count) {
          max_connection_count = m_client + m_server;
          if (max_connection_count % 10 == 0)  /* only send on even tens */
            sendto_ops("Maximum connections: %d (%d clients)",
                     max_connection_count, max_client_count);
        }
        current_load_data.local_count = m_client_local;
        current_load_data.client_count = m_client;
        current_load_data.conn_count = m_client + m_server;
	return 0;
    }

  
/***********************************************************************
 * m_connect() - Added by Jto 11 Feb 1989
 ***********************************************************************/

/*
** m_connect
**	parv[0] = sender prefix
**	parv[1] = servername
**	parv[2] = port number
**	parv[3] = remote server
*/
int	m_connect(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	int	port, tmpport, retval;
	aConfItem *aconf, *cconf;
	aClient *acptr;

         if (check_registered(sptr))
                 return 0;
  
	if (!IsPrivileged(sptr))
	    {
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return -1;
	    }

	if (MyClient(sptr) && !OPCanGRoute(sptr) && parc > 3)
	    {				/* Only allow LocOps to make */
					/* local CONNECTS --SRB      */
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	    }
	if (MyClient(sptr) && !OPCanLRoute(sptr) && parc <= 3)
	    {
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;   
	    }
	if (hunt_server(cptr,sptr,":%s CONNECT %s %s :%s",
		       3,parc,parv) != HUNTED_ISME)
		return 0;

	if (parc < 2 || *parv[1] == '\0')
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "CONNECT");
		return -1;
	    }

	if ((acptr = find_server(parv[1], NULL)))
	    {
		sendto_one(sptr, ":%s NOTICE %s :Connect: Server %s %s %s.",
			   me.name, parv[0], parv[1], "already exists from",
			   acptr->from->name);
		return 0;
	    }

	for (aconf = conf; aconf; aconf = aconf->next)
		if (aconf->status == CONF_CONNECT_SERVER &&
		    match(parv[1], aconf->name) == 0)
		  break;
	/* Checked first servernames, then try hostnames. */
	if (!aconf)
        	for (aconf = conf; aconf; aconf = aconf->next)
                	if (aconf->status == CONF_CONNECT_SERVER &&
                            (match(parv[1], aconf->host) == 0 ||
                             match(parv[1], index(aconf->host, '@')+1) == 0))
                  		break;

	if (!aconf)
	    {
	      sendto_one(sptr,
			 "NOTICE %s :Connect: Host %s not listed in irc.conf",
			 parv[0], parv[1]);
	      return 0;
	    }
	/*
	** Get port number from user, if given. If not specified,
	** use the default form configuration structure. If missing
	** from there, then use the precompiled default.
	*/
	tmpport = port = aconf->port;
	if (parc > 2 && !BadPtr(parv[2]))
	    {
		if ((port = atoi(parv[2])) <= 0)
		    {
			sendto_one(sptr,
				   "NOTICE %s :Connect: Illegal port number",
				   parv[0]);
			return 0;
		    }
	    }
	else if (port <= 0 && (port = PORTNUM) <= 0)
	    {
		sendto_one(sptr, ":%s NOTICE %s :Connect: missing port number",
			   me.name, parv[0]);
		return 0;
	    }

        /*
	** Evaluate connection rules...  If no rules found, allow the
        ** connect.  Otherwise stop with the first true rule (ie: rules
        ** are ored together.  Oper connects are effected only by D
        ** lines (CRULEALL) not d lines (CRULEAUTO).
        */
	for (cconf = conf; cconf; cconf = cconf->next)
	  if ((cconf->status == CONF_CRULEALL) &&
	      (match(cconf->host, aconf->name) == 0))
	    if (crule_eval (cconf->passwd))
	      {
		sendto_one(sptr,
			   "NOTICE %s :Connect: Disallowed by rule: %s",
			   parv[0], cconf->name);
		return 0;
	      }

	/*
	** Notify all operators about remote connect requests
	*/
	if (!IsAnOper(cptr))
	    {
		sendto_serv_butone(&me,
				  ":%s GNOTICE :Remote CONNECT %s %s from %s",
				   me.name, parv[1], parv[2] ? parv[2] : "",
				   get_client_name(sptr,FALSE));
#if defined(USE_SYSLOG) && defined(SYSLOG_CONNECT)
		syslog(LOG_DEBUG, "CONNECT From %s : %s %d", parv[0], parv[1], parv[2] ? parv[2] : "");
#endif
	    }
	aconf->port = port;
	switch (retval = connect_server(aconf, sptr, NULL))
	{
	case 0:
		sendto_one(sptr,
			   ":%s NOTICE %s :*** Connecting to %s[%s].",
			   me.name, parv[0], aconf->host, aconf->name);
		break;
	case -1:
		sendto_one(sptr, ":%s NOTICE %s :*** Couldn't connect to %s.",
			   me.name, parv[0], aconf->host);
		break;
	case -2:
		sendto_one(sptr, ":%s NOTICE %s :*** Host %s is unknown.",
			   me.name, parv[0], aconf->host);
		break;
	default:
		sendto_one(sptr,
			   ":%s NOTICE %s :*** Connection to %s failed: %s",
			   me.name, parv[0], aconf->host, strerror(retval));
	}
	aconf->port = tmpport;
	return 0;
    }

/*
** m_wallops (write to *all* opers currently online)
**	parv[0] = sender prefix
**	parv[1] = message text
*/
int	m_wallops(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	char	*message, *pv[4];

         if (check_registered(sptr))
                 return 0;
  
	message = parc > 1 ? parv[1] : NULL;

	if (BadPtr(message))
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "WALLOPS");
		return 0;
	    }
	if (MyClient(sptr) && !OPCanWallOps(sptr))
	    {
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	    }
        sendto_ops_butone(IsServer(cptr) ? cptr : NULL, sptr,
                        ":%s WALLOPS :%s", parv[0], message); 
	return 0;
}

/* m_gnotice  (Russell) sort of like wallop, but only to +g clients on 
** this server.
**	parv[0] = sender prefix
**	parv[1] = message text
*/
int	m_gnotice(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	char *message, *pv[4];

         if (check_registered(sptr))
                 return 0;
  
        message = parc > 1 ? parv[1] : NULL;

        if (BadPtr(message))
            {
                sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
                           me.name, parv[0], "GNOTICE");
                return 0;
            }
        if (!IsServer(sptr) && MyConnect(sptr))
	    {
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	    }
	sendto_serv_butone(IsServer(cptr) ? cptr : NULL, ":%s GNOTICE :%s",
		 parv[0], message);
	sendto_failops_whoare_opers("from %s: %s", parv[0], message);
	return 0;
}

/*
** m_globops (write to opers who are +g currently online)
**      parv[0] = sender prefix
**      parv[1] = message text
*/
int     m_globops(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
        char    *message, *pv[4];

         if (check_registered(sptr))
                 return 0;
 
        message = parc > 1 ? parv[1] : NULL;

        if (BadPtr(message))
            {
                sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
                           me.name, parv[0], "GLOBOPS");
                return 0;
            }
	if (MyClient(sptr) && !OPCanGlobOps(sptr))
	    {
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	    }
        sendto_serv_butone(IsServer(cptr) ? cptr : NULL,
                        ":%s GLOBOPS :%s", parv[0], message);
        sendto_failops_whoare_opers("from %s: %s", parv[0], message);
        return 0;
}

/*
** m_locops (write to opers who are +g currently online *this* server)
**      parv[0] = sender prefix
**      parv[1] = message text
*/
int     m_locops(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
        char    *message, *pv[4];

        if (check_registered_user(cptr))
                return 0;
 
         message = parc > 1 ? parv[1] : NULL;

        if (BadPtr(message))
            {
                sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
                           me.name, parv[0], "LOCOPS");
                return 0;
            }
        if (MyClient(sptr) && !OPCanLocOps(sptr))
	    {
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	    }
        sendto_locfailops("from %s: %s", parv[0], message);
        return 0;
}

/*
** m_trojan (write to opers who are +t currently online)
**      parv[0] = sender prefix
**      parv[1] = message text
*/
int     m_trojan(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
        char    *message, *pv[4];

         if (check_registered(sptr))
                 return 0;
 
        message = parc > 1 ? parv[1] : NULL;

        if (BadPtr(message))
            {
                sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
                           me.name, parv[0], "TROJAN");
                return 0;
            }
	if (MyClient(sptr) /* && !OPCanGlobOps(sptr) */)
	    {
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	    }
        sendto_serv_butone(IsServer(cptr) ? cptr : NULL,
                        ":%s TROJAN :%s", parv[0], message);
        sendto_umode(U_OPER|U_TROJAN, "*** Trojan -- from %s: %s", parv[0], message);
        return 0;
}


/* m_goper  (Russell) sort of like wallop, but only to ALL +o clients on
** every server.
**      parv[0] = sender prefix
**      parv[1] = message text
*/
int     m_goper(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
        char *message, *pv[4];

         if (check_registered(sptr))
                 return 0;
 
       message = parc > 1 ? parv[1] : NULL;

        if (BadPtr(message))
            {
                sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
                           me.name, parv[0], "GOPER");
                return 0;
            }
/*      if (!IsServer(sptr) && MyConnect(sptr) && !IsAnOper(sptr))*/
	if (!IsServer(sptr) || !IsULine(cptr,sptr))
	    {
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	    }
	sendto_serv_butone(IsServer(cptr) ? cptr : NULL, ":%s GOPER :%s", 
		parv[0], message);
        sendto_opers("from %s: %s", parv[0], message);
	return 0;
}
/*
** m_time
**	parv[0] = sender prefix
**	parv[1] = servername
*/
int	m_time(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	if (check_registered_user(sptr))
		return 0;
	if (hunt_server(cptr,sptr,":%s TIME :%s",1,parc,parv) == HUNTED_ISME)
		sendto_one(sptr, rpl_str(RPL_TIME), me.name,
			   parv[0], me.name, date((long)0));
	return 0;
}


/*
** m_admin
**	parv[0] = sender prefix
**	parv[1] = servername
*/
int	m_admin(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aConfItem *aconf;

	/* Users may want to get the address in case k-lined, etc. -- Barubary
	if (check_registered(sptr))
		return 0; */

	/* Only allow remote ADMINs if registered -- Barubary */
	if (IsPerson(sptr) || IsServer(cptr))
	if (hunt_server(cptr,sptr,":%s ADMIN :%s",1,parc,parv) != HUNTED_ISME)
		return 0;
	if ((aconf = find_admin()))
	    {
		sendto_one(sptr, rpl_str(RPL_ADMINME),
			   me.name, parv[0], me.name);
		sendto_one(sptr, rpl_str(RPL_ADMINLOC1),
			   me.name, parv[0], aconf->host);
		sendto_one(sptr, rpl_str(RPL_ADMINLOC2),
			   me.name, parv[0], aconf->passwd);
		sendto_one(sptr, rpl_str(RPL_ADMINEMAIL),
			   me.name, parv[0], aconf->name);
	    }
	else
		sendto_one(sptr, err_str(ERR_NOADMININFO),
			   me.name, parv[0], me.name);
	return 0;
}

/*
** m_rehash
**
*/
int	m_rehash(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	if (!MyClient(sptr) || !OPCanRehash(sptr))
	    {
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	    }
	sendto_one(sptr, rpl_str(RPL_REHASHING), me.name, parv[0], configfile);
	sendto_ops("%s is rehashing Server config file", parv[0]);
#ifdef USE_SYSLOG
	syslog(LOG_INFO, "REHASH From %s\n", get_client_name(sptr, FALSE));
#endif
	return rehash(cptr, sptr, (parc > 1) ? ((*parv[1] == 'q')?2:0) : 0);
}

/*
** m_restart
**
** parv[1] - reason for restart (optional)
**
*/
int	m_restart(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
         if (check_registered(sptr))
                 return 0;
 
	if (!MyClient(sptr) || !OPCanRestart(sptr))
	    {
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	    }
#ifdef USE_SYSLOG
	syslog(LOG_WARNING, "Server RESTART by %s - %s\n",
		get_client_name(sptr,FALSE), (parc > 1 ? parv[1] :
							 "No reason"));
#endif

	server_reboot((parc > 1 ? parv[1] : ""));
	return 0;
}

/*
** m_trace
**	parv[0] = sender prefix
**	parv[1] = servername
*/
int	m_trace(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	int	i;
	aClient	*acptr;
	aClass	*cltmp;
	char	*tname;
	int	doall, link_s[MAXCONNECTIONS], link_u[MAXCONNECTIONS];
	int	cnt = 0, wilds, dow;
	time_t  now;

	if (check_registered(sptr))
		return 0;

	if (parc > 2)
		if (hunt_server(cptr, sptr, ":%s TRACE %s :%s",
				2, parc, parv))
			return 0;

	if (parc > 1)
		tname = parv[1];
	else
		tname = me.name;

	switch (hunt_server(cptr, sptr, ":%s TRACE :%s", 1, parc, parv))
	{
	case HUNTED_PASS: /* note: gets here only if parv[1] exists */
	    {
		aClient	*ac2ptr;

		ac2ptr = next_client(client, tname);
		sendto_one(sptr, rpl_str(RPL_TRACELINK), me.name, parv[0],
			   version, debugmode, tname, ac2ptr->from->name);
		return 0;
	    }
	case HUNTED_ISME:
		break;
	default:
		return 0;
	}

	doall = (parv[1] && (parc > 1)) ? !match(tname, me.name): TRUE;
	wilds = !parv[1] || index(tname, '*') || index(tname, '?');
	dow = wilds || doall;

#ifndef _WIN32
	for (i = 0; i < MAXCONNECTIONS; i++)
		link_s[i] = 0, link_u[i] = 0;
#else
	bzero(link_s, sizeof(link_s));
	bzero(link_u, sizeof(link_u));
#endif

	if (doall)
		for (acptr = client; acptr; acptr = acptr->next)
			if (IsPerson(acptr))
				link_u[acptr->from->fd]++;
			else if (IsServer(acptr))
				link_s[acptr->from->fd]++;

	/* report all direct connections */
	
	now = NOW;
	for (i = 0; i <= highest_fd; i++)
	    {
		char	*name;
		int	class;

		if (!(acptr = local[i])) /* Local Connection? */
			continue;
/* More bits of code to allow opers to see all users on remote traces
 *		if (IsInvisible(acptr) && dow &&
 *		if (dow &&
 *		    !(MyConnect(sptr) && IsOper(sptr)) && */
 		if (!IsOper(sptr) &&
		    !IsAnOper(acptr) && (acptr != sptr))
			continue;
		if (!doall && wilds && match(tname, acptr->name))
			continue;
		if (!dow && mycmp(tname, acptr->name))
			continue;
		name = get_client_name(acptr,FALSE);
		class = get_client_class(acptr);

		switch(acptr->status)
		{
		case STAT_CONNECTING:
			sendto_one(sptr, rpl_str(RPL_TRACECONNECTING), me.name,
				   parv[0], class, name);
			cnt++;
			break;
		case STAT_HANDSHAKE:
			sendto_one(sptr, rpl_str(RPL_TRACEHANDSHAKE), me.name,
				   parv[0], class, name);
			cnt++;
			break;
		case STAT_ME:
			break;
		case STAT_UNKNOWN:
			sendto_one(sptr, rpl_str(RPL_TRACEUNKNOWN),
				   me.name, parv[0], class, name);
			cnt++;
			break;
		case STAT_CLIENT:
			/* Only opers see users if there is a wildcard
			 * but anyone can see all the opers.
			 */
/*			if (IsOper(sptr) &&
 * Allow opers to see invisible users on a remote trace or wildcard 
 * search  ... sure as hell  helps to find clonebots.  --Russell
 *			    (MyClient(sptr) || !(dow && IsInvisible(acptr)))
 *                           || !dow || IsAnOper(acptr)) */
                       if (IsOper(sptr) ||
                           (IsAnOper(acptr) && !IsInvisible(acptr)))
			    {
				if (IsAnOper(acptr))
					sendto_one(sptr,
						   rpl_str(RPL_TRACEOPERATOR),
						   me.name,
						   parv[0], class, name,
						   now - acptr->lasttime);
				else
					sendto_one(sptr,rpl_str(RPL_TRACEUSER),
						   me.name, parv[0],
						   class, name,
						   now - acptr->lasttime);
				cnt++;
			    }
			break;
		case STAT_SERVER:
			if (acptr->serv->user)
				sendto_one(sptr, rpl_str(RPL_TRACESERVER),
					   me.name, parv[0], class, link_s[i],
					   link_u[i], name, acptr->serv->by,
					   acptr->serv->user->username,
					   acptr->serv->user->host,
   					   now - acptr->lasttime);
			else
				sendto_one(sptr, rpl_str(RPL_TRACESERVER),
					   me.name, parv[0], class, link_s[i],
					   link_u[i], name, *(acptr->serv->by) ?
					   acptr->serv->by : "*", "*", me.name,
					   now - acptr->lasttime);
			cnt++;
			break;
		case STAT_SERVICE:
			sendto_one(sptr, rpl_str(RPL_TRACESERVICE),
				   me.name, parv[0], class, name);
			cnt++;
			break;
		case STAT_LOG:
			sendto_one(sptr, rpl_str(RPL_TRACELOG), me.name,
				   parv[0], LOGFILE, acptr->port);
			cnt++;
			break;
		default: /* ...we actually shouldn't come here... --msa */
			sendto_one(sptr, rpl_str(RPL_TRACENEWTYPE), me.name,
				   parv[0], name);
			cnt++;
			break;
		}
	    }
	/*
	 * Add these lines to summarize the above which can get rather long
         * and messy when done remotely - Avalon
         */
       	if (!IsAnOper(sptr) || !cnt)
	    {
		if (cnt)
			return 0;
		/* let the user have some idea that its at the end of the
		 * trace
		 */
		sendto_one(sptr, rpl_str(RPL_TRACESERVER),
			   me.name, parv[0], 0, link_s[me.fd],
			   link_u[me.fd], me.name, "*", "*", me.name);
		return 0;
	    }
	for (cltmp = FirstClass(); doall && cltmp; cltmp = NextClass(cltmp))
		if (Links(cltmp) > 0)
			sendto_one(sptr, rpl_str(RPL_TRACECLASS), me.name,
				   parv[0], Class(cltmp), Links(cltmp));
	return 0;
    }

/*
** m_motd
**	parv[0] = sender prefix
**	parv[1] = servername
*/
int	m_motd(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	int	fd, nr;
	char	line[80];
	char	 *tmp;
	struct	stat	sb;
	struct	tm	*tm;

	if (check_registered(sptr))
		return 0;

	if (hunt_server(cptr, sptr, ":%s MOTD :%s", 1,parc,parv)!=HUNTED_ISME)
		return 0;
	/*
	 * stop NFS hangs...most systems should be able to open a file in
	 * 3 seconds. -avalon (curtesy of wumpus)
	 */
	(void)alarm(3);
	fd = open(MOTD, O_RDONLY);
	(void)alarm(0);
	if (fd == -1)
	    {
		sendto_one(sptr, err_str(ERR_NOMOTD), me.name, parv[0]);
		return 0;
	    }
	(void)fstat(fd, &sb);
	sendto_one(sptr, rpl_str(RPL_MOTDSTART), me.name, parv[0], me.name);
	tm = localtime(&sb.st_mtime);
	sendto_one(sptr, ":%s %d %s :- %d/%d/%d %d:%02d", me.name, RPL_MOTD,
		   parv[0], tm->tm_mday, tm->tm_mon + 1, 1900 + tm->tm_year,
		   tm->tm_hour, tm->tm_min);
	(void)dgets(-1, NULL, 0); /* make sure buffer is at empty pos */
	while ((nr=dgets(fd, line, sizeof(line)-1)) > 0)
	    {
	    	line[nr]='\0';
		if ((tmp = (char *)index(line,'\n')))
			*tmp = '\0';
		if ((tmp = (char *)index(line,'\r')))
			*tmp = '\0';
		sendto_one(sptr, rpl_str(RPL_MOTD), me.name, parv[0], line);
	    }
	(void)dgets(-1, NULL, 0); /* make sure buffer is at empty pos */
	sendto_one(sptr, rpl_str(RPL_ENDOFMOTD), me.name, parv[0]);
	(void)closefile(fd);
	return 0;
    }

/*
** m_close - added by Darren Reed Jul 13 1992.
*/
int	m_close(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient	*acptr;
	int	i;
	int	closed = 0;

	if (check_registered(sptr))
	   return 0;

	if (!MyOper(sptr))
	    {
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	    }

	for (i = highest_fd; i; i--)
	    {
		if (!(acptr = local[i]))
			continue;
		if (!IsUnknown(acptr) && !IsConnecting(acptr) &&
		    !IsHandshake(acptr))
			continue;
		sendto_one(sptr, rpl_str(RPL_CLOSING), me.name, parv[0],
			   get_client_name(acptr, TRUE), acptr->status);
		(void)exit_client(acptr, acptr, acptr, "Oper Closing");
		closed++;
	    }
	sendto_one(sptr, rpl_str(RPL_CLOSEEND), me.name, parv[0], closed);
	return 0;
}

int	m_die(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient	*acptr;
	int	i, nomatch = 0;

	if (check_registered(sptr))
                 return 0;
 
	if (!MyClient(sptr) || !IsAnOper(sptr) || !OPCanDie(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}

	if (parc < 2)
	{
		sendto_one(sptr, ":%s NOTICE %s :Syntax is: /die <server> [<reason>]", me.name, parv[0]);
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "DIE");
		return (0);
	}

        nomatch = match(parv[1], me.name) ? 1 : 0;

        if ((!nomatch) && ((!strchr(parv[1], '.') && strchr(me.name, '.')) || (strchr(parv[1], '*') && (strchr(parv[1], '*') < strchr(parv[1], '.')))))
            nomatch = 1;

        if (nomatch)
	{
		sendto_one(sptr, ":%s NOTICE %s :Syntax is: /die <server> [<reason>]", me.name, parv[0]);
		sendto_one(sptr, ":%s NOTICE %s :No no, I am %s!", me.name, parv[0], me.name);
		return(0);
	}

	tolog(LOG_NET, "Terminating by command of %s [%s]",
				   get_client_name(sptr, TRUE),
                                   parc > 2 ? parv[2] : "No reason specified");
	sendto_serv_butone(&me, ":%s LOG :terminating by command of %s [%s]",
				   me.name, get_client_name(sptr, TRUE),
                                   parc > 2 ? parv[2] : "No reason specified");

	for (i = 0; i <= highest_fd; i++)
	    {
		if (!(acptr = local[i]))
			continue;
		if (IsClient(acptr))
			sendto_one(acptr,
				   ":%s NOTICE %s :Server Terminating. %s (%s)",
				   me.name, acptr->name,
				   get_client_name(sptr, TRUE), 
                                   parc > 2 ? parv[2] : "No reason specified");
		else if (IsServer(acptr))
			sendto_one(acptr, ":%s ERROR :Terminated by %s (%s)",
				   me.name, get_client_name(sptr, TRUE),
                                   parc > 2 ? parv[2] : "No reason specified");
	    }
	(void)s_die();
	return 0;
}

#ifdef _WIN32
/*
 * Added to let the local console shutdown the server without just
 * calling exit(-1), in Windows mode.  -Cabal95
 */
int	localdie(void)
{
	aClient	*acptr;
	int	i;

	for (i = 0; i <= highest_fd; i++)
	    {
		if (!(acptr = local[i]))
			continue;
		if (IsClient(acptr))
			sendto_one(acptr,
                ":%s NOTICE %s :Server Terminated by local console",
                me.name, acptr->name);
		else if (IsServer(acptr))
			sendto_one(acptr, ":%s ERROR :Terminated by local console",
                me.name);
	    }
	(void)s_die();
	return 0;
}
#endif

/*
** m_showmask                  Show what a hostname would be, masked
**      parv[0] = sender prefix
**      parv[1] = mask
*/
int     m_showmask(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
  char mask[NICKLEN+USERLEN+HOSTLEN+255];

  if (!IsOper(sptr))
  {
	sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
	return 0;
  }
  if (parc < 2) {
	sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
	   me.name, parv[0], MSG_SHOWMASK);
	return 0;
  }
  strncpyzt(mask, parv[1], sizeof(mask));
  sendto_one(sptr, ":%s NOTICE %s :%25s -> %25s", me.name, parv[0],
             parv[1], genHostMask(mask));
}


/*
 * RPL_NOWON   - Online at the moment (Succesfully added to WATCH-list)
 * RPL_NOWOFF  - Offline at the moement (Succesfully added to WATCH-list)
 * RPL_WATCHOFF   - Succesfully removed from WATCH-list.
 * ERR_TOOMANYWATCH - Take a guess :>  Too many WATCH entries.
 */
static void
show_watch(aClient *cptr, char *name, int rpl1, int rpl2)
{
        aClient *acptr;

        if ((acptr = find_person(name, NULL)))
          sendto_one(cptr, rpl_str(rpl1), me.name, cptr->name,
                                         acptr->name, acptr->user->username,
                                         UGETHOST(cptr, acptr->user), acptr->lasttime);
        else
          sendto_one(cptr, rpl_str(rpl2), me.name, cptr->name,
                                         name, "*", "*", 0);
}

/*
 * m_watch
 */
int   m_watch(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
        aClient  *acptr;
        char  *s, *p, *user;
        char def[2] = "l";

        if (parc < 2)
        {
                /* Default to 'l' - list who's currently online */
                parc = 2;
                parv[1] = def;
        }

        for (p = NULL, s = strtoken(&p, parv[1], ", "); s; s = strtoken(&p, NULL, ", "))
        {
                if ((user = (char *)strchr(s, '!')))
                  *user++ = '\0'; /* Not used */

                /*
                 * Prefix of "+", they want to add a name to their WATCH
                 * list.
                 */
                if (*s == '+')
                {
                        if (*(s+1))
                        {
                                if (sptr->watches >= MAXWATCH)
                                {
                                        sendto_one(sptr, err_str(ERR_TOOMANYWATCH),
                                                                  me.name, cptr->name, s+1);
                                        continue;
                                }
                                add_to_watch_hash_table(s+1, sptr);
                        }
                        show_watch(sptr, s+1, RPL_NOWON, RPL_NOWOFF);
                        continue;
                }

                /*
                 * Prefix of "-", coward wants to remove somebody from their
                 * WATCH list.  So do it. :-)
                 */
                if (*s == '-')
                {
                        del_from_watch_hash_table(s+1, sptr);
                        show_watch(sptr, s+1, RPL_WATCHOFF, RPL_WATCHOFF);
                        continue;
                }

                /*
                 * Fancy "C" or "c", they want to nuke their WATCH list and start
                 * over, so be it.
                 */
                if (*s == 'C' || *s == 'c')
                {
                        hash_del_watch_list(sptr);
                        continue;
                }

                /*
                 * Now comes the fun stuff, "S" or "s" returns a status report of
                 * their WATCH list.  I imagine this could be CPU intensive if its
                 * done alot, perhaps an auto-lag on this?
                 */
                if (*s == 'S' || *s == 's')
                {
                        Link *lp;
                        aWatch *anptr;
                        int  count = 0;

                        /*
                         * Send a list of how many users they have on their WATCH list
                         * and how many WATCH lists they are on.
                         */
                        anptr = hash_get_watch(sptr->name);
                        if (anptr)
                          for (lp = anptr->watch, count = 1; (lp = lp->next); count++);
                        sendto_one(sptr, rpl_str(RPL_WATCHSTAT), me.name, parv[0],
                                                  sptr->watches, count);
                        /*
                         * Send a list of everybody in their WATCH list. Be careful
                         * not to buffer overflow.
                         */
                        if ((lp = sptr->watch) == NULL)
                        {
                                sendto_one(sptr, rpl_str(RPL_ENDOFWATCHLIST), me.name, parv[0],
                                                          *s);
                                continue;
                        }
                        *buf = '\0';
                        strcpy(buf, lp->value.wptr->nick);
                        count = strlen(parv[0])+strlen(me.name)+10+strlen(buf);
                        while ((lp = lp->next))
                        {
                                if (count+strlen(lp->value.wptr->nick)+1 > BUFSIZE - 2)
                                {
                                        sendto_one(sptr, rpl_str(RPL_WATCHLIST), me.name,
                                                                  parv[0], buf);
                                        *buf = '\0';
                                        count = strlen(parv[0])+strlen(me.name)+10;
                                }
                                strcat(buf, " ");
                                strcat(buf, lp->value.wptr->nick);
                                count += (strlen(lp->value.wptr->nick)+1);
                        }
                        sendto_one(sptr, rpl_str(RPL_WATCHLIST), me.name, parv[0], buf);
                        sendto_one(sptr, rpl_str(RPL_ENDOFWATCHLIST), me.name, parv[0], *s);
                        continue;
                }

                /*
                 * Well that was fun, NOT.  Now they want a list of everybody in
                 * their WATCH list AND if they are online or offline? Sheesh,
                 * greedy arn't we?
                 */
                if (*s == 'L' || *s == 'l')
                {
                        Link *lp = sptr->watch;

                        while (lp)
                        {
                                if ((acptr = find_person(lp->value.wptr->nick, NULL)))
                                  sendto_one(sptr, rpl_str(RPL_NOWON), me.name, parv[0],
                                                                 acptr->name, acptr->user->username,
                                                                 UGETHOST(sptr, acptr->user), acptr->lastnick);
                                /*
                                 * But actually, only show them offline if its a capital
                                 * 'L' (full list wanted).
                                 */
                                else if (isupper(*s))
                                  sendto_one(sptr, rpl_str(RPL_NOWOFF), me.name, parv[0],
                                                                 lp->value.wptr->nick, "*", "*",
                                                                 lp->value.wptr->lasttime);
                                lp = lp->next;
                        }

                        sendto_one(sptr, rpl_str(RPL_ENDOFWATCHLIST), me.name, parv[0],  *s);
                        continue;
                }
                /* Hmm.. unknown prefix character.. Ignore it. :-) */
        }

        return 0;
}

/***************************************************************************/
/* Protocol Negotiation ****************************************************/

struct protocol_info
{
	char *name;
	int can;
	int optflag;
	int doflag;
}
protocols[] = {
	{ "ZIP",	1,	CAP_ZIP,	CAP_ZIP_DO },
	{ (char *)0 }
};

int proto_will(aClient *cptr, int parc, char **parv, int proto)
{
	if (cptr->inOpt & protocols[proto].optflag)
		return 0;

	if (proto == -1) {
		sendto_one(cptr, "CAPAB DONT %s", parv[2]);
		return 0;
	}

	if ((cptr->outOpt & protocols[proto].doflag))
	{
		flush_connections(cptr->fd);
		cptr->inOpt |= protocols[proto].optflag;
		return 0;
	}

	if (protocols[proto].can)
	{
		cptr->inOpt |= protocols[proto].doflag;
		sendto_one(cptr, "CAPAB DO %s", parv[2]);
	}
	else
	{
		cptr->inOpt |= protocols[proto].doflag;
		sendto_one(cptr, "CAPAB DONT %s", parv[2]);
	}
}

int proto_wont(aClient *cptr, int parc, char **parv, int proto)
{
}

int proto_do(aClient *cptr, int parc, char **parv, int proto)
{
}

int proto_dont(aClient *cptr, int parc, char **parv, int proto)
{
}

/* send_to_one(cptr, "CAPAB :VER=%x", SORIRC_VERSION); */
int	m_capab(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
    char *opt, *p, *s, *q, *qq;
    int i = 0, j = 0;

    if (parc < 2)
        return 0;

    if (MyConnect(sptr))
    {
	struct {char *name; int(* func)(aClient*,int,char**,int);}
	negTypes[] = {
		{"WILL", proto_will},
		{"WONT", proto_wont},
		{"DO", proto_do},
		{"DONT", proto_dont},
		{ (char *)0 }
	};

	for(i = 0; negTypes[i].name; i++)
	{
		if (mycmp(parv[1], negTypes[i].name) == 0) {
		    if (parc < 3)
			return 0;
		    for(j = 0; protocols[j].name; j++) {
			if (mycmp(protocols[j].name, parv[2]) == 0) {
			    return (* negTypes[i].func)(sptr, parc, parv, j);
			}
		    }
		    return (* negTypes[i].func)(sptr, parc, parv, -1);
		}
	}
    }

    for(s = parv[1]; (opt = strtoken(&p, s, " ")); s = NULL) {
        if ((q = qq = index(opt, '='))) {
            *qq = '\0';
            q++;

            if (!mycmp(opt, "VER")) {
                int ver_num = strtol(q, (char **)0, 16);

                if (ver_num >= 0x00010400) {
                    SET_BIT(sptr->cap, CAP_USERIP);
		}
            }
 
            *qq = '=';
        }
    }
    sendto_serv_butone(cptr, ":%s CAPAB :%s", parv[0], parv[1]);
    return 0;
}
