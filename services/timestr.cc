/*
 * Copyright (c) 2004 James Hess
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
 * \file timestr.cc
 * \brief Methods for dealing with Time Strings
 *
 * Procedures for constructing and interpreting time strings
 *
 * \mysid
 * \date 2004
 *
 * $Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "timestr.h"
#include "services.h"

bool TimeLengthString::isValid() const
{
	return f_isValid;
}

/// Build a time length from a number of seconds
TimeLengthString::TimeLengthString(int num_secs)
{
	length.minutes = 0;
	length.hours = 0;
	length.days = 0;
	length.seconds = num_secs;
	f_isValid = true;
	normalize();
}

/// Build a time length from a string
TimeLengthString::TimeLengthString(const char* input)
{
	const char* p = input;
	int value = 0;

	if (input == NULL) {
		f_isValid = false;
		return;
	}
	length.seconds = length.days = length.hours = length.minutes = 0;
	f_isValid = true;

	while(*p)
	{
		if (isascii(*p) == 0 || isdigit(*p) == 0) {
			f_isValid = false;
			return;
		}

		value = strtol(p, (char **)&p, 10);
		while (*p && !isalpha(*p) && !isdigit(*p))
			p++;

		if (isalpha(*p) && tolower(*p) == 'h') {
			length.hours = value;
		}
		else if (isalpha(*p) && tolower(*p) == 'm')
			length.minutes = value;
		else if (isalpha(*p) && tolower(*p) == 's')
			length.seconds = value;
		else if (isalpha(*p) && tolower(*p) == 'd')
			length.days = value;
		else {
			f_isValid = false;
			return;
		}

		while(isalpha(*p) || isspace(*p))
			p++;
	}

	// Now normalize the representation
	normalize();
}

int TimeLengthString::getTotalDays() const
{
	return length.days 
		+ (length.hours / 24) 
 		+ (length.minutes / 60) / 24
		+ (length.seconds / 3600) / 24;
}

int TimeLengthString::getTotalHours() const
{
	return length.days * 24
		+ (length.hours)
		+ (length.minutes / 60)
		+ (length.seconds / 3600);
}

int TimeLengthString::getTotalMinutes() const
{
	return length.days * 24 * 60
		+ (length.hours * 60)
		+ (length.minutes)
		+ (length.seconds / 60);
}

int TimeLengthString::getTotalSeconds() const
{
	return length.days * 24 * 3600
		+ (length.hours * 3600)
		+ (length.minutes * 60)
		+ length.seconds;
}

int TimeLengthString::getDays() const
{
	return length.days;
}

int TimeLengthString::getMinutes() const
{
	return length.minutes;
}

int TimeLengthString::getHours() const
{
	return length.hours;
}

int TimeLengthString::getSeconds() const
{
	return length.seconds;
}

const char* TimeLengthString::asString(char* buf, int len, bool pad,
			bool long_format, bool show_secs) const
{
	int k = 0, l = 0, showdays, showhours, showmins, showsecs;
	int flag = 0;

	ASSERT(len >= 10);

	if (pad == false)
	{
		showdays = length.days > 0;
		showhours = length.hours > 0;
		showmins = length.minutes > 0;
		showsecs = length.seconds > 0;

		if (show_secs == false)
			showsecs = 0;

		if (showdays && showsecs)
			showhours = showmins = 1;

		if (showhours && showsecs)
			showmins = 1;
	}
	else
	{
		showsecs = show_secs;
		showdays = showhours = showmins = 1;
	}

	if (pad == false && long_format != 0) {
		showdays = 1;
		showmins = 1;
		showhours = 1;
	}


	// Number of nnn output
	// Macro to produce the code for generating the individual
	// field info.
	#define TIME_WRITE(field, f1, f2, f3) \
	{ \
		if (l >= len) \
			return NULL; \
		\
		if (long_format == false) { \
			if (pad == false) \
				k = snprintf(buf + l, len - l, f1, field); \
			else \
				k = snprintf(buf + l, len - l, f2, field); \
		} \
		else { \
			k = snprintf(buf + l, len - l, "%s" f3, \
				     (flag ? ", " : ""), field); \
			flag = 1; \
		} \
		if (k < 0) \
			return NULL; \
		l += k; \
	}


	if (showdays) {
		TIME_WRITE(length.days, "%dd", "%4dd", "%4d days");
	}

	// Number of hours output
	if (showhours) {
		TIME_WRITE(length.hours, "%dh", "%.2dh", "%2d hours");
	}

	// Number of minutes output
	if (showmins) {
		if (long_format == 0 || showsecs != 0)
		{
			TIME_WRITE(length.minutes, "%dm", "%.2dm", "%2d minutes");
		}
		else 
		{
			TIME_WRITE(length.minutes, "%dm", "%.2dm", "and %2d minutes");
		}
	}
		
	if (showsecs) {
		TIME_WRITE(length.seconds, "%ds", "%.2ds", "and %2d seconds");
	}

	return buf;
}

