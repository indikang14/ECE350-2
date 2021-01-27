/*
 ****************************************************************************
 *
 *                  UNIVERSITY OF WATERLOO ECE 350 RTOS LAB
 *
 *                     Copyright 2020-2021 Yiqing Huang
 *
 *          This software is subject to an open source license and
 *          may be freely redistributed under the terms of MIT License.
 ****************************************************************************
 */

/**************************************************************************//**
 * @file        k_mem.c
 * @brief       Kernel Memory Management API C Code
 *
 * @version     V1.2021.01
 * @authors     Yiqing Huang
 * @date        2021 JAN
 *
 * @note        skeleton code
 *
 *****************************************************************************/

/** 
 * @brief:  k_mem.c kernel API implementations, this is only a skeleton.
 * @author: Yiqing Huang
 */

#include "k_mem.h"
#include "Serial.h"
#ifdef DEBUG_0
#include "printf.h"
#endif  /* DEBUG_0 */

/*
 *==========================================================================
 *                            GLOBAL VARIABLES
 *==========================================================================
 */
// kernel stack size, referred by startup_a9.s
const U32 g_k_stack_size = KERN_STACK_SIZE;

// task kernel stacks
U32 g_k_stacks[MAX_TASKS][KERN_STACK_SIZE >> 2] __attribute__((aligned(8)));

/*
 *===========================================================================
 *                            FUNCTIONS
 *===========================================================================
 

 */

// blocks of memory which are unallocated
typedef struct free_node {
    unsigned int size; // contains the 8 Bytes of overhead
    struct __free_node *next;
} free_node;

// blocks of memory which are allocated
typedef struct header {
    unsigned int size; // contains the 8 Bytes of overhead
    unsigned int padding; // needed to make size_of( struct header ) == size_of( struct free_node )
} header;

free_node * head;
unsigned int *managed_memory_start; // beginning of managed memory
unsigned int last_valid_address = 0xBFFFFFFF; // end of managed memory

int k_mem_init(void) {

	unsigned int end_addr = (unsigned int) &Image$$ZI_DATA$$ZI$$Limit;
    managed_memory_start = (unsigned int*) end_addr;

    // there is no memory available return -1
    if ( end_addr >= last_valid_address ) {
    	return RTX_ERR;
    }

    head = (free_node *) managed_memory_start;
    head->size = last_valid_address - (unsigned int) managed_memory_start;
    head->next = NULL;

    printf("managed_memory_start: 0x%x\r\n", managed_memory_start);
    printf("last_valid_address: 0x%x\r\n", last_valid_address);
    printf("head_size: %d\r\n", head->size);
    printf("head_struct_size: %d\r\n", sizeof( *head ));
    printf("head_location: %x\r\n", head );

    #ifdef DEBUG_0
		printf("k_mem_init: image ends at 0x%x\r\n", end_addr);
		printf("k_mem_init: RAM ends at 0x%x\r\n", RAM_END);
	#endif /* DEBUG_0 */

    // head node was created and setup
    return RTX_OK;
}

void* k_mem_alloc(size_t size) {
#ifdef DEBUG_0
    printf("k_mem_alloc: requested memory size = %d\r\n", size);
#endif /* DEBUG_0 */
    return NULL;
}

int k_mem_dealloc(void *ptr) {
#ifdef DEBUG_0
    printf("k_mem_dealloc: freeing 0x%x\r\n", (U32) ptr);
#endif /* DEBUG_0 */
    return RTX_OK;
}

int k_mem_count_extfrag(size_t size) {
#ifdef DEBUG_0
    printf("k_mem_extfrag: size = %d\r\n", size);
#endif /* DEBUG_0 */
    return RTX_OK;
}

/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
