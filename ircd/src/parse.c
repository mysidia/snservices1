/************************************************************************
 *   IRC - Internet Relay Chat, common/parse.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
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
#define MSGTAB
#include "msg.h"
#undef MSGTAB

IRCD_SCCSID("@(#)parse.c	2.33 1/30/94 (C) 1988 University of Oulu, Computing Center and Jarkko Oikarinen");
IRCD_RCSID("$Id$");

/*
 * NOTE: parse() should not be called recursively by other functions!
 */

static	char	*para[MAXPARA + 1];
static	char	sender[HOSTLEN + 1];
static	int	cancel_clients(aClient *, aClient *, char *);
static	void	remove_unknown(aClient *, char *);

/*
**  Find a client (server or user) by name.
**
**  *Note*
**	Semantics of this function has been changed from
**	the old. 'name' is now assumed to be a null terminated
**	string and the search is the for server and user.
*/
aClient *
find_client(char *name, aClient *cptr)
{
	if (name)
		cptr = hash_find_client(name, cptr);

	return cptr;
}

aClient	*
find_nickserv(char *name, aClient *cptr)
{
	if (name)
		cptr = hash_find_nickserver(name, cptr);

	return cptr;
}

#ifdef HASH_MSGTAB
#define MSG_HASH_SIZE  255
#define MHASH(x)       (irc_toupper(*x)%MSG_HASH_SIZE)
struct HMessage *msghash[MSG_HASH_SIZE+255];
struct Message BlankMptrEnt;
struct Message VMptrEnt;

struct Message *
hash_findcommand(char *cmd, aClient *cptr)
{
	unsigned char hval = cmd ? MHASH(cmd) : 0;
	int iOper = (!MyClient(cptr) || IsAnOper(cptr)) ? 1 : 0;

	if (!msghash[hval])
		return NULL;
	else {
		struct HMessage *ptr;
		for (ptr = msghash[hval]; ptr; ptr = ptr->next) {
			if ((ptr->mptr && (mycmp(ptr->mptr->cmd, cmd) == 0))) {
				if ( (ptr->mptr->flags & MF_OPER) && !iOper) {
					sendto_one(cptr, err_str(ERR_NOPRIVILEGES),
						   me.name, ptr->mptr->cmd);
					return &VMptrEnt;
				}
				return ptr->mptr;
			}
		}
		return NULL;
	}
}

void
msgtab_buildhash(void)
{
        int i = 0, trun = 0;
        static int run = 0;
        struct HMessage *ipt;
        unsigned char hval;
        BlankMptrEnt.cmd = NULL;
        BlankMptrEnt.func= NULL;

        if (run) { /* empty the existing hashtable */
		struct HMessage *h_next, *h_ptr;
		trun = 1;

		for (i = 0; i <= MSG_HASH_SIZE; i++) {
			if (msghash[i]) {
				for (h_ptr = msghash[i]; h_ptr; h_ptr = h_next) {
					h_next = h_ptr->next;
					irc_free(h_ptr);
				}
				msghash[i] = NULL;
				continue;
			}
			msghash[i] = NULL;
		}
        } else {
		for (i = 0; i <= MSG_HASH_SIZE; i++)
			msghash[i] = NULL;
		run = 1;
        }

#ifdef BOOT_MSGS
        if (trun < 1)
		printf("Building msgtab[] hashes... ", hval);
#endif

        for ( i = 0 ; msgtab[i].cmd ; i++) {
		hval = (unsigned char)MHASH(msgtab[i].cmd);
		if (!msghash[hval]) {
#ifdef BOOT_MSGS
			if (trun < 1)
				printf("\e[1;32m%c\e[0m ", hval);
#endif
			ipt = msghash[hval] = irc_malloc(sizeof(struct HMessage));
			memset(msghash[hval], 0, sizeof(struct HMessage));
		} else {
			ipt = msghash[hval];
			msghash[hval] = irc_malloc(sizeof(struct HMessage));
			memset(msghash[hval], 0, sizeof(struct HMessage));
			msghash[hval]->next = ipt;
			ipt = msghash[hval];
		}
		ipt->mptr = &msgtab[i];
        }
#ifdef BOOT_MSGS
        if (trun < 1)
		printf("\nmsgtab[] hashtable built; rehash to rebuild.\n", hval);
#endif
}
#endif




