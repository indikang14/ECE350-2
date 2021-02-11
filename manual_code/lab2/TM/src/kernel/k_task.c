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
 * @file        k_task.c
 * @brief       task management C file
 *              l2
 * @version     V1.2021.01
 * @authors     Yiqing Huang
 * @date        2021 JAN
 *
 * @attention   assumes NO HARDWARE INTERRUPTS
 * @details     The starter code shows one way of implementing context switching.
 *              The code only has minimal sanity check.
 *              There is no stack overflow check.
 *              The implementation assumes only two simple privileged task and
 *              NO HARDWARE INTERRUPTS.
 *              The purpose is to show how context switch could be done
 *              under stated assumptions.
 *              These assumptions are not true in the required RTX Project!!!
 *              Understand the assumptions and the limitations of the code before
 *              using the code piece in your own project!!!
 *
 *****************************************************************************/

//#include "VE_A9_MP.h"
#include "Serial.h"
#include "k_task.h"
#include "k_rtx.h"

#ifdef DEBUG_0
#include "printf.h"
#endif /* DEBUG_0 */

/*
 *==========================================================================
 *                            GLOBAL VARIABLES
 *==========================================================================
 */

TCB             *gp_current_task = NULL;	// the current RUNNING task
TCB             g_tcbs[MAX_TASKS];			// an array of TCBs
RTX_TASK_INFO   g_null_task_info;			// The null task info
U32             g_num_active_tasks = 0;		// number of non-dormant tasks
TCB 			*TCBhead;						//points to starting TCB not NULL

U8				memBlockOwner = 0; //TODO: needs to be initialized when gp_current_task is first set

/*---------------------------------------------------------------------------
The memory map of the OS image may look like the following:

                       RAM_END+---------------------------+ High Address
                              |                           |
                              |                           |
                              |    Free memory space      |
                              |   (user space stacks      |
                              |         + heap            |
                              |                           |
                              |                           |
                              |                           |
 &Image$$ZI_DATA$$ZI$$Limit-->|---------------------------|-----+-----
                              |         ......            |     ^
                              |---------------------------|     |
                              |      PROC_STACK_SIZE      |     |
             g_p_stacks[15]-->|---------------------------|     |
                              |                           |     |
                              |  other kernel proc stacks |     |
                              |---------------------------|     |
                              |      PROC_STACK_SIZE      |  OS Image
              g_p_stacks[2]-->|---------------------------|     |
                              |      PROC_STACK_SIZE      |     |
              g_p_stacks[1]-->|---------------------------|     |
                              |      PROC_STACK_SIZE      |     |
              g_p_stacks[0]-->|---------------------------|     |
                              |   other  global vars      |     |
                              |                           |  OS Image
                              |---------------------------|     |
                              |      KERN_STACK_SIZE      |     |                
             g_k_stacks[15]-->|---------------------------|     |
                              |                           |     |
                              |     other kernel stacks   |     |                              
                              |---------------------------|     |
                              |      KERN_STACK_SIZE      |  OS Image
              g_k_stacks[2]-->|---------------------------|     |
                              |      KERN_STACK_SIZE      |     |                      
              g_k_stacks[1]-->|---------------------------|     |
                              |      KERN_STACK_SIZE      |     |
              g_k_stacks[0]-->|---------------------------|     |
                              |   other  global vars      |     |
                              |---------------------------|     |
                              |        TCBs               |  OS Image
                      g_tcbs->|---------------------------|     |
                              |        global vars        |     |
                              |---------------------------|     |
                              |                           |     |          
                              |                           |     |
                              |                           |     |
                              |                           |     V
                     RAM_START+---------------------------+ Low Address
    
---------------------------------------------------------------------------*/ 

/*
 *===========================================================================
 *                            FUNCTIONS
 *===========================================================================
 */

/**************************************************************************//**
 * @brief   scheduler, pick the TCB of the next to run task
 *
 * @return  TCB pointer of the next to run task
 * @post    gp_curret_task is updated
 *
 *****************************************************************************/

TCB *scheduler(void)
{
    task_t tid = gp_current_task->tid;
    printf("current task tid to run: %d ",gp_current_task->tid );
    return &g_tcbs[(++tid)%g_num_active_tasks];

}



