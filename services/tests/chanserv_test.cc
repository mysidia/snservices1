// Copyright James Hess; All Rights Reserved

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "chanserv_test.h"
#include "../services.h"
#include "../chanserv.h"
#include "../nickserv.h"

CPPUNIT_TEST_SUITE_REGISTRATION( ChanServTestCase );

void
ChanServTestCase::testchanlist()
{
	char *str_dup(const char *);
	void addChan(ChanList *);
	ChanList *x, *y, *z, *q, *p;

	// Test online channels

	x = (ChanList *)oalloc(sizeof(ChanList));
	y = (ChanList *)oalloc(sizeof(ChanList));
	z = (ChanList *)oalloc(sizeof(ChanList));
	q = (ChanList *)oalloc(sizeof(ChanList));

	strcpy(x->name, "#Test");
	addChan(x);

	strcpy(y->name, "#2.Test");
	addChan(y);

	strcpy(z->name, "#3.Test");
	addChan(z);

	strcpy(q->name, "#4.Test");
	addChan(q);

	CPPUNIT_ASSERT(z == getChanData("#3.Test"));
	CPPUNIT_ASSERT(x == getChanData("#Test"));
	CPPUNIT_ASSERT(q == getChanData("#4.Test"));
	CPPUNIT_ASSERT(y == getChanData("#2.Test"));
	CPPUNIT_ASSERT(NULL == getChanData("#asdf.Test"));
	delChan(x);
	delChan(y);
	delChan(z);
	delChan(q);
	CPPUNIT_ASSERT(getChanData("#Test") == NULL);
	CPPUNIT_ASSERT(getChanData("#3.Test") == NULL);
	CPPUNIT_ASSERT(getChanData("#2.Test") == NULL);
	CPPUNIT_ASSERT(getChanData("#4.Test") == NULL);

	x = (ChanList *)oalloc(sizeof(ChanList));
	y = (ChanList *)oalloc(sizeof(ChanList));
	z = (ChanList *)oalloc(sizeof(ChanList));
	q = (ChanList *)oalloc(sizeof(ChanList));

	strcpy(x->name, "#Test");
	strcpy(y->name, "#Test");
	strcpy(z->name, "#Test");
	strcpy(q->name, "#Test");

	addChan(x);
	addChan(y);
	addChan(z);
	addChan(q);

	CPPUNIT_ASSERT(getChanData("#Test") != NULL);
	CPPUNIT_ASSERT(getChanData("#Test") != NULL);
	CPPUNIT_ASSERT(getChanData("#Test") != NULL);
	CPPUNIT_ASSERT(getChanData("#Test") != NULL);
	CPPUNIT_ASSERT((getChanData("#Test") == getChanData("#Test")) &&
                       (getChanData("#Test") == getChanData("#Test")));

	delChan(x);

	CPPUNIT_ASSERT(getChanData("#Test") == NULL);
	CPPUNIT_ASSERT(getChanData("#3.Test") == NULL);
	CPPUNIT_ASSERT(getChanData("#2.Test") == NULL);
	CPPUNIT_ASSERT(getChanData("#4.Test") == NULL);
}



