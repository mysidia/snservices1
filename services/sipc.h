/**
 * \file sipc.h
 * \brief Services IPC class
 *
 * Services interface for talking with other software
 *
 * \mysid
 * \date 2001
 *
 * $Id$
 */

/*
 * Copyright (c) 2001 James Hess
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
#include <sys/types.h>
#include <sys/socket.h>

#define IPCBUFSIZE 1024

#define PRIV_QUERY_NICK_ISREG	0x00000001
#define PRIV_QUERY_CHAN_ISREG	0x00000002
#define PRIV_QUERY_AKILL_LIST	0x00000004
#define PRIV_QUERY_NICK_PUBLIC	0x00000008
#define PRIV_QUERY_NICK_PRIVATE	0x00000010
#define PRIV_QUERY_NICK_UNMASK  0x00000020
#define PRIV_MAKE_NICK		0x00000020
#define PRIV_SET_BYPASS		0x00000040
#define PRIV_UNSET_BYPASS	0x00000080
#define PRIV_ALTER_RNICK_GEN	0x00000100
#define PRIV_ALTER_RNICK_2	0x00000200
#define PRIV_UNDEF		0x00000400
#define PRIV_ALTER_RNICK_3	0x00000800
#define PRIV_NOWNER_EQUIV	0x00001000
#define PRIV_COWNER_EQUIV	0x00002000
#define PRIV_RNICK_LOGIN	0x00040000
#define PRIV_RCHAN_LOGIN	0x00080000
#define PRIV_LOGW		0x00100000
#define PRIV_QUERY_CHAN_PUBLIC  0x01000000
#define PRIV_QUERY_CHAN_PRIVATE 0x02000000
#define PRIV_ALTER_RCHAN_GEN	0x04000000
#define PRIV_ALTER_RCHAN_2	0x08000000
#define PRIV_ALTER_RCHAN_3	0x10000000

#define OPRIV_OWNER		0x00000001
#define OPRIV_SETPASS		0x00000002	

class IpcQ;

/**
 * @brief An Ipc Message queue (buffer) element
 */
struct IpcQel
{
		char *text;	///< Text of the line
		IpcQel *next;	///< Pointer to the next line of text
		size_t len;	///< Line length
};

/**
 * @brief An Ipc message queue container
 * 
 * These objects are used to hold line buffers read from an
 * IPC channel.
 */
class IpcQ
{
	protected:
		/**
		 * @internal
		 * @brief Called by shove() to push a piece of the message
		 *        before a \\n onto the buffer.
		 */
		char *qPush(char *text, char *sep, int *rlen) {
			IpcQel *z;

			z = (IpcQel *)oalloc(sizeof(IpcQel) + (sep - text + 1));

			*sep = '\0';
			*rlen = z->len = sep - text;
			z->text = (char *)oalloc(z->len + 1);
			strcpy(z->text, text);

			if (firstEls == NULL)
				firstEls = lastEls = z;
			else
			{
				lastEls->next = z;
				lastEls = z;
			}

			*sep = '\n';

			return (sep + 1);
		}

	public:

		/**
		 * @brief Fill the buffer from input data and return
		 *        the leftovers
		 *
		 * @param textIn Text to buffer for processing
		 * @param textLen Length of the text (in bytes)
		 * @return An offset of textIn after which no text
		 *         is buffered.
		 * @pre   textIn points to a character array of
		 *        size textLen.
		 * @post  Each substring ending with a \\n inside
		 *        #textIn is now in a buffer element, and
		 *        buffer elements are in the order in which
		 *        text was input.  If the array returned is
		 *        empty then all text has been buffered,
		 *        else that which has not been buffered is
		 *        contained.
		 */
		char *shove(char *textIn, size_t textLen, int *rlen) {
			char *p, *text = textIn;

			for(p = text; p < (textIn + textLen); p++) {
				if (*p == '\n' || (*p == '\r' && p[1] == '\n')) {
					text = qPush(text, p, rlen);
				}
			}

			return text;
		}

