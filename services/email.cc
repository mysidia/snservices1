/*
 * Copyright (c) 2001 James Hess
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
 * \file email.cc
 * \brief E-mail methods
 *
 * Procedures for constructing and transmitting e-mail messages
 * by services
 *
 * \mysid
 * \date 1996-2001
 *
 * $Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "email.h"
#include "options.h"

/**
 * \brief EmailString constructor
 * \post An EmailString object has been initialized
 */
EmailString::EmailString() : theLength(0), theString(NULL)
{
}

/**
 * \brief EmailString destructor
 * \post An EmailString object has been destroyed
 */
EmailString::~EmailString()
{
	if (theString)
		delete theString;
	return;
}

/**
 * \brief Return a pointer to the internal representation (char array)
 * \return A pointer to the current value of the actual string
 */
const char *EmailString::get_string()
{
	if ( theString )
		return theString;
	return "";
}

/**
 * \brief Sets the internal string pointer for an EmailString object
 * \param s The new character pointer to set
 * \return A pointer to the new value of the string
 * \pre s must point to a valid NUL-terminated character array
 * \post The string received has been copied into the present value
 *       which is located in a new memory area and pointed to by the
 *       object.             
 */
const char *EmailString::set_string_ptr(char *s)
{
	theLength = s ? strlen(s) : 0;
	return theString = s;
}

/**
 * \brief Subscript operator to access any character in an EmailString object
 * \param i subscript to access
 * \return reference to the character accessed
 * \pre integer i is >= 0
 */
char &EmailString::operator [](int i)
{
	if (i<0||i>theLength) {
		abort();
		return ((char *)0x0)[0];
	}

	return theString[i];
}

/**
 * \brief Assign a new string value to an EmailString object
 * \param str New string to set
 * \return A pointer to the new value of the string
 * \pre    Str is a pointer to a valid NUL-terminated character array
 * \post   The string received has been copied into the present value
 *         which is located in a new memory area and pointed to by the
 *         object.
 */
const char *EmailString::set_string(const char *str)
{
	int len;

	if (theString)
		delete theString;
	if (!str)
		return (theString = NULL);

	theString = new char[(len = strlen(str)) + 1];
	strcpy(theString, str);
	theLength = 0;

	if (!theString) {
		fprintf(stderr, "Out of memory!\n");
		abort();
		return theString;
	}
	theLength = len;
	return theString;
}

/**
 * \brief Appends an email string
 * \param str Text to add
 * \pre   Str is a pointer to a valid NUL-terminated character array
 */
const char *EmailString::add(const char *str)
{
	int len;

	if (!str)
		return NULL;
	len = strlen(str);

	if (!theString) {
		theString = new char[len + 1];
		if (!theString) {
			fprintf(stderr, "Out of memory!\n");
			abort();
			return str;
		}
		strcpy(theString, str);
		theLength = len;
		return str;
	}

	theString =
		static_cast<char *>(realloc(theString, len + theLength + 1));
	strcat(theString + theLength, str);
	theLength += len;
	return theString;
}

/**
 * \brief Adds an e-mail address to a list of addresses
 * \param str Text of e-mail address to be added
 * \pre   Str is a pointer to a valid NUL-terminated character array
 *        containing a valid e-mail address.
 */
const char *EmailAddressBuf::add_email(const char *str)
{
	int len;

	if (!str)
		return NULL;
	len = strlen(str);

	if (!theString) {
		theString = new char[len + 1];
		if (!theString) {
			fprintf(stderr, "Out of memory!\n");
			abort();
			return str;
		}
		strcpy(theString, str);
		theLength = len;
		return str;
	}

	theString =
		static_cast<char *>(realloc(theString, len + theLength + 3));
	strcat(theString + theLength, ", ");
	strcat(theString + theLength + 2, str);
	theLength += len + 2;
	return theString;
}

/**
 * \brief EmailMessage constructor
 * \post An EmailMessage object has been initialized
 */
EmailMessage::EmailMessage()
{
}

/**
 * \brief The message has been blanked
 * \post The email message is empty
 */
void EmailMessage::reset()
{
	from = NULL;
	to = NULL;
	subject = NULL;
	body = NULL;
}

/**
 * \brief Sends an e-mail
 * \pre   The 'from', 'body', 'subject', and 'to' fields have been
 *        set to valid values since the last reset or send.
 * \post  The e-mail is sent
 */
void EmailMessage::send()
{
	FILE *p;

	if (!to.get_string() || !from.get_string() || !body.get_string()) {
		fprintf(stderr, "EmailMessage::send(): Failed.");
		return;
	}

	p = popen(SENDMAIL, "w");
	if (p == NULL)
		return;

	fprintf(p, "To: %s\n"
	           "From: %s\n"
	           "Subject: %s\n",
	to.get_string(), from.get_string(), subject.get_string()? subject.
	get_string() : "[services] No subject");

	fputs(body.get_string(), p);
	fflush(p);
	pclose(p);
}
