/************************************************************************
 *   IRC - Internet Relay Chat, include/struct.h
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

#ifndef	__struct_include__
#define __struct_include__

#include "config.h"
#include "common.h"
#include "sys.h"

#include <sys/socket.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#if !defined(va_copy) && defined(SOL20)
#define va_copy(dst, src) ((dst) = (src))
#endif

#include "snprintf.h"

#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#ifdef STDDEFH
# include <stddef.h>
#endif

#ifdef USE_SYSLOG
# include <syslog.h>
# ifdef SYSSYSLOGH
#  include <sys/syslog.h>
# endif
#endif

typedef	union	Address	anAddress;
typedef	struct	ConfItem aConfItem;
typedef	struct 	Client	aClient;
typedef	struct	Channel	aChannel;
typedef	struct	User	anUser;
typedef	struct	Server	aServer;
typedef	struct	SLink	Link;
typedef	struct	SMode	Mode;
typedef struct	Watch	aWatch;
typedef struct  ListOptions     LOpts;

typedef struct  CloneItem aClone;

#ifdef NEED_U_INT32_T
typedef unsigned int  u_int32_t; /* XXX Hope this works! */
#endif

#ifndef VMSP
#include "class.h"
#include "dbuf.h"	/* THIS REALLY SHOULDN'T BE HERE!!! --msa */
#endif

/*#define NETWORK                 "SorceryNet"*/
/*#define NETWORK_KLINE_ADDRESS	"kline@sorcery.net"*/
#define DEBUG_CHAN "#debug"     /* channel to output fake directions to */

#define	HOSTLEN		63	/* Length of hostname.  Updated to         */
				/* comply with RFC1123                     */

#undef	KEEP_HURTBY            /* remember who hurt a user */
#undef NICKLEN_CHANGE          /* used for a network nickname length change
                                * to reduce nicklen:
                                * have all servers running with these
                                * two defined, then undef both and
                                * change nicklen to what 'newnicklen' was set 
                                * *NOTE* services must be prepared to
                                * accept the change 
                                * to raise nicklen:
                                * define these and set them to what nicklen
                                * is, raise the nicklen define itself
                                * have all servers do the same, then
                                * comment these two defines. */
/* #define NEWNICKLEN       17 */


#define	NICKLEN		17	/* maximum length of a nickname. */
#define	USERLEN		10
#define	REALLEN	 	50
#define	TOPICLEN	307
/* DAL MADE ME PUT THIS IN THE FIEND:  --Russell
 * This number will be expanded to 200 in the near future
 */
#define	CHANNELLEN	32
#define	PASSWDLEN 	20
#define	KEYLEN		23
#define	BUFSIZE		512		/* WARNING: *DONT* CHANGE THIS!!!! */
#define	MAXRECIPIENTS 	20
#define	MAXBANS		60
#define	MAXBANLENGTH	1024
#define	MAXSILES	5
#define	MAXSILELENGTH	128

#define	USERHOST_REPLYLEN	(NICKLEN+HOSTLEN+USERLEN+5)
#define MAXTIME			0x7FFFFFFF /* largest thing time() returns, when time() exceeds this,
                                           we're dead meat as far as hurt timing goes:/  */
#define HELPOP_CHAN     "#HelpOps"


/*
** 'offsetof' is defined in ANSI-C. The following definition
** is not absolutely portable (I have been told), but so far
** it has worked on all machines I have needed it. The type
** should be size_t but...  --msa
*/
#ifndef offsetof
#define	offsetof(t,m) (int)((&((t *)0L)->m))
#endif

#define	elementsof(x) (sizeof(x)/sizeof(x[0]))

/*
** flags for bootup options (command line flags)
*/
#define	BOOT_DEBUG	0x0000001
#define	BOOT_FORK	0x0000002

#define	STAT_AUTHSERV	-7	/* Server waiting identd check */
#define	STAT_LOG	-6	/* logfile for -x */
#define	STAT_MASTER	-5	/* Local ircd master before identification */
#define	STAT_CONNECTING	-4
#define	STAT_HANDSHAKE	-3
#define	STAT_ME		-2
#define	STAT_UNKNOWN	-1
#define	STAT_SERVER	0
#define	STAT_CLIENT	1
#define	STAT_SERVICE	2	/* Services not implemented yet */

/*
 * status macros.
 */
#define	IsRegisteredUser(x)	((x)->status == STAT_CLIENT)
#define	IsRegistered(x)		((x)->status >= STAT_SERVER)
#define	IsConnecting(x)		((x)->status == STAT_CONNECTING)
#define	IsHandshake(x)		((x)->status == STAT_HANDSHAKE)
#define	IsMe(x)			((x)->status == STAT_ME)
#define	IsUnknown(x)		((x)->status == STAT_UNKNOWN || \
				 (x)->status == STAT_MASTER)
#define	IsServer(x)		((x)->status == STAT_SERVER)
#define	IsClient(x)		((x)->status == STAT_CLIENT)
#define	IsAuthServ(x)		((x)->status == STAT_AUTHSERV)
#define	IsLog(x)		((x)->status == STAT_LOG)
#define	IsService(x)		((x)->status == STAT_SERVICE)

#define	SetMaster(x)		((x)->status = STAT_MASTER)
#define	SetConnecting(x)	((x)->status = STAT_CONNECTING)
#define	SetHandshake(x)		((x)->status = STAT_HANDSHAKE)
#define	SetMe(x)		((x)->status = STAT_ME)
#define	SetUnknown(x)		((x)->status = STAT_UNKNOWN)
#define	SetServer(x)		((x)->status = STAT_SERVER)
#define	SetClient(x)		((x)->status = STAT_CLIENT)
#define	SetAuthServ(x)		((x)->status = STAT_AUTHSERV)
#define	SetLog(x)		((x)->status = STAT_LOG)
#define	SetService(x)		((x)->status = STAT_SERVICE)