/**************************************************************************//**
 * @brief       initialize all boot-time tasks in the system,
 *
 *
 * @return      RTX_OK on success; RTX_ERR on failure
 * @param       task_info   boot-time task information structure pointer
 * @param       num_tasks   boot-time number of tasks
 * @pre         memory has been properly initialized
 * @post        none
 *
 * @see         k_tsk_create_new
 *****************************************************************************/

int k_tsk_init(RTX_TASK_INFO *task_info, int num_tasks)
{
    extern U32 SVC_RESTORE;

    RTX_TASK_INFO *p_taskinfo = &g_null_task_info;
    g_num_active_tasks = 0;



    if (num_tasks >= MAX_TASKS) {
    	return RTX_ERR;
    }

    // create the first task
    TCB *p_tcb = &g_tcbs[0];
    p_tcb->prio     = PRIO_NULL;
    p_tcb->priv     = 1;
    p_tcb->tid      = TID_NULL;
    p_tcb->state    = RUNNING;
    p_tcb->next 	= NULL; //initialize head of TCBs
    TCBhead 		= NULL;
    g_num_active_tasks++;
    gp_current_task = p_tcb;
    memBlockOwner = gp_current_task->tid;
    TCB* oldTCB = p_tcb;
    TCB* newTCB;

    // create the rest of the tasks
    p_taskinfo = task_info;
    for ( int i = 0; i < num_tasks; i++ ) {
    	printf("address of oldTCB is: 0x%x \r\n ", oldTCB);
    	printf("address of new TCB is: 0x%x \r\n ", newTCB);

        newTCB = &g_tcbs[i+1];
        printf("address of next pTcb is: 0x%x \r\n ", newTCB);

        oldTCB->next = newTCB; //join the linked list of TCBs LL only has active tasks

        printf("address of next(next pTcb) is: 0x%x \r\n ", oldTCB -> next);
        printf("address of head next is: 0x%x \r\n ", TCBhead -> next);

        newTCB->next = NULL; //initialize next pointer  of current task
        newTCB->TcbInfo = p_taskinfo; // point to TCB's taskinfo in TCB struct

        if(i + 1 == 1) {
        	printf("address of first non null task: 0x%x \r\n", newTCB );
            	TCBhead =  newTCB; // there was another task other than NULL, make that head
            	printf("address of head next is: 0x%x \r\n ", TCBhead -> next);
          }

        if (k_tsk_create_new(newTCB->TcbInfo, newTCB , i+1) == RTX_OK) { // use RTXInfo pointer from TCB struct as parameter
        	g_num_active_tasks++;
        }
        printf("tid of current tcb is: %d \r\n", newTCB->tid);
        printf("address of current pTcb is: 0x%x \r\n ", newTCB);
        printf("address of previous pTcb is: 0x%x \r\n ", oldTCB);
        p_taskinfo++;
        printf("address of current pTcb is: 0x%x \r\n ", newTCB);
        oldTCB = newTCB; // end of loop current TCB becomes old TCB

    }
    return RTX_OK;
}
/**************************************************************************//**
 * @brief       initialize a new task in the system,
 *              one dummy kernel stack frame, one dummy user stack frame
 *
 * @return      RTX_OK on success; RTX_ERR on failure
 * @param       p_taskinfo  task information structure pointer
 * @param       p_tcb       the tcb the task is assigned to
 * @param       tid         the tid the task is assigned to
 *
 * @details     From bottom of the stack,
 *              we have user initial context (xPSR, PC, SP_USR, uR0-uR12)
 *              then we stack up the kernel initial context (kLR, kR0-kR12)
 *              The PC is the entry point of the user task
 *              The kLR is set to SVC_RESTORE
 *              30 registers in total
 *
 *****************************************************************************/
