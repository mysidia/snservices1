/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_conf.c
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

/* Changed all calls of check_pings so that only when a kline-related command
   is used will a kline check occur -- Barubary */

#define KLINE_RET_AKILL 3
#define KLINE_RET_PERM 2
#define KLINE_RET_DELOK 1
#define KLINE_DEL_ERR 0


#ifndef lint
static  char sccsid[] = "@(#)s_conf.c	2.56 02 Apr 1994 (C) 1988 University of Oulu, \
Computing Center and Jarkko Oikarinen";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include <fcntl.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <sys/wait.h>
#else
#include <io.h>
#endif
#include <sys/stat.h>
#ifdef __hpux
#include "inet.h"
#endif
#if defined(PCS) || defined(AIX) || defined(DYNIXPTX) || defined(SVR3)
#include <time.h>
#endif
#ifdef	R_LINES
#include <signal.h>
#endif

#include "h.h"

static	int	check_time_interval PROTO((char *, char *));
static	int	lookup_confhost PROTO((aConfItem *));
static	int	is_comment PROTO((char *));

aConfItem	*conf = NULL;
extern char	zlinebuf[];

/*
 * remove all conf entries from the client except those which match
 * the status field mask.
 */
void	det_confs_butmask(cptr, mask)
aClient	*cptr;
int	mask;
{
	Reg1 Link *tmp, *tmp2;

	for (tmp = cptr->confs; tmp; tmp = tmp2)
	    {
		tmp2 = tmp->next;
		if ((tmp->value.aconf->status & mask) == 0)
			(void)detach_conf(cptr, tmp->value.aconf);
	    }
}

/*
 * Add a temporary line to the configuration
 */
void	add_temp_conf(status, host, passwd, name, port, class, temp)
unsigned int	status;
char	*host;
char	*passwd;
char	*name;
int	port, class, temp;	/* temp: 0 = perm 1 = temp 2 = akill */
{
	Reg1 aConfItem *aconf;

	aconf = make_conf();

	aconf->tmpconf = temp;
	aconf->status = status;
	if (host)
	  DupString(aconf->host, host);
	if (passwd)
	  DupString(aconf->passwd, passwd);
	if (name)
	  DupString(aconf->name, name);
	aconf->port = port;
	if (class)
		Class(aconf) = find_class(class);
	if (!find_temp_conf_entry(aconf, status)) {
		aconf->next = conf;
		conf = aconf;
		aconf = NULL;
	}

	if (aconf)
	  free_conf(aconf);
}

/*
 * delete a temporary conf line.  *only* temporary conf lines may be deleted.
 */
int	del_temp_conf(status, host, passwd, name, port, class, akill)
unsigned int    status, akill;
char    *host;
char    *passwd;
char    *name;
int     port, class;
{
	Reg1	aConfItem	*aconf;
	Reg3	aConfItem	*bconf;
	u_int	mask;
	u_int	result=KLINE_DEL_ERR;

	aconf = make_conf();
	
	aconf->status=status;
	if(host)
		DupString(aconf->host, host);
	if(passwd)
		DupString(aconf->passwd, passwd);
	if(name)
		DupString(aconf->name, name);
	aconf->port = port;
	if(class)
		Class(aconf) = find_class(class);
	mask = CONF_KILL;
	if (bconf=find_temp_conf_entry(aconf,mask)) /* only if non-null ptr */
	{
/* Completely skirt the akill error messages if akill is set to 1
 * this allows RAKILL to do its thing without having to go through the
 * error checkers.  If it had to it would go kaplooey. --Russell
 */
		if (bconf->tmpconf == KLINE_PERM)
			result = KLINE_RET_PERM;/* Kline permanent */
		else if (!akill && (bconf->tmpconf == KLINE_AKILL))
			result = KLINE_RET_AKILL;  /* Akill */
		else if (akill && (bconf->tmpconf != KLINE_AKILL))
			result = KLINE_RET_PERM;
		else
		{
			bconf->status |= CONF_ILLEGAL; /* just mark illegal */
			result = KLINE_RET_DELOK;      /* same as deletion */
		}	
	}
	if (aconf)
		free_conf(aconf);
	return result; /* if it gets to here, it doesn't exist */
}

/*
 * find the first (best) I line to attach.
 */
int	attach_Iline(cptr, hp, sockhost)
aClient *cptr;
Reg2	struct	hostent	*hp;
char	*sockhost;
{
	Reg1	aConfItem	*aconf;
	Reg3	char	*hname;
	Reg4	int	i;
	static	char	uhost[HOSTLEN+USERLEN+3];
	static	char	fullname[HOSTLEN+1];

	for (aconf = conf; aconf; aconf = aconf->next)
	    {
		if (aconf->status != CONF_CLIENT)
			continue;
		if (aconf->port && aconf->port != cptr->acpt->port)
			continue;
		if (!aconf->host || !aconf->name)
			goto attach_iline;
		if (hp)
			for (i = 0, hname = hp->h_name; hname;
			     hname = hp->h_aliases[i++])
			    {
				(void)strncpy(fullname, hname,
					sizeof(fullname)-1);
				add_local_domain(fullname,
						 HOSTLEN - strlen(fullname));
				Debug((DEBUG_DNS, "a_il: %s->%s",
				      sockhost, fullname));
				if (index(aconf->name, '@'))
				    {
					(void)strcpy(uhost, cptr->username);
					(void)strcat(uhost, "@");
				    }
				else
					*uhost = '\0';
				(void)strncat(uhost, fullname,
					sizeof(uhost) - strlen(uhost));
				if (!match(aconf->name, uhost))
					goto attach_iline;
			    }

		if (index(aconf->host, '@'))
		    {
			strncpyzt(uhost, cptr->username, sizeof(uhost));
			(void)strcat(uhost, "@");
		    }
		else
			*uhost = '\0';
		(void)strncat(uhost, sockhost, sizeof(uhost) - strlen(uhost));
		if (!match(aconf->host, uhost))
			goto attach_iline;
		continue;
attach_iline:
		if (index(uhost, '@'))
			cptr->flags |= FLAGS_DOID;
		get_sockhost(cptr, uhost);
		return attach_conf(cptr, aconf);
	    }
	return -1;
}

/*
 * Find the single N line and return pointer to it (from list).
 * If more than one then return NULL pointer.
 */
aConfItem	*count_cnlines(lp)
Reg1	Link	*lp;
{
	Reg1	aConfItem	*aconf, *cline = NULL, *nline = NULL;

	for (; lp; lp = lp->next)
	    {
		aconf = lp->value.aconf;
		if (!(aconf->status & CONF_SERVER_MASK))
			continue;
		if (aconf->status == CONF_CONNECT_SERVER && !cline)
			cline = aconf;
		else if (aconf->status == CONF_NOCONNECT_SERVER && !nline)
			nline = aconf;
	    }
	return nline;
}

/*
** detach_conf
**	Disassociate configuration from the client.
**      Also removes a class from the list if marked for deleting.
*/
int	detach_conf(cptr, aconf)
aClient *cptr;
aConfItem *aconf;
{
	Reg1	Link	**lp, *tmp;

	lp = &(cptr->confs);

	while (*lp)
	    {
		if ((*lp)->value.aconf == aconf)
		    {
			if ((aconf) && (Class(aconf)))
			    {
				if (aconf->status & CONF_CLIENT_MASK)
					if (ConfLinks(aconf) > 0)
						--ConfLinks(aconf);
       				if (ConfMaxLinks(aconf) == -1 &&
				    ConfLinks(aconf) == 0)
		 		    {
					free_class(Class(aconf));
					Class(aconf) = NULL;
				    }
			     }
			if (aconf && !--aconf->clients && IsIllegal(aconf))
				free_conf(aconf);
			tmp = *lp;
			*lp = tmp->next;
			free_link(tmp);
			return 0;
		    }
		else
			lp = &((*lp)->next);
	    }
	return -1;
}

