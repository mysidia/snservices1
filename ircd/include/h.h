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

/*
 * "h.h". - Headers file.
 *
 * Most of the externs and prototypes thrown in here to 'cleanup' things.
 * -avalon
 */

extern	time_t	nextconnect, nextdnscheck, nextping;
extern	aClient	*client, me, *local[];
extern	aChannel *channel;
extern	struct	stats	*ircstp;
extern	int	bootopt;
/* Prototype added to force errors -- Barubary */
extern	time_t	check_pings(time_t now, int check_kills, aConfItem *conf_target);
#ifdef _WIN32
extern	void	*hCio;
#endif

extern int expr_match (const char *pattern, const char *text);

extern void dup_sup_version(anUser* user, const char* text);
extern void free_sup_version(char* text);

extern	aChannel *find_channel PROTO((char *, aChannel *));
extern	void	remove_user_from_channel PROTO((aClient *, aChannel *));
extern	void	del_invite PROTO((aClient *, aChannel *));
extern	int	del_silence PROTO((aClient *, char *));
extern	void	send_user_joins PROTO((aClient *, aClient *));
extern	void	clean_channelname PROTO((char *));
extern	int	can_send PROTO((aClient *, aChannel *));
extern	int	is_chan_op PROTO((aClient *, aChannel *));
extern	int	is_zombie PROTO((aClient *, aChannel *));
extern	int	has_voice PROTO((aClient *, aChannel *));
extern	int	count_channels PROTO((aClient *));
extern  Link    *is_banned PROTO((aClient *, aChannel *, int *));
extern	int	parse_help PROTO((aClient *, char *, char *));

extern	aClient	*find_client PROTO((char *, aClient *));
extern	aClient	*find_name PROTO((char *, aClient *));
extern	aClient	*find_person PROTO((char *, aClient *));
extern	aClient	*find_server PROTO((char *, aClient *));
extern	aClient	*find_service PROTO((char *, aClient *));
extern	aClient	*find_userhost PROTO((char *, char *, aClient *, int *));

extern	int	conf_xbits(aConfItem *aconf, char *field);
extern	int	attach_conf PROTO((aClient *, aConfItem *));
extern	aConfItem *attach_confs PROTO((aClient*, char *, int));
extern	aConfItem *attach_confs_host PROTO((aClient*, char *, int));
extern	int	attach_Iline PROTO((aClient *, struct hostent *, char *));
extern	aConfItem *conf, *find_me PROTO(()), *find_admin PROTO(());
extern	aConfItem *count_cnlines PROTO((Link *));
extern	void	det_confs_butmask PROTO((aClient *, int));
extern	int	detach_conf PROTO((aClient *, aConfItem *));
extern	aConfItem *det_confs_butone PROTO((aClient *, aConfItem *));
extern	aConfItem *find_conf PROTO((Link *, char*, int));
extern	aConfItem *find_conf_exact PROTO((char *, char *, char *, int));
extern	aConfItem *find_conf_host PROTO((Link *, char *, int));
extern  aConfItem *find_socksline_host(char *host);
extern	aConfItem *find_iline_host PROTO((char *));
extern	aConfItem *find_conf_ip PROTO((Link *, char *, char *, int));
extern	aConfItem *find_conf_name PROTO((char *, int));
extern  aConfItem *find_temp_conf_entry PROTO((aConfItem *, u_int));
extern  aConfItem *find_conf_servern PROTO((char *));
extern	int	find_kill(aClient *, aConfItem*);
extern  char    *find_sup_zap PROTO((aClient *, int));
extern	char	*find_zap PROTO((aClient *, int));
extern	int	find_restrict PROTO((aClient *));
extern	int	rehash PROTO((aClient *, aClient *, int));
extern	int	initconf PROTO((int));
extern	aConfItem *add_temp_conf(unsigned int status, char *host, char *passwd, char *name, int port, int class, int temp);

extern	char	*MyMalloc PROTO((int)), *MyRealloc PROTO((char *, int));
extern	char	*debugmode, *configfile, *sbrk0;
extern	char	*getfield PROTO((char *));
extern	void	get_sockhost PROTO((aClient *, char *));
extern	char	*rpl_str PROTO((int)), *err_str PROTO((int));
extern	char	*strerror PROTO((int));
extern	int	dgets PROTO((int, char *, int));
extern	char	*inetntoa PROTO((char *));

