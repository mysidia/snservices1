/* $Id$ */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../parse.h"

char *strings[] = {
	"foo",
	" foo2",
	" foo2 ",
	"foo ",
	"   foo",
	"   foo   ",
	"foo bar baz",
	" foo bar baz",
	" foo  bar    baz ",
	"foo bar baz ",
	"   foo bar baz",
	"   foo bar baz   ",
	"foo bar baz   ",
	"foo :.",
	"a :b c d e f g",
	"a :b",
	"a  : b",
	NULL
};

int
main(int argc, char **argv)
{
	char *ts;
	char *ts1;
	char *ts2;
	char *ts3;
	char *arg;
	parse_t p;
	int n;

	n = 0;
	ts = strings[0];

	while (ts != NULL) {
		ts1 = strdup(ts);  /* strings might not be writable, so */
		ts2 = strdup(ts);  /* save away a few copies */
		ts3 = strdup(ts);

		printf("Parsing '%s'\n", ts1);
		parse_init(&p, ts1);
		while ((arg = parse_getarg(&p)) != NULL)
			printf("\tgetarg: '%s'\n", arg);
		parse_cleanup(&p);

		printf("Parsing '%s'\n", ts2);
		parse_init(&p, ts2);
		arg = parse_getarg(&p);
		if (arg != NULL) {
			printf("\tgetarg: '%s'\n", arg);
			arg = parse_getallargs(&p);
			if (arg != NULL)
				printf("\tallarg: '%s'\n", arg);
		}
		parse_cleanup(&p);
	    
		printf("Parsing '%s'\n", ts3);
		parse_init(&p, ts3);
		arg = parse_getallargs(&p);
		if (arg != NULL) {
			printf("\tallarg: '%s'\n", arg);
		}
		parse_cleanup(&p);

		n++;
		ts = strings[n];
	}
}