/*
**  Find server by name.
**
**	This implementation assumes that server and user names
**	are unique, no user can have a server name and vice versa.
**	One should maintain separate lists for users and servers,
**	if this restriction is removed.
**
**  *Note*
**	Semantics of this function has been changed from
**	the old. 'name' is now assumed to be a null terminated
**	string.
*/
aClient *find_server(char *name, aClient *cptr)
{
	if (name)
		cptr = hash_find_server(name, cptr);
	return cptr;
}

aClient *find_name(char *name, aClient *cptr)
{
	aClient *c2ptr = cptr;


	if (!collapse(name))
		return c2ptr;

	if ((c2ptr = hash_find_server(name, cptr)))
		return (c2ptr);
	if (!index(name, '*'))
		return c2ptr;
	for (c2ptr = client; c2ptr; c2ptr = c2ptr->next)
	    {
		if (!IsServer(c2ptr) && !IsMe(c2ptr))
			continue;
		if (match(name, c2ptr->name) == 0)
			break;
		if (index(c2ptr->name, '*'))
			if (match(c2ptr->name, name) == 0)
					break;
	    }
	return (c2ptr ? c2ptr : cptr);
}

/*
**  Find person by (nick)name.
*/
aClient *find_person(char *name, aClient *cptr)
{
	aClient	*c2ptr = cptr;


	c2ptr = find_client(name, c2ptr);

	if (c2ptr && IsClient(c2ptr) && c2ptr->user)
		return c2ptr;
	else
		return cptr;
}

/*
 * parse a buffer.
 *
 * NOTE: parse() should not be called recusively by any other fucntions!
 */
