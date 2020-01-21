/*
 *  Copyright (c) 2015, Manuel Stuehn
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


#ifndef CMD_H
#define CMD_H

int is_breakpoint( unsigned int addr );

int cmd_print_breakpoints();
int cmd_set_breakpoint (unsigned int bpnum, unsigned int addr);
int cmd_clear_breakpoint (unsigned int bpnum);
int cmd_disable_breakpoint();
int cmd_d (int offset, int addr, int len);
int cmd_dis (int offset, int addr, int len);
void cmd_halt();
int cmd_loadprog(unsigned int addr, char *fn);
void cmd_setreg( int reg, unsigned int value );
void cmd_printregs();
void cmd_run();
void cmd_runb();
void cmd_runss();
void cmd_single_step();
void cmd_soft_reset();
void cmd_print_watch();
void cmd_clear_watch (unsigned int wanum);
void cmd_set_watch_any (unsigned int wanum, unsigned int addr);
void cmd_set_watch (unsigned int wanum, unsigned int addr, unsigned int value);

#endif // CMD_H
