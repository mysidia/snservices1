/****************************************************************************
 *  Userload module by Michael L. VanLoon (mlv) <michaelv@iastate.edu>
 *  Written 2/93.  Originally grafted into irc2.7.2g 4/93.
 *
 *   IRC - Internet Relay Chat, ircd/userload.c
 *   Copyright (C) 1990 University of Oulu, Computing Center
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
 ****************************************************************************/

#include "ircd.h"

#include "userload.h"
#include <sys/time.h>
#include <sys/errno.h>
#include <string.h>
#include <signal.h>
#include <sys/resource.h>

IRCD_RCSID("$Id$");

struct current_load_struct current_load_data;
struct load_entry *load_list_head = NULL, *load_list_tail = NULL,
                  *load_free_head = NULL, *load_free_tail = NULL;


void update_load()
{
  static struct timeval now, last;
  struct load_entry *cur_load_entry;

  /* This seems to get polluted on startup by an exit_client()
   * before any connections have been recorded.
   */
  if (current_load_data.client_count > MAXCONNECTIONS ||
      current_load_data.conn_count > MAXCONNECTIONS)
    bzero(&current_load_data, sizeof(struct current_load_struct));
  
  memcpy(&last, &now, sizeof(struct timeval));
  if (gettimeofday(&now, NULL) != 0)
    return;  /* error getting time of day--can't calculate time diff */

  if (load_free_tail == NULL) {
    if ((cur_load_entry = irc_malloc(sizeof(struct load_entry))) == NULL)
      return;
    /* printf("malloc pointer: %x\n", cur_load_entry); */
  } else {
    cur_load_entry = load_free_tail;
    load_free_tail = cur_load_entry->prev;
    if (load_free_tail == NULL)
      load_free_head = NULL;
    /* printf("free pointer: %x\n", cur_load_entry); */
  }
  if (load_list_tail != NULL) {
    cur_load_entry->time_incr = ((now.tv_sec * 1000 + now.tv_usec / 1000 + 5)
	   - (last.tv_sec * 1000 + last.tv_usec / 1000)) / 10;
    cur_load_entry->client_count = current_load_data.client_count;
    cur_load_entry->conn_count = current_load_data.conn_count;
  } else {
    load_list_head = cur_load_entry;
    bzero(cur_load_entry, sizeof(struct load_entry));
    cur_load_entry->time_incr = 1;
  }
  cur_load_entry->prev = load_list_tail;
  load_list_tail = cur_load_entry;
}