static	int	is_attached(aconf, cptr)
aConfItem *aconf;
aClient *cptr;
{
	Reg1	Link	*lp;

	for (lp = cptr->confs; lp; lp = lp->next)
		if (lp->value.aconf == aconf)
			break;

	return (lp) ? 1 : 0;
}

/*
** attach_conf
**	Associate a specific configuration entry to a *local*
**	client (this is the one which used in accepting the
**	connection). Note, that this automaticly changes the
**	attachment if there was an old one...
*/
int	attach_conf(cptr, aconf)
aConfItem *aconf;
aClient *cptr;
{
	Reg1 Link *lp;

	if (is_attached(aconf, cptr))
		return 1;
	if (IsIllegal(aconf))
		return -1;
	if ((aconf->status & (CONF_LOCOP | CONF_OPERATOR | CONF_CLIENT)) &&
	    aconf->clients >= ConfMaxLinks(aconf) && ConfMaxLinks(aconf) > 0)
		return -3;	/* Use this for printing error message */
	lp = make_link();
	lp->next = cptr->confs;
	lp->value.aconf = aconf;
	cptr->confs = lp;
	aconf->clients++;
	if (aconf->status & CONF_CLIENT_MASK)
		ConfLinks(aconf)++;
	return 0;
}


aConfItem *find_admin()
    {
	Reg1 aConfItem *aconf;

	for (aconf = conf; aconf; aconf = aconf->next)
		if (aconf->status & CONF_ADMIN)
			break;
	
	return (aconf);
    }

aConfItem *find_me()
    {
	Reg1 aConfItem *aconf;
	for (aconf = conf; aconf; aconf = aconf->next)
		if (aconf->status & CONF_ME)
			break;
	
	return (aconf);
    }

/*
 * attach_confs
 *  Attach a CONF line to a client if the name passed matches that for
 * the conf file (for non-C/N lines) or is an exact match (C/N lines
 * only).  The difference in behaviour is to stop C:*::* and N:*::*.
 */
aConfItem *attach_confs(cptr, name, statmask)
aClient	*cptr;
char	*name;
int	statmask;
{
	Reg1 aConfItem *tmp;
	aConfItem *first = NULL;
	int len = strlen(name);
  
	if (!name || len > HOSTLEN)
		return NULL;
	for (tmp = conf; tmp; tmp = tmp->next)
	    {
		if ((tmp->status & statmask) && !IsIllegal(tmp) &&
		    ((tmp->status & (CONF_SERVER_MASK|CONF_HUB)) == 0) &&
		    tmp->name && !match(tmp->name, name))
		    {
			if (!attach_conf(cptr, tmp) && !first)
				first = tmp;
		    }
		else if ((tmp->status & statmask) && !IsIllegal(tmp) &&
			 (tmp->status & (CONF_SERVER_MASK|CONF_HUB)) &&
			 tmp->name && !mycmp(tmp->name, name))
		    {
			if (!attach_conf(cptr, tmp) && !first)
				first = tmp;
		    }
	    }
	return (first);
}

/*
 * Added for new access check    meLazy
 */
aConfItem *attach_confs_host(cptr, host, statmask)
aClient *cptr;
char	*host;
int	statmask;
{
	Reg1	aConfItem *tmp;
	aConfItem *first = NULL;
	int	len = strlen(host);
  
	if (!host || len > HOSTLEN)
		return NULL;

	for (tmp = conf; tmp; tmp = tmp->next)
	    {
		if ((tmp->status & statmask) && !IsIllegal(tmp) &&
		    (tmp->status & CONF_SERVER_MASK) == 0 &&
		    (!tmp->host || match(tmp->host, host) == 0))
		    {
			if (!attach_conf(cptr, tmp) && !first)
				first = tmp;
		    }
		else if ((tmp->status & statmask) && !IsIllegal(tmp) &&
	       	    (tmp->status & CONF_SERVER_MASK) &&
	       	    (tmp->host && mycmp(tmp->host, host) == 0))
		    {
			if (!attach_conf(cptr, tmp) && !first)
				first = tmp;
		    }
	    }
	return (first);
}

/*
 * find a conf entry which matches the hostname and has the same name.
 */
aConfItem *find_conf_exact(name, user, host, statmask)
char	*name, *host, *user;
int	statmask;
{
	Reg1	aConfItem *tmp;
	char	userhost[USERLEN+HOSTLEN+3];

	(void)sprintf(userhost, "%s@%s", user, host);

	for (tmp = conf; tmp; tmp = tmp->next)
	    {
		if (!(tmp->status & statmask) || !tmp->name || !tmp->host ||
		    mycmp(tmp->name, name))
			continue;
		/*
		** Accept if the *real* hostname (usually sockecthost)
		** socket host) matches *either* host or name field
		** of the configuration.
		*/
		if (match(tmp->host, userhost))
			continue;
		if (tmp->status & (CONF_OPERATOR|CONF_LOCOP))
		    {
			if (tmp->clients < MaxLinks(Class(tmp)))
				return tmp;
			else
				continue;
		    }
		else
			return tmp;
	    }
	return NULL;
}

aConfItem *find_conf_name(name, statmask)
char	*name;
int	statmask;
{
	Reg1	aConfItem *tmp;
 
	for (tmp = conf; tmp; tmp = tmp->next)
	    {
		/*
		** Accept if the *real* hostname (usually sockecthost)
		** matches *either* host or name field of the configuration.
		*/
		if ((tmp->status & statmask) &&
		    (!tmp->name || match(tmp->name, name) == 0))
			return tmp;
	    }
	return NULL;
}

aConfItem *find_conf_servern(name)
char	*name;
{
	Reg1	aConfItem *tmp;
 
	for (tmp = conf; tmp; tmp = tmp->next)
	    {
		/*
		** Accept if the *real* hostname (usually sockecthost)
		** matches *either* host or name field of the configuration.
		*/
		if ((tmp->status & CONF_NOCONNECT_SERVER) &&
		    (!tmp->name || match(tmp->name, name) == 0))
			return tmp;
	    }
	return NULL;
}

aConfItem *find_conf(lp, name, statmask)
char	*name;
Link	*lp;
int	statmask;
{
	Reg1	aConfItem *tmp;
	int	namelen = name ? strlen(name) : 0;
  
	if (namelen > HOSTLEN)
		return (aConfItem *) 0;

	for (; lp; lp = lp->next)
	    {
		tmp = lp->value.aconf;
		if ((tmp->status & statmask) &&
		    (((tmp->status & (CONF_SERVER_MASK|CONF_HUB)) &&
	 	     tmp->name && !mycmp(tmp->name, name)) ||
		     ((tmp->status & (CONF_SERVER_MASK|CONF_HUB)) == 0 &&
		     tmp->name && !match(tmp->name, name))))
			return tmp;
	    }
	return NULL;
}

/*
 * Added for new access check    meLazy
 */
