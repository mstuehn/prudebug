/*
 *
 *  PRU Debug Program
 *  (c) Copyright 2011, 2013 by Arctica Technologies
 *  Written by Steven Anderson
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <inttypes.h>

#include "prudbg.h"
#include "cmd.h"
#include "dbgfile.h"

int is_breakpoint( unsigned int addr )
{
   int i;

   for (i=0; i<MAX_BREAKPOINTS; i++) {
        if( bp[pru_num][i].state == BP_ACTIVE
         && bp[pru_num][i].address == addr )
        {
            return i;
        }
    }
   return -1;
}

// breakpoint management
int cmd_print_breakpoints()
{
    int i;

    printf("##  Address\n");
    for (i=0; i<MAX_BREAKPOINTS; i++) {
        if (bp[pru_num][i].state == BP_ACTIVE) {
            printf("%02u  0x%04x\n", i, bp[pru_num][i].address);
        } else {
            printf("%02u  UNUSED\n", i);
        }
    }
    printf("\n");
    return 0;
}

// set breakpoint
int cmd_set_breakpoint (unsigned int bpnum, unsigned int addr)
{
    cmd_disable_breakpoint( bpnum );

    bp[pru_num][bpnum].state = BP_ACTIVE;
    bp[pru_num][bpnum].address = addr;
    bp[pru_num][bpnum].opcode = pru[pru_inst_base[pru_num] + addr];

    return 0;
}

// clear breakpoint
int cmd_clear_breakpoint (unsigned int bpnum)
{
    cmd_disable_breakpoint( bpnum );

    bp[pru_num][bpnum].state = BP_UNUSED;

    return 0;
}

// enable breakpoint
int cmd_enable_breakpoint (unsigned int bpnum)
{
    if( bp[pru_num][bpnum].state != BP_ACTIVE || bp[pru_num][bpnum].enabled ) return 0;

    bp[pru_num][bpnum].enabled = 1;
    pru[pru_inst_base[pru_num] + bp[pru_num][bpnum].address] = 0x2a000000;

    return 0;
}

// disable breakpoint
int cmd_disable_breakpoint (unsigned int bpnum)
{
    if( !bp[pru_num][bpnum].enabled ) return 0;

    bp[pru_num][bpnum].enabled = 0;
    pru[pru_inst_base[pru_num] + bp[pru_num][bpnum].address] = bp[pru_num][bpnum].opcode;

    return 0;
}

// dump data memory
int cmd_d (int offset, int addr, int len)
{
    int i, j;

    for (i=0; i<len; ) {
        printf ("[0x%04x] ", addr+i);
        for (j=0;(i<len)&&(j<4); i++,j++) printf ("0x%08x ", pru[offset+addr+i]);
        printf ("\n");
    }
    printf("\n");
    return 0;
}

// disassemble instruction memory
int cmd_dis (int offset, int addr, int len)
{
    int i, j;
    char inst_str[50];
    unsigned int status_reg, ctrl_reg;
    char *pc[] = {"  ", ">>"};
    char *brkp[] = {"  ", "BR"};
    int pc_on = 0;
    int bp_on = 0;
    unsigned int inst;
    int bp_idx;

    ctrl_reg = pru[pru_ctrl_base[pru_num] + PRU_CTRL_REG];
    if( (ctrl_reg & PRU_REG_RUNSTATE) != 0 ) {
        printf("Instruction ram not accessible since PRU is RUNNING.\n");
        return 0;
    }

    if( addr == 0 && len == 0 ) {
        addr = pru[pru_ctrl_base[pru_num] + PRU_STATUS_REG] - 3;
        len = 16;
        if( addr + len >  MAX_PRU_MEM ) len = MAX_PRU_MEM - addr;
        else if( addr < 0 ) {
            len += addr;
            addr = 0;
        }
    }

    status_reg = (pru[pru_ctrl_base[pru_num] + PRU_STATUS_REG]) & 0xFFFF;

    for( i = 0; i < len; i++ )
    {
        if( status_reg == (addr + i) ) pc_on = 1; else pc_on = 0;

        bp_idx = is_breakpoint( addr + i );
        if( bp_idx < 0 )
        {
            bp_on = 0;
            inst = pru[offset+addr+i];
        }
        else
        {
            bp_on = 1;
            inst = bp[pru_num][bp_idx].opcode;
        }

        disassemble(inst_str, inst);
        printf ("[0x%04x] 0x%08x %s%c %s %s\n", addr+i, inst, brkp[bp_on], bp_idx < 0 ? ' ' : (char)('0' + bp_idx), pc[pc_on], inst_str);
    }
    printf("\n");
    return 0;
}

// halt the current PRU
void cmd_halt()
{
    unsigned int    ctrl_reg;

    ctrl_reg = pru[pru_ctrl_base[pru_num] + PRU_CTRL_REG];
    ctrl_reg &= ~PRU_REG_PROC_EN;
    pru[pru_ctrl_base[pru_num] + PRU_CTRL_REG] = ctrl_reg;
    printf("PRU%u Halted.\n", pru_num);
}

// load program into instruction memory
int cmd_loadprog(unsigned int addr, char *fn)
{
    int f, r;
    struct stat file_info;

    cmd_flushdbg();

    r = stat(fn, &file_info);
    if (r == -1) {
        printf("ERROR: could not open file\n");
        return 1;
    }
    if (((file_info.st_size/4)*4) != file_info.st_size) {
        printf("ERROR: file size is not evenly divisible by 4\n");
    } else {
        f = open(fn, O_RDONLY);
        if (f == -1) {
            printf("ERROR: could not open file 2\n");
        } else {
            read(f, (void*)&pru[pru_inst_base[pru_num] + addr], file_info.st_size);
            close(f);
#if defined( __FreeBSD__ )
            printf("Binary file of size %" PRId64 " bytes loaded into PRU%u instruction RAM.\n", file_info.st_size, pru_num);
#else
            printf("Binary file of size %lu bytes loaded into PRU%u instruction RAM.\n", file_info.st_size, pru_num);
#endif
        }
    }
    return 0;
}

// set a register
void cmd_setreg( int reg, unsigned int value )
{
    unsigned int ctrl_reg;

    if( reg < 0 || reg > 31 ) {
    	printf( "ERROR: illegal register name\n" );
    	return;
    }

    ctrl_reg = pru[pru_ctrl_base[pru_num] + PRU_CTRL_REG];
    if ( (ctrl_reg & PRU_REG_RUNSTATE) != 0 ) {
        printf( "    Rxx registers not available since PRU is RUNNING.\n" );
        return;
    }

    pru[ pru_ctrl_base[pru_num] + PRU_INTGPR_REG + reg ] = value;
}

// print current PRU registers
void cmd_printregs()
{
    unsigned int ctrl_reg, reset_pc, status_reg;
    char *run_state, *single_step, *cycle_cnt_en, *pru_sleep, *proc_en;
    unsigned int i, addr, inst;
    int bp_idx;
    char inst_str[80];

    ctrl_reg = pru[pru_ctrl_base[pru_num] + PRU_CTRL_REG];
    status_reg = pru[pru_ctrl_base[pru_num] + PRU_STATUS_REG];
    reset_pc = (ctrl_reg >> 16);
    if (ctrl_reg&PRU_REG_RUNSTATE)
        run_state = "RUNNING";
    else
        run_state = "STOPPED";

    if (ctrl_reg&PRU_REG_SINGLE_STEP)
        single_step = "SINGLE_STEP";
    else
        single_step = "FREE_RUN";

    if (ctrl_reg&PRU_REG_COUNT_EN)
        cycle_cnt_en = "COUNTER_ENABLED";
    else
        cycle_cnt_en = "COUNTER_DISABLED";

    if (ctrl_reg&PRU_REG_SLEEPING)
        pru_sleep = "SLEEPING";
    else
        pru_sleep = "NOT_SLEEPING";

    if (ctrl_reg&PRU_REG_PROC_EN)
        proc_en = "PROC_ENABLED";
    else
        proc_en = "PROC_DISABLED";

    printf("Register info for PRU%u\n", pru_num);
    printf("    Control register: 0x%08x\n", ctrl_reg);
    printf("      Reset PC:0x%04x  %s, %s, %s, %s, %s\n\n", reset_pc, run_state, single_step, cycle_cnt_en, pru_sleep, proc_en);

    addr = pru_inst_base[pru_num] + (status_reg&0xFFFF);
    bp_idx = is_breakpoint( addr );
    inst = ( bp_idx < 0) ? pru[addr] : bp[pru_num][bp_idx].opcode;

    disassemble(inst_str, inst);
    printf("    Program counter: 0x%04x\n", (status_reg&0xFFFF));
    printf("      Current instruction: %s\n\n", inst_str);

    if (ctrl_reg&PRU_REG_RUNSTATE) {
        printf("    Rxx registers not available since PRU is RUNNING.\n");
    } else {
        for (i=0; i<8; i++) printf("    R%02u: 0x%08x    R%02u: 0x%08x    R%02u: 0x%08x    R%02u: 0x%08x\n", i, pru[pru_ctrl_base[pru_num] + PRU_INTGPR_REG + i], i+8, pru[pru_ctrl_base[pru_num] + PRU_INTGPR_REG + i + 8], i+16, pru[pru_ctrl_base[pru_num] + PRU_INTGPR_REG + i + 16], i+24, pru[pru_ctrl_base[pru_num] + PRU_INTGPR_REG + i + 24]);
    }

    printf("\n");
}

// start PRU running
void cmd_run()
{
    unsigned int ctrl_reg;

    printf("Running in background\n");

    // start pru
    ctrl_reg = pru[pru_ctrl_base[pru_num] + PRU_CTRL_REG];
    ctrl_reg |= PRU_REG_PROC_EN;
    ctrl_reg &= ~PRU_REG_SINGLE_STEP;
    pru[pru_ctrl_base[pru_num] + PRU_CTRL_REG] = ctrl_reg;
}

// start PRU running until breakpoint is hit
void cmd_runb()
{
    unsigned int ctrl_reg, addr, offset;
    int i;

    printf("Running (will run until a breakpoint is hit or CTRL-C is pressed)....\n");

    addr = pru[pru_ctrl_base[pru_num] + PRU_STATUS_REG] & 0xFFFF;

    // single step when PC is at breakpoint
    if( is_breakpoint( addr ) >= 0 ) {
        ctrl_reg = pru[pru_ctrl_base[pru_num] + PRU_CTRL_REG];
        ctrl_reg |= PRU_REG_PROC_EN | PRU_REG_SINGLE_STEP;
        pru[pru_ctrl_base[pru_num] + PRU_CTRL_REG] = ctrl_reg;

        do usleep( 1 );
        while( (pru[pru_ctrl_base[pru_num] + PRU_CTRL_REG] & PRU_REG_RUNSTATE) != 0 );
    }

    // enable breakpoints
    for( i = 0; i < MAX_BREAKPOINTS; i++ ) cmd_enable_breakpoint(i);

    // start pru
    ctrl_reg = pru[pru_ctrl_base[pru_num] + PRU_CTRL_REG];
    ctrl_reg |= PRU_REG_PROC_EN;
    ctrl_reg &= ~PRU_REG_SINGLE_STEP;
    pru[pru_ctrl_base[pru_num] + PRU_CTRL_REG] = ctrl_reg;

    // wait until pru halts
    sigint_stoppru = 1;
    do usleep( 50000 );
    while( (pru[pru_ctrl_base[pru_num] + PRU_CTRL_REG] & PRU_REG_RUNSTATE) != 0 );
    sigint_stoppru = 0;

    // disable breakpoints
    for( i = 0; i < MAX_BREAKPOINTS; i++ ) cmd_disable_breakpoint(i);

    // print the registers
    cmd_printregs();

    // print disassembly
    offset = pru_inst_base[pru_num];
    cmd_disdbg( offset, 0, 0 );
}

// run PRU in a single stepping mode - used for watch variables
void cmd_runss()
{
    unsigned int i, addr, offset;
    unsigned int done = 0;
    unsigned int ctrl_reg;
    unsigned long t_cyc = 0;

    printf("Running (will run until a breakpoint or watchpoint is hit or CTRL-C is pressed)....\n");

    // enter single-step loop
    sigint_sent = 0;
    do {
        // set single step mode and enable processor
        ctrl_reg = pru[pru_ctrl_base[pru_num] + PRU_CTRL_REG];
        ctrl_reg |= PRU_REG_PROC_EN | PRU_REG_SINGLE_STEP;
        pru[pru_ctrl_base[pru_num] + PRU_CTRL_REG] = ctrl_reg;

        do usleep( 1 );
        while( (pru[pru_ctrl_base[pru_num] + PRU_CTRL_REG] & PRU_REG_RUNSTATE) != 0 );

        // check if we've hit a breakpoint
        addr = pru[pru_ctrl_base[pru_num] + PRU_STATUS_REG] & 0xFFFF;
        for (i=0; i<MAX_BREAKPOINTS; i++) if ((bp[pru_num][i].state == BP_ACTIVE) && (bp[pru_num][i].address == addr)) done = 1;

        // check if we've hit a watch point
        // addr = pru[pru_ctrl_base[pru_num] + PRU_STATUS_REG] & 0xFFFF;
        for (i=0; i<MAX_WATCH; i++) {
            if ((wa[pru_num][i].state == WA_PRINT_ON_ANY) && (wa[pru_num][i].old_value != pru[pru_data_base[pru_num] + wa[pru_num][i].address])) {
                printf("[0x%04x]  0x%04x  t=%lu\n", wa[pru_num][i].address, pru[pru_data_base[pru_num] + wa[pru_num][i].address], t_cyc);
                wa[pru_num][i].old_value = pru[pru_data_base[pru_num] + wa[pru_num][i].address];
            } else if ((wa[pru_num][i].state == WA_HALT_ON_VALUE) && (wa[pru_num][i].value == pru[pru_data_base[pru_num] + wa[pru_num][i].address])) {
                printf("[0x%04x]  0x%04x  t=%lu\n", wa[pru_num][i].address, pru[pru_data_base[pru_num] + wa[pru_num][i].address], t_cyc);
                done = 1;
            }
        }

        // check if we are on a HALT instruction - if so, stop single step execution
        if (pru[pru_inst_base[pru_num] + addr] == 0x2a000000) {
            printf("\nHALT instruction hit.\n");
            done = 1;
        }

        // increase time
        t_cyc++;

    } while ((!done) && (!sigint_sent));

    // print the registers
    cmd_printregs();

    // print disassembly
    offset = pru_inst_base[pru_num];
    cmd_disdbg( offset, 0, 0 );
}

void cmd_single_step()
{
    unsigned int		ctrl_reg;
    unsigned int        offset;
    int                 addr, len, i;

    // set single step mode and enable processor
    ctrl_reg = pru[pru_ctrl_base[pru_num] + PRU_CTRL_REG];
    ctrl_reg |= PRU_REG_PROC_EN | PRU_REG_SINGLE_STEP;
    pru[pru_ctrl_base[pru_num] + PRU_CTRL_REG] = ctrl_reg;

    do usleep( 1 );
    while( (pru[pru_ctrl_base[pru_num] + PRU_CTRL_REG] & PRU_REG_RUNSTATE) != 0 );

    // print the registers
    cmd_printregs();

    // print disassembly
    offset = pru_inst_base[pru_num];
    cmd_disdbg( offset, 0, 0 );

    // disable single step mode and disable processor
    ctrl_reg = pru[pru_ctrl_base[pru_num] + PRU_CTRL_REG];
    ctrl_reg &= ~PRU_REG_PROC_EN;
    ctrl_reg &= ~PRU_REG_SINGLE_STEP;
    pru[pru_ctrl_base[pru_num] + PRU_CTRL_REG] = ctrl_reg;
}

void cmd_soft_reset()
{
    unsigned int ctrl_reg;

    ctrl_reg = pru[pru_ctrl_base[pru_num] + PRU_CTRL_REG];
    ctrl_reg &= ~PRU_REG_SOFT_RESET;
    pru[pru_ctrl_base[pru_num] + PRU_CTRL_REG] = ctrl_reg;

    printf("PRU%u reset.\n", pru_num);
}

// print list of watches
void cmd_print_watch()
{
    int i;

    printf("##  Address  Value\n");
    for (i=0; i<MAX_WATCH; i++) {
        if (wa[pru_num][i].state == WA_PRINT_ON_ANY) {
            printf("%02u  0x%04x     Print on any\n", i, wa[pru_num][i].address);
        } else if (wa[pru_num][i].state == WA_HALT_ON_VALUE) {
            printf("%02u  0x%04x     Halt = 0x%04x\n", i, wa[pru_num][i].address, wa[pru_num][i].value);
        } else {
            printf("%02u  UNUSED\n", i);
        }
    }
    printf("\n");
}

// clear a watch from list
void cmd_clear_watch (unsigned int wanum)
{
    wa[pru_num][wanum].state = WA_UNUSED;
}

// set a watch for any change in value and no halt
void cmd_set_watch_any (unsigned int wanum, unsigned int addr)
{
    wa[pru_num][wanum].state = WA_PRINT_ON_ANY;
    wa[pru_num][wanum].address = addr;
    wa[pru_num][wanum].old_value = pru[pru_data_base[pru_num] + addr];
}

// set a watch for a specific value and halt
void cmd_set_watch (unsigned int wanum, unsigned int addr, unsigned int value)
{
    wa[pru_num][wanum].state = WA_HALT_ON_VALUE;
    wa[pru_num][wanum].address = addr;
    wa[pru_num][wanum].value = value;
}


