 /* $Id$ */

/*
 * Copyright (c) 1996-1997 Chip Norkus
 * Copyright (c) 1997 Max Byrd
 * Copyright (c) 1997 Greg Poma
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

/**
 * \file chanserv.h
 * ChanServ header file
 */

#ifndef __CHANSERV_H
#define __CHANSERV_H

/**
 * @brief Magic channel used for auto-+h by services
 */
#define HELPOPS_CHAN "#helpops"

/************************************************************************/

/* mode flags */
#define PM_MASK 0x00f000ff	///< Plus-mode mask
#define PM_I 	0x00000001	///< +i
#define PM_K 	0x00000002	///< +k
#define PM_L 	0x00000004	///< +l
#define PM_M 	0x00000008	///< +m
#define PM_N 	0x00000010	///< +n
#define PM_P 	0x00000020	///< +p
#define PM_S 	0x00000040	///< +s
#define PM_T 	0x00000080	///< +t [?]
#define MM_MASK 0x000fff00	///< Minus-mode mask */
#define MM_I	0x00000100	///< -i
#define MM_K	0x00000200	///< -k
#define MM_L	0x00000400	///< -l
#define MM_M	0x00000800	///< -m
#define MM_N	0x00001000	///< -n
#define MM_P	0x00002000	///< -p
#define MM_S	0x00004000	///< -s
#define MM_T	0x00008000	///< -t [?]

#define MM_H	0x00010000
#define MM_C    0x00020000
#define PM_H    0x00100000
#define PM_C	0x00200000

/************************************************************************/

/* Channel flags */
#define COPGUARD	0x00000001	///< Channel opguard
#define CKTOPIC		0x00000002	///< Keeptopic mode
#define CLEAVEOPS	0x00000004	///< Leaveops \bug unimplemented
#define CQUIET 		0x00000008	///< Quiet Changes
#define CCSJOIN		0x00000010	///< Channel is a 'ghost' channel ATM
					///< original meaning (ChanServ
					///< stays in channel) is antiquated
#define CIDENT		0x00000020	///< Ops must identify for access
#define CHOLD		0x00000040	///< Held -- will not expire
#define CREG		0x00000080	///< Unknown
#define CBANISH		0x00000100	///< Banished channel
#define CPROTOP		0x00000200	///< Protect sops from deops/such
#define CMARK		0x00000400	///< Channel is marked
#define CCLOSE		0x00000800	///< Channel has been closed
#define CFORCEXFER	0x00001000	///< Password is not authoritative
					///< at the moment for drop command.
#define CENCRYPT	0x00002000	///< Channel password encrypted
#define CGAMESERV	0x00004000	///< Enable GameServ

/************************************************************************/

/*
 * op levels.
 *
 * Anything named OPxx is an open field.
 * feel free to fill open fields with whatever you want
 * For more information, see docs/levelscheme.
 */
#define OPNONE       0			///< No chanop access
#define OP1          1			///<  Unused
#define OP2          2			///<  Unused
#define MAOP         3			///< Mini-AOP
#define OP4          4			///<  Unused
#define AOP          5			///<  Unused
#define OP6          6			///< AutoOp
#define OP7          7			///<  Unused
#define MSOP         8			///< MiniSop (Channel Operator)
#define OP9          9			///<  Unused
#define SOP         10			///< SuperOp
#define OP11        11			///<  Unused
#define OP12        12			///<  Unused
#define MFOUNDER    13			///< Mini Founder
#define OP14        14			///<  Unused
#define FOUNDER     15			///< Founder

/************************************************************************/

#define CHANOP 0x0001			///< +o - Channel Operator Status
#define CHANVOICE 0x0002		///< +v - Voice Status



void sendToChanServ(UserList *, char **, int);
void addUserToChan(UserList *, char *);
void remUserFromChan(UserList *, char *);
void remFromAllChans(UserList *);
void changeNickOnAllChans(UserList *, UserList *);
void setChanMode(char **, int);
void setChanTopic(char **, int);
void sendToChanOps(ChanList *, char *,...);
void sendToChanOpsAlways(ChanList *, char *,...);
void addChan(ChanList *);
void delChan(ChanList *);
void addRegChan(RegChanList *);
void delRegChan(RegChanList *);
void addChanAkick(RegChanList *, cAkickList *);
void delChanAkick(RegChanList *, cAkickList *);
void addChanOp(RegChanList *, cAccessList *);
void delChanOp(RegChanList *, cAccessList *);
void initRegChanData(RegChanList *);
void addChanUser(ChanList *, cNickList *);
void delChanUser(ChanList *, cNickList *, int);
void addChanBan(ChanList *, cBanList *);
void delChanBan(ChanList *, cBanList *);
ChanList *getChanData(char *);
RegChanList *getRegChanData(char *);
cNickList *getChanUserData(ChanList *, UserList *);
cBanList *getChanBan(ChanList *, char *);
int getChanOp(RegChanList *, char *);
cAccessList *getChanOpData(const RegChanList *, const char *);
cAkickList *getChanAKick(RegChanList *, char *);
void syncChanData(time_t);
cAkickList *getChanAkick(RegChanList *, char *);
void freeRegChan(RegChanList *);
void indexAkickItems(RegChanList *);
void indexOpItems(RegChanList *);
int isFounder(RegChanList *, UserList *);
void makeModeLockStr(RegChanList *, char *);
char *initModeStr(char *chan);
void banKick(ChanList *, UserList *, char *, ...);
void rshift_argv(char **args, int x, int numargs);
const char *opLevelName(int level, int x_case);

#endif /* __CHANSERV_H */