#ifdef _WIN32
extern	int	dbufalloc, dbufblocks, debuglevel;
#else
extern	int	dbufalloc, dbufblocks, debuglevel, errno, h_errno;
#endif
extern	int	highest_fd, debuglevel, portnum, debugtty, maxusersperchannel;
extern	int	readcalls, udpfd, resfd;
extern	aClient	*add_connection PROTO((aClient *, int));
extern	int	add_listener PROTO((aConfItem *));
extern	void	add_local_domain PROTO((char *, int));
extern	int	check_client PROTO((aClient *));
extern	int	check_server PROTO((aClient *, struct hostent *, \
				    aConfItem *, aConfItem *, int));
extern	int	check_server_init PROTO((aClient *));
extern	void	close_connection PROTO((aClient *));
extern	void	close_listeners PROTO(());
extern	int connect_server PROTO((aConfItem *, aClient *, struct hostent *));
extern	void	get_my_name PROTO((aClient *, char *, int));
extern	int	get_sockerr PROTO((aClient *));
extern	int	inetport PROTO((aClient *, char *, int));
extern	void	init_sys PROTO(());
extern	int	read_message PROTO((time_t));
extern	void	report_error PROTO((char *, aClient *));
extern	void	set_non_blocking PROTO((int, aClient *));
extern	int	setup_ping PROTO(());
extern	void	summon PROTO((aClient *, char *, char *, char *));
extern	int	unixport PROTO((aClient *, char *, int));
extern	int	utmp_open PROTO(());
extern	int	utmp_read PROTO((int, char *, char *, char *, int));
extern	int	utmp_close PROTO((int));

extern	void	start_auth PROTO((aClient *));
extern	void	read_authports PROTO((aClient *));
extern	void	send_authports PROTO((aClient *));

extern	void	restart PROTO((char *));
extern	void	send_channel_modes PROTO((aClient *, aChannel *));
extern	void	server_reboot PROTO((char *));
extern	void	terminate PROTO(()), write_pidfile PROTO(());

extern	int	send_queued PROTO((aClient *));
extern	int	tolog( int logtype, char *fmt, ... );

/*VARARGS2*/
extern	void	sendto_one();
/*VARARGS4*/
extern	void	sendto_channel_butone();
extern	void	sendto_channelops_butone();
extern	void	sendto_channelvoices_butone();
/*VARARGS2*/
extern	void	sendto_serv_butone();
/*VARARGS2*/
extern	void	sendto_common_channels();
/*VARARGS3*/
extern	void	sendto_channel_butserv();
/*VARARGS3*/
extern	void	sendto_match_servs();
/*VARARGS5*/
extern	void	sendto_match_butone();
/*VARARGS3*/
extern	void	sendto_all_butone();
/*VARARGS1*/
extern	void	sendto_ops();
/*VARARGS3*/
extern	void	sendto_ops_butone();
/*VARARGS3*/
extern	void	sendto_ops_butme();
/*VARARGS3*/
extern	void	sendto_prefix_one();
/*VARARGS3*/
extern  void    sendto_failops_whoare_opers();
/*VARARGS3*/
extern  void    sendto_failops();
/*VARARGS3*/
extern  void    sendto_opers();
/*VARARGS?*/
extern	void	sendto_flag();
/*VARARGS?*/
extern	void	sendto_flag_norep();
/*VARARGS?*/
extern	void	sendto_umode();
/*VARARGS?*/
extern	void	sendto_socks();

extern	int	writecalls, writeb[];
extern	int	deliver_it PROTO((aClient *, char *, int));

extern	int	check_registered PROTO((aClient *));
extern	int	check_registered_user PROTO((aClient *));
extern	char	*get_client_name PROTO((aClient *, int));
extern	char	*get_client_host PROTO((aClient *));
extern	char	*get_client_name_mask PROTO((aClient *, int, int, int));
extern	char	*get_client_namp PROTO((char *, aClient *, int, int));
extern 	int remove_hurt(aClient *acptr);
extern 	int set_hurt(aClient *acptr, const char *from, int ht);

extern	char	*my_name_for_link PROTO((char *, aConfItem *));
extern	char	*myctime PROTO((time_t)), *date PROTO((time_t));
extern	int	exit_client PROTO((aClient *, aClient *, aClient *, char *));
extern	void	initstats PROTO(()), tstats PROTO((aClient *, char *));

extern	int	parse PROTO((aClient *, char *, char *, struct Message *));
extern	int	do_numeric PROTO((int, aClient *, aClient *, int, char **));
extern	int hunt_server PROTO((aClient *,aClient *,char *,int,int,char **));
extern	aClient	*next_client PROTO((aClient *, char *));
extern	int	m_umode PROTO((aClient *, aClient *, int, char **));
extern	int	m_names PROTO((aClient *, aClient *, int, char **));
extern	int	m_server_estab PROTO((aClient *));
extern	void	send_umode PROTO((aClient *, aClient *, aClient *, int, int, char *));
extern	void	send_umode_out PROTO((aClient*, aClient *, aClient *, int));
extern	char	*genHostMask(char *host);
extern	char	*genHostMask2(char *host);