void
ChanServTestCase::testregchanlist()
{
	char *str_dup(const char *);
	void addChan(ChanList *);
	RegChanList *x, *y, *z, *q, *p;

	// Test online channels

	x = (RegChanList *)oalloc(sizeof(RegChanList));
	y = (RegChanList *)oalloc(sizeof(RegChanList));
	z = (RegChanList *)oalloc(sizeof(RegChanList));
	q = (RegChanList *)oalloc(sizeof(RegChanList));

	strcpy(x->name, "#Test");
	addRegChan(x);

	strcpy(y->name, "#2.Test");
	addRegChan(y);

	strcpy(z->name, "#3.Test");
	addRegChan(z);

	strcpy(q->name, "#4.Test");
	addRegChan(q);

	CPPUNIT_ASSERT(z == getRegChanData("#3.Test"));
	CPPUNIT_ASSERT(x == getRegChanData("#Test"));
	CPPUNIT_ASSERT(q == getRegChanData("#4.Test"));
	CPPUNIT_ASSERT(y == getRegChanData("#2.Test"));
	CPPUNIT_ASSERT(NULL == getRegChanData("#asdf.Test"));
	delRegChan(x);
	delRegChan(y);
	delRegChan(z);
	delRegChan(q);
	CPPUNIT_ASSERT(getRegChanData("#Test") == NULL);
	CPPUNIT_ASSERT(getRegChanData("#3.Test") == NULL);
	CPPUNIT_ASSERT(getRegChanData("#2.Test") == NULL);
	CPPUNIT_ASSERT(getRegChanData("#4.Test") == NULL);


	x = (RegChanList *)oalloc(sizeof(RegChanList));
	y = (RegChanList *)oalloc(sizeof(RegChanList));
	z = (RegChanList *)oalloc(sizeof(RegChanList));
	q = (RegChanList *)oalloc(sizeof(RegChanList));

	strcpy(x->name, "#Test");
	strcpy(y->name, "#Test");
	strcpy(z->name, "#Test");
	strcpy(q->name, "#Test");

	addRegChan(x);
	addRegChan(y);
	addRegChan(z);
	addRegChan(q);

	CPPUNIT_ASSERT(getRegChanData("#Test") != NULL);
	CPPUNIT_ASSERT(getRegChanData("#Test") != NULL);
	CPPUNIT_ASSERT(getRegChanData("#Test") != NULL);
	CPPUNIT_ASSERT(getRegChanData("#Test") != NULL);
	CPPUNIT_ASSERT((getRegChanData("#Test") == getRegChanData("#Test")) &&
                       (getRegChanData("#Test") == getRegChanData("#Test")));

	delRegChan(x);

	CPPUNIT_ASSERT(getRegChanData("#Test") == NULL);
	CPPUNIT_ASSERT(getRegChanData("#3.Test") == NULL);
	CPPUNIT_ASSERT(getRegChanData("#2.Test") == NULL);
	CPPUNIT_ASSERT(getRegChanData("#4.Test") == NULL);
}


void
ChanServTestCase::testbanlist()
{
	ChanList foo;
	cBanList *a[10];
	char buf[25];
	int i;

	for(i = 0; i < 10; i++) {
		a[i] = (cBanList *)oalloc(sizeof(cBanList));
		sprintf(a[i]->ban, "%d", i);
	}

	foo.firstBan = foo.lastBan = NULL;

	for(i = 0; i < 9; i++) {
		addChanBan(&foo, a[i]);
		CPPUNIT_ASSERT(a[i] != NULL);
		CPPUNIT_ASSERT(getChanBan(&foo, a[i]->ban) == a[i]);
	}
	CPPUNIT_ASSERT(getChanBan(&foo, a[9]->ban) == NULL);

	delChanBan(&foo, a[8]);
	CPPUNIT_ASSERT(getChanBan(&foo, "9") == NULL);
	CPPUNIT_ASSERT(getChanBan(&foo, "8") == NULL);
	CPPUNIT_ASSERT(getChanBan(&foo, "7") != NULL);
	CPPUNIT_ASSERT(getChanBan(&foo, "7") == a[7]);

	for(i = 7; i >= 0; i--)
		CPPUNIT_ASSERT(getChanBan(&foo, a[i]->ban) != NULL);
	/* delChanBan(&foo, a[8]); */

	for(i = 7; i >= 0; i--) {
		if (i < 8 && i > 1) {
			CPPUNIT_ASSERT(getChanBan(&foo, a[i]->ban) != NULL);
			CPPUNIT_ASSERT(getChanBan(&foo, a[i]->ban) == a[i]);

			sprintf(buf, "%d", i);
			CPPUNIT_ASSERT(getChanBan(&foo, buf) != NULL);
			CPPUNIT_ASSERT(getChanBan(&foo, buf) == a[i]);
		}
		else if (a[0]) {
			delChanBan(&foo, a[0]);
			a[0] = NULL;
			CPPUNIT_ASSERT(getChanBan(&foo, "0") == NULL);
		}
        }

	delChanBan(&foo, a[5]);
	CPPUNIT_ASSERT(getChanBan(&foo, "5") == NULL);

	for(i = 7; i > 0; i--)
		if (i != 5)
			delChanBan(&foo, a[i]);

	CPPUNIT_ASSERT(foo.firstBan == foo.lastBan);
	CPPUNIT_ASSERT(foo.firstBan == NULL);
}