/* the lovely little bits used in flags  the defines are to shorten lengths
     BIT32 as a flag is simpler to allign than 0x10000000 */
#define BIT01		(1 << 0)
#define BIT02		(1 << 1)
#define BIT03		(1 << 2)
#define BIT04		(1 << 3)
#define BIT05		(1 << 4)
#define BIT06		(1 << 5)
#define BIT07		(1 << 6)
#define BIT08		(1 << 7)
#define BIT09		(1 << 8)
#define BIT10		(1 << 9)
#define BIT11		(1 << 10)
#define BIT12		(1 << 11)
#define BIT13		(1 << 12)
#define BIT14		(1 << 13)
#define BIT15		(1 << 14)
#define BIT16		(1 << 15)
#define BIT17		(1 << 16)
#define BIT18		(1 << 17)
#define BIT19		(1 << 18)
#define BIT20		(1 << 19)
#define BIT21		(1 << 20)
#define BIT22		(1 << 21)
#define BIT23		(1 << 22)
#define BIT24		(1 << 23)
#define BIT25		(1 << 24)
#define BIT26		(1 << 25)
#define BIT27		(1 << 26)
#define BIT28		(1 << 27)
#define BIT29		(1 << 28)
#define BIT30		(1 << 29)
#define BIT31		(1 << 30)
#define BIT32		(1 << 31)

/* general client flags */
#define	FLAGS_PINGSENT		BIT01 /* Unreplied ping was sent */
#define	FLAGS_DEADSOCKET	BIT02 /* Local socket is dead--Exiting soon */
#define	FLAGS_KILLED		BIT03 /* kill message sent, no QUIT needed */
#define	FLAGS_BLOCKED		BIT04 /* socket is in a blocked condition */
/* bit 5 unused */
#define	FLAGS_CLOSING		BIT06 /* set when closing to suppress errors */
#define	FLAGS_LISTEN		BIT07 /* used to mark clients which we listen() on */
#define	FLAGS_CHKACCESS		BIT08 /* ok to check clients access if set */
#define	FLAGS_DOINGDNS		BIT09 /* client is waiting for a DNS response */
#define	FLAGS_AUTH		BIT10 /* client is waiting on rfc931 auth/ident response */
#define	FLAGS_WRAUTH		BIT11 /* set if we havent written to ident server */
#define	FLAGS_LOCAL		BIT12 /* set for local clients */
#define	FLAGS_GOTID		BIT13 /* successful ident lookup achieved */
#define	FLAGS_DOID		BIT14 /* I-lines say must use ident return */
#define	FLAGS_NONL		BIT15 /* No \n in buffer */
#define FLAGS_TS8		BIT16 /* TS8 timestamping flag [who knows what it does] */
#define FLAGS_ULINE		BIT17 /* User/server is considered U-lined */
#define FLAGS_SQUIT		BIT18 /* Server has been /squit by an oper */
#define FLAGS_HURT		BIT19 /* if ->hurt is set, user is silenced */
/* bit20 unused */
#define FLAGS_GOT_VERSION	BIT21 /* Ctcp version reply received */
#define FLAGS_GOT_SPOOFCODE	BIT22 /* Is not spoof */
#define FLAGS_SENT_SPOOFCODE	BIT23

/* usermode flags
     NOTE: these are still held in sptr->flags */
#define U_FULLMASK	BIT18
#define U_MASK		BIT21 /* Masked */
#define	U_OPER		BIT22 /* Global IRCop */
#define	U_LOCOP		BIT23 /* Local IRCop */
#define	U_INVISIBLE	BIT24 /* Invisible user, not shown in user searches */
#define	U_WALLOP	BIT25 /* User has selected to receive wallops */
#define	U_SERVNOTICE	BIT26 /* User has selected to receive server notices */
#define U_FAILOP	BIT27 /* Shows GNOTICE messages */
#define U_HELPOP	BIT28 /* 'HelpOp' */
#define U_KILLS		BIT29 /* Show server-kills/nick collisions... */
#define U_CLIENT	BIT30 /* Show client information [connects/exits/errors] */
#define U_FLOOD		BIT31 /* Receive flood warnings */
#define U_LOG           BIT32 /* See network-level server logging */

/* list of usermodes that are propogated/passed on to other servers*/
#define	SEND_UMODES	(U_INVISIBLE|U_OPER|U_WALLOP|U_FAILOP|U_HELPOP|U_MASK)
/* list of all usermodes except those that are in send_umodes*/
#define	ALL_UMODES (SEND_UMODES|U_SERVNOTICE|U_LOCOP|U_KILLS|U_CLIENT|U_FLOOD|U_LOG)
#define	FLAGS_ID	(FLAGS_DOID|FLAGS_GOTID)

/*
 * Default mode(s) to set on new user connections.
 */
#define UFLAGS_DEFAULT	(U_MASK)

#define FLAGSET_FLOOD   (U_FLOOD)  /* what clients should flood notices be sent to ? */
#define FLAGSET_CLIENT	(U_CLIENT) /* what clients should client notices be sent to ? */

typedef	enum {
	LOG_OPER,
 	LOG_USER,
        LOG_NET,
	LOG_HI
} loglevel_value_t;