void calc_load(aClient *sptr, char *parv) /* we only get passed the original parv[0] */
{
  struct load_entry *cur_load_entry;
  struct load_entry *last = NULL;
  u_long secs = 0, adj_secs, total[2], adj[2];/*[client,conn]*/
  int i, times[5][2]; /* [min,hour,day,Yest,YYest][client,conn] */
  char what[2][HOSTLEN + 1 + 10];

  bzero(total, 2 * sizeof(u_long));
  current_load_data.entries = 0;

  update_load();  /* we want stats accurate as of *now* */

  for (cur_load_entry = load_list_tail; (secs < 6000) &&
       (cur_load_entry != NULL); cur_load_entry = cur_load_entry->prev) {
    u_long time_incr = cur_load_entry->time_incr;
    total[0] += time_incr * cur_load_entry->client_count;
    total[1] += time_incr * cur_load_entry->conn_count;
    last = cur_load_entry;
    secs += cur_load_entry->time_incr;
    current_load_data.entries++;
  }
  if ((secs > 6000) && (last != NULL)) {
    adj_secs = secs - 6000;
    adj[0] = adj_secs * last->client_count;
    adj[1] = adj_secs * last->conn_count;
  } else
    adj_secs = adj[0] = adj[1] = 0;
  for (i = 0; i < 2; i++) {
    times[0][i] = ((total[i] - adj[i]) * 1000 / (secs - adj_secs) + 5) / 10;
  }

  secs = (secs + 5) / 10;
  for (i = 0; i < 2; i++)
    total[i] = (total[i] + 5) / 10;

  for ( ; (secs < 36000) && (cur_load_entry != NULL); secs +=
       (cur_load_entry->time_incr + 5) / 10, cur_load_entry =
       cur_load_entry->prev, current_load_data.entries++) {
    u_long time_incr = (cur_load_entry->time_incr + 5) / 10;
    total[0] += time_incr * cur_load_entry->client_count;
    total[1] += time_incr * cur_load_entry->conn_count;
    last = cur_load_entry;
  }
  if ((secs > 36000) && (last != NULL)) {
    adj_secs = secs - 36000;
    adj[0] = adj_secs * last->client_count;
    adj[1] = adj_secs * last->conn_count;
  } else
    adj_secs = adj[0] = adj[1] = 0;
  for (i = 0; i < 2; i++) {
    times[1][i] = ((total[i] - adj[i]) * 100 / (secs - adj_secs) + 5) / 10;
  }

  secs = (secs + 5) / 10;
  for (i = 0; i < 2; i++)
    total[i] = (total[i] + 5) / 10;

  for ( ; (secs < 86400) && (cur_load_entry != NULL); secs +=
       (cur_load_entry->time_incr + 50) / 100, cur_load_entry =
       cur_load_entry->prev, current_load_data.entries++) {
    u_long time_incr = (cur_load_entry->time_incr + 50) / 100;
    total[0] += time_incr * cur_load_entry->client_count;
    total[1] += time_incr * cur_load_entry->conn_count;
    last = cur_load_entry;
  }
  if ((secs > 86400) && (last != NULL)) {
    adj_secs = secs - 86400;
    adj[0] = adj_secs * last->client_count;
    adj[1] = adj_secs * last->conn_count;
  } else
    adj_secs = adj[0] = adj[1] = 0;
  for (i = 0; i < 2; i++) {
    times[2][i] = ((total[i] - adj[i]) * 10 / (secs - adj_secs) + 5) / 10;
  }

  bzero(total, 2 * sizeof(u_long));

  for (secs = 1 ; (secs < 86400) && (cur_load_entry != NULL); secs +=
       (cur_load_entry->time_incr + 50) / 100, cur_load_entry =
       cur_load_entry->prev, current_load_data.entries++) {
    u_long time_incr = (cur_load_entry->time_incr + 50) / 100;
    total[0] += time_incr * cur_load_entry->client_count;
    total[1] += time_incr * cur_load_entry->conn_count;
    last = cur_load_entry;
  }
  if ((secs > 86400) && (last != NULL)) {
    adj_secs = secs - 86400;
    adj[0] = adj_secs * last->client_count;
    adj[1] = adj_secs * last->conn_count;
  } else
    adj_secs = adj[0] = adj[1] = 0;
  for (i = 0; i < 2; i++) {
    times[3][i] = ((total[i] - adj[i]) * 10 / (secs - adj_secs) + 5) / 10;
  }

  bzero(total, 2 * sizeof(u_long));

  for (secs = 1 ; (secs < 86400) && (cur_load_entry != NULL); secs +=
       (cur_load_entry->time_incr + 50) / 100, cur_load_entry =
       cur_load_entry->prev, current_load_data.entries++) {
    u_long time_incr = (cur_load_entry->time_incr + 50) / 100;
    total[0] += time_incr * cur_load_entry->client_count;
    total[1] += time_incr * cur_load_entry->conn_count;
    last = cur_load_entry;
  }
  if ((secs > 86400) && (last != NULL)) {
    adj_secs = secs - 86400;
    adj[0] = adj_secs * last->client_count;
    adj[1] = adj_secs * last->conn_count;
  } else
    adj_secs = adj[0] = adj[1] = 0;
  for (i = 0; i < 2; i++) {
    times[4][i] = ((total[i] - adj[i]) * 10 / (secs - adj_secs) + 5) / 10;
  }

  if ((cur_load_entry != NULL) && (cur_load_entry->prev != NULL) &&
      (secs > 86400)) {  /* have nodes to free -- more than 3 days old */
    struct load_entry *cur_free_entry = load_free_head;

    load_free_head = load_list_head;
    load_list_head = cur_load_entry;
    if (cur_free_entry != NULL)
      cur_free_entry->prev = cur_load_entry->prev;
    else
      load_free_tail = cur_load_entry->prev;

    /* printf("freeing: %x  (head: %x,  tail: %x)\n", cur_load_entry->prev,
	   load_free_head, load_free_tail); */

    cur_load_entry->prev = NULL;
  }

  strcpy(what[0], "total clients");
  strcpy(what[1], "total connections");
  sendto_one(sptr,
    ":%s NOTICE %s :Minute   Hour  Day  Yest.  YYest.  Userload for:",
    me.name, parv);
  for (i = 0; i < 2; i++)
    sendto_one(sptr,
      ":%s NOTICE %s :%3d.%02d  %3d.%01d  %3d   %3d     %3d   %s",
      me.name, parv, times[0][i] / 100, times[0][i] % 100, times[1][i] / 10,
      times[1][i] % 10, times[2][i], times[3][i], times[4][i], what[i]);
}


void initload()
{
  bzero(&current_load_data, sizeof(struct current_load_struct));
  update_load();  /* Initialize the load list */
}
