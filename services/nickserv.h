/** @file nickserv.h
 * \brief NickServ Header
 *
 * Defines the various nickname flags, oper flags, and declares some
 * nickname-related functions used throughout services.
 *
 * \wd \taz \greg \mysid
 * \date 1996-2001
 *
 * $Id$ *
 *
 */

/*
 * Copyright (c) 1996-1997 Chip Norkus
 * Copyright (c) 1997 Max Byrd
 * Copyright (c) 1997 Greg Poma
 * Copyright (c) 2001 James Hess
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the authors nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __NICKSERV_H
#define __NICKSERV_H
#include "queue.h"
#include "memoserv.h"
#include "string.h"
#include "struct.h"

void addGhost(char *);
void delGhost(char *);
void delTimedGhost(char *);


/* online flags: */
#define NISOPER   0x0001	///< Is client +o ?
#define NISAWAY   0x0002	///< Is client /AWAY ?
#define NISHELPOP 0x0004	///< Is client +h ?
#define NISONLINE 0x0008	///< Is client online? (huh?)
#define NISAHURT  0x0010	///< Is client ahurt?
#define NOISMASK  0x0020	///< Is client +m ?
#define NCNICK    0x1000	///< CNICK sent?
#define NOISREG   0x2000

/* nickname flag slots, 5 open */
#define NKILL		 0x00001 ///< Enforcement
#define NVACATION 	 0x00002 ///< Vacation */
#define NHOLD     	 0x00004 ///< Held nickname for SN purposes */
#define NIDENT	 	 0x00008 ///< Require identification */
#define NTERSE   	 0x00010 ///< Terse mode */
/* defines if a user can be added to channel op lists */
#define NOADDOP  	 0x00040 ///< User not allowing addition to op lists
#define NEMAIL   	 0x00080 ///< Show email
#define NBANISH  	 0x00100 ///< Banished
#define NGRPOP   	 0x00200 ///< Old grpop flag
#define NBYPASS  	 0x00400 ///< Bypass AHURT
#define NUSEDPW  	 0x00800 ///< Has ever identified
#define NDBISMASK	 0x01000 ///< +m option in nick db
#define NMARK	 	 0x02000 ///< MARKED nick
#define NDOEDIT		 0x04000 ///< Not used anymore ATM, see /OS OVERRIDE
#define NOSENDPASS	 0x08000 ///< User doesn't want password recovery
#define NACTIVE		 0x10000 ///< User e-mail has been verified
#define NDEACC		 0x20000 ///< E-mail needs to be re-verified
#define NFORCEXFER	 0x40000 ///< Nick is subject to forced transfer
				 ///< by an oper
#define NENCRYPT	 0x80000 ///< Password encrypted
#define NAHURT		 0x100000 ///< ???

/* New Nick flags */
#define NVERIFIED	 0x200000
#define NNOVERIFY	 0x400000
#define NVERIFY_SENT     0x800000
#define NNETWORKNICK     0x1000000 ///< Nick reserved for network use


/* users opflags */
#define OROOT     0x000001	/*!< Services Root DONT CHANGE*/
#define OREMROOT  0x000002	/*!< A secure flag            */
#define OADMIN    0x000004	/*!< is admin                 */
#define OSERVOP   0x000008	/*!< is servop (deprecated)   */
#define OOPER     0x000010	/*!< basic oper privs         */
#define ORAKILL   0x000020	/*!< restricted akill priv    */
#define OAKILL    0x000040	/*!< full akill priv          */
#define OINFOPOST 0x000080	/*!< high-priority info post  */
#define OSETOP    0x000100	/*!< can use setop/add/del    */
#define OSETFLAG  0x000200	/*!< can use setflag          */
#define ONBANDEL  0x000400	/*!< can banish/del nicks     */
#define OCBANDEL  0x000800	/*!< can banish/del chans     */
#define OIGNORE   0x001000	/*!< can set services ignores */
#define OGRP      0x002000	/*!< can getrealpass          */
#define ORAW      0x004000      /*!< can use raw              */
#define OJUPE     0x008000	/*!< can jupe                 */
#define OLIST     0x010000	/*!< can cs/ns list & cs whois*/
#define OCLONE    0x020000	/*!< can edit clonerules      */
#define OPROT     0x080000	/*!< protect from normal setop*/
#define OACC      0x100000	/*!< override user restrictions */
#define OHELPOP   0x200000	/*!< user is a helpop	      */
#define ODMOD	  0x400000	/*!< direct modification      */
#define OAHURT    0x800000	/*!< access to AutoHurt commands */
#define OVERRIDE  OACC		/*!< symbolic alias	      */

/// Default set of opflags that opers get
#define OPFLAG_DEFAULT	(OOPER|ORAKILL|OINFOPOST|OACC)

/// default flags assigned with A
#define OPFLAG_ADMIN	(OADMIN|OOPER|OSERVOP|OAKILL|OINFOPOST|OSETOP| \
                         OIGNORE|OPROT|OACC)
/// All positive flags
#define OPFLAG_PLUS	((~0) & ~(OPFLAG_MINUS))

/// All Negative/restrictive flags
#define OPFLAG_MINUS	(0)

/// Root-controlled flags
#define OPFLAG_ROOTSET	(OADMIN|OSERVOP|OGRP|ORAW|OJUPE|OLIST|OSETOP|OSETFLAG| \
                         ONBANDEL|OCBANDEL|OIGNORE|OAKILL|OCLONE|OPROT|ODMOD|OAHURT)

/// Flags granted to root
#define OPFLAG_ROOT	(OPFLAG_PLUS|OROOT)


/*
 * functions
 */
void		setIdentify(UserList *, RegNickList *);
void		clearIdentify(UserList *);
int		isIdentified(UserList *, RegNickList *);
int		isRecognized(UserList *, RegNickList *);
int             isOper(UserList *);
int             isRoot(UserList *);
int             issRoot(UserList *);
int             isGRPop(UserList *);
int		opFlagged(UserList *, flag_t);
flag_t		getOpFlags(UserList *);
int		canAkill(UserList *);
int             isServop(UserList *);
int             checkAccess(char *, char *, RegNickList *);
int             isGhost(char *);
int             addFlood(UserList *, int);
int		addNReg(char *);
int		addAccItem(RegNickList *, char *);
int		delAccItem(RegNickList *, char *, char *);
void            addNewUser(char **, int);
void            remUser(char *, int ignoreDesync);
void		addGhost(char *);
void            sendToNickServ(UserList *, char **, int);
void            setMode(char *, char *);
void            changeNick(char *, char *, char *);
void            nDesynch(char *, char *);
void		addNick(UserList *);
void		addRegNick(RegNickList *);
void		delNick(UserList *);
void		delRegNick(RegNickList *);
void            setFlags(char *, int, char);
void            addAccessMask(char *, UserList *);
void            delAccessMask(char *, UserList *);
void		delNReg(char *);
void		initNickData(RegNickList *);
void            delGhost(char *);
void		syncNickData(time_t);
UserList       *getNickData(char *);
RegNickList    *getRegNickData(const char *);
void		addOpData(RegNickList *);
void		delOpData(RegNickList *);
char	       *regnick_ugethost(UserList *, RegNickList *, int sM = 1);

/*
 * semi-global variables
 */
/* None left */
#endif /* __NICKSERV_H */