#define SET_BIT(x, y)		((x) |= (y))
#define REMOVE_BIT(x, y)	((x) &= ~(y))
#define IS_SET(x, y)		((x) & (y))
#define IS_FLAGGED(x, y)	((x)->flags & (y))
#define SET_FLAG(x, y)		((x)->flags |= (y))
#define REMOVE_FLAG(x, y)	((x)->flags &= ~(y))

/* flags macros. */
#define ClientUmode(x)		(x)->uflags
#define ClientFlags(x)		(x)->flags

#define	IsListening(x)		((x)->flags & FLAGS_LISTEN)
#define SetListening(x)		((x)->flags |= FLAGS_LISTEN)

#define	DoAccess(x)		((x)->flags & FLAGS_CHKACCESS)   /* Has he got "access"? */
#define	SetAccess(x)		((x)->flags |= FLAGS_CHKACCESS)  /* Give him access */
#define	ClearAccess(x)		((x)->flags &= ~FLAGS_CHKACCESS) /* Remove his access */

#define IsHurt(x)		((x)->flags & FLAGS_HURT)   /* Is he hurt? */
#define SetHurt(x)		((x)->flags |= FLAGS_HURT)  /* Hurt him! Hurt him! */
#define ClearHurt(x)		((x)->flags &= ~FLAGS_HURT) /* Ye Gods! He's healed! */
#define check_hurt(x)		((IsHurt((x)) && (x)->hurt && ((x)->hurt > 5) && ((x)->hurt < NOW)) ? (remove_hurt((x)) || 1) : 0)

#define	IsPerson(x)		((x)->user && IsClient(x))
#define	IsLocal(x)		((x)->flags & FLAGS_LOCAL)
#define	IsDead(x)		((x)->flags & FLAGS_DEADSOCKET)

#define	DoingDNS(x)		((x)->flags & FLAGS_DOINGDNS)   /* Are we doing a DNS check? */
#define	SetDNS(x)		((x)->flags |= FLAGS_DOINGDNS)  /* DNS check pending */
#define	ClearDNS(x)		((x)->flags &= ~FLAGS_DOINGDNS) /* Yay, finished the DNS check */

#define	DoingAuth(x)		((x)->flags & FLAGS_AUTH)
#define SetAuth(x)		((x)->flags |= FLAGS_AUTh)
#define	ClearAuth(x)		((x)->flags &= ~FLAGS_AUTH)

#define	NoNewLine(x)		((x)->flags & FLAGS_NONL)
#define	IsPrivileged(x)		(IsAnOper(x) || IsServer(x)) /* Can this client see cool messages? */
#define IsULine(cptr,sptr)      (ClientFlags(sptr) & FLAGS_ULINE)

/* usermode macros */
#define IsKillsF(x)		(ClientUmode(x) & U_KILLS)
#define SetKillsF(x)		(ClientUmode(x) |= U_KILLS)
#define ClearKillsF(x)		(ClientUmode(x) &= ~U_KILLS)

#define IsClientF(x)		(ClientUmode(x) & U_CLIENT)
#define SetClientF(x)		(ClientUmode(x) |= U_CLIENT)
#define ClearClientF(x)		(ClientUmode(x) &= ~U_CLIENT)

#define IsFloodF(x)		(ClientUmode(x) & U_FLOOD)
#define SetFloodF(x)		(ClientUmode(x) |= U_FLOOD)
#define ClearFloodF(x)		(ClientUmode(x) &= ~U_FLOOD)

#define IsHelpOp(x)		(ClientUmode(x) & U_HELPOP)
#define SetHelpOp(x)		(ClientUmode(x) |= U_HELPOP)
#define ClearHelpOp(x)		(ClientUmode(x) &= ~U_HELPOP)

#define SendFailops(x)		(ClientUmode(x) & U_FAILOP)
#define SetFailops(x)		(ClientUmode(x) & U_FAILOP)
#define ClearFailops(x)		(ClientUmode(x) &= ~U_FAILOP)

#define	IsOper(x)		(ClientUmode(x) & U_OPER)
#define	SetOper(x)		(ClientUmode(x) |= U_OPER)
#define	ClearOper(x)		(ClientUmode(x) &= ~U_OPER)

#define	IsAnOper(x)		(ClientUmode(x) & (U_OPER|U_LOCOP))

#define	IsLocOp(x)		(ClientUmode(x) & U_LOCOP)
#define	SetLocOp(x)    		(ClientUmode(x) |= U_LOCOP)
#define	ClearLocOp(x)		(ClientUmode(x) &= ~U_LOCOP)

#define	IsInvisible(x)		(ClientUmode(x) & U_INVISIBLE)
#define	SetInvisible(x)		(ClientUmode(x) |= U_INVISIBLE)
#define	ClearInvisible(x)	(ClientUmode(x) &= ~U_INVISIBLE)

#define	SendWallops(x)		(ClientUmode(x) & U_WALLOP)
#define	SetWallops(x)  		(ClientUmode(x) |= U_WALLOP)
#define	ClearWallops(x)		(ClientUmode(x) &= ~U_WALLOP)

#define	SendServNotice(x)	(ClientUmode(x) & U_SERVNOTICE)
#define SetServNotice(x)	(ClientUmode(x) |= U_SERVNOTICE)

#define IsLogMode(x)		(ClientUmode(x) & U_LOG)
#define SetLogMode(x)		(ClientUmode(x) |= U_LOG)
#define ClearLogMode(x)		(ClientUmode(x) &= ~U_LOG)

#define IsMasked(x)		(ClientUmode(x) & U_MASK)
#define SetMasked(x)		(ClientUmode(x) |= U_MASK)
#define ClearMasked(x)		(ClientUmode(x) &= ~U_MASK)
#define UGETHOST(s, x)		(((s)->user == (x) || !(x)->mask || ((s) && IsOper((s)))) ? (x)->host : (x)->mask)

