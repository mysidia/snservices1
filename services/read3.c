#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "queue.h"

#define COPGUARD	0x00000001
#define CKTOPIC		0x00000002
#define CLEAVEOPS	0x00000004
#define CQUIET 		0x00000008
#define CCSJOIN		0x00000010
#define CIDENT		0x00000020
#define CHOLD		0x00000040
#define CREG		0x00000080
#define CBANISH		0x00000100
#define CPROTOP		0x00000200
#define CMARK		0x00000400
#define CCLOSE		0x00000800

#define strncpyzt(dd,ff,nn) \
	do { \
		strncpy(dd,ff,nn); \
		dd[nn - 1] = '\0'; \
	} while(0)

#define MIN(x,y) ((x)<(y)?(x):(y))
#define MAX(x,y) ((x)>(y)?(x):(y))

#define NBANISH   0x0100
#define NHOLD     0x0004
#define NVACATION 0x0002
#define GNUPLOT_DAT
#define MAILSTAT_WRITE
#define NZERO(x) ((x)?(x):1)

int main(void);
char *sfgets(char *, int, FILE *);
void *oalloc(size_t);

int email=0, email_valid=0;
int nicks=0, usernicks=0;
int urls=0;
int access=0;
int held=0;
time_t CTime;

typedef struct emaillist_struct EmailList;

struct emaillist_struct {
	char email[128];
	LIST_ENTRY(emaillist_struct) emails;
};

typedef struct
{
	char *current;
	char *start;
} ParseState;

typedef struct akill Akill;

struct akill
{
	time_t setat, unset;
	int type;
	LIST_ENTRY(akill) ak_lst;

	char nick[80], user[20], host[80];
	char setby[80], reason[255];
};

typedef struct urllist_struct UrlList;

struct urllist_struct {
	char url[128];
	LIST_ENTRY(urllist_struct) urls;
};

typedef struct regnicklist RegNickList;

struct regnicklist {
	char nick[33];
	unsigned long reged;
	char realname[128];
	u_int32_t flags;
	LIST_ENTRY(regnicklist) users;
};


typedef struct regchanlist RegChanList;

struct regchanlist {
	char chan[255];
	time_t ts;
	u_int32_t flags, mlock;
	int ops, akicks;

	LIST_ENTRY(regchanlist) chans;
};

char *get_arg(ParseState *);

LIST_HEAD(,regnicklist)       firstUser;
LIST_HEAD(,regnicklist)	      heldNick;
LIST_HEAD(,urllist_struct)    firstUrl;
LIST_HEAD(,emaillist_struct)  firstEmail;
LIST_HEAD(,regchanlist)       chanList;
LIST_HEAD(,akill)	      akList;

