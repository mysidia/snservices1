/************************************************************************
 *   IRC - Internet Relay Chat, ircd/s_err.c
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

#include "ircd.h"

IRCD_SCCSID("@(#)s_err.c	1.12 11/1/93 (C) 1992 Darren Reed");
IRCD_RCSID("$Id$");

typedef	struct	{
	int	code;
	char	*msg;
} Numeric;

char numbuf[514];

char *prepbuf(int, char *);

static  Numeric local_replies[] = {
/* 000 */	{ 0, NULL },
/* 001 RPL_WELCOME */	{ 1,	":Welcome to the %s IRC Network %s" },
/* 002 RPL_YOURHOST*/	{ 2,	":Your host is %s, running version %s" },
/* 003 RPL_CREATED */	{ 3,	":This server was created %s" },
/* 004 RPL_MYINFO */	{ 4,	"%s %s oilmwsghOkcf Hbciklmnopstv" },
/* 005 RPL_PROTOCTL*/	{ 5,	"NETWORK=%s "
/* 005 ------------------*/	"WTCH_BROKEN=128 "
/* 005 ------------------*/	"MODES=3 "
/* 005 ------------------*/	"CHANMODES=b,k,l,Hcimnpst "
/* 005 ------------------*/	"CHANTYPES=#& "
/* 005 ------------------*/	"KICKLEN=307 " /*TOPICLEN*/
/* 005 ------------------*/	"MAXBANS=60 " /*MAXBANS*/
/* 005 ------------------*/	"MAXCHANNELS=%d " /*MAXCHANNELSPERUSER*/
/* 005 ------------------*/	"NICKLEN=17 " /*NICKLEN*/
/* 005 ------------------*/	"PREFIX=(ov)@+ "
/* 005 ------------------*/	"SILENCE=5 "
/* 005 ------------------*/	"TOPICLEN=307 " /*TOPICLEN*/
/* 005 ------------------*/	"WALLCHOPS "
/* 005 ------------------*/	":are available on this server" },

/* 200 */	{ RPL_TRACELINK, "Link %s%s %s %s" },
/* 201 */	{ RPL_TRACECONNECTING, "Attempt %d %s" },
/* 202 */	{ RPL_TRACEHANDSHAKE, "Handshaking %d %s" },
/* 203 */	{ RPL_TRACEUNKNOWN, "???? %d %s" },
/* 204 */	{ RPL_TRACEOPERATOR, "Operator %s %s %ld" },
/* 205 */	{ RPL_TRACEUSER, "User %s %s %ld" },
/* 206 */	{ RPL_TRACESERVER, "Server %s %dS %dC %s %s!%s@%s %ld" },
/* 208 */	{ RPL_TRACENEWTYPE, "<newtype> 0 %s" },
/* 209 */	{ RPL_TRACECLASS, "Class %s %d" },
/* 211 */	{ RPL_STATSLINKINFO, NULL },
/* 212 */	{ RPL_STATSCOMMANDS, "%s %u %u" },
/* 213 */	{ RPL_STATSCLINE, "%c %s * %s %d %s" },
/* 214 */	{ RPL_STATSNLINE, "%c %s * %s %d %s" },
/* 215 */	{ RPL_STATSILINE, "%c %s * %s %d %s" },
/* 216 */	{ RPL_STATSKLINE, "%c %s %s %s %d %d" },
/* 217 */	{ RPL_STATSQLINE, "%c %s %s %s %d %d" },
/* 218 */	{ RPL_STATSYLINE, "%c %d %d %d %d %ld" },
/* 219 */	{ RPL_ENDOFSTATS, "%c :End of /STATS report" },
/* 221 */	{ RPL_UMODEIS, "%s" },
/* 231 */	{ RPL_SERVICEINFO, NULL },
/* 232 */	{ RPL_ENDOFSERVICES, NULL },
/* 233 */	{ RPL_SERVICE, NULL },
/* 234 */	{ RPL_SERVLIST, NULL },
/* 235 */	{ RPL_SERVLISTEND, NULL },
/* 241 */	{ RPL_STATSLLINE, "%c %s * %s %d %d" },
/* 242 */	{ RPL_STATSUPTIME, ":Server Up %d days, %d:%02d:%02d" },
/* 243 */	{ RPL_STATSOLINE, "%c %s * %s %s %s" },
/* 244 */	{ RPL_STATSHLINE, "%c %s * %s %d %s" }, 
/* 245 */	{ RPL_STATSSLINE, "%c %s * %s %d %d" }, 
/* 247 */	{ RPL_STATSXLINE, "X %s %d" }, 
/* 248 */	{ RPL_STATSULINE, "%c %s * %s %d %s" }, 
/* 250 */       { RPL_STATSCONN,
                ":Highest connection count: %d (%d clients)" },
