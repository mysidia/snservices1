/************************************************************************
 *   IRC - Internet Relay Chat, ircd/list.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Finland
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

/* -- Jto -- 20 Jun 1990
 * extern void free() fixed as suggested by
 * gruner@informatik.tu-muenchen.de
 */

/* -- Jto -- 03 Jun 1990
 * Added chname initialization...
 */

/* -- Jto -- 24 May 1990
 * Moved is_full() to channel.c
 */

/* -- Jto -- 10 May 1990
 * Added #include <sys.h>
 * Changed memset(xx,0,yy) into bzero(xx,yy)
 */

#ifndef lint
static  char sccsid[] = "@(#)list.c	2.24 4/20/94 (C) 1988 University of Oulu, \
Computing Center and Jarkko Oikarinen";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "h.h"
#include "hash.h"
#include "numeric.h"
#ifdef	DBMALLOC
#include "malloc.h"
#endif
void	free_link PROTO((Link *));
Link	*make_link PROTO(());

#ifdef	DEBUGMODE
static	struct	liststats {
	int	inuse;
} cloc, crem, users, servs, links, classs, aconfs;

#endif

void	outofmemory();

int	numclients = 0;

void	initlists()
{
#ifdef	DEBUGMODE
	bzero((char *)&cloc, sizeof(cloc));
	bzero((char *)&crem, sizeof(crem));
	bzero((char *)&users, sizeof(users));
	bzero((char *)&servs, sizeof(servs));
	bzero((char *)&links, sizeof(links));
	bzero((char *)&classs, sizeof(classs));
	bzero((char *)&aconfs, sizeof(aconfs));
#endif
}

void	outofmemory()
{
	Debug((DEBUG_FATAL, "Out of memory: restarting server..."));
	restart("Out of Memory");
}

	
/*
** Create a new aClient structure and set it to initial state.
**
**	from == NULL,	create local client (a client connected
**			to a socket).
**
**	from,	create remote client (behind a socket
**			associated with the client defined by
**			'from'). ('from' is a local client!!).
*/
aClient	*make_client(aClient *from)
{
	aClient *cptr = NULL;
	unsigned size = CLIENT_REMOTE_SIZE;

	/*
	 * Check freelists first to see if we can grab a client without
	 * having to call malloc.
	 */
	if (!from)
		size = CLIENT_LOCAL_SIZE;

	if (!(cptr = (aClient *)MyMalloc(size)))
		outofmemory();
	bzero((char *)cptr, (int)size);

#ifdef	DEBUGMODE
	if (size == CLIENT_LOCAL_SIZE)
		cloc.inuse++;
	else
		crem.inuse++;
#endif

	/* Note:  structure is zero (calloc) */
	cptr->from = from ? from : cptr; /* 'from' of local client is self! */
	cptr->next = NULL; /* For machines with NON-ZERO NULL pointers >;) */
	cptr->prev = NULL;
	cptr->hnext = NULL;
	cptr->user = NULL;
	cptr->serv = NULL;
	cptr->status = STAT_UNKNOWN;
	cptr->fd = -1;
	cptr->socks = (void *)0;
	(void)strcpy(cptr->username, "unknown");
	if (size == CLIENT_LOCAL_SIZE)
	    {
	        cptr->lopt = (LOpts *)0;
		cptr->since = cptr->lasttime =
		  cptr->lastnick = cptr->firsttime = NOW;
		cptr->confs = NULL;
		cptr->sockhost[0] = '\0';
		cptr->buffer[0] = '\0';
	    }
	return (cptr);
}

int	free_socks(struct Socks *zap)
{
	if (zap == NULL)
		return (-1);

	if (zap->fd >= 0)
		closesocket(zap->fd);
	zap->fd = -1;

	MyFree(zap);

	Debug((DEBUG_ERROR, "freeing socks"));
	return (0);
}

void	free_client(aClient *cptr)
{
	if (cptr->from == cptr && cptr->lopt) {
		free_str_list(cptr->lopt->yeslist);
		free_str_list(cptr->lopt->nolist);
		MyFree(cptr->lopt);
	}

	MyFree((char *)cptr);
}

