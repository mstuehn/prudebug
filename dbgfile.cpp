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

#include "dbgfile.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>

#include <stdio.h>

#include <stdint.h>

#include <sys/ioctl.h>
#include <unistd.h>

extern "C" {
#include "prudbg.h"
#include "cmd.h"
}

#define TAB_SIZE 4

using namespace std;

static vector< string > dbgfile;
static map< uint16_t, size_t > linemap;
static map< size_t, uint16_t > addrmap;
static map< string, uint16_t > filemap;

void cmd_flushdbg()
{
    dbgfile.clear();
    linemap.clear();
    addrmap.clear();
    filemap.clear();
}

int cmd_loaddbg( const char* filename )
{
    ifstream file( filename );

    if( !file ) {
        printf( "ERROR: could not open file\n" );
        return 1;
    }

    cmd_flushdbg();


    unsigned int num_inst = 0;
    string srcfile_name = "";
    uint16_t srcfile_addr = 0;
    bool srcfile_valid = false;
    while( file ) {
        string line;
        getline( file, line );

        if( line.find( "Source File" ) != string::npos ) {
            string::size_type start = line.find( '\'', 0 );
            if( start != string::npos ) {
                string::size_type end = line.find( '\'', start + 1 );
                if( end != string::npos && end - start > 1 ) {
                    srcfile_name = line.substr( start + 1, end - start - 1 );
                    srcfile_valid = false;
                }
            }
        }

        string::size_type pos_hex = line.find( ':', 0 );
        if( pos_hex == string::npos ) continue;
        string::size_type pos_code = line.find( ':', pos_hex + 1 );
        if( pos_code == string::npos ) continue;

        string srcline = line.substr( 0, pos_hex - 1 );
        string bin = line.substr( pos_hex + 1, pos_code - pos_hex - 1 );
        string code = line.substr( pos_code + 2 );

        for(;;) {
            string::size_type pos = code.find( '\t' );
            if( pos == string::npos ) break;
            code.replace( pos, 1, string( TAB_SIZE - pos % TAB_SIZE, ' ' ) );
        }

        size_t lineno = dbgfile.size();
        dbgfile.push_back( code );

        uint16_t addr;
        uint32_t opcode;

        istringstream istr( bin );
        istr >> hex >> addr >> hex >> opcode;

        if( istr ) {
            linemap[addr] = lineno;
            addrmap[lineno] = addr;

            uint32_t* iram = (uint32_t*)&pru[ pru_inst_base[pru_num] + addr ];
            *iram = opcode;

            srcfile_addr = addr;
            if( srcfile_name.size() > 0 ) srcfile_valid = true;

            num_inst++;
        }

        if( srcfile_valid ) {
            int srclineno;

            istringstream istr( srcline );
            istr >> dec >> srclineno;

            if( istr ) filemap[ srcfile_name + ":" + to_string(srclineno) ] = srcfile_addr;
        }
    }

    printf( "Loaded %u instructions into PRU%u instruction RAM.\n", num_inst, pru_num );

    return 0;
}

int cmd_disdbg( int offset, int addr, int len )
{
    const unsigned int pc = pru[ pru_ctrl_base[pru_num] + PRU_STATUS_REG ] & 0xffff;

    uint16_t inst = (uint16_t)addr;
    if( addr == 0 && len == 0 ) {
        inst = (uint16_t)pru[pru_ctrl_base[pru_num] + PRU_STATUS_REG];
    }

    auto lineiter = linemap.find( inst );
    if( lineiter == linemap.end() ) return cmd_dis( offset, addr, len );

    size_t start = lineiter->second;
    if( addr == 0 && len == 0 ) {
        if( start >= 3 ) start -= 3;
        else start = 0;
        len = 16;
    }

    struct winsize ws;
    if( ioctl( STDOUT_FILENO, TIOCGWINSZ, &ws ) == -1 ) ws.ws_col = 80;
    if( ws.ws_col < 40 ) ws.ws_col = 40;

    for( size_t i = 0; i < len && start + i < dbgfile.size(); i++ ) {
        const string code = dbgfile[ start + i ].substr( 0, ws.ws_col - 16 );
        auto addriter = addrmap.find( start + i );

        if( addriter == addrmap.end() ) {
            printf( "                %s\n", code.c_str() );
        }
        else {
            int bp = is_breakpoint( addriter->second );
            printf( "[0x%04hx] %s%c %s %s\n",
                    addriter->second,
                    (bp >= 0) ? "BR" : "  ",
                    (bp >= 0) ? '0' + bp : ' ',
                    (addriter->second == pc) ? ">>" : "  ",
                    code.c_str() );
        }
    }

    return 0;
}

int cmd_set_breakpointdbg( unsigned int bpnum, const char* pos )
{
    auto fileiter = filemap.find( pos );
    if( fileiter == filemap.end() ) {
        printf( "breakpoint location not found\n" );
        return 0;
    }

    return cmd_set_breakpoint( bpnum, fileiter->second );
}
