/*
 * Copyright (c) 2001 James Hess
 * Copyright (c) 1996-1997 Chip Norkus
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

/**
 * \file laxp.c
 * \brief Nick log data extraction
 *
 * $Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

u_int16_t
getHashKey(const char *hname)
{
	const u_char    *name;
	u_int16_t  hash;
	
	hash = 0x5555;
	name = (const u_char *)hname;
	
	if (!hname) {
		/*dlogEntry("getHashKey: hashing NULL, returning 0");*/

		return 0;
	}
	
	for ( ; *name ; name++) {
		hash = (hash << 3) | (hash >> 13);
		hash ^= tolower(*name);
	}

	return (hash);
}

struct domain_data
{
	char *name;
	int count;
	struct domain_data *next;
} *domains = NULL;

#define HZ 256

struct domain_index_data
{
	char *nick;
	struct domain_data *domain;
	struct domain_index_data *next;
} *dhash[HZ];

void add_domain_index(struct domain_data *d, char *name)
{
	struct domain_index_data *in;
	u_int16_t k = getHashKey(name) % HZ;
	char *p = strchr(name,'!');

	if (p) *p = '\0';

	//printf("add_domain_index(%s,%s)\n",d->name,name);
	in = malloc(sizeof(struct domain_index_data));
	if (in == 0)
		abort();

	in->nick = strdup(name);
	if (in->nick == 0)
		abort();

	in->domain = d;
	in->next = dhash[k];
	dhash[k] = in;
}

struct domain_data *find_domain_index(char *name)
{
	struct domain_index_data *in;
	u_int16_t k = getHashKey(name) % HZ;
	char *p = strchr(name, '!');

	if (p) *p ='\0';

	for(in = dhash[k]; in; in = in->next)
		if (strcasecmp(in->nick, name) == 0)
			break;


	//printf("find_domain_index(%s) == %p\n",name, in ? in->domain : 0);
	if (in)
		return in->domain;
	return NULL;
}

struct domain_data *del_domain_index(struct domain_data *d, char *name)
{
	struct domain_index_data *in, *in_next, *tmp = NULL;
	u_int16_t k = getHashKey(name) % HZ;
	static int Z=0;

	if ((Z++ %10) == 0)
		printf("del_domain_index(%s,%s)\n",d->name,name);

	for(in = dhash[k]; in; in = in_next)
	{
		in_next = in->next;

		if (in->domain != d || strcasecmp(name, in->nick)) {
			tmp = in;
			continue;
		}
		else {
			if (tmp)
				tmp->next = in_next;
			else
				dhash[k] = in_next;
			free(in->nick);
			free(in);
		}
	}
	return NULL;
}
//add_domain_index
//find_domain_index
//del_domain_index

char *domainize(char *host, int a) {
	int i;
	char *p;

	if (p = strchr(host, '@'))
		host = p;

	for(i = 0; host && host[i]; i++)
		;

	for(; i >= 0; i--)
	{
		if (host[i] == '.')
			if (a-- < 1)
				break;
	}

	/* printf("Domainize(%s,%d) -> %s\n", host, a, host+i+1); */
	return host + i + 1;
}

struct domain_data *find_domain(char *host, int a) {
	struct domain_data *dp;
	char *x;

	x = domainize(host, a);

	for(dp = domains; dp; dp = dp->next)
		if (strcasecmp(dp->name, x) == 0)
			return dp;
	return NULL;
}

struct domain_data *add_domain(char *host, int a) {
	char *x = domainize(host, a);
	struct domain_data *dp;

	dp = malloc(sizeof(struct domain_data));
	memset(dp, 0, sizeof(struct domain_data));
	dp->next = domains;
	dp->name = strdup(x);
	if (dp->name == 0)
		abort();
	return domains = dp;
}

void del_domain(struct domain_data *zap)
{
	struct domain_data *dp, *dp_prev = NULL, *dp_next;

	for(dp = domains; dp; dp = dp_next)
	{
		dp_next = dp->next;

		if (dp == zap) {
			if (dp_prev)
				dp_prev->next = dp_next;
			else
				domains = dp_next;
			free(zap);

			return;
		}

		dp_prev = dp;
	}
}

FILE *fpDom = 0;

void
print_dstats(int day)
{
	struct domain_data *dp;
	char buf[255];

	if (fpDom == 0)
		fpDom = fopen("domains.dat", "w");

	for(dp = domains; dp; dp = dp->next) {
		if (fpDom == 0 || dp->count < 2)
			continue;
		fprintf(fpDom, "%d %s %d\n", day, dp->name, dp->count);
	}

}

int main()
{
	char buf[512];
	time_t v = 851990400, z;
	int days_since = 0, daytmp = 0;
	int xps[5000]={}, rgs[5000]={}, d = 0;
	FILE *fpReg = fopen("regstats.dat", "w"), *fpExp = fopen("expstats.dat", "w");
	{
		rewind(stdin);

		while(fgets(buf, 512, stdin)) {
			if (strncmp (buf, "NS_RE", 5) == 0 || strncmp (buf, "NSE_EXP", 7) == 0)
			{
				int tmp_XX;
				static char xbuf[512];
				struct domain_data *dp;

				xbuf[0] = '\0';

				if (sscanf (buf, "NS_REGISTER %ld  %s *", &z, xbuf) > 1)
				{
					if (z < 0 || z < v || ((z - v) / 86400) >= 2000)
					{
						printf("WARNING: z - v = %d\n", z-v);
						continue;
					}

				 	if (((z - v) / 86400) - daytmp < 0)
						continue; 	

					days_since = ((z - v) / 86400) - daytmp;
					daytmp = (z - v) / 86400;

					if (days_since > 0) { 
						int i = 0;

						for(i=daytmp-days_since;i<daytmp;i++)
							print_dstats(i);
					}

					if (z >= v)
						rgs[(z - v) / 86400]++;
					if ((dp = find_domain(xbuf, 1)) == NULL)
						dp = add_domain(xbuf, 1);
					add_domain_index(dp,xbuf);
					dp->count++;
				}

				if (sscanf (buf, "NSE_EXPIRE %ld - %d %s *", &z, &tmp_XX, xbuf) > 0)
				{
					if (z < 0 || z < v || ((z - v) / 86400) >= 2000)
					{
						printf("WARNING: z - v = %d\n", z-v);
						continue;
					}

				 	if (((z - v) / 86400) - daytmp < 0)
						continue; 	

					days_since = ((z - v) / 86400) - daytmp;
					daytmp = (z - v) / 86400;

					if (days_since > 0) { 
						int i = 0;

						for(i=daytmp-days_since;i<daytmp;i++)
							print_dstats(i);
					} /**/

					if (z >= v)
						xps[(z - v) / 86400]++;
					if (dp = find_domain_index(xbuf)) {
						dp->count--;
						del_domain_index(dp,xbuf);

						if (dp->count < 1)
							del_domain(dp);
						dp = NULL;
					}
				}
			}
		}
	}

	for(d = 0; d < 5000; d++)
	{
		if (rgs[d] != 0)		
			fprintf(fpReg, "%d %d\n", d, rgs[d]);
		if (xps[d] != 0)
			fprintf(fpExp, "%d %d\n", d, xps[d]);
	}

	fclose(fpReg);
	fclose(fpExp);
}