/* 251 */	{ RPL_LUSERCLIENT,
		":There are %d users and %d invisible on %d servers" },
/* 252 */	{ RPL_LUSEROP, "%d :operator(s) online" },
/* 253 */	{ RPL_LUSERUNKNOWN, "%d :unknown connection(s)" },
/* 254 */	{ RPL_LUSERCHANNELS, "%d :channels formed" },
/* 255 */	{ RPL_LUSERME, ":I have %d clients and %d servers" },
/* 256 */	{ RPL_ADMINME, ":Administrative info about %s" },
/* 257 */	{ RPL_ADMINLOC1, ":%s" },
/* 258 */	{ RPL_ADMINLOC2, ":%s" },
/* 259 */	{ RPL_ADMINEMAIL, ":%s" },
/* 261 */	{ RPL_TRACELOG, "File %s %d" },
/* 271 */	{ RPL_SILELIST, "%s %s" },
/* 272 */	{ RPL_ENDOFSILELIST, ":End of Silence List" },
/* 275 */	{ RPL_STATSDLINE, "%c %s %s" },
/* 300 */	{ RPL_NONE, NULL },
/* 301 */	{ RPL_AWAY, "%s :%s" },
/* 302 */	{ RPL_USERHOST, ":" },
/* 303 */	{ RPL_ISON, ":" },
/* 304 */	{ RPL_TEXT, NULL },
/* 305 */	{ RPL_UNAWAY, ":You are no longer marked as being away" },
/* 306 */	{ RPL_NOWAWAY, ":You have been marked as being away" },
/* 307 */       { RPL_WHOISREGNICK, "%s :has identified for this nick%s" },
/* 309 */	{ RPL_WHOISHURT, "%s :has been muted" },
/* 310 */	{ RPL_WHOISHELPOP, "%s :looks very helpful." },
/* 311 */	{ RPL_WHOISUSER, "%s %s %s * :%s" },
/* 312 */	{ RPL_WHOISSERVER, "%s %s :%s" },
/* 313 */	{ RPL_WHOISOPERATOR, "%s :is an IRC Operator" },
/* 314 */	{ RPL_WHOWASUSER, "%s %s %s * :%s" },
/* 315 */	{ RPL_ENDOFWHO, "%s :End of /WHO list." },
/* 316 */	{ RPL_WHOISCHANOP, NULL },
/* 317 */	{ RPL_WHOISIDLE, "%s %ld %ld :seconds idle, signon time" },
/* 318 */	{ RPL_ENDOFWHOIS, "%s :End of /WHOIS list." },
/* 319 */	{ RPL_WHOISCHANNELS, "%s :%s" },
/* 321 */	{ RPL_LISTSTART, "Channel :Users  Name" },
/* 322 */	{ RPL_LIST, "%s %d :%s" },
/* 323 */	{ RPL_LISTEND, ":End of /LIST" },
/* 324 */	{ RPL_CHANNELMODEIS, "%s %s %s" },
/* 325 */	{ RPL_CHANNELMLOCKIS, "%s :Channel mlock is %s" },
/* 329 */ { RPL_CREATIONTIME, "%s %lu" },
/* 331 */	{ RPL_NOTOPIC, "%s :No topic is set." },
/* 332 */	{ RPL_TOPIC, "%s :%s" },
/* 333 */       { RPL_TOPICWHOTIME, "%s %s %lu" },
/* 334 */	{ RPL_COMMANDSYNTAX, "%s" },
/* 341 */	{ RPL_INVITING, "%s %s" },
/* 342 */	{ RPL_SUMMONING, "%s :User summoned to irc" },
#ifdef USE_CASETABLES
/* 351 */	{ RPL_VERSION, "%s.%s %s :%s ct=%d" },
#else
/* 351 */	{ RPL_VERSION, "%s.%s %s :%s" },
#endif
/* 352 */	{ RPL_WHOREPLY, "%s %s %s %s %s %s :%d %s" },
/* 353 */	{ RPL_NAMREPLY, "%s" },
/* 361 */	{ RPL_KILLDONE, NULL },
/* 362 */	{ RPL_CLOSING, "%s :Closed. Status = %d" },
/* 363 */	{ RPL_CLOSEEND, "%d: Connections Closed" },
/* 364 */	{ RPL_LINKS, "%s %s :%d %s" },
/* 365 */	{ RPL_ENDOFLINKS, "%s :End of /LINKS list." },
/* 366 */	{ RPL_ENDOFNAMES, "%s :End of /NAMES list." },
/* 367 */	{ RPL_BANLIST, "%s %s %s %lu" },
/* 368 */	{ RPL_ENDOFBANLIST, "%s :End of Channel Ban List" },
/* 369 */	{ RPL_ENDOFWHOWAS, "%s :End of WHOWAS" },
/* 371 */	{ RPL_INFO, ":%s" },
/* 372 */	{ RPL_MOTD, ":- %s" },
/* 373 */	{ RPL_INFOSTART, ":Server INFO" },
/* 374 */	{ RPL_ENDOFINFO, ":End of /INFO list." },
/* 375 */	{ RPL_MOTDSTART, ":- %s Message of the Day - " },
/* 376 */	{ RPL_ENDOFMOTD, ":End of /MOTD command." },
/* 381 */	{ RPL_YOUREOPER, ":You are now an IRC Operator" },
/* 382 */	{ RPL_REHASHING, "%s :Rehashing" },
/* 383 */	{ RPL_YOURESERVICE, NULL },
/* 384 */	{ RPL_MYPORTIS, "%d :Port to local server is\r\n" },
/* 385 */	{ RPL_NOTOPERANYMORE, NULL },
/* 391 */	{ RPL_TIME, "%s :%s" },
/* 392 */	{ RPL_USERSSTART, ":UserID   Terminal  Host" },
/* 393 */	{ RPL_USERS, ":%-8s %-9s %-8s" },
/* 394 */	{ RPL_ENDOFUSERS, ":End of Users" },
/* 395 */	{ RPL_NOUSERS, ":Nobody logged in." },