/*
** make_user() adds an User information block to a client
** if it was not previously allocated.
*/
anUser	*make_user(aClient *cptr)
{
	anUser	*user;

	user = cptr->user;
	if (!user)
	    {
		user = (anUser *)MyMalloc(sizeof(anUser));
#ifdef	DEBUGMODE
		users.inuse++;
#endif
		user->mask = NULL;
		user->away = NULL;
		user->refcnt = 1;
		user->joined = 0;
		user->channel = NULL;
		user->invited = NULL;
		user->silence = NULL;
		user->mask = NULL;
		user->sup_version = NULL;
#ifdef  KEEP_HURTBY
		user->hurtby = NULL;
#endif
		cptr->user = user;
	    }
	return user;
}

/* similarly, make_server() creates a nice Server struct
 * for the client
 */
aServer	*make_server(aClient *cptr)
{
	aServer	*serv = cptr->serv;

	if (!serv)
	    {
		serv = (aServer *)MyMalloc(sizeof(aServer));
#ifdef	DEBUGMODE
		servs.inuse++;
#endif
		serv->user = NULL;
		serv->nexts = NULL;
		*serv->by = '\0';
		*serv->up = '\0';
		cptr->serv = serv;
	    }
	return cptr->serv;
}

static struct StringHash version_strings[STRINGHASHSIZE];

struct StringHashElement* make_stringhash_element()
{
	struct StringHashElement *e;

	e = (struct StringHashElement*)
		MyMalloc(sizeof(struct StringHashElement));
	e->str = NULL;
	e->next = NULL;
	e->refct = 0;

	return e;
}

void free_stringhash_element(struct StringHashElement* e)
{
	if ( e->str )
		MyFree(e->str);
	MyFree(e);
}


void dup_sup_version(anUser* user, const char* buf)
{
	long k;
	struct StringHashElement *e, *tmp;
	
	if (BadPtr(buf)) {
		user->sup_version = NULL;
		return;
	}
	
	k = hash_nn_name(buf) % STRINGHASHSIZE;
	if ( k < 0 )
		k = -k;

	for(e = version_strings[k].ptr, tmp = NULL; e; e = e->next) {
		if (!BadPtr(e->str) && mycmp(buf, e->str) == 0) {
			if (e->refct < 0 || e->refct == INT_MAX )
				continue;

			/* Move it to the top */
			if ( tmp ) {
				tmp->next = e->next;
				e->next = version_strings[k].ptr;
				version_strings[k].ptr = e;
			}
			user->sup_version = e->str;
			e->refct++;
			break;
		}
		else tmp = e;
	}

	if ( !e ) {
		e = make_stringhash_element();
		
		DupString(user->sup_version, buf);
		e->str = user->sup_version;
		e->refct = 1;
		e->next = version_strings[k].ptr;
		version_strings[k].ptr = e;
	}
}
			
void free_sup_version(char *x)
{
	long k;
	struct StringHashElement* e, *tmp;

	if (BadPtr(x))
		return;

	k = hash_nn_name(x) % STRINGHASHSIZE;
	if (k < 0)
		k = -k;

	for(e = version_strings[k].ptr, tmp = NULL; e; e = e->next)
	{
		if (e->str == x) {
			if (e->refct > 0)	
				e->refct--;
			if (e->refct == 0) {
				if ( tmp )
					tmp->next = e->next;
				else
					version_strings[k].ptr = e->next;
				
				free_stringhash_element(e);
				return;
			}
		} else tmp = e;
	}
}

/*
** free_user
**	Decrease user reference count by one and realease block,
**	if count reaches 0
*/
void	free_user(anUser *user, aClient *cptr)
{
	if (--user->refcnt <= 0)
	    {
		if (user->away)
			MyFree((char *)user->away);
		if (user->mask)
			MyFree((char *)user->mask);

		if (!BadPtr(user->sup_version))
			free_sup_version(user->sup_version);
		user->sup_version = NULL;

#ifdef  KEEP_HURTBY
		if (user->hurtby)
			MyFree((char *)user->hurtby);
		user->hurtby = NULL;
#endif
		/*
		 * sanity check
		 */
		if (user->joined || user->refcnt < 0 ||
		    user->invited || user->channel)
#ifdef DEBUGMODE
			dumpcore("%#x user (%s!%s@%s) %#x %#x %#x %d %d",
				cptr, cptr ? cptr->name : "<noname>",
				user->username, user->host, user,
				user->invited, user->channel, user->joined,
				user->refcnt);
#else
			sendto_ops("* %#x user (%s!%s@%s) %#x %#x %#x %d %d *",
				cptr, cptr ? cptr->name : "<noname>",
				user->username, user->host, user,
				user->invited, user->channel, user->joined,
				user->refcnt);
#endif
		MyFree((char *)user);
#ifdef	DEBUGMODE
		users.inuse--;
#endif
	    }
}

