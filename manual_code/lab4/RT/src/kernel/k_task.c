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
#include "kcd_task.h"
#include "printf.h"

/*
 *==========================================================================
 *                            GLOBAL VARIABLES
 *==========================================================================
 */

TCB             *gp_current_task = NULL;	// the current RUNNING task
TCB             g_tcbs[MAX_TASKS];			// an array of TCBs
RTX_TASK_INFO   g_null_task_info;			// The null task info
U32             g_num_active_tasks = 0;		// number of non-dormant tasks
TCB 			*TCBhead = NULL;						//points to starting TCB not NULL

BOOL			kernelOwnedMemory = 0;



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

int initialized = 0;

TCB * heap[MAX_TASKS];
int q_size = 0;
//int initialized = 0;
SUSPEND_INFO * suspended_tasks[MAX_TASKS];
int total_suspended_tasks = 0;

//priority_queue * global_q = NULL;


TCB * rt_heap[MAX_TASKS];
int rt_q_size = 0;

void clearEvent( void );
void swap(TCB * p1, TCB * p2, int mode );
void remove( TCB * p );
void changePriority( TCB * p );
TCB dequeue( int mode );
void moveToEnd( TCB * p );
int moveUp(int i, int mode);
void enqueue( TCB * p );
TCB * get_highest_priority( void );
void heapify( int i, int mode );

TCB * thread_changed_p = NULL; // if a thread has created, exits, and prio changes
int  thread_changed_event = -1; // if a thread has created, exits, and prio changes
int old_priority = NULL; // if a thread switched priority I need the previous stateTCB * thread_changed_p = NULL
unsigned int global_clk = 0;

 //thread_changed_p = NULL; // if a thread has created, exits, and prio changes
 //thread_changed_event = NULL; // if a thread has created, exits, and prio changes
 //old_priority = NULL; // if a thread switched priority I need the previous state

// note: scheduler is called when task is 
TCB *scheduler(void)
{
    if ( initialized == 0 ) {  // initialize the queues

        // build the priority_queue datastructure
        q_size = 0;
        rt_q_size = 0;

        // go through linked list
        TCB* curr;
        curr = TCBhead;

        while (curr != NULL ) {
        	enqueue(curr);
        	printf("enque element is: 0x%x and prio %d \r\n", curr, curr->prio);
        	curr= curr->next;

        }
        initialized = 1;

    } else { // check if a state has changed, this could be called from created, exits, prio changes
    	//printf("thread_changed_event: %d \r\n", thread_changed_event);
        if ( thread_changed_event != -1 ) {

            if ( thread_changed_event == TCREATED ) {
            	printf("CREATING\r\n");
                enqueue( thread_changed_p );
            } else if ( thread_changed_event == TEXITED ) {
            	printf("REMOVING\r\n");
                remove( thread_changed_p );
            } else if ( thread_changed_event == TPRIORITY ) {
            	printf("CHANGING PRIO\r\n");
                changePriority( thread_changed_p );
            }

            clearEvent();
        }
    }
    printf("size: %d \r\n", q_size );
    printf("checking queue ======================================= \r\n");
    for(int i =0; i<q_size; i++){
    	printf("element in queue: 0x%x and priority: %d \r\n",heap[i], heap[i]->prio);
    }

    printf("size: %d \r\n", rt_q_size );
    printf("checking RT queue ======================================= \r\n");
    for(int i =0; i < rt_q_size; i++){
    	printf("element in RT queue: 0x%x and priority: %d \r\n",rt_heap[i], rt_heap[i]->prio);
    }
    printf("\r\n");
    return get_highest_priority();
}

// resets the global variables
void clearEvent() {
    thread_changed_event = NULL;
    thread_changed_p = NULL;
    old_priority = NULL;
}

// swap two TCBS on the global queue
// mode 0 for RT heap
// mode 1 for Non RT heap
void swap(TCB * p1, TCB * p2, int mode ) {

    if ( mode == 0 ) {
        rt_heap[p1->scheduler_index] = p2;
        rt_heap[p2->scheduler_index] = p1;
/*        printf("indexes: %d, %d \r\n",p1->scheduler_index,p2->scheduler_index  );
        printf("p1: 0x%x p2: 0x%x \r\n", p1 , p2 );
        printf("new p1: 0x%x new p2: 0x%x \r\n", rt_heap[p1->scheduler_index] , rt_heap[p2->scheduler_index]  );
*/
        int tmp = p1->scheduler_index;
        p1->scheduler_index = p2->scheduler_index;
        p2->scheduler_index = tmp;
    } else {
        heap[p1->scheduler_index] = p2;
        heap[p2->scheduler_index] = p1;
        /*
        printf("indexes: %d, %d \r\n",p1->scheduler_index,p2->scheduler_index  );
        printf("p1: 0x%x p2: 0x%x \r\n", p1 , p2 );
        printf("new p1: 0x%x new p2: 0x%x \r\n", heap[p1->scheduler_index] , heap[p2->scheduler_index]  );
*/
        int tmp = p1->scheduler_index;
        p1->scheduler_index = p2->scheduler_index;
        p2->scheduler_index = tmp;
    }
}

