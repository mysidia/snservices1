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
 * \file timestr.h
 * \brief A class for dealing with Time Strings
 *
 * Procedures for constructing and interpreting time strings
 *
 * \mysid
 * \date 2004
 *
 * $Id$
 */

/**
 * A structure for holding some kind of (seconds*minutes*hours*days*...)
 * representation of a length of time
 */
struct DurationRepresentation {
	long seconds, minutes, hours, days;
};

/**
 * A class for interpreting and generating string representations
 * of simple duration lengths in (seconds,minutes,hours,days)
 */
class TimeLengthString
{
public:

	/// Create a time length string from a number of seconds
	TimeLengthString(int num_secs);

	/*! Create a time length string from a given short str
	 * of the form: "XXhXXd" etc.
         */
	TimeLengthString(const char* short_str);

	/// Does the object represent a legally created time string?
	bool isValid() const;

	/*! Get the total number of days
         *  @pre isValid() == true
         */ 
	int getTotalDays() const;

	/*! Get the total number of hours
	 *  @pre isValid() == true
         */
	int getTotalHours() const;

	/*! Get the total number of minutes
         * @pre isValid() == true
         */
	int getTotalMinutes() const;

        /*! Get the total number of seconds
         * @pre isValid() == true
	 */
	int getTotalSeconds() const;

	/*! Get the number of normalized days
	 * @pre isValid() == true
	 */
	int getDays() const;

	/*! Get the number of normalized hours
	 * @pre isValid() == true
	 */
	int getHours() const;

	/*! Get the number of normalized minutes
	 * @pre isValid() == true
	 */
	int getMinutes() const;

	/*! Get the number of normal seconds
	 * @pre isValid() == true
	 */
	int getSeconds() const;

	/*! Generate a string inside 'buf'
	 * @param buf The target memory space
	 * @param len The size of buf
	 * @param pad Should each output number be padded for output
	 * @param long_format Show it long?
	 * @param show_secs Show the seconds field at all?
	 * @pre 'buf' points to an allocated space of size len
	 * @pre len >= 25
	 * @pre isValid() == true
	 * @post Contents of 'buf' are unknown, it potentially contains
	 * a binary data structure in unspecified format 
	 * @return A pointer to a string representation of the short
	 *  string, or a NULL pointer if an error occurs.
	 *  If a string pointer is returned, it will be into somewhere
	 *  in the target space (buf).
	 */
	const char* TimeLengthString::asString(char* buf, int len, bool pad,
                        bool long_format, bool show_secs) const;


private:
	DurationRepresentation length;
	char* tempstr;
	bool f_isValid;
};