/*
 * taken the code from ExitOneClient() for this and placed it here.
 * - avalon
 */
void	remove_client_from_list(aClient *cptr)
{
	checklist();
	if (cptr->prev)
		cptr->prev->next = cptr->next;
	else
	    {
		client = cptr->next;
		client->prev = NULL;
	    }
	if (cptr->next)
		cptr->next->prev = cptr->prev;
	if (IsPerson(cptr)) /* Only persons can have been added before */
	    {
		add_history(cptr);
		off_history(cptr); /* Remove all pointers to cptr */
	    }
	if (cptr->user)
		(void)free_user(cptr->user, cptr);
	if (cptr->serv)
	    {
		if (cptr->serv->user)
			free_user(cptr->serv->user, cptr);
		MyFree((char *)cptr->serv);
#ifdef	DEBUGMODE
		servs.inuse--;
#endif
	    }
#ifdef	DEBUGMODE
	if (cptr->fd == -2)
		cloc.inuse--;
	else
		crem.inuse--;
#endif
	(void)free_client(cptr);
	numclients--;
	return;
}

/*
 * although only a small routine, it appears in a number of places
 * as a collection of a few lines...functions like this *should* be
 * in this file, shouldnt they ?  after all, this is list.c, isnt it ?
 * -avalon
 */
void	add_client_to_list(aClient *cptr)
{
	/*
	 * since we always insert new clients to the top of the list,
	 * this should mean the "me" is the bottom most item in the list.
	 */
	cptr->next = client;
	client = cptr;
	if (cptr->next)
		cptr->next->prev = cptr;
	return;
}

/*
 * Look for ptr in the linked listed pointed to by link.
 */
Link	*find_user_link(Link *lp, aClient *ptr)
{
  if (ptr)
	while (lp)
	   {
		if (lp->value.cptr == ptr)
			return (lp);
		lp = lp->next;
	    }
	return NULL;
}

Link	*make_link()
{
	Link	*lp;

	lp = (Link *)MyMalloc(sizeof(Link));
#ifdef	DEBUGMODE
	links.inuse++;
#endif
	return lp;
}

void	free_link(Link *lp)
{
	MyFree((char *)lp);
#ifdef	DEBUGMODE
	links.inuse--;
#endif
}


aClass	*make_class()
{
	aClass	*tmp;

	tmp = (aClass *)MyMalloc(sizeof(aClass));
#ifdef	DEBUGMODE
	classs.inuse++;
#endif
	return tmp;
}

void	free_class(aClass *tmp)
{
	MyFree((char *)tmp);
#ifdef	DEBUGMODE
	classs.inuse--;
#endif
}

aConfItem	*make_conf()
{
	aConfItem *aconf;

	aconf = (struct ConfItem *)MyMalloc(sizeof(aConfItem));
#ifdef	DEBUGMODE
	aconfs.inuse++;
#endif
	bzero((char *)&aconf->ipnum, sizeof(struct in_addr));
	aconf->next = NULL;
	aconf->host = aconf->passwd = aconf->name = NULL;
	aconf->status = CONF_ILLEGAL;
	aconf->clients = 0;
	aconf->port = 0;
	aconf->hold = 0;
	aconf->bits = 0;
	aconf->tmpconf = 0;
	aconf->string4 = 0;
	aconf->string5 = 0;
	aconf->string6 = 0;
	aconf->string7 = 0;
	Class(aconf) = 0;
	return (aconf);
}

