/************************************************************************
 *   IRC - Internet Relay Chat, ircd/help.c
 *   Copyright (C) 1996 DALnet
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
#ifndef lint
static char sccsid[] = "@(#)help.c	6.00 9/22/96 (C) 1996 DALnet";
#endif

#include <sys/types.h>
#include <sys/file.h>
#include <string.h>

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "h.h"
#include "msg.h"

int h_nignores = 0;
char **h_ignores;

#define HDR(str) sendto_one(sptr, ":%s 290 %s :" str "", me.name, sptr->name)
#define SND(str) sendto_one(sptr, ":%s 291 %s :" str "", me.name, sptr->name)
#define FTR(str) sendto_one(sptr, ":%s 292 %s :" str "", me.name, sptr->name)
#define HLP(str) sendto_one(sptr, ":%s 293 %s :" str "", me.name, sptr->name)

int helpop_ignored(aClient *cptr)
{
  char buf[NICKLEN + USERLEN + HOSTLEN + 10] = "";
  int i = 0;

  if (!cptr->user || !IsPerson(cptr) || !h_nignores)
      return (0);
  sprintf(buf, "%s!%s@%s", cptr->name, cptr->user->username, cptr->user->host);
  for ( i = 0 ; i < h_nignores; i++ )
        if (!match(h_ignores[i], buf))
            return (1);
  return (0);
}

void helpop_ignore(char *mask)
{
   char buf[USERLEN + HOSTLEN + NICKLEN + 10];
   int i;

   strncpy(buf, mask ? mask : "", sizeof(buf));
   buf[sizeof(buf) - 1] = '\0';

   for ( i = 0 ; i < h_nignores ; i++ )
        if (!mycmp(h_ignores[i], mask))
             return;
   if (h_ignores)
       h_ignores = realloc(h_ignores, sizeof(char *) * (h_nignores + 3));
   else
       h_ignores = (char **)MyMalloc(sizeof(char *) * (h_nignores + 3));
   ++h_nignores;
   DupString(h_ignores[h_nignores-1], buf);
   h_ignores[h_nignores] = NULL;
}

void helpop_unignore(int num)
{
   int i = 0;

   if (num < 0 || num > h_nignores)
       return;
   for (i = num; i < h_nignores; i++)
   {
        if (i == num)
            MyFree(h_ignores[i]);
        h_ignores[i] = h_ignores[i+1];
   }
   --h_nignores;
   h_ignores[h_nignores] = NULL;
   h_ignores = realloc(h_ignores, sizeof(char *) * (h_nignores + 3));
}


int nohelp_message(aClient *sptr, int g)
{
   int going_nowhere = helpop_ignored(sptr);

     if (going_nowhere || !g)
        SND("Your request was not forwarded to the helpops.");
     else
        SND("Your request has been forwarded to the Helpops!");
        SND("for further help, please type one of the following");
        SND("commands:");
        SND("   \2/join #Help\2             <-- for human assistance");
        SND("   \2/raw help chanserv\2      <-- for index of channel topics");
        SND("   \2/raw nickserv nickserv\2  <-- for index of nick topics");
        SND("   \2/raw memoserv memoserv\2  <-- for index of memo topics");
        SND("   ircII users use \2/quote\2, pIRCH users use \2/verbose\2");
        SND("   instead of raw.");
    return (!going_nowhere);
}



/*
 * This is _not_ the final help.c, we're just testing the functionality...
 * Return 0 to forward request to helpops... -Donwulff
 */
