/* $Id$ */

/* Note: This program now generates the databases in the current directory
 * you need to move them to the proper locations
 */
 
#define GENNICKS 3000
#define GENCHANS 1500
#define CHANOPS 25

#include <stdio.h>
#include <time.h>

void main(void);
void genNickInfo(void);
void genChanInfo(void);
void genMemoInfo(void);

void main (void) {
	genNickInfo();
	genChanInfo();
	genMemoInfo();
}

void
genNickInfo(void)
{
	unsigned long x=0;
	FILE *ns;

	ns = fopen("nickserv.db", "w");

	for (x = GENNICKS ; x > 1; x--) {
		fprintf(ns, "nick ");
		fprintf(ns, "A%lu A%lu blah.com ", x, x);
		fprintf(ns, "llllllll "); /* the nick password */
		fprintf(ns, "%lu ", time(NULL)); /* last seen */
		fprintf(ns, "%lu ", time(NULL)); /* timereg */
		fprintf(ns, " 0 0\n"); /* flags, #channels */
		fprintf(ns, "url A%lu http://blah.com/%lu\n", x, x);
		fprintf(ns, "email A%lu A%lu@blah.com\n", x, x);
		fprintf(ns, "access A%lu A%lu@blah.com\n", x, x);
		fflush(ns);
		}
	fprintf(ns, "done\n");
	fflush(ns);
	fclose(ns);
}

void
genChanInfo(void)
{
	unsigned long x = 0, q = 0;
	FILE *cs;

	cs = fopen("chanserv.db", "w");
	for(x = GENCHANS ; x > 1 ; x--) {
		fprintf(cs, "channel ");
		fprintf(cs, "#A%lu A%lu ", x, x);
		fprintf(cs, "5 5 %lu ", x);
		fprintf(cs, "%lu %lu ", time(NULL), time(NULL));
		fprintf(cs, "hiya 124 3 ");
		fprintf(cs, "3 1 :Some really awesome channel!\n");
		fprintf(cs, "topic #A%lu A%lu 884410401 :This channel rocks!\n", x, x);
		fprintf(cs, "url #A%lu http://www.blah.org%\n", x);
		fprintf(cs, "autogreet #A%lu Hi, there!\n", x);

		for(q = CHANOPS ; q > 1 ; q--) {
			fprintf(cs, "op #A%lu A%lu ", x, q);
			fprintf(cs, "3\n");
		}
		fprintf(cs, "akick #A%lu A%lu@blah.org ", x, x);
		fprintf(cs, "884410401 :for the hellof it\n");
		fflush(cs);
	}
	fprintf(cs, "done\n");
	fflush(cs);
	fclose(cs);
}

void
genMemoInfo(void)
{
	unsigned long x=0;
	FILE *ms;

        ms = fopen("memoserv.db", "w");

	for (x = GENNICKS ; x > 2 ; x--) {
		fprintf(ms, "data A%lu 1 0 0 50\n", x);
		fprintf(ms, "memo A%lu 0 0 0 A%lu A%lu :hiya and such.. just sending this test memo!\n", x, x, x-1);
		fflush(ms);
        }

        fprintf(ms, "done\n");
        fclose(ms);
}
