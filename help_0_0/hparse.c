#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

int htmlGen = 0;

struct xt { xmlChar *topic; xmlChar *filename; };
struct xt *topics;
int ntopics = 0;

void parseHelp(xmlChar *fileName);

const xmlChar *strpv(const xmlChar *service)
{
        if (xmlStrcmp(service, (const xmlChar *) "chanserv") == 0)
		return "Channel Services";
        else if (xmlStrcmp(service, (const xmlChar *) "nickserv") == 0)
		return "NickName Services";
        else if (xmlStrcmp(service, (const xmlChar *) "memoserv") == 0)
		return "Memo Services";
        else if (xmlStrcmp(service, (const xmlChar *) "gameserv") == 0)
		return "Game Services";
        else if (xmlStrcmp(service, (const xmlChar *) "infoserv") == 0)
		return "Information Services";
        else if (xmlStrcmp(service, (const xmlChar *) "operserv") == 0)
		return "Operator Services";
        return service;
}

xmlChar *trimSpaces(xmlChar *ct, int f)
{
	xmlChar *q, *r;
	xmlChar *p, *cto = ct, *s;
	int ss = 0, sp = 0, j;
	int countLines = 0, cy = 0, B, Q, g;
	int reserve;

	if (ct == NULL)
		return NULL;

	if (f)
	{
		while(isspace(*ct) && isspace(ct[1]))
			ct++;

		for(p = ct + xmlStrlen(ct) - 1;
			(*p == '\n' || (isspace(*p) && isspace(p[-1]))) && p > ct; p--)
		{
			if (*p == '\n' && !isspace(p[-1])) {
				*p = ' ';
				p[1] = '\0';
			}
			else
				*p = '\0';
		}

	}

	for(p = ct; *p; p++) {
		if (*p == '\n')
			countLines++;
		if (htmlGen && (*p == '<'))
			cy += 3;
		if (htmlGen && (*p == '&'))
			cy += 4;
	}

	p = ct;

	if (htmlGen == 0) {
		reserve = 0;
		q = (xmlChar *)malloc(xmlStrlen(p) + (countLines * 5) + cy + 1);
	}
	else {
		q = (xmlChar *)malloc(512 + xmlStrlen(p) + (countLines * 25) + cy + 1);
		reserve = 512 + (20 * countLines) - 25;
	}

	*q = '\0';

	// xmlStrcat(q, p);

	s = q;

	for(r = p; *r; r++) {
		if (!f || !isspace(*r) || *r == '\n' || ss == 0) {
		   if (htmlGen && *r == '<') { 
                       *(s++) = '&';
                       *(s++) = 'l';
                       *(s++) = 't';
                       *(s++) = ';';
                   }
		   else if (htmlGen && *r == '&') { 
                       *(s++) = '&';
                       *(s++) = 'a';
                       *(s++) = 'm';
                       *(s++) = 'p';
                       *(s++) = ';';
                   }
		   else {
			if (htmlGen && (sp || ss || r == q) && reserve >= 30) {
				for(B = 0; B < ntopics; B++)
					if (xmlStrncasecmp(topics[B].topic, r, strlen(topics[B].topic)) == 0
					    && (r[strlen(topics[B].topic)] == 0 || isspace(r[strlen(topics[B].topic)]) || ispunct(r[strlen(topics[B].topic)])))
					    break;
				if (B < ntopics) 
				{
					*(s++) = '<';	*(s++) = 'a';
					*(s++) = ' ';	*(s++) = 'h';
					*(s++) = 'r';	*(s++) = 'e';
					*(s++) = 'f';   *(s++) = '=';
					*(s++) = '\"';;
					reserve -= 9;

					for(Q = 0; topics[B].filename[Q] != '\0'; Q++)
					{
						*(s++) = topics[B].filename[Q];
						reserve--;
					}

					*(s++) = '#';

					for(Q = 0; topics[B].topic[Q] != '\0'; Q++)
					{
						*(s++) = topics[B].topic[Q];
						reserve--;
					}

					*(s++) = '\"';
					*(s++) = '>';
					reserve -= 3;

					for(Q = 0; topics[B].topic[Q] != '\0'; Q++)
					{
						*(s++) = r[Q];
						reserve--;
					}
					*(s++) = '<';	*(s++) = '/';
					*(s++) = 'a';	*(s++) = '>';
					reserve -= 4;

					ss = 2;
					r += Q;
				}
			}
			*(s++) = *r;
		  }
		    if (!f && *r == '\n' && r[1] != '\0') {
			for(j = 0; j < 5; j++)
			   *(s++) = ' ';
		    }
		}

		if (ss == 2)
			ss = 0;
		else if (isspace(*r))
			ss = 1;
		else
			ss = 0;

		sp = ispunct(*r);
	}

	*(s++) = '\0';
	free(cto);

	return q;
}