#ifdef NOSPOOF
/*#define	IsNotSpoof(x)		((x)->nospoof == 0)*/
#define		IsNotSpoof(x)	((ClientFlags(x)) & FLAGS_GOT_SPOOFCODE)
#define		SetNotSpoof(x)	((ClientFlags(x)) |= FLAGS_GOT_SPOOFCODE)
#define		SetSentNoSpoof(x) ((ClientFlags(x)) |= FLAGS_SENT_SPOOFCODE)
#define         SentNoSpoof(x)  ((ClientFlags(x)) & FLAGS_SENT_SPOOFCODE)
#else
#define IsNotSpoof(x)           (1)
#define SetNotSpoof(x)		do {} while (0)
#endif

#define IsUserVersionKnown(x)	(ClientFlags(x) & FLAGS_GOT_VERSION)
#define SetUserVersionKnown(x)	(ClientFlags(x) |= FLAGS_GOT_VERSION)

/*
 * defined operator access levels
 */
#define OFLAG_REHASH	0x00000001  /* Oper can /rehash server */
#define OFLAG_DIE	0x00000002  /* Oper can /die the server */
#define OFLAG_RESTART	0x00000004  /* Oper can /restart the server */
#define OFLAG_HELPOP	0x00000010  /* Oper can send /HelpOps */
#define OFLAG_GLOBOP	0x00000020  /* Oper can send /GlobOps */
#define OFLAG_WALLOP	0x00000040  /* Oper can send /WallOps */
#define OFLAG_LOCOP	0x00000080  /* Oper can send /LocOps */
#define OFLAG_LROUTE	0x00000100  /* Oper can do local routing */
#define OFLAG_GROUTE	0x00000200  /* Oper can do global routing */
#define OFLAG_LKILL	0x00000400  /* Oper can do local kills */
#define OFLAG_GKILL	0x00000800  /* Oper can do global kills */
#define OFLAG_KLINE	0x00001000  /* Oper can /kline users */
#define OFLAG_UNKLINE	0x00002000  /* Oper can /unkline users */
#define OFLAG_LNOTICE	0x00004000  /* Oper can send local serv notices */
#define OFLAG_GNOTICE	0x00008000  /* Oper can send global notices */
#define OFLAG_ADMIN	0x00080000  /* Admin */
#define OFLAG_UMODEC	0x01000000  /* Oper can set umode +c */
#define OFLAG_UMODEF	0x02000000  /* Oper can set umode +f */
#define OFLAG_SGLOB	0x04000000  /* Oper can send globops */
#define OFLAG_ZLINE     0x08000000  /* Oper can use /zline and /unzline */
#define	OFLAG_MHACK	0x10000000  /* Oper can hack channel modes */

#define OFLAG_SGLOBOP	(OFLAG_GLOBOP|OFLAG_SGLOB)
#define OFLAG_LOCAL	(OFLAG_REHASH|OFLAG_HELPOP|OFLAG_SGLOBOP|OFLAG_GLOBOP|OFLAG_WALLOP|OFLAG_LOCOP|OFLAG_LROUTE|OFLAG_LKILL|OFLAG_KLINE|OFLAG_UNKLINE|OFLAG_LNOTICE|OFLAG_UMODEC|OFLAG_UMODEF)
#define OFLAG_GLOBAL	(OFLAG_LOCAL|OFLAG_DIE|OFLAG_RESTART|OFLAG_GROUTE|OFLAG_GKILL|OFLAG_GNOTICE|OFLAG_ZLINE|OFLAG_MHACK)
#define OFLAG_ISGLOBAL	(OFLAG_GROUTE|OFLAG_GKILL|OFLAG_GNOTICE|OFLAG_ZLINE|OFLAG_MHACK)

#define OPCanRehash(x)	((x)->oflag & OFLAG_REHASH)
#define OPCanDie(x)	((x)->oflag & OFLAG_DIE)
#define OPCanRestart(x)	((x)->oflag & OFLAG_RESTART)
#define OPCanHelpOp(x)	((x)->oflag & OFLAG_HELPOP)
#define OPCanGlobOps(x)	((x)->oflag & OFLAG_SGLOBOP)
#define OPCangmode(x)	((x)->oflag & OFLAG_GLOBOP)
#define OPCanWallOps(x)	((x)->oflag & OFLAG_WALLOP)
#define OPCanLocOps(x)	((x)->oflag & OFLAG_LOCOP)
#define OPCanLRoute(x)	((x)->oflag & OFLAG_LROUTE)
#define OPCanGRoute(x)	((x)->oflag & OFLAG_GROUTE)
#define OPCanLKill(x)	((x)->oflag & OFLAG_LKILL)
#define OPCanGKill(x)	((x)->oflag & OFLAG_GKILL)
#define OPCanKline(x)	((x)->oflag & OFLAG_KLINE)
#define OPCanUnKline(x)	((x)->oflag & OFLAG_UNKLINE)
#define OPCanLNotice(x)	((x)->oflag & OFLAG_LNOTICE)
#define OPCanGNotice(x)	((x)->oflag & OFLAG_GNOTICE)
#define OPIsAdmin(x)	((x)->oflag & OFLAG_ADMIN)
#define OPCanUModeC(x)	((x)->oflag & OFLAG_UMODEC)
#define OPCanUModeF(x)	((x)->oflag & OFLAG_UMODEF)
#define OPCanZline(x)	((x)->oflag & OFLAG_ZLINE)
#ifdef ALLOW_MODEHACK
#define	OPCanModeHack(x) ((x)->oflag & OFLAG_MHACK)
#else
#define	OPCanModeHack(x) (0)
#endif

