/*
 ****************************************************************************
 *
 *                  UNIVERSITY OF WATERLOO ECE 350 RTOS LAB
 *
 *                 Copyright 2020-2021 ECE 350 Teaching Team
 *                          All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice and the following disclaimer.
 *
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS AND CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 ****************************************************************************
 */

#ifndef AE_
#define AE_

#include "Serial.h"
#include "printf.h"
#include "rtx.h"
#include "ae_priv_tasks.h"
#include "ae_usr_tasks.h"



#define TEST 10
#define KCD_CASE 0

#if TEST == 0
	#define init_tasks 1
#endif

#if TEST == 1
	#if KCD_CASE == 0
		#define init_tasks 2
	#elif KCD_CASE == 1
		#define init_tasks 1
	#endif
#endif

#if TEST == 2
	#define init_tasks 1
#endif

#if TEST == 3
	#define init_tasks 1
#endif

#if TEST == 4
	#define init_tasks 2
#endif

#if TEST == 5
	#define init_tasks 2
#endif

#if TEST == 6
	#define init_tasks 1
#endif

#if TEST == 7
	#define init_tasks 3
#endif

#if TEST == 8
	#if KCD_CASE == 0
		#define init_tasks 2
	#elif KCD_CASE == 1
		#define init_tasks 1
	#endif
#endif

#if TEST == 9
	#if KCD_CASE == 0
		#define init_tasks 4
	#elif KCD_CASE == 1
		#define init_tasks 3
	#endif
#endif

#if TEST == 10
	#define init_tasks 2
#endif


/*
 *===========================================================================
 *                            FUNCTION PROTOTYPES
 *===========================================================================
 */

int  ae_init          (RTX_SYS_INFO *sys_info, \
                       RTX_TASK_INFO *task_info, int num_tasks);
int  ae_set_sys_info  (RTX_SYS_INFO *sys_info);
void ae_set_task_info (RTX_TASK_INFO *tasks, int num_tasks);
int  ae_start(void);

int  test_mem(void);

#endif // ! AE_
/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */

