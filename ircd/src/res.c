/*
 * ircd/res.c (C)opyright 1992, 1993, 1994 Darren Reed. All rights reserved.
 * This file may not be distributed without the author's prior permission in
 * any shape or form. The author takes no responsibility for any damage or
 * loss of property which results from the use of this software.  Distribution
 * of this file must include this notice.
 */
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "res.h"
#include "numeric.h"
#include "h.h"

#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <signal.h>

#include "nameser.h"
#include "resolv.h"

#include "ircd/send.h"
#include "ircd/string.h"
#include "ircd/res.h"

IRCD_SCCSID("@(#)res.c	2.38 4/13/94 (C) 1992 Darren Reed");
IRCD_RCSID("$Id$");

//#undef	DEBUG	/* because there is a lot of debug code in here :-) */
#define DEBUG

extern	int	dn_expand(char *, char *, char *, char *, int);
extern	int	dn_skipname(char *, char *);
extern	int	res_mkquery(int, char *, int, int, char *, int,
				   struct rrec *, char *, int);

extern	int	errno, h_errno;
extern	int	highest_fd;
extern	aClient	*local[];

static	char	hostbuf[HOSTLEN+64]; /* must be at least 73 bytes for IPv6 addresses --Onno */
static	char	dot[] = ".";
static	int	incache = 0;
static	CacheTable	hashtable[ARES_CACSIZE];
static	aCache	*cachetop = NULL;
static	ResRQ	*last, *first;

static	void	rem_cache(aCache *);
static	void	rem_request(ResRQ *);
static	int	do_query_name(Link *, char *, int, ResRQ *);
static	int	do_query_number(Link *, anAddress *, ResRQ *);
static	void	resend_query(ResRQ *);
static	int	proc_answer(ResRQ *, HEADER *, char *, char *);
static	int	query_name(char *, int, int, ResRQ *);
static	aCache	*make_cache(ResRQ *);
static	aCache	*find_cache_name(char *);
static	aCache	*find_cache_number(ResRQ *, anAddress *);
static	int	add_request(ResRQ *);
static	ResRQ	*make_request(Link *);
static	int	send_res_msg(char *, int, int);
static	ResRQ	*find_id(int);
static	int	hash_number(anAddress *);
static	void	update_list(ResRQ *, aCache *);
static	int	hash_name(char *);

static	struct cacheinfo {
	int	ca_adds;
	int	ca_dels;
	int	ca_expires;
	int	ca_lookups;
	int	ca_na_hits;
	int	ca_nu_hits;
	int	ca_updates;
} cainfo;

static	struct	resinfo {
	int	re_errors;
	int	re_nu_look;
	int	re_na_look;
	int	re_replies;
	int	re_requests;
	int	re_resends;
	int	re_sent;
	int	re_timeouts;
	int	re_shortttl;
	int	re_unkrep;
} reinfo;

int
init_resolver(int op)
{
	int ret = 0;

#ifdef	LRAND48
	srand48(NOW);
#endif
	if (op & RES_INITLIST) {
		bzero((char *)&reinfo, sizeof(reinfo));
		first = last = NULL;
	}
	if (op & RES_CALLINIT) {
		ret = res_init();
		if (!_res.nscount) {
			_res.nscount = 1;
			_res.nsaddr_list[0].sin_addr.s_addr =
				inet_addr("127.0.0.1");
		}
	}

	if (op & RES_INITSOCK) {
        	int on = 0;
		ret = resfd = socket(AF_INET, SOCK_DGRAM, 0);
		set_non_blocking(resfd, &me);
                (void) setsockopt(ret, SOL_SOCKET, SO_BROADCAST,
				  (char *)&on, sizeof(on));
        }
#ifdef DEBUG
	if (op & RES_INITDEBG);
	_res.options |= RES_DEBUG;
#endif
	if (op & RES_INITCACH) {
		bzero((char *)&cainfo, sizeof(cainfo));
		bzero((char *)hashtable, sizeof(hashtable));
	}
	if (op == 0)
		ret = resfd;

	return ret;
}

static	int	add_request(new)
ResRQ *new;
{
	if (!new)
		return -1;
	if (!first)
		first = last = new;
	else
	    {
		last->next = new;
		last = new;
	    }
	new->next = NULL;
	reinfo.re_requests++;
	return 0;
}

/*
 * remove a request from the list. This must also free any memory that has
 * been allocated for temporary storage of DNS results.
 */
static	void	rem_request(old)
ResRQ	*old;
{
	ResRQ	**rptr, *r2ptr = NULL;
	int	i;
	char	*s;

	if (!old)
		return;
	for (rptr = &first; *rptr; r2ptr = *rptr, rptr = &(*rptr)->next)
		if (*rptr == old)
		    {
			*rptr = old->next;
			if (last == old)
				last = r2ptr;
			break;
		    }
#ifdef	DEBUG
	Debug((DEBUG_INFO,"rem_request:Remove %#x at %#x %#x",
		old, *rptr, r2ptr));
#endif
	r2ptr = old;
	if (r2ptr->he.h_name)
		irc_free(r2ptr->he.h_name);
	for (i = 0; i < MAXALIASES; i++)
		if ((s = r2ptr->he.h_aliases[i]))
			irc_free(s);
	if (r2ptr->name)
		irc_free(r2ptr->name);
	irc_free(r2ptr);

	return;
}

/*
 * Create a DNS request record for the server.
 */