/* Test op and akick access list procedures */
void
ChanServTestCase::testboplist()
{
	RegId topId(0, 0), j;
	RegNickList *n = (RegNickList *)oalloc(sizeof(RegNickList));
	RegChanList *x = (RegChanList *)oalloc(sizeof(RegChanList));
	cAccessList *ac = (cAccessList*)oalloc(sizeof(cAccessList));
	cAkickList  *ak = (cAkickList *)oalloc(sizeof(cAkickList));
	cNickList  *cnl = (cNickList *)oalloc(sizeof(cNickList));
	UserList fidoUser;
	ChanList fidoChan;
	int xi;

	strcpy(n->nick, "Foo");
	n->regnum.SetNext(topId);
	j = n->regnum;
	addRegNick(n);
	CPPUNIT_ASSERT(getRegNickData(n->nick));

	strcpy(x->name, "#Test");
	addRegChan(x);
	CPPUNIT_ASSERT(getRegChanData(x->name));

	ac->nickId = n->regnum;
	addChanOp(x, ac);
	CPPUNIT_ASSERT(getRegChanData(x->name));
	CPPUNIT_ASSERT(getChanOpData(x, n->nick));

	delRegNick(n);
	CPPUNIT_ASSERT(getRegNickData(n->nick) == NULL);
	CPPUNIT_ASSERT(getRegChanData("#Test") != NULL);
	CPPUNIT_ASSERT(((getChanOpData(x, "Foo")) == NULL));

	n = (RegNickList *)oalloc(sizeof(RegNickList));
	strcpy(n->nick, "Foo");
	n->regnum = j;
	addRegNick(n);

	cnl->person = &fidoUser;
	cnl->op = 0;

	fidoChan.firstUser = fidoChan.lastUser = NULL;
	for(xi = 0; xi < CHANUSERHASHSIZE; xi++)
		memset(&fidoChan.users[xi], 0, sizeof(cNickListHashEnt));
	addChanUser(&fidoChan, cnl);

	CPPUNIT_ASSERT(getChanUserData(&fidoChan, &fidoUser) == cnl);
	delChanUser(&fidoChan, cnl, 0);
	CPPUNIT_ASSERT(getChanUserData(&fidoChan, &fidoUser) == NULL);

	delChanOp(x, ac);
	CPPUNIT_ASSERT(getRegNickData(n->nick) != NULL);
	CPPUNIT_ASSERT(getRegChanData(x->name) != NULL);
	CPPUNIT_ASSERT(((getChanOpData(x, n->nick)) == NULL));

	delRegNick(n);
	CPPUNIT_ASSERT(getRegNickData(n->nick) == NULL);
	CPPUNIT_ASSERT(getRegChanData("#Test") != NULL);

	/***************************************************/

	strcpy(ak->mask, "*!*@*.com");
	strcpy(ak->reason, "Test");
	ak->added = time(0);
	addChanAkick(x, ak);

	CPPUNIT_ASSERT(getChanAkick(x, "*!*@*.comx") == NULL);
	CPPUNIT_ASSERT(getChanAkick(x, "*!*@*") == NULL);
	CPPUNIT_ASSERT(getChanAkick(x, "") == NULL);
	CPPUNIT_ASSERT(getChanAkick(x, "*") == NULL);
	CPPUNIT_ASSERT(getChanAkick(x, "*!*@*.com") == ak);

	delChanAkick(x, ak);

	CPPUNIT_ASSERT(getChanAkick(x, "*!*@*.com") == NULL);
	CPPUNIT_ASSERT(getChanAkick(x, "*!*@*.comx") == NULL);
	CPPUNIT_ASSERT(getChanAkick(x, "*!*@*") == NULL);
	CPPUNIT_ASSERT(getChanAkick(x, "") == NULL);
	CPPUNIT_ASSERT(getChanAkick(x, "*") == NULL);

	/***************************************************/

	delRegChan(x);
	CPPUNIT_ASSERT(getRegNickData("Foo") == NULL);
	CPPUNIT_ASSERT(getRegChanData("#Test") == NULL);
}

ChanServTestCase::ChanServTestCase()
{
}

ChanServTestCase::~ChanServTestCase()
{
}