/********************************************************************
 * help system flags/structs
 */

typedef struct help_struct {
	int flags;
	int ndesc;
	int naliases;
	char *command;
	char *usage;
	char **desc;
	char **aliases;
} aHelptopic;


#define H_NICK    0x00001     /* NickServ command */
#define H_MEMO    0x00002     /* MemoServ command */
#define H_CHAN    0x00004     /* ChanServ command */
#define H_OSERV   0x00008     /* OperServ Command */
#define H_ISERV   0x00010     /* InfoServ Command */
#define H_IRC     0x00020     /* non-services command/help */
#define H_GS	  0x00040     /* GameServ Command */

#define H_AOP     0x00100     /* requires AOP/higher access*/
#define H_SOP     0x00200     /* requires SOP/higher access*/
#define H_FOUNDER 0x00400     /* requires founder access*/

#define H_ACC3    0x01000     /* requires ACC3 */

#define H_HELPOP  0x10000     /* only helpops can need this help */
#define H_OPER    0x20000     /* only opers can need this help */
#define H_GOPER   0x40000     /* only global opers can need this help */

#define H_TOPIC   0x100000    /* help on a topic, not a command */
#define H_PAGE    0x200000    /* a raw help page */


#define OPSetRehash(x)	((x)->oflag |= OFLAG_REHASH)
#define OPSetDie(x)	((x)->oflag |= OFLAG_DIE)
#define OPSetRestart(x)	((x)->oflag |= OFLAG_RESTART)
#define OPSetHelpOp(x)	((x)->oflag |= OFLAG_HELPOP)
#define OPSetGlobOps(x)	((x)->oflag |= OFLAG_GLOBOP)
#define OPSetWallOps(x)	((x)->oflag |= OFLAG_WALLOP)
#define OPSetLocOps(x)	((x)->oflag |= OFLAG_LOCOP)
#define OPSetLRoute(x)	((x)->oflag |= OFLAG_LROUTE)
#define OPSetGRoute(x)	((x)->oflag |= OFLAG_GROUTE)
#define OPSetLKill(x)	((x)->oflag |= OFLAG_LKILL)
#define OPSetGKill(x)	((x)->oflag |= OFLAG_GKILL)
#define OPSetKline(x)	((x)->oflag |= OFLAG_KLINE)
#define OPSetUnKline(x)	((x)->oflag |= OFLAG_UNKLINE)
#define OPSetLNotice(x)	((x)->oflag |= OFLAG_LNOTICE)
#define OPSetGNotice(x)	((x)->oflag |= OFLAG_GNOTICE)
#define OPSSetAdmin(x)	((x)->oflag |= OFLAG_ADMIN)
#define OPSetUModeC(x)	((x)->oflag |= OFLAG_UMODEC)
#define OPSetUModeF(x)	((x)->oflag |= OFLAG_UMODEF)
#define OPSetZline(x)	((x)->oflag |= OFLAG_ZLINE)
#define	OPSetModeHack(x) ((x)->oflag |= OFLAG_MHACK)

#define OPClearRehash(x)	((x)->oflag &= ~OFLAG_REHASH)
#define OPClearDie(x)		((x)->oflag &= ~OFLAG_DIE)  
#define OPClearRestart(x)	((x)->oflag &= ~OFLAG_RESTART)
#define OPClearHelpOp(x)	((x)->oflag &= ~OFLAG_HELPOP)
#define OPClearGlobOps(x)	((x)->oflag &= ~OFLAG_GLOBOP)
#define OPClearWallOps(x)	((x)->oflag &= ~OFLAG_WALLOP)
#define OPClearLocOps(x)	((x)->oflag &= ~OFLAG_LOCOP)
#define OPClearLRoute(x)	((x)->oflag &= ~OFLAG_LROUTE)
#define OPClearGRoute(x)	((x)->oflag &= ~OFLAG_GROUTE)
#define OPClearLKill(x)		((x)->oflag &= ~OFLAG_LKILL)
#define OPClearGKill(x)		((x)->oflag &= ~OFLAG_GKILL)
#define OPClearKline(x)		((x)->oflag &= ~OFLAG_KLINE)
#define OPClearUnKline(x)	((x)->oflag &= ~OFLAG_UNKLINE)
#define OPClearLNotice(x)	((x)->oflag &= ~OFLAG_LNOTICE)
#define OPClearGNotice(x)	((x)->oflag &= ~OFLAG_GNOTICE)
#define OPClearAdmin(x)		((x)->oflag &= ~OFLAG_ADMIN)
#define OPClearUModeC(x)	((x)->oflag &= ~OFLAG_UMODEC)
#define OPClearUModeF(x)	((x)->oflag &= ~OFLAG_UMODEF)
#define OPClearZline(x) 	((x)->oflag &= ~OFLAG_ZLINE)
#define	OPClearModeHack(x)	((x)->oflag &= ~OFLAG_MODEHACK)


/*
 * defined debugging levels
 */
#define	DEBUG_FATAL  0
#define	DEBUG_ERROR  1	/* report_error() and other errors that are found */
#define	DEBUG_NOTICE 3
#define	DEBUG_DNS    4	/* used by all DNS related routines - a *lot* */
#define	DEBUG_INFO   5	/* general usful info */
#define	DEBUG_NUM    6	/* numerics */
#define	DEBUG_SEND   7	/* everything that is sent out */
#define	DEBUG_DEBUG  8	/* anything to do with debugging, ie unimportant :) */
#define	DEBUG_MALLOC 9	/* malloc/free calls */
#define	DEBUG_LIST  10	/* debug list use */