int chanstats()
{
	FILE *fp;
	u_int		 thechans[33], theakicks[100], theops[200];
	u_int		 theflags[32], themlocks[32];
	u_int		 thedays[50];
	u_int		 opitems = 0, akitems = 0;
	u_int		 chans = 0, urls = 0;
	u_int		 descs = 0, munged = 0;

	char		*command;
	char 		 line[1024];
	ParseState	*state = (ParseState *)oalloc(sizeof(ParseState));
	int a, done = 0;
	RegChanList	*cp = NULL;

	for (a = 0; a < 33; a++)
	  thechans[a] = 0;

	for (a = 0; a < 32; a++) {
	  theflags[a] = 0;
	  themlocks[a] = 0;
	}
	  
	for(a = 0; a < 50; a++)
	  thedays[a] = 0;

	for(a = 0; a < 100; a++)
	  theakicks[a] = 0;

	for(a = 0; a < 200; a++)
	  theops[a] = 0;
	  
	fp = fopen("chanserv/chanserv.db", "r");
	if(!fp)
	  exit(-1);
	while(!done) {
		if (!(sfgets(line, 1024, fp))) {
			done = 1;
			break;
		}
		state->current = state->start = line;
		command = get_arg(state);
		if(!strcmp(command, "channel")) {
			cp = oalloc(sizeof(RegChanList));
			command = get_arg(state);

			strncpy(cp->chan, command, 255);
			cp->chan[254] = '\0';

			thechans[MAX(0, MIN(32, strlen(cp->chan) - 1))]++;


			command = get_arg(state); // founder
			if (command && command[0] == '-')
				munged++;

			command = get_arg(state); // mlock
			cp->mlock = strtoul(command, (char **)0, 10);

			command = get_arg(state); // flags
			cp->flags = strtoul(command, (char **)0, 10);

			get_arg(state); // password
			get_arg(state); // regtime
			command = get_arg(state); // last time
			get_arg(state);	// key
			get_arg(state);	// limit
			get_arg(state);	// memolevel
			get_arg(state);	// tlock
			get_arg(state);	// restrict
			command = get_arg(state); // desc
			if (command && *command && command[1])
				descs++;

			cp->ts = atoi(command);

			LIST_INSERT_HEAD(&chanList, cp, chans);
			chans++;
		}
		else if (!strcmp(command, "url")) {
			urls++;
		}
		else if (!strcmp(command, "desc")) {
			descs++;
		}
		else if(!strcmp(command, "op")) {
			if (cp)
				cp->ops++;
			opitems++;
		}
		else if(!strcmp(command, "akick")) {
			if (cp)
				cp->akicks++;
			akitems++;
		}
	}

	for(cp = LIST_FIRST(&chanList); cp; cp = LIST_NEXT(cp, chans))
	{
		time_t workingtime = (CTime - cp->ts);
		int dayssince = ((3600*24*15) - workingtime) / (3600*24);

		thedays[MIN(15, MAX(0, dayssince))]++;
		theops[MIN(199, cp->ops)]++;
		theakicks[MIN(99, cp->akicks)]++;
		for(a = 0; a < 32; a++) {
			if (cp->flags & (1 << a))
				theflags[a]++;
			if (cp->mlock & (1 << a))
				themlocks[a]++;
		}
	}

	printf("<chanstats time=\"%ld\" total=\"%i\"\n numurls=\"%i\" numdescs=\"%i\" opitems=\"%i\" akitems=\"%i\" munged=\"%i\">\n", CTime, chans,
		urls, descs, opitems, akitems, munged);

	printf(" <expirelens>\n ");
	for(a = 0; a < 15; a++) 
	{
		printf(" <explen i=\"%i\" val=\"%i\" />", a, thedays[a]);
		if (((a+1) % 2) == 0)
			printf("\n ");

	}
	printf("\n </expirelens>\n");

	printf(" <opcts>\n ");
	for(a = 0; a < 200; a++) 
	{
		printf(" <opct i=\"%i\" val=\"%i\" />", a, theops[a]);
		if (((a+1) % 2) == 0)
			printf("\n ");

	}
	if (((a) % 2) != 0)
		printf("\n ");
	printf("</opcts>\n");

	printf(" <bancts>\n ");
	for(a = 0; a < 100; a++) 
	{
		printf(" <banct i=\"%i\" val=\"%i\" />", a, theakicks[a]);
		if (((a+1) % 2) == 0)
			printf("\n ");

	}
	if (((a) % 2) != 0)
		printf("\n ");
	printf("</bancts>\n");


	printf(" <flagcts>\n ");
	for(a = 0; a < 32; a++) 
	{
		printf(" <flagct i=\"%i\" val=\"%i\" />", a, theflags[a]);
		if (((a+1) % 2) == 0)
			printf("\n ");

	}
	if (((a) % 2) != 0)
		printf("\n ");
	printf("</flagcts>\n");


	printf(" <mlockcts>\n ");
	for(a = 0; a < 32; a++) 
	{
		printf(" <mlockct i=\"%i\" val=\"%i\" />", a, themlocks[a]);
		if (((a+1) % 2) == 0)
			printf("\n ");

	}
	if (((a) % 2) != 0)
		printf("\n ");
	printf("</mlockcts>\n");

	for(cp = LIST_FIRST(&chanList); cp; cp = LIST_NEXT(cp, chans))
	{
		if ( cp->flags & CBANISH)
			printf("<banchan>%s</banchan>\n", cp->chan);
		else if ( cp->flags & CCLOSE )
			printf("<closedchan>%s</closedchan>\n", cp->chan);
		if ( cp->flags & CHOLD)
			printf("<heldchan>%s</heldchan>\n", cp->chan);
	}

	printf("</chanstats>\n");
	fclose(fp);
}

