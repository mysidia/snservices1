/*
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

#include "ircd.h"

#define KLINE_RET_AKILL 3
#define KLINE_RET_PERM 2
#define KLINE_RET_DELOK 1
#define KLINE_DEL_ERR 0

#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <arpa/inet.h>

#include "msg.h"

IRCD_SCCSID("@(#)s_conf.c	2.56 02 Apr 1994 (C) 1988 University of Oulu, Computing Center and Jarkko Oikarinen");
IRCD_RCSID("$Id$");

static int check_time_interval(char *, char *);
static int lookup_confhost(aConfItem *);
static int is_comment(char *);
static int banmask_check(char *userhost, int ipstat);

aConfItem	*conf = NULL;
extern char	zlinebuf[];

char	*network = NULL;
char	*kline = NULL;
char	*network_kline = NULL;

/*
 * remove all conf entries from the client except those which match
 * the status field mask.
 */
void	det_confs_butmask(aClient *cptr, int mask)
{
	Link *tmp, *tmp2;

	for (tmp = cptr->confs; tmp; tmp = tmp2)
	    {
		tmp2 = tmp->next;
		if ((tmp->value.aconf->status & mask) == 0)
			detach_conf(cptr, tmp->value.aconf);
	    }
}

/*
 * Add a temporary line to the configuration
 * temp:
 * 0 = perm
 * 1 = temp
 * 2 = akill
 */
aConfItem *
add_temp_conf(unsigned int status, char *host, char *passwd, char *name,
	      int port, int class, int temp)
{
	aConfItem *aconf;
	aConfItem *new_conf = NULL;

	aconf = make_conf();

	aconf->tmpconf = temp;
	aconf->status = status;
	if (host)
		aconf->host = irc_strdup(host);
	if (passwd)
		aconf->passwd = irc_strdup(passwd);
	if (name)
		aconf->name = irc_strdup(name);

	aconf->port = port;
	if (class)
		Class(aconf) = find_class(class);
	if (!find_temp_conf_entry(aconf, status)) {
		aconf->next = conf;
		conf = aconf;
		new_conf = aconf;
		aconf = NULL;
	}

	if (aconf)
		free_conf(aconf);

	return (new_conf);
}

/*
 * delete a temporary conf line.  *only* temporary conf lines may be deleted.
 */