static	ResRQ	*make_request(lp)
Link	*lp;
{
	ResRQ	*nreq;

	nreq = irc_malloc(sizeof(ResRQ));
	bzero((char *)nreq, sizeof(ResRQ));
	nreq->next = NULL; /* where NULL is non-zero ;) */
	nreq->sentat = NOW;
	nreq->retries = 3;
	nreq->resend = 1;
	nreq->srch = -1;
	if (lp)
		bcopy((char *)lp, (char *)&nreq->cinfo, sizeof(Link));
	else
		bzero((char *)&nreq->cinfo, sizeof(Link));
	nreq->timeout = 4;	/* start at 4 and exponential inc. */
	nreq->he.h_name = NULL;
	nreq->he.h_aliases[0] = NULL;

	(void)add_request(nreq);
	return nreq;
}

/*
 * Remove queries from the list which have been there too long without
 * being resolved.
 */
time_t	timeout_query_list(now)
time_t	now;
{
	ResRQ	*rptr, *r2ptr;
	time_t	next = 0, tout;
	aClient	*cptr;

	Debug((DEBUG_DNS,"timeout_query_list at %s",myctime(now)));
	for (rptr = first; rptr; rptr = r2ptr)
	    {
		r2ptr = rptr->next;
		tout = rptr->sentat + rptr->timeout;
		if (now >= tout) {
			if (--rptr->retries <= 0) {
#ifdef DEBUG
				Debug((DEBUG_ERROR,"timeout %x now %d cptr %x",
					rptr, now, rptr->cinfo.value.cptr));
#endif
				reinfo.re_timeouts++;
				cptr = rptr->cinfo.value.cptr;
				switch (rptr->cinfo.flags) {
				case ASYNC_CLIENT :
                                        connotice(cptr, REPORT_FAIL_DNS);
					ClearDNS(cptr);
					SetAccess(cptr);
					break;
				case ASYNC_SERVER :
					sendto_ops("Host %s unknown", rptr->name);
					ClearDNS(cptr);
					if (check_server(cptr, NULL, NULL, NULL, 1))
					  (void)exit_client(cptr, cptr, &me, "No Permission");
					break;
				case ASYNC_CONNECT :
					sendto_ops("Host %s unknown", rptr->name);
					break;
				}
				rem_request(rptr);
				continue;
			} else {
				rptr->sentat = now;
				rptr->timeout += rptr->timeout;
 				resend_query(rptr);
				tout = now + rptr->timeout;
#ifdef DEBUG
				Debug((DEBUG_INFO,"r %x now %d retry %d c %x",
					rptr, now, rptr->retries,
					rptr->cinfo.value.cptr));
#endif
			}
		}
		if (!next || tout < next)
			next = tout;
	    }
	Debug((DEBUG_DNS,"Next timeout_query_list() at %s, %d",
	    myctime((next > now) ? next : (now + AR_TTL)),
	    (next > now) ? (next - now) : AR_TTL));
	return (next > now) ? next : (now + AR_TTL);
}

/*
 * del_queries - called by the server to cleanup outstanding queries for
 * which there no longer exist clients or conf lines.
 */
void	del_queries(cp)
char	*cp;
{
	ResRQ	*rptr, *r2ptr;

	for (rptr = first; rptr; rptr = r2ptr)
	    {
		r2ptr = rptr->next;
		if (cp == rptr->cinfo.value.cp)
			rem_request(rptr);
	    }
}

/*
 * sends msg to all nameservers found in the "_res" structure.
 * This should reflect /etc/resolv.conf. We will get responses
 * which arent needed but is easier than checking to see if nameserver
 * isnt present. Returns number of messages successfully sent to 
 * nameservers or -1 if no successful sends.
 */
static	int	send_res_msg(msg, len, rcount)
char	*msg;
int	len, rcount;
{
	int	i;
	int	sent = 0, max;

	if (!msg)
		return -1;

	max = MIN(_res.nscount, rcount);
	if (_res.options & RES_PRIMARY)
		max = 1;
	if (!max)
		max = 1;

	for (i = 0; i < max; i++)
	    {
		_res.nsaddr_list[i].sin_family = AF_INET;
		if (sendto(resfd, msg, len, 0, (struct sockaddr *)
			&(_res.nsaddr_list[i]), sizeof(struct sockaddr)) == len)
		    {
			reinfo.re_sent++;
			sent++;
		    }
		else
			Debug((DEBUG_ERROR,"s_r_m:sendto: %d on %d",
				errno, resfd));
	    }

	return (sent) ? sent : -1;
}

/*
 * find a dns request id (id is determined by dn_mkquery)
 */
static	ResRQ	*find_id(id)
int	id;
{
	ResRQ	*rptr;

	for (rptr = first; rptr; rptr = rptr->next)
		if (rptr->id == id)
			return rptr;
	return NULL;
}

struct	HostEnt	*gethost_byname(name, af, lp)
char	*name;
int	af;
Link	*lp;
{
	aCache	*cp;

	reinfo.re_na_look++;
	if ((cp = find_cache_name(name)))
		return (struct HostEnt *)&(cp->he);
	if (!lp)
		return NULL;
	(void)do_query_name(lp, name, af, NULL);
	return NULL;
}

struct	HostEnt	*gethost_byaddr(addr, lp)
anAddress	*addr;
Link	*lp;
{
	aCache	*cp;

	reinfo.re_nu_look++;
	if ((cp = find_cache_number(NULL, addr)))
		return (struct HostEnt *)&(cp->he);
	if (!lp)
		return NULL;
	(void)do_query_number(lp, addr, NULL);
	return NULL;
}

static	int	do_query_name(lp, name, af, rptr)
Link	*lp;
char	*name;
int	af;
ResRQ	*rptr;
{
	char	hname[HOSTLEN+1];
	int	len;

	(void)strncpy(hname, name, sizeof(hname) - 1);
	hname[sizeof(hname) - 1] = '\0';

	len = strlen(hname);

	if (rptr && !index(hname, '.') && _res.options & RES_DEFNAMES)
	    {
		(void)strncat(hname, dot, sizeof(hname) - len - 1);
		len++;
		(void)strncat(hname, _res.defdname, sizeof(hname) - len -1);
	    }
	/*
	 * Store the name passed as the one to lookup and generate other host
	 * names to pass onto the nameserver(s) for lookups.
	 */
	if (!rptr)
	    {
		rptr = make_request(lp);
		switch (af)
		{
			case AF_INET:
				rptr->type = T_A;
				break;
			case AF_INET6:
				rptr->type = T_AAAA;
				break;
		}
		rptr->name = (char *)irc_malloc(strlen(name) + 1);
		(void)strcpy(rptr->name, name);
	    }
	switch (af)
	{
		default:
		case AF_INET:
			return (query_name(hname, C_IN, T_A, rptr));
		case AF_INET6:
			return (query_name(hname, C_IN, T_AAAA, rptr));
	}
}

/*
 * Use this to do reverse IP# lookups.
 */
static	int	do_query_number(lp, numb, rptr)
Link	*lp;
anAddress	*numb;
ResRQ	*rptr;
{
	char	ipbuf[73];
	u_char	*cp, *cp2;

	switch (numb->addr_family)
	{
		case AF_INET:
			cp = (u_char *)&numb->in.sin_addr.s_addr;
			(void)sprintf(ipbuf,"%u.%u.%u.%u.in-addr.arpa.",
				(u_int)(cp[3]), (u_int)(cp[2]),
				(u_int)(cp[1]), (u_int)(cp[0]));
			break;
		case AF_INET6:
			cp = (u_char *)&numb->in6.sin6_addr.s6_addr[15];
			cp2 = (u_char *)ipbuf;
			while (cp >= &numb->in6.sin6_addr.s6_addr[0])
			{
				sprintf(cp2, "%x.%x.",(*cp & 0xf),(*cp >> 4));
				cp--;
				cp2++; cp2++; cp2++; cp2++;
			}
			sprintf(cp2,"ip6.int.");
			break;
	}

	if (!rptr)
	    {
		rptr = make_request(lp);
		rptr->type = T_PTR;

		bcopy((char *)numb, (char *)&rptr->addr, sizeof(anAddress));
		bcopy((char *)numb,
			(char *)&rptr->he.h_addr, sizeof(anAddress));
	    }
	return (query_name(ipbuf, C_IN, T_PTR, rptr));
}

/*
 * generate a query based on class, type and name.
 */