int hasWild(char *s)
{
	while(*s) {
		if (*s == '?' || *s == '*')
			return 1;
		s++;
	}
	return 0;
}

int hasNonWild(char *s)
{
	while(*s) {
		if (*s != '?' || *s != '*')
			return 1;
		s++;
	}
	return 0;
}

char *knock(char *h)
{
	return (h = strchr(h, '.')) ? h + 1 : NULL;
}

char *domain(char *h)
{
	char *htmp;

	while((htmp = knock(h)) != NULL)
		h = htmp;
	return h;
}

int countDots(char *x)
{
	int dots = 0;

	while(*x)
	{
		if (*x == '.')
			dots++;
		x++;
	}
	return dots;
}

int banType(char *n, char *u, char *h)
{
	if (!hasWild(n))
		return 1;
	if (hasNonWild(u))
		return 2;
	if (!hasWild(h) && *h == '*' && countDots(h) <= 3)
		return 3;
	return 4;
}

int akillstats(void)
{
	int bans[4][10] = {0}, i = 0, a = 0;
	int mm[] = { 0,0,1,2,2,3,3,3,3,4 };
	char *xx[] = { "akill", "ignore", "ahurt" };

	char 		 line[1024];
	Akill	*ak;
	FILE *fp;

	fp = fopen("operserv/akill.db", "r");

	if (!fp)
		return -1;

	while(sfgets(line, 256, fp))
	{
		i++;

		if (strcmp(line, "-") != 0)
			return -1;
		ak = (struct akill*)oalloc(sizeof(Akill));
		sfgets(line, 256, fp); // nick or id
		if (*line == '#')
			sfgets(line, 256, fp); // nick
		strncpyzt(ak->nick, line, sizeof(ak->nick));

		sfgets(line, 256, fp); // user
		strncpyzt(ak->user, line, sizeof(ak->user));

		sfgets(line, 256, fp); // host
		strncpyzt(ak->host, line, sizeof(ak->host));

		sfgets(line, 256, fp); // set/unset
		sscanf(line, "%d %d", &ak->setat, &ak->unset);

		sfgets(line, 256, fp); // setby
		strncpyzt(ak->setby, line, sizeof(ak->setby));

		sfgets(line, 256, fp); // reason
		strncpyzt(ak->reason, line, sizeof(ak->reason));

		sfgets(line, 256, fp); // type
		ak->type = atoi(line);
		LIST_INSERT_HEAD(&akList, ak, ak_lst);
	}

	for(ak = LIST_FIRST(&akList); ak; ak = LIST_NEXT(ak, ak_lst))
	{
		bans[mm[ak->type]][0]++;
		bans[mm[ak->type]][banType(ak->nick,ak->user,ak->host)]++;
	}

	printf("<akillstats akills=\"%i\" ignores=\"%i\" ahurts=\"%i\">\n", 
                bans[0][0], bans[1][0], bans[2][0]);
	for(i = 0; i < 3; i++) {		
		printf("<s-%s-site count=\"%i\" />", xx[i], bans[i][4]);
		printf("<s-%s-host count=\"%i\" />\n", xx[i], bans[i][3]);
		printf("<s-%s-user count=\"%i\" />", xx[i], bans[i][2]);
		printf("<s-%s-nick count=\"%i\" />\n", xx[i], bans[i][1]);
	}

	printf("</akillstats>\n");

	fclose(fp);
}