		/**
		 * @brief Get buffered text and place it in cmd.
		 * @param Buffer to place text in
		 * @return 1 if a cmd has been loaded with a buffered item,
		 *         0 if there is no text in the buffer.
		 * @pre   Cmd is a memory area of type char [] with
		 *        a size of at least #IPCBUFSIZE bytes.
		 * @post  If an item is buffered for processing,
		 *        then the text of the next item will
		 *        fill cmd, and be removed from the buffer.
		 */
		int pop(char cmd[IPCBUFSIZE]) {
			IpcQel *f;
			char *cp;

			if (firstEls == NULL)
				return 0;

			f = firstEls;

			if (firstEls == lastEls)
				firstEls = lastEls = NULL;
			else
				firstEls = firstEls->next;

			strncpy(cmd, f->text, IPCBUFSIZE);
			cmd[IPCBUFSIZE - 1] = '\0';

			cp = cmd + strlen(cmd) - 1;

			if (*cp == '\n' || *cp == '\r')
				*cp = '\0';

			if ((cp - 1) > cmd && (cp[-1] == '\r' || cp[-1] == '\n'))
				cp[-1] = '\0';

			free(f->text);
			free(f);

			return 1;
		}

		/**
		 * @brief Is the buffer empty or no?
		 * @return False if the buffer contains any text,
		 *         not-false if the buffer contains text.
		 */
		bool IsEmpty()
		{
			if (firstEls == NULL)
				return 1;
			return 0;
		}

		/**
		 * @brief Empty the buffer
		 * @post  The buffer contains no items.
		 */
		void empty()
		{
			char cmd[1025];

			while(pop(cmd))
				return;
		}

	private:
		IpcQel *firstEls, *lastEls;
};

enum sipc_obj_t
{
	SIPC_UNDEF,
	SIPC_RNICK,
	SIPC_RCHAN,
};

/**
 * @brief A single connection to services
 */
struct IpcConnectType 
{
	int fd;			///< File descriptor of the connection
	flag_t gpriv;		///< Global Privileges
	flag_t opriv;		///< Global Privileges
	struct in_addr addr;	///< Incoming address of the user
	IpcQ buf;		///< Buffer for incoming messages
	long cookie;		///< Cookie used for authentication purposes
	long cookie_in;		///< Cookie used to authenticate services to
				///< the user
	char *user, *pass;	///< User/pass/hash fields for system auth
	char *objName, *objPass;///< User/pass/hash fields for object auth
	enum sipc_obj_t objType;///< Type of object
	char s;			///< State

	IpcConnectType *next;	///< Next connection

	void sWrite(const char *text);	///< Send the user a message
	void fWrite(const char *text, ...); ///< Send the user a message
	void fWriteLn(const char *text, ...); ///< Send the user a message
	int sendAkills(int akillType, const char *searchText);
	flag_t GetPriv() {return gpriv;}
	int CheckPriv(flag_t pSys, flag_t pObj = 0);
	int CheckOrPriv(flag_t pSys, flag_t pObj = 0);
};

/**
 * @brief The master IPC class
 *
 * Manages IPC listeners and connections
 */
class IpcType
{
	public:
		int start(int portNum);
		void fdFillSet(fd_set &);
		int getListenfd() { return listenDesc; }
		int getTopfd() { return topFd; }
		void pollAndHandle(fd_set &read, fd_set &write, fd_set &except);
		void freeCon(IpcConnectType *);
		void addCon(IpcConnectType *);
		void delCon(IpcConnectType *);

		IpcType() : listenDesc(-1), links(NULL) {}
	private:
		int ReadPackets(IpcConnectType *);
		void authMessage(IpcConnectType *, parse_t *);
		void authSysMessage(IpcConnectType *, parse_t *);
		void authObjMessage(IpcConnectType *, parse_t *);
		void queryMessage(IpcConnectType *, parse_t *);
		void querySysMessage(IpcConnectType *, parse_t *);
		void queryObjMessage(IpcConnectType *, parse_t *);
		int queryRegNickMessage(RegNickList *, const char *, IpcConnectType *, parse_t *);
		int queryRegChanMessage(RegChanList *, const char *, IpcConnectType *, parse_t *);
		void makeMessage(IpcConnectType *, parse_t *);
		void alterMessage(IpcConnectType *, parse_t *);
		void logMessage(IpcConnectType *, parse_t *);
		void alterObjMessage(IpcConnectType *, parse_t *);
		int alterRegNickMessage(RegNickList *, const char *, IpcConnectType *, parse_t *);


		int listenDesc; 	///< Descriptor of listener
		int topFd;		///< Highest fd of this IPC unit
		IpcConnectType *links;	///< Connected clients
};
