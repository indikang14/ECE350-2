/*
 ****************************************************************************
 *
 *                  UNIVERSITY OF WATERLOO ECE 350 RTOS LAB
 *
 *                     Copyright 2020-2021 Yiqing Huang
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

/**************************************************************************//**
 * @file        a usr_tasks.c
 * @brief       Two user/unprivileged  tasks: task1 and task2
 *
 * @version     V1.2021.01
 * @authors     Yiqing Huang
 * @date        2021 JAN
 *****************************************************************************/

#include "ae_usr_tasks.h"
#include "rtx.h"
#include "Serial.h"
#include "printf.h"


void* ownedMemory;
task_t *ownerTask;
/**
 * @brief: a dummy task1
 */
void task1(void)
{
    task_t tid;
    task_t tid3;
    RTX_TASK_INFO task_info;
    
    SER_PutStr ("task1: entering \n\r");

    tsk_create(&tid, &task2, LOW, 0x200);  /*create a user task */
    tsk_get(tid, &task_info);
    tsk_set_prio(tid, LOWEST);

    //create task for testing memory ownership
    ownedMemory = NULL;
    ownerTask = &tid3;
    tsk_create(&tid3, &task3, HIGH, 0x200);  /*create a user task */
    tsk_yield(); //should switch to task 3
    if (mem_dealloc(ownedMemory) == -1) {
    	SER_PutStr ("task1: SUCCESS - can't deallocate owned memory \n\r");
    } else {
    	SER_PutStr ("task1: ERROR - shouldn't be able to deallocate owned memory \n\r");
    }

    /* terminating */
    tsk_exit();
}

/**
 * @brief: a dummy task2
 */
void task2(void)
{
	SER_PutStr ("task2: entering \n\r");
    /* do something */
    /* terminating */
    tsk_exit();
}

void task3(void)
{
	SER_PutStr ("task2: entering \n\r");
	ownedMemory = mem_alloc(sizeof(U8) * 32);
	tsk_set_prio(*ownerTask, LOWEST);
	tsk_yield();
	if (mem_dealloc(ownedMemory) == 0) {
		SER_PutStr ("task2: memory deallocated successfully \n\r");
	} else {
		SER_PutStr ("task2: failure deallocating owned memory \n\r");
	}
	tsk_exit();
}


/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