// removes a specific thread from the priority queue
// RT
void remove( TCB * p ) {



    //printf("remove index: %d \r\n", p->scheduler_index);


    // Extract the node
    if ( p->prio == PRIO_RT ) {
    	p->prio = -1; // hopefully this works
        // Shift the node to the root
        p->scheduler_index = moveUp( p->scheduler_index, 2 );

        dequeue( 0 );
    } else {
    	p->prio = -1; // hopefully this works
        // Shift the node to the root
        p->scheduler_index = moveUp( p->scheduler_index, 1 );

        dequeue( 1 );
    }
}

// changes the priority of a thread
// RT
void changePriority( TCB * p ) {

    if ( p->prio < old_priority ) {

        // Extract the node
        if ( p->prio == PRIO_RT ) {
            p->scheduler_index = moveUp( p->scheduler_index, 0 );
        } else {
            p->scheduler_index = moveUp( p->scheduler_index, 1 );
        }
    } else {
        if ( p->prio == PRIO_RT ) {
            heapify( p->scheduler_index, 0 );
        } else {
            heapify( p->scheduler_index, 1 );
        }
    }
}

// deletes the max item and return
// mode 0 for RT heap
// mode 1 for Non RT heap
TCB dequeue( int mode ) {

    TCB max;

    if ( mode == 0 ) { // RT mode

        max = *rt_heap[0];

        // replace the first item with the last item
        swap( rt_heap[0], rt_heap[ rt_q_size - 1], 0 );
        rt_q_size--;
        for(int i = 0; i<rt_q_size; i++){
        	//printf("rt element in queue: 0x%x and priority: %d and sched index: %d \r\n",rt_heap[i], rt_heap[i]->prio, rt_heap[i]->scheduler_index);
        }

        // maintain the heap property by heapifying the
        // first item
        heapify( 0, 0 );

    } else { // Non RT mode

        max = *heap[0];

        // replace the first item with the last item
        swap( heap[0], heap[ q_size - 1], 1 );
        q_size--;
        for(int i =0; i<q_size; i++){
        	//printf("element in queue: 0x%x and priority: %d and sched index: %d \r\n",heap[i], heap[i]->prio, heap[i]->scheduler_index);
        }

        // maintain the heap property by heapifying the
        // first item
        heapify( 0, 1 );
    }

    

    return max;
}

// moves the element up to the top
// mode 0 for RT heap
// mode 1 for Non RT
// mode 2 for remove() from RT heap
int moveUp(int i, int mode) {

    if ( mode == 0 ){
        while (i != 0 && (*rt_heap[(i - 1) / 2]).next_job_deadline > (*rt_heap[i]).next_job_deadline) {
            //printf("SWAPPED! \r\n");
            swap( rt_heap[(i - 1) / 2], rt_heap[i], 0);
            i = (i - 1) / 2;
        }
    } else if (mode == 1 ) {
        while (i != 0 && (*heap[(i - 1) / 2]).prio > (*heap[i]).prio) {
            //printf("SWAPPED! \r\n");
            swap( heap[(i - 1) / 2], heap[i], 1);
            i = (i - 1) / 2;
        }
    } else if (mode == 2) {
    	while (i != 0 && (*rt_heap[(i - 1) / 2]).prio > (*rt_heap[i]).prio) {
			//printf("SWAPPED! \r\n");
			swap( rt_heap[(i - 1) / 2], rt_heap[i], 0);
			i = (i - 1) / 2;
		}
    	rt_heap[0]->prio = 0; //set the prio back to zero after dequeueing a rt task
    }
    

    return i;
}



// for RT tasks the deadline is the curr_time % longest_deadline
void compute_next_job_deadline( TCB * p ) {

    unsigned int seconds_in_usec = p->TcbInfo->p_n.sec * 1000000;
    unsigned int usec = p->TcbInfo->p_n.usec;

    if ( p->next_job_deadline == 0 ) {

        p->next_job_deadline = global_clk + seconds_in_usec + usec + 100;
    
    } else {

        p->next_job_deadline = p->next_job_deadline + seconds_in_usec + usec;

    }
}

