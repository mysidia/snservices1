/**
 * \file credits.c
 * \brief Services credits
 *
 * This module contains the credits for services.
 *
 * \mysid
 * \date 2000, 2001
 *
 * $Id$
 */

/*
 * Copyright (c) 2000 James Hess
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

#include <stdio.h>

/**
 * @brief Services' credits
 * This is the text shown when a user does /INFO <services>
 */

char *services_info[] =
{
 "Services is  Copyright (c) 1996-1997 Chip Norkus",
 "             Copyright (c) 1997 Max Byrd",
 "             Copyright (c) 1997 Greg Poma",
 "             Copyright (c) 1997, 1999 Michael Graff", 
 "             Copyright (c) 1998-2001 James Hess", 
 "             Copyright (c) 1997 Dafydd James", 
 "             All rights reserved.",
 "",
 "The following helped in the development of the SorceryNet",
 "services system based on the original services set which was",
 "developed by Chip Norkus, Max Byrd, and Greg Poma:",
 "",
 "   Some2           -                  -                  ",
 "   TazQ            -                  tazq@sorcery.net   ",
 "   White_dragon    -                  -                  ",
 "   Mysidia         James Hess         mysidia-ss@flame.org",
 "   DuffJ           Dafydd James       duffj@sorcery.net  ",
 "   Echostar        -                  -                  ",
 "   Slvrchair       -                  slvrchair@sorcery.net",
 "",
 "This product includes software developed by: Chip Norkus, Max Byrd,",
 "Greg Poma, Michael Graff, James Hess, and Dafydd James.",
 NULL
};
