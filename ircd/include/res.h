/*
 * irc2.7.2/ircd/res.h (C)opyright 1992 Darren Reed.
 */
#ifndef	__res_include__
#define	__res_include__

#define	RES_INITLIST	1
#define	RES_CALLINIT	2
#define RES_INITSOCK	4
#define RES_INITDEBG	8
#define RES_INITCACH    16

#define MAXPACKET	1024
#define MAXALIASES	35
#define MAXADDRS	35

#define	AR_TTL		600	/* TTL in seconds for dns cache entries */

struct	hent {
	char	*h_name;	/* official name of host */
	char	*h_aliases[MAXALIASES];	/* alias list */
	/* list of addresses from name server */
	sock_address	h_addr_list[MAXADDRS];
#define	h_addr	h_addr_list[0]	/* address, for backward compatiblity */
};

typedef	struct	reslist {
	int	id;
	int	sent;	/* number of requests sent */
	int	srch;
#ifdef  __alpha
        u_int   ttl;                  /* time to live */
#else
        u_long  ttl;                  /* time to live */
#endif
	char	type;
	char	retries; /* retry counter */
	char	sends;	/* number of sends (>1 means resent) */
	char	resend;	/* send flag. 0 == dont resend */
	time_t	sentat;
	time_t	timeout;
	sock_address	addr;
	char	*name;
	struct	reslist	*next;
	Link	cinfo;
	struct	hent he;
	} ResRQ;

typedef	struct	cache {
	time_t	expireat;
	time_t	ttl;
	struct	HostEnt	he;
	struct	cache	*hname_next, *hnum_next, *list_next;
	} aCache;

typedef struct	cachetable {
	aCache	*num_list;
	aCache	*name_list;
	} CacheTable;

#define ARES_CACSIZE	101

#define	MAXCACHED	81

#endif /* __res_include__ */