int	parse(aClient *cptr, char *buffer, char *bufend, struct Message *mptr)
{
	aClient *from = cptr;
	char *ch, *s;
	int	len, i, numeric = 0, paramcount, noprefix = 0;

	Debug((DEBUG_DEBUG,"Parsing: %s", buffer));
	if (IsDead(cptr))
		return 0;

	s = sender;
	*s = '\0';
	for (ch = buffer; *ch == ' '; ch++)
		;
	para[0] = from->name;
	if (*ch == ':') {
		/*
		** Copy the prefix to 'sender' assuming it terminates
		** with SPACE (or NULL, which is an error, though).
		*/
		for (++ch, i = 0; *ch && *ch != ' '; ++ch )
			if (s < (sender + sizeof(sender)-1))
				*s++ = *ch; /* leave room for NULL */
		*s = '\0';
		/*
		** Actually, only messages coming from servers can have
		** the prefix--prefix silently ignored, if coming from
		** a user client...
		**
		** ...sigh, the current release "v2.2PL1" generates also
		** null prefixes, at least to NOTIFY messages (e.g. it
		** puts "sptr->nickname" as prefix from server structures
		** where it's null--the following will handle this case
		** as "no prefix" at all --msa  (": NOTICE nick ...")
		*/
		if (*sender && IsServer(cptr)) {
 			from = find_client(sender, (aClient *) NULL);
			if (!from || match(from->name, sender))
				from = find_server(sender, (aClient *)NULL);
			else if (!from && index(sender, '@'))
				from = find_nickserv(sender, (aClient *)NULL);

			para[0] = sender;

			/* Hmm! If the client corresponding to the
			 * prefix is not found--what is the correct
			 * action??? Now, I will ignore the message
			 * (old IRC just let it through as if the
			 * prefix just wasn't there...) --msa
			 */
			if (!from) {
				Debug((DEBUG_ERROR,
				       "Unknown prefix (%s)(%s) from (%s)",
				       sender, buffer, cptr->name));
				ircstp->is_unpf++;
				remove_unknown(cptr, sender);
				return -1;
			}
			if (from->from != cptr) {
				ircstp->is_wrdi++;
				Debug((DEBUG_ERROR,
				       "Message (%s) coming from (%s)",
				       buffer, cptr->name));
				return cancel_clients(cptr, from, ch);
			}
		}
		while (*ch == ' ')
			ch++;
	} else
		noprefix = 1;
	if (*ch == '\0') {
		ircstp->is_empt++;
		Debug((DEBUG_NOTICE, "Empty message from host %s:%s",
		       cptr->name, from->name));
		return(-1);
	}
	/*
	** Extract the command code from the packet.  Point s to the end
	** of the command code and calculate the length using pointer
	** arithmetic.  Note: only need length for numerics and *all*
	** numerics must have paramters and thus a space after the command
	** code. -avalon
	*/
	s = (char *)index(ch, ' '); /* s -> End of the command code */
	len = (s) ? (s - ch) : 0;
	if (len == 3 &&
	    isdigit(*ch) && isdigit(*(ch + 1)) && isdigit(*(ch + 2))) {
		mptr = NULL;
		numeric = (*ch - '0') * 100 + (*(ch + 1) - '0') * 10
			+ (*(ch + 2) - '0');
		paramcount = MAXPARA;
		ircstp->is_num++;
	} else {
		if (s)
			*s++ = '\0';
#define prevent_dumping(mptr, cptr)                                              \
               i = bufend - ((s) ? s : ch);                                      \
               if ((mptr->flags & 0x1) && !IsServer(cptr))  \
                       cptr->since += (2 + i / 120);                             
#if !defined(HASH_MSGTAB)
		for (; mptr->cmd; mptr++) 
			if (mycmp(mptr->cmd, ch) == 0)
				break;
#else
		mptr = hash_findcommand(ch, from);
		if (!mptr)
			mptr = &BlankMptrEnt;
		if (mptr == &VMptrEnt) {
			VMptrEnt.flags = 1;
			prevent_dumping(mptr, cptr);
			return 0;
		}
#endif

                if (!mptr->cmd)
                { 
		   if (MyConnect(from)) {
                      if (!IsAnOper(from) && from->hurt > 5)
                      {
                          if (from->hurt < (MAXTIME-20)) from->hurt += 15;
                          /*if (from->since < (MAXTIME-20)) from->since += 2; */
                          /*cptr->since += (2 + (bufend - ((s) ? s : ch)) / 120);*/
                          prevent_dumping(mptr, cptr);
                      }
                      else
                      {
                          /*cptr->since += (2 + (bufend - ((s) ? s : ch)) / 120);*/
                          prevent_dumping(mptr, cptr);
                      }
		   }
            
                 }
		else if (mptr->cmd)	
	        {

                   if ((mptr->flags) && !(mptr->flags == 1))
                   {
                     int stopcmd = 0;
                     if ((mptr->flags & MF_OPER) && !IsPrivileged(from) && MyClient(from))
                     {
                       sendto_one(from, err_str(ERR_NOPRIVILEGES), me.name, mptr->cmd);
                       stopcmd=1;
                     }
                     else
                     if ((mptr->flags & MF_ULINE) && MyConnect(from) &&
                        (!IsULine(cptr, from)))
                     {   stopcmd=1;
                         sendto_one(from, ":%s NOTICE %s :(%s) This command can only be used by a U-lined server.",
                                    me.name, from->name, mptr->cmd);
                         if (IsServer(cptr))
                         {
                            sendto_ops("%s is not U-lined and attempted a services command (%s).", from->name, mptr->cmd);
                            sendto_serv_butone(cptr, ":%s GLOBOPS :%s is not U-lined and "
                                                     "attempted a services command. (%s)",
                                                      me.name, from->name, mptr->cmd);
                         }
                     }
                     if (stopcmd) { prevent_dumping(mptr, cptr); return 0; }
                   }


/*                             replaced with a flag in the message table
				if (mptr->func != m_pong && mptr->func != m_quit &&
				    mptr->func != m_part && mptr->func != m_ison &&
				    mptr->func != m_heal && mptr->func != m_userhost)*/
			if (IsPerson(from) && MyConnect(from))
			{
#if defined(NOSPOOF) && defined(REQ_VERSION_RESPONSE)
			        if (MyClient(from) && !IsUserVersionKnown(from)
					&& IsHurt(from)
					&& !IsAnOper(from)
					&& mptr->func != m_notice && mptr->func != m_mode 
                                        && mptr->func != m_mode  && mptr->func != m_ison
					&& mptr->func != m_join
					&& mptr->func != m_private
					&& mptr->func != m_who
                                        && (mptr->while_hurt < 1 ||
					mptr->while_hurt > 2)) {

					{
					   sendto_one(from, ":%s NOTICE %s :Sorry, but your IRC software "
                                                      "program has not yet reported its version. "
                                                      "Your request (%s) was not "
                                                      "processed.",
                                                       me.name, from->name, 
                                                       mptr->cmd);
					   from->since += 3;

					   return 0;
						
					}
        			}
#endif

				if (mptr->while_hurt < 1 
				    || ((mptr->while_hurt > 1 && (from->hurt < mptr->while_hurt)))) {
					if (!IsHurt(from))
					   from->hurt = 0;
					if (IsHurt(from) && from->hurt
					    && from->hurt != 4)
					{
						if ((NOW < from->hurt || (from->hurt>0 && from->hurt<5)) &&
                                                    (from->hurt != 3 || !IsOper(from))) {
							if (from->hurt>5 && from->hurt > NOW) {
								if (from->hurt < MAXTIME-20)
									from->hurt += 4;
							}
							/* hurt replies */
							if (!(mptr->func == m_notice))
								if (((from->since - NOW ) <= 60)) {
									switch(from->hurt) {
									default:
									case 1:
										sendto_one(from, err_str(ERR_YOURHURT), me.name, from->name);
										break;
									case 2:
										sendto_one(from, err_str(ERR_YOURHURT), me.name, "AUTH");
										break;
									case 3: 
										sendto_one(from, ":%s %d %s :You must identify to a registered nick before you can use this command from a partially-banned site.", me.name, ERR_YOURHURT, from->name);
										break;
									case 4: 
										break;
									}
								}

							return 0;
						} else
							remove_hurt(from);
					}
				}
			} 
		}


		if (!mptr->cmd) {
			/*
			** Note: Give error message *only* to recognized
			** persons. It's a nightmare situation to have
			** two programs sending "Unknown command"'s or
			** equivalent to each other at full blast....
			** If it has got to person state, it at least
			** seems to be well behaving. Perhaps this message
			** should never be generated, though...  --msa
			** Hm, when is the buffer empty -- if a command
			** code has been found ?? -Armin
			*/
			if (buffer[0] != '\0') {
				if (IsPerson(from))
					sendto_one(from,
						   ":%s %d %s %s :Unknown command",
						   me.name, ERR_UNKNOWNCOMMAND,
						   from->name, ch);
				Debug((DEBUG_ERROR,"Unknown (%s) from %s",
				       ch, get_client_name(cptr, TRUE)));
			}
			ircstp->is_unco++;
			return(-1);
		}
		paramcount = mptr->parameters;
		i = bufend - ((s) ? s : ch);
		mptr->bytes += i;
		if ((mptr->flags & 1) && !IsServer(cptr))
			cptr->since += (2 + i / 120);
		/* Allow only 1 msg per 2 seconds
		 * (on average) to prevent dumping.
		 * to keep the response rate up,
		 * bursts of up to 5 msgs are allowed
		 * -SRB
		 */
	}
	/*
	** Must the following loop really be so devious? On
	** surface it splits the message to parameters from
	** blank spaces. But, if paramcount has been reached,
	** the rest of the message goes into this last parameter
	** (about same effect as ":" has...) --msa
	*/

	/* Note initially true: s==NULL || *(s-1) == '\0' !! */

	i = 0;
	if (s) {
		if (paramcount > MAXPARA)
			paramcount = MAXPARA;
		for (;;) {
			/*
			** Never "FRANCE " again!! ;-) Clean
			** out *all* blanks.. --msa
			*/
			while (*s == ' ')
				*s++ = '\0';

			if (*s == '\0')
				break;
			if (*s == ':') {
				/*
				** The rest is single parameter--can
				** include blanks also.
				*/
				para[++i] = s + 1;
				break;
			}
			para[++i] = s;
			if (i >= paramcount)
				break;
			for (; *s != ' ' && *s; s++)
				;
		}
	}
	para[++i] = NULL;
	if (mptr == NULL)
		return (do_numeric(numeric, cptr, from, i, para));
	mptr->count++;
	if (IsRegisteredUser(cptr) &&
#ifdef	IDLE_FROM_MSG
	    mptr->func == m_private
#else
	    mptr->func != m_ping && mptr->func != m_pong
#endif
	    )
		from->user->last = NOW;

	/* Lame protocol 4 stuff... this if can be removed when all are 2.9 */
	if (noprefix && IsServer(cptr) && i >= 2 && mptr->func == m_squit &&
	    (!(from = find_server(para[1], (aClient *)NULL)) ||
	     from->from != cptr)) {
		Debug((DEBUG_DEBUG,"Ignoring protocol 4 \"%s %s %s ...\"",
		       para[0], para[1], para[2]));
		return 0;
        }

	return (*mptr->func)(cptr, from, i, para);
}