/*
 * Don't use sockaddr_storage: it's too big. --Onno
 */
union Address
{
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
	u_char  addr_dummy[2];
#define addr_family addr_dummy[1]
#else
	unsigned short int	addr_family;
#endif
	struct sockaddr_in	in;
#ifdef AF_INET6
	struct sockaddr_in6	in6;
#endif
};

struct HostEnt
{
  char *h_name;                 /* Official name of host.  */
  char **h_aliases;             /* Alias list.  */
  anAddress **h_addr_list;      /* List of addresses from name server. */
#define h_addr  h_addr_list[0]  /* Address, for backward compatibility.  */
};

struct	ConfItem	{
	unsigned int	status;	/* If CONF_ILLEGAL, delete when no clients */
	int	clients;	/* Number of *LOCAL* clients using this */
	anAddress	addr;	/* network address of host */
	char	*host;
	char	*passwd;
	char	*name;
	char    *string4;
	char    *string5;
	char	*string6;
	char	*string7;
	int	port;
	time_t	hold;	/* Hold action until this time (calendar time) */
	int	tmpconf, bits;
#ifndef VMSP
	aClass	*class;  /* Class of connection */
#endif
	struct	ConfItem *next;
};

#define	CONF_ILLEGAL		0x80000000
#define	CONF_MATCH		0x40000000
#define	CONF_QUARANTINED_SERVER	0x0001
#define	CONF_CLIENT		0x0002
#define	CONF_CONNECT_SERVER	0x0004
#define	CONF_NOCONNECT_SERVER	0x0008
#define	CONF_LOCOP		0x0010
#define	CONF_OPERATOR		0x0020
#define	CONF_ME			0x0040
#define	CONF_KILL		0x0080
#define	CONF_ADMIN		0x0100
#define	CONF_CLASS		0x0400
#define	CONF_SERVICE		0x0800
#define	CONF_LEAF		0x1000
#define	CONF_LISTEN_PORT	0x2000
#define	CONF_HUB		0x4000
#define	CONF_UWORLD		0x8000
#define CONF_QUARANTINED_NICK	0x10000
#define CONF_ZAP		0x20000
#define CONF_CONFIG             0x100000
#define CONF_CRULEALL           0x200000
#define CONF_CRULEAUTO          0x400000
#define CONF_MISSING		0x800000
#define CONF_AHURT		0x1000000
#define CONF_SUP_ZAP		0x2000000

#define CONF_SHOWPASS		(CONF_KILL | CONF_ZAP | CONF_QUARANTINE | CONF_AHURT | CONF_SUP_ZAP)
#define	CONF_OPS		(CONF_OPERATOR | CONF_LOCOP)
#define	CONF_SERVER_MASK	(CONF_CONNECT_SERVER | CONF_NOCONNECT_SERVER)
#define	CONF_CLIENT_MASK	(CONF_CLIENT | CONF_SERVICE | CONF_OPS | CONF_SERVER_MASK )
#define CONF_CRULE              (CONF_CRULEALL | CONF_CRULEAUTO)
#define CONF_QUARANTINE		(CONF_QUARANTINED_SERVER|CONF_QUARANTINED_NICK)

#define	IsIllegal(x)	((x)->status & CONF_ILLEGAL)
#define	IsCNLine(x)	((x)->status & CONF_SERVER_MASK)
#define IsTemp(x)	((x)->tmpconf)

#define GetUserSupVersion(x)	((x)->sup_version)

struct  StringHash
{
	struct StringHashElement* ptr;
};

struct	StringHashElement {
	char* str;
	int   refct;

	struct StringHashElement* next;
};

/*
 * Client structures
 */
struct	User	{
	struct	User	*nextu;
	Link	*channel;	/* chain of channel pointer blocks */
	Link	*invited;	/* chain of invite pointer blocks */
	Link	*silence;	/* chain of silence pointer blocks */
	char	*away;		/* pointer to away message */
        char    *mask;          /* masked host */
	time_t	last;
	int	refcnt;		/* Number of times this block is referenced */
	int	joined;		/* number of channels joined */
	char	username[USERLEN+1];
	char	host[HOSTLEN+1];
        char	server[HOSTLEN+1];
        char    *sup_version;
#ifdef  KEEP_HURTBY
	char    *hurtby;
#endif
				/*
				** In a perfect world the 'server' name
				** should not be needed, a pointer to the
				** client describing the server is enough.
				** Unfortunately, in reality, server may
				** not yet be in links while USER is
				** introduced... --msa
				*/
#ifdef	LIST_DEBUG
	aClient	*bcptr;
#endif
};

struct	Server	{
	struct	Server	*nexts;
	anUser	*user;		/* who activated this connection */
	char	up[HOSTLEN+1];	/* uplink for this server */
	char	by[NICKLEN+1];
	aConfItem *nline;	/* N-line pointer for this server */
#ifdef	LIST_DEBUG
	aClient	*bcptr;
#endif
};