int k_tsk_create_new(RTX_TASK_INFO *p_taskinfo, TCB *p_tcb, task_t tid)
{
    extern U32 SVC_RESTORE;
    extern U32 K_RESTORE;

    U32 *sp;

    if (p_taskinfo == NULL || p_tcb == NULL)
    {
        return RTX_ERR;
    }

    //initialize the taskInfo params
    p_tcb ->tid = tid;
    p_tcb->state = READY;
    p_taskinfo->prio = p_tcb->prio; // setting prio for initialization
    p_taskinfo->priv = p_tcb->priv;
    p_taskinfo->tid = tid;


    /*---------------------------------------------------------------
     *  Step1: allocate kernel stack for the task
     *         stacks grows down, stack base is at the high address
     * -------------------------------------------------------------*/

    ///////sp = g_k_stacks[tid] + (KERN_STACK_SIZE >> 2) ;
    sp = k_alloc_k_stack(tid);

    // 8B stack alignment adjustment
    if ((U32)sp & 0x04) {   // if sp not 8B aligned, then it must be 4B aligned
        sp--;               // adjust it to 8B aligned
    }

    /*-------------------------------------------------------------------
     *  Step2: create task's user/sys mode initial context on the kernel stack.
     *         fabricate the stack so that the stack looks like that
     *         task executed and entered kernel from the SVC handler
     *         hence had the user/sys mode context saved on the kernel stack.
     *         This fabrication allows the task to return
     *         to SVC_Handler before its execution.
     *
     *         16 registers listed in push order
     *         <xPSR, PC, uSP, uR12, uR11, ...., uR0>
     * -------------------------------------------------------------*/

    // if kernel task runs under SVC mode, then no need to create user context stack frame for SVC handler entering
    // since we never enter from SVC handler in this case
    // uSP: initial user stack
    if ( p_taskinfo->priv == 0 ) { // unprivileged task
        // xPSR: Initial Processor State
        *(--sp) = INIT_CPSR_USER;
        // PC contains the entry point of the user/privileged task
        *(--sp) = (U32) (p_taskinfo->ptask);

        //********************************************************************//
        //*** allocate user stack from the user space, not implemented yet ***//
        //********************************************************************//
        *(--sp) = (U32) k_alloc_p_stack(p_taskinfo);

        // uR12, uR11, ..., uR0
        for ( int j = 0; j < 13; j++ ) {
            *(--sp) = 0x0;
        }
    }


    /*---------------------------------------------------------------
     *  Step3: create task kernel initial context on kernel stack
     *
     *         14 registers listed in push order
     *         <kLR, kR0-kR12>
     * -------------------------------------------------------------*/
    if ( p_taskinfo->priv == 0 ) {
        // user thread LR: return to the SVC handler
        *(--sp) = (U32) (&SVC_RESTORE);
    } else {
        // kernel thread LR: return to the entry point of the task
        *(--sp) = (U32) (p_taskinfo->ptask);
    }

    // kernel stack R0 - R12, 13 registers
    for ( int j = 0; j < 13; j++) {
        *(--sp) = 0x0;
    }

    p_tcb->msp = sp;

    return RTX_OK;
}

/**************************************************************************//**
 * @brief       switching kernel stacks of two TCBs
 * @param:      p_tcb_old, the old tcb that was in RUNNING
 * @return:     RTX_OK upon success
 *              RTX_ERR upon failure
 * @pre:        gp_current_task is pointing to a valid TCB
 *              gp_current_task->state = RUNNING
 *              gp_crrent_task != p_tcb_old
 *              p_tcb_old == NULL or p_tcb_old->state updated
 * @note:       caller must ensure the pre-conditions are met before calling.
 *              the function does not check the pre-condition!
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * @attention   CRITICAL SECTION
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *
 *****************************************************************************/
__asm void k_tsk_switch(TCB *p_tcb_old)
{
        PUSH    {R0-R12, LR}
        STR     SP, [R0, #TCB_MSP_OFFSET]   ; save SP to p_old_tcb->msp
K_RESTORE
        LDR     R1, =__cpp(&gp_current_task);
        LDR     R2, [R1]
        LDR     SP, [R2, #TCB_MSP_OFFSET]   ; restore msp of the gp_current_task

        POP     {R0-R12, PC}
}


/**************************************************************************//**
 * @brief       run a new thread. The caller becomes READY and
 *              the scheduler picks the next ready to run task.
 * @return      RTX_ERR on error and zero on success
 * @pre         gp_current_task != NULL && gp_current_task == RUNNING
 * @post        gp_current_task gets updated to next to run task
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * @attention   CRITICAL SECTION
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *****************************************************************************/
int k_tsk_run_new(void)
{
    TCB *p_tcb_old = NULL;
    
    if (gp_current_task == NULL) {
    	return RTX_ERR;
    }

    p_tcb_old = gp_current_task;
    gp_current_task = scheduler();
    
    if ( gp_current_task == NULL  ) {
        gp_current_task = p_tcb_old;        // revert back to the old task
        memBlockOwner = gp_current_task->tid;
        return RTX_ERR;
    }
    printf("address of current task: 0x%x \r\n",gp_current_task );
    // at this point, gp_current_task != NULL and p_tcb_old != NULL
    if (gp_current_task != p_tcb_old) {
        gp_current_task->state = RUNNING;   // change state of the to-be-switched-in  tcb
        p_tcb_old->state = READY;           // change state of the to-be-switched-out tcb
        k_tsk_switch(p_tcb_old);            // switch stacks
    }

    memBlockOwner = gp_current_task->tid;

    return RTX_OK;
}

/**************************************************************************//**
 * @brief       yield the cpu
 * @return:     RTX_OK upon success
 *              RTX_ERR upon failure
 * @pre:        gp_current_task != NULL &&
 *              gp_current_task->state = RUNNING
 * @post        gp_current_task gets updated to next to run task
 * @note:       caller must ensure the pre-conditions before calling.
 *****************************************************************************/
int k_tsk_yield(void)
{
    return k_tsk_run_new();
}


/*
 *===========================================================================
 *                             TO BE IMPLEMETED IN LAB2
 *===========================================================================
 */

int k_tsk_create(task_t *task, void (*task_entry)(void), U8 prio, U16 stack_size)
{

#ifdef DEBUG_0
    printf("k_tsk_create: entering...\n\r");
    printf("task = 0x%x, task_entry = 0x% /x, prio=%d, stack_size = %d\n\r", task, task_entry, prio, stack_size);
#endif /* DEBUG_0 */

	//initialize
	//RTX_TASK_INFO* newTaskInfo;
	TCB* newTaskBlock;
	int taskFound = 0;//flag to indicate TCB created

	TCB* traverse;

	//Linked List houses all the active TCBS
	for(traverse = TCBhead; taskFound == 0  ; traverse = traverse->next ) {

		printf("address of traverse 0x%x \r\n", traverse);
		printf("traverse next Tid is: %d \r\n",traverse->next->tid );
		printf("traverse Tid is: %d \r\n",traverse->tid );

		//if you find an unused TCB at the end of the linked  list
			if (traverse->next == NULL) {
				printf("free space at end of list!");
				*task = traverse->tid +1;
				printf("task(tid): %d", *task);
				newTaskBlock =  &g_tcbs[*task];
				printf("address of new TCB is: 0x%x \r\n ", newTaskBlock);
				traverse->next = newTaskBlock;
				newTaskBlock->next = NULL;
				taskFound = 1;

			}

		//if you find a dormant or unused TCB in between two active TCBs...
			else if((traverse->next->tid - traverse->tid) > 1) {
				printf("there is space between two TCBs!");

				*task =   traverse->tid +1;
				printf("task(tid): %d", *task);
				newTaskBlock = &g_tcbs[*task];
				printf("address of new TCB is: 0x%x \r\n ", newTaskBlock);
				newTaskBlock->next = traverse->next;
				traverse->next = newTaskBlock;
				taskFound = 1;
			}

	}

// initialize TCB structure
	//newTaskBlock->prio = prio;
	//newTaskBlock->state = READY;
	newTaskBlock->priv = 0;
	newTaskBlock->tid = (U8) *task;
	newTaskBlock->TcbInfo->tid = *task;
	newTaskBlock->TcbInfo->u_stack_size = stack_size;

	if(k_tsk_create_new(newTaskBlock->TcbInfo,newTaskBlock, newTaskBlock->TcbInfo->tid ) != RTX_OK) {
		return RTX_ERR;
	}




    return RTX_OK;

}

void k_tsk_exit(void)
{
#ifdef DEBUG_0
    printf("k_tsk_exit: entering...\n\r");
#endif /* DEBUG_0 */
    TCB *p_tcb_old = NULL;

    if (gp_current_task == NULL) {
    	return;
    }

    p_tcb_old = gp_current_task;
    gp_current_task = scheduler();

    if ( gp_current_task == NULL  ) {
    	gp_current_task = p_tcb_old;        // revert back to the old task
    	memBlockOwner = gp_current_task->tid;
    	return;
    }

    // at this point, gp_current_task != NULL and p_tcb_old != NULL
    if (gp_current_task != p_tcb_old) {
    	gp_current_task->state = RUNNING;   // change state of the to-be-switched-in  tcb
        p_tcb_old->state = DORMANT;           // change state of the to-be-switched-out tcb
        k_tsk_switch(p_tcb_old);            // switch stacks
    }
    memBlockOwner = gp_current_task->tid;

    // remove from linked list
    if (TCBhead->tid == p_tcb_old->tid) {
    	TCBhead = TCBhead->next;
        return;
    }

    TCB* prev = TCBhead;
    // find task
    while (prev->next != NULL || prev->next->tid != p_tcb_old->tid) {
    	prev = prev->next;
    }
    if (prev->next == NULL) {
    	return;
    }

    // found the thing
    prev->next = prev->next->next;

    // still need to dealloc

    return;
}

int k_tsk_set_prio(task_t task_id, U8 prio)
{
#ifdef DEBUG_0
    printf("k_tsk_set_prio: entering...\n\r");
    printf("task_id = %d, prio = %d.\n\r", task_id, prio);
#endif /* DEBUG_0 */
    TCB* traverse = TCBhead;

    // check valid prio
    if (prio != HIGH && prio != MEDIUM && prio != LOW && prio != LOWEST ) {
      return RTX_ERR;
    }

    // find task
    while (traverse != NULL || traverse->tid != task_id) {
      traverse = traverse->next;
    }
    if (traverse == NULL || traverse->tid == TID_NULL) {
      return RTX_ERR;
    }

    // check that the current task is has edit privledges
    if (gp_current_task->priv == 0 && traverse->priv == 1) {
      return RTX_ERR;
    }

    traverse->prio = prio;
    return RTX_OK;
}

int k_tsk_get(task_t task_id, RTX_TASK_INFO *buffer)
{
#ifdef DEBUG_0
    printf("k_tsk_get: entering...\n\r");
    printf("task_id = %d, buffer = 0x%x.\n\r", task_id, buffer);
#endif /* DEBUG_0 */
    if (buffer == NULL) {
        return RTX_ERR;
    }
    TCB* traverse = TCBhead;
    while (traverse != NULL || traverse->tid != task_id) {
      traverse = traverse->next;
    }
    if (traverse == NULL) {
      return RTX_ERR;
    }
    /* The code fills the buffer with some fake task information.
       You should fill the buffer with correct information    */
    buffer->tid = traverse->tid;
    buffer->prio = traverse->prio;
    buffer->state = traverse->state;
    buffer->priv = traverse->priv;
    // task entry ?
    // buffer->ptask = traverse->ptask;
    buffer->k_sp = *g_k_stacks[task_id];

    // stack size?
    // buffer->k_stack_size =;

    if (traverse->prio == 0) {
      buffer->u_sp = NULL;
      buffer->u_stack_size = NULL;
    } else {
      buffer->u_sp = *g_p_stacks[task_id];
      // stack size?
      // buffer->u_stack_size = ;
    }

    return RTX_OK;
}

int k_tsk_ls(task_t *buf, int count){
#ifdef DEBUG_0
    printf("k_tsk_ls: buf=0x%x, count=%d\r\n", buf, count);
#endif /* DEBUG_0 */
    return 0;
}

/*
 *===========================================================================
 *                             TO BE IMPLEMETED IN LAB4
 *===========================================================================
 */

int k_tsk_create_rt(task_t *tid, TASK_RT *task, RTX_MSG_HDR *msg_hdr, U32 num_msgs)
{
    return 0;
}

void k_tsk_done_rt(void) {
#ifdef DEBUG_0
    printf("k_tsk_done: Entering\r\n");
#endif /* DEBUG_0 */
    return;
}

void k_tsk_suspend(struct timeval_rt *tv)
{
#ifdef DEBUG_0
    printf("k_tsk_suspend: Entering\r\n");
#endif /* DEBUG_0 */
    return;
}

/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
