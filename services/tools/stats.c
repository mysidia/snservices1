#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

FILE *regNicks, *fpAccItems, *fpNumEmails, *fpValidEmail, *fpNoneEmail,
     *fpEmailLen, *fpEmailLenAv;
FILE *fpNExpire[25];

time_t statBot, statTop;

float dataTimeIndex(time_t v)
{
	if (v > statTop || v < statBot)
		return 0;

	v -= statBot;

	// if ((statTop - statBot) < (3600*24))
	return ((float)v / (3600*24));

	// return ((statTop - statBot) / (float)(3600 * 24)) - ((float)v / (3600*24));

	if ((statTop - statBot) < 3600*24*365)
		return ((float)v / (3600*24*30));

	return v;
}

void parseEmailLength(xmlDocPtr doc, xmlNodePtr node, time_t tV)
{
    xmlChar *p;

    p = xmlGetProp(node, "longest");
    if (p != NULL && fpEmailLen != NULL) {
        if (tV >= statBot && tV <= statTop)
        fprintf(fpEmailLen, "%f %s\n", dataTimeIndex(tV), p);
    }

    p = xmlGetProp(node, "average");
    if (p != NULL && fpEmailLenAv != NULL) {
        if (tV >= statBot && tV <= statTop)
        fprintf(fpEmailLenAv, "%f %s\n", dataTimeIndex(tV), p);
    }
}

void parseExpLen(xmlDocPtr doc, xmlNodePtr node, time_t tV)
{
    xmlChar *p;
    int i;

    p = xmlGetProp(node, "i");

    if (p == NULL || (i = atoi(p)) < 0)
        return;
    if (i > 24) i = 24;

    p = xmlGetProp(node, "val");
    if (p != NULL && fpValidEmail != NULL) {
        if (tV >= statBot && tV <= statTop)
        fprintf(fpNExpire[i], "%f %s\n", dataTimeIndex(tV), p);
    }
}

void parseExpireLens(xmlDocPtr doc, xmlNodePtr node, time_t tV)
{
    xmlChar *p;

    node = node->xmlChildrenNode;

    while ( node && xmlIsBlankNode ( node ) )
        node = node -> next;

    while (node != NULL) {
        if ((!xmlStrcmp(node->name, (const xmlChar *) "explen"))) {
            parseExpLen(doc, node, tV);
        }
        node = node->next;
    }
}

void parseEmailStats(xmlDocPtr doc, xmlNodePtr node, time_t tV)
{
    xmlChar *p;

    p = xmlGetProp(node, "valid");
    if (p != NULL && fpValidEmail != NULL) {
        if (tV >= statBot && tV <= statTop)
        fprintf(fpValidEmail, "%f %s\n", dataTimeIndex(tV), p);
    }

    p = xmlGetProp(node, "none");
    if (p != NULL && fpNoneEmail != NULL) {
        if (tV >= statBot && tV <= statTop)
        fprintf(fpNoneEmail, "%f %s\n", dataTimeIndex(tV), p);
    }
}

void parseNickStats(xmlDocPtr doc, xmlNodePtr node, time_t tV)
{
    xmlChar *p;

    p = xmlGetProp(node, "total");
    if (p != NULL && regNicks != NULL) {
        if (tV >= statBot && tV <= statTop)
        fprintf(regNicks, "%f %s\n", dataTimeIndex(tV), p);
    }

    p = xmlGetProp(node, "accitems");
    if (p != NULL && fpAccItems != NULL) {
        if (tV >= statBot && tV <= statTop)
        fprintf(fpAccItems, "%f %s\n", dataTimeIndex(tV), p);
    }

    p = xmlGetProp(node, "numemails");
    if (p != NULL && fpNumEmails != NULL) {
        if (tV >= statBot && tV <= statTop)
        fprintf(fpNumEmails, "%f %s\n", dataTimeIndex(tV), p);
    }

    node = node->xmlChildrenNode;

    while ( node && xmlIsBlankNode ( node ) )
        node = node -> next;

    while (node != NULL) {
        if ((!xmlStrcmp(node->name, (const xmlChar *) "emails"))) {
            parseEmailStats(doc, node, tV);
        }
        if ((!xmlStrcmp(node->name, (const xmlChar *) "emaillength"))) {
            parseEmailLength(doc, node, tV);
        }
        if ((!xmlStrcmp(node->name, (const xmlChar *) "expirelens"))) {
            parseExpireLens(doc, node, tV);
        }
        node = node->next;
    }
}

void parseRun(xmlDocPtr doc, xmlNodePtr node)
{
    time_t tV;
    xmlChar *tm;

    tm = xmlGetProp(node, "time");
    if (tm == NULL)
	return;
    tV = atol(tm);

    node = node->xmlChildrenNode;

    while ( node && xmlIsBlankNode ( node ) )
        node = node -> next;

    while (node != NULL) {
        if ((!xmlStrcmp(node->name, (const xmlChar *) "nickstats"))) {
            parseNickStats(doc, node, tV);
        }
        node = node->next;
    }
}

void parseStats(xmlChar *fileName)
{
        extern int xmlDoValidityCheckingDefaultValue, xmlSubstituteEntitiesDefaultValue;
	xmlDocPtr doc;
	xmlNodePtr node;

        xmlDoValidityCheckingDefaultValue = 0;
	xmlKeepBlanksDefault(0);
	xmlSubstituteEntitiesDefault(1);

	doc = xmlParseFile(fileName);

	if (!doc)
		abort();

        node = xmlDocGetRootElement(doc);
	if (node == NULL) {
            fprintf(stderr,"empty document\n");
            xmlFreeDoc(doc);
            return;
        }

        if (xmlStrcmp(node->name, (const xmlChar *) "stats")) {
            printf("root is not stats, ack.\n");
            xmlFreeDoc(doc);
            return;
        }

    node = node->xmlChildrenNode;
    while ( node && xmlIsBlankNode ( node ) )
        node = node -> next;

    if (!node)
      return;

    //if ((service = xmlGetProp(xmlDocGetRootElement(doc), "service")) == NULL) { /// error

    while (node != NULL) {
        if ((!xmlStrcmp(node->name, (const xmlChar *) "statsrun"))) {
            parseRun(doc, node);
        }
        node = node->next;
    }

    xmlFreeDoc(doc);
    return;
}


int main()
{
	struct tm x;
	char buf[25];
	int i;
	statTop = time(NULL);

	setenv("TZ", "GMT", 1);
	tzset();
	x.tm_sec = 0;
	x.tm_min = 0;
	x.tm_hour = 0;
	x.tm_mday = 0;
	x.tm_mon = 0;
	x.tm_year = 97;
	x.tm_isdst = 0;
	statBot = mktime(&x);

	for(i = 0; i < 25; i++) {
		sprintf(buf, "nexp%d.dat", i);
		fpNExpire[i] = fopen(buf, "w");
	}

	regNicks = fopen("regnicks.dat", "w");
	fpNumEmails = fopen("emails.dat", "w");
	fpValidEmail = fopen("validemails.dat", "w");
	fpNoneEmail = fopen("noneemails.dat", "w");
	fpAccItems = fopen("accitems.dat", "w");
	fpEmailLen = fopen("emaillen_long.dat", "w");
	fpEmailLenAv = fopen("emaillen_av.dat", "w");
	parseStats("stats.xml");

	fclose(regNicks);
}

