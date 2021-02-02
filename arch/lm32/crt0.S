/****************************************************************************
**
**  Name: crt0ram.S
**
**  Description:
**        Implements boot-code that calls LatticeDDInit (that calls main())
**        Implements exception handlers (actually, redirectors)
**
**  $Revision: $
**
** Disclaimer:
**
**   This source code is intended as a design reference which
**   illustrates how these types of functions can be implemented.  It
**   is the user's responsibility to verify their design for
**   consistency and functionality through the use of formal
**   verification methods.  Lattice Semiconductor provides no warranty
**   regarding the use or functionality of this code.
**
** --------------------------------------------------------------------
**
**                     Lattice Semiconductor Corporation
**                     5555 NE Moore Court
**                     Hillsboro, OR 97214
**                     U.S.A
**
**                     TEL: 1-800-Lattice (USA and Canada)
**                          (503)268-8001 (other locations)
**
**                     web:   http://www.latticesemi.com
**                     email: techsupport@latticesemi.com
**
** --------------------------------------------------------------------------
**
**  Change History (Latest changes on top)
**
**  Ver    Date        Description
** --------------------------------------------------------------------------
**  3.8   Apr-15-2011  Added __MICO_USER_<handler>_HANDLER__ preprocessor to 
**                     allow customers to implement their own handlers for:
**                     DATA_ABORT, INST_ABORT
**  
**  3.1   Jun-18-2008  Added __MICO_NO_INTERRUPTS__ preprocessor
**                     option to exclude invoking MicoISRHandler
**                     to reduce code-size in apps that don't use
**                     interrupts
**
**  3.0   Mar-25-2008  Added Header
**
**---------------------------------------------------------------------------
*****************************************************************************/

/*
 * LatticeMico32 C startup code.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#define __ASSEMBLY__
#include "include/revision.h"
#include "ppsi/include/hw-specific/wrh.h"
#include "arch/lm32/crt0.h"
#include "include/generated/autoconf.h"
/* From include/sys/signal.h */  
#define SIGINT  2   /* interrupt */
#define SIGTRAP 5   /* trace trap */
#define SIGFPE  8   /* arithmetic exception */
#define SIGSEGV 11  /* segmentation violation */

//#define MICO32_FULL_CONTEXT_SAVE_RESTORE

/* Exception handlers - Must be 32 bytes long. */
        .section    .boot, "ax", @progbits
	
        .global	_start  
_start: 
	   
        .global _reset_handler
        .type 	_reset_handler, @function
_reset_handler:
_reset_handler:
_true_reset:
    nop
        .size	_reset_handler, .-_reset_handler

.org WRPC_MARK
/* Used by the dumping tool to identify byte ordering */	
.ascii	"WRPC----"
.int	0x01234567
.short	0x89ab, 0xcdef
/* Pointer to structures, for the dumping tool */
.org SOFTPLL_PADDR
	.word softpll
.org FIFO_LOG_PADDR
	.word fifo_log
.org PPG_STATIC_PADDR
	.word ppg_static
.org STATS_PADDR
	.word stats

.org UPTIME_SEC_ADDR
.global uptime_sec
uptime_sec:
	.word 0

.org VERSION_WRPC_ADDR
.global version_wrpc
version_wrpc:
	.byte WRPC_SHMEM_VERSION

.org VERSION_PPSI_ADDR
.global version_ppsi
version_ppsi:
	.byte WRS_PPSI_SHMEM_VERSION

/* Pointer to a structure used by testbenches, use only when
 * CONFIG_WR_NODE_SIM is set */
.org HDL_TESTBENCH_PADDR
.global hdl_testbench_p
hdl_testbench_p:
#ifdef CONFIG_WR_NODE_SIM
	.word hdl_testbench
#else
	.word 0
#endif
.extern _irq_entry
.org 0xc0
        .global _interrupt_handler
        .type 	_interrupt_handler, @function
_interrupt_handler:
    call   _save_all
#ifndef __MICO_NO_INTERRUPTS__
    call  _irq_entry
#else
#endif
    call      _restore_all_and_return
    nop
    nop
    nop

.org 0x100
        .global _crt0
        .type 	_crt0, @function
_crt0:
    /* Clear r0 */
    /* Setup stack and global pointer */
	
    call   memset
    call   main

loopf:
	call loopf

        .global _save_all
        .type 	_save_all, @function
_save_all:
#ifdef MICO32_FULL_CONTEXT_SAVE_RESTORE
#else
#endif
#ifdef MICO32_FULL_CONTEXT_SAVE_RESTORE
#else
#endif
//    xor     r1, r1, r1
//    wcsr    ie, r1
    ret
        .size  	_save_all, .-_save_all
	
        .global _restore_all_and_return
        .type 	_restore_all_and_return, @function
    /* Restore all registers and return from exception */
_restore_all_and_return:
//    addi    r1, r0, 2
//    wcsr    ie, r1
#ifdef MICO32_FULL_CONTEXT_SAVE_RESTORE
    /* Stack pointer must be restored last, in case it has been updated */
#else
    /* Stack pointer must be restored last, in case it has been updated */
#endif
    nop
        .size   _restore_all_and_return, .-_restore_all_and_return