int parse_help (sptr, name, help)
aClient	*sptr;
char	*name;
char	*help;
{
int fwdstat = 0;

if (help && *help == '!')
{
    ++help;
    fwdstat = -1;
}
else if (help && *help == '?')
{
    ++help;
    fwdstat = 1;
}

  if(!myncmp(help, "NICKSERV", 8)) {
SND(" ***** NickServ Help *****");
    if(!*(help+8) || !mycmp(help+9, "HELP")) {

SND("     \37NickServ\37 is a service which enables users to have a");
SND("     a regular nickname whenever they use SorceryNet. NickServ");
SND("     allows you to register, protect, and configure your");
SND("     nickname settings.                                 ");
SND("                                                        ");
SND("     \2COMMANDS\2");
SND("     General Access");
SND("     HELP       REGISTER           ");
SND("     INFO       RELEASE       GHOST");
/*SND("     Restricted/Activated Nicknames only");*/
SND("     ADDMASK    ACCESS        IDENTIFY/ID");
SND("     SET        DROP");

} else
    if(!mycmp(help+9, "SET")) {

SND(" === \2SET\2 ===");
SND("     SYNTAX:");
SND("     1:  SET <command> | [option]");
SND("     \2DESCRIPTION\2");
SND("     NickServ SET allows you to change options for your");
SND("     nickname.  You must be identifed with NickServ to");
SND("     activate or deactivate these settings.");
SND("     \2COMMANDS\2");
SND("     ADDRESS   EMAIL   KILL   PASSWD   URL");


} else
  if(!myncmp(help+9, "SET ", 4)) {
    if(!mycmp(help+13, "URL")) {
SND(" === \2URL\2 ===");
SND("     \2SYNTAX\2:");
SND("     1:  set URL <url>");
SND("       ex. /msg NickServ set URL http://www.sorcery.net");
SND("     2:  set URL");
SND("       ex. /msg NickServ set URL");
SND("     \2DESCRIPTION\2");
SND("     Sets the URL line which can be requested");
SND("     by anyone using /msg NickServ info <nickname>.");
SND("     Ommiting the URL clears the URL line from the");
SND("     NickServ database.");

} else
    if(!mycmp(help+13, "KILL")) {

SND(" === \2KILL\2 ===");
SND("     \2SYNTAX\2:");
SND("     1: KILL [on | off]");
SND("       ex. /msg NickServ set KILL on");
SND("           /msg NickServ set KILL off");
SND("     \2DESCRIPTION\2");
SND("     Kill is used a protection feature for your");
SND("     nickname.  If someone connects to Sorcery");
SND("     Net using your nick with kill set ON, the");
SND("     person will be disconnected from the server");
SND("     if they cannot or do not identify with NickServ.");

} else
    if(!mycmp(help+13, "PASSWD")) {
SND(" === \2PASSWD\2 ===");
SND("     \2SYNTAX\2:");
SND("     1:  set PASSWD [new password]");
SND("       ex. /msg NickServ set PASSWD icecream");
SND("     DESCRIPTION");
SND("     Updates the NickServ database with your new password.");

} else
    if(!mycmp(help+13, "EMAIL")) {
SND(" === \2EMAIL\2 ===");
SND("     \2SYNTAX\2:");
SND("     1:  EMAIL [on | off]");
SND("       ex. /msg NickServ set email on");
SND("       ex. /msg NickServ set email off");
SND("     \2DESCRIPTION\2");
SND("     Toggles the ability of those viewing NickServ");
SND("     info to see your e-mail address.");
} else
    if(!mycmp(help+13, "ADDRESS")) {
SND(" === \2ADDRESS\2 ===");
SND("     \2SYNTAX\2:");
SND("     1:  set ADDRESS <e-mail address>");
SND("       ex. /msg NickServ set ADDRESS sleepy@flame.org");
SND("     \2DESCRIPTION\2");
SND("     Resets the e-mail address for your nickname in");
SND("     the NickServ database.                        ");


} else SND("Try \2/raw help NICKSERV SET\2 for list of available settings.");
  } else
    if(!mycmp(help+9, "REGISTER")) {

SND(" === \2REGISTER\2 ===");
SND("     \2SYNTAX\2:");
SND("     1:  REGISTER <your password>");
SND("       ex. /msg NickServ REGISTER chocolate");
SND("     DESCRIPTION");
SND("     \37Nickserv\37 REGISTER allows you to 'own' a nickname with");
SND("     SorceryNet's services, this nickname is your own and");
SND("     allows you to have and configure your own aliases for");
SND("     your needs and desires.");

} else
    if(!mycmp(help+9, "INFO")) {

SND(" === \2INFO\2 ===");
SND("     \2SYNTAX\2:");
SND("     1:  INFO <nickname>");
SND("       ex. /msg NickServ INFO Sleepy");
SND("     DESCRIPTION");
SND("     Gives information for the specified nickname.");
SND("     Do not include a nickname to get information");
SND("     for the nickanem you are currently using.");

} else
    if(!mycmp(help+9, "IDENTIFY") || !mycmp(help+9, "ID")) {
SND(" === \2ID/IDENTIFY\2 ===");
SND("     \2SYNTAX\2:");
SND("     1:  IDENTIFY <your password>");
SND("       ex. /msg NickServ IDENTIFY chocolate");
SND("     DESCRIPTION");
SND("     Tells NickServ you are the 'legal' owner of the");
SND("     nickname and allows you full access to change");
SND("     settings for the nickname.");

} else
    if(!mycmp(help+9, "RECOVER")) {

SND("Command - RECOVER");
SND("Usage   - RECOVER <nick> [<password>]");
SND("  This command allows those who have registered nicks which have");
SND("  been taken because they didn't set the kill switch to ON a chance");
SND("  to revenge themselves.  It issues a manual version of the same");
SND("  kill used in the automatic nick kill.  This of course means that");
SND("  you will need to use the NickServ RELEASE command to immediately");
SND("  regain the nick, otherwise it will be held for the usual 2 mins.");
SND("  A check in this command prevents you from killing yourself. :)");

} else
    if(!mycmp(help+9, "GHOST")) {

SND(" === \2GHOST\2 ===");
SND("     \2SYNTAX\2:");
SND("     1:  GHOST <nickname>");
SND("       ex.   /msg NickServ GHOST Sleepy");
SND("     2:  GHOST <nickname> [password]");
SND("       ex.   /msg NickServ GHOST Sleepy chocolate");
SND("     \2DESCRIPTION\2");
SND("     GHOST removes the specified nickname, with a kill.  This");
SND("     enables you to remove 'ghosted' connections, *and* others");
SND("     using your nick.  The password is required for the ghost");
SND("     command if you do not match the nickname's access list.");

} else
    if(!mycmp(help+9, "DROP")) {

SND(" === DROP ===");
SND("     SYNTAX:");
SND("     1:  DROP");
SND("       /msg NickServ DROP");
SND(" ");
SND("     DESCRIPTION");
SND("     NickServ DROP removes a nickname (and outstanding");
SND("     memos) from SorceryNet's services.  You must be");
SND("     identified with NickServ to use this command.");

} else
    if(!mycmp(help+9, "RELEASE")) {

SND(" === \2RELEASE\2 ===");
SND("     \2SYNTAX\2:");
SND("     1:  RELEASE <nickname>");
SND("       ex. /msg NickServ RELEASE SLEEPY");
SND("     2:  RELEASE <nickname> [password]");
SND("       ex. /msg NickServ RELEASE Sleepy chocolate");
SND("     \2DESCRIPTION\2");
SND("     RELEASE removes a NickServ ghost, thus allowing");
SND("     the nick to be used.  If you do not match the");
SND("     nickname's access list, you must use your password.");


} else
    if(!mycmp(help+9, "ACCESS")) {
SND(" === \2ACCESS, ACCESS LIST, ACCESS ADD, and ACCESS DEL\2 ===");
SND("     1:  /msg NickServ ACCESS LIST <nickname>");
SND("         ex:  /msg nickserv ACCESS evenstar");
SND("     2:  /msg NickServ ACCESS ADD <user@host>");
SND("         ex:  /msg nickserv ACCESS ADD star@starlite.net");
SND("     \2DESCRIPTION\2");
SND("     The ACCESS command allows users to manipulate their access");
SND("     lists.  ACCESS LIST lists all users masks for the nickname.");
SND("     ACCESS ADD is for accessing the users nickname without having");
SND("     to identify when using the added mask.  ACCESS DEL is used to");
SND("     remove unwanted masks from the access list.");


} else
    if(!mycmp(help+9, "ADDMASK")) {
SND(" === \2ADDMASK\2 ===");
SND("     \2SYNTAX\2:");
SND("     1:  ADDMASK");
SND("       /msg NickServ ADDMASK");
SND("     \2DESCRIPTION\2");
SND("     The ADDMASK command adds the mask currently in");
SND("     use by your nickname to your NickServ access list.");
SND("     For more help on masks, please see the ACCESS command.");


} else
    if(!mycmp(help+9, "ACC")) {

SND("Command - ACC");
SND("Usage   - ACC <nick>");
SND("  This command serves as a protocol for bots to query user access");
SND("  using the NickServ facility, thus relieving the bot of");
SND("  keeping track of access lists.  The reply is givin in a NOTICE");
SND("  of the following format:");
SND("");
SND("        ACC <nick> <access level>");
SND("");
SND("  The returned access level is a number from 0 to 2:");
SND("   0 = No such registered nickname");
SND("   1 = User is not online or not identified to NickServ");
SND("   2 = User is identifed with NickServ access");

} else
    if(!mycmp(help+9, "LIST")) {

SND("Command - LIST");
SND("Usage   - LIST <search pattern>");
SND("  This command allows searching of registered nicks by both the");
SND("  nicks themselves and the last valid address that the nick was");
SND("  recognzied as.");
SND("  Examples:");
SND("      LIST *Bob*");
SND("      LIST *@*.netcom.com");

} else
    SND("I do not know what you mean. Try NICKSERV HELP instead.");

  } else if(!myncmp(help, "CHANSERV", 8)) {
SND(" ***** ChanServ Help *****");
    if(!*(help+8) || !mycmp(help+9, "HELP")) {

SND("     Channel Services provides users with the ability to   ");
SND("     register channels.  ChanServ allows you to customize  ");
SND("     your channel to suit your needs, eliminating the      ");
SND("     need for bot.                                         ");
SND(" ");
SND("    \2Base Commands\2                                      ");
SND("    REGISTER     -Registers a channel                      ");
SND("    DROP         -Releases a registered channel            ");
SND("    ADDOP        -Add to the Channel Operator list         ");
SND("    DELOP        -Delete from the Channel Operator list    ");
SND("    LISTOP       -List current Channel Operators           ");
SND("    SET          -Maintains various channel options        ");
SND(" ");
SND("    For more information:  /quote help ChanServ <command>  ");
SND("    \2Other Commands\2                                     ");
SND("    RESTRICT          DESC         OPGUARD         INFO    ");

} else
    if(!mycmp(help+9, "REGISTER")) {

SND(" === \2REGISTER\2 ===");
SND("     SYNTAX:");
SND("     1:  REGISTER <channel> <password> [description]");
SND("       ex.  /msg ChanServ REGISTER #sorcery blah IRCops idle here");
SND("     DESCRIPTION");
SND("     REGISTER allows a user to \"own\" a channel.  Registering");
SND("     your channel makes it almost impossible for anyone to");
SND("     take over your channel, eliminating the need for bots.");
SND("     The founder registers the channel with a password, then");
SND("     he or she can change the channel settings, and add");
SND("     Operators of different access levels.");
SND("     Passwords are case sensitive");
SND("     Channel registrations are limited to 5 per user.");


} else
    if(!mycmp(help+9, "SET")) {

SND(" === \2SET\2 ===");
SND("     Set offers various ways of configuring your");
SND("     channel settings to suit your needs.");
SND("     For more information on each set command:");
SND("     /msg ChanServ help set <command>");
SND("     ex. /msg ChanServ help set opguard");
SND("     \2COMMANDS\2");
SND("         OPGUARD     DESC      KEEPTOPIC");
SND("         PROTOP      QUIET     PASSWORD/PASSWD");


} else
    if(!mycmp(help+9, "ADDOP")) {
SND("     \2SYNTAX\2:");
SND("     1:  ADDOP <channel> <nickname> <level>");
SND("       ex. /msg Chanserv ADDOP #sorcery Sleepy 8");
SND("     DESCRIPTION");
SND("     ADDOP adds a user to the channel access list.  The level");
SND("     determines how much access the user has, and their ability to");
SND("                change");
SND("                channel settings");
SND("    \2LEVELS\2");
SND(" 3 Mini-AOP +v status       8 Mini-SOP   AOP abilities,adds AKICKs");
SND(" 5 AOP      Automatic ops   10 SOP       Level 8,adds AOPs.");
SND(" 13 Mini-Founder  Access determined by founder.");
SND(" 15 Founder       Channel owner.  Can make any changes to channel.");
SND(" Cannot be reset without channel password.");


} else
    if(!mycmp(help+9, "DELOP")) {
SND(" === \2DELOP\2 ===");
SND("     \2SYNTAX\2:");
SND("     1:  DELOP <channel> <nickname>");
SND("       ex.  /msg ChanServ DELOP #sorcery Sleepy");
SND("     \2DESCRIPTION\2");
SND("     Removes all channel access for a user.");

} else
    if(!mycmp(help+9, "AKICK")) {

SND("Command - AKICK");
SND("Usage   - AKICK <channel> ADD <nick or mask>");
SND("          AKICK <channel> DEL <index number or mask>");
SND("          AKICK <channel> LIST [<search pattern>]");
SND("  Maintains the channel AutoKick list. These users will be");
SND("  forcibly removed (kicked) and banned from the channel if");
SND("  they join it. Limited to SuperOp access.ADD adds a user to a");
SND("  channels AutoKick access list. DEL removes a user from a channel's");
SND("  AutoKick access list.  Index number is found using the LIST subcommand.");
SND("  LIST lists the AutoKick access list. When used with the optional search");
SND("  pattern, it will only show those masks matching the specified pattern.");
SND("  Examples:");
SND("     AKICK #dragonrealm ADD dalvenjah");
SND("     AKICK #dragonrealm ADD *!besmith@*.uncc.edu");
SND("     AKICK #dragonrealm DEL 3");
SND("     AKICK #dragonrealm LIST");

} else
    if(!mycmp(help+9, "DROP")) {
SND("     \2SYNTAX\2:");
SND("     1:  DROP <#channel>");
SND("       ex. /msg ChanServ DROP #Sorcery");
SND("     \2DESCRIPTION\2");
SND("     DROP removes a channel registration from the ChanServ");
SND("     database.  You must be using the nickname used to register");
SND("     the channel and identiied with Services to use this command.");

} else
    if(!mycmp(help+9, "LISTOP")) {
SND(" === \2LISTOP\2 ===");
SND("     \2SYNTAX\2:");
SND("     1:  LISTOP <#channel> [level]");
SND("       ex. /msg ChanServ LISTOP #sorcery 5");
SND("           /msg ChanServ LISTOP #sorcery");
SND("");
SND("     \2DESCRIPTION\2");
SND("     LISTOP lists users with access to the channel.  If a level");
SND("     is given, ChanServ lists all channel users with that access");
SND("     level.  If the level is omitted, all users with access to the");
SND("     channel are listed.");


} else
    if(!mycmp(help+9, "IDENTIFY")) {

SND("Command - IDENTIFY");
SND("Usage   - IDENTIFY <channel> <password>");
SND("  Allows a channel founder to gain Founder access to the channel's");
SND("  data even if the access mask fails to recognize the person.");

} else
    if(!mycmp(help+9, "OPGUARD")) {
SND(" === \2OPGUARD\2 ===");
SND("     \2OPGUARD\2:");
SND("     1:  set <channel> OPGUARD <on|off>");
SND("       ex. /msg ChanServ set #channel OPGUARD on");
SND("     \2DESCRIPTION\2");
SND("     OPGUARD is a channel setting which only allows those");
SND("     users with access levels set in ChanServ to have Ops");
SND("     in the channel.");

} else
    if(!mycmp(help+9, "DESC")) {
SND(" === \2DESC\2 ===");
SND("     \2SYNTAX\2:");
SND("     1:  SET <#channel> DESC <new description>");
SND("       ex. /msg ChanServ SET #Sorcery DESC Where the IRCops are.");
SND("     \2DESCRIPTION\2");
SND("     \37DESC\37 is used to change the channel description which is");
SND("     set at the time of registration.  Description information");
SND("     can be viewed using the \37INFO\37 command. The \37DESC\37 command is");
SND("     restricted to those with founder access to the channe.");


} else
    if(!mycmp(help+9, "RESTRICT")) {
SND(" === \2RESTRICT\2 ===");
SND("     \2SYNTAX\2:");
SND("     1:  RESTRICT <#channel> <level>");
SND("       ex. /msg ChanServ RESTRICT #Sorcery 8");
SND("       ex. /msg ChanServ RESTRICT OFF");
SND("     \2DESCRIPTION\2");
SND("      \37RESTRICT\37 is used to set a minimum level for which");
SND("      nicks can join the channel.  If a nick attempts to");
SND("      join a level restricted channel which their level");
SND("      is NOT high enough then they will be auto-kicked and");
SND("      banned from the channel.");
SND("      \2For a list of access levels /msg ChanServ help LEVEL\2");


} else
    if(!mycmp(help+9, "ACCESS")) {

SND("Command - ACCESS");
SND("Usage   - ACCESS <channel> [<nick>]");
SND("  Allows a user to query his/her access to a registered channel.");
SND("  Access levels are Basic, AutoOp, SuperOp, and Founder for any");
SND("  given channel.When used with the optional <nick> variable, this");
SND("  command serves as a protocol for bots to query user access");
SND("  on a channel using the ChanServ facility, thus relieving the");
SND("  bot of keeping track of access lists.  For a registered channel,");
SND("  the reply is given in a NOTICE of the following format:");
SND("");
SND("       ACC <channel> <nick> <user@host> <access level>");
SND("");
SND("  The returned access level is a number from 0 to 3, where");
SND("  0=basic, 1=AutoOp, 2=SuperOp, and 3=founder. If");
SND("  the user is not online, the user@host and access level will");
SND("  be *UNKNOWN* and 0, respectively.");
SND("  This command is limited to channel AutoOps.");

} else
    if(!mycmp(help+9, "OP")) {

SND("Command - OP");
SND("Usage   - OP <channel> [-]<nick>...");
SND("  Grants operator status to named users. When used with the - flag,");
SND("  deops the named users. Limited to channel AutoOps and above.");

} else
    if(!mycmp(help+9, "UNBAN")) {

SND("Command - UNBAN");
SND("Usage   - UNBAN <channel> [ME|ALL]");
SND("  Allows a user to remove bans against him/her on a registered");
SND("  channel.  Limited to channel AutoOps.");
SND("  When used with the optional ALL option, removes all bans on");
SND("  a registered channel. Limited to channel SuperOps.");

} else
    if(!mycmp(help+9, "INFO")) {
SND(" === \2INFO\2 ===");
SND("     \2SYNTAX\2:");
SND("     1:  INFO <channel>");
SND("       ex.  /msg ChanServ info #sorcery");
SND("     \2DESCRIPTION\2");
SND("     INFO provides channel information. INFO gives the date the");
SND("                channel was registered, when it was last used, the founders");
SND("    provides other information such as the last description, the last");
SND("     topic, and mode lock settings.");


} else
    if(!mycmp(help+9, "INVITE")) {

SND("Command - INVITE");
SND("Usage   - INVITE <channel> [<nickname>]");
SND("  Invites the sender, or the named user, to a channel that is set to");
SND("  mode +i. Limited to channel AutoOps and above.");

} else
    if(!mycmp(help+9, "LIST")) {

SND("Command - LIST");
SND("Usage   - LIST <search pattern>");
SND("  Lists matching channels that have been registered with");
SND("  ChanServ");

} else
    if(!mycmp(help+9, "MDEOP")) {

SND("Command - MDEOP");
SND("Usage   - MDEOP <channel>");
SND("  Removes operator status from all users on the named channel who");
SND("  don't outrank the user. Limited to channel AutoOps and above.");

} else
  if(!myncmp(help+9, "SET ", 4)) {
    if(!mycmp(help+13, "OPGUARD")) {

SND(" === \2SET Opguard\2 ===");
SND("     \2OPGUARD\2:");
SND("     1:  SET <channel> OPGUARD <on|off>");
SND("       ex. /msg ChanServ SET #channel OPGUARD on");
SND("     \2DESCRIPTION\2");
SND("     \2OPGUARD\2 is a channel setting which only allows those");
SND("     users with access levels set in ChanServ to have Ops");
SND("     in the channel.");

} else
    if(!mycmp(help+13, "PROTOP")) {

SND(" === \2PROTOP\2 ===");
SND("     \2SYNTAX\2:");
SND("     1:  SET <#channel> PROTOP <ON | OFF>");
SND("       ex. /msg ChanServ SET #Sorcery PROTOP ON");
SND("     \2DESCRIPTION\2");
SND("     PROTOP keeps all people in the ops list");
SND("     of the specified channel Op'd");

} else
    if(!mycmp(help+13, "DESC")) {
SND(" === \2DESC\2 ===");
SND("     \2SYNTAX\2:");
SND("     1:  SET <#channel> DESC <new description>");
SND("       ex. /msg ChanServ SET #Sorcery DESC Where the IRCops are.");
SND("     \2DESCRIPTION\2");
SND("     DESC is used to change the channel description which is");
SND("     set at the time of registration.  Description information");
SND("     can be viewed using the INFO command. The DESC command is");
SND("     restricted to those with founder access to the channe.");
} else
    if(!mycmp(help+13, "QUIET")) {
SND(" === \2QUIET\2 ===");
SND("     \2SYNTAX\2:");
SND("     1:  SET <#channel> QUIET <ON | OFF>");
SND("       ex. SET #Sorcery QUIET ON");
SND("     \2DESCRIPTION\2");
SND("     QUIET prevents services from sending a notice to");
SND("     members of the channel when a change is made to");
SND("     the settings of the channel.");


} else
    if(!mycmp(help+13, "OPGUARD")) {

SND("Command - SET OPGUARD");
SND("Usage   - SET <channel> OPGUARD [ON|OFF]");
SND("  Turns on/off ops guarding on a channel.  When ON, only");
SND("  AutoOps, SuperOps, and the channel founder will be allowed");
SND("  ops on the channel.  Limited to channel founder.");

} else
    if(!mycmp(help+13, "KEEPTOPIC")) {

SND(" === \2KEEPTOPIC\2 ===");
SND("     \2SYNTAX\2:");
SND("     1:  SET <#channel> KEEPTOPIC <ON | OFF>");
SND("       ex. /msg ChanServ set #Sorcery KEEPTOPIC ON");
SND("     \2DESCRIPTION\2");
SND("     KEEPTOPIC locks the channel topic.  The topic may");
SND("     NOT be changed until KEEPTOPIC is toggled to OFF.");

} else
    if(!mycmp(help+13, "PRIVATE")) {

SND("Command - SET PRIVATE");
SND("Usage   - SET <channel> PRIVATE [ON|OFF]");
SND("  Turns on/off the privacy option for a channel.  When");
SND("  ON, ChanServ will keep a channel's existance secret from");
SND("  those who do not explicitly know about it. It will also");
SND("  prevent the INVITE command from being used. Limited to");
SND("  channel founder.");

} else
    if(!mycmp(help+13, "PASSWD") || !mycmp(help+13, "PASSWORD")) {
SND(" === \2PASSWD\2 ===");
SND("     \2SYNTAX\2:");
SND("     1:  SET <#channel> PASSWD <new password>");
SND("       ex. /msg ChanServ SET #Sorcery PASSWD icecream");
SND("     \2DESCRIPTION\2");
SND("     PASSWD changes the password set during the");
SND("     registration process.");


/*} else
*    if(!mycmp(help+13, "LEAVEOPS")) {
*
*SND("Command - SET LEAVEOPS");
*SND("Usage   - SET <channel> LEAVEOPS [ON|OFF]");
*SND("  Turns on/off leave-ops behavior on a channel.  When ON, the");
*SND("  channel will behave as if ChanServ was not present, and will");
*SND("  not deop users who establish the channel. Limited to");
*SND("  channel founder.");
*
*/
} else
    if(!mycmp(help+13, "TOPICLOCK")) {

SND("Command - SET TOPICLOCK");
SND("Usage   - SET <channel> TOPICLOCK [FOUNDER|SOP|OFF]");
SND("  Sets the \"topic lock\" option for a channel. When on, only the");
SND("  founder or SuperOps (depending on the option) are able to change");
SND("  the topic. This setting also performs the function of the");
SND("  KEEPTOPIC command. Limited to channel founder.");
/*
 *} else
 *   if(!mycmp(help+13, "URL")) {
 *
 *SND("Command - SET URL");
 *SND("Usage   - SET <channel> URL [<url>]");
 *SND("  Allows a URL (Uniform Resource Locator) to be set, indicating");
 *SND("  where more information on the channel may be found. Limited to");
 *SND("  channel founder. For example:");
 *SND("   SET #dragonrealm URL http://www.dal.net/");
 *SND("   SET #dragonrealm URL mailto:info@dal.net    // An email address");
 *SND("   SET #dragonrealm URL                        // Remove the URL");
*/
} else SND("Try CHANSERV SET for list of available settings.");
  }
	} else
	if(!myncmp(help, "MEMOSERV", 8) || !myncmp(help, "MEMOSERV ", 9) ) {
SND(" ***** MemoServ Help *****");
    if(!*(help+8) || !mycmp(help+9, "HELP")) {

SND("     Memo Services provide users with the ability to send");
SND("     short notes to other registered nicknames on Sorcery");
SND("     Net.  Memos are limited to 350 characters");
SND("     \2MemoServ is currently under development\2,");
SND("      some commands may not be available.");
SND("                              ");
SND("     \2BASE COMMANDS\2");
SND("      SEND     -Sends a Memo to a specific Nickname");
SND("      READ     -Read a Memo");
SND("      LIST     -Lists all Memos");
SND("      DELETE   -Deletes Memos");
SND("                              ");
SND("     \2OTHER COMMANDS\2");
SND("      CLEAN      MARK      UNSEND");
SND("      SET        REPLY     REDIRECT");
SND("      RETRIEVE   FORWARD");

} else
    if(!mycmp(help+9, "SEND")) {

SND(" === \2SEND\2 ===");
SND("     \2SYNTAX\2:");
SND("     1:  SEND");
SND("     2:  SEND <nick> [message]");
SND("     3:  etc...");
SND("                                                      ");
SND("     \2DESCRIPTION\2");
SND("               send message to specified nickname");

} else
    if(!mycmp(help+9, "LIST")) {
SND(" === \2LIST\2 ===");
SND("     \2SYNTAX\2:");
SND("     1:  LIST");
SND("       ex. /msg MemoServ LIST");
SND("          ");
SND("     \2DESCRIPTION\2");
SND("     LIST returns a list of current Memos in the memobox.");

} else
    if(!mycmp(help+9, "READ")) {

SND(" === \2READ\2 ===");
SND("     \2SYNTAX\2:");
SND("     1:  READ <number>");
SND("       ex. /msg MemoServ READ 3");
SND("                                ");
SND("     \2DESCRIPTION\2");
SND("      READ calls the Memo specified for reading.");
SND("                                ");
SND("      To get a list of Memos \2/msg MemoServ LIST\2");

} else
    if(!mycmp(help+9, "DEL")) {

SND("Command - \2DEL\2");
SND("Usage   - \2DEL\2 <memo number>");
SND("          \2DEL ALL\2");
SND("  Deletes memo <memo number> (as given by the LIST command)");
SND("  from your memo list. DEL ALL deletes and purges all of your");
SND("  memos without restriction, so use it with care.");
SND("  Note: DEL does not actually remove the memo, but simply");
SND("  marks it as deleted (indicated with D in the LIST display).");
SND("  The memo is not removed until you sign off or the PURGE");
SND("  command is used.");

} else
    if(!mycmp(help+9, "DELETE")) {

SND(" === \2DELETE\2 ===");
SND("     \2SYNTAX\2:");
SND("     1:  DELETE <number>");
SND("       ex. /msg MemoServ DELETE 3");
SND("     \2DESCRIPTION\2");
SND("     DELETE is used to remove the specified memo");
SND("     from the memobox.");
SND("     To get a list of Memos /msg MemoServ LIST");


} else
    if(!mycmp(help+9, "CLEAN")) {
SND(" === \2CLEAN\2 ===");
SND("     \2SYNTAX\2:");
SND("     1:  CLEAN");
SND("       ex. /msg MemoServ CLEAN");
SND("          ");
SND("     \2DESCRIPTION\2");
SND("     CLEAN clears ALL Memos from the memobox.");


}
/*} else
    if(!mycmp(help+9, "FORWARD")) {

SND("Command - FORWARD");
SND("Usage   - FORWARD");
SND("          FORWARD -");
SND("          FORWARD <nickname> [password]");
SND("  Allows memos sent to one registered nickname to be forwarded to");
SND("  another. The first command will tell you if memos to your");
SND("  current nick are being forwarded. The second will disable");
SND("  forwarding for memos to your current nick. The third command");
SND("  will establish forwarding for memos to your present nickname;");
SND("  this command requires you to identify to your current nick and");
SND("  may require you to specify the password of the nickname you wish");
SND("  to begin forwarding to.");
}
*/
  } else { /* Flood back the user ;) */
    return 0;
  }
  SND(" ***** Go to #sorcerynet if you have any further questions *****");
  return 1;
}
