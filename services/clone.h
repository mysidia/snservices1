/**
 * \file clone.h
 * \brief Clone system headers
 *
 * Constants and structure data used by clone detection
 *
 * \wd \taz \greg \mysid
 * \date 1996-1997, 2000-2001
 *
 * $Id$
 */

/*
 * Copyright (c) 1996-1997 Chip Norkus
 * Copyright (c) 1997 Max Byrd
 * Copyright (c) 1997 Greg Poma
 * Copyright (c) 2000-2001 James Hess
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

#ifndef __CLONE_H
#define __CLONE_H

/// A user@ clone trigger element
typedef struct userclonestruct UserClone;

/// Structure describing a username trigger
struct userclonestruct {
          /// Username affected
	char user[USERLEN];

          /// Current number of matching clients
	int clones;

          /// Current trigger level
        int trigger;


	   /// Current flags
	flag_t uflags;

          /// Next user trigger item
	UserClone *next;

          /// Previous user trigger item
	UserClone *previous;
};

/// @Host clone trigger element
typedef struct hostclonestruct HostClone;

/// Structure of a database item describing host-level clone
/// information, controls.
struct hostclonestruct {
          /// Hostname buffer
	char host[HOSTLEN];

          /// First user trigger item
	UserClone *firstUser;

          /// Last user trigger item
	UserClone *lastUser;

          /// Host level trigger
	int trigger;

          /// Number of matching clients
	int clones;

          /// Host clone flags
	int flags;

	HostClone *next, *previous, *hashnext, *hashprev;
};

/// A clonerule element
typedef struct trigger_rule CloneRule;

/// Trigger rewrite rule database item structure
struct trigger_rule {

          /// Mask pattern buffer
   char  mask[HOSTLEN+USERLEN+2];

          /// Host level trigger reset     
   int   trigger;

          /// User level trigger reset
   int   utrigger;

          /// Clone flags reset
   long  flags;

          /// Kill message to use
   char  *kill_msg;

          /// Warning message to use
   char  *warn_msg;

          /// Next trigger rewrite rule
   CloneRule *next;
};

extern CloneRule *first_crule;
CloneRule *GetCrule(char *);
CloneRule *GetCruleMatch(char *);
CloneRule *NewCrule();
void AddCrule(CloneRule *, int);
void RemoveCrule(CloneRule *);
void UpdateCrule(CloneRule, CloneRule *);
int addClone(char *, char *, char *);
void delClone(char *, char *);
HostClone *getCloneData(char *);
UserClone *addUserClone(HostClone *, char *);
void delUserClone(HostClone *, UserClone *);
UserClone *getUserCloneData(HostClone *, char *);
void initCloneData(HostClone *);
#endif