struct Client	{
	struct	Client *next, *prev, *hnext;
	anUser	*user;		/* ...defined, if this is a User */
	aServer	*serv;		/* ...defined, if this is a server */
	int	hashv;		/* raw hash value */
	time_t  hurt;           /* hurt til... */
	time_t	lasttime;	/* ...should be only LOCAL clients? --msa */
	time_t	firsttime;	/* time client was created */
	time_t	since;		/* last time we parsed something */
	time_t	lastnick;	/* TimeStamp on nick */
	long	flags;		/* client flags */
	long	uflags;		/* client usermode */
	aClient	*from;		/* == self, if Local Client, *NEVER* NULL! */
	int	fd;		/* >= 0, for local clients */
	int	hopcount;	/* number of servers to this 0 = local */
	int	watches;	/* How many watches user has set */
	short	status;		/* Client type */
	char	name[HOSTLEN+1]; /* Unique name of the client, nick or host */
	char	username[USERLEN+1]; /* username here now for auth stuff */
	char	info[REALLEN+1]; /* Free form additional client information */
	/*
	** The following fields are allocated only for local clients
	** (directly connected to *this* server with a socket.
	** The first of them *MUST* be the "count"--it is the field
	** to which the allocation is tied to! *Never* refer to
	** these fields, if (from != self).
	*/
	int	count;		/* Amount of data in buffer */
	char	buffer[BUFSIZE]; /* Incoming message buffer */
	char	sup_server[HOSTLEN+1], sup_host[HOSTLEN+1];
	short	lastsq;		/* # of 2k blocks when sendqueued called last*/
	dbuf	sendQ;		/* Outgoing message queue--if socket full */
	dbuf	recvQ;		/* Hold for data incoming yet to be parsed */
#ifdef NOSPOOF
	u_int32_t	nospoof;	/* Anti-spoofing random number */
#endif
	long	oflag;		/* Operator access flags -Cabal95 */
	long	sendM;		/* Statistics: protocol messages send */
	long	sendK;		/* Statistics: total k-bytes send */
	long	receiveM;	/* Statistics: protocol messages received */
	long	receiveK;	/* Statistics: total k-bytes received */
	u_short	sendB;		/* counters to count upto 1-k lots of bytes */
	u_short	receiveB;	/* sent and received. */
	aClient	*acpt;		/* listening client which we accepted from */
	Link	*confs;		/* Configuration record associated */
	Link	*watch;		/* User's watch list */
	int	authfd;		/* fd for rfc931 authentication */
	anAddress	addr;	/* keep real ip# too */
	u_short	port;	/* and the remote port# too :-) */
	struct	HostEnt	*hostp;
	LOpts   *lopt;
#ifdef	pyr
	struct	timeval	lw;
#endif
	char	sockhost[HOSTLEN+1]; /* This is the host name from the socket
				     ** and after which the connection was
				     ** accepted.
				     */
	char	passwd[PASSWDLEN+1];
};

#define	CLIENT_LOCAL_SIZE sizeof(aClient)
#define	CLIENT_REMOTE_SIZE offsetof(aClient,count)

/*
 * statistics structures
 */
struct	stats {
	unsigned int	is_cl;	/* number of client connections */
	unsigned int	is_sv;	/* number of server connections */
	unsigned int	is_ni;	/* connection but no idea who it was */
	unsigned short	is_cbs;	/* bytes sent to clients */
	unsigned short	is_cbr;	/* bytes received to clients */
	unsigned short	is_sbs;	/* bytes sent to servers */
	unsigned short	is_sbr;	/* bytes received to servers */
	unsigned long	is_cks;	/* k-bytes sent to clients */
	unsigned long	is_ckr;	/* k-bytes received to clients */
	unsigned long	is_sks;	/* k-bytes sent to servers */
	unsigned long	is_skr;	/* k-bytes received to servers */
	time_t 		is_cti;	/* time spent connected by clients */
	time_t		is_sti;	/* time spent connected by servers */
	unsigned int	is_ac;	/* connections accepted */
	unsigned int	is_ref;	/* accepts refused */
	unsigned int	is_unco; /* unknown commands */
	unsigned int	is_wrdi; /* command going in wrong direction */
	unsigned int	is_unpf; /* unknown prefix */
	unsigned int	is_empt; /* empty message */
	unsigned int	is_num;	/* numeric message */
	unsigned int	is_kill; /* number of kills generated on collisions */
	unsigned int	is_fake; /* MODE 'fakes' */
	unsigned int	is_asuc; /* successful auth requests */
	unsigned int	is_abad; /* bad auth requests */
	unsigned int	is_udp;	/* packets recv'd on udp port */
	unsigned int	is_loc;	/* local connections made */
};

struct ListOptions {
        LOpts   *next;
        Link    *yeslist, *nolist;
        int     flag;
        int     starthash;
        short int       showall;
        unsigned short  usermin;
        int     usermax;
        time_t  currenttime;
        time_t  chantimemin;
        time_t  chantimemax;
        time_t  topictimemin;
        time_t  topictimemax;
};



/* mode structure for channels */

struct	SMode	{
	unsigned int	mode;
	unsigned int	mlock_on, mlock_off;
	int	limit;
	char	key[KEYLEN+1];
};

/* Message table structure */

struct	Message	{
	char	*cmd;
	int	(* func)();
	unsigned int	count;
	int	parameters;
	char	flags;
		/* bit 0 set means that this command is allowed to be used
		 * only on the average of once per 2 seconds -SRB */
	unsigned long	bytes;
	int while_hurt;
};

/* message hashtable structure */
struct  HMessage {
        struct Message  *mptr;
        struct HMessage *next;
};

/* general link structure used for chains */

struct	SLink	{
	struct	SLink	*next;
	union {
		aClient	*cptr;
		aChannel *chptr;
		aConfItem *aconf;
		aWatch *wptr;
		char	*cp;
		struct {
		  char *banstr;
		  char *who;
		  time_t when;
		} ban;
	} value;
	int	flags;
};

struct Watch {
           aWatch  *hnext;
           time_t   lasttime;
           Link  *watch;
           char  nick[1];
};

