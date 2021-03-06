/************************************************************************
 *   IRC - Internet Relay Chat, include/h.h
 *   Copyright (C) 1992 Darren Reed
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

#include "ircd/memory.h"

VOIDSIG s_die(int);

extern	time_t	nextconnect, nextdnscheck, nextping;
extern	aClient	*client, me;
extern	aChannel *channel;
extern	struct	stats	*ircstp;
extern	int	bootopt;
extern	char	stripped[BUFSIZE];

extern int expr_match (const char *pattern, const char *text);
time_t	check_pings(time_t now, int check_kills, aConfItem *conf_target);

extern void dup_sup_version(anUser* user, const char* text);
extern void free_sup_version(char* text);

aChannel *find_channel(char *, aChannel *);
void	remove_user_from_channel(aClient *, aChannel *);
void	del_invite(aClient *, aChannel *);
int	del_silence(aClient *, char *);
void	send_user_joins(aClient *, aClient *);
void	clean_channelname(char *);
int	can_send(aClient *, aChannel *);
int	is_chan_op(aClient *, aChannel *);
int	has_voice(aClient *, aChannel *);
int	count_channels(aClient *);
Link    *is_banned(aClient *, aChannel *, int *);

aClient	*find_client(char *, aClient *);
aClient	*find_name(char *, aClient *);
aClient	*find_person(char *, aClient *);
aClient	*find_server(char *, aClient *);

int	conf_xbits(aConfItem *aconf, char *field);
int	attach_conf(aClient *, aConfItem *);
aConfItem *attach_confs(aClient*, char *, int);
aConfItem *attach_confs_host(aClient*, char *, int);
int	attach_Iline(aClient *, struct HostEnt *, char *);
aConfItem *conf, *find_me(void), *find_admin(void);
aConfItem *count_cnlines(Link *);
void	det_confs_butmask(aClient *, int);
int	detach_conf(aClient *, aConfItem *);
aConfItem *det_confs_butone(aClient *, aConfItem *);
aConfItem *find_conf(Link *, char*, int);
aConfItem *find_conf_exact(char *, char *, char *, int);
aConfItem *find_conf_host(Link *, char *, int);
aConfItem *find_socksline_host(char *host);
aConfItem *find_iline_host(char *);
aConfItem *find_conf_ip(Link *, sock_address *, char *, int);
aConfItem *find_conf_name(char *, int);
aConfItem *find_temp_conf_entry(aConfItem *, u_int);
aConfItem *find_conf_servern(char *);
int	find_kill(aClient *, aConfItem *);
char	*find_zap(aClient *, int);
char    *find_sup_zap(aClient *, int);
int	find_restrict(aClient *);
int	rehash(aClient *, aClient *, int);
void	conf_init();
aConfItem *add_temp_conf(unsigned int status, char *host, char *passwd, char *name, int port, int temp);

extern	char	*network, *kline, *network_kline, *help_url;

char	*debugmode, *configfile, *sbrk0;
char	*getfield(char *);
void	get_sockhost(aClient *, char *);
char	*rpl_str(int);
char *err_str(int);
char	*strerror(int);
int	dgets(int, char *, int);
void boot_replies(void);

extern	int	dbufalloc, dbufblocks, debuglevel;
extern	int	debuglevel, portnum, debugtty, maxusersperchannel;
extern	int	readcalls, resfd;

aClient	*add_connection(aClient *, sock *);
aClient	*add_listener(aConfItem *);
void	add_local_domain(char *, int);
int	check_client(aClient *);
int	check_server(aClient *, struct HostEnt *, \
				    aConfItem *, aConfItem *, int);
int	check_server_init(aClient *);
void	close_connection(aClient *);
void	close_listeners(void);
int connect_server(aConfItem *, aClient *, struct HostEnt *);
int	completed_connection(aClient *);
void flush_connections(aClient*);
void	get_my_name(aClient *, char *, int);
int	get_sockerr(aClient *);
void	init_sys(void);
int	read_message(time_t);
int	read_packet(aClient *cptr);

void	do_dns_async();

void	restart(char *);
void	send_channel_modes(aClient *, aChannel *);
void	server_reboot(char *);
void	terminate(void), write_pidfile(void);

int	send_queued(aClient *);
int	tolog( int logtype, char *fmt, ... );
void close_logs(void);
void open_logs(void);
void msgtab_buildhash(void);
void open_checkport(void);

extern	int	writecalls, writeb[];
int	deliver_it(aClient *, char *, int);
void perform_mask(aClient *acptr, int v);

int	check_registered(aClient *);
int	check_registered_user(aClient *);
char	*get_client_name(aClient *, int);
char	*get_client_name3(aClient *, int);
char	*get_client_host(aClient *);
char	*get_client_name_mask(aClient *, int, int, int);
char	*get_client_namp(char *, aClient *, int, int);
	int remove_hurt(aClient *acptr);
	int set_hurt(aClient *acptr, const char *from, int ht);

char	*my_name_for_link(char *, aConfItem *);
char	*myctime(time_t), *date(time_t);
int	exit_client(aClient *, aClient *, aClient *, char *);
void	initstats(void), tstats(aClient *, char *);

int	parse(aClient *, char *, char *, struct Message *);
int	do_numeric(int, aClient *, aClient *, int, char **);
int hunt_server(aClient *,aClient *,char *,int,int,char **);
aClient	*next_client(aClient *, char *);
int	msg_has_colors(const char *);
void	strip_colors(char *, const char *);
int	m_umode(aClient *, aClient *, int, char **);
int	m_names(aClient *, aClient *, int, char **);
int	m_server_estab(aClient *);
void	send_umode(aClient *, aClient *, aClient *, int, int, char *);
void	send_umode_out(aClient*, aClient *, aClient *, int);
char	*genHostMask(char *host);
char	*genHostMask2(char *host);

void	free_client(aClient *);
void	free_link(Link *);
void delist_conf(aConfItem *aconf);
void	free_conf(aConfItem *);
void	free_user(anUser *, aClient *);
Link	*make_link(void);
anUser	*make_user(aClient *);
aConfItem *make_conf(void);
aServer	*make_server(aClient *);
aClient	*make_client(aClient *);
Link	*find_user_link(Link *, aClient *);
int	IsMember(aClient *, aChannel *);
char	*pretty_mask(char *, int);
void	add_client_to_list(aClient *);
void	remove_client_from_list(aClient *);
void	initlists(void);

struct	HostEnt	*get_res(char *);

struct	HostEnt	*gethost_byaddr(sock_address *, Link *);
struct	HostEnt	*gethost_byname(char *, int, Link *);
void	flush_cache(void);
int	resolver_init();
time_t	timeout_query_list(time_t);
time_t	expire_cache(time_t);
void    del_queries(char *);

void	clear_channel_hash_table(void);
void	clear_client_hash_table(void);
int	add_to_client_hash_table(char *, aClient *);
int	del_from_client_hash_table(char *, aClient *);
int	add_to_channel_hash_table(char *, aChannel *);
int	del_from_channel_hash_table(char *, aChannel *);
aChannel *hash_find_channel(char *, aChannel *);
aClient	*hash_find_client(char *, aClient *);
aClient	*hash_find_nickserver(char *, aClient *);
aClient	*hash_find_server(char *, aClient *);

void	add_history(aClient *);
aClient	*get_history(char *, time_t);
void	initwhowas(void);
void	off_history(aClient *);

int	dopacket(aClient *, char *, int);
void dumpcore(char *msg, ...);

void	debug(int level, char *form, ...);
#if defined(DEBUGMODE)
void	send_usage(aClient *, char *);
void	send_listinfo(aClient *, char *);
#endif
void	count_memory(aClient *, char *);

void       count_watch_memory(int *, u_long *);
void       clear_watch_hash_table(void);
int        add_to_watch_hash_table(char *, aClient *);
int        del_from_watch_hash_table(char *, aClient *);
int        hash_check_watch(aClient *, int);
int        hash_del_watch_list(aClient  *);
aWatch    *hash_get_watch(char *);
#define MAXWATCH       128

void free_str_list(Link *);
int find_str_match_link(Link **, char *str);
void send_list(aClient *, int);

u_long cres_mem(aClient *);


/*
 * ahurt stuff
 */
aConfItem	*find_ahurt(aClient *);

/*
 * VCcheck stuff
 */
int FailClientCheck(aClient* sptr);