extern	void	free_client PROTO((aClient *));
extern	void	free_link PROTO((Link *));
extern	void	free_conf PROTO((aConfItem *));
extern	void	free_class PROTO((aClass *));
extern	void	free_user PROTO((anUser *, aClient *));
extern	Link	*make_link PROTO(());
extern	anUser	*make_user PROTO((aClient *));
extern	aConfItem *make_conf PROTO(());
extern	aClass	*make_class PROTO(());
extern	aServer	*make_server PROTO(());
extern	aClient	*make_client PROTO((aClient *));
extern	int	free_socks PROTO((struct Socks *zap));
extern	aSocks	*make_socks PROTO((aClient *to));
extern	Link	*find_user_link PROTO((Link *, aClient *));
extern	int	IsMember PROTO((aClient *, aChannel *));
extern	char	*pretty_mask PROTO((char *, int));
extern	void	add_client_to_list PROTO((aClient *));
extern	void	checklist PROTO(());
extern	void	remove_client_from_list PROTO((aClient *));
extern	void	initlists PROTO(());

extern	void	add_class PROTO((int, int, int, int, long));
extern	void	fix_class PROTO((aConfItem *, aConfItem *));
extern	long	get_sendq PROTO((aClient *));
extern	int	get_con_freq PROTO((aClass *));
extern	int	get_client_ping PROTO((aClient *));
extern	int	get_client_class PROTO((aClient *));
extern	int	get_conf_class PROTO((aConfItem *));
extern	void	report_classes PROTO((aClient *));

#ifndef _WIN32
extern	struct	hostent	*get_res PROTO((char *));
#endif

extern	struct	hostent	*gethost_byaddr PROTO((char *, Link *));
extern	struct	hostent	*gethost_byname PROTO((char *, Link *));
extern	void	flush_cache PROTO(());
extern	int	init_resolver PROTO((int));
extern	time_t	timeout_query_list PROTO((time_t));
extern	time_t	expire_cache PROTO((time_t));
extern	void    del_queries PROTO((char *));

extern	void	clear_channel_hash_table PROTO(());
extern	void	clear_client_hash_table PROTO(());
extern	int	add_to_client_hash_table PROTO((char *, aClient *));
extern	int	del_from_client_hash_table PROTO((char *, aClient *));
extern	int	add_to_channel_hash_table PROTO((char *, aChannel *));
extern	int	del_from_channel_hash_table PROTO((char *, aChannel *));
extern	aChannel *hash_find_channel PROTO((char *, aChannel *));
extern	aClient	*hash_find_client PROTO((char *, aClient *));
extern	aClient	*hash_find_nickserver PROTO((char *, aClient *));
extern	aClient	*hash_find_server PROTO((char *, aClient *));

extern	void	add_history PROTO((aClient *));
extern	aClient	*get_history PROTO((char *, time_t));
extern	void	initwhowas PROTO(());
extern	void	off_history PROTO((aClient *));

extern	int	dopacket PROTO((aClient *, char *, int));

extern void send_socksquery (aSocks *);
extern void read_socks (aSocks *);

extern const char* safe_info(int hideIp, aClient* acptr);


/*VARARGS2*/
extern	void	debug();
#if defined(DEBUGMODE)
extern	void	send_usage PROTO((aClient *, char *));
extern	void	send_listinfo PROTO((aClient *, char *));
extern	void	count_memory PROTO((aClient *, char *));
#endif

extern void       count_watch_memory(int *, u_long *);
extern void       clear_watch_hash_table(void);
extern int        add_to_watch_hash_table(char *, aClient *);
extern int        del_from_watch_hash_table(char *, aClient *);
extern int        hash_check_watch(aClient *, int);
extern int        hash_del_watch_list(aClient  *);
extern aWatch    *hash_get_watch(char *);
#define MAXWATCH       128


int parse_help (aClient *sptr, char *name, char *help);
void free_str_list(Link *);
int find_str_match_link(Link **, char *str);
void send_list(aClient *, int);

char *crule_parse PROTO((char *));
int crule_eval PROTO((char *));
void crule_free PROTO((char **));
void helpop_ignore(char *);
void helpop_unignore(int);
int helpop_ignored(aClient *);
extern int nhtopics;
extern aHelptopic **htopics;
extern int h_nignores;
extern char **h_ignores;


/*
 * ahurt stuff
 */
aConfItem	*find_ahurt(aClient *);

/*
 * VCcheck stuff
 */
int FailClientCheck(aClient* sptr);
