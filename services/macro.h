/** @file macro.h
 * \brief Various utilities used by services
 *
 * \greg
 * \date 1997, 2001
 * $Id$ *
 */

/*
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
#include "queue.h"

/**
 * deep, dark magic to tell gcc (but not g++, at least not in 2.7.2.x)
 * that an argument to a function is not used.
 */
#if defined(__GNUC__)
#if defined(__cplusplus)
#define ARGUNUSED(arg) arg
#else
#define ARGUNUSED(arg) arg __attribute__ ((unused))
#endif
#else /* not __GNUC__ */
#define ARGUNUSED(arg) arg
#endif

/// Create a memo box for a user
#define ADD_MEMO_BOX(who) do {						\
	(who)->memos = (MemoBox *)oalloc(sizeof(MemoBox));		\
        (who)->memos->max = MS_DEF_RCV_MAX;				\
        LIST_INIT(&(who)->memos->mb_memos);				\
} while(0)