int del_temp_conf(unsigned int status, char *host, char *passwd, char *name, int port, int class, unsigned int akill)
{
	aConfItem	*aconf, *bconf;
	u_int	mask, result = KLINE_DEL_ERR;

	aconf = make_conf();
	
	aconf->status=status;
	if(host)
		aconf->host = irc_strdup(host);
	if(passwd)
		aconf->passwd = irc_strdup(passwd);
	if(name)
		aconf->name = irc_strdup(name);
	aconf->port = port;
	if(class)
		Class(aconf) = find_class(class);
        if (status & CONF_AHURT) mask = status &= ~(CONF_ILLEGAL);
	else if (status & CONF_ZAP) mask = CONF_ZAP;
	else                     mask = CONF_KILL;
	if ((bconf = find_temp_conf_entry(aconf, mask))) {
/* Completely skirt the akill error messages if akill is set to 1
 * this allows RAKILL to do its thing without having to go through the
 * error checkers.  If it had to it would go kaplooey. --Russell
 */
		if (bconf->tmpconf == KLINE_PERM && (akill < 3))
			result = KLINE_RET_PERM;/* Kline permanent */
		else if (!akill && (bconf->tmpconf == KLINE_AKILL))
			result = KLINE_RET_AKILL;  /* Akill */
		else if (akill && (bconf->tmpconf != KLINE_AKILL) && (akill < 3))
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
int	attach_Iline(aClient *cptr, struct HostEnt *hp, char *sockhost)
{
	aConfItem	*aconf;
	char	*hname;
	int	i;
	static	char	uhost[HOSTLEN+USERLEN+3];
	static	char	fullname[HOSTLEN+1];

	for (aconf = conf; aconf; aconf = aconf->next)
	    {
		if (aconf->status != CONF_CLIENT)
			continue;
		if (!aconf->host || !aconf->name)
			goto attach_iline;
		if (hp)
			for (i = 0, hname = hp->h_name; hname;
			     hname = hp->h_aliases[i++])
			    {
				strncpy(fullname, hname,
					sizeof(fullname)-1);
				add_local_domain(fullname,
						 HOSTLEN - strlen(fullname));
				Debug((DEBUG_DNS, "a_il: %s->%s",
				      sockhost, fullname));
				if (index(aconf->name, '@'))
				    {
					strcpy(uhost, cptr->username);
					strcat(uhost, "@");
				    }
				else
					*uhost = '\0';
				strncat(uhost, fullname,
					sizeof(uhost) - strlen(uhost));
				if (*uhost != '%') {
					if (!match(aconf->name, uhost))
						goto attach_iline;
					else if (!match(aconf->name, uhost+1))
						goto attach_iline;
				}
			    }

		if (index(aconf->host, '@'))
		    {
			strncpyzt(uhost, cptr->username, sizeof(uhost));
			strcat(uhost, "@");
		    }
		else
			*uhost = '\0';
		strncat(uhost, sockhost, sizeof(uhost) - strlen(uhost));
		if (!match(aconf->host, uhost))
			goto attach_iline;
		continue;
attach_iline:
		get_sockhost(cptr, uhost);
		return attach_conf(cptr, aconf);
	    }
	return -1;
}

/*
 * Find the single N line and return pointer to it (from list).
 * If more than one then return NULL pointer.
 */
aConfItem *count_cnlines(Link *lp)
{
	aConfItem *aconf, *cline = NULL, *nline = NULL;

	for (; lp; lp = lp->next)
	    {
		aconf = lp->value.aconf;
		if (!IsCNLine(aconf))
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
int	detach_conf(aClient *cptr, aConfItem *aconf)
{
	Link	**lp, *tmp;

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

static int is_attached(aConfItem *aconf, aClient *cptr)
{
	Link	*lp;

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
int	attach_conf(aClient *cptr, aConfItem *aconf)
{
	Link *lp;

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
	aConfItem *aconf;

	for (aconf = conf; aconf; aconf = aconf->next)
		if (aconf->status & CONF_ADMIN)
			break;
	
	return (aconf);
}

/*
 * attach_confs
 *  Attach a CONF line to a client if the name passed matches that for
 * the conf file (for non-C/N lines) or is an exact match (C/N lines
 * only).  The difference in behaviour is to stop C:*::* and N:*::*.
 */
aConfItem *attach_confs(aClient *cptr, char *name, int statmask)
{
	aConfItem *tmp, *first = NULL;
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
aConfItem *attach_confs_host(aClient *cptr, char *host, int statmask)
{
	aConfItem *tmp, *first = NULL;
	int	len = strlen(host);
  
	if (!host || len > HOSTLEN)
		return NULL;

	for (tmp = conf; tmp; tmp = tmp->next)
	    {
		if ((tmp->status & statmask) && !IsIllegal(tmp) &&
		    (IsCNLine(tmp) == 0) &&
		    (!tmp->host || match(tmp->host, host) == 0))
		    {
			if (!attach_conf(cptr, tmp) && !first)
				first = tmp;
		    }
		else if ((tmp->status & statmask) && !IsIllegal(tmp) &&
	       	    IsCNLine(tmp) &&
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
aConfItem *find_conf_exact(char *name, char *user, char *host, int statmask)
{
	aConfItem *tmp;
	char	userhost[USERLEN+HOSTLEN+3];

	sprintf(userhost, "%s@%s", user, host);

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

aConfItem *find_conf_name(char *name, int statmask)
{
	aConfItem *tmp;
 
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

aConfItem *find_conf_servern(char *name)
{
	aConfItem *tmp;
 
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

aConfItem *find_conf(Link *lp, char *name, int statmask)
{
	aConfItem *tmp;
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
aConfItem *find_conf_host(Link *lp, char *host, int statmask)
{
	aConfItem *tmp;
	int	hostlen = host ? strlen(host) : 0;
  
	if (hostlen > HOSTLEN || BadPtr(host))
		return (aConfItem *)NULL;
	for (; lp; lp = lp->next)
	    {
		tmp = lp->value.aconf;
		if (tmp->status & statmask &&
		    (!(IsCNLine(tmp) || tmp->host) ||
	 	     (tmp->host && !match(tmp->host, host))))
			return tmp;
	    }
	return NULL;
}

/*
 * find_conf_ip
 *
 * Find a conf line using the IP# stored in it to search upon.
 * Added 1/8/92 by Avalon.
 */
aConfItem *find_conf_ip(Link *lp, sock_address *addr, char *user, int statmask)
{
	aConfItem *tmp;
	char	*s;
  
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
		if (!address_compare(tmp->addr, addr))
			return tmp;
	    }
	return NULL;
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
aConfItem *find_temp_conf_entry(aConfItem *aconf, u_int mask)
{
        aConfItem *bconf;

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
int	rehash(aClient *cptr, aClient *sptr, int sig)
{
	aConfItem **tmp = &conf, *tmp2;
	aClient	*acptr;
	int	ret = 0;

	if (sig == 1)
	{
		sendto_ops("Got signal SIGHUP, reloading ircd conf. file");
	}

	for (acptr = &me; acptr; acptr = acptr->lnext)
	{
		if (!IsMe(acptr))
		{
			/*
			 * Nullify any references from client structures to
			 * this host structure which is about to be freed.
			 * Could always keep reference counts instead of
			 * this....-avalon
			 */
			acptr->hostp = NULL;
		}
	}

	if (sig != 2)
		flush_cache();
	config_read("ircd", configfile);

	/*
	 * flush out deleted lines although still in use.
	 */
	for (tmp = &conf; (tmp2 = *tmp); )
	{
		if (!(tmp2->status & CONF_ILLEGAL))
		{
			tmp = &tmp2->next;
		}
		else
		{
			*tmp = tmp2->next;
			tmp2->next = NULL;
			if (!tmp2->clients)
			{
				free_conf(tmp2);
			}
		}
	}
#ifdef HASH_MSGTAB
	/* rebuild the commands hashtable  */
	msgtab_buildhash();
#endif

	/* Added to make sure K-lines are checked -- Barubary */
	check_pings(NOW, 1, NULL);

	/* Recheck all U-lines -- Barubary */
	for (acptr = &me; acptr; acptr = acptr->lnext)
		if (!IsMe(acptr))
		{
			if (find_conf_host(acptr->from->confs, acptr->name,
				CONF_UWORLD) || (acptr->user && find_conf_host(
				acptr->from->confs, acptr->user->server,
				CONF_UWORLD)))
				ClientFlags(acptr) |= FLAGS_ULINE;
			else
				ClientFlags(acptr) &= ~FLAGS_ULINE;
		}

	return ret;
}

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
	OFLAG_MHACK,		'M',
	OFLAG_LNOTICE,  	'n',
	OFLAG_GNOTICE,  	'N',
	OFLAG_ADMIN,    	'A',
	OFLAG_UMODEC,   	'u',
	OFLAG_UMODEF,   	'f',
	OFLAG_ZLINE,		'z',
	0, 0 };

/*
 * conf_me is called when the global configuration options are changed.
 */

static CONF_HANDLER(conf_me)
{
	char	*tmp = config_get_string(n, "name");

	if (tmp != NULL && me.name[0] == '\0')
	{
		strncpyzt(me.name, tmp, sizeof(me.name));
	}
	tmp = config_get_string(n, "info");
	if (tmp != NULL)
	{
		strncpyzt(me.info, tmp, sizeof(me.info));
	}
	tmp = config_get_string(n, "port");
	if (tmp != NULL)
	{
		portnum = atoi(tmp);
	}
	tmp = config_get_string(n, "network");
	if (tmp != NULL)
	{
		irc_free(network);
		network = irc_strdup(tmp);
	}
	tmp = config_get_string(n, "kline-address");
	if (tmp != NULL)
	{
		irc_free(kline);
		kline = irc_strdup(tmp);
	}
	tmp = config_get_string(n, "network-kline-address");
	if (tmp != NULL)
	{
		irc_free(network_kline);
		network_kline = irc_strdup(tmp);
	}
	return CONFIG_OK;
}

/*
 * conf_listener is called when a listener is added or removed.
 */

static CONF_HANDLER(conf_listener)
{
	aConfItem	*aconf;
	aClient	*cptr;
	char	*tmp;

	if (n->status == CONFIG_NEW)
	{
		tmp = config_get_string(n, "address");
		if (tmp == NULL)
		{
			return CONFIG_BAD;
		}
		aconf = make_conf();
		aconf->status = CONF_LISTEN_PORT;
		aconf->host = irc_strdup(tmp);
		tmp = config_get_string(n, "port");
		aconf->port = (tmp == NULL?0:atoi(tmp));
		tmp = config_get_string(n, "ssl");
		if (tmp != NULL)
		{
			aconf->name = irc_strdup(tmp);
		}
		Class(aconf) = find_class(0);

		n->data = add_listener(aconf);

		aconf->next = conf;
		conf = aconf;
	}
	else
	{
		cptr = (aClient *) n->data;
		aconf = cptr->confs->value.aconf;
		close_connection(cptr);
		aconf->status = CONF_ILLEGAL;
	}
	return CONFIG_OK;
}

/*
 * conf_client is called when a client configuration node is added or removed.
 */

static CONF_HANDLER(conf_client)
{
	aConfItem	*aconf;
	char	*amask, *dmask, *password, *class;
	if (n->status == CONFIG_NEW)
	{
		amask = config_get_string(n, "address-mask");
		dmask = config_get_string(n, "domain-mask");
		password = config_get_string(n, "password");
		class = config_get_string(n, "class");

		if (amask == NULL || dmask == NULL || class == NULL)
		{
			return CONFIG_BAD;
		}

		aconf = make_conf();
		aconf->status = CONF_CLIENT;
		aconf->host = irc_strdup(amask);
		if (password != NULL)
		{
			aconf->passwd = irc_strdup(password);
		}
		aconf->name = irc_strdup(dmask);
		Class(aconf) = find_class(class == NULL?0:atoi(class));
		n->data = aconf;
		aconf->next = conf;
		conf = aconf;
	}
	else
	{
		aconf = n->data;
		aconf->status = CONF_ILLEGAL;
	}
	return CONFIG_OK;
}

static CONF_HANDLER(conf_server)
{
	aConfItem	*aconf, **confs;
	char	*name, *address, *password, *connect, *class, *hub, *services;
	char	*ssl;
	int	i;

	if (n->status == CONFIG_NEW)
	{
		name = config_get_string(n, "name");
		address = config_get_string(n, "address");
		password = config_get_string(n, "password");
		connect = config_get_string(n, "connect");
		class = config_get_string(n, "class");
		hub = config_get_string(n, "hub");
		services = config_get_string(n, "services");
		ssl = config_get_string(n, "ssl");

		if (name == NULL || ((address == NULL || password == NULL || class == NULL) && services == NULL))
		{
			return CONFIG_BAD;
		}
		confs = irc_malloc(sizeof(aConfItem *) * 4);
		confs[0] = confs[1] = confs[2] = confs[3] = NULL;
		if (address != NULL && password != NULL)
		{
			aconf = make_conf();
			aconf->status = CONF_NOCONNECT_SERVER;
			aconf->host = irc_strdup(address);
			aconf->passwd = irc_strdup(password);
			aconf->name = irc_strdup(name);
			Class(aconf) = find_class(class == NULL?0:atoi(class));
			lookup_confhost(aconf);
			aconf->next = conf;
			conf = aconf;
			confs[0] = aconf;

			if (connect != NULL && (!strcmp(connect, "yes") || !strcmp(connect, "auto")))
			{
				aconf = make_conf();
				aconf->status = CONF_CONNECT_SERVER;
				aconf->host = irc_strdup(address);
				aconf->passwd = irc_strdup(password);
				aconf->name = irc_strdup(name);
				if (!strcmp(connect, "auto"))
				{
					aconf->port = portnum;
				}
				if (ssl != NULL)
				{
					aconf->string4 = irc_strdup(ssl);
				}
				Class(aconf) = find_class(class == NULL?0:atoi(class));
				lookup_confhost(aconf);
				aconf->next = conf;
				conf = aconf;
				confs[1] = aconf;
			}
			if (hub != NULL && !strcmp(hub, "yes"))
			{
				aconf = make_conf();
				aconf->status = CONF_HUB;
				aconf->name = irc_strdup(name);
				aconf->next = conf;
				conf = aconf;
				confs[2] = aconf;
			}
		}
		if (services != NULL && !strcmp(services, "yes"))
		{
			aconf = make_conf();
			aconf->status = CONF_UWORLD;
			aconf->host = irc_strdup(name);
			aconf->next = conf;
			conf = aconf;
			confs[3] = aconf;
		}
		n->data = confs;
	}
	else
	{
		confs = n->data;
		for (i=0; i<4; i++)
		{
			if (confs[i] != NULL)
			{
				confs[i]->status = CONF_ILLEGAL;
			}
		}		
	}
	return CONFIG_OK;
}

static CONF_HANDLER(conf_class)
{
	aClass	*class;
	char	*name, *pingfreq, *connfreq, *maxconn, *sendqueue;

	if (n->status == CONFIG_NEW)
	{
		name = config_get_string(n, "name");
		if (name == NULL)
		{
			return CONFIG_BAD;
		}
		pingfreq = config_get_string(n, "ping-frequency");
		connfreq = config_get_string(n, "connect-frequency");
		maxconn = config_get_string(n, "max-connections");
		sendqueue = config_get_string(n, "send-queue");
		n->data = add_class((name == NULL?0:atoi(name)),
			(pingfreq == NULL?0:atoi(pingfreq)),
			(connfreq == NULL?0:atoi(connfreq)),
			(maxconn == NULL?0:atoi(maxconn)),
			(sendqueue == NULL?0:atoi(sendqueue)));
	}
	else
	{
		class = (aClass *) n->data;
		MaxLinks(class) = -1;
	}
	return CONFIG_OK;
}

static CONF_HANDLER(conf_operator)
{
	aConfItem	*aconf;
	char	*nick, *mask, *password, *class, *flags;
	int	*i, flag;
	char	*m = "*";

	if (n->status == CONFIG_NEW)
	{
		nick = config_get_string(n, "nick");
		mask = config_get_string(n, "mask");
		password = config_get_string(n, "password");
		class = config_get_string(n, "class");
		flags = config_get_string(n, "flags");

		if (nick == NULL || mask == NULL || password == NULL)
		{
			return CONFIG_BAD;
		}

		aconf = make_conf();
		aconf->status = CONF_OPERATOR;
		aconf->host = irc_strdup(mask);
		aconf->passwd = irc_strdup(password);
		aconf->name = irc_strdup(nick);
		Class(aconf) = find_class(class == NULL?0:atoi(class));

		for (m = (*flags) ? flags : m; *m; m++)
		{
			for (i = oper_access; (flag = *i); i += 2)
			{
				if (*m == (char)(*(i+1)))
				{
					aconf->port |= flag;
					break;
				}
			}
		}
		if (!(aconf->port & OFLAG_ISGLOBAL))
		{
			aconf->status = CONF_LOCOP;
		}

		if (!index(aconf->host, '@'))
		{
			char	*newhost;
			int	len = 3;	/* *@\0 = 3 */

			len += strlen(aconf->host);
			newhost = irc_malloc(len);
			sprintf(newhost, "*@%s", aconf->host);
			irc_free(aconf->host);
			aconf->host = newhost;
		}

		aconf->next = conf;
		conf = aconf;

		n->data = aconf;
	}
	else
	{
		aconf = n->data;
		aconf->status = CONF_ILLEGAL;		
	}
	return CONFIG_OK;
}

/*
 * conf_init is called once, at startup, to setup the necessary configuration
 * monitoring handlers.
 */

void conf_init()
{
	char	*net = "ExampleNet";
	char	*kl = "kline@example.net";

	config_monitor("listener", conf_listener, CONFIG_LIST);
	config_monitor("server", conf_server, CONFIG_LIST);
	config_monitor("client", conf_client, CONFIG_LIST);
	config_monitor("operator", conf_operator, CONFIG_LIST);
	config_monitor("class", conf_class, CONFIG_LIST);
	config_monitor("config", conf_me, CONFIG_SINGLE);

	network = irc_strdup(net);
	kline = irc_strdup(kl);
	network_kline = irc_strdup(kl);
}

/*
 * lookup_confhost
 *   Do (start) DNS lookups of all hostnames in the conf line and convert
 * an IP addresses in a.b.c.d number for to IP#s.
 */
static	int	lookup_confhost(aConfItem *aconf)
{
	char	*s;
	/* struct	HostEnt *hp;*/
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

	if (aconf->status == CONF_SUP_ZAP) {
		goto badlookup;
	}

	/*
	** Prepare structure in case we have to wait for a
	** reply which we get later and store away.
	*/
	ln.value.aconf = aconf;
	ln.flags = ASYNC_CONF;

	aconf->addr = address_make(s, (aconf->port > 0 ? aconf->port : portnum));
	if (aconf->addr == NULL)
	{
		goto badlookup;
	}
	return 0;
badlookup:
	memset(&aconf->addr, 0, sizeof(sock_address));
	Debug((DEBUG_ERROR,"Host/server name error: (%s) (%s)",
		aconf->host, aconf->name));
	return -1;
}

int	find_kill(aClient *cptr, aConfItem *conf_target)
{
	char	reply[256], *host, *name, u_ip[HOSTLEN + 25], *u_sip;
	aConfItem *tmp = NULL;

	if (!cptr->user)
		return 0;

	host = cptr->sockhost;
	name = cptr->user->username;
        if ((u_sip = address_tostring(cptr->sock->raddr, 0)))
            strncpyzt(u_ip, u_sip, HOSTLEN);
        else u_ip[0] = '\0';

	if (strlen(host)  > (size_t) HOSTLEN ||
            (name ? strlen(name) : 0) > (size_t) HOSTLEN)
		return (0);

	reply[0] = '\0';

	if (conf_target != NULL) {
		if ((conf_target->status == CONF_KILL) && conf_target->host && conf_target->name &&
		    (match(conf_target->host, u_ip) == 0 || match(conf_target->host, host) == 0) &&
		    (!name || match(conf_target->name, name) == 0)) {
			if (BadPtr(conf_target->passwd))
				tmp = conf_target;
			else if (is_comment(conf_target->passwd))
				tmp = conf_target;
			else if (check_time_interval(conf_target->passwd, reply))
				tmp = conf_target;
		}
	} else {
		for (tmp = conf; tmp; tmp = tmp->next)
			if ((tmp->status == CONF_KILL) && tmp->host && tmp->name &&
			    (match(tmp->host, u_ip) == 0 || match(tmp->host, host) == 0) &&
			    (!name || match(tmp->name, name) == 0)) {
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
			}
	}

	if (reply[0])
		sendto_one(cptr, reply,
			   me.name, ERR_YOUREBANNEDCREEP, cptr->name);
	else if (tmp) {
            if (BadPtr(tmp->passwd))
		sendto_one(cptr,
			   ":%s %d %s :*** You are not welcome on this server."
			   "  Email %s for more information.",
			   me.name, ERR_YOUREBANNEDCREEP, cptr->name, kline);
	    else {
#ifdef COMMENT_IS_FILE
                   m_killcomment(cptr,cptr->name, tmp->passwd);
#else
		   if (tmp->tmpconf == KLINE_AKILL)
			   sendto_one(cptr,
				      ":%s %d %s :*** %s",
				      me.name, ERR_YOUREBANNEDCREEP,
				      cptr->name, tmp->passwd);
		   else
			   sendto_one(cptr,
                         ":%s %d %s :*** You are not welcome on this server: "
			 "%s.  Email %s for more information.",
                         me.name, ERR_YOUREBANNEDCREEP, cptr->name,
                         tmp->passwd, kline);
#endif  /* COMMENT_IS_FILE */
	    }
	}

 	return (tmp ? -1 : 0);
}



aConfItem	*find_ahurt(aClient *cptr)
{
	char	*host, *name;
	aConfItem *tmp;

	if (!cptr->user)
		return NULL;

	host = cptr->sockhost;
	name = cptr->user->username;

	if (strlen(host)  > (size_t) HOSTLEN ||
            (name ? strlen(name) : 0) > (size_t) HOSTLEN)
		return (NULL);

	for (tmp = conf; tmp; tmp = tmp->next)
 		if ((tmp->status == CONF_AHURT) && tmp->host && tmp->name &&
		    (match(tmp->host, host) == 0) &&
 		    (!name || match(tmp->name, name) == 0))
                {
                        return tmp;
                }

 	return (NULL);
}


char    *find_sup_zap(aClient *cptr, int dokillmsg)
{
	char *retval = "Reason Unspecified";
	char u_ip[HOSTLEN + 25], *u_sip;
	static char supbuf[BUFSIZE];
	/* int m = 0;*/
	aConfItem* node;

	supbuf[0] = '\0';

	if ((u_sip = address_tostring(cptr->sock->raddr, 0)))
		strncpyzt(u_ip, u_sip, HOSTLEN);
	else u_ip[0] = '\0';

	for(node = conf; node; node = node->next)
	{
		if (node->status != CONF_SUP_ZAP)
			continue;
		if (!BadPtr(node->host)) {
			if (match(node->host, u_ip) &&
			    match(node->host, cptr->sockhost))
				continue;
		}

		if (!BadPtr(node->name) && match(node->name, cptr->name)) {
			continue;
		}

		if (!BadPtr(node->string4)) {
			if (!cptr->user || match(node->string4, 
						cptr->user->username))
				continue;
		}
		
		if (!BadPtr(node->string5)) {
			if (match(node->string5, cptr->sup_host))
				continue;
		}

		if (!BadPtr(node->string6)) {
			if (match(node->string6, cptr->sup_server))
				continue;
		}
		
		if (!BadPtr(node->string7)) {
			if (match(node->string7, cptr->info))
				continue;
		}

		break;
	}

	if ( ! node )
		return 0;

	if ( node->passwd && node->passwd[0] && node->passwd[1] )
		retval = node->passwd;


	if ( dokillmsg ) {
                sendto_one(cptr,
                        ":%s %d %s :*** You are not welcome on this server: "
                        "%s.  Email %s for more information.",
                        me.name, ERR_YOUREBANNEDCREEP, cptr->name,
                        retval, kline);
        }
        else {
                sprintf(supbuf,
                        "ERROR :Closing Link: [%s] (You are not welcome on "
                        "this server: %s.  Email %s for more"
                        " information.)\r\n", address_tostring(cptr->sock->raddr, 0),
                        retval, kline);
                retval = supbuf;
        }
	return retval;
}


char *find_zap(aClient *cptr, int dokillmsg)  
{
	aConfItem *tmp;
	char *retval = NULL;
	for (tmp = conf; tmp; tmp = tmp->next)
		if ((tmp->status == CONF_ZAP) && tmp->host &&
			!match(tmp->host, address_tostring(cptr->sock->raddr, 0)))
			{
				retval = (tmp->passwd) ? tmp->passwd :
					"Reason unspecified";
				break;
			}
	if (dokillmsg && retval)
		sendto_one(cptr,
			":%s %d %s :*** You are not welcome on this server: "
			"%s.  Email %s for more information.",
			me.name, ERR_YOUREBANNEDCREEP, cptr->name,
			retval, kline);
	if (!dokillmsg && retval)
	{
		sprintf(zlinebuf,
			"ERROR :Closing Link: [%s] (You are not welcome on "
			"this server: %s.  Email %s for more"
			" information.)\r\n", address_tostring(cptr->sock->raddr, 0),
			retval, kline);
		retval = zlinebuf;
	}
	return retval;
}

int	find_kill_byname(char *host, char *name)
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

/*
**  output the reason for being k lined from a file  - Mmmm
** sptr is server    
** parv is the sender prefix
** filename is the file that is to be output to the K lined client
*/
int     m_killcomment(aClient *sptr, char *parv, char *filename)
{
      int     fd;
      char    line[80];
      char     *tmp;
      struct  stat    sb;
      struct  tm      *tm;

      /*
       * stop NFS hangs...most systems should be able to open a file in
       * 3 seconds. -avalon (curtesy of wumpus)
       */
      alarm(3);
      fd = open(filename, O_RDONLY);
      alarm(0);
      if (fd == -1)
          {
              sendto_one(sptr, err_str(ERR_NOMOTD), me.name, parv);
              sendto_one(sptr,
                         ":%s %d %s :*** You are not welcome to this server.",
                         me.name, ERR_YOUREBANNEDCREEP, parv);
              return 0;
          }
      fstat(fd, &sb);
      tm = localtime(&sb.st_mtime);
      dgets(-1, NULL, 0); /* make sure buffer is at empty pos */
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
      dgets(-1, NULL, 0); /* make sure buffer is at empty pos */
      close(fd);
      return 0;
  }


/*
** is the K line field an interval or a comment? - Mmmm
*/

static int is_comment(char *comment)
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

static	int	check_time_interval(char *interval, char *reply)
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
			sprintf(reply,
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
			sprintf(reply, ":%%s %%d %%s :%d minute%s%s",
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
int     m_rakill(aClient *cptr, aClient *sptr, int parc, char *parv[])
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
        }

	return 0;
}
/* ** m_akill;
**	parv[0] = sender prefix
**	parv[1] = hostmask
**	parv[2] = username
**	parv[3] = comment
*/
int	m_akill(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aConfItem *temp_conf = NULL;

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
	 		temp_conf = add_temp_conf(CONF_KILL, parv[1], parv[3], parv[2], 0, 0, 2); 
#else
			temp_conf = add_temp_conf(CONF_KILL, parv[1], NULL, parv[2], 0, 0, 2); 
#endif
		}
		if(parv[3])
			sendto_serv_butone(cptr, ":%s AKILL %s %s :%s", parv[0],
				     parv[1], parv[2], parv[3]);
		else
			sendto_serv_butone(cptr, ":%s AKILL %s %s", parv[0],
				     parv[1], parv[2]);
		check_pings(NOW, 1, temp_conf);
	}

	return 0;
}


/*
** m_rahurt;
**      parv[0] = sender prefix
**      parv[1] = hostmask
**      parv[2] = username
**      parv[3] = comment
*/
int     m_rahurt(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
        if (check_registered(sptr))
                return 0;

        if (parc < 3)
            {
                sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
                           me.name, parv[0], "RAHURT");
                return 0;
            }

           if (!IsULine(sptr, sptr)) 
               return 0;

                	del_temp_conf(CONF_AHURT, parv[1], NULL, 
				parv[2], 0, 0, 1); 

                if(parv[3])
                        sendto_serv_butone(cptr, ":%s RAHURT %s %s :%s", 
				parv[0], parv[1], parv[2], parv[3]);
                else
                        sendto_serv_butone(cptr, ":%s RAHURT %s %s", 
				parv[0], parv[1], parv[2]);
		return 0;
}


/*
** m_ahurt
**	parv[0] = sender prefix
**	parv[1] = hostmask
**	parv[2] = username
**	parv[3] = comment
*/
int	m_ahurt(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	if (check_registered(sptr))
		return 0;
	if (parc < 3)
	    {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "AHURT");
		return 0;
	    }

        if (!IsULine(cptr, sptr))
                 return 0;
		add_temp_conf(CONF_AHURT, parv[1], parv[3], parv[2], 0, 0, KLINE_AKILL); 
		if(parv[3])
			sendto_serv_butone(cptr, ":%s AHURT %s %s :%s", parv[0],
				     parv[1], parv[2], parv[3]);
		else
			sendto_serv_butone(cptr, ":%s AHURT %s %s", parv[0],
				     parv[1], parv[2]);
		return 0;
}


/*
** m_kline;
**	parv[0] = sender prefix
**	parv[1] = nickname
**	parv[2] = comment or filename
*/
int	m_kline(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	char *host, *tmp, *hosttemp;
	char uhost[80], name[80];
	int ip1, ip2, ip3, temp;
	aClient *acptr;
	aConfItem *temp_conf;

        *uhost = *name = (char)0;
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
	if ((hosttemp = (char *)strchr(parv[1], '@')))
	{
		temp = 0;
		while (temp <= 20)
			name[temp++] = 0;
		strncpy(uhost, ++hosttemp, sizeof(uhost) - strlen(uhost));
		uhost[sizeof(uhost)-1] = 0;
		strncpy(name, parv[1], ((hosttemp-parv[1]) > sizeof(name) ? sizeof(name) :(hosttemp-parv[1])) );
		name[((hosttemp-parv[1] > sizeof(name)) ? (sizeof(name)-1) : hosttemp-1-parv[1])]=0;
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

		strncpy(name, acptr->user->username, sizeof(name));
		name[sizeof(name)-1]=0;

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

		strncpy(uhost, host, sizeof(uhost) - strlen(uhost));
		uhost[sizeof(uhost)-1] = 0;
		if (isdigit(host[strlen(host)-1])) {
			if (sscanf(host, "%d.%d.%d.%*d", &ip1, &ip2, &ip3))
				sprintf(uhost, "%d.%d.%d.*", ip1, ip2, ip3);
		}
		else if (sscanf(host, "%*[^.].%*[^.].%s", uhost)) { /* Not really... */
			tmp = (char*)strchr(host, '.');
			sprintf(uhost, "*%s", tmp);
		}
	}

	sendto_ops("%s added a temp k:line for %s@%s %s", parv[0], name, uhost, parv[2] ? parv[2] : "");
 	temp_conf = add_temp_conf(CONF_KILL, uhost, parv[2], name, 0, 0, 1);
	check_pings(NOW, 1, temp_conf);
        return 0;
    }
	

/*
 *  m_unkline
 *    parv[0] = sender prefix
 *    parv[1] = userhost
 */

int m_unkline(aClient *cptr, aClient *sptr, int parc, char *parv[])
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
        if ((hosttemp = (char *)strchr(parv[1], '@')))
        {
                temp = 0;
                while (temp <= 20)
                        name[temp++] = 0;
                strncpy(host, ++hosttemp, sizeof(host));
                strncpy(name, parv[1], hosttemp-1-parv[1]);
		name[sizeof(name)-1]=0;
                host[sizeof(host)-1]=0;
                if (name[0] == '\0' || host[0] == '\0')
                {
                        Debug((DEBUG_INFO, "UNKLINE: Bad field"));
			sendto_one(sptr, "NOTICE %s : Both user and host fields must be non-null", parv[0]);
			return 0;
		}
		result = del_temp_conf(CONF_KILL, host, NULL, name, 
			0, 0, 0);
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
        return 0;
}

  /******************************************************************************
  ***       tempzline mods   : allow opers/servers to (safely) set and remove ***
  ***                          temporary Z-lines                              ***
  ***   Client Usage: /zline <nick/ip> <reason>                               ***
  ***   Services Usage: /zline <nick> [onwhatserver] <reason>                 ***
  ***                                                                         ***
  ***                                                     -- Mysidia          ***
  ******************************************************************************/

/*
 *  m_zline                       add a temporary zap line
 *    parv[0] = sender prefix
 *    parv[1] = host
 *    parv[2] = reason
 */
int m_zline(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	char userhost[512+2]="", *in;
	int uline=0, propo=0;
	char *reason, *mask, *server, *person;
	aClient *acptr;
	
	reason=mask=server=person=NULL;
	
	reason = ((parc>=3) ? parv[parc-1] : "Reason unspecified");
	mask   = ((parc>=2) ? parv[parc-2] : NULL);
	server   = ((parc>=4) ? parv[parc-1] : NULL);

	if (parc == 4)
	{
	      mask = parv[parc-3];
	      server = parv[parc-2];
	      reason = parv[parc-1];
	}

	uline = IsULine(cptr, sptr) ? 1 : 0;

	if (!uline && ( !MyConnect(sptr) || !OPCanZline(sptr) || !IsOper(sptr)))
	{
	  sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
	  return -1;
	}

	if (uline)
	{
	  if (parc>=4 && server)
	  {
	    if (hunt_server(cptr, sptr, ":%s ZLINE %s %s :%s", 2, parc, parv)
                != HUNTED_ISME)
	      return 0;
	    else    ;
	  }
	  else propo=1;
	}

	if (parc < 2)
	{
	  sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
	             me.name, parv[0], "ZLINE");
	  return -1;
	}

	if ((acptr = find_client(parv[1], NULL)))
	{
	  strcpy(userhost, address_tostring(acptr->sock->raddr, 0));
	  person = &acptr->name[0];
	  acptr = NULL;
	}
     /* z-lines don't support user@host format, they only 
        work with ip addresses and nicks */
	else
	if ((in = index(parv[1], '@')) && (*(in+1)!='\0'))
	{
	  strcpy(userhost, in+1);
	  in = &userhost[0];
	  while(*in) 
	  { 
	    if (!isxdigit(*in) && !ispunct(*in)) 
	    {
	      sendto_one(sptr, ":%s NOTICE %s :z:lines work only with ip addresses (you cannot specify ident either)", me.name, sptr->name);
	      return 0;
	    }
	    in++;
	    }
	  } else if (in && !(*(in+1))) /* sheesh not only specifying a ident@, but
                                   omitting the ip...?*/
	  {
	    sendto_one(sptr, ":%s NOTICE %s :Hey! z:lines need an ip address...",
                       me.name, sptr->name);
	    return -1;
	  }
	else
	{
	  strcpy(userhost, parv[1]);
	  in = &userhost[0];
	  while(*in) 
	  { 
	    if (!isxdigit(*in) && !ispunct(*in)) 
	    {
	       sendto_one(sptr, ":%s NOTICE %s :z:lines work only with ip addresses (you cannot specify ident either)", me.name, sptr->name);
	       return 0;
	    }
	    in++;
	  }
	}
    
	   /* this'll protect against z-lining *.* or something */
	if (banmask_check(userhost, TRUE) == FALSE)
	{ 
	  sendto_ops("Bad z:line mask from %s *@%s [%s]", parv[0], userhost, reason?reason:"");
	  if (MyClient(sptr))
	  sendto_one(sptr, ":%s NOTICE %s :*@%s is a bad z:line mask...", me.name, sptr->name, userhost);
	  return 0;
        }

	if (uline == 0)
	{
	  if (person)
	    sendto_ops("%s added a temp z:line for %s (*@%s) [%s]", parv[0], person, userhost, reason?reason:"");
	  else
	    sendto_ops("%s added a temp z:line *@%s [%s]", parv[0], userhost, reason?reason:"");
	  add_temp_conf(CONF_ZAP, userhost,  reason, NULL, 0, 0, KLINE_TEMP); 
        }
	else
	 {
	 if (person)
	   sendto_ops("%s z:lined %s (*@%s) on %s [%s]", parv[0], person, userhost, server?server:network , reason?reason:"");
	 else
	   sendto_ops("%s z:lined *@%s on %s [%s]", parv[0], userhost, server?server:network , reason?reason:"");
	 add_temp_conf(CONF_ZAP, userhost,  reason, NULL, 0, 0, KLINE_AKILL); 
        }

                                           /* something's wrong if i'm
                                              zapping the command source... */
       if (find_zap(cptr, 0)||find_zap(sptr, 0))
       {
             sendto_failops("z:line error: mask=%s parsed=%s I tried to zap cptr", mask, userhost);
             sendto_serv_butone(NULL,":%s GLOBOPS :z:line error: mask=%s parsed=%s I tried to zap cptr", me.name, mask, userhost);
             flush_connections(&me);
             rehash(&me, &me, 0);
             return 0;
       }

	for (acptr = &me; acptr; acptr = acptr->lnext)
	{
	  if (IsLog(acptr) || IsMe(acptr));
	     continue;
	  if (  find_zap(acptr, 1) )
          {
	    if (!IsServer(acptr))
	    {
               sendto_one(sptr,":%s NOTICE %s :*** %s %s",
                          me.name, sptr->name, 
	                  IsPerson(acptr)?"exiting":"closing", 
	                  acptr->name[0]?acptr->name:"<unknown>");
               exit_client(acptr, acptr, acptr, "z-lined");
	    }
	    else
	    {
	      sendto_one(sptr, ":%s NOTICE %s :*** exiting %s",
	                 me.name, sptr->name, acptr->name);
	      sendto_ops("dropping server %s (z-lined)", acptr->name);
	      sendto_serv_butone(cptr, "GNOTICE :dropping server %s (z-lined)",
	                         acptr->name);
	      exit_client(acptr, acptr, acptr, "z-lined");

	    }
	  }
	}

       if (propo==1)      /* propo is if a ulined server is propagating a z-line
                             this should go after the above check */
            sendto_serv_butone(cptr, ":%s ZLINE %s :%s", parv[0], parv[1], reason?reason:"");

        check_pings(time(NULL), 1, NULL);
        return 0;
}


/*
 *  m_unzline                        remove a temporary zap line
 *    parv[0] = sender prefix
 *    parv[1] = host
 */

int m_unzline(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
   char userhost[512+2]="", *in;
   int result=0, uline=0, akill=0;
   char *mask = NULL, *server = NULL;

   uline = IsULine(cptr, sptr)? 1 : 0;

   if (parc < 2)
   {
      sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
                 me.name, parv[0], "UNZLINE");
                 return -1;
   }


  if (parc < 3 || !uline)
  {
      mask   = parv[parc-1];
      server = NULL;
  }
  else if (parc == 3)
  {
      mask   = parv[parc-2];
      server = parv[parc-1];
  }
 
    if (!uline && (!MyConnect(sptr) || !OPCanZline(sptr) || !IsOper(sptr)))
    {
        sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
        return -1;
    }
 
    /* before we even check ourselves we need to do the uline checks
       because we aren't supposed to add a z:line if the message is
       destined to be passed on...*/
 
    if (uline)
    {
      if (parc == 3 && server)
      {
         if (hunt_server(cptr, sptr, ":%s UNZLINE %s %s", 2, parc, parv) != HUNTED_ISME)
                    return 0;
         else    ;
      }
       else
             sendto_serv_butone(cptr, ":%s UNZLINE %s", parv[0], parv[1]);
 
    }
 
 
    /* parse the removal mask the same way so an oper can just use
       the same thing to remove it if they specified *@ or something... */
    if ((in = index(parv[1], '@')))
    {
        strcpy(userhost, in+1);
        in = &userhost[0];
        while(*in) 
        { 
            if (!isxdigit(*in) && !ispunct(*in)) 
            {
               sendto_one(sptr, ":%s NOTICE %s :it's not possible to have a z:line that's not an ip addresss...", me.name, sptr->name);
               return 0;
           }
            in++;
       }
   }
   else
   {
       strcpy(userhost, parv[1]);
       in = &userhost[0];
       while(*in) 
       { 
           if (!isxdigit(*in) && !ispunct(*in)) 
           {
              sendto_one(sptr, ":%s NOTICE %s :it's not possible to have a z:line that's not an ip addresss...", me.name, sptr->name);
              return 0;
           }
            in++;
       }
   }

       akill = 0;
retry_unzline:

        if (uline == 0)
        {
           result = del_temp_conf(CONF_ZAP, userhost,  NULL, NULL, 0, 0, akill);
	  if ((result) == KLINE_RET_DELOK)
          {
   	      sendto_one(sptr,":%s NOTICE %s :temp z:line *@%s removed", me.name, parv[0], userhost);
   	      sendto_ops("%s removed temp z:line *@%s", parv[0], userhost);
          }
          else if (result == KLINE_RET_PERM)
              sendto_one(sptr, ":%s NOTICE %s :You may not remove permanent z:lines talk to your admin...", me.name, sptr->name);

/*          else if (result == KLINE_RET_AKILL && !(ClientFlags(sptr) & FLAGS_SADMIN))
          {
              sendto_one(sptr, ":%s NOTICE %s :You may not remove z:lines placed by services...", me.name, sptr->name);
          }*/
          else if (result == KLINE_RET_AKILL && !akill)
          {
             akill=1;
             goto retry_unzline;
          }
          else
              sendto_one(sptr, ":%s NOTICE %s :Couldn't find/remove zline for *@%s", me.name, sptr->name, userhost);

        }
        else    
        {      /* services did it, services should be able to remove
                  both types...;> */
	  if (del_temp_conf(CONF_ZAP, userhost,  NULL, NULL, 0, 0, 1) == KLINE_RET_DELOK||
              del_temp_conf(CONF_ZAP, userhost,  NULL, NULL, 0, 0, 0) == KLINE_RET_DELOK)
          {
              if (MyClient(sptr))
   	      sendto_one(sptr,"NOTICE %s :temp z:line *@%s removed", parv[0], userhost);
   	      sendto_ops("%s removed temp z:line *@%s", parv[0], userhost);
          }
          else
              sendto_one(sptr, ":%s NOTICE %s :Unable to find z:line", me.name, sptr->name);
       }
       return 0;
}


/* ok, given a mask, our job is to determine
 * wether or not it's a safe mask to kline, zline, or
 * otherwise banish...
 *
 * userhost= mask to verify
 * ipstat= TRUE  == it's an ip
 *         FALSE == it's a hostname
 *         UNSURE == we need to find out
 * return value
 *         TRUE  == mask is ok
 *         FALSE == mask is not ok
 *        UNSURE == [unused] something went wrong
 */
static int
banmask_check(char *userhost, int ipstat)
{
   int	retval = TRUE;
   char	*up = NULL, *p, *thisseg;
   int	numdots=0, segno=0, numseg, i=0;
   int  ipv6=FALSE;
   char	*ipseg[HOSTLEN+1];
   char	safebuffer[512]=""; /* buffer strtoken() can mess up to its heart's content...;>*/

  if (strlen(userhost) > HOSTLEN)
	  return 0;
  strcpy(safebuffer, userhost);

#define userhost safebuffer
#define IP_WILDS_OK(x) ((x)<2? 0 : 1)

  if (strchr(safebuffer, ':')) {
	  if (ipstat == FALSE)
		  return FALSE;
	  ipv6 = TRUE;
  }

   if (ipstat == UNSURE)
   {
        ipstat=TRUE;
        for (up = userhost;*up;up++) 
        {
           if (*up=='.'||*up==':') numdots++;
           if (!isxdigit(*up) && !ispunct(*up)) {
		   ipstat=FALSE; 
		   continue;
	   }
        }
        if (numdots != 3) ipstat=FALSE;
        if (numdots < 1 || numdots > 9)  return(0);
   }

     /* fill in the array elements with the corresponding ip segments */
  {
     int l = 0;


        for (segno = 0, i = 0, thisseg = strtok_r(userhost, (ipv6 ? ":" : "."), &p); thisseg;
             thisseg = strtok_r(NULL, (ipv6 ? "." : ":"), &p), i++)
        {
            
            l = strlen(thisseg)+2;
            ipseg[segno] = irc_malloc(l+1);
            if (!ipseg[segno]) {
                 sendto_realops("[***] PANIC! UNABLE TO ALLOCATE MEMORY (banmask_check)"); 
                 for (l = 0; l < segno ; l++) free(ipseg[segno]);
                 server_reboot("UNABLE TO ALLOCATE MEMORY (banmask_check)");
                 return 0;
           }
            strncpy(ipseg[segno], thisseg, l);
            ipseg[segno++][l] = 0;
        }
  }
     if (segno < 2 && ipstat==TRUE) retval = FALSE;  
     numseg = segno;
     if (ipstat==TRUE)
      for(i=0;i<numseg;i++)
      {
            if (!IP_WILDS_OK(i) && (index(ipseg[i], '*')||index(ipseg[i], '?')))
               retval=FALSE;            
            irc_free(ipseg[i]);
      }
     else
     {
      int wildsok=0;

      for(i=0;i<numseg;i++)
      {
             /* for hosts, let the mask extend all the way to 
                the second-level domain... */
           wildsok=1;
          if (i==numseg||(i+1)==numseg) wildsok=0;
           if (wildsok == 0 && (index(ipseg[i], '*')||index(ipseg[i], '?')))
           {
             retval=FALSE;
           }
            irc_free(ipseg[i]);
      }
     }
     return(retval);
#undef userhost
#undef IP_WILDS_OK
}