void doAddTopic(xmlNodePtr node, xmlChar *fileName)
{
	xmlChar *tN = xmlGetProp(node, "name");
	xmlChar *fn, *i, *j;

	if (tN == NULL)
		return;

	if (ntopics == 0)
		topics = malloc(sizeof(struct xt) * 2);
	else
		topics = realloc(topics, sizeof(struct xt) * (ntopics + 1));

	fn = NULL;
	fn = xmlStrdup(fileName);

	for(i = fn, j = NULL; *i; i++)
		if (*i == '.')
			j = i;

	if (j) {
		*j++ = '.';
		*j++ = 'h';
		*j++ = 't';
		*j++ = 'm';
		*j = '\0';
		fn = xmlStrcat(fn, "l");
	}

	topics[ntopics].topic = xmlStrdup(tN);
	topics[ntopics].filename = fn;
	ntopics++;

	// printf("Added %s/%s.\n", tN, fileName);
}

void addToTopicIndex(xmlChar *fileName)
{
        extern int xmlDoValidityCheckingDefaultValue, xmlSubstituteEntitiesDefaultValue;
	xmlDocPtr doc;
	xmlNsPtr ns;
	xmlNodePtr node;
	char *service;

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

        if (xmlStrcmp(node->name, (const xmlChar *) "help")) {
            printf("root is not help, ack.\n");
            xmlFreeDoc(doc);
            return;
        }

    node = node->xmlChildrenNode;
    while ( node && xmlIsBlankNode ( node ) )
        node = node -> next;

    if (!node)
      return;

    if ((service = xmlGetProp(xmlDocGetRootElement(doc), "service")) == NULL) { /// error
        return;
    }

    while (node != NULL) {
        if ((!xmlStrcmp(node->name, (const xmlChar *) "topic"))) {
            doAddTopic(node, fileName);
        }
        node = node->next;
    }

    xmlFreeDoc(doc);

    return;
}

int main(int argc, char *argv[])
{
	int i;

#ifdef HTML
         htmlGen = 1;
#endif
	if (argc < 2) {
		printf("Usage: hparse <Filename>\n");
		return 0;
	}

	for(i = 1; i < argc; i++)
		addToTopicIndex(argv[i]);

	parseHelp(argv[1]);
}

void parseSyntax(xmlDocPtr doc, xmlNsPtr ns, xmlNodePtr node)
{
	xmlChar *p;

	if (htmlGen == 0)
		printf("     \2SYNTAX:\2\n");
	else
		printf("<p><b>Syntax:</b><pre>");

	node = node->xmlChildrenNode;

	while(node != NULL)
	{
		if (xmlStrcmp(node->name, "text") == 0 && node->content)
		{
			node->content = p = trimSpaces(node->content, 1);
			printf("    %s\n", p);
		}
		node = node->next;
	}

	if (htmlGen == 0)
		printf(" \n");
	else
		printf(" </pre></p>\n");
}

