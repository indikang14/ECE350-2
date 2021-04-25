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
#include "k_task.h"


/**
 * @brief: a dummy task1
 */
void utask1(void)
{
	//SER_PutStr(0, "utask1: entering \n\r");
	int temp = 0;
	char strbuff[50];
	sprintf(strbuff, "\tUTASK1: this is the global clock: %d\n\r", global_clk);
	SER_PutStr(0, strbuff);
	tsk_done_rt();
	/* do something
	long int x = 0;
	int i = 0;
	int j = 0;
	while (1) {
		SER_PutStr(0, "utask1: ");
		char out_char = 'a' + i % 10;
		for (j = 0; j < 5; j++)
		{
			SER_PutChar(0, out_char);
		}
		char strbuff[50];
		sprintf(strbuff, "this is the global clock: %d\n\r", global_clk);

		SER_PutStr(0, strbuff);
		++i;
		for (x = 0; x < 5000000; x++)
			; // some artifical delay
	}
	/* terminating */
	 //tsk_exit();
}

void utask2(void)
{

	int asdf = 0;
	SER_PutStr(0, "utask2: entering \n\r");
	/* do something */
	long int x = 0;
	int i = 0;
	int j = 0;
/*
	task_t tid;
	TASK_RT trt;
	TIMEVAL newpn;
	newpn.sec = 0;
	newpn.usec = 700000;
	trt.p_n = newpn;
	trt.rt_mbx_size = 512;
	trt.task_entry = &utask3;
	trt.u_stack_size = 0x200;
	tsk_create_rt(&tid,  &trt);
*/
	while (1)
	{
		SER_PutStr(0, "utask2: ");
		char out_char = 'A' + i % 10;
		for (j = 0; j < 5; j++) {

			SER_PutChar(0, out_char);


		}
		char mode = __get_mode();
		char tf = mode == MODE_USR ? 'u' : 'p';
		SER_PutChar(0, tf);

		char strbuff[50];
		sprintf(strbuff, "this is the global clock: %d\n\r", global_clk);

		SER_PutStr(0, strbuff);

		++i;
		TIMEVAL temptv;
		temptv.sec = 1;
		temptv.usec = 0;
		tsk_suspend(&temptv);

	}
	/* terminating */
	// tsk_exit();
}
/*
void utask3(void)
{
	//SER_PutStr(0, "utask1: entering \n\r");
	char strbuff[50];
	sprintf(strbuff, "\tUTASK3: this is the global clock: %d\n\r", global_clk);
	SER_PutStr(0, strbuff);
	tsk_done_rt();
}
*/
/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */