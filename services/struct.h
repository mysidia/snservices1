/**
 * \file struct.h
 * \brief Services' data structure declarations
 *
 * \wd \taz \greg \mysid
 * \date 1996-2001
 *
 * $Id$
 */

/*
 * Copyright (c) 1998 Greg Poma
 * Copyright (c) 2001 James Hess
 * Portions Copyright (c) 1996, 1997 Max Byrd, Chip Norkus
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
#ifndef _STRUCT_H
#define _STRUCT_H

#include "queue.h"
#include "options.h"
#include "services.h"

#if PASSLEN < 15
#error PASSLEN must be 15 or higher so that nick and channel password fields \
       have 16 bytes for MD5 data.
#endif

#define KEYLEN 23

typedef unsigned long flag_t;
typedef struct helpline_t help_line;
typedef struct _memobox MemoBox;
typedef struct _naccesslist nAccessList;
typedef struct helpcache_t help_cache;
typedef struct _regnicklist RegNickList;
typedef struct _userlist UserList;
typedef struct _memolist MemoList;
typedef struct cnicklist_struct cNickList;
typedef struct caccesslist_struct cAccessList;
typedef struct cnicklisthashent_struct cNickListHashEnt;
typedef struct caccesslisthashent_struct cAccessListHashEnt;
typedef struct cakicklist_struct cAkickList;
typedef struct cbanlist_struct cBanList;
typedef struct regchanlist_struct RegChanList;
typedef struct chanlist_struct ChanList;
typedef struct database_struct database;
typedef struct memoblocklist_struct MemoBlock;
typedef struct service_struct Service;
typedef struct operlist_struct OperList;
typedef struct _identifydata IdentifyData;
typedef struct _ChanTrigInfo ChanTrigger;
typedef struct mask_structure MaskData;
typedef u_int16_t IdVal;
typedef u_int16_t HashKeyVal;

#define IDVAL_MAX USHRT_MAX

struct memoblocklist_struct;
struct RegNickIdMap;


enum t_nickserv_acc_level
{
	ACC_OFFLINE = 0,
	ACC_NOT_RECOGNIZED = 1,
	ACC_RECOGNIZED = 2,
	ACC_IDENTIFIED = 3,
	ACC_DIGEST_AUTH = 4
};

/**
 * \brief Registered entity identity number
 */
class RegId
{
	public:
		friend void saveNickData(void);

		RegId() : a(0), b(0) { }

		RegId(IdVal aIn, IdVal bIn) : a(aIn), b(bIn) {
		}

		RegId& operator=(const RegId &x) {
			a = x.a;
			b = x.b;

			return *this;
		}

		bool operator==(const RegId &x) { return (a == x.a && b == x.b); }
		bool operator!=(const RegId &x) { return (a != x.a || b != x.b); }
		bool operator<(const RegId &x) {
			if (a < x.a || (a == x.a && a < x.b))
				return 1;
			return 0;
		}
		bool operator>(const RegId &x) {
			if (a > x.a || (a == x.a && a > x.b))
				return 1;
			return 0;
		}

		void SetNext(RegId &topId);
		void SetDirect(RegId &topId, IdVal aVal, IdVal bVal);
		RegNickIdMap *getIdMap();
		RegNickList *getNickItem();
		const char *getNick();
		const char *getChan();

		HashKeyVal getHashKey() const { 
			HashKeyVal x = (a ^ 27);
			return (x + (b * IDVAL_MAX));
		}

	private:
		IdVal a, b;
};

/**
 * Hash entry for registered nickname id number mappings
 */
struct RegNickIdMap
{
	RegId id;
	RegNickList *nick;

	LIST_ENTRY(RegNickIdMap)     id_lst; 
};

/**
 * A channel trigger item
 */
struct _ChanTrigInfo
{
	LIST_ENTRY(_ChanTrigInfo)    cn_lst;
	int                          impose_modes;
	char                         *chan_name;
	short                        op_trigger, ak_trigger;
	short                        flags;
};


/**
 * @brief Used to hold information about the rate or expense of
 *        activity by a user.  Part of services' flood protection.
 */
class RateInfo
{
	public:
		RateInfo();
		void Event(int weight, time_t tNow);
		void Warn();
		int  Warned(), GetLev();

	private:
			/// Time of last received message (UTC)
		time_t lastEventTime;

			/// Flood level value
		unsigned int rateFloodValue;

			/// Number events sent
		unsigned int numOfEvents;

			/// Number of warnings received
		unsigned char warningsSent;
};

/// One line in a help page
struct helpline_t {
           /// Text of the line, 0...80 characters long: fixed buffer
  char line [81];   

          /// Pointer to the next line in the topic
  help_line *next;
};

/// A help topic cache item
struct helpcache_t {
          /// Name of a help topic, fixed buffer between 0 and HELPTOICBUF units
  char *name;

          /// First line in the help topic
  help_line *first;

          /// List pointer to the next cache item
  help_cache *next;
};

/// Nickserv access list item
struct _naccesslist {
          /// Mask pattern buffer (max size is 71)
  char            mask[71];

          /// List pointers
  LIST_ENTRY(_naccesslist)  al_lst;
};


/**
 * \todo A system of 'user accounts' where the notion of a nickname is  *
 * subordinate to the notion of a user, part of a common account with   *
 * some data such as url, channel ops shared but things like last-seen  *
 * time for expires, last seen address, and nick flags specific to the  *
 * nick. 
 */

// Registered nick list structure
/**
 This structure is that of an item in the NickServ database of registered
 nicknames.
 */

/// Registered nickname structure
struct _regnicklist {
          /// Nickname buffer
  char nick[NICKLEN];

          /// Username buffer
  char user[USERLEN];

          /// Number bad passwords on this nick
  u_char badpws;

          /// Hostname buffer
  char *host;

          /// Password buffer
  unsigned char password[PASSLEN+1];

#ifdef TRACK_GECOS
          /// user's realname field
  char *gecos;
#endif

          /// pointer to the user's URL
  char *url;

          /// If this nick is marked, the nick that marked it
  char *markby;

          /// E-mail address buffer
  char email[EMAILLEN];

          /// Signon time (UTC)
  time_t timestamp;

          /// Time of registration (UTC)
  time_t timereg;

          /// Number of access masks in list
  u_int amasks;

          /// Standard flags assigned to the nickname
  unsigned int flags;

          /// Operator flags assigned to the nickname
  flag_t opflags;

          /// Number of channels the nickname owns
  unsigned int chans;

          /// User's allowed time to identify
  unsigned int idtime;

          /// Last time user listed InfoServ articles (UTC)
  time_t is_readtime;

          /// Id# of the nickname
  RegId  regnum;

          /// Password change authorization key
  u_int32_t chpw_key;

          /// Pointer to the associated memo box
  MemoBox *memos;

          /// NickServ access list
  LIST_HEAD(,_naccesslist)  acl;

          /// List pointers
  LIST_ENTRY(_regnicklist)  rn_lst;
};

/// Structure for storing information about an identification to an object
struct _identifydata {
          /// Pointer to a nickname string
  char   *nick;

          /// Time of the identification, used so that an old identification won't hold for a new registration
  time_t timestamp;

          /// Id number of the nick identified to
  RegId idnum;
};

/// Structure of an online user database object
struct _userlist {
          /// Nickname buffer
  char nick[NICKLEN];

          /// Username buffer
  char user[USERLEN];

          /// Number bad passwords by this user
  u_char badpws;

          /// Hostname buffer
  char *host;

#ifdef TRACK_GECOS
          /// Realname field of online user
  char *gecos;
#endif

          /// Online user flags
  unsigned int oflags;

          /// Rate of commands and flood detection
  RateInfo floodlevel;

          /// Level of identification to present nickname
  unsigned int caccess;

          /// Time of user signon (UTC)
  time_t timestamp;

          /// Hash of channels user is in
  ChanList *chan[NICKCHANHASHSIZE];

          /// Pointer to a registered nickname
  RegNickList *reg;

          /// Identification information
  IdentifyData id;

          /// Online user identifier
  RegId idnum;

          /// Cookie used for hash-based authentication to nicknames
  u_int32_t auth_cookie;

          /// List pointers
  LIST_ENTRY(_userlist)     ul_lst; 
};

/// Information about a memo
struct _memolist {
          /// Sent memo flags
  int          flags;

          /// Time the memo was sent
  time_t       sent;          

          /// Who it was from
  char         from[NICKLEN]; 

          /// Pointer to the text of the memo
  char        *memotxt;

          /// Who the memo is to
  char         to[CHANLEN];

          /// Pointer to nick item of recipient
  RegNickList *realto;

          /// Linked list of memos incoming
  LIST_ENTRY(_memolist)    ml_lst;

          /// Linked list of memos sent
  LIST_ENTRY(_memolist)    ml_sent;
};

/// Information about a memo box
struct _memobox {
          /// Number of memos
  int          memocnt;

          ///  User memo flags (above)
  int          flags;

          /// Maximum memos that this box can receive
  int          max;

          /// List header of Memo Blocking list
  MemoBlock    *firstMblock;

          /// If memos are forwarded, this is a pointer to a registered nick
  RegNickList *forward;

          /// List header of memo inbox
  LIST_HEAD(,_memolist)    mb_memos;

          /// List header of memo outbox
  LIST_HEAD(,_memolist)    mb_sent;
};

/// Information about a memo block
struct memoblocklist_struct {
          /// Blocked nickname identifier
  RegId		blockId;

          ///  Pointer to next block item
  MemoBlock	*next;
};

/// Structure of an opflag list index item
struct operlist_struct {
          /// Pointer to the oper's regnick structure
  RegNickList *who;

          /// Next item in the index
  struct operlist_struct *next;
};

/// Structure of a channel user list item
struct cnicklist_struct {

          /// Pointer to persons user info
  UserList       *person;

          /// Flag: is user opped (or voiced) ? 
  int             op;

  cNickList      *next;              
  cNickList      *previous;          
  cNickList      *hashnext;          
  cNickList      *hashprev;          
};