/* 401 */	{ ERR_NOSUCHNICK, "%s :No such nick/channel" },
/* 402 */	{ ERR_NOSUCHSERVER, "%s :No such server" },
/* 403 */	{ ERR_NOSUCHCHANNEL, "%s :No such channel" },
/* 404 */	{ ERR_CANNOTSENDTOCHAN, "%s :Cannot send to channel" },
/* 405 */	{ ERR_TOOMANYCHANNELS, "%s :You have joined too many channels" },
/* 406 */	{ ERR_WASNOSUCHNICK, "%s :There was no such nickname" },
/* 407 */	{ ERR_TOOMANYTARGETS,
		"%s :Duplicate recipients. No message delivered" },
/* 408 */	{ ERR_NOCOLORSONCHAN, "%s :This channel does not allow colors to be used. Unable to send your message: %s" },
/* 409 */	{ ERR_NOORIGIN, ":No origin specified" },
/* 410 */	{ ERR_NORECIPIENT, ":No recipient given (%s)" },
/* 411 */	{ ERR_NOTEXTTOSEND, ":No text to send" },
/* 412 */	{ ERR_NOTOPLEVEL, "%s :No toplevel domain specified" },
/* 413 */	{ ERR_WILDTOPLEVEL, "%s :Wildcard in toplevel Domain" },
/* 414 */	{ ERR_UNKNOWNCOMMAND, "%s :Unknown command" },
/* 415 */	{ ERR_NOMOTD, ":MOTD File is missing" },
/* 416 */	{ ERR_NOADMININFO,
		"%s :No administrative info available" },
/* 417 */	{ ERR_FILEERROR, ":File error doing %s on %s" },
/* 418 */	{ ERR_NONICKNAMEGIVEN, ":No nickname given" },
/* 432 */	{ ERR_ERRONEUSNICKNAME, "%s :Erroneus Nickname: %s" },
/* 433 */	{ ERR_NICKNAMEINUSE, "%s :Nickname is already in use." },
/* 434 */	{ ERR_SERVICENAMEINUSE, NULL },
/* 435 */	{ ERR_SERVICECONFUSED, NULL },
/* 436 */	{ ERR_NICKCOLLISION, "%s :Nickname collision KILL" },
/* 437 */	{ ERR_BANNICKCHANGE,
		"%s :Cannot change nickname when moderated/banned on a channel." },
/* 440 */	{ ERR_SERVICESDOWN, ":Services is currently down. Please wait a few moments and then try again." },
/* 441 */	{ ERR_USERNOTINCHANNEL, "%s %s :They aren't on that channel" },
/* 442 */	{ ERR_NOTONCHANNEL, "%s :You're not on that channel" },
/* 443 */	{ ERR_USERONCHANNEL, "%s %s :is already on channel" },
/* 444 */	{ ERR_NOLOGIN, "%s :User not logged in" },
/* 445 */	{ ERR_SUMMONDISABLED, ":SUMMON has been disabled" },
/* 446 */	{ ERR_USERSDISABLED, ":USERS has been disabled" },
/* 451 */	{ ERR_NOTREGISTERED, ":You have not registered" },
#ifdef HOSTILENAME
/* 455 */	{ ERR_HOSTILENAME, ":Your username %s contained the invalid "
			"character(s) %s and has been changed to %s. "
			"Please use only the characters 0-9 a-z A-Z _ - "
			"or . in your username. Your username is the part "
			"before the @ in your email address." },
#endif
/* 461 */	{ ERR_NEEDMOREPARAMS, "%s :Not enough parameters" },
/* 462 */	{ ERR_ALREADYREGISTRED, ":You may not reregister" },
/* 463 */	{ ERR_NOPERMFORHOST, ":Your host isn't among the privileged" },
/* 464 */	{ ERR_PASSWDMISMATCH, ":Password Incorrect" },
/* 465 */	{ ERR_YOUREBANNEDCREEP, ":You are banned from this server. Mail %s for more information" }, 
/* 466 */	{ ERR_YOUWILLBEBANNED, NULL },
/* 467 */	{ ERR_KEYSET, "%s :Channel key already set" },
/* 471 */	{ ERR_CHANNELISFULL, "%s :Cannot join channel (+l)" },
/* 472 */	{ ERR_UNKNOWNMODE  , "%c :is unknown mode char to me" },
/* 473 */	{ ERR_INVITEONLYCHAN, "%s :Cannot join channel (+i)" },
/* 474 */	{ ERR_BANNEDFROMCHAN, "%s :Cannot join channel (+b)" },
/* 475 */	{ ERR_BADCHANNELKEY, "%s :Cannot join channel (+k)" },
/* 476 */	{ ERR_BADCHANMASK, "%s :Bad Channel Mask" },
/* 477 */       { ERR_NEEDREGGEDNICK, "%s :You need to identify to a registered nick to "
                                      "join that channel. For help with registering "
                                      "your nickname, type /msg nickserv@ help register or see "
				      "http://www.sorcery.net/help/register" },
/* 478 */	{ ERR_BANLISTFULL, "%s %s :Channel ban/ignore list is full" },
/* 481 */	{ ERR_NOPRIVILEGES,
		":Permission Denied- You're not an IRC operator" },
/* 482 */	{ ERR_CHANOPRIVSNEEDED, "%s :You're not channel operator" },
/* 483 */	{ ERR_CANTKILLSERVER, ":You cant kill (or hurt) a server!" },
/* 486 */       { ERR_NONONREG, ":You must identify to a "
                                    "%s nick to private message %s", },
    
/* 491 */	{ ERR_NOOPERHOST, ":No O-lines for your host" },
/* 492 */	{ ERR_NOSERVICEHOST, NULL },
/* 501 */	{ ERR_UMODEUNKNOWNFLAG, ":Unknown MODE flag" },
/* 502 */	{ ERR_USERSDONTMATCH, ":Cant change mode for other users" },
/* 511 */	{ ERR_SILELISTFULL, "%s :Your silence list is full" },
/* 512 */	{ ERR_TOOMANYWATCH, "%s :Maximum size for WATCH-list is 128 entries." },
/* 513 */	{ ERR_NEEDPONG, ":To connect, type /QUOTE PONG %lX" },
/* 514 */	{ ERR_YOURHURT, ":Your connection is silenced:\
 You should not attempt to speak or issue any IRC commands: Attempts\
 to do so will not be successful." },
