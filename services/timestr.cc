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
#include <assert.h>

#include "timestr.h"
#include "options.h"

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
}

/// Build a time length from a string
TimeLengthString::TimeLengthString(const char* input)
{
	const char* p = input;
	int value = 0, k;

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

	// Now, normalize the representation.

	if (length.seconds >= 60) {
		k = length.seconds / 60;
		length.minutes += k;
		length.seconds -= k * 60;
	}

	// Reduce every 60 mins to an hour
	if (length.minutes >= 60) {
		k = length.minutes / 60;
		length.hours += k;
		length.minutes -= k * 60;
	}

	// Every 24 hours to a day
	if (length.hours >= 24) {
		k = length.hours / 24;
		length.days += k;
		length.hours -= k * 24;
	}

	if (length.hours < 0 || length.minutes < 0 || length.seconds < 0
		|| length.days < 0)
		f_isValid = false;
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

const char* TimeLengthString::asShortStr(char* buf, int len, bool pad) const
{
	int k = 0, l = 0;

	assert(len >= 10);

	// Number of days output
	if (length.days > 0) {
		if (pad == false) 
			k = snprintf(buf, len, "%dd", length.days);
		else
			k = snprintf(buf, len, "%4dd", length.days);
		if (k < 0)
			return NULL;
		l += k;
	}

	// Number of hours output
	if (length.hours > 0) {
		if (l >= len)
			return NULL;

		if (pad == false)
			k = snprintf(buf + l, len - l, "%dh", length.hours);
		else
			k = snprintf(buf + l, len - l, "%2dh", length.hours);

		if (k < 0)
			return NULL;
		l += k;
	}

	// Number of minutes output
	if (length.minutes > 0) {
		if (l >= len)
			return NULL;

		if (pad == false)
			k = snprintf(buf + l, len - l, "%dm", length.minutes);
		else
			k = snprintf(buf + l, len - l, "%2dm", length.minutes);

		if (k < 0)
			return NULL;
		l += k;
	}

	if (length.seconds > 0) {
		if (l >= len)
			return NULL;

		k = snprintf(buf + l, len - l, "%ds", length.seconds);
		if (k < 0)
			return NULL;
		l += k;
	}

	return buf;
}

const char* TimeLengthString::asLongStr(char* buf, int len) const
{
	int k =
	snprintf(buf, len, "%4d days, %2d hours, %2d minutes, and %2d seconds",
		length.days, length.hours, length.minutes, length.seconds);

	if (k == -1)
		return NULL;
	return buf;
}
