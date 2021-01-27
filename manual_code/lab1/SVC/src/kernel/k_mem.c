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
    struct free_node *next;
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
    //0. edge case
    if (size == 0) return NULL;
    //1. Traverse linked list, find first node where node->size >= (size + node overhead)
    free_node *currnode = head;
    free_node *prevnode = NULL;
    unsigned int real_req_size = size + sizeof(*currnode);
    while (currnode != NULL) {
    	if (currnode->size >= real_req_size) {
    		break;
    	}
    	else {
    		prevnode = currnode;
    		currnode = currnode->next;
    	}
    }
    if (currnode == NULL) return NULL; //couldn't find a big enough free_node, allocation fails

    //2. currnode is a freenode with size >= real_size, prevnode comes before currnode. handle > and = cases indiv.
    if (currnode->size == real_req_size) {
    	//no splitting. remove node from list. replace free_node bytes with header.
    	if (prevnode != NULL) {
    		prevnode->next = currnode->next; //removing currnode from the linked list
    	} else { //then currnode is head, and we're taking all of head's available memory
    		head = NULL; //just change the address pointed to by head to be NULL
    	}
    	//at the address pointed to by currnode is a free_node. change that to a header
    	header *header_p = (header *) currnode;
    	header_p->size = real_req_size;
    	header_p->padding = 0;
    	return header_p + sizeof(*header_p); //return pointer to start of memory block
    } else if (currnode->size > real_req_size) {
    	//have to split the free_node.
    	if (currnode->size - real_req_size - sizeof(*currnode) < sizeof(*currnode) + 1) {
    		//not enough bytes to split and have enough bytes left over for node overhead + 1 byte. e.g.:
    		//17 - 1 - 8 < 8 + 1
    		//8 < 8+1
    		//have to allocate whole block
    		if (prevnode != NULL) {
				prevnode->next = currnode->next; //removing currnode from the linked list
			} else { //then currnode is head, and we're taking all of head's available memory
				head = NULL; //just change the address pointed to by head to be NULL
			}
			//at the address pointed to by currnode is a free_node. change that to a header
			unsigned int currnodeSize = currnode->size; //note: not the requested size
			header *header_p = (header *) currnode;
			header_p->size = currnodeSize;
			header_p->padding = 0;
			return header_p + currnodeSize - real_req_size; //return pointer such that real_req_size number of bytes is at the end of the physical memory block
    	} else {
    		//enough bytes in the remaining free_node to split it.
    		//we'll allocate from the top of the block. therefore create new freenode below memory to be allocate
    		free_node *newnode = currnode + real_req_size;
    		newnode->size = currnode->size - real_req_size;
    		newnode->next = currnode->next;
    		if (prevnode != NULL) {
    			prevnode->next = newnode;
    		} else { //currnode was head
    			head = newnode;
    		}
    		//new free_node created, now change the old free_node to allocated block
    		//at the address pointed to by currnode is a free_node. change that to a header
			header *header_p = (header *) currnode;
			header_p->size = real_req_size;
			header_p->padding = 0;
			return header_p + sizeof(*header_p); //return pointer to start of memory block
    	}
    }
    return NULL;
}

int k_mem_dealloc(void *ptr) {

	/*
	 * replace header with free node
	 */
	free_node *new; //replacing header
	header *current; //current header of ptr
	unsigned int addr = (unsigned int) ptr;
	addr -= (unsigned int) sizeof(header); //ptr arithmetic to get start of header
	printf ("starting address of free block is 0x%x\r\n", addr);
	current =  (header *)addr; //point to start of header
	new->size = current->size; //replace header size with free size
	new = (free_node *) addr; //replace header with
	printf ("confirm starting address of free block is 0x%x\r\n", new);
	/**
	 * check for coalescing and merge blocks
	 ***/

	/**
	 * address ordered policy
	 */

	free_node* traverse = head;
	if(head -> next == NULL ) { //no fragments yet
		if(new == head - new->size) { // if freed memory is adjacent to head merge them
			printf("in here because memory block adjacent to head;");
			head = new;
			head -> size += new->size ;
		}
		else if(head > new) { // if not adjacent just free the block
			printf("in here because memory block freed at an earlier address than head");
			head = new;
			new->next = traverse;
		}

		return new->size;

	}
	while(traverse->next != NULL) { // traverse the list
		printf("traversing the list and coalescing");

		if(new==traverse + traverse->size  ) { // if adjacent memory to the
			traverse -> size += new->size;
			return new->size;
		}
		else if(new == traverse - traverse->size) {
			traverse = new ;
			traverse ->size += new->size;
			return new->size;

		}
		else if(new > traverse && new < traverse->next ) {

			new->next = traverse -> next;
			traverse->next = new;
			return new->size;
		}
			traverse = traverse -> next;

	}

#ifdef DEBUG_0

    printf("k_mem_dealloc: freeing 0x%x\r\n", (U32) ptr);
#endif /* DEBUG_0 */
    return RTX_OK;
}

int k_mem_count_extfrag(size_t size) {
	unsigned int count = 0;
	free_node * current = head;

	while (current != NULL) {
		if (current->size <= size) {
			count++;
		};
		current = current->next;
	};

#ifdef DEBUG_0
    printf("k_mem_extfrag: size = %d\r\n", size);
    printf("k_mem_extfrag: count = %d\r\n", count);
#endif /* DEBUG_0 */
    return count;
}

/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