// add the thread to our heap at the appropriate position for both RT and non-RT
void enqueue( TCB * p ) {

    int i;
	//printf("address of p: 0x%x \r\n", p);

    if ( q_size + rt_q_size >= MAX_TASKS ) {
        //printf("%s\n", "The heap is full. Cannot insert");
        return;
    }

    if ( p->prio == PRIO_RT ) {

        // first insert at end and increment size
        i = rt_q_size;
        rt_heap[rt_q_size] = p;
        p->scheduler_index = rt_q_size;
        //printf("rt heap element: 0x%x \r\n",rt_heap[rt_q_size] );
        rt_q_size += 1;
        //printf("rt size: %d \r\n", rt_q_size);

        compute_next_job_deadline( p );

        // move up until the heap property satisfies
        p->scheduler_index = moveUp(i, 0);
        for(int i = 0; i<rt_q_size; i++){
            //printf("rt queue element: 0x%x priority: %d scheduler index: %d \r\n",rt_heap[i], rt_heap[i]->prio, rt_heap[i]->scheduler_index );

        }

    } else {

        // first insert at end and increment size
        i = q_size;
        heap[q_size] = p;
        p->scheduler_index = q_size;
        //printf("heap element: 0x%x \r\n",heap[q_size] );
        q_size += 1;
        //printf("size: %d \r\n", q_size);

        // move up until the heap property satisfies
        p->scheduler_index =moveUp(i, 1);
        for(int i = 0; i<q_size; i++){
            //printf("queue element: 0x%x priority: %d scheduler index: %d \r\n",heap[i], heap[i]->prio, heap[i]->scheduler_index );

        }
    }
}



// returns the minimum item of either RT or Non-RT heap
// if RT then move to back of the line, as we've executed a job
TCB * get_highest_priority() {
    if ( rt_q_size == 0 ) {
        return heap[ 0 ];
    } else {
        TCB * p = rt_heap[ 0 ];
        moveToEnd( p );
        compute_next_job_deadline( p );
        return p;
    }
}

// moves an RT task to the end of the line after it executes it's job
void moveToEnd( TCB * p ) {
    
    dequeue( 0 ); // dequeue from the RT heap

    rt_heap[rt_q_size] = p;
    p->scheduler_index = rt_q_size;
    rt_q_size += 1;

}