void parseBold(xmlDocPtr doc, xmlNsPtr ns, xmlNodePtr node)
{
	xmlChar *p;
	int k = 0;

	if (htmlGen == 0)
		printf("\2");
	else
		printf("<b>");

	node = node->xmlChildrenNode;

	while(node != NULL)
	{
		if (xmlStrcmp(node->name, "text") == 0 && node->content)
		{
			p = node->content = trimSpaces(node->content, 1);
			if (k != 0)
				printf("    %s\n", p);
			else
				printf("%s", p);
		}

		node = node->next;
//		if (k == 0)
//		    k = 1;
	}

	if (htmlGen == 0)
		printf("\2");
	else
		printf("</b>");
}

void parseDesc(xmlDocPtr doc, xmlNsPtr ns, xmlNodePtr node)
{
	xmlChar *p;
	int kS = 1;

	if (htmlGen == 0)
		printf("     \2DESCRIPTION:\2\n");
	else
		printf("     <b>Description:</b><p>\n");

	node = node->xmlChildrenNode;

	while(node != NULL)
	{
		if (xmlStrcmp(node->name, "text") == 0 && node->content)
		{
			p = node->content = trimSpaces(node->content, 1);

			if (kS == 1)
				printf("    %s", p);
			else
				printf("%s", p);

			kS = 1;

		}

		if (xmlStrcmp(node->name, "b") == 0) {
			parseBold(doc, ns, node);
			kS = 0;
		}
		node = node->next;
	}

	if (htmlGen == 0)
		printf("\n \n");
	else
		printf("</p>\n");
}

void parseSection(xmlDocPtr doc, xmlNsPtr ns, xmlNodePtr node)
{
	xmlChar *n = xmlGetProp(node, "name");
	xmlChar *f = xmlGetProp(node, "fmt");
	xmlChar *h = xmlGetProp(node, "h");
	xmlChar *p;
	int kS = 1;

	if (n) {
		if (h == NULL || xmlStrcmp(h, "b") == 0) {
			if (htmlGen == 0)
				printf("     \2%s:\2\n", n);
			else
				printf("     <b>%s</b><p>", n);
		}
		else if (xmlStrcmp(h, "u") == 0) {
			if (htmlGen == 0)
				printf("     \37%s:\37\n", n);
			else
				printf("     <u>%s</u><p>", n);
		}
		else {
			if (htmlGen == 0)
				printf("     %s:\n", n);
			else
				printf("     %s:<p>\n", n);
		}
	}
	else if (htmlGen)
		printf("<p>");

	if (f != NULL && xmlStrcmp(f, "pre"))
		f = NULL;

        if (f) printf("<pre>");

	node = node->xmlChildrenNode;

	while(node != NULL)
	{
		if (xmlStrcmp(node->name, "text") == 0 && node->content)
		{
			if (f == NULL)
				p = node->content = trimSpaces(node->content, 1);
			else
				p = node->content = trimSpaces(node->content, 0);

			if (kS == 1)
				printf("    %s", p);
			else
				printf("%s", p);

			kS = 1;
		}

		if (xmlStrcmp(node->name, "b") == 0) {
			parseBold(doc, ns, node);
			kS = 0;
		}
		node = node->next;
	}

        if (f) printf("</pre>");

	if (htmlGen == 0) {
		if (f == NULL)
			printf("\n \n");
		else 
			printf(" \n");
	}
	else printf("</p>\n");
}

void parseEx(xmlDocPtr doc, xmlNsPtr ns, xmlNodePtr node)
{
	xmlChar *pre = xmlGetProp(node, "pre");
	xmlChar *p;

	node = node->xmlChildrenNode;

	if (pre && *pre) {
		printf("     %s\n", pre);
	}

	if (htmlGen)
		printf("<blockquote><dd>");

	while(node != NULL)
	{
		if (xmlStrcmp(node->name, "text") == 0 && node->content)
		{
			p = node->content = trimSpaces(node->content, 1);

			printf("       %s\n", p);
		}
		node = node->next;
	}

	if (htmlGen)
		printf("</blockquote><br>\n");
}