int nickstats(void)
{
	RegNickList 	*read;
	RegNickList	*read2;
	u_long		 totalnicklen=0;
	u_long		 totalurllen=0;
	u_long		 totalemail=0;
	u_long		 totalrn=0;
	time_t		 workingtime;
	time_t		 dayssince;
	u_int		 longestnick=0;
	u_int		 longesturl=0;
	u_int		 longestemail=0;
	u_int		 longestrn=0;
	u_int		 averagenick=0;
	u_int		 averageurl=0;
	u_int		 averageemail=0;
	u_int		 averagern=0;
	u_int		 thenicks[33];
	u_int		 thedays[50];
	UrlList		*url;
	EmailList	*email2;
	ParseState	*state = (ParseState *)oalloc(sizeof(ParseState));
	char		*command;
	char 		 line[1024];
	FILE 		*fp;
	int		 done = 0;
	int		 a, i = 0;

	for (a = 0; a < 33; a++)
	  thenicks[a] = 0;
	  
	for(a = 0; a < 50; a++)
	  thedays[a] = 0;
	  
	fp = fopen("nickserv/nickserv.db", "r");
	if(!fp)
	  exit(-1);
	while(!done) {
		if (!(sfgets(line, 1024, fp))) {
			done = 1;
			break;
		}
		
		state->current = state->start = line;
		command = get_arg(state);
		if(!strcmp(command, "nick")) {
			read = (RegNickList *)oalloc(sizeof(RegNickList));
			LIST_INSERT_HEAD(&firstUser, read, users);
			strcpy(read->nick, get_arg(state));
 			get_arg(state);
 			get_arg(state);
 			get_arg(state);
			read->reged = atol(get_arg(state));
 			get_arg(state);
			read->flags = atoi(get_arg(state));
			strcpy(read->realname, state->current);
			if (!(read->flags & (NBANISH|NHOLD)))
			    usernicks++;
			nicks++;
		}
		else if(!strcmp(command, "url")) {
			url = (UrlList *)oalloc(sizeof(UrlList));
			LIST_INSERT_HEAD(&firstUrl, url, urls);
			get_arg(state);
			strncpy(url->url, get_arg(state), 128);
			urls++;
		}
		else if(!strcmp(command, "access")) {
			access++;
		}
		else if(!strcmp(command, "email")) {
			if (!(read->flags & (NBANISH|NHOLD))) {
			email2 = (EmailList *)oalloc(sizeof(EmailList));
			LIST_INSERT_HEAD(&firstEmail, email2, emails);
			get_arg(state);
			strcpy(email2->email, get_arg(state));
				email++;
			if (strncmp(email2->email, "(none)", 6))
                           email_valid++;
			}
		}
		else if(!strcmp(command, "done"))
		  done = 1;
	}

	fclose(fp);
	printf("<nickstats time=\"%ld\" total=\"%i\"\n numurls=\"%i\" accitems=\"%i\" numemails=\"%i\">\n", CTime, nicks, urls, access, email);
	for(read = firstUser.lh_first;read;read = read->users.le_next) {
		if(read->flags & NBANISH || read->flags & NHOLD) {
			read2 = (RegNickList *)oalloc(sizeof(RegNickList));
			LIST_INSERT_HEAD(&heldNick, read2, users);
			strcpy(read2->nick, read->nick);
			read2->flags = read->flags;

			held++;
		}
		else {
		  if(read->flags & NVACATION)
		    workingtime = (CTime - read->reged)/2;
		  else
		    workingtime=CTime - read->reged;
		  dayssince = ((60*60*24*25) - workingtime) / (60*60*24);
		  thedays[dayssince]++;
		}
		thenicks[strlen(read->nick) - 1]++;
		totalnicklen = totalnicklen + strlen(read->nick);
		if (strlen(read->nick) > longestnick)
		  longestnick = strlen(read->nick);
		if (strlen(read->realname) > longestrn)
		  longestrn = strlen(read->realname);
		totalrn = totalrn + strlen(read->realname);
	}

	for(email2 = firstEmail.lh_first;email2;email2 = email2->emails.le_next) {
		if(strlen(email2->email) > longestemail)
		   longestemail = strlen(email2->email);
		totalemail = totalemail + strlen(email2->email);
	}
	for(url = firstUrl.lh_first;url;url = url->urls.le_next) {
		totalurllen = totalurllen + strlen(url->url);
		if (strlen(url->url) > longesturl)
		  longesturl = strlen(url->url);
	}
	
	averagenick = totalnicklen / NZERO(nicks);
	averageurl = totalurllen / NZERO(urls);
	averageemail = totalemail / NZERO(email);
	averagern = totalrn / NZERO(nicks);
	printf(" <emails valid=\"%i\" none=\"%i\" count=\"%i\" />\n", email_valid, email - email_valid, nicks - email);
	printf(" <nicklength longest=\"%i\" average=\"%i\" />\n", longestnick,  averagenick);
	printf(" <urllength longest=\"%i\" average=\"%i\" />\n", longesturl,   averageurl);
	printf(" <emaillength longest=\"%i\" average=\"%i\" />\n", longestemail, averageemail);
	printf(" <reallen longest=\"%i\" average=\"%i\" />\n", longestrn, averagern);
	printf(" <nicklens>\n");
	for(i = 0; i < 17; i++) {
		printf(" <nicklen i=\"%i\" val=\"%i\" /> ", i, thenicks[i]);
		if (((i+1) % 2) == 0)
			printf("\n");
	}
	printf(" </nicklens>\n");
	printf(" <expirelens>\n");
	for(i = 0; i < 25; i++) {
		printf(" <explen i=\"%i\" val=\"%i\" /> ", i, thedays[i]);
		if (((i+1) % 2) == 0)
			printf("\n");
	}
	printf(" </expirelens>\n");

	for(read = heldNick.lh_first;read;read = read->users.le_next) {
		if (!(read->flags & NBANISH))
		  printf(" <heldnick>%s</heldnick>\n", read->nick);
	}
	for(read = heldNick.lh_first;read;read = read->users.le_next) {
		if ((read->flags & NBANISH))
		  printf(" <bannick>%s</bannick>\n", read->nick);
	}
	printf("\n");

#ifdef MAILSTAT_WRITE
	if (nicks && (fp = fopen("emailstats", "a")))
	{
		/* %m/%d %H:%M %Z */
		#define PC(x, y) (((100*(x))/y) + (((100*(x))%y > (y/2)) ? 1 : 0))
		fprintf(fp, "[%ld]\n", CTime);
		fprintf(fp, "E-mail addies          : %5i(%3d%%) * Elected to set none    : %5i(%3d%%)\n", email_valid, PC(email_valid, usernicks), email - email_valid, PC(email - email_valid, usernicks));
		fprintf(fp, "Neglected e-mail field : %5i(%3d%%) * Nicks                  :  %5i/%5i\n", usernicks - email, PC(usernicks - email, usernicks), usernicks, nicks - usernicks);
		fflush(fp);
	fclose(fp);
	}
#endif

#ifdef GNUPLOT_DAT
	fp = fopen("NickExpire.dat", "w");
	for (a = 0; a < 25 ; a++) {
	  fprintf(fp, "%d %d\n", a+1, thedays[a]);
	  fflush(fp);
	}
	fclose(fp);
	fp = fopen("NickLength.dat", "w");
	for(a = 0; a < 17; a++) {
	  fprintf(fp, "%d %d\n", a+1, thenicks[a]);
	  fflush(fp);
	}
	fclose(fp);
#endif  
	printf("</nickstats>\n");

}

int
main(void)
{
	CTime = time(NULL);

	printf("<statsrun time=\"%ld\">\n", CTime);
	nickstats();
	chanstats();
	akillstats();
	printf("</statsrun>\n");

	return 0;
}    

char *
sfgets(char *str, int len, FILE *fp)
{
        if(!fgets(str, len, fp))
                  return NULL;
        else {
          if(str[0])
            str[strlen(str) - 1] = 0;
          return str;
        }
}

/* 
 * oalloc (size)
 * Allocate n bytes of memory, check that is was properly allocated
 * and clean it.  This function is like calloc(), but will shut
 * down services cleanly if it fails to allocate
 */

void *oalloc(size_t size)
{
	void *alloctmp = 0;
	
	if (size < 1) {
	  printf("oalloc: Error, requested size is less than 1");
	  exit(1);
	}
	
	alloctmp = malloc(size);
	
	if (alloctmp == NULL) {
	  printf("oalloc: Error allocating memory, terminating services\n");
	  exit(1);
	}

	memset(alloctmp, 0, size);

	return alloctmp;
}                                                                                    

char *
get_arg (ParseState * state)
{
  state->current = strsep (&state->start, " ");
  return state->current;
}

char *
curr_arg (ParseState * state)
{
  return state->current;
}