static	int	query_name(name, class, type, rptr)
char	*name;
int	class, type;
ResRQ	*rptr;
{
	struct	timeval	tv;
	char	buf[MAXPACKET];
	int	r,s,k = 0;
	HEADER	*hptr;

        Debug((DEBUG_DNS,"query_name: na %s cl %d ty %d", name, class, type));
	bzero(buf, sizeof(buf));
	r = res_mkquery(QUERY, name, class, type, NULL, 0, NULL,
			buf, sizeof(buf));
	if (r <= 0)
	    {
		h_errno = NO_RECOVERY;
		return r;
	    }
	hptr = (HEADER *)buf;
#ifdef LRAND48
        do {
		hptr->id = htons(ntohs(hptr->id) + k + lrand48() & 0xffff);
#else
	(void) gettimeofday(&tv, NULL);
	do {
		/* htons/ntohs can be assembler macros, which cannot
		   be nested. Thus two lines.	-Vesa		    */
		u_short nstmp = ntohs(hptr->id) + k +
				(u_short)(tv.tv_usec & 0xffff);
		hptr->id = htons(nstmp);
#endif /* LRAND48 */
		k++;
	} while (find_id(ntohs(hptr->id)));
	rptr->id = ntohs(hptr->id);
	rptr->sends++;
	s = send_res_msg(buf, r, rptr->sends);
	if (s == -1)
	    {
		h_errno = TRY_AGAIN;
		return -1;
	    }
	else
		rptr->sent += s;
	return 0;
}

static	void	resend_query(rptr)
ResRQ	*rptr;
{
	if (rptr->resend == 0)
		return;
	reinfo.re_resends++;
	switch(rptr->type)
	{
	case T_PTR:
		(void)do_query_number(NULL, &rptr->addr, rptr);
		break;
	case T_A:
		(void)do_query_name(NULL, rptr->name, AF_INET, rptr);
		break;
	case T_AAAA:
		(void)do_query_name(NULL, rptr->name, AF_INET6, rptr);
	default:
		break;
	}
	return;
}

/*
 * process name server reply.
 */
static	int	proc_answer(rptr, hptr, buf, eob)
ResRQ	*rptr;
char	*buf, *eob;
HEADER	*hptr;
{
	char	*cp, **alias;
	struct	hent	*hp;
	int	class, type, dlen, len, ans = 0, n;
	anAddress	dr, *adr;

	cp = buf + sizeof(HEADER);
	hp = (struct hent *)&(rptr->he);
	adr = &hp->h_addr;
	while (adr->addr_family)
		adr++;
	alias = hp->h_aliases;
	while (*alias)
		alias++;
#ifdef	SOL20		/* brain damaged compiler (Solaris2) it seems */
	for (; hptr->qdcount > 0; hptr->qdcount--)
#else
	while (hptr->qdcount-- > 0)
#endif
		if ((n = dn_skipname(cp, eob)) == -1)
			break;
		else
			cp += (n + QFIXEDSZ);
	/*
	 * proccess each answer sent to us blech.
	 */
	while (hptr->ancount-- > 0 && cp && cp < eob) {
		n = dn_expand(buf, eob, cp, hostbuf, sizeof(hostbuf));
		if (n <= 0)
			break;

		cp += n;
		type = (int)_getshort(cp);
		cp += sizeof(short);
		class = (int)_getshort(cp);
		cp += sizeof(short);
		rptr->ttl = _getlong(cp);
		cp += sizeof(rptr->ttl);
		dlen =  (int)_getshort(cp);
		cp += sizeof(short);
		/* rptr->type = type; */

		len = strlen(hostbuf);
		/* name server never returns with trailing '.' */
		if (!index(hostbuf,'.') && (_res.options & RES_DEFNAMES))
		    {
			(void)strcat(hostbuf, dot);
			len++;
			(void)strncat(hostbuf, _res.defdname,
				sizeof(hostbuf) - 1 - len);
			len = MIN(len + strlen(_res.defdname),
				  sizeof(hostbuf) - 1);
		    }

		switch(type)
		{
		case T_A :
                                 /* from Christophe Kalt <kalt@stealth.net> */
                                 if (dlen != sizeof(struct in_addr)) {
                                                sendto_realops("Bad IP length (%d) returned for %s",
                                                               dlen, hostbuf);
                                                Debug((DEBUG_DNS, "Bad IP length (%d) returned for %s", dlen, hostbuf));
                                                return(-2);
                                 }
			memcpy((char *)&dr.in.sin_addr, cp, sizeof(struct in_addr));
			adr->in.sin_addr = dr.in.sin_addr;
			dr.addr_family = AF_INET;
			adr->addr_family = AF_INET;
			Debug((DEBUG_INFO,"got ip # %s for %s",
				inetntoa(adr), hostbuf));
			if (!hp->h_name)
			    {
				hp->h_name =(char *)irc_malloc(len+1);
				(void)strcpy(hp->h_name, hostbuf);
			    }
			ans++;
			adr++;
			cp += dlen;
 			break;
		case T_AAAA:
                                 /* from Christophe Kalt <kalt@stealth.net> */
                                 if (dlen != sizeof(struct in6_addr)) {
                                                sendto_realops("Bad IP length (%d) returned for %s",
                                                               dlen, hostbuf);
                                                Debug((DEBUG_DNS, "Bad IP length (%d) returned for %s", dlen, hostbuf));
                                                return(-2);
                                 }
			memcpy((char *)&dr.in6.sin6_addr, cp, sizeof(struct in6_addr));
			memcpy((char *)&adr->in6.sin6_addr, cp, sizeof(struct in6_addr));
			dr.addr_family = AF_INET6;
			adr->addr_family = AF_INET6;
			Debug((DEBUG_INFO,"got ip # %s for %s",
				inetntoa(adr), hostbuf));
			if (!hp->h_name)
			    {
				hp->h_name =irc_malloc(len+1);
				(void)strcpy(hp->h_name, hostbuf);
			    }
			ans++;
			adr++;
			cp += dlen;
 			break;
		case T_PTR :
			if((n = dn_expand(buf, eob, cp, hostbuf,
					  sizeof(hostbuf) )) < 0)
			    {
				cp = NULL;
				break;
			    }
			cp += n;
			len = strlen(hostbuf);
			Debug((DEBUG_INFO,"got host %s",hostbuf));
			/*
			 * copy the returned hostname into the host name
			 * or alias field if there is a known hostname
			 * already.
			 */
			if (hp->h_name)
			    {
				if (alias >= &(hp->h_aliases[MAXALIASES-1]))
					break;
				*alias = irc_malloc(len + 1);
				(void)strcpy(*alias++, hostbuf);
				*alias = NULL;
			    }
			else
			    {
				hp->h_name = irc_malloc(len + 1);
				(void)strcpy(hp->h_name, hostbuf);
			    }
			ans++;
			break;
		case T_CNAME :
			cp += dlen;
			Debug((DEBUG_INFO,"got cname %s", hostbuf));
			if (alias >= &(hp->h_aliases[MAXALIASES-1]))
				break;
			*alias = irc_malloc(len + 1);
			(void)strcpy(*alias++, hostbuf);
			*alias = NULL;
			ans++;
			break;
		default :
#ifdef DEBUG
			Debug((DEBUG_INFO,"proc_answer: type:%d for:%s",
			      type, hostbuf));
#endif
			break;
		}
	}
	return ans;
}



/*
 * read a dns reply from the nameserver and process it.
 * UNIX version.
 */
struct	HostEnt	*get_res(lp)
char	*lp;
{
	static	char	buf[sizeof(HEADER) + MAXPACKET];
	HEADER	*hptr;
	struct	sockaddr_in	sin;
	int	rc, a, len = sizeof(sin), max;
	ResRQ	*rptr = NULL;
	aCache	*cp = NULL;

	(void)alarm((unsigned)4);
	rc = recvfrom(resfd, buf, sizeof(buf), 0, (struct sockaddr *)&sin,
		      &len);
	(void)alarm((unsigned)0);
	if (rc == -1 || rc <= sizeof(HEADER))
		goto getres_err;
	/*
	 * convert DNS reply reader from Network byte order to CPU byte order.
	 */
	hptr = (HEADER *)buf;
	hptr->id = ntohs(hptr->id);
	hptr->ancount = ntohs(hptr->ancount);
	hptr->qdcount = ntohs(hptr->qdcount);
	hptr->nscount = ntohs(hptr->nscount);
	hptr->arcount = ntohs(hptr->arcount);
#ifdef	DEBUG
	Debug((DEBUG_NOTICE, "get_res:id = %d rcode = %d ancount = %d",
		hptr->id, hptr->rcode, hptr->ancount));
#endif

	reinfo.re_replies++;
	/*
	 * response for an id which we have already received an answer for
	 * just ignore this response.
	 */

	rptr = find_id(hptr->id);
	if (!rptr)
		goto getres_err;

	/*
	 * check against possibly fake replies
	 */
	max = MIN(_res.nscount, rptr->sends);
	if (!max)
		max = 1;

	for (a = 0; a < max; a++)
		if (!_res.nsaddr_list[a].sin_addr.s_addr ||
		    !bcmp((char *)&sin.sin_addr,
			  (char *)&_res.nsaddr_list[a].sin_addr,
			  sizeof(struct in_addr)))
			break;
	if (a == max)
	    {
		reinfo.re_unkrep++;
		goto getres_err;
	    }

	if ((hptr->rcode != NOERROR) || (hptr->ancount == 0))
	    {
		switch (hptr->rcode)
		{
		case NXDOMAIN:
			h_errno = TRY_AGAIN;
			break;
		case SERVFAIL:
			h_errno = TRY_AGAIN;
			break;
		case NOERROR:
			h_errno = NO_DATA;
			break;
		case FORMERR:
		case NOTIMP:
		case REFUSED:
		default:
			h_errno = NO_RECOVERY;
			break;
		}
		reinfo.re_errors++;
		/*
		** If a bad error was returned, we stop here and dont send
		** send any more (no retries granted).
		*/
		if (h_errno != TRY_AGAIN)
		    {
			Debug((DEBUG_DNS, "Fatal DNS error %d for %d",
				h_errno, hptr->rcode));
			rptr->resend = 0;
			rptr->retries = 0;
		    }
		goto getres_err;
	    }
#ifdef DEBUGMODE
	Debug((DEBUG_INFO, "get_res: proc_answer [pre] rptr->type = %d",
		rptr->type));
#endif
	a = proc_answer(rptr, hptr, buf, buf+rc);
#ifdef DEBUGMODE
	Debug((DEBUG_INFO,"get_res:Proc answer = %d, rptr->type = %d",a, rptr->type));
#endif
	if (a && rptr->type == T_PTR)
	    {
		struct	HostEnt	*hp2 = NULL;

                if (rptr->he.h_name == NULL)
                    goto getres_err;

		Debug((DEBUG_DNS, "relookup %s <-> %s",
			rptr->he.h_name, inetntoa(&rptr->he.h_addr)));
		/*
		 * Lookup the 'authoritive' name that we were given for the
		 * ip#.  By using this call rather than regenerating the
		 * type we automatically gain the use of the cache with no
		 * extra kludges.
		 */
		if ((hp2 = gethost_byname(rptr->he.h_name, rptr->addr.addr_family, &rptr->cinfo)))
			if (lp)
				bcopy((char *)&rptr->cinfo, lp, sizeof(Link));
		/*
		 * If name wasn't found, a request has been queued and it will
		 * be the last one queued.  This is rather nasty way to keep
		 * a host alias with the query. -avalon
		 */
		if (!hp2 && rptr->he.h_aliases[0])
			for (a = 0; rptr->he.h_aliases[a]; a++)
			    {
				Debug((DEBUG_DNS, "Copied CNAME %s for %s",
					rptr->he.h_aliases[a],
					rptr->he.h_name));
				last->he.h_aliases[a] = rptr->he.h_aliases[a];
				rptr->he.h_aliases[a] = NULL;
			    }

		rem_request(rptr);
		return hp2;
	    }

	if (a > 0)
	    {
		if (lp)
			bcopy((char *)&rptr->cinfo, lp, sizeof(Link));
		cp = make_cache(rptr);
#ifdef	DEBUG
	Debug((DEBUG_INFO,"get_res:cp=%#x rptr=%#x (made)",cp,rptr));
#endif

		rem_request(rptr);
	    }
	else
		if (!rptr->sent)
			rem_request(rptr);
	return cp ? (struct HostEnt *)&cp->he : NULL;

getres_err:
	/*
	 * Reprocess an error if the nameserver didnt tell us to "TRY_AGAIN".
	 */
	if (rptr)
	    {
		if (h_errno != TRY_AGAIN)
		    {
			/*
			 * If we havent tried with the default domain and its
			 * set, then give it a try next.
			 */
			if (_res.options & RES_DEFNAMES && ++rptr->srch == 0)
			    {
				rptr->retries = _res.retry;
				rptr->sends = 0;
				rptr->resend = 1;
				resend_query(rptr);
			    }
			else
				resend_query(rptr);
		    }
		else if (lp)
			bcopy((char *)&rptr->cinfo, lp, sizeof(Link));
	    }

	return (struct HostEnt *)NULL;
}

static	int	hash_number(ip)
anAddress *ip;
{
	char *p4;
	int *p6;
	u_int	hashv = 0;

	switch (ip->addr_family)
	{
		case AF_INET:
			/* could use loop but slower */
			p4 = (char *)&ip->in.sin_addr;
			hashv = (int)*p4++;
			hashv += hashv + (int)*p4++;
			hashv += hashv + (int)*p4++;
			hashv += hashv + (int)*p4;
			break;
		case AF_INET6:
			p6 = (int *)&ip->in6.sin6_addr;
			hashv = *p6++;
			hashv += hashv + *p6++;
			hashv += hashv + *p6++;
			hashv += hashv + *p6;
			break;
	}
	hashv %= ARES_CACSIZE;
	return (hashv);
}

static	int	hash_name(name)
char	*name;
{
	u_int	hashv = 0;

	for (; *name && *name != '.'; name++)
		hashv += *name;
	hashv %= ARES_CACSIZE;
	return (hashv);
}

/*
** Add a new cache item to the queue and hash table.
*/
static	aCache	*add_to_cache(ocp)
aCache	*ocp;
{
	aCache	*cp = NULL;
        int	hashv;

#ifdef DEBUG
	Debug((DEBUG_INFO,
	      "add_to_cache:ocp %#x he %#x name %#x addrl %#x 0 %#x",
		ocp, &ocp->he, ocp->he.h_name, ocp->he.h_addr_list,
		ocp->he.h_addr_list[0]));
#endif
	ocp->list_next = cachetop;
	cachetop = ocp;

	hashv = hash_name(ocp->he.h_name);
	ocp->hname_next = hashtable[hashv].name_list;
	hashtable[hashv].name_list = ocp;

	hashv = hash_number((anAddress *)ocp->he.h_addr);
	ocp->hnum_next = hashtable[hashv].num_list;
	hashtable[hashv].num_list = ocp;

#ifdef	DEBUG
	Debug((DEBUG_INFO, "add_to_cache:added %s[%08x] cache %#x.",
		ocp->he.h_name, ocp->he.h_addr_list[0], ocp));
# endif
 	Debug((DEBUG_INFO,
 		"add_to_cache:h1 %d h2 %x lnext %#x namnext %#x numnext %#x",
		hash_name(ocp->he.h_name), hashv, ocp->list_next,
		ocp->hname_next, ocp->hnum_next));

	/*
	 * LRU deletion of excessive cache entries.
	 */
	if (++incache > MAXCACHED)
	    {
		for (cp = cachetop; cp->list_next; cp = cp->list_next)
			;
		rem_cache(cp);
	    }
	cainfo.ca_adds++;

	return ocp;
}

/*
** update_list does not alter the cache structure passed. It is assumed that
** it already contains the correct expire time, if it is a new entry. Old
** entries have the expirey time updated.
*/
static	void	update_list(rptr, cachep)
ResRQ	*rptr;
aCache	*cachep;
{
        aCache	**cpp, *cp = cachep;
	char	*s, *t, **base;
	int	i, j;
	int	addrcount;

	/*
	** search for the new cache item in the cache list by hostname.
	** If found, move the entry to the top of the list and return.
	*/
	cainfo.ca_updates++;

	for (cpp = &cachetop; *cpp; cpp = &((*cpp)->list_next))
		if (cp == *cpp)
			break;
	if (!*cpp)
		return;
	*cpp = cp->list_next;
	cp->list_next = cachetop;
	cachetop = cp;
	if (!rptr)
		return;

#ifdef	DEBUG
	Debug((DEBUG_DEBUG,"u_l:cp %#x na %#x al %#x ad %#x",
		cp,cp->he.h_name,cp->he.h_aliases,cp->he.h_addr));
	Debug((DEBUG_DEBUG,"u_l:rptr %#x h_n %#x", rptr, rptr->he.h_name));
#endif
	/*
	 * Compare the cache entry against the new record.  Add any
	 * previously missing names for this entry.
	 */
	for (i = 0; cp->he.h_aliases[i]; i++)
		;
	addrcount = i;
	for (i = 0, s = rptr->he.h_name; s && i < MAXALIASES;
	     s = rptr->he.h_aliases[i++])
	    {
		for (j = 0, t = cp->he.h_name; t && j < MAXALIASES;
		     t = cp->he.h_aliases[j++])
			if (!mycmp(t, s))
				break;
		if (!t && j < MAXALIASES-1)
		    {
			base = cp->he.h_aliases;

			addrcount++;
			base = irc_realloc(base,
					   sizeof(char *) * (addrcount + 1));
			cp->he.h_aliases = base;
#ifdef	DEBUG
			Debug((DEBUG_DNS,"u_l:add name %s hal %x ac %d",
				s, cp->he.h_aliases, addrcount));
#endif
			base[addrcount-1] = s;
			base[addrcount] = NULL;
			if (i)
				rptr->he.h_aliases[i-1] = NULL;
			else
				rptr->he.h_name = NULL;
		    }
	    }
	for (i = 0; cp->he.h_addr_list[i]; i++)
		;
	addrcount = i;

	/*
	 * Do the same again for IP#'s.
	 */
	for (s = (char *)&rptr->he.h_addr;
	     ((anAddress *)s)->addr_family; s += sizeof(anAddress))
	    {
		for (i = 0; (t = (char *)cp->he.h_addr_list[i]); i++)
			if (!addr_cmp((anAddress *) s, (anAddress *) t))
				break;
		if (i >= MAXADDRS || addrcount >= MAXADDRS)
			break;
		/*
		 * Oh man this is bad...I *HATE* it. -avalon
		 *
		 * Whats it do ?  Reallocate two arrays, one of pointers
		 * to "char *" and the other of IP addresses.  Contents of
		 * the IP array *MUST* be preserved and the pointers into
		 * it recalculated.
		 */
		if (!t)
		    {
			base = (char **)cp->he.h_addr_list;
			addrcount++;

			t = (char *)irc_realloc(*base,
					addrcount * sizeof(anAddress));
			base = (char **)irc_realloc((char *)base,
					(addrcount + 1) * sizeof(char *));
			cp->he.h_addr_list = (anAddress**) base;
#ifdef	DEBUG
			Debug((DEBUG_DNS,"u_l:add IP %x hal %x ac %d",
				ntohl(((struct in_addr *)s)->s_addr),
				cp->he.h_addr_list,
				addrcount));
#endif
			for (; addrcount; addrcount--)
			    {
				*base++ = t;
				t += sizeof(anAddress);
			    }
			*base = NULL;
			bcopy(s, *--base, sizeof(anAddress));
		    }
	    }
	return;
}

static	aCache	*find_cache_name(name)
char	*name;
{
	aCache	*cp;
	char	*s;
	int	hashv, i;

	hashv = hash_name(name);

	cp = hashtable[hashv].name_list;
#ifdef	DEBUG
	Debug((DEBUG_DNS,"find_cache_name:find %s : hashv = %d",name,hashv));
#endif

	for (; cp; cp = cp->hname_next)
		for (i = 0, s = cp->he.h_name; s; s = cp->he.h_aliases[i++])
			if (mycmp(s, name) == 0)
			    {
				cainfo.ca_na_hits++;
				update_list(NULL, cp);
				return cp;
			    }

	for (cp = cachetop; cp; cp = cp->list_next)
	    {
		/*
		 * if no aliases or the hash value matches, we've already
		 * done this entry and all possiblilities concerning it.
		 */
		if (!*cp->he.h_aliases)
			continue;
		if (hashv == hash_name(cp->he.h_name))
			continue;
		for (i = 0, s = cp->he.h_aliases[i]; s && i < MAXALIASES; i++)
			if (!mycmp(name, s)) {
				cainfo.ca_na_hits++;
				update_list(NULL, cp);
				return cp;
			    }
	    }
	return NULL;
}

/*
 * find a cache entry by ip# and update its expire time
 */
static	aCache	*find_cache_number(rptr, numb)
ResRQ	*rptr;
anAddress	*numb;
{
	aCache	*cp;
	int	hashv,i;
#ifdef	DEBUG
	anAddress	*ip = numb;
#endif

	hashv = hash_number(numb);

	cp = hashtable[hashv].num_list;
#ifdef DEBUG
	Debug((DEBUG_DNS,"find_cache_number:find %s: hashv = %d",
		inetntoa(numb), hashv));
#endif

	for (; cp; cp = cp->hnum_next)
		for (i = 0; cp->he.h_addr_list[i]; i++)
			if (!addr_cmp(cp->he.h_addr_list[i], numb))
			    {
				cainfo.ca_nu_hits++;
				update_list(rptr, cp);
				return cp;
			    }

	for (cp = cachetop; cp; cp = cp->list_next)
	    {
		/*
		 * single address entry...would have been done by hashed
		 * search above...
		 */
		if (!cp->he.h_addr_list[1])
			continue;
		/*
		 * if the first IP# has the same hashnumber as the IP# we
		 * are looking for, its been done already.
		 */
		if (hashv == hash_number(cp->he.h_addr_list[0]))
			continue;
		for (i = 1; cp->he.h_addr_list[i]; i++)
			if (!bcmp(cp->he.h_addr_list[i], numb,
				  sizeof(anAddress)))
			    {
				cainfo.ca_nu_hits++;
				update_list(rptr, cp);
				return cp;
			    }
	    }
	return NULL;
}

static	aCache	*make_cache(rptr)
ResRQ	*rptr;
{
	aCache	*cp;
	int	i, n;
	struct	HostEnt	*hp;
	char	*s, **t;

	/*
	** shouldn't happen but it just might...
	*/
	if (!rptr->he.h_name || !rptr->he.h_addr.addr_family)
		return NULL;
	/*
	** Make cache entry.  First check to see if the cache already exists
	** and if so, return a pointer to it.
	*/
	if ((cp = find_cache_number(rptr, &rptr->he.h_addr)))
		return cp;
	for (i = 1; rptr->he.h_addr_list[i].addr_family; i++)
		if ((cp = find_cache_number(rptr,
				&(rptr->he.h_addr_list[i]))))
			return cp;

	/*
	** a matching entry wasnt found in the cache so go and make one up.
	*/
	cp = (aCache *)irc_malloc(sizeof(aCache));

	bzero((char *)cp, sizeof(aCache));
	hp = &cp->he;
	for (i = 0; i < MAXADDRS; i++)
		if (!rptr->he.h_addr_list[i].addr_family)
			break;

	/*
	** build two arrays, one for IP#'s, another of pointers to them.
	*/
	t = (char **)hp->h_addr_list = (char **)irc_malloc(sizeof(anAddress *) * (i+1));
	bzero((char *)t, sizeof(anAddress *) * (i+1));

	s = (char *)irc_malloc(sizeof(anAddress) * i);
	bzero(s, sizeof(anAddress) * i);

	for (n = 0; n < i; n++, s += sizeof(anAddress))
	    {
		*t++ = s;
		bcopy(&rptr->he.h_addr_list[n], s, sizeof(anAddress));
	    }
	*t = (char *)NULL;

	/*
	** an array of pointers to CNAMEs.
	*/
	for (i = 0; i < MAXALIASES; i++)
		if (!rptr->he.h_aliases[i])
			break;
	i++;
	t = hp->h_aliases = irc_malloc(sizeof(char *) * i);
	for (n = 0; n < i; n++, t++)
	    {
		*t = rptr->he.h_aliases[n];
		rptr->he.h_aliases[n] = NULL;
	    }

	hp->h_name = rptr->he.h_name;
	if (rptr->ttl < 600)
	    {
		reinfo.re_shortttl++;
		cp->ttl = 600;
	    }
	else
		cp->ttl = rptr->ttl;
	cp->expireat = NOW + cp->ttl;
	rptr->he.h_name = NULL;
#ifdef DEBUG
	Debug((DEBUG_INFO,"make_cache:made cache %#x", cp));
#endif
	return add_to_cache(cp);
}

/*
 * rem_cache
 *     delete a cache entry from the cache structures and lists and return
 *     all memory used for the cache back to the memory pool.
 */
static	void	rem_cache(ocp)
aCache	*ocp;
{
	aCache	**cp;
	struct	HostEnt *hp = &ocp->he;
	int	hashv;
	aClient	*cptr;

#ifdef	DEBUG
	Debug((DEBUG_DNS, "rem_cache: ocp %#x hp %#x l_n %#x aliases %#x",
		ocp, hp, ocp->list_next, hp->h_aliases));
#endif
	/*
	** Cleanup any references to this structure by destroying the
	** pointer.
	*/
	for (hashv = highest_fd; hashv >= 0; hashv--)
		if ((cptr = local[hashv]) && (cptr->hostp == hp))
			cptr->hostp = NULL;
	/*
	 * remove cache entry from linked list
	 */
	for (cp = &cachetop; *cp; cp = &((*cp)->list_next))
		if (*cp == ocp)
		    {
			*cp = ocp->list_next;
			break;
		    }
	/*
	 * remove cache entry from hashed name lists
	 */
	hashv = hash_name(hp->h_name);
#ifdef	DEBUG
	Debug((DEBUG_DEBUG,"rem_cache: h_name %s hashv %d next %#x first %#x",
		hp->h_name, hashv, ocp->hname_next,
		hashtable[hashv].name_list));
#endif
	for (cp = &hashtable[hashv].name_list; *cp; cp = &((*cp)->hname_next))
		if (*cp == ocp)
		    {
			*cp = ocp->hname_next;
			break;
		    }
	/*
	 * remove cache entry from hashed number list
	 */
	hashv = hash_number(hp->h_addr);
#ifdef	DEBUG
	Debug((DEBUG_DEBUG,"rem_cache: h_addr %s hashv %d next %#x first %#x",
		inetntoa(hp->h_addr), hashv, ocp->hnum_next,
		hashtable[hashv].num_list));
#endif
	for (cp = &hashtable[hashv].num_list; *cp; cp = &((*cp)->hnum_next))
		if (*cp == ocp)
		    {
			*cp = ocp->hnum_next;
			break;
		    }

	/*
	 * free memory used to hold the various host names and the array
	 * of alias pointers.
	 */
	if (hp->h_name)
		irc_free(hp->h_name);
	if (hp->h_aliases)
	    {
		for (hashv = 0; hp->h_aliases[hashv]; hashv++)
			irc_free(hp->h_aliases[hashv]);
		irc_free(hp->h_aliases);
	    }

	/*
	 * free memory used to hold ip numbers and the array of them.
	 */
	if (hp->h_addr_list)
	    {
		if (*hp->h_addr_list)
			irc_free(*hp->h_addr_list);
		irc_free(hp->h_addr_list);
	    }
	irc_free(ocp);

	incache--;
	cainfo.ca_dels++;

	return;
}

/*
 * removes entries from the cache which are older than their expirey times.
 * returns the time at which the server should next poll the cache.
 */
time_t	expire_cache(now)
time_t	now;
{
	aCache	*cp, *cp2;
	time_t	next = 0;

	for (cp = cachetop; cp; cp = cp2)
	    {
		cp2 = cp->list_next;

		if (now >= cp->expireat)
		    {
			cainfo.ca_expires++;
			rem_cache(cp);
		    }
		else if (!next || next > cp->expireat)
			next = cp->expireat;
	    }
	return (next > now) ? next : (now + AR_TTL);
}

/*
 * remove all dns cache entries.
 */
void	flush_cache()
{
	aCache	*cp;

	while ((cp = cachetop))
		rem_cache(cp);
}

int	m_dns(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
	aCache	*cp;
	int	i;

	if (!IsAnOper(sptr)) {
	    sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
	    return 0;
	}

	if (parv[1] && *parv[1] == 'l') {
		for(cp = cachetop; cp; cp = cp->list_next)
		    {
			sendto_one(sptr, "NOTICE %s :Ex %d ttl %d host %s(%s)",
				   parv[0], cp->expireat - NOW, cp->ttl,
				   cp->he.h_name, inetntoa(cp->he.h_addr));
			for (i = 0; cp->he.h_aliases[i]; i++)
				sendto_one(sptr,"NOTICE %s : %s = %s (CN)",
					   parv[0], cp->he.h_name,
					   cp->he.h_aliases[i]);
			for (i = 1; cp->he.h_addr_list[i]; i++)
				sendto_one(sptr,"NOTICE %s : %s = %s (IP)",
					   parv[0], cp->he.h_name,
					   inetntoa(cp->he.h_addr_list[i]));
		    }
		return 0;
	}
	sendto_one(sptr,"NOTICE %s :Ca %d Cd %d Ce %d Cl %d Ch %d:%d Cu %d",
		   sptr->name,
		   cainfo.ca_adds, cainfo.ca_dels, cainfo.ca_expires,
		   cainfo.ca_lookups,
		   cainfo.ca_na_hits, cainfo.ca_nu_hits, cainfo.ca_updates);

	sendto_one(sptr,"NOTICE %s :Re %d Rl %d/%d Rp %d Rq %d",
		   sptr->name, reinfo.re_errors, reinfo.re_nu_look,
		   reinfo.re_na_look, reinfo.re_replies, reinfo.re_requests);
	sendto_one(sptr,"NOTICE %s :Ru %d Rsh %d Rs %d(%d) Rt %d", sptr->name,
		   reinfo.re_unkrep, reinfo.re_shortttl, reinfo.re_sent,
		   reinfo.re_resends, reinfo.re_timeouts);
	return 0;
}

u_long	cres_mem(sptr)
aClient	*sptr;
{
	aCache	*c = cachetop;
	struct	HostEnt	*h;
	int	i;
	u_long	nm = 0, im = 0, sm = 0, ts = 0;

	for ( ;c ; c = c->list_next)
	    {
		sm += sizeof(*c);
		h = &c->he;
		for (i = 0; h->h_addr_list[i]; i++)
		    {
			im += sizeof(char *);
			im += sizeof(anAddress);
		    }
		im += sizeof(char *);
		for (i = 0; h->h_aliases[i]; i++)
		    {
			nm += sizeof(char *);
			nm += strlen(h->h_aliases[i]);
		    }
		nm += i - 1;
		nm += sizeof(char *);
		if (h->h_name)
			nm += strlen(h->h_name);
	    }
	ts = ARES_CACSIZE * sizeof(CacheTable);
	sendto_one(sptr, ":%s %d %s :RES table %d",
		   me.name, RPL_STATSDEBUG, sptr->name, ts);
	sendto_one(sptr, ":%s %d %s :Structs %d IP storage %d Name storage %d",
		   me.name, RPL_STATSDEBUG, sptr->name, sm, im, nm);
	return ts + sm + im + nm;
}