/*
 * field breakup for ircd.conf file.
 */
char	*getfield(char *newline)
{
	static	char *line = NULL;
	char	*end, *field;


	if (newline)
		line = newline;
	if (line == NULL)
		return(NULL);

	field = line;
	if (*line == '<')
	{
		if ((line = (char *)index(line,'>')) == NULL)
			line = field;
		else
		{
			field++;
			*line++ = '\0';
		}
	}
	if ((end = (char *)index(line,':')) == NULL)
	    {
		line = NULL;
		if ((end = (char *)index(field,'\n')) == NULL)
			end = field + strlen(field);
	    }
	else
		line = end + 1;
	*end = '\0';
	return(field);
}

static	int	cancel_clients(aClient *cptr, aClient *sptr, char *cmd)
{
        aChannel *chptr;


	/*
	 * kill all possible points that are causing confusion here,
	 * I'm not sure I've got this all right...
	 * - avalon
	 * No you didn't...
	 * - Run
	 */
	/* This little bit of code allowed paswords to nickserv to be 
	 * seen.  A definite no-no.  --Russell
	sendto_ops("Message (%s) for %s[%s!%s@%s] from %s", cmd,
		   sptr->name, sptr->from->name, sptr->from->username,
		   sptr->from->sockhost, get_client_name(cptr, TRUE));*/
	/*
	 * Incorrect prefix for a server from some connection.  If it is a
	 * client trying to be annoying, just QUIT them, if it is a server
	 * then the same deal.
	 */
	if (IsServer(sptr) || IsMe(sptr))
	    {
		/*
		 * First go at tracking down what really causes the
		 * dreaded Fake Direction error.  It should not be possible
		 * ever to happen.  Assume nothing here since this is an
		 * impossibility.
		 *
		 * Check for valid fields, then send out globops with
		 * the msg command recieved, who apperently sent it,
		 * where it came from, and where it was suppose to come
		 * from.  We send the msg command to find out if its some
		 * bug somebody found with an old command, maybe some
		 * weird thing like, /ping serverto.* serverfrom.* and on
		 * the way back, fake direction?  Don't know, maybe this
		 * will tell us.  -Cabal95
                 *
                 * Take #2 on Fake Direction.  Most of them seem to be
                 * numerics.  But sometimes its getting fake direction on
                 * SERVER msgs.. HOW??  Display the full message now to
                 * figure it out... -Cabal95
                 *
                 * Okay I give up.  Can't find it.  Seems like it will
                 * exist untill ircd is completely rewritten. :/ For now
                 * just completely ignore them.  Needs to be modified to
                 * send these messages to a special oper channel. -Cabal95
                 */


		char	*fromname=NULL, *sptrname=NULL, *cptrname=NULL, *s;

		while (*cmd == ' ')
			cmd++;
		if ((s = index(cmd, ' ')))
			*s = '\0';
		if (sptr && sptr->name)
			sptrname = sptr->name;
		if (cptr && cptr->name)
			cptrname = cptr->name;
		if (sptr && sptr->from && sptr->from->name)
			fromname = sptr->from->name;
               
#ifdef DEBUG_CHAN
                if ((chptr = find_channel(DEBUG_CHAN, ((aChannel *)0) )) &&
                     mycmp(cmd,"NOTICE") && mycmp(cmd,"PRIVMSG"))
                    {
                                sendto_channel_butone(&me, NULL, chptr,
                      ":%s %s %s :Fake Direction: Message[%s] from %s via %s instead of %s",
                                       me.name, "NOTICE", chptr->chname,
                                       cmd, (sptr->name!=NULL)?sptr->name:"<unknown>",
                                       (cptr->name!=NULL)?cptr->name:"<unknown>",
                                        (fromname!=NULL)?fromname:"<unknown>");
                    }
#else
		sendto_serv_butone(NULL, ":%s GLOBOPS :"
			"Fake Direction: Message[%s] from %s via %s "
			"instead of %s", me.name, cmd,
			(sptr->name!=NULL)?sptr->name:"<unknown>",
			(cptr->name!=NULL)?cptr->name:"<unknown>",
			(fromname!=NULL)?fromname:"<unknown>");
		sendto_ops(
			"Fake Direction: Message[%s] from %s via %s "
			"instead of %s", cmd,
			(sptr->name!=NULL)?sptr->name:"<unknown>",
			(cptr->name!=NULL)?cptr->name:"<unknown>",
			(fromname!=NULL)?fromname:"<unknown>");

#endif

		/*sendto_ops("Dropping server %s", cptr->name);
		return exit_client(cptr, cptr, &me, "Fake Direction");*/
	    }
	/*
	 * Ok, someone is trying to impose as a client and things are
	 * confused.  If we got the wrong prefix from a server, send out a
	 * kill, else just exit the lame client.
	 */
	if (IsServer(cptr))
	    {
		/*
		** It is NOT necessary to send a KILL here...
		** We come here when a previous 'NICK new'
		** nick collided with an older nick, and was
		** ignored, and other messages still were on
		** the way (like the following USER).
		** We simply ignore it all, a purge will be done
		** automatically by the server 'cptr' as a reaction
		** on our 'NICK older'. --Run
		*/
		return 0; /* On our side, nothing changed */
	    }
	return exit_client(cptr, cptr, &me, "Fake prefix");
}

static	void	remove_unknown(aClient *cptr, char *sender)
{
	if (!IsRegistered(cptr) || IsClient(cptr))
		return;
	/*
	 * Not from a server so don't need to worry about it.
	 */
	if (!IsServer(cptr))
		return;
	/*
	 * Do kill if it came from a server because it means there is a ghost
	 * user on the other server which needs to be removed. -avalon
	 */
	if (!index(sender, '.'))
		sendto_one(cptr, ":%s KILL %s :%s (%s(?) <- %s)",
			   me.name, sender, me.name, sender,
			   get_client_name(cptr, FALSE));
	else
		sendto_one(cptr, ":%s SQUIT %s :(Unknown from %s)",
			   me.name, sender, get_client_name(cptr, FALSE));
}


aClient *find_server_const(const char *name, aClient *cptr)
{
       char cname[256];

       strncpy(cname, name, 256);
       cname[255] = '\0';

       if (name)
               cptr = hash_find_server(cname, cptr);
       return cptr;
}
