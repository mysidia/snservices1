/** @file memoserv.h
 *  \brief MemoServ headers
 *
 *  Defines various memo flags and declares functions used by MemoServ.
 *
 *  \wd \taz \greg
 *  \date 1996-1997
 *
 * $Id$
 */

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

#ifndef __MEMOSERV_H
#define __MEMOSERV_H

#include "queue.h"

/*
 * user MemoServ flags
 */
#define MNOMEMO 		0x0001	///< No Memos accepted
#define MSECURE 		0x0002	///< User requires identify to send/receive
#define MSELFCLEAN		0x0004	///< Automatically clean deleted memos
#define MFORWARDED		0x0008	///< Memos forwarded

/*
 * sent memo flags
 */
#define MEMO_UNREAD		0x0001	///< Memo has not yet been read
#define MEMO_DELETE		0x0002	///< Memo has been deleted
#define MEMO_SAVE		0x0004	///< User has saved memo
#define MEMO_FWD		0x0008	///< Memo was forwarded
#define MEMO_REPLY		0x0010	///< Memo is a reply message

/*
 * functions 
 */
void initMemoBox(MemoBox *);		///< Initialize a memo box
void delMemo(MemoBox *, MemoList *);
void checkMemos(UserList *);
void cleanMemos(UserList *);
void addMemoBlock(MemoBox *, MemoBlock *);
void delMemoBlock(MemoBox *, MemoBlock *);
MemoBlock *getMemoBlockData(MemoBox *, RegNickList *);

void syncMemoData(time_t);
void sendToMemoServ(UserList *, char **, int);
int ShouldMemoExpire(MemoList *, int);

#endif /* __MEMOSERV_H */
