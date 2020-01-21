/*
 *  Copyright (c) 2015, Martin Hammel
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *      * Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *      * Neither the name of Arctica Technologies nor the
 *        names of its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "console.h"

#include <stdio.h>

#ifdef USE_EDITLINE
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <histedit.h>


static char* console_prompt = NULL;

static History* console_history = NULL;
static EditLine* console_editline = NULL;


static char* console_get_prompt( EditLine* el )
{
	(void)el;

	return console_prompt;
}

void console_init()
{
	console_prompt = strcpy( malloc( 3 ), "> " );

	HistEvent hev;
	console_history = history_init();
	history( console_history, &hev, H_SETSIZE, 100 );
	history( console_history, &hev, H_SETUNIQUE, 1 );

	console_editline = el_init( "prudebug", stdin, stdout, stderr );
	el_set( console_editline, EL_EDITOR, "emacs" );
	el_set( console_editline, EL_PROMPT, console_get_prompt );
	el_set( console_editline, EL_HIST, history, console_history );
}

void console_shutdown()
{
	el_end( console_editline );
	history_end( console_history );

	free( console_prompt );
	console_prompt = NULL;
}

void console_set_prompt( const char* prompt )
{
	free( console_prompt );
	console_prompt = strcpy( malloc( strlen( prompt ) + 1 ), prompt );
}

static const char* console_getline()
{
	int count;
	const char* line = el_gets( console_editline, &count );

	// do not add to history if line is empty
	if( line ) {
		const char* l;
		for( l = line; *l; l++ ) {
			if( !isspace(*l) ) {
				HistEvent hev;
				history( console_history, &hev, H_ENTER, line );
				break;
			}
		}
	}

	return line;
}

char console_getchar()
{
	static const char* line = NULL;

	for(;;) {
		while( line == NULL ) line = console_getline();

		if( *line ) return *line++;
		else line = NULL;
	}
}

#else

void console_init()
{}

void console_shutdown()
{}

void console_set_prompt( const char* prompt )
{
	printf( "%s", prompt );
}

char console_getchar()
{
	return getchar();
}

#endif
