/************************************************************************
 *   IRC - Internet Relay Chat, include/msg.h
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
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

#ifndef	__msg_include__
#define __msg_include__

#define MSG_PRIVATE  "PRIVMSG"	/* PRIV */
#define MSG_WHO      "WHO"	/* WHO  -> WHOC */
#define MSG_WHOIS    "WHOIS"	/* WHOI */
#define MSG_WHOWAS   "WHOWAS"	/* WHOW */
#define MSG_USER     "USER"	/* USER */
#define MSG_NICK     "NICK"	/* NICK */
#define MSG_SERVER   "SERVER"	/* SERV */
#define MSG_LIST     "LIST"	/* LIST */
#define MSG_TOPIC    "TOPIC"	/* TOPI */
#define MSG_INVITE   "INVITE"	/* INVI */
#define MSG_VERSION  "VERSION"	/* VERS */
#define MSG_QUIT     "QUIT"	/* QUIT */
#define MSG_SQUIT    "SQUIT"	/* SQUI */
#define MSG_KILL     "KILL"	/* KILL */
#define MSG_HURT     "HURT"	/* HURT */
#define MSG_HEAL     "HEAL"	/* HEAL */
#define MSG_HUSH     "HUSH"	/* HUSH - HURT */
#define MSG_UNHUSH   "UNHUSH"	/* UNHUSH - HEAL */
#define MSG_INFO     "INFO"	/* INFO */
#define MSG_LINKS    "LINKS"	/* LINK */
#define MSG_STATS    "STATS"	/* STAT */
#define MSG_USERS    "USERS"	/* USER -> USRS */
#define MSG_ERROR    "ERROR"	/* ERRO */
#define MSG_AWAY     "AWAY"	/* AWAY */
#define MSG_CONNECT  "CONNECT"	/* CONN */
#define MSG_PING     "PING"	/* PING */
#define MSG_PONG     "PONG"	/* PONG */
#define MSG_OPER     "OPER"	/* OPER */
#define MSG_PASS     "PASS"	/* PASS */
#define MSG_WALLOPS  "WALLOPS"	/* WALL */
#define MSG_TIME     "TIME"	/* TIME */
#define MSG_NAMES    "NAMES"	/* NAME */
#define MSG_ADMIN    "ADMIN"	/* ADMI */
#define MSG_TRACE    "TRACE"	/* TRAC */
#define MSG_NOTICE   "NOTICE"	/* NOTI */
#define MSG_JOIN     "JOIN"	/* JOIN */
#define MSG_PART     "PART"	/* PART */
#define MSG_LUSERS   "LUSERS"	/* LUSE */
#define MSG_MOTD     "MOTD"	/* MOTD */
#define MSG_MODE     "MODE"	/* MODE */
#define MSG_KICK     "KICK"	/* KICK */
#define MSG_USERHOST "USERHOST"	/* USER -> USRH */
#define MSG_ISON     "ISON"	/* ISON */
#define	MSG_REHASH   "REHASH"	/* REHA */
#define	MSG_RESTART  "RESTART"	/* REST */
#define	MSG_CLOSE    "CLOSE"	/* CLOS */
#define	MSG_DIE	     "DIE"
#define	MSG_HASH     "HASH"	/* HASH */
#define	MSG_DNS      "DNS"	/* DNS  -> DNSS */
#define MSG_SILENCE  "SILENCE"  /* SILE */
#define MSG_AHURT    "AHURT"    /* ahurt */
#define MSG_AKILL    "AKILL"	/* AKILL */
#define MSG_KLINE    "KLINE"	/* KLINE */
#define MSG_ZLINE    "ZLINE"	/* ZLINE */
#define MSG_UNZLINE  "UNZLINE"	/* UNZLINE */
#define MSG_UNKLINE  "UNKLINE"  /* UNKLINE */
#define MSG_RAHURT   "RAHURT"	/* RAHURT */
#define MSG_RAKILL   "RAKILL"   /* RAKILL */
#define MSG_GNOTICE  "GNOTICE"  /* GNOTICE */
#define MSG_GOPER    "GOPER"    /* GOPER */
#define MSG_GLOBOPS  "GLOBOPS"  /* GLOBOPS */
#define MSG_LOCOPS   "LOCOPS"   /* LOCOPS */
#define	MSG_OPMODE   "OPMODE"   /* OPMODE */
#define MSG_CNICK    "CNICK"    /* CNICK */
#define MSG_MLOCK    "MLOCK"    /* MLOCK */
#define MSG_HURTSET  "HURTSET"	/* HURTSET */
#define MSG_SHOWMASK "SHOWMASK" /* SHOWMASK */
#define MSG_LOG	     "LOG"	/* LOG */
#define MSG_WATCH    "WATCH"	/* WATCH */
#define MSG_SHOWCON  "SHOWCON" 
 