void parseExamples(xmlDocPtr doc, xmlNsPtr ns, xmlNodePtr node)
{
	if (htmlGen == 0)
		printf("     \2EXAMPLES:\2\n");
	else {
		printf("<p><b>Examples:</b><br>\n");
	}
	node = node->xmlChildrenNode;

	while(node != NULL)
	{
		if (xmlStrcmp(node->name, "ex") == 0)
			parseEx(doc, ns, node);
		node = node->next;
	}

	if (htmlGen == 0)
		printf(" \n");
	else
		printf(" </p>\n");
}


void parseCommand(xmlDocPtr doc, xmlNsPtr ns, xmlNodePtr node)
{
	node = node->xmlChildrenNode;

	while(node != NULL)
	{
		if (xmlStrcmp(node->name, "syntax") == 0)
			parseSyntax(doc, ns, node);
		else if (xmlStrcmp(node->name, "desc") == 0)
			parseDesc(doc, ns, node);
		else if (xmlStrcmp(node->name, "section") == 0)
			parseSection(doc, ns, node);
		else if (xmlStrcmp(node->name, "examples") == 0)
			parseExamples(doc, ns, node);
		else if (xmlStrcmp(node->name, "limit") == 0) {
			xmlChar *xx = xmlGetProp(node, "access");

			if (xx) {
				printf("     \2Limited to %s Access\2\n \n", xx);
			}
		}

		node = node->next;
	}
}

void parseTopic(xmlDocPtr doc, xmlNsPtr ns, xmlNodePtr node)
{
	xmlNodePtr root = xmlDocGetRootElement(doc);
	xmlChar *service = xmlGetProp(root, "service");
	xmlChar *topicName;

	topicName = xmlGetProp(node, "name");

	if (service == NULL)
		return;

	if (topicName != NULL && !xmlGetProp(node, "hide")) {
		if (htmlGen == 0)
			printf(" \n === \2%s\2 ===\n \n", topicName);
		else
			printf(" \n <br>=== <b>%s</b> ===\n<br>", topicName);
	}

	node = node->xmlChildrenNode;

	while (node != NULL) {
	     if (xmlStrcmp(node->name, "command") == 0)
		 parseCommand(doc, ns, node);

             node = node->next;
        }

	//if (htmlGen != 0)
	//	printf("<br>\n");

	return;
}

void parseHelp(xmlChar *fileName)
{
        extern int xmlDoValidityCheckingDefaultValue, xmlSubstituteEntitiesDefaultValue;
	xmlDocPtr doc;
	xmlNsPtr ns;
	xmlNodePtr node;
	char *service;

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

        if (xmlStrcmp(node->name, (const xmlChar *) "help")) {
            printf("root is not help, ack.\n");
            xmlFreeDoc(doc);
            return;
        }


    node = node->xmlChildrenNode;
    while ( node && xmlIsBlankNode ( node ) )
        node = node -> next;

    if (!node)
      return;

    if ((service = xmlGetProp(xmlDocGetRootElement(doc), "service")) == NULL) { /// error
        return;
    }


    if (htmlGen == 0)
        printf("--- \2SorceryNet %s Help\2 ---\n",
           strpv(service));
    else
        printf("<HTML><BODY BGCOLOR=ffffff>--- <b>SorceryNet %s Help</b> ---<br>\n",
           strpv(service));

    while (node != NULL) {
        if ((!xmlStrcmp(node->name, (const xmlChar *) "topic"))) {
            parseTopic(doc, ns, node);
        }
        node = node->next;
    }

    if (htmlGen == 0)
        printf("--- \2End of Services Help\2 ---\n");
    else
        printf("--- <b>End of Services Help</b> ---</BODY></HTML>\n");

    xmlFreeDoc(doc);

    return;
}


