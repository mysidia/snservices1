/**
 * \file email.h
 * \brief Email class
 *
 * Services interface for constructing and sending e-mails 
 *
 * \mysid
 * \date 2001
 *
 * $Id$
 */

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

class EmailString;
class EmailAddressBuf;
class EmailMessage;

/**
 * \class EmailString
 * \brief A string associated with an email field
 */

/** 
 * \fn const char *EmailString::add(const char *string);
 * \param string String to add
 * Concatenates a new string on an existing EmailString.
 */

/** 
 * \fn const char *EmailString::get_string()
 * \return The contents of an EmailString
 */

class EmailString
{
    friend class EmailAddressBuf;
    friend int main();

    public:
    EmailString() : theLength(0), theString(NULL) {
    }

    ~EmailString() {
       if (theString)
           delete theString;
       return;
    }

    const char *add(const char *);
    const char *get_string() { 
	    if ( theString ) 
		    return theString;
	    return "";
    }
    const char *set_string(const char *s);
    const char *set_string_ptr(char *s) {
          theLength = s ? strlen(s) : 0;
          return theString = s;  
    }
    int length() { return theLength; }

    const char *operator=(const char *buf) { return set_string(buf); };
    const char *operator+=(const char *buf) { return add(buf); };

    char &operator[](int i) {
         if (i<0||i>theLength) {
             abort();
             return ((char *)0x0)[0];
         }
         return theString[i];
    }

    operator char* () {
        return theString;
    }

    private:
    int theLength;
    char *theString;
};

/**
 * \class EmailAddressBuf
 *   A buffer, list of e-mail addresses
 */ 
 /**
 * \fn const char *EmailAddressBuf::add_email(const char *buf);
 * \param buf Adds a new e-mail address to the list
 */ 
 /**
 * \fn const char *EmailAddressBuf::get_string();
 * Returns a string representing the list of e-mail addresses
 */

class EmailAddressBuf : public EmailString
{
	public:
		const char *add_email(const char *);	
		const char *operator=(const char *buf) { return set_string(buf); };
		const char *operator+=(const char *buf) { return add_email(buf); };
};

/**
 * \class EmailMessage
 * \brief Describes an e-mail to be sent
 * Items of the EmailMessage class are e-mails to be built and sent.
 */
/**
 * \fn void EmailMessage::reset()
 * \brief Clear the contents of the e-mail structure
 * This is used to zero data in an EmailMessage structure so that a new
 * message may be constructed.
 */
class EmailMessage
{
    public:

          /// Sender address for e-mail
	EmailAddressBuf  from;

          /// Recipient address for e-mail
	EmailAddressBuf  to;

          /// Subject line for e-mail
	EmailString      subject;

          /// Body for e-mail
	EmailString      body;

          /// Reset the data, can be used to construct another message with
          /// the same object or to wipe out the data in the destruction
          /// process.
        void reset() {
             from = NULL;
	     to = NULL;
	     subject = NULL;
	     body = NULL;
        }

          /// Send an e-mail
	void send();
};