#define MSG_NICKSERV "NickServ"
#define MSG_CHANSERV "ChanServ"
#define MSG_OPERSERV "OperServ"
#define MSG_MEMOSERV "MemoServ"
#define MSG_INFOSERV "InfoServ"
#define MSG_GAMESERV "GameServ"
#define MSG_IDENT    "IDENTIFY"
#define MSG_SERVICE  "SERVICES"

#define MAXPARA    15 

int m_cnick(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_private(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_topic(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_join(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_part(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_mode(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_ping(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_pong(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_wallops(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_kick(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_nick(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_error(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_notice(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_invite(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_quit(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_kill(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_hurt(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_heal(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_hurtset(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_akill(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_kline(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_unkline(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_rakill(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_gnotice(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_goper(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_globops(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_locops(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_zline(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_unzline(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_ahurt(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_rahurt(aClient *cptr, aClient *sptr, int parc, char *parv[]);

int m_mlock(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_motd(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_who(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_whois(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_user(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_list(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_server(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_info(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_links(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_stats(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_version(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_showmask(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_squit(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_away(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_connect(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_oper(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_pass(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_trace(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_time(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_names(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_admin(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_lusers(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_umode(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_close(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_motd(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_whowas(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_silence(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_userhost(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_ison(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_nickserv(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_chanserv(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_operserv(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_memoserv(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_infoserv(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_gameserv(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_identify(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_services(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_opmode(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_rehash(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_restart(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_die(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_hash(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_dns(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_log(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_watch(aClient *cptr, aClient *sptr, int parc, char *parv[]);
int m_showcon(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Message flags */
#define MF_NODUMP		0x1	/* only allow once per 2 seconds */
#define MF_OPER			0x2	/* only opers may use */
#define MF_ULINE		0x10	/* only opers may use */
#define MF_H			0x20	/* semi-invis */
#define M_UL			(0x1|0x10)
#define M_OP			(0x1|0x2)

#ifdef MSGTAB
#define SERVICES_TAB(msg, short, func, flags)    \
  { msg,    func,      0, 1, 1|flags, 0L, 2 },   \
  { short,  func,    0, 1, 1|MF_H|flags, 0L, 2 },

struct Message msgtab[] = {
  { MSG_PRIVATE, m_private,  0, MAXPARA, 1,        0L, 2 },
  { MSG_NICK,    m_nick,     0, MAXPARA, 1,        0L, 3 },
  { MSG_NOTICE,  m_notice,   0, MAXPARA, 1,        0L, 0 },
  { MSG_JOIN,    m_join,     0, MAXPARA, 1,        0L, 0 },
  { MSG_MODE,    m_mode,     0, MAXPARA, 1,        0L, 0 },
  { MSG_QUIT,    m_quit,     0, MAXPARA, 1,        0L, 1 },
  { MSG_PART,    m_part,     0, MAXPARA, 1,        0L, 1 },
  { MSG_TOPIC,   m_topic,    0, MAXPARA, 1,        0L, 0 },
  { MSG_INVITE,  m_invite,   0, MAXPARA, 1,        0L, 0 },
  { MSG_KICK,    m_kick,     0, MAXPARA, 1,        0L, 0 },
  { MSG_WALLOPS, m_wallops,  0, MAXPARA, 1,        0L, 0 },
  { MSG_CNICK,   m_cnick,    0, MAXPARA, M_UL,     0L, 0 },
  { MSG_MLOCK,   m_mlock,    0, MAXPARA, 1,        0L, 0 },

  { MSG_PING,    m_ping,     0, MAXPARA, 1,        0L, 0 },
  { MSG_PONG,    m_pong,     0, MAXPARA, 1,        0L, 1 },
  { MSG_ERROR,   m_error,    0, MAXPARA, 1,        0L, 0 },
  { MSG_KILL,    m_kill,     0, MAXPARA, 1,        0L, 0 },
  { MSG_HURT,    m_hurt,     0, MAXPARA, 1 | MF_H, 0L, 0 },
  { MSG_HEAL,    m_heal,     0, MAXPARA, 1 | MF_H, 0L, 1 },
  { MSG_HUSH,    m_hurt,     0, MAXPARA, 1,        0L, 0 },
  { MSG_UNHUSH,  m_heal,     0, MAXPARA, 1,        0L, 1 },
  { MSG_USER,    m_user,     0, MAXPARA, 1,        0L, 1 },
  { MSG_AWAY,    m_away,     0, MAXPARA, 1,        0L, 1 },
  { MSG_ISON,    m_ison,     0, 1      , 1,        0L, 1 },
  { MSG_SERVER,  m_server,   0, MAXPARA, 1,        0L, 0 },
  { MSG_SQUIT,   m_squit,    0, MAXPARA, 1,        0L, 1 },
  { MSG_WHOIS,   m_whois,    0, MAXPARA, 1,        0L, 0 },
  { MSG_WHO,     m_who,      0, MAXPARA, 1,        0L, 2 },
  { MSG_WHOWAS,  m_whowas,   0, MAXPARA, 1,        0L, 0 },
  { MSG_LIST,    m_list,     0, MAXPARA, 1,        0L, 0 },
  { MSG_NAMES,   m_names,    0, MAXPARA, 1,        0L, 0 },
  { MSG_USERHOST,m_userhost, 0, 1      , 1,        0L, 1 },
  SERVICES_TAB( MSG_NICKSERV, "NS", m_nickserv, 0 )
  SERVICES_TAB( MSG_CHANSERV, "CS", m_chanserv, 0 )
  SERVICES_TAB( MSG_OPERSERV, "OS", m_operserv, MF_OPER )
  SERVICES_TAB( MSG_MEMOSERV, "MS", m_memoserv, 0 )
  SERVICES_TAB( MSG_INFOSERV, "IS", m_infoserv, 0 )
  SERVICES_TAB( MSG_GAMESERV, "GS", m_gameserv, 0 )
  { MSG_SERVICE, m_services, 0, 3,       1,        0L, 2 },
  { MSG_IDENT,   m_identify, 0, 3,       1,        0L, 1 },
  { MSG_TRACE,   m_trace,    0, MAXPARA, 1,        0L, 0 },
  { MSG_PASS,    m_pass,     0, MAXPARA, 1,        0L, 1 },
  { MSG_LUSERS,  m_lusers,   0, MAXPARA, 1,        0L, 1 },
  { MSG_TIME,    m_time,     0, MAXPARA, 1,        0L, 0 },
  { MSG_OPER,    m_oper,     0, MAXPARA, 1,        0L, 2 },
  { MSG_CONNECT, m_connect,  0, MAXPARA, 1,        0L, 0 },
  { MSG_VERSION, m_version,  0, MAXPARA, 1,        0L, 1 },
  { MSG_STATS,   m_stats,    0, MAXPARA, 1,        0L, 0 },
  { MSG_LINKS,   m_links,    0, MAXPARA, 1,        0L, 0 },
  { MSG_ADMIN,   m_admin,    0, MAXPARA, 1,        0L, 1 },
  { MSG_INFO,    m_info,     0, MAXPARA, 1,        0L, 0 },
  { MSG_MOTD,    m_motd,     0, MAXPARA, 1,        0L, 0 },
  { MSG_CLOSE,   m_close,    0, MAXPARA, 1,        0L, 0 },
  { MSG_SILENCE, m_silence,  0, MAXPARA, 1,        0L, 0 },
  { MSG_AHURT,   m_ahurt,    0, MAXPARA, M_UL,     0L, 0 },
  { MSG_AKILL,   m_akill,    0, MAXPARA, 1,        0L, 0 },
  { MSG_KLINE,   m_kline,    0, MAXPARA, 1,        0L, 0 },
  { MSG_UNKLINE, m_unkline,  0, MAXPARA, 1,        0L, 0 },
  { MSG_ZLINE,   m_zline,    0, MAXPARA, 1,        0L, 0 },
  { MSG_UNZLINE, m_unzline,  0, MAXPARA, 1,        0L, 0 },
  { MSG_RAHURT,  m_rahurt,   0, MAXPARA, M_UL,     0L, 0 },
  { MSG_RAKILL,  m_rakill,   0, MAXPARA, 1,        0L, 0 },
  { MSG_GNOTICE, m_gnotice,  0, MAXPARA, 1,        0L, 0 },
  { MSG_GOPER,   m_goper,    0, MAXPARA, 1,        0L, 0 },
  { MSG_GLOBOPS, m_globops,  0, MAXPARA, 1,        0L, 0 },
  { MSG_LOCOPS,  m_locops,   0, MAXPARA, 1,        0L, 0 },
  { MSG_OPMODE,  m_opmode,   0, MAXPARA, 1,        0L, 0 },
  { MSG_HASH,    m_hash,     0, MAXPARA, 1,        0L, 0 },
  { MSG_DNS,     m_dns,      0, MAXPARA, 1,        0L, 0 },
  { MSG_REHASH,  m_rehash,   0, MAXPARA, 1,        0L, 0 },
  { MSG_RESTART, m_restart,  0, 1      , 1,        0L, 0 },
  { MSG_DIE,     m_die,      0, MAXPARA, 1,        0L, 0 },
  { MSG_SHOWMASK,m_showmask, 0, MAXPARA, 1,        0L, 0 },
  { MSG_HURTSET, m_hurtset,  0, MAXPARA, 1 | MF_H, 0L, 0 },
  { MSG_LOG,	 m_log,      0, MAXPARA, 1,	   0L, 0 },
  { MSG_SHOWCON, m_showcon,  0, MAXPARA, 1 | MF_H, 0L, 0 },
  { MSG_WATCH,	 m_watch,    0, MAXPARA, 1,	   0L, 0 },
  { NULL,        NULL,       0, 0,       0,        0L, 0 }
};
#else
extern struct Message msgtab[];
#endif
#endif /* __msg_include__ */
