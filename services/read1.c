#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "queue.h"

void main(void);
char *sfgets(char *, int, FILE *);

int email=0;
int nicks=0;
int urls=0;
int access=0;

typedef struct urllist_struct UrlList;

struct urllist_struct {
	char url[128];
	LIST_ENTRY(urllist_struct) urls;
};

typedef struct regnicklist RegNickList;

struct regnicklist {
	char nick[33];
	time_t reged;
	LIST_ENTRY(regnicklist) users;
};

LIST_HEAD(,regnicklist)       firstUser;
LIST_HEAD(,regnicklist)	      lastUser;
LIST_HEAD(,urllist_struct)    firstUrl;
LIST_HEAD(,urllist_struct)    lastUrl;

void main(void)
{
	unsigned long totalnicklen=0;
	unsigned long totalurllen=0;
	int longestnick=0;
	int longesturl=0;
	int averagenick=0;
	int averageurl=0;
	unsigned int thenicks[33];
	RegNickList *read;
	UrlList *url;
	char line[1024];
	char *tmp = line;
	char args[13][129];
	FILE *ns;
	int done = 0;
	int a, x;
	time_t CTime;

	for (a = 0; a < 33; a++)
	  thenicks[a] = 0;
	  
	ns = fopen("nickserv/nickserv.db", "r");
	if(!ns)
	  return;
	while(!done) {
		if (!(sfgets(line, 1024, ns))) {
			done = 1;
			break;
		}
		tmp = line;
		x = a = 0;
		while(*tmp && x < 10) {
			if(x == 9) {
				tmp++;
				while(*tmp) {
					args[x][a] = *tmp;
					a++;
					tmp++;
				}
			}
			else {
				while(*tmp != ' ' && *tmp) {
					args[x][a] = *tmp;
					a++;
					tmp++;
				}
			}
			
			args[x][a] = 0;
			x++;
			while(*tmp == ' ')
			  tmp++;
			a = 0;
		}
		
		if(!strcmp(args[0], "nick")) {
			read = calloc(1, sizeof(RegNickList));
			LIST_INSERT_HEAD(&firstUser, read, users);
			strcpy(read->nick, args[1]);
			read->reged = (time_t)atol(args[6]);
			nicks++;
		}
		else if(!strcmp(args[0], "url")) {
			url = calloc(1, sizeof(UrlList));
			LIST_INSERT_HEAD(&firstUrl, url, urls);
			strncpy(url->url, args[2], 128);
			urls++;
		}
		else if(!strcmp(args[0], "access")) {
			access++;
		}
		else if(!strcmp(args[0], "email")) {
			email++;
		}
		else if(!strcmp(args[0], "done"))
		  done = 1;
	}
	fclose(ns);
	CTime = time(NULL);
	printf("Nicks: %i\nUrls: %i\nAccess List Entries: %i\nEmails: %i\n", nicks, urls, access, email);
	for(read = firstUser.lh_first;read;read = read->users.le_next) {
		thenicks[strlen(read->nick) - 1]++;
		totalnicklen = totalnicklen + strlen(read->nick);
		if (strlen(read->nick) > longestnick)
		  longestnick = strlen(read->nick);
	}

	for(url = firstUrl.lh_first;url;url = url->urls.le_next) {
		totalurllen = totalurllen + strlen(url->url);
		if (strlen(url->url) > longesturl)
		  longesturl = strlen(url->url);
	}
	
	averagenick = totalnicklen / nicks;
	averageurl = totalurllen / urls;
	printf("Largest Nick Length: %5i * Largest URL Length: %5i\n", longestnick, longesturl);
	printf("Average Nick Length: %5i * Average URL Length: %5i\n", averagenick, averageurl);
	printf("Nick Lengths (chars):\n");
	printf("  1 = %3i *  2 = %3i *  3 = %3i *  4 = %3i *  5 = %3i\n", thenicks[0], thenicks[1], thenicks[2], thenicks[3], thenicks[4]);
	printf("  6 = %3i *  7 = %3i *  8 = %3i *  9 = %3i * 10 = %3i\n", thenicks[5], thenicks[6], thenicks[17], thenicks[8], thenicks[9]);
	printf(" 11 = %3i * 12 = %3i * 13 = %3i * 14 = %3i * 15 = %3i\n", thenicks[10], thenicks[11], thenicks[12], thenicks[13], thenicks[14]);
	printf(" 16 = %3i * 17 = %3i * 18 = %3i * 19 = %3i * 20 = %3i\n", thenicks[15], thenicks[16], thenicks[27], thenicks[18], thenicks[19]);
	printf(" 21 = %3i * 22 = %3i * 23 = %3i * 24 = %3i * 25 = %3i\n", thenicks[20], thenicks[21], thenicks[26], thenicks[23], thenicks[24]);
	printf(" 26 = %3i * 27 = %3i * 28 = %3i * 29 = %3i * 30 = %3i\n", thenicks[25], thenicks[26], thenicks[27], thenicks[28], thenicks[29]);
	printf(" 31 = %3i * 32 = %3i\n", thenicks[30], thenicks[31]);
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
                                                                                    