/// Structure of a channel user hash item
struct cnicklisthashent_struct {
  cNickList      *item;
  cNickList      *lastitem;
};

/// Structure of a channel operator list item
struct caccesslist_struct {

          /// Access ickname
  RegId		 nickId;

          /// flags:  AOP/SOP/CFOUNDER/RFOUNDER
          /** \todo a truly flag-based system for managing channel access lists */
  short           uflags;

          /// Unique index used for quickly deleting this item?
  int index;

          /// Next access item of channel
  cAccessList    *next;

          /// Previous access item of channel
  cAccessList    *previous;
  cAccessList    *hashnext;
  cAccessList    *hashprev;
};

struct caccesslisthashent_struct {
  cAccessList    *item, *lastitem;
};

/// Autokick list structure
struct cakicklist_struct {

          /// Autokick mask buffer
        char            mask[USERLEN+HOSTLEN+3];

          /// Why they're akicked
        char            reason[51 + NICKLEN];

          /// Index number of item
        int index;

          /// When it was set (UTC)
        time_t          added;

          /// Next item in akick list
        cAkickList     *next;

          /// Previous item in list
        cAkickList     *previous;
};

/// Structure of a channel banlist item
struct cbanlist_struct {
          /// Ban pattern buffer
  char            ban[NICKLEN + USERLEN + HOSTLEN];

          /// Next banlist item
  cBanList       *next;

          /// Previous banlist item
  cBanList       *previous;
};

/// Structure of an online channel
struct chanlist_struct {

          /// Channel name buffer
        char name[CHANLEN];

          /// List of banned masks 
        cBanList       *firstBan;

          /// Last ban in list
        cBanList       *lastBan;

          /// Hash of users in the channel
        cNickListHashEnt users[CHANUSERHASHSIZE];

          /// First user in channel list
        cNickList      *firstUser;

          /// Last user in channel list
        cNickList      *lastUser;

          /// Modes of channel
        long modes;

          /// Pointer to channel registration information
        RegChanList *reg;

        ChanList *next, *previous, *hashnext, *hashprev;
};

/// Structure of a registered channel database item
struct regchanlist_struct {

          /// Channel name buffer
  char            name[CHANLEN];

          /// Founder nickname buffer
  RegId		  founderId;

          /// Founder identified?
  int             facc;

          /// Description buffer
  char            desc[CHANDESCBUF];

          /// Autogreet pointer
  char            *autogreet;

          /// Topic pointer
  char            *topic;

          /// URL pointer
  char            *url;

          /// Pointer to marker nickname
  char		  *markby;

          /// Nickname (buffer) of topic setter
  char            tsetby[NICKLEN];

          /// Registration password
  unsigned char   password[PASSLEN+1];

          /// When the topic was set (UTC)
  time_t          ttimestamp;

          /// Modelock flags (+/-)
  long            mlock;

          /// Registration flags
  long            flags;

          /// Time of registration (UTC)
  time_t          timereg;

          /// Last time used (UTC)
  time_t          timestamp;

          /// Channel key (modelock)
  char            key[KEYLEN];

          /// Channel limit (modelock)
  long            limit;

          /// Number of chanops in database
  u_int           ops;

          /// Number of autokicks in database
  int             akicks;

          /// Topiclock level
  int             tlocklevel;

          /// Restriction level
  int             restrictlevel;

          /// Level required to memo channel
  int             memolevel;

          /// Password change authorization key
  u_int32_t chpw_key;

          /// Identification information
  IdentifyData    id;

          /// First akick list item
  cAkickList     *firstAkick;

          /// Last akick list item
  cAkickList     *lastAkick;

          /// Hash of channel operator list
  cAccessListHashEnt op[OPHASHSIZE];

          /// First chanop list item
  cAccessList    *firstOp;

          /// Last chanop list item
  cAccessList    *lastOp;

          /// Next registered channel item
  RegChanList       *next;

          /// Previous registered channel item
  RegChanList       *previous;

          /// Next hashed regchan item
  RegChanList       *hashnext;

          /// Previous hashed regchan item
  RegChanList       *hashprev;

          /// Number bad passwords on this chan
  u_char badpws;

};

/// Information about a SorceryNet services nickname
struct service_struct {

          /// Nickname buffer of service
  char            name[21];

          /// Username of service
  char            uname[10];

          /// Hostname of service
  char            host[67];

          /// Realname of service
  char            rname[51];

          /// Usermode buffer of service
  char            mode[10];

};

/// Pointers to services database files
struct database_struct {
          /// NickServ database
        FILE *ns;

#ifdef REQ_EMAIL
          /// NickServ reg-email database
        FILE *nsreg;
#endif

          /// ChanServ database
        FILE *cs;

          /// OperServ database
        FILE *os;

          /// MemoServ database
        FILE *ms;

          /// InfoServ database
        FILE *is;

          /// Trigger database
        FILE *trigger;

};

/// Structure of a split usermask
struct mask_structure
{
          /// Nickname pointer
	char *nick;

          /// Username pointer
	char *user;

          /// Hostname pointer
	char *host;
};

#endif /* _STRUCT_H */

