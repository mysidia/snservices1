/*
 * Copyright (c) 2004, Onno Molenkamp
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "ircd.h"

IRCD_RCSID("$Id$");

#include <expat.h>
#include <fcntl.h>

node	*config = NULL;
monitor	*mon = NULL;

/*
 * element_start is called by the XML parser when a new configuration element
 * is read. It creates a node structure for the element and adds it to the
 * configuration tree.
 *
 * data       User defined data. In our case, a pointer to the variable
 *            containing the current node structure.
 * name       Name of the XML element.
 * attr       List of attributes.
 */

static void element_start(void *data, const char *name, const char **attr)
{
	node	**current = (node **) data;
	attribute	**a;
	node	*e = irc_malloc(sizeof(node));
	e->name = irc_strdup(name);
	e->attrs = NULL;
	e->parent = *current;
	e->next = NULL;
	e->children = NULL;
	e->data = NULL;
	e->status = CONFIG_NEW;

	a = &e->attrs;
	while (*attr != NULL)
	{
		*a = irc_malloc(sizeof(attribute));
		(*a)->name = irc_strdup(*attr++);
		(*a)->value = irc_strdup(*attr++);
		(*a)->next = NULL;
		a = &(*a)->next;
	}

	if (*current != NULL)
	{
		node	**p = &(*current)->children;
		while (*p != NULL)
		{
			p = &(*p)->next;
		}
		*p = e;
	}
	*current = e;
}

/*
 * element_end is called by the XML parser when a configuration element is
 * finished.
 *
 * data       User defined data. In our case, a pointer to the variable
 *            containing the current node structure.
 * name       Name of the XML element.
 */


static void element_end(void *data, const char *name)
{
	node	**current = (node **) data;
	if ((*current)->parent != NULL)
	{
		*current = (*current)->parent;
	}
}

/*
 * is_empty is a helper function to check if a string contains anything other
 * than whitespace.
 *
 * s          String. (not \0 terminated!)
 * len        Length of string.
 *
 * returns    1 if s only contains whitespace, 0 otherwise
 */

static int is_empty(char *s, int len)
{
	for (;len>0; len--, s++)
	{
		if (!isspace(*s))
		{
			return 0;
		}
	}
	return 1;
}

/*
 * element_data is called by the XML parser when a character data is read.
 * It adds the data to the current configuration node, if not whitespace.
 *
 * data       User defined data. In our case, a pointer to the variable
 *            containing the current node structure.
 * s          Character data. (not \0 terminated!)
 * len        Length of data.
 */

static void element_data(void *data, const XML_Char *s, int len)
{
	node	**current = (node **) data;
	node	*e;
	if (is_empty((char *) s, len))
	{
		return;
	}
	e = irc_malloc(sizeof(node));
	e->name = NULL;
	e->attrs = NULL;
	e->parent = *current;
	e->next = NULL;
	e->children = NULL;
	e->data = irc_malloc(len + 1);
	strncpy(e->data, s, len);

	if (*current != NULL)
	{
		node	**p = &(*current)->children;
		while (*p != NULL)
		{
			p = &(*p)->next;
		}
		*p = e;
	}
}

/*
 * config_get_attribute returns the value of an attribute for a given node.
 *
 * n          Node structure.
 * name       Name of attribute to return.
 *
 * returns    Value of attribute, or NULL if attribute wasn't found.
 */

char *config_get_attribute(node *n, char *name)
{
	attribute	*a = n->attrs;
	while (a != NULL)
	{
		if (!strcmp(a->name, name))
		{
			return a->value;
		}
		a = a->next;
	}
	return NULL;
}

/*
 * config_get_string returns the character data of a specified node.
 *
 * root       Root node.
 * name       Name of node, relative to root node. If NULL, look for character
 *            data in root node itself.
 *
 * returns    Character data of node, or NULL if not found.
 */

char *config_get_string(node *root, char *name)
{
	node	*n = config_get_node(root, name);
	node	*c;
	if (n == NULL)
	{
		return NULL;
	}
	c = n->children;
	while (c != NULL)
	{
		if (c->name == NULL)
		{
			return (char *) c->data;
		}
		c = c->next;
	}
	return NULL;
}

/*
 * config_get_node returns a node structure for a specified node.
 *
 * root       Root node.
 * name       Name of node, relative to root node. If NULL, return root node
 *            itself.
 *
 * returns    Node structure, or NULL is not found.
 */

node *config_get_node(node *root, char *name)
{
	node	*n;
	char	*x, *y, *z = NULL;

	if (name == NULL)
	{
		return root;
	}
	x = irc_strdup(name);
	n = root;
	for (y = strtok_r(x, "/", &z); y != NULL && n != NULL; y = strtok_r(NULL, "/", &z))
	{
		n = n->children;
		while (n != NULL)
		{
			if (n->name != NULL && !strcmp(y, n->name))
			{
				break;
			}
			n = n->next;
		}
	} while (y != NULL && n != NULL);
	irc_free(x);
	return n;
}

/*
 * config_get_next_node can be used to iterate through a list of nodes.
 *
 * root       Root node.
 * name       Name of node, relative to root node.
 * last       Last returned node, or NULL.
 *
 * returns    Next node in list.
 */

node *config_get_next_node(node *root, char *name, node *last)
{
	node	*n;
	if (last == NULL)
	{
		return config_get_node(root, name);
	}
	n = last->next;
	while (n != NULL)
	{
		if (n->name != NULL && !strcmp(last->name, n->name))
		{
			return n;
		}
		n = n->next;
	}
	return NULL;
}

/*
 * config_compare_node compares two nodes.
 *
 * a          Node.
 * b          Node.
 *
 * returns    0 if a and b are equal, !0 otherwise
 */

