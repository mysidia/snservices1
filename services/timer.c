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
 * \file timer.c
 * Implementation of services timed events
 */


#include "services.h"
#include "mass.h"
#include <time.h>

/**
 * @brief Element of the timer function name table
 */
struct t_table
{
	char *name;		///< Name of timer function

	void (*func) (char *);	///< Pointer to timer function
	/* void (*func) (void *); */
};

extern void autoremoveakill(char *mask);
extern void killide(char *);
extern void delTimedGhost(char *);
extern void deleteTimedGhostChannel(char *);
extern void flushLogs(char *);

/* This is used to map timer addressed to the function names */
// *INDENT-OFF*

static struct t_table tt[] = {
  { "autoremoveakill",         autoremoveakill },
  { "killide",                 killide },
  { "delTimedGhost",           delTimedGhost },
  { "deleteTimedGhostChannel", deleteTimedGhostChannel },
  { "timed_akill_queue",       timed_akill_queue },
  { "timed_advert_maint",      timed_advert_maint },
  { "sync_cfg",	               sync_cfg },
  { "expireNicks",             expireNicks },
  { "expireChans",             expireChans },
  { "checkTusers",             checkTusers },
  { "flushLogs",               flushLogs }
};

static int      curtid = 0;

/// Timer list item
struct t_chain {
          /// List next pointer
	struct t_chain *next;

          /// List previous pointer
	struct t_chain *prev;

          /// When to run (UTC)
	time_t          run;

          /// Timer identifying number
	time_t          tid;

          /// Function to execute when timer expires
	void            (*func) (char *);

          /// String argument to pass to function
	char           *args;
};

// *INDENT-ON*

/**
 * \brief First item in the list of timers
 */
static struct t_chain *firstTimerItem = NULL;

/**
 * \brief Schedules a timer
 * \param seconds How long (in seconds) to wait before executing the function
 * \param func A pointer to the function to execute of type void (*)(char *)
 * \param args This string is passed to the function when it is executed
 *
 * This function creates and adds a timer structure to the list
 * saying that the specified function should be executed.
 *
 * \warning The function called by timer is expected to free the string
 *          passed to it.  This is irregular memory management, however,
 *          so the free of that string should be clearly marked so that
 *          it can be removed in the future, when this behavior may change.
 *          Functions called should also take that change probability
 *          into consideration.
 */
int
timer(long seconds, void (*func) (char *), void *args)
{
	struct t_chain *tc, *temp;
	time_t curtime;

	time_t timetorun;

	curtime = time(NULL);

	if (seconds <= 0) {
		(void)func((char *)args);
		return 0;
	}
	timetorun = curtime + seconds;

	tc = (struct t_chain *)oalloc(sizeof(struct t_chain));
	tc->func = func;
	tc->run = timetorun;
	tc->args = (char *)args;
	curtid++;
	if (!curtid) {
		sSend
			("%s GOPER :Timer ID overrun!  Services may be unstable until a restart.\n",
			 myname);
		curtid++;
	}
	tc->tid = curtid;

	/*
	 * If there isn't any timers 
	 */
	if (firstTimerItem == NULL) {
		firstTimerItem = tc;
		tc->next = NULL;
		tc->prev = NULL;
		return tc->tid;
	}
	if (timetorun < firstTimerItem->run) {
		tc->next = firstTimerItem;
		tc->prev = NULL;
		firstTimerItem->prev = tc;
		firstTimerItem = tc;
		return tc->tid;
	}
	for (temp = firstTimerItem; temp->next; temp = temp->next) {
		if (temp->next) {
			if (temp->next->run <= timetorun)
				continue;
		}
		tc->next = temp->next;
		temp->next = tc;
		tc->prev = temp;
		if (tc->next)
			tc->next->prev = tc;
		return tc->tid;
	}

/*
 * Last entry 
 */
	tc->next = NULL;
	tc->prev = temp;
	temp->next = tc;

	return tc->tid;
}

/**
 * \param tid Identifier of the timer to cancel
 * \brief Cancels a timer event
 *
 * Destroys a timer prematurely, preventing the timed event from firing 
 * later
 */
int
cancel_timer(int tid)
{
	struct t_chain *temp;
	time_t curtime;

	curtime = time(NULL);

	if (!firstTimerItem)
		return -1;

	for (temp = firstTimerItem; temp; temp = temp->next) {
		if (temp->tid == tid) {	/* match */
			if (temp->prev)
				temp->prev->next = temp->next;
			if (temp->next)
				temp->next->prev = temp->prev;
			if (temp == firstTimerItem)
				firstTimerItem = temp->next;

			FREE(temp);
			return 0;
		}
	}
	return -1;
}

/**
 * \brief Check for expired timers
 *
 * Checks for timers that are expired, runs the specified
 * function on those timers and removes them from the timer list.
 * 
 * \sideffect inherits side-effects from any function placed on a timer
 */
void
timeralarm(void)
{
	struct t_chain *tc;
	time_t curtime;

	curtime = time(NULL);

	/*
	 * This is called when sigalarm is raised.. first, reset the signal 
	 */
	tc = firstTimerItem;

	if (firstTimerItem == NULL)
		return;
	while (firstTimerItem->run <= curtime) {
		firstTimerItem->func(firstTimerItem->args);
		firstTimerItem = firstTimerItem->next;
		FREE(firstTimerItem->prev);
		firstTimerItem->prev = NULL;
	}
}

/**
 * \param from Nickname that timer list is to be sent to
 * \brief Reports the list of timers
 *
 * Causes services to transmit the status of the timer list
 * to the specified nickname.
 *
 * This is part of the debug command /OPERSERV TIMERS
 */
void
dumptimer(char *from)
{
	struct t_chain *tc;
	char temp[IRCBUF] = "\0";
	int i, num;
	time_t curtime;

	curtime = time(NULL);

	if (!firstTimerItem) {
		sSend(":%s NOTICE %s :No timers right now!", OperServ, from);
		return;
	}
	num = sizeof(tt) / sizeof(struct t_table);
	sSend(":%s NOTICE %s :tid secondsleft function             args",
		  OperServ, from);
	for (tc = firstTimerItem; tc; tc = tc->next) {
		for (i = 0; i < num; i++) {
			if (tc->func == tt[i].func) {
				strcpy(temp, tt[i].name);
				break;
			} else
				(*temp = '\0');
		}
		if (*temp == '\0')
			sprintf(temp, "%p", tc->func);
		sSend(":%s NOTICE %s :%-4ld %-11ld %-20s %s", OperServ, from,
			  tc->tid, tc->run - curtime, temp, tc->args);
	}
}
