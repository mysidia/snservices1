/*
 *   IRCD - Internet Relay Chat Daemon, src/s_service.c
 *   Copyright (C) 1998 Mysidia
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

#include "msg.h"
#include "channel.h"
#include "userload.h"
#include <sys/stat.h>

IRCD_RCSID("$Id$");

/* these are only considered services when the nicks are U-lined. */
/* this array is an evil hack so pointer comparisons will do... */
const char *service_nick[] = {
	"NickServ", "ChanServ", "MemoServ", "OperServ", 
	"InfoServ", "GameServ", (char *)0, (char *)0, (char *)0
};

extern __inline int    m_sendto_service();

int   m_services(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
     const char *sService;
     const char *nickchan;
     aClient *acptr;

     /* /services <command> <nick|chan> <args> */
     if ( parc < 3 && (parc<2 || ( strcasecmp(parv[1], "help") && strcasecmp(parv[1], "info") )) )
     {
             sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "SERVICES");
             return 0;
     }
     if (parc > 2)     nickchan = parv[2];
     else              nickchan = NULL;
     if (nickchan && *nickchan == '#') 
                           sService = cChanServ;
     else                  sService = cNickServ;

       if ( (acptr = find_person((char *)sService, NULL)) && !MyConnect(acptr) &&
             IsULine(acptr, acptr))
       {
               sendto_one(acptr,":%s PRIVMSG %s@%s :%s%s%s%s%s", parv[0],
                       sService, SERVICES_NAME,
                       parv[1], nickchan? " " : "", nickchan?nickchan:"",
                       (parc>3&&parv[3])?" ":"", (parc>3&&parv[3])?parv[3]:"");
       }
       else
               sendto_one(sptr, err_str(ERR_SERVICESDOWN), me.name,
                       parv[0], sService);
     return 0;
}



int   m_identify(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
     aClient *acptr;
     const char *sService;
     const char *nickchan, *pass;
     /* : /identify <nick|chan> <pass> */
     if ( parc < 2 )
     {
             sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "IDENTIFY");
             return 0;
     }
     nickchan = parv[parc-2];
     pass = parv[parc-1];

     if (*nickchan == '#') sService = cChanServ;
     else                  sService = cNickServ;
       if ( (acptr = find_person((char *)sService, NULL)) && !MyConnect(acptr) &&
             IsULine(acptr, acptr))
       {
               if (sService == cNickServ)
               sendto_one(acptr,":%s PRIVMSG %s@%s :IDENTIFY %s", parv[0],
                       sService, SERVICES_NAME, pass);
               else
               sendto_one(acptr,":%s PRIVMSG %s@%s :IDENTIFY %s %s", parv[0],
                       sService, SERVICES_NAME, nickchan, pass);
       }
       else
               sendto_one(sptr, err_str(ERR_SERVICESDOWN), me.name,
                       parv[0], sService);

     return 0;
}


#define SERVICES_FUNC(function, service)                                       \
int    function(aClient *cptr, aClient *sptr, int parc, char *parv[])          \
{                                                                              \
     int fOper;                                                                \
     const char *sService = (service);                                         \
     if (sService == cOperServ) fOper=1;                                       \
     else                       fOper=0;                                       \
     switch ( m_sendto_service(cptr, sptr, parc, parv, fOper, sService))       \
           {                                                                   \
             default: case 0: break;                                           \
             case -2:                                                          \
                sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]); \
             break;                                                            \
           }                                                                   \
     return 0;                                                                 \
}


SERVICES_FUNC(m_nickserv, cNickServ);
SERVICES_FUNC(m_chanserv, cChanServ);
SERVICES_FUNC(m_memoserv, cMemoServ);
SERVICES_FUNC(m_operserv, cOperServ);
SERVICES_FUNC(m_infoserv, cInfoServ);
SERVICES_FUNC(m_gameserv, cGameServ);


__inline int
m_sendto_service(cptr, sptr, parc, parv, fOper, sService)
aClient	*cptr, *sptr;
	int	parc;
	char	*parv[];
	int	fOper;
const	char *sService;
{
       aClient *acptr;

       if (check_registered_user(sptr))
               return 0;

       if (parc < 2 || *parv[1] == '\0') {
               sendto_one(sptr, err_str(ERR_NOTEXTTOSEND), me.name, parv[0]);
               return -1;
       }
       if ( (acptr = find_person((char *)sService, NULL)) && !MyConnect(acptr) &&
             IsULine(acptr, acptr))
       {
               if (fOper && !IsAnOper(sptr) && MyClient(sptr))
                  return -2;
                   sendto_one(acptr,":%s PRIVMSG %s@%s :%s", parv[0],
                           sService, SERVICES_NAME, parv[1]);
       }
       else
               sendto_one(sptr, err_str(ERR_SERVICESDOWN), me.name,
                       parv[0], sService);
       return 0;
}