static int config_compare_node(node *a, node *b)
{
	int	n;
	attribute	*attr1, *attr2;
	char	*s;
	if (a->name == NULL || b->name == NULL)
	{
		if (a->name != b->name)
		{
			return -1;
		}
		return strcmp(a->data, b->data);
	}
	n = strcmp(a->name, b->name);
	if (n != 0)
	{
		return n;
	}
	attr1 = a->attrs;
	attr2 = b->attrs;
	while (attr1 != NULL)
	{
		if (attr2 == NULL)
		{
			return -1;
		}
		s = config_get_attribute(b, attr1->name);
		if (s == NULL)
		{
			return -1;
		}
		n = strcmp(attr1->value, s);
		if (n != 0)
		{
			return n;
		}
		attr1 = attr1->next;
		attr2 = attr2->next;
	}
	if (attr2 != NULL)
	{
		return -1;
	}
	a = a->children;
	b = b->children;
	while (a != NULL)
	{
		if (b == NULL)
		{
			return -1;
		}
		n = config_compare_node(a, b);
		if (n != 0)
		{
			return n;
		}
		a = a->next;
		b = b->next;
	}
	if (b != NULL)
	{
		return -1;
	}
	return 0;
}

/*
 * config_free_node frees all memory used by a node.
 *
 * n          Node to be freed.
 */

static void config_free_node(node *n)
{
	node	*m;
	attribute	*a, *b;
	if (n == NULL)
	{
		return;
	}
	n->parent = NULL;
	n->next = NULL;
	while (n != NULL)
	{
		m = n;
		if (n->children != NULL)
		{
			n = n->children;
			m->children = NULL;
		}
		else
		{
			if (n->next != NULL)
			{
				n = n->next;
			}
			else
			{
				n = n->parent;
			}
			if (m->name == NULL)
			{
				free(m->data);
			}
			else
			{
				irc_free(m->name);
			}
			a = m->attrs;
			while (a != NULL)
			{
				irc_free(a->name);
				irc_free(a->value);
				b = a->next;
				irc_free(a);
				a = b;
			}
			irc_free(m);
		}
	}
}

/*
 * config_read reads in a new configuration and fires events for changed
 * nodes that are monitored if the configuration is valid.
 *
 * name       Name of the rootnode.
 * file       Filename of the configuration file to read.
 *
 * returns    0 if succesful, !0 otherwise
 */

int config_read(char *name, char *file)
{
	node	*root = NULL;
	XML_Parser	p;
	int	fd;
	int	len;
	monitor	*m;
	node	*n, *n2;
	static char	buffer[1024];

	fd = open(file, O_RDONLY);
	if (fd < 0)
	{
		Debug((DEBUG_ERROR, "Couldn't open configuration file"));
		return -1;
	}

	p = XML_ParserCreate(NULL);
	if (!p)
	{
		Debug((DEBUG_ERROR, "Couldn't allocate memory for parser"));
		close(fd);
		return -1;
	}

	XML_SetElementHandler(p, element_start, element_end);
	XML_SetCharacterDataHandler(p, element_data);
	XML_SetUserData(p, &root);
	do
	{
		len = read(fd, buffer, sizeof(buffer));
		if (len < 0)
		{
			Debug((DEBUG_ERROR, "Read error"));
			close(fd);
			XML_ParserFree(p);
			config_free_node(root);
			return -1;
		}
		if (XML_Parse(p, buffer, len, len>0?0:1) == XML_STATUS_ERROR)
		{
			Debug((DEBUG_ERROR, "Parse error at line %d: %s",
				XML_GetCurrentLineNumber(p),
				XML_ErrorString(XML_GetErrorCode(p))));
			close(fd);
			XML_ParserFree(p);
			config_free_node(root);
			return -1;
		}
	} while (len > 0);
	close(fd);
	XML_ParserFree(p);
	if (strcmp(root->name, name))
	{
		Debug((DEBUG_ERROR, "Invalid rootnode"));
		config_free_node(root);
		return -1;
	}
	if (config != NULL)
	{
		for (m = mon; m != NULL; m = m->next)
		{
			if (m->type == CONFIG_SINGLE)
			{
				continue;
			}
			n = NULL;
			while ((n = config_get_next_node(config, m->path, n)) != NULL)
			{
				n2 = NULL;
				while ((n2 = config_get_next_node(root, m->path, n2)) != NULL)
				{
					if (n2->data == NULL && !config_compare_node(n, n2))
					{
						n2->data = n;
						break;
					}
				}
				if (n2 == NULL && n->status == CONFIG_OK)
				{
					m->h(n);
				}
			}
		}
	}
	for (m = mon; m != NULL; m = m->next)
	{
		n = NULL;
		while ((n = config_get_next_node(root, m->path, n)) != NULL)
		{
			if (n->data != NULL)
			{
				n->status = ((node *) n->data)->status;
				n->data = ((node *) n->data)->data;
				if (n->status != CONFIG_OK)
				{
					n->status = m->h(n);
				}
			}
			else
			{
				n->status = m->h(n);
			}
		}
	}
	config_free_node(config);
	config = root;
	return 0;
}

/*
 * config_monitor can be used to monitor a specific node(list). Every time
 * one of the selected nodes changes the handler is called.
 * With CONFIG_SINGLE, the handler will only be called with the new node. With
 * CONFIG_LIST, the handler will first be called for the nodes that are
 * removed, and then with the nodes that are added.
 *
 * path       Node path, relative to the configuration rootnode.
 * h          Handler.
 * type       CONFIG_SINGLE or CONFIG_LIST
 */

void config_monitor(char *path, conf_handler h, int type)
{
	monitor	*m = irc_malloc(sizeof(monitor));
	m->path = path;
	m->type = type;
	m->h = h;
	m->next = mon;
	mon = m;
}