aConfItem *find_conf_host(lp, host, statmask)
Reg2	Link	*lp;
char	*host;
Reg3	int	statmask;
{
	Reg1	aConfItem *tmp;
	int	hostlen = host ? strlen(host) : 0;
  
	if (hostlen > HOSTLEN || BadPtr(host))
		return (aConfItem *)NULL;
	for (; lp; lp = lp->next)
	    {
		tmp = lp->value.aconf;
		if (tmp->status & statmask &&
		    (!(tmp->status & CONF_SERVER_MASK || tmp->host) ||
	 	     (tmp->host && !match(tmp->host, host))))
			return tmp;
	    }
	return NULL;
}

/*
 * Added for pre-I:Line checking. Skims through the conf lines looking
 * for an I:Line that matches the given mask.
 */
aConfItem *find_iline_host(host)
char	*host;
{
    Reg1	aConfItem *tmp;
    char	*s;


    for (tmp = conf; tmp; tmp = tmp->next) {
	if (!(tmp->status & CONF_CLIENT))
	    continue;
	if (!tmp->host)
	    return tmp;
	if ((s=(char *)strchr(tmp->host, '@')) != NULL) {
	    if (!match(s+1, host))
		return tmp;
	}
	else if (!match(tmp->host, host))
	    return NULL;	/* This is correct!! */
    }

    return NULL;
}

/*
 * find_conf_ip
 *
 * Find a conf line using the IP# stored in it to search upon.
 * Added 1/8/92 by Avalon.
 */
aConfItem *find_conf_ip(lp, ip, user, statmask)
char	*ip, *user;
Link	*lp;
int	statmask;
{
	Reg1	aConfItem *tmp;
	Reg2	char	*s;
  
	for (; lp; lp = lp->next)
	    {
		tmp = lp->value.aconf;
		if (!(tmp->status & statmask))
			continue;
		s = index(tmp->host, '@');
		*s = '\0';
		if (match(tmp->host, user))
		    {
			*s = '@';
			continue;
		    }
		*s = '@';
		if (!bcmp((char *)&tmp->ipnum, ip, sizeof(struct in_addr)))
			return tmp;
	    }
	return NULL;
}

/*
 * find_conf_entry
 *
 * - looks for a match on all given fields.
 */
aConfItem *find_conf_entry(aconf, mask)
aConfItem *aconf;
u_int	mask;
{
	Reg1	aConfItem *bconf;

	for (bconf = conf, mask &= ~CONF_ILLEGAL; bconf; bconf = bconf->next)
	    {
		if (!(bconf->status & mask) || (bconf->port != aconf->port))
			continue;

		if ((BadPtr(bconf->host) && !BadPtr(aconf->host)) ||
		    (BadPtr(aconf->host) && !BadPtr(bconf->host)))
			continue;
		if (!BadPtr(bconf->host) && mycmp(bconf->host, aconf->host))
			continue;

		if ((BadPtr(bconf->passwd) && !BadPtr(aconf->passwd)) ||
		    (BadPtr(aconf->passwd) && !BadPtr(bconf->passwd)))
			continue;
		if (!BadPtr(bconf->passwd) &&
		    mycmp(bconf->passwd, aconf->passwd))
			continue;

		if ((BadPtr(bconf->name) && !BadPtr(aconf->name)) ||
		    (BadPtr(aconf->name) && !BadPtr(bconf->name)))
			continue;
		if (!BadPtr(bconf->name) && mycmp(bconf->name, aconf->name))
			continue;
		break;
	    }
	return bconf;
}

/*
 * find_temp_conf_entry
 *
 * - looks for a match on all given fields for a TEMP conf line.
 *  Right now the passwd,port, and class fields are ignored, because it's
 *  only useful for k:lines anyway.  -Russell   11/22/95
 *  1/21/95 Now looks for any conf line.  I'm leaving this routine and its
 *  call in because this routine has potential in future upgrades. -Russell
 */
aConfItem *find_temp_conf_entry(aconf, mask)
aConfItem *aconf;
u_int   mask;
{
        Reg1    aConfItem *bconf;

        for (bconf = conf, mask &= ~CONF_ILLEGAL; bconf; bconf = bconf->next)
            {
		/* kline/unkline/kline fix -- Barubary */
		if (bconf->status & CONF_ILLEGAL) continue;
                if (!(bconf->status & mask) || (bconf->port != aconf->port))
                        continue;

/*                if (!bconf->tempconf) continue;*/
                if ((BadPtr(bconf->host) && !BadPtr(aconf->host)) ||
                    (BadPtr(aconf->host) && !BadPtr(bconf->host)))
                       continue;
                if (!BadPtr(bconf->host) && mycmp(bconf->host, aconf->host))
                        continue;

/*                if ((BadPtr(bconf->passwd) && !BadPtr(aconf->passwd)) ||
                    (BadPtr(aconf->passwd) && !BadPtr(bconf->passwd)))
                        continue;
                if (!BadPtr(bconf->passwd) &&
                    mycmp(bconf->passwd, aconf->passwd))
                        continue;*/

                if ((BadPtr(bconf->name) && !BadPtr(aconf->name)) ||
                    (BadPtr(aconf->name) && !BadPtr(bconf->name)))
			continue;
                if (!BadPtr(bconf->name) && mycmp(bconf->name, aconf->name))
                        continue;
                break;
            }
        return bconf;
}

/*
 * rehash
 *
 * Actual REHASH service routine. Called with sig == 0 if it has been called
 * as a result of an operator issuing this command, else assume it has been
 * called as a result of the server receiving a HUP signal.
 */
int	rehash(cptr, sptr, sig)
aClient	*cptr, *sptr;
int	sig;
{
	Reg1	aConfItem **tmp = &conf, *tmp2;
	Reg2	aClass	*cltmp;
	Reg1	aClient	*acptr;
	Reg2	int	i;
	int	ret = 0;

	if (sig == 1)
	    {
		sendto_ops("Got signal SIGHUP, reloading ircd conf. file");
#ifdef	ULTRIX
		if (fork() > 0)
			exit(0);
		write_pidfile();
#endif
	    }

	for (i = 0; i <= highest_fd; i++)
		if ((acptr = local[i]) && !IsMe(acptr))
		    {
			/*
			 * Nullify any references from client structures to
			 * this host structure which is about to be freed.
			 * Could always keep reference counts instead of
			 * this....-avalon
			 */
			acptr->hostp = NULL;
#if defined(R_LINES_REHASH) && !defined(R_LINES_OFTEN)
			if (find_restrict(acptr))
			    {
				sendto_ops("Restricting %s, closing lp",
					   get_client_name(acptr,FALSE));
				if (exit_client(cptr,acptr,sptr,"R-lined") ==
				    FLUSH_BUFFER)
					ret = FLUSH_BUFFER;
			    }
#endif
		    }

	while ((tmp2 = *tmp))
		if (tmp2->clients || tmp2->status & CONF_LISTEN_PORT)
		    {
			/*
			** Configuration entry is still in use by some
			** local clients, cannot delete it--mark it so
			** that it will be deleted when the last client
			** exits...
			*/
			if (!(tmp2->status & (CONF_LISTEN_PORT|CONF_CLIENT)))
			    {
				*tmp = tmp2->next;
				tmp2->next = NULL;
			    }
			else
				tmp = &tmp2->next;
			tmp2->status |= CONF_ILLEGAL;
		    }
		else
		    {
			*tmp = tmp2->next;
			/* free expression trees of connect rules */
			if ((tmp2->status & (CONF_CRULEALL|CONF_CRULEAUTO))
			    && (tmp2->passwd != NULL))
			  crule_free (&(tmp2->passwd));
			free_conf(tmp2);
	    	    }

	/*
	 * We don't delete the class table, rather mark all entries
	 * for deletion. The table is cleaned up by check_class. - avalon
	 */
	for (cltmp = NextClass(FirstClass()); cltmp; cltmp = NextClass(cltmp))
		MaxLinks(cltmp) = -1;

	if (sig != 2)
		flush_cache();
	(void) initconf(0);
	close_listeners();

	/*
	 * flush out deleted I and P lines although still in use.
	 */
	for (tmp = &conf; (tmp2 = *tmp); )
		if (!(tmp2->status & CONF_ILLEGAL))
			tmp = &tmp2->next;
		else
		    {
			*tmp = tmp2->next;
			tmp2->next = NULL;
			if (!tmp2->clients)
				free_conf(tmp2);
		    }
	/* Added to make sure K-lines are checked -- Barubary */
	check_pings(NOW, 1);

	/* Recheck all U-lines -- Barubary */
	for (i = 0; i < highest_fd; i++)
		if ((acptr = local[i]) && !IsMe(acptr))
		{
			if (find_conf_host(acptr->from->confs, acptr->name,
				CONF_UWORLD) || (acptr->user && find_conf_host(
				acptr->from->confs, acptr->user->server,
				CONF_UWORLD)))
				acptr->flags |= FLAGS_ULINE;
			else
				acptr->flags &= ~FLAGS_ULINE;
		}

	return ret;
}

/*
 * openconf
 *
 * returns -1 on any error or else the fd opened from which to read the
 * configuration file from.  This may either be th4 file direct or one end
 * of a pipe from m4.
 */
int	openconf()
{
#ifdef	M4_PREPROC
	int	pi[2], i;

	if (pipe(pi) == -1)
		return -1;
	switch(fork())
	{
	case -1 :
		return -1;
	case 0 :
		(void)close(pi[0]);
		if (pi[1] != 1)
		    {
			(void)dup2(pi[1], 1);
			(void)close(pi[1]);
		    }
		(void)dup2(1,2);
		for (i = 3; i < MAXCONNECTIONS; i++)
			if (local[i])
				(void) close(i);
		/*
		 * m4 maybe anywhere, use execvp to find it.  Any error
		 * goes out with report_error.  Could be dangerous,
		 * two servers running with the same fd's >:-) -avalon
		 */
		(void)execlp("m4", "m4", "ircd.m4", configfile, 0);
		report_error("Error executing m4 %s:%s", &me);
		exit(-1);
	default :
		(void)close(pi[1]);
		return pi[0];
	}
#else
	return open(configfile, O_RDONLY);
#endif
}
extern char *getfield();

int oper_access[] = {
	~(OFLAG_ADMIN),	'*',
	OFLAG_LOCAL,    	'o',
	OFLAG_GLOBAL,   	'O',
	OFLAG_SGLOBOP,  	'G',
	OFLAG_REHASH,   	'r',
	OFLAG_DIE,      	'D',
	OFLAG_RESTART,   	'R',
	OFLAG_HELPOP,   	'h',
	OFLAG_GLOBOP,   	'g',
	OFLAG_WALLOP,   	'w',
	OFLAG_LOCOP,    	'l',
	OFLAG_LROUTE,   	'c',
	OFLAG_GROUTE,   	'C',
	OFLAG_LKILL,    	'k',
	OFLAG_GKILL,    	'K',
	OFLAG_KLINE,    	'b',
	OFLAG_UNKLINE,   	'B',
	OFLAG_LNOTICE,  	'n',
	OFLAG_GNOTICE,  	'N',
	OFLAG_ADMIN,    	'A',
	OFLAG_UMODEC,   	'u',
	OFLAG_UMODEF,   	'f',
	0, 0 };

/*
** initconf() 
**    Read configuration file.
**
**    returns -1, if file cannot be opened
**             0, if file opened
*/

#define MAXCONFLINKS 150

int 	initconf(opt)
int	opt;
{
	static	char	quotes[9][2] = {{'b', '\b'}, {'f', '\f'}, {'n', '\n'},
					{'r', '\r'}, {'t', '\t'}, {'v', '\v'},
					{'\\', '\\'}, { 0, 0}};
	Reg1	char	*tmp, *s;
	int	fd, i;
	char	line[512], c[80];
	int	ccount = 0, ncount = 0;
	aConfItem *aconf = NULL;

	Debug((DEBUG_DEBUG, "initconf(): ircd.conf = %s", configfile));
	if ((fd = openconf()) == -1)
	    {
#ifdef	M4_PREPROC
		(void)wait(0);
#endif
		return -1;
	    }
	(void)dgets(-1, NULL, 0); /* make sure buffer is at empty pos */
	while ((i = dgets(fd, line, sizeof(line) - 1)) > 0)
	    {
		line[i] = '\0';
		if ((tmp = (char *)index(line, '\n')))
			*tmp = 0;
		else while(dgets(fd, c, sizeof(c) - 1) > 0)
			if ((tmp = (char *)index(c, '\n')))
			    {
				*tmp = 0;
				break;
			    }
		/*
		 * Do quoting of characters and # detection.
		 */
		for (tmp = line; *tmp; tmp++)
		    {
			if (*tmp == '\\')
			    {
				for (i = 0; quotes[i][0]; i++)
					if (quotes[i][0] == *(tmp+1))
					    {
						*tmp = quotes[i][1];
						break;
					    }
				if (!quotes[i][0])
					*tmp = *(tmp+1);
				if (!*(tmp+1))
					break;
				else
					for (s = tmp; *s = *(s+1); s++)
						;
			    }
			else if (*tmp == '#')
				*tmp = '\0';
		    }
		if (!*line || line[0] == '#' || line[0] == '\n' ||
		    line[0] == ' ' || line[0] == '\t')
			continue;
		/* Could we test if it's conf line at all?	-Vesa */
		if (line[1] != ':')
		    {
                        Debug((DEBUG_ERROR, "Bad config line: %s", line));
                        continue;
                    }
		if (aconf)
			free_conf(aconf);
		aconf = make_conf();

		tmp = getfield(line);
		if (!tmp)
			continue;
		switch (*tmp)
		{
			case 'A': /* Name, e-mail address of administrator */
			case 'a': /* of this server. */
				aconf->status = CONF_ADMIN;
				break;
			case 'C': /* Server where I should try to connect */
			case 'c': /* in case of lp failures             */
				ccount++;
				aconf->status = CONF_CONNECT_SERVER;
				break;
			/* Connect rule */
			case 'D':
				aconf->status = CONF_CRULEALL;
				break;
			/* Connect rule - autos only */
			case 'd':
				aconf->status = CONF_CRULEAUTO;
				break;
		        case 'G':
			case 'g':
			  /* General config options */
			  aconf->status = CONF_CONFIG;
			  break;
		        case 'H': /* Hub server line */
			case 'h':
				aconf->status = CONF_HUB;
				break;
			case 'I': /* Just plain normal irc client trying  */
			case 'i': /* to connect me */
				aconf->status = CONF_CLIENT;
				break;
			case 'K': /* Kill user line on irc.conf           */
			case 'k':
				aconf->status = CONF_KILL;
				break;
			/* Operator. Line should contain at least */
			/* password and host where connection is  */
			case 'L': /* guaranteed leaf server */
			case 'l':
				aconf->status = CONF_LEAF;
				break;
			/* Me. Host field is name used for this host */
			/* and port number is the number of the port */
			case 'M':
			case 'm':
				aconf->status = CONF_ME;
				break;
			case 'N': /* Server where I should NOT try to     */
			case 'n': /* connect in case of lp failures     */
				  /* but which tries to connect ME        */
				++ncount;
				aconf->status = CONF_NOCONNECT_SERVER;
				break;
			case 'O':
				aconf->status = CONF_OPERATOR;
				break;
			/* Local Operator, (limited privs --SRB)
			 * Not anymore, OperFlag access levels. -Cabal95 */
			case 'o':
				aconf->status = CONF_OPERATOR;
				break;
			case 'P': /* listen port line */
			case 'p':
				aconf->status = CONF_LISTEN_PORT;
				break;
			case 'Q': /* reserved nicks */
				aconf->status = CONF_QUARANTINED_NICK;
				break;
			case 'q': /* a server that you don't want in your */
				  /* network. USE WITH CAUTION! */
				aconf->status = CONF_QUARANTINED_SERVER;
				break;
#ifdef R_LINES
			case 'R': /* extended K line */
			case 'r': /* Offers more options of how to restrict */
				aconf->status = CONF_RESTRICT;
				break;
#endif
			case 'S': /* Service. Same semantics as   */
			case 's': /* CONF_OPERATOR                */
				aconf->status = CONF_SERVICE;
				break;
			case 'U': /* Underworld server, allowed to hack modes */
			case 'u': /* *Every* server on the net must define the same !!! */
				aconf->status = CONF_UWORLD;
				break;
			case 'Y':
			case 'y':
			        aconf->status = CONF_CLASS;
		        	break;
			case 'Z':
			case 'z':
				aconf->status = CONF_ZAP;
				break;
		    default:
			Debug((DEBUG_ERROR, "Error in config file: %s", line));
			break;
		    }
		if (IsIllegal(aconf))
			continue;

		for (;;) /* Fake loop, that I can use break here --msa */
		    {
			if ((tmp = getfield(NULL)) == NULL)
				break;
			DupString(aconf->host, tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			DupString(aconf->passwd, tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			DupString(aconf->name, tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			if (aconf->status & CONF_OPS) {
			  int   *i, flag;
			  char  *m = "*";
			  /*
			   * Now we use access flags to define
			   * what an operator can do with their O.
			   */
			  for (m = (*tmp) ? tmp : m; *m; m++) {
			    for (i = oper_access; (flag = *i); i += 2)
			      if (*m == (char)(*(i+1))) {
				aconf->port |= flag;
				break;
			      }
			  }
			  if (!(aconf->port&OFLAG_ISGLOBAL))
				aconf->status = CONF_LOCOP;
			}
			else
				aconf->port = atoi(tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			Class(aconf) = find_class(atoi(tmp));
			break;
		    }
		/*
		** If conf line is a general config, just
		** see if we recognize the keyword, and set
		** the appropriate global.  We don't use a "standard"
		** config link here, because these are things which need
		** to be tested SO often that a simple global test
		** is much better!  -Aeto
		*/
		if ((aconf->status & CONF_CONFIG) == CONF_CONFIG) {
#ifdef USE_CASETABLES
		  if (mycmp(aconf->host,"casetable") == 0) {
		    if (atoi(aconf->passwd) == 1)
                    {
                       casetable = 1;
                      touppertab = touppertab2;
                      tolowertab = tolowertab2;
                    }
		    else if (casetable == 1)
		      sendto_ops ("WARNING -- rehash tried to reduce casetable value, which is illegal.  Restart to reduce it.");

		  }
#endif
		  continue;
		}

		/* Check for bad Z-lines masks as they are *very* dangerous
		   if not correct!!! */
		if (aconf->status == CONF_ZAP)
		{
			char *tempc = aconf->host;
			if (!tempc)
			{
				free_conf(aconf);
				aconf = NULL;
				continue;
			}
			for (; *tempc; tempc++)
				if ((*tempc >= '0') && (*tempc <= '9'))
					goto zap_safe;
			free_conf(aconf);
			aconf = NULL;
			continue;
			zap_safe:;
		}
		/*
                ** If conf line is a class definition, create a class entry
                ** for it and make the conf_line illegal and delete it.
                */
		if (aconf->status & CONF_CLASS)
		    {
			add_class(atoi(aconf->host), atoi(aconf->passwd),
				  atoi(aconf->name), aconf->port,
				  tmp ? atoi(tmp) : 0);
			continue;
		    }
		/*
                ** associate each conf line with a class by using a pointer
                ** to the correct class record. -avalon
                */
		if (aconf->status & (CONF_CLIENT_MASK|CONF_LISTEN_PORT))
		    {
			if (Class(aconf) == 0)
				Class(aconf) = find_class(0);
			if (MaxLinks(Class(aconf)) < 0)
				Class(aconf) = find_class(0);
		    }
		if (aconf->status & (CONF_LISTEN_PORT|CONF_CLIENT))
		    {
			aConfItem *bconf;

			if (bconf = find_conf_entry(aconf, aconf->status))
			    {
				delist_conf(bconf);
				bconf->status &= ~CONF_ILLEGAL;
				if (aconf->status == CONF_CLIENT)
				    {
					bconf->class->links -= bconf->clients;
					bconf->class = aconf->class;
                                        if (bconf->class)
					 bconf->class->links += bconf->clients;
				    }
				free_conf(aconf);
				aconf = bconf;
			    }
			else if (aconf->host &&
				 aconf->status == CONF_LISTEN_PORT)
				(void)add_listener(aconf);
		    }
		if (aconf->status & CONF_SERVER_MASK)
			if (ncount > MAXCONFLINKS || ccount > MAXCONFLINKS ||
			    !aconf->host || index(aconf->host, '*') ||
			     index(aconf->host,'?') || !aconf->name)
				continue;

		if (aconf->status &
		    (CONF_SERVER_MASK|CONF_LOCOP|CONF_OPERATOR))
			if (!index(aconf->host, '@') && *aconf->host != '/')
			    {
				char	*newhost;
				int	len = 3;	/* *@\0 = 3 */

				len += strlen(aconf->host);
				newhost = (char *)MyMalloc(len);
				(void)sprintf(newhost, "*@%s", aconf->host);
				MyFree(aconf->host);
				aconf->host = newhost;
			    }
		if (aconf->status & CONF_SERVER_MASK)
		    {
			if (BadPtr(aconf->passwd))
				continue;
			else if (!(opt & BOOT_QUICK))
				(void)lookup_confhost(aconf);
		    }

                /* Create expression tree from connect rule...
                ** If there's a parsing error, nuke the conf structure */
                if (aconf->status & (CONF_CRULEALL | CONF_CRULEAUTO))
		  {
		    MyFree (aconf->passwd);
		    if ((aconf->passwd =
			 (char *) crule_parse (aconf->name)) == NULL)
		      {
			free_conf (aconf);
			aconf = NULL;
			continue;
		      }
		  }

		/*
		** Own port and name cannot be changed after the startup.
		** (or could be allowed, but only if all links are closed
		** first).
		** Configuration info does not override the name and port
		** if previously defined. Note, that "info"-field can be
		** changed by "/rehash".
		*/
		if (aconf->status == CONF_ME)
		    {
			strncpyzt(me.info, aconf->name, sizeof(me.info));
			if (me.name[0] == '\0' && aconf->host[0])
				strncpyzt(me.name, aconf->host,
					  sizeof(me.name));
			if (aconf->passwd[0] && (aconf->passwd[0] != '*'))
				me.ip.s_addr = inet_addr(aconf->passwd);
			else
				me.ip.s_addr = INADDR_ANY;
			if (portnum < 0 && aconf->port >= 0)
				portnum = aconf->port;
		    }
		if (aconf->status == CONF_KILL)
			aconf->tmpconf = KLINE_PERM;
		(void)collapse(aconf->host);
		(void)collapse(aconf->name);
		Debug((DEBUG_NOTICE,
		      "Read Init: (%d) (%s) (%s) (%s) (%d) (%d)",
		      aconf->status, aconf->host, aconf->passwd,
		      aconf->name, aconf->port, Class(aconf)));
		aconf->next = conf;
		conf = aconf;
		aconf = NULL;
	    }
	if (aconf)
		free_conf(aconf);
	(void)dgets(-1, NULL, 0); /* make sure buffer is at empty pos */
	(void)close(fd);
#ifdef	M4_PREPROC
	(void)wait(0);
#endif
	check_class();
	nextping = nextconnect = NOW;
	return 0;
    }

/*
 * lookup_confhost
 *   Do (start) DNS lookups of all hostnames in the conf line and convert
 * an IP addresses in a.b.c.d number for to IP#s.
 */
static	int	lookup_confhost(aconf)
Reg1	aConfItem	*aconf;
{
	Reg2	char	*s;
	Reg3	struct	hostent *hp;
	Link	ln;

	if (BadPtr(aconf->host) || BadPtr(aconf->name))
		goto badlookup;
	if ((s = index(aconf->host, '@')))
		s++;
	else
		s = aconf->host;
	/*
	** Do name lookup now on hostnames given and store the
	** ip numbers in conf structure.
	*/
	if (!isalpha(*s) && !isdigit(*s))
		goto badlookup;

	/*
	** Prepare structure in case we have to wait for a
	** reply which we get later and store away.
	*/
	ln.value.aconf = aconf;
	ln.flags = ASYNC_CONF;

	if (isdigit(*s))
		aconf->ipnum.s_addr = inet_addr(s);
	else if ((hp = gethost_byname(s, &ln)))
		bcopy(hp->h_addr, (char *)&(aconf->ipnum),
			sizeof(struct in_addr));

	if (aconf->ipnum.s_addr == -1)
		goto badlookup;
	return 0;
badlookup:
	if (aconf->ipnum.s_addr == -1)
		bzero((char *)&aconf->ipnum, sizeof(struct in_addr));
	Debug((DEBUG_ERROR,"Host/server name error: (%s) (%s)",
		aconf->host, aconf->name));
	return -1;
}

int	find_kill(cptr)
aClient	*cptr;
{
	char	reply[256], *host, *name;
	aConfItem *tmp;

	if (!cptr->user)
		return 0;

	host = cptr->sockhost;
	name = cptr->user->username;

	if (strlen(host)  > (size_t) HOSTLEN ||
            (name ? strlen(name) : 0) > (size_t) HOSTLEN)
		return (0);

	reply[0] = '\0';

	for (tmp = conf; tmp; tmp = tmp->next)
 		if ((tmp->status == CONF_KILL) && tmp->host && tmp->name &&
		    (match(tmp->host, host) == 0) &&
 		    (!name || match(tmp->name, name) == 0) &&
		    (!tmp->port || (tmp->port == cptr->acpt->port)))
                       /* can short-circuit evaluation - not taking chances
                           cos check_time_interval destroys tmp->passwd
                                                                - Mmmm
                         */
                        if (BadPtr(tmp->passwd))
                                break;
                        else if (is_comment(tmp->passwd))
                                break;
                        else if (check_time_interval(tmp->passwd, reply))
                                break;


	if (reply[0])
		sendto_one(cptr, reply,
			   me.name, ERR_YOUREBANNEDCREEP, cptr->name);
	else if (tmp)
            if (BadPtr(tmp->passwd))
		sendto_one(cptr,
			   ":%s %d %s :*** You are not welcome on this server."
			   "  Email " KLINE_ADDRESS " for more information.",
			   me.name, ERR_YOUREBANNEDCREEP, cptr->name);
             else
#ifdef COMMENT_IS_FILE
                   m_killcomment(cptr,cptr->name, tmp->passwd);
#else
	     {
	      if (tmp->tmpconf == KLINE_AKILL)
              sendto_one(cptr,
                         ":%s %d %s :*** %s",
                         me.name, ERR_YOUREBANNEDCREEP, cptr->name,
                         tmp->passwd);
	      else
              sendto_one(cptr,
                         ":%s %d %s :*** You are not welcome on this server: "
			 "%s.  Email " KLINE_ADDRESS " for more information.",
                         me.name, ERR_YOUREBANNEDCREEP, cptr->name,
                         tmp->passwd);
	     }
#endif  /* COMMENT_IS_FILE */

 	return (tmp ? -1 : 0);
}

char *find_zap(aClient *cptr, int dokillmsg)  
{
	aConfItem *tmp;
	char *retval = NULL;
	for (tmp = conf; tmp; tmp = tmp->next)
		if ((tmp->status == CONF_ZAP) && tmp->host &&
			!match(tmp->host, inetntoa((char *) &cptr->ip)))
			{
				retval = (tmp->passwd) ? tmp->passwd :
					"Reason unspecified";
				break;
			}
	if (dokillmsg && retval)
		sendto_one(cptr,
			":%s %d %s :*** You are not welcome on this server: "
			"%s.  Email " KLINE_ADDRESS " for more information.",
			me.name, ERR_YOUREBANNEDCREEP, cptr->name,
			retval);
	if (!dokillmsg && retval)
	{
		sprintf(zlinebuf,
			"ERROR :Closing Link: [%s] (You are not welcome on "
			"this server: %s.  Email " KLINE_ADDRESS " for more"
			" information.)\r\n", inetntoa((char *) &cptr->ip),
			retval);
		retval = zlinebuf;
	}
	return retval;
}

int	find_kill_byname(host, name)
char *host, *name;
{
	aConfItem *tmp;

	for (tmp = conf; tmp; tmp = tmp->next) {
 		if ((tmp->status == CONF_KILL) && tmp->host && tmp->name &&
		    (match(tmp->host, host) == 0) &&
 		    (!name || match(tmp->name, name) == 0))
		return 1;
	}

 	return 0;
 }

#ifdef R_LINES
/* find_restrict works against host/name and calls an outside program 
 * to determine whether a client is allowed to connect.  This allows 
 * more freedom to determine who is legal and who isn't, for example
 * machine load considerations.  The outside program is expected to 
 * return a reply line where the first word is either 'Y' or 'N' meaning 
 * "Yes Let them in" or "No don't let them in."  If the first word 
 * begins with neither 'Y' or 'N' the default is to let the person on.
 * It returns a value of 0 if the user is to be let through -Hoppie
 */
int	find_restrict(cptr)
aClient	*cptr;
{
	aConfItem *tmp;
	char	reply[80], temprpl[80];
	char	*rplhold = reply, *host, *name, *s;
	char	rplchar = 'Y';
	int	pi[2], rc = 0, n;

	if (!cptr->user)
		return 0;
	name = cptr->user->username;
	host = cptr->sockhost;
	Debug((DEBUG_INFO, "R-line check for %s[%s]", name, host));

	for (tmp = conf; tmp; tmp = tmp->next)
	    {
		if (tmp->status != CONF_RESTRICT ||
		    (tmp->host && host && match(tmp->host, host)) ||
		    (tmp->name && name && match(tmp->name, name)))
			continue;

		if (BadPtr(tmp->passwd))
		    {
			sendto_ops("Program missing on R-line %s/%s, ignoring",
				   name, host);
			continue;
		    }

		if (pipe(pi) == -1)
		    {
			report_error("Error creating pipe for R-line %s:%s",
				     &me);
			return 0;
		    }
		switch (rc = fork())
		{
		case -1 :
			report_error("Error forking for R-line %s:%s", &me);
			return 0;
		case 0 :
		    {
			Reg1	int	i;

			(void)close(pi[0]);
			for (i = 2; i < MAXCONNECTIONS; i++)
				if (i != pi[1])
					(void)close(i);
			if (pi[1] != 2)
				(void)dup2(pi[1], 2);
			(void)dup2(2, 1);
			if (pi[1] != 2 && pi[1] != 1)
				(void)close(pi[1]);
			(void)execlp(tmp->passwd, tmp->passwd, name, host, 0);
			exit(-1);
		    }
		default :
			(void)close(pi[1]);
			break;
		}
		*reply = '\0';
		(void)dgets(-1, NULL, 0); /* make sure buffer marked empty */
		while ((n = dgets(pi[0], temprpl, sizeof(temprpl)-1)) > 0)
		    {
			temprpl[n] = '\0';
			if ((s = (char *)index(temprpl, '\n')))
			      *s = '\0';
			if (strlen(temprpl) + strlen(reply) < sizeof(reply)-2)
				(void)sprintf(rplhold, "%s %s", rplhold,
					temprpl);
			else
			    {
				sendto_ops("R-line %s/%s: reply too long!",
					   name, host);
				break;
			    }
		    }
		(void)dgets(-1, NULL, 0); /* make sure buffer marked empty */
		(void)close(pi[0]);
		(void)kill(rc, SIGKILL); /* cleanup time */
		(void)wait(0);

		rc = 0;
		while (*rplhold == ' ')
			rplhold++;
		rplchar = *rplhold; /* Pull out the yes or no */
		while (*rplhold != ' ')
			rplhold++;
		while (*rplhold == ' ')
			rplhold++;
		(void)strcpy(reply,rplhold);
		rplhold = reply;

		if ((rc = (rplchar == 'n' || rplchar == 'N')))
			break;
	    }
	if (rc)
	    {
		sendto_one(cptr, ":%s %d %s :Restriction: %s",
			   me.name, ERR_YOUREBANNEDCREEP, cptr->name,
			   reply);
		return -1;
	    }
	return 0;
}
#endif

/*
**  output the reason for being k lined from a file  - Mmmm
** sptr is server    
** parv is the sender prefix
** filename is the file that is to be output to the K lined client
*/
int     m_killcomment(sptr, parv, filename)
aClient *sptr;
char    *parv, *filename;
{
      int     fd;
      char    line[80];
      Reg1    char     *tmp;
      struct  stat    sb;
      struct  tm      *tm;

      /*
       * stop NFS hangs...most systems should be able to open a file in
       * 3 seconds. -avalon (curtesy of wumpus)
       */
      (void)alarm(3);
      fd = open(filename, O_RDONLY);
      (void)alarm(0);
      if (fd == -1)
          {
              sendto_one(sptr, err_str(ERR_NOMOTD), me.name, parv);
              sendto_one(sptr,
                         ":%s %d %s :*** You are not welcome to this server.",
                         me.name, ERR_YOUREBANNEDCREEP, parv);
              return 0;
          }
      (void)fstat(fd, &sb);
      tm = localtime(&sb.st_mtime);
      (void)dgets(-1, NULL, 0); /* make sure buffer is at empty pos */
      while (dgets(fd, line, sizeof(line)-1) > 0)
          {
              if ((tmp = (char *)index(line,'\n')))
                      *tmp = '\0';
              if ((tmp = (char *)index(line,'\r')))
                      *tmp = '\0';
              /* sendto_one(sptr,
                         ":%s %d %s : %s.",
                         me.name, ERR_YOUREBANNEDCREEP, parv,line); */
              sendto_one(sptr, rpl_str(RPL_MOTD), me.name, parv, line); 
          }
       sendto_one(sptr,
                  ":%s %d %s :*** You are not welcome to this server.",
                   me.name, ERR_YOUREBANNEDCREEP, parv);
      (void)dgets(-1, NULL, 0); /* make sure buffer is at empty pos */
      (void)close(fd);
      return 0;
  }


/*
** is the K line field an interval or a comment? - Mmmm
*/

static int is_comment(comment)
char 	*comment;
{
	int i;
        for (i=0; i<strlen(comment); i++)
          if ( (comment[i] != ' ') && (comment[i] != '-')
                                     && (comment[i] != ',') 
                  &&  ( (comment[i] < '0') || (comment[i] > '9') ) )
           		return(1);

	return(0);
}


/*
** check against a set of time intervals
*/

static	int	check_time_interval(interval, reply)
char	*interval, *reply;
{
	struct tm *tptr;
 	time_t	tick;
 	char	*p;
 	int	perm_min_hours, perm_min_minutes,
 		perm_max_hours, perm_max_minutes;
 	int	now, perm_min, perm_max;

 	tick = NOW;
	tptr = localtime(&tick);
 	now = tptr->tm_hour * 60 + tptr->tm_min;

	while (interval)
	    {
		p = (char *)index(interval, ',');
		if (p)
			*p = '\0';
		if (sscanf(interval, "%2d%2d-%2d%2d",
			   &perm_min_hours, &perm_min_minutes,
			   &perm_max_hours, &perm_max_minutes) != 4)
		    {
			if (p)
				*p = ',';
			return(0);
		    }
		if (p)
			*(p++) = ',';
		perm_min = 60 * perm_min_hours + perm_min_minutes;
		perm_max = 60 * perm_max_hours + perm_max_minutes;
           	/*
           	** The following check allows intervals over midnight ...
           	*/
		if ((perm_min < perm_max)
		    ? (perm_min <= now && now <= perm_max)
		    : (perm_min <= now || now <= perm_max))
		    {
			(void)sprintf(reply,
				":%%s %%d %%s :%s %d:%02d to %d:%02d.",
				"You are not allowed to connect from",
				perm_min_hours, perm_min_minutes,
				perm_max_hours, perm_max_minutes);
			return(ERR_YOUREBANNEDCREEP);
		    }
		if ((perm_min < perm_max)
		    ? (perm_min <= now + 5 && now + 5 <= perm_max)
		    : (perm_min <= now + 5 || now + 5 <= perm_max))
		    {
			(void)sprintf(reply, ":%%s %%d %%s :%d minute%s%s",
				perm_min-now,(perm_min-now)>1?"s ":" ",
				"and you will be denied for further access");
			return(ERR_YOUWILLBEBANNED);
		    }
		interval = p;
	    }
	return(0);
}

/*
** m_rakill;
**      parv[0] = sender prefix
**      parv[1] = hostmask
**      parv[2] = username
**      parv[3] = comment
*/
int     m_rakill(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int     parc;
char    *parv[];
    {
        if (check_registered(sptr))
                return 0;

        if (parc < 3)
            {
                sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
                           me.name, parv[0], "RAKILL");
                return 0;
            }

        if (IsServer(cptr) &&
            find_conf_host(cptr->confs, sptr->name, CONF_UWORLD))
        {
                if (find_kill_byname(parv[1], parv[2]))
		{
#ifndef COMMENT_IS_FILE
                	del_temp_conf(CONF_KILL, parv[1], parv[3], 
				parv[2], 0, 0, 1); 
#else
                	del_temp_conf(CONF_KILL, parv[1], NULL,
				parv[2], 0, 0, 1);
#endif
		}
                if(parv[3])
                        sendto_serv_butone(cptr, ":%s RAKILL %s %s :%s", 
				parv[0], parv[1], parv[2], parv[3]);
                else
                        sendto_serv_butone(cptr, ":%s RAKILL %s %s", 
				parv[0], parv[1], parv[2]);
                check_pings(NOW, 1);
        }

    }
/* ** m_akill;
**	parv[0] = sender prefix
**	parv[1] = hostmask
**	parv[2] = username
**	parv[3] = comment
*/
int	m_akill(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	if (check_registered(sptr))
		return 0;

	if (parc < 3)
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "AKILL");
		return 0;
	    }

	if (IsServer(cptr) &&
	    find_conf_host(cptr->confs, sptr->name, CONF_UWORLD))
	{
		if (!find_kill_byname(parv[1], parv[2]))
		{

#ifndef COMMENT_IS_FILE
	 		add_temp_conf(CONF_KILL, parv[1], parv[3], parv[2], 0, 0, 2); 
#else
			add_temp_conf(CONF_KILL, parv[1], NULL, parv[2], 0, 0, 2); 
#endif
		}
		if(parv[3])
			sendto_serv_butone(cptr, ":%s AKILL %s %s :%s", parv[0],
				     parv[1], parv[2], parv[3]);
		else
			sendto_serv_butone(cptr, ":%s AKILL %s %s", parv[0],
				     parv[1], parv[2]);
		check_pings(NOW, 1);
	}

    }

/*
** m_kline;
**	parv[0] = sender prefix
**	parv[1] = nickname
**	parv[2] = comment or filename
*/
int	m_kline(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
    {
	char *host, *tmp, *hosttemp;
	char uhost[80], name[80];
	int ip1, ip2, ip3, temp;
	aClient *acptr;

        if (!MyClient(sptr) || !OPCanKline(sptr))
            {
                sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
                return 0;
            }

	if (check_registered(sptr))
		return 0;

	if (parc < 2)
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "KLINE");
		return 0;
	    }


/* This patch allows opers to quote kline by address as well as nick
 * --Russell
 */
	if (hosttemp = strchr(parv[1], '@'))
	{
		temp = 0;
		while (temp <= 20)
			name[temp++] = 0;
		strcpy(uhost, ++hosttemp);
		strncpy(name, parv[1], hosttemp-1-parv[1]);
		if (name[0] == '\0' || uhost[0] == '\0')
		{
			Debug((DEBUG_INFO, "KLINE: Bad field!"));
			sendto_one (sptr, "NOTICE %s :If you're going to add a userhost, at LEAST specify both fields", parv[0]);	
			return 0;
		}
		if (!strcmp(uhost, "*") || !strchr(uhost, '.'))
		{
			sendto_one (sptr, "NOTICE %s :What a sweeping K:Line.  If only your admin knew you tried that..", parv[0]);
			return 0;
		}
	}	

/* by nick */
	else
	{
		if (!(acptr = find_client(parv[1], NULL))) {
			if (!(acptr = get_history(parv[1], (long)KILLCHASETIMELIMIT))) {
				sendto_one(sptr, "NOTICE %s :Can't find user %s to add KLINE",
					   parv[0], parv[1]);
				return 0;
			}
		}

		if (!acptr->user)
			return 0;

		strcpy(name, acptr->user->username);
		if (MyClient(acptr))
			host = acptr->sockhost;
		else
			host = acptr->user->host;

		/* Sanity checks */

		if (name == '\0' || host == '\0')
		{
			Debug((DEBUG_INFO, "KLINE: Bad field"));
			sendto_one(sptr, "NOTICE %s :Bad field!", parv[0]);
			return 0;
		}

		/* Add some wildcards */


		strcpy(uhost, host);
		if (strchr("0123456789", host[0])) {
			if (sscanf(host, "%d.%d.%d.%*d", &ip1, &ip2, &ip3))
				sprintf(uhost, "%d.%d.%d.*", ip1, ip2, ip3);
		}
		else if (sscanf(host, "%*[^.].%*[^.].%s", uhost)) { /* Not really... */
			tmp = (char*)strchr(host, '.');
			sprintf(uhost, "*%s", tmp);
		}
	}

	sendto_ops("%s added a temp k:line for %s@%s %s", parv[0], name, uhost, parv[2] ? parv[2] : "");
 	add_temp_conf(CONF_KILL, uhost, parv[2], name, 0, 0, 1);
	check_pings(NOW, 1);
    }
	

/*
 *  m_unkline
 *    parv[0] = sender prefix
 *    parv[1] = userhost
 */

int m_unkline(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int     parc;
char    *parv[];
{

	int	result, temp;
	char	*hosttemp=parv[1], host[80], name[80];

	if (!MyClient(sptr) || !OPCanUnKline(sptr))
	{
                sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
                return 0;
	}
	if (parc<2)
	{
		sendto_one(sptr,"NOTICE %s :Not enough parameters", parv[0]);
		return 0;
	}
        if (hosttemp = strchr(parv[1], '@'))
        {
                temp = 0;
                while (temp <= 20)
                        name[temp++] = 0;
                strcpy(host, ++hosttemp);
                strncpy(name, parv[1], hosttemp-1-parv[1]);
                if (name[0] == '\0' || host[0] == '\0')
                {
                        Debug((DEBUG_INFO, "UNKLINE: Bad field"));
			sendto_one(sptr, "NOTICE %s : Both user and host fields must be non-null", parv[0]);
			return 0;
		}
		result = del_temp_conf(CONF_KILL, host, NULL, name, 
			NULL, NULL, 0);
		if (result == KLINE_RET_AKILL) {	/* akill - result = 3 */
			sendto_one(sptr, "NOTICE %s :You may not remove autokills.  Only U:lined clients may.", parv[0]);
			return 0;
		}
		if (result ==  KLINE_RET_PERM) {	/* Not a temporary line - result =2 */
			sendto_one(sptr,"NOTICE %s :You may not remove permanent K:Lines - talk to the admin", parv[0]);
			return 0;
		}
		if (result ==  KLINE_RET_DELOK)  {	/* Successful result = 1*/
			sendto_one(sptr,"NOTICE %s :Temp K:Line %s@%s is now removed.", parv[0],name,host);
			sendto_ops("%s removed temp k:line %s@%s", parv[0], name, host);
			return 0;
		}
		if (result == KLINE_DEL_ERR) {	/* Unsuccessful result = 0*/
			sendto_one(sptr,"NOTICE %s :Temporary K:Line %s@%s not found", parv[0],name,host);
			return 0;
		}
	}
	/* This wasn't here before -- Barubary */
	check_pings(NOW, 1);
}