/* channel structure */

struct Channel	{
	struct	Channel *nextch, *prevch, *hnextch;
	int	hashv;		/* raw hash value */
	Mode	mode;
	time_t	creationtime;
	char	topic[TOPICLEN+1];
	char	topic_nick[NICKLEN+1];
	time_t	topic_time;
	int	users;
	Link	*members;
	Link	*invites;
	Link	*banlist;
	char	chname[1];
};

/*
** Channel Related macros follow
*/

/* Channel related flags */

#define	CHFL_CHANOP     0x0001 /* Channel operator */
#define	CHFL_VOICE      0x0002 /* The power to speak */
#define	CHFL_DEOPPED	0x0004 /* Is de-opped by a server */
#define	CHFL_SERVOPOK   0x0008 /* Server op allowed */
#define	CHFL_ZOMBIE     0x0010 /* Kicked from channel */
#define	CHFL_BAN	0x0020 /* Ban channel flag */
#define CHFL_BQUIET	0x0040 /* Is banned on the channel? */
#define	CHFL_OVERLAP    (CHFL_CHANOP|CHFL_VOICE)

#define BAN_BLOCK      0x0001
#define BAN_BQUIET     0x0002
#define BAN_MASK       0x0004
#define BAN_REQUIRE    0x0008
#define BAN_RBLOCK     0x0010
#define BAN_GECOS      0x0020

#define BAN_STD                (BAN_BLOCK|BAN_BQUIET)

/* Channel Visibility macros */

#define	MODE_CHANOP	CHFL_CHANOP
#define	MODE_VOICE	CHFL_VOICE
#define	MODE_PRIVATE	0x0004
#define	MODE_SECRET	0x0008
#define	MODE_MODERATED  0x0010
#define	MODE_TOPICLIMIT 0x0020
#define	MODE_INVITEONLY 0x0040
#define	MODE_NOPRIVMSGS 0x0080
#define	MODE_KEY	0x0100
#define	MODE_BAN	0x0200
#define	MODE_LIMIT	0x0400
#define MODE_SHOWHOST	0x8000
#define MODE_NOCOLORS	0x1000

/*
 * mode flags which take another parameter (With PARAmeterS)
 */
#define	MODE_WPARAS	(MODE_CHANOP|MODE_VOICE|MODE_BAN|MODE_KEY|MODE_LIMIT)

/*
 * Undefined here, these are used in conjunction with the above modes in
 * the source.
#define	MODE_DEL       0x40000000
#define	MODE_ADD       0x80000000
 */

/* Lifted somewhat from Undernet code --Rak */

#define IsSendable(x)           (DBufLength(&x->sendQ) < 2048)
#define DoList(x)               ((x)->lopt)


#define	HoldChannel(x)		(!(x))
/* name invisible */
#define	SecretChannel(x)	((x) && ((x)->mode.mode & MODE_SECRET))
/* channel not shown but names are */
#define	HiddenChannel(x)	((x) && ((x)->mode.mode & MODE_PRIVATE))
/* channel visible */
#define	ShowChannel(v,c)	(PubChannel(c) || IsMember((v),(c)))
#define	PubChannel(x)		((!x) || ((x)->mode.mode &\
				 (MODE_PRIVATE | MODE_SECRET | MODE_SHOWHOST)) == 0)

#define	IsChannelName(name) ((name) && (*(name) == '#' || *(name) == '&' || *(name) == '+') \
                             && (*(name+1) != '#' || *(name) != '+'))
#define IsModelessChannel(name) (0 /*&& (name) && (*(name) == '+')*/)
#define IsSystemChannel(name) ((name) && (*name) && (!mycmp((name)+1, HELPOP_CHAN+1)))

/* Misc macros */

#define	BadPtr(x) (!(x) || (*(x) == '\0'))

#define	isvalid(c) (((c) >= 'A' && (c) <= '~') || isdigit(c) || (c) == '-')

#define	MyConnect(x)			((x)->fd >= 0)
#define	MyClient(x)			(MyConnect(x) && IsClient(x))
#define	MyOper(x)			(MyConnect(x) && IsOper(x))

/* String manipulation macros */

/* strncopynt --> strncpyzt to avoid confusion, sematics changed
   N must be now the number of bytes in the array --msa */
#define	strncpyzt(x, y, N) do{(void)strncpy(x,y,N);x[N-1]='\0';}while(0)
#define	StrEq(x,y)	(!strcmp((x),(y)))

/* used in SetMode() in channel.c and m_umode() in s_msg.c */

#define	MODE_NULL      0
#define	MODE_ADD       0x40000000
#define	MODE_DEL       0x20000000

/* return values for hunt_server() */

#define	HUNTED_NOSUCH	(-1)	/* if the hunted server is not found */
#define	HUNTED_ISME	0	/* if this server should execute the command */
#define	HUNTED_PASS	1	/* if message passed onwards successfully */

/* used when sending to #mask or $mask */

#define	MATCH_SERVER  1
#define	MATCH_HOST    2

/* used for async dns values */

#define	ASYNC_NONE	(-1)
#define	ASYNC_CLIENT	0
#define	ASYNC_CONNECT	1
#define	ASYNC_CONF	2
#define	ASYNC_SERVER	3

/* misc variable externs */

extern	char	*version, *infotext[];
extern	char	*generation, *creation;
extern	int	schecksfd;

/* misc defines */

#define	FLUSH_BUFFER	-2
#define	UTMP		"/etc/utmp"
#define	COMMA		","

/* IRC client structures */

#endif /* __struct_include__ */