// we are going to maintain a heap for the tcbs
// 0 for RT tasks
// 1 for Non RT tasks
void heapify( int i, int mode )
{
    int smallest = i;
    int l = 2 * i + 1;
    int r = 2 * i + 2;

    if ( mode == 0 ) {

        int n = rt_q_size;

        // If left child is larger than root
        if (l < n && rt_heap[l]->next_job_deadline < rt_heap[smallest]->next_job_deadline)
            smallest = l;

        // If right child is larger than largest so far
        if (r < n && rt_heap[r]->next_job_deadline < rt_heap[smallest]->next_job_deadline)
            smallest = r;

        // If largest is not root
        if (smallest != i) {
            swap(rt_heap[i], rt_heap[smallest], 0);
            heapify( smallest, 0  );
        }

    } else {

        int n = q_size;

        // If left child is larger than root
        if (l < n && heap[l]->prio < heap[smallest]->prio)
            smallest = l;

        // If right child is larger than largest so far
        if (r < n && heap[r]->prio < heap[smallest]->prio)
            smallest = r;

        // If largest is not root
        if (smallest != i) {
            swap(heap[i], heap[smallest], 1);
            heapify( smallest, 1 );
        }
    }
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


    // setup the A9 timer in auto loading mode
    // calculate prescaler to make each cycle = 1 us
    config_a9_timer( 0xFFFFFFFF, 1, 0, 200);

    // start the HPS timer 0
    // 100 us / 10 ns = 10,000 cyles
    config_hps_timer(0, 10000, 1, 0);

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
    TCBhead 		= &g_tcbs[0]; // head should point to NULL
    TCBhead->next 	= NULL;
    g_num_active_tasks++;
    gp_current_task = p_tcb;
    TCB* oldTCB = p_tcb;
    TCB* newTCB;

    // create the rest of the tasks
    p_taskinfo = task_info;
    for ( int i = 0; i < num_tasks; i++ ) {
    	//printf("address of oldTCB is: 0x%x \r\n ", oldTCB);
    	//printf("address of new TCB is: 0x%x \r\n ", newTCB);

        newTCB = &g_tcbs[i+1];
        //printf("address of next pTcb is: 0x%x \r\n ", newTCB);
        if (i+1 == 1) {
        	TCBhead->next = &g_tcbs[i+1];
        }

        oldTCB->next = newTCB; //join the linked list of TCBs LL only has active tasks

        //printf("address of next(next pTcb) is: 0x%x \r\n ", oldTCB -> next);
       // printf("address of head next is: 0x%x \r\n ", TCBhead -> next);

        newTCB->next = NULL; //initialize next pointer  of current task
        newTCB->TcbInfo = p_taskinfo;
        newTCB->tid = i+1;
        newTCB->state = READY;
        newTCB->priv = newTCB->TcbInfo->priv;
        newTCB->prio = newTCB->TcbInfo->prio;
        CQ mbx_cq;
        mbx_cq.head = NULL;
        mbx_cq.tail = NULL;
        mbx_cq.size = 0;
        mbx_cq.remainingSize = 0;
        mbx_cq.memblock_p = NULL;
        newTCB->mbx_cq = mbx_cq;
        //if task is real time then create mailbox with byte size in rtx task info
        if(newTCB->prio == PRIO_RT) {
			newTCB->next_job_deadline = 0;
			newTCB->TcbInfo->rt_jobNumber = 0;

			if( newTCB->TcbInfo->rt_mbx_size > 0 && newTCB->TcbInfo->rt_mbx_size > MIN_MBX_SIZE ) {
				size_t newsize = newTCB->TcbInfo->rt_mbx_size % 4 == 0 ? newTCB->TcbInfo->rt_mbx_size : (newTCB->TcbInfo->rt_mbx_size / 4 + 1) * 4;
				//allocate memory for mailbox
				kernelOwnedMemory = 1;
				U8 *p_mbx = k_mem_alloc(newsize);
				kernelOwnedMemory = 0;
				//check that allocation was successful
				if (p_mbx == NULL) return -1;
				newTCB->mbx_cq.memblock_p = p_mbx;
				newTCB->mbx_cq.size = newsize;
				newTCB->mbx_cq.remainingSize = newsize;
			}

		}




        if (k_tsk_create_new(newTCB->TcbInfo, newTCB , newTCB->tid) == RTX_OK) { // use RTXInfo pointer from TCB struct as parameter
        	g_num_active_tasks++;
        }
        printf("tid of current tcb is: %d \r\n", newTCB->tid);
        printf("address of current pTcb is: 0x%x \r\n ", newTCB);
        printf("address of previous pTcb is: 0x%x \r\n ", oldTCB);
        p_taskinfo++;
        oldTCB = newTCB; // end of loop current TCB becomes old TCB

    }

    //initialize KCD task
//    if(oldTCB->next == NULL) {
//    	newTCB = &g_tcbs[15];
//    	 newTCB->next = NULL; //initialize next pointer  of current task
//    	 newTCB->tid = 15;
//    	 oldTCB->next = newTCB;
//
//    	 RTX_TASK_INFO* kcdInfo;
//    	 kcdInfo = k_mem_alloc(sizeof(RTX_TASK_INFO));
//    	 newTCB->TcbInfo = kcdInfo;
//    	 // initialize TCB structure
//    	 	newTCB->prio = 1;
//    	 	newTCB->state = READY;
//    	 	newTCB->priv = 0;
//    	 	newTCB->TcbInfo->state = READY;
//    	 	newTCB->TcbInfo->u_stack_size = KCD_MBX_SIZE;
//    	 	newTCB->TcbInfo->k_stack_size = KERN_STACK_SIZE;
//    	 	newTCB->TcbInfo->ptask = &kcd_task;
//    	 	//initializing mailbox
//    	 	CQ mbx_cq;
//    	 	        mbx_cq.head = NULL;
//    	 	        mbx_cq.tail = NULL;
//    	 	        mbx_cq.size = 0;
//    	 	        mbx_cq.remainingSize = 0;
//    	 	        mbx_cq.memblock_p = NULL;
//    	 	       newTCB->mbx_cq = mbx_cq;
//
//    	 	//increment number of active tasks
//    	 	if(k_tsk_create_new(newTCB->TcbInfo,newTCB, newTCB->tid ) == RTX_OK) {
//    	 		g_num_active_tasks++;
//    	 	}



    //}


    oldTCB = TCBhead;
    while(oldTCB!= NULL) {
    	printf("PRINTING LIST OF TASKS CURRENTLY IN GTCBS:  \r\n");
    	printf("address of task: 0x%x and task prio is: %d \r\n", oldTCB, oldTCB->prio);
    	oldTCB = oldTCB->next;

    }


    //scheduler();
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
    p_taskinfo->prio = p_tcb->prio; // setting prio for initialization
    p_taskinfo->priv = p_tcb->priv;
    p_taskinfo->tid = tid;


    /*---------------------------------------------------------------
     *  Step1: allocate kernel stack for the task
     *         stacks grows down, stack base is at the high address
     * -------------------------------------------------------------*/

    ///////sp = g_k_stacks[tid] + (KERN_STACK_SIZE >> 2) ;
    sp = k_alloc_k_stack(tid);
    //set current kernel stack pointer to same address as base high address of kernel

    // 8B stack alignment adjustment
    if ((U32)sp & 0x04) {   // if sp not 8B aligned, then it must be 4B aligned
        sp--;               // adjust it to 8B aligned
    }
    p_taskinfo->k_sp = (U32) sp;
    p_taskinfo->k_stack_hi = (U32) sp;

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
        //allocating current stack pointer
        p_taskinfo->u_sp = (U32) sp;
        p_taskinfo->u_stack_hi = (U32) sp;

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

    //temp
    TCB* curr = gp_current_task;

    
    if ( gp_current_task == NULL  ) {
        gp_current_task = p_tcb_old;        // revert back to the old task
        return RTX_ERR;
    }
    printf("address of current task: 0x%x \r\n",gp_current_task );



    // at this point, gp_current_task != NULL and p_tcb_old != NULL
    if (gp_current_task != p_tcb_old) {
        gp_current_task->state = RUNNING;   // change state of the to-be-switched-in  tcb
        p_tcb_old->state = READY;   // change state of the to-be-switched-out tcb
        k_tsk_switch(p_tcb_old);            // switch stacks
    }

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

    //some error checking...

    if(g_num_active_tasks > MAX_TASKS) {
    	return RTX_ERR;
    }

    if(stack_size < PROC_STACK_SIZE) {
    	return RTX_ERR;

    }

    if(prio < 1 || prio > 254 ) {
    	return RTX_ERR;
    }


	//initialize
	RTX_TASK_INFO* newTaskInfo;
	TCB* newTaskBlock;
	int taskFound = 0;//flag to indicate TCB created

	TCB* traverse = TCBhead;

	//Linked List houses all the active TCBS
	while(taskFound == 0  ) {

		printf("address of traverse 0x%x \r\n", traverse);
		printf("traverse next Tid is: %d \r\n",traverse->next->tid );
		printf("traverse Tid is: %d \r\n",traverse->tid );

		//if rtx_init is called with no num_tasks parameter

			if(traverse == NULL ) {

				*task = 1;
				newTaskBlock =  &g_tcbs[*task];
				newTaskInfo = k_mem_alloc(sizeof(RTX_TASK_INFO));
				newTaskBlock->TcbInfo = newTaskInfo;
				TCBhead = newTaskBlock;
				TCBhead->next = NULL;
				taskFound = 1;
			}

		//if you find an unused TCB at the end of the linked  list
			else if (traverse->next == NULL) {
				printf("free space at end of list!");
				*task = traverse->tid +1;
				printf("task(tid): %d", *task);
				newTaskBlock =  &g_tcbs[*task];
				printf("address of new TCB is: 0x%x \r\n ", newTaskBlock);
				//check if vacant spot in array already had a dormant tcb
				if(newTaskBlock->state != DORMANT) {
					printf("allocating memory for task info struct");
					newTaskInfo = k_mem_alloc(sizeof(RTX_TASK_INFO));
					newTaskBlock->TcbInfo = newTaskInfo;
				}
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
				newTaskInfo = k_mem_alloc(sizeof(RTX_TASK_INFO));
				newTaskBlock->TcbInfo = newTaskInfo;
				newTaskBlock->next = traverse->next;
				traverse->next = newTaskBlock;
				taskFound = 1;
			}

			else {

			traverse = traverse -> next;
			}
	}


	// initialize TCB structure
	newTaskBlock->prio = prio;
	newTaskBlock->state = READY;
	newTaskBlock->priv = 0;
	newTaskBlock->tid = (U8) *task;
	newTaskBlock->TcbInfo->tid = *task;
	newTaskBlock->TcbInfo->u_stack_size = stack_size;
	newTaskBlock->TcbInfo->k_stack_size = KERN_STACK_SIZE;
	newTaskBlock->TcbInfo->ptask = task_entry;
	newTaskBlock->TcbInfo->rt_jobNumber = 0;

	//initializing mailbox
	CQ mbx_cq;
	mbx_cq.head = NULL;
	mbx_cq.tail = NULL;
	mbx_cq.size = 0;
	mbx_cq.remainingSize = 0;
	mbx_cq.memblock_p = NULL;
	newTaskBlock->mbx_cq = mbx_cq;

	//increment number of active tasks
	if(k_tsk_create_new(newTaskBlock->TcbInfo,newTaskBlock, newTaskBlock->TcbInfo->tid ) == RTX_OK) {
		g_num_active_tasks++;
	}

	//set global flags for scheduler and run new task

	thread_changed_p = newTaskBlock; // if a thread has created, exits, and prio changes
	printf("address of created task: 0x%x \r\n",thread_changed_p );
	thread_changed_event = TCREATED;

	//call new task to run
	 if (k_tsk_run_new() != RTX_OK) {
		 return RTX_ERR;
	 }



    return RTX_OK;

}

