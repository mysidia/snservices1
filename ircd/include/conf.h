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

/*
 * This header defines the generic configuration functions and structures used
 * by ircd.
 *
 * $Id$
 */

#ifndef CONF_H
#define CONF_H

#define CONF_HANDLER(name) int name(node *n)

typedef struct attribute_t attribute;
typedef struct node_t node;
typedef struct monitor_t monitor;
typedef int (*conf_handler)(node *);

struct attribute_t {
        char    *name;
        char    *value;
        attribute       *next;
};

struct node_t {
        char    *name;
        attribute       *attrs;
        node    *parent;
        node    *next;
        node    *children;
        void    *data;
	int	status;
};

struct monitor_t {
        char    *path;
        int     type;
        conf_handler h;
        monitor *next;
};

#define CONFIG_NEW	0
#define CONFIG_OK	1
#define CONFIG_BAD	2

#define CONFIG_SINGLE	0
#define CONFIG_LIST	1

char *config_get_attribute(node *n, char *name);
char *config_get_string(node *root, char *name);
node *config_get_node(node *root, char *name);
node *config_get_next_node(node *root, char *name, node *elm);
int config_read(char *name, char *file);
void config_monitor(char *path, conf_handler h, int type);

#endif
