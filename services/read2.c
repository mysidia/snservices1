#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "queue.h"

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

typedef struct urllist_struct UrlList;

struct urllist_struct {
	char url[128];
	LIST_ENTRY(urllist_struct) urls;
};

typedef struct regnicklist RegNickList;

struct regnicklist {
	char nick[33];
	unsigned long reged;
	char host[128];
	char user[128];
	char realname[128];
	u_int32_t flags;
	LIST_ENTRY(regnicklist) users;
};

char *get_arg(ParseState *);

LIST_HEAD(,regnicklist)       firstUser;
LIST_HEAD(,regnicklist)	      heldNick;
LIST_HEAD(,urllist_struct)    firstUrl;
LIST_HEAD(,emaillist_struct)  firstEmail;

int
main(void)
{
	RegNickList 	*read;
	RegNickList	*read2;
	u_long		 totalnicklen=0;
	u_long		 totalurllen=0;
	u_long		 totalemail=0;
	u_long		 totalrn=0;
	u_long		 totalhn=0;
	u_long		 totalun=0;
	time_t		 workingtime;
	time_t		 dayssince;
	time_t		 CTime;
	u_int		 longestnick=0;
	u_int		 longesturl=0;
	u_int		 longestemail=0;
	u_int		 longestrn=0;
	u_int		 longesthn=0;
	u_int		 longestun=0;
	u_int		 averagenick=0;
	u_int		 averageurl=0;
	u_int		 averageemail=0;
	u_int		 averagern=0;
	u_int		 averagehn=0;
	u_int		 averageun=0;
	u_int		 thenicks[33];
	u_int		 thedays[50];
	UrlList		*url;
	EmailList	*email2;
	ParseState	*state = (ParseState *)oalloc(sizeof(ParseState));
	char		*command;
	char 		 line[1024];
	FILE 		*fp;
	int		 done = 0;
	int		 a;

	for (a = 0; a < 33; a++)
	  thenicks[a] = 0;
	  
	for(a = 0; a < 50; a++)
	  thedays[a] = 0;
	  
	fp = fopen("nickserv/nickserv.db", "r");
	if(!fp)
	  return -1;
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
 			strcpy(read->user, get_arg(state));
 			strcpy(read->host, get_arg(state));
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
	CTime = time(NULL);
	printf("Nicks: %i\nUrls: %i\nAccess List Entries: %i\nEmails: %i\n", nicks, urls, access, email);
	for(read = firstUser.lh_first;read;read = read->users.le_next) {
		if(read->flags & NBANISH || read->flags & NHOLD) {
			read2 = (RegNickList *)oalloc(sizeof(RegNickList));
			LIST_INSERT_HEAD(&heldNick, read2, users);
			strcpy(read2->nick, read->nick);
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
		if (strlen(read->host) > longesthn)
		  longesthn = strlen(read->host);
		totalhn = totalhn + strlen(read->host);
		if (strlen(read->user) > longestun)
		  longestun = strlen(read->user);
		totalun = totalun + strlen(read->user);
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
	
	averagenick = totalnicklen / nicks;
	averageurl = totalurllen / urls;
	averageemail = totalemail / email;
	averagern = totalrn / nicks;
	averagehn = totalhn / nicks;
	averageun = totalun / nicks;
	printf("E-mail addies          : %5i * Elected to set none    : %5i\n", email_valid, email - email_valid);
	printf("Neglected e-mail field : %5i\n", nicks - email);
	printf("Largest Nick Length    : %5i * Average Nick Length    : %5i\n", longestnick,  averagenick);
	printf("Largest URL Length     : %5i * Average URL Length     : %5i\n", longesturl,   averageurl);
	printf("Largest Email Length   : %5i * Average Email Length   : %5i\n", longestemail, averageemail);
	printf("Largest Realname Length: %5i * Average Realname Length: %5i\n", longestrn, averagern);
	printf("Largest Hostname Length: %5i * Average Hostname Length: %5i\n", longesthn, averagehn);
	printf("Largest Username Length: %5i * Average Username Length: %5i\n", longestun, averageun);
	printf("Nick Lengths (chars):\n");
	printf("( 1) = %3i * ( 2) = %3i * ( 3) = %3i * ( 4) = %3i * ( 5) = %3i\n", thenicks[0], thenicks[1], thenicks[2], thenicks[3], thenicks[4]);
	printf("( 6) = %3i * ( 7) = %3i * ( 8) = %3i * ( 9) = %3i * (10) = %3i\n", thenicks[5], thenicks[6], thenicks[7], thenicks[8], thenicks[9]);
	printf("(11) = %3i * (12) = %3i * (13) = %3i * (14) = %3i * (15) = %3i\n", thenicks[10], thenicks[11], thenicks[12], thenicks[13], thenicks[14]);
	printf("(16) = %3i * (17) = %3i\n", thenicks[15], thenicks[16]);
	printf("Days until expire (days):\n");
	printf("( 1) = %3i * ( 2) = %3i * ( 3) = %3i * ( 4) = %3i * ( 5) = %3i\n", thedays[0], thedays[1], thedays[2], thedays[3], thedays[4]);
	printf("( 6) = %3i * ( 7) = %3i * ( 8) = %3i * ( 9) = %3i * (10) = %3i\n", thedays[5], thedays[6], thedays[17], thedays[8], thedays[9]);
	printf("(11) = %3i * (12) = %3i * (13) = %3i * (14) = %3i * (15) = %3i\n", thedays[10], thedays[11], thedays[12], thedays[13], thedays[14]);
	printf("(16) = %3i * (17) = %3i * (18) = %3i * (19) = %3i * (20) = %3i\n", thedays[15], thedays[16], thedays[17], thedays[18], thedays[19]);
	printf("(21) = %3i * (22) = %3i * (23) = %3i * (24) = %3i * (25) = %3i\n", thedays[20], thedays[21], thedays[22], thedays[23], thedays[24]);
	printf("Held/Banished nicks:");
	for(read = heldNick.lh_first;read;read = read->users.le_next)
	  printf(" %s", read->nick);
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