void k_tsk_exit(void)
{
#ifdef DEBUG_0
    printf("k_tsk_exit: entering...\n\r");
#endif /* DEBUG_0 */
    TCB *p_tcb_old = gp_current_task;

    //deallocate task's mailbox
    if (p_tcb_old->mbx_cq.memblock_p != NULL) {
    	kernelOwnedMemory = 1;
    	k_mem_dealloc(p_tcb_old->mbx_cq.memblock_p);
    	kernelOwnedMemory = 0;
    }

    // remove from linked list
    if (TCBhead->tid == p_tcb_old->tid) {
    	TCBhead = TCBhead->next;
//        return;
    }

    TCB* prev = TCBhead;
    printf("address of head: 0x%x \r\n",TCBhead );
    // find task
    while (prev->next != NULL && prev->next->tid != p_tcb_old->tid ) {
    	prev = prev->next;
    }
    //this means the ending task is at the end of the active task linked list
    if (prev->next == NULL) {
    	return;
    }

    // found the thing
    prev->next = prev->next->next;


    if (gp_current_task == NULL) {
		return;
	}


    p_tcb_old = gp_current_task;

    if(gp_current_task->priv == 0) {

    		kernelOwnedMemory = 1;
           //dealloc user stack; from stack High, move back by the stack size to get the original pointer returned from mem_alloc
            k_mem_dealloc((U8 *)gp_current_task->TcbInfo->u_stack_hi - gp_current_task->TcbInfo->u_stack_size);
            kernelOwnedMemory = 0;

    }

	thread_changed_event = TEXITED;
    thread_changed_p = p_tcb_old;



    gp_current_task = scheduler();

    printf("address of current task: 0x%x \r\n", gp_current_task);

    if ( gp_current_task == NULL  ) {
    	gp_current_task = p_tcb_old;        // revert back to the old task
    	return;
    }

    // at this point, gp_current_task != NULL and p_tcb_old != NULL
    if (gp_current_task != p_tcb_old) {
    	gp_current_task->state = RUNNING;   // change state of the to-be-switched-in  tcb
        p_tcb_old->state = DORMANT;           // change state of the to-be-switched-out tcb
        g_num_active_tasks--;				//number of active tasks decreases
        k_tsk_switch(p_tcb_old);            // switch stacks
    }



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
    if (prio < 1 || prio > 254 ) {
      return RTX_ERR;
    }

    // find task
    while (traverse != NULL && traverse->tid != task_id) {
      traverse = traverse->next;
    }
    if (traverse == NULL || traverse->tid == TID_NULL) {
      return RTX_ERR;
    }

    // check that the current task is has edit priviledges
    if (gp_current_task->priv == 0 && traverse->priv == 1) {
      return RTX_ERR;
    }
    
    thread_changed_p = traverse;
    thread_changed_event = TPRIORITY;
    printf("old priority: %d \r\n",  traverse->prio);
    old_priority = traverse->prio;

    traverse->prio = prio;
    printf("new priority: %d \r\n",  traverse->prio);

    k_tsk_run_new();
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
    while (traverse != NULL && traverse->tid != task_id) {// CHANGED
      traverse = traverse->next;
    }
    if (traverse == NULL) {
      return RTX_ERR;
    }
	
    buffer->tid = traverse->tid;
    buffer->prio = traverse->prio;
    buffer->state = traverse->state;
    buffer->priv = traverse->priv;
    buffer->ptask = traverse->TcbInfo->ptask;
    buffer->k_sp = traverse->TcbInfo->k_sp;
    buffer->k_stack_hi = traverse->TcbInfo->k_stack_hi;

    buffer->k_stack_size = traverse->TcbInfo->k_stack_size;

    if (traverse->priv != 0) {
       buffer->u_sp = NULL;
       buffer->u_stack_hi = NULL;
      buffer->u_stack_size = NULL;
    } else {
    	buffer->u_stack_hi = traverse->TcbInfo->u_stack_hi;
      buffer->u_sp = traverse->TcbInfo->u_sp;
      buffer->u_stack_size = traverse->TcbInfo->u_stack_size;
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

int k_tsk_create_rt(task_t *tid, TASK_RT *task)
{


	//error checking...
	if(task->p_n.sec == 0 && task->p_n.usec == 0){
		return RTX_ERR;
	}

	if( task->p_n.usec % 500 != 0 ) {
		return RTX_ERR;
	}

	if(g_num_active_tasks > MAX_TASKS) {
		return RTX_ERR;
	}

	if(task->u_stack_size < PROC_STACK_SIZE) {
		return RTX_ERR;
	}

	RTX_TASK_INFO* newTaskInfo;
		TCB* newTaskBlock;
		int taskFound = 0;//flag to indicate TCB created

		TCB* traverse = TCBhead;

		//Linked List houses all the active TCBS
		while(taskFound == 0  ) {

			printf("address of traverse 0x%x \r\n", traverse);
			printf("traverse next Tid is: %d \r\n",traverse->next->tid );
			printf("traverse Tid is: %d \r\n",traverse->tid );

			//if rtx_init is called with no num_tasks parameter

				if(traverse == NULL ) {

					*tid = 1;
					newTaskBlock =  &g_tcbs[*tid];
					newTaskInfo = k_mem_alloc(sizeof(RTX_TASK_INFO));
					newTaskBlock->TcbInfo = newTaskInfo;
					TCBhead = newTaskBlock;
					TCBhead->next = NULL;
					taskFound = 1;
				}

			//if you find an unused TCB at the end of the linked  list
				else if (traverse->next == NULL) {
					printf("free space at end of list!");
					*tid = traverse->tid +1;
					printf("task(tid): %d", *tid);
					newTaskBlock =  &g_tcbs[*tid];
					printf("address of new TCB is: 0x%x \r\n ", newTaskBlock);
					//check if vacant spot in array already had a dormant tcb
					if(newTaskBlock->state != DORMANT) {
						printf("allocating memory for task info struct");
						newTaskInfo = k_mem_alloc(sizeof(RTX_TASK_INFO));
						newTaskBlock->TcbInfo = newTaskInfo;
					}
					traverse->next = newTaskBlock;
					newTaskBlock->next = NULL;
					taskFound = 1;

				}

			//if you find a dormant or unused TCB in between two active TCBs...
				else if((traverse->next->tid - traverse->tid) > 1) {
					printf("there is space between two TCBs!");

					*tid =   traverse->tid +1;
					printf("task(tid): %d", *tid);
					newTaskBlock = &g_tcbs[*tid];
					printf("address of new TCB is: 0x%x \r\n ", newTaskBlock);
					newTaskInfo = k_mem_alloc(sizeof(RTX_TASK_INFO));
					newTaskBlock->TcbInfo = newTaskInfo;
					newTaskBlock->next = traverse->next;
					traverse->next = newTaskBlock;
					taskFound = 1;
				}

				else {

				traverse = traverse -> next;
				}
		}


		// initialize TCB structure
		newTaskBlock->prio = PRIO_RT;
		newTaskBlock->state = READY;
		newTaskBlock->priv = 0;
		newTaskBlock->tid = (U8) *tid;
		newTaskBlock->TcbInfo->tid = *tid;
		newTaskBlock->TcbInfo->u_stack_size = task->u_stack_size;
		newTaskBlock->TcbInfo->k_stack_size = KERN_STACK_SIZE;
		newTaskBlock->TcbInfo->ptask = task->task_entry;
		newTaskBlock->TcbInfo->rt_mbx_size = task->rt_mbx_size;
		newTaskBlock->TcbInfo->p_n.sec = task->p_n.sec;
		newTaskBlock->TcbInfo->p_n.usec = task->p_n.usec;
		newTaskBlock->next_job_deadline = 0;
		newTaskBlock->TcbInfo->rt_jobNumber = 0;


		//initializing mailbox
			CQ mbx_cq;
			        mbx_cq.head = NULL;
			        mbx_cq.tail = NULL;
			        mbx_cq.size = 0;
			        mbx_cq.remainingSize = 0;
			        mbx_cq.memblock_p = NULL;
			        newTaskBlock->mbx_cq = mbx_cq;

			        // allocating space for mailbox
		if(task->rt_mbx_size > 0 && task->rt_mbx_size > MIN_MBX_SIZE ) {
			 size_t newsize = newTaskBlock->TcbInfo->rt_mbx_size % 4 == 0 ? newTaskBlock->TcbInfo->rt_mbx_size : (newTaskBlock->TcbInfo->rt_mbx_size / 4 + 1) * 4;

			 //allocate memory for mailbox
			 kernelOwnedMemory = 1;
			 U8 *p_mbx = k_mem_alloc(newsize);
			 kernelOwnedMemory = 0;
			 //check that allocation was successful
			 if (p_mbx == NULL) return -1;
			 newTaskBlock->mbx_cq.memblock_p = p_mbx;
			 newTaskBlock->mbx_cq.size = newsize;
			 newTaskBlock->mbx_cq.remainingSize = newsize;
			}


		//increment number of active tasks
		if(k_tsk_create_new(newTaskBlock->TcbInfo,newTaskBlock, newTaskBlock->TcbInfo->tid ) == RTX_OK) {
			g_num_active_tasks++;
		}

		thread_changed_p = newTaskBlock; // if a thread has created, exits, and prio changes
		printf("address of created task: 0x%x \r\n",thread_changed_p );
		thread_changed_event = TCREATED;

				//call new task to run
		 if (k_tsk_run_new() != RTX_OK) {
			 return RTX_ERR;
		 }



	return RTX_OK;

}

void k_tsk_done_rt(void) {
#ifdef DEBUG_0
    printf("k_tsk_done: Entering\r\n");
#endif /* DEBUG_0 */

    //1. reset calling task's user stack to what it looks on create
    //+
    //2. reset calling task's program counter on wake-up to 'task_entry'

    //dealloc user stack (kernel stack will be overwritten below)
    kernelOwnedMemory = 1;
	k_mem_dealloc((U8 *)gp_current_task->TcbInfo->u_stack_hi - gp_current_task->TcbInfo->u_stack_size);
	kernelOwnedMemory = 0;
	//now it's as if we're creating a new task, with all the overhead data structures already set up
    k_tsk_create_new(gp_current_task->TcbInfo, gp_current_task, gp_current_task->tid);


    //3. check deadline
    BOOL deadlineMet = gp_current_task->next_job_deadline > global_clk ? TRUE : FALSE;

    //3a. if deadline met, set task to SUSPENDED, then switch tasks
    if (deadlineMet == TRUE) {
    	SUSPEND_INFO* new_sus;
		new_sus->task = gp_current_task;
		new_sus->total_usecs = gp_current_task->next_job_deadline - global_clk;
		suspended_tasks[total_suspended_tasks] = new_sus;
		total_suspended_tasks++;

    	//assumption: the scheduler will always return a different task.
    	gp_current_task->TcbInfo->rt_jobNumber++;
    	gp_current_task->state = SUSPENDED;
    	TCB *p_tcb_old = gp_current_task;
    	thread_changed_event = TEXITED;
    	thread_changed_p = p_tcb_old;
		gp_current_task = scheduler();
		gp_current_task->state = RUNNING;   // change state of the to-be-switched-in  tcb
		k_tsk_switch(p_tcb_old);            // switch stacks
    }
    //3b. if deadline missed, send error message to UART port (putty), set state to ready, then switch tasks
    else {
    	char strbuff[50];
    	sprintf(strbuff, "Job %u of task %u missed its deadline\r\n", gp_current_task->TcbInfo->rt_jobNumber, (U32) gp_current_task->tid);
    	SER_PutStr(1, strbuff);
    	gp_current_task->TcbInfo->rt_jobNumber++;
    	k_tsk_run_new(); //set this task to ready and get a new task to run
    }
    return;
}

void k_tsk_suspend(TIMEVAL *tv)
{
#ifdef DEBUG_0
    printf("k_tsk_suspend: Entering\r\n");
#endif /* DEBUG_0 */
    // empty time? return
    if (tv->sec == 0 && tv->usec == 0) {
    	return;
    }
    // if not multiple of 500, return
    if (tv->usec % 500 != 0) {
    	return;
    }

    SUSPEND_INFO* new_sus;
    new_sus->task = gp_current_task;
    new_sus->total_usecs = tv->sec * 1000000 + tv->usec;
    suspended_tasks[total_suspended_tasks] = new_sus;

    total_suspended_tasks++;

    TCB* p_tcb_old;

    p_tcb_old = gp_current_task;
    p_tcb_old->state = SUSPENDED;

    thread_changed_event = TEXITED;
    thread_changed_p = p_tcb_old;

    gp_current_task = scheduler();

    gp_current_task->state = RUNNING;
    k_tsk_switch(p_tcb_old);

    return;
}

/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
