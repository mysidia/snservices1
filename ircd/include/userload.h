/****************************************************************************
 *  Userload module by Michael L. VanLoon (mlv) <michaelv@iastate.edu>
 *  Written 2/93.  Originally grafted into irc2.7.2g 4/93.
 *
 *   IRC - Internet Relay Chat, ircd/userload.h
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

struct current_load_struct {
  u_short client_count, conn_count;
  u_long  entries;
};  

extern struct current_load_struct current_load_data;

struct load_entry {
  struct  load_entry *prev;
  u_short client_count, conn_count;
  long    time_incr;
};

extern struct load_entry *load_list_head, *load_list_tail,
                         *load_free_head, *load_free_tail;


extern void initload(void);
extern void update_load(void);
extern void calc_load(aClient *, char *);
