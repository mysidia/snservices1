/* $Id$ */

/* Copyright (c) 1998 Michael Graff <explorer@flame.org>
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

#include "config.h"

/**
 * Maximum number help lines
 * \bug should be dyniamic, or a linked list
 */
#define HELPFILE_MAX_LINES 128

#define HELPFILE_CMD_PRINTF    1  // print the line
#define HELPFILE_CMD_EXIT      2  // terminate printing

#define HELPFILE_FLAG_OPER		0x00000001
#define HELPFILE_FLAG_SERVOP		0x00000002
#define HELPFILE_FLAG_SRA		0x00000004

class helpline_t {
private:
	u_int32_t   flags;
	u_int32_t   mask;
	u_int32_t   cmd;
	char       *text;
	helpline_t *next;

public:
	helpline_t(void);
	helpline_t(u_int32_t, u_int32_t, u_int32_t, char *);

	~helpline_t();

	void delchain(void);

	inline helpline_t *get_next(void) { return next; }
	inline void set_next(helpline_t *x_next) { next = x_next; }

	inline u_int32_t get_flags(void) { return flags; }
	inline void set_flags(u_int32_t x_flags) { flags = x_flags; }

	inline u_int32_t get_mask(void) { return mask; }
	inline void set_mask(u_int32_t x_mask) { mask = x_mask; }

	inline u_int32_t get_cmd(void) { return cmd; }
	inline void set_cmd(u_int32_t x_cmd) { cmd = x_cmd; }

	inline char *get_text(void) { return text; }
	inline void set_text(char *x_text) { text = x_text; }
};

class helpfile_t {
private:
	char        *fname;
  	helpline_t  *first;
	helpline_t  *last;

public:
	helpfile_t(void);
	~helpfile_t();

	void readfile(char *);
	void addline(u_int32_t, u_int32_t, u_int32_t, char *);
	void display(u_int32_t);
};

typedef struct {
	char      *name;
	u_int32_t  flags;
	u_int32_t  mask;
} flagmap_t;

void helpfile_parse_if(u_int32_t *, u_int32_t *, char *);