void	delist_conf(aConfItem *aconf)
{
	if (aconf == conf)
		conf = conf->next;
	else
	    {
		aConfItem	*bconf;

		for (bconf = conf; aconf != bconf->next; bconf = bconf->next)
			;
		bconf->next = aconf->next;
	    }
	aconf->next = NULL;
}

void	free_conf(aConfItem *aconf)
{
	del_queries((char *)aconf);
	MyFree(aconf->host);
	if (aconf->passwd)
		bzero(aconf->passwd, strlen(aconf->passwd));
	MyFree(aconf->passwd);
	MyFree(aconf->name);
	MyFree(aconf->string4);
	MyFree(aconf->string5);
	MyFree(aconf->string6);
	MyFree(aconf->string7);
	MyFree((char *)aconf);
#ifdef	DEBUGMODE
	aconfs.inuse--;
#endif
	return;
}

#ifdef	DEBUGMODE
void	send_listinfo(aClient *cptr, char *name)
{
	int	inuse = 0, mem = 0, tmp = 0;

	sendto_one(cptr, ":%s %d %s :Local: inuse: %d(%d)",
		   me.name, RPL_STATSDEBUG, name, inuse += cloc.inuse,
		   tmp = cloc.inuse * CLIENT_LOCAL_SIZE);
	mem += tmp;
	sendto_one(cptr, ":%s %d %s :Remote: inuse: %d(%d)",
		   me.name, RPL_STATSDEBUG, name,
		   crem.inuse, tmp = crem.inuse * CLIENT_REMOTE_SIZE);
	mem += tmp;
	inuse += crem.inuse;
	sendto_one(cptr, ":%s %d %s :Users: inuse: %d(%d)",
		   me.name, RPL_STATSDEBUG, name, users.inuse,
		   tmp = users.inuse * sizeof(anUser));
	mem += tmp;
	inuse += users.inuse,
	sendto_one(cptr, ":%s %d %s :Servs: inuse: %d(%d)",
		   me.name, RPL_STATSDEBUG, name, servs.inuse,
		   tmp = servs.inuse * sizeof(aServer));
	mem += tmp;
	inuse += servs.inuse,
	sendto_one(cptr, ":%s %d %s :Links: inuse: %d(%d)",
		   me.name, RPL_STATSDEBUG, name, links.inuse,
		   tmp = links.inuse * sizeof(Link));
	mem += tmp;
	inuse += links.inuse,
	sendto_one(cptr, ":%s %d %s :Classes: inuse: %d(%d)",
		   me.name, RPL_STATSDEBUG, name, classs.inuse,
		   tmp = classs.inuse * sizeof(aClass));
	mem += tmp;
	inuse += classs.inuse,
	sendto_one(cptr, ":%s %d %s :Confs: inuse: %d(%d)",
		   me.name, RPL_STATSDEBUG, name, aconfs.inuse,
		   tmp = aconfs.inuse * sizeof(aConfItem));
	mem += tmp;
	inuse += aconfs.inuse,
	sendto_one(cptr, ":%s %d %s :Totals: inuse %d %d",
		   me.name, RPL_STATSDEBUG, name, inuse, mem);
}
#endif

void    free_str_list(lp)
Reg1	Link    *lp;
{
        Reg2    Link    *next;


        while (lp) {
                next = lp->next;
                MyFree((char *)lp->value.cp);
                free_link(lp);
                lp = next;
        }

        return;
}


/*
 * Look for a match in a list of strings. Go through the list, and run
 * match() on it. Side effect: if found, this link is moved to the top of
 * the list.
 */
int     find_str_match_link(lp, str)
Reg1    Link    **lp; /* Two **'s, since we might modify the original *lp */
Reg2    char    *str;
{
        Link    *ptr, **head = lp;

        if (lp && *lp)
        {
                if (!match((*lp)->value.cp, str))
                        return 1;
                for (; (*lp)->next; *lp = (*lp)->next)
                        if (!match((*lp)->next->value.cp, str))
                        {
                                Link *temp = (*lp)->next;
                                *lp = (*lp)->next->next;
                                temp->next = *head;
                                *head = temp;
                                return 1;
                        }
                return 0;
        }
        return 0;
}