/* 521 */	{ ERR_LISTSYNTAX, "Bad list syntax, type /quote list ? or /raw list ?" },
/* 522 */	{ 0, NULL }, /* who syntax */
/* 523 */	{ 0, NULL }, /* who limit */

/* 550 */	{ RPL_WHOISMASKED, "%s :is masked as %s" },

/* 551 */	{ RPL_WHOISVERSION, "%s :is using %s" },

/* 600 */ { RPL_LOGON, "%s %s %s %d :logged online" },
/* 601 */ { RPL_LOGOFF, "%s %s %s %d :logged offline" },
/* 602 */ { RPL_WATCHOFF, "%s %s %s %d :stopped watching" },
/* 603 */ { RPL_WATCHSTAT, ":You have %d and are on %d WATCH entries" },
/* 604 */ { RPL_NOWON, "%s %s %s %d :is online" },
/* 605 */ { RPL_NOWOFF, "%s %s %s %d :is offline" },
/* 606 */ { RPL_WATCHLIST, ":%s" },
/* 607 */ { RPL_ENDOFWATCHLIST, ":End of WATCH %c" },
/* 608 */	{ 0, NULL }, /* RESERVED - watch*/
/* 617-620 DALnet dccallow/dccstatus */
/* 625 */ { ERR_NOMASKCHAN, "%s :To join, you must show your IP address "
	         "(which you have masked) to the channel operators inside. "
	         "To do this, type: /join %s UNMASK" },

/* 626 */ { ERR_BANRULE, "%s :This channel has established a special entry "
                       "requirement that you do not satisfy." },
/* 627 */ { ERR_BANREQUIRE, "%s :This channel has established a special entry "
                       "requirement that you do not satisfy." },
/* 628 */ { ERR_NEEDVERNICK, "%s :You need to identify to a registered AND verified nickname to "
                             "join that channel. For help with registering "
	                     "your nickname, type /msg nickserv@ help register "
			     "And /msg nickserv@ help verify" },
			     
};

static char *replies[1000];

void
boot_replies(void)
{
	int i;
	char *s;

	for (i = 0 ; i < 1000 ; i++)
		replies[i] = NULL;

	for (i = 0 ; i < sizeof(local_replies) / sizeof(Numeric) ; i++) {
		if (local_replies[i].code < 1 || local_replies[i].code > 999)
			continue;

		s = prepbuf(local_replies[i].code, local_replies[i].msg);
		replies[local_replies[i].code] = s;
	}
}


char *
err_str(int numeric)
{
	if (numeric < 0 || numeric > (ERR_YOURHURT + 400)) {
		sprintf(numbuf,
			":%%s %d %%s :INTERNAL ERROR: BAD NUMERIC! %d",
			numeric, numeric);
		return numbuf;
	}

	if (!replies[numeric]) {
		sprintf(numbuf,
			":%%s %d %%s :NO ERROR FOR NUMERIC ERROR %d",
			numeric, numeric);
		return numbuf;
	}

	return replies[numeric];
}


char *
rpl_str(int numeric)
{
	if (numeric < 0 || numeric > 999) {
		sprintf(numbuf,
			":%%s %d %%s "
			":INTERNAL REPLY ERROR: BAD NUMERIC! %d",
			numeric, numeric);
		return numbuf;
	}

	Debug((DEBUG_NUM, "rpl_str: numeric %d num %d %x",
	       numeric, numeric, replies[numeric]));

	if (!replies[numeric]) {
		sprintf(numbuf,
			":%%s %d %%s :NO REPLY FOR NUMERIC ERROR %d",
			numeric, numeric);
		return numbuf;
	}

	return replies[numeric];
}

char *
prepbuf(int num, char *tail)
{
	char *s;

	/*
	 * Allocate enough space for the string, as well as our additions.
	 */
	if (tail == NULL) {
		s = irc_malloc(11);
		sprintf(s, ":%%s %03d %%s", num);
	} else {
		s = irc_malloc(strlen(tail) + 11 + 1);
		sprintf(s, ":%%s %03d %%s %s", num, tail);
	}

	return (s);
}
