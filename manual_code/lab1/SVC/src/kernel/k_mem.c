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
	printf("################# K_MEM_ALLOC() #################\r\n");
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
    printf("prevnode pointer: 0x%x\r\n", prevnode);
    printf("currnode pointer: 0x%x\r\n", currnode);
    printf("currnode->size: %d\r\n", currnode->size);
    printf("real_req_size: %d\r\n", real_req_size);
    if (currnode == NULL) {
    	printf("################# END K_MEM_ALLOC() #################\r\n");
    	return NULL; //couldn't find a big enough free_node, allocation fails
    }

    //2. currnode is a freenode with size >= real_size, prevnode comes before currnode. handle > and = cases indiv.
    if (currnode->size == real_req_size) {
    	printf("CASE: free node size == total req size\r\n");
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
    	printf("################# END K_MEM_ALLOC() #################\r\n");
    	return (void*) ( ((unsigned char *)header_p) + sizeof(*header_p)); //return pointer to start of memory block
    } else if (currnode->size > real_req_size) {
    	//have to split the free_node.
    	if (currnode->size - real_req_size - sizeof(*currnode) < sizeof(*currnode) + 1) {
    		printf("CASE: freenode size > total req size, but not large enough to split\r\n");
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
			printf("################# END K_MEM_ALLOC() #################\r\n");
			return (void *) (((unsigned char *)header_p) + sizeof(*header_p)); //return pointer such that real_req_size number of bytes is at the end of the physical memory block
    	} else {
    		printf("CASE: freenode size > total req size, and large enough to split\r\n");
    		//enough bytes in the remaining free_node to split it.
    		//we'll allocate from the top of the block. therefore create new freenode below memory to be allocated
    		free_node *newnode = (free_node *)((unsigned char *)currnode + real_req_size);
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
			printf("################# END K_MEM_ALLOC() #################\r\n");
			return (void *)( ((unsigned char *)header_p) + sizeof(*header_p)); //return pointer to start of memory block
    	}
    }
    return NULL;
}

int k_mem_dealloc(void *ptr) { //ptr represents end of alloc header, start of allocated mem block

	/*
	 * replace header with free node
	 */
	if(ptr == NULL) { // if there is nothing to be freed
		return -1;
	}
	free_node *new; //replacing header
	header *current; //current header of ptr
	unsigned char *addr = (unsigned char *) ptr; //copy of ptr
	addr = addr - sizeof(header); //ptr arithmetic to get start of header
	printf("pointer argument provided: 0x%x\r\n", ptr);
	printf ("starting address of free block is 0x%x\r\n", addr);
	current = (header *) addr; //point to start of header
	new = (free_node *) addr; //treat the start of header as start of free node
	new->size = current->size; //replace header size with free size
	new->next = NULL;
	printf ("confirm starting address of free block is 0x%x\r\n", new);

	//at this point, new is the free node overhead for the freed block of memory
	//

	/**
	 * check for coalescing and merge blocks
	 ***/

	/**
	 * address ordered policy
	 */

	free_node* traverse = head;
	//if (head)

	//new has info on what used to be allocated
	if(head->next == NULL ) { //no fragments yet OR freeing after the whole memory region was allocated
		if((unsigned char *)new == (unsigned char *)head - new->size) { // if freed memory is adjacent to head merge them
			printf("in here because memory block adjacent to head \r\n");
			unsigned int totalSize = head->size + new->size;
			head = new;
			head->size = totalSize;
			head->next = NULL;
		}
		else if ((unsigned char*)new == (unsigned char *)head + head->size) {
			printf("in here because memory block adjacent to head \r\n");
			unsigned int totalSize = head->size + new->size;
			head -> size = totalSize;
		}
		else if(head > new) { // if not adjacent just free the block
			printf("in here because memory block freed at an earlier address than head \r\n");
			head = new;
			head->next = traverse;
		}
		else if(head < new ) { //if not adjacent and at a higher address than head
			printf("in here because memory block freed at a higher address than head \r\n");
			head->next = new;

		}
		return 0;
	}
	//if on the other hand it already has fragments
	//value of traverse = head, head has a non-NULL next value
	free_node* prevNode = NULL; // keep track of next and previous nodes of current free node that is being inserted into free list
	free_node* nextNode = NULL;
	while(traverse != NULL) { // traverse the list
		printf("traversing the list and coalescing \r\n");

        if (new < traverse) { //if freed memory block is at a lower address than current node aka traverse has to be head
			new->next = traverse; //new free node is at the start of the LL pointing to higher address
			head = new; // head is at the front of the LL
			nextNode = traverse; //nextNode is now the previous head
			if((unsigned char *) nextNode == (unsigned char *)new + new->size) { //if new head is adjacent to nextNode...
				unsigned int totalSize = head->size + nextNode->size;
				head->size = totalSize;
				head->next = nextNode -> next;
				return 0;

			}
			return 0;

		}
		else if(new > traverse && new < traverse->next ) {

			printf("new free block in the middle of of other two free blocks! \r\n");

			new->next = traverse -> next;
			nextNode = new->next;
			traverse->next = new;
			prevNode = traverse;

			if ((unsigned char*) new == (unsigned char*) prevNode + prevNode->size
					&& (unsigned char*) new !=  (unsigned char*) nextNode - new->size  ) {// if prevNode and current are adjacent only
				unsigned int totalSize = prevNode->size + new->size;
				prevNode->size = totalSize;
				prevNode->next = new->next;
				return 0;
			}
			else if ((unsigned char*) new == (unsigned char*) nextNode - new->size
					&& (unsigned char*) new !=  (unsigned char*) prevNode + prevNode->size) { // if nextNode and current are adjacent only
				unsigned int totalSize = nextNode->size + new->size;
								new->size = totalSize;
								new->next = nextNode->next;
								return 0;

			}

			else if ((unsigned char*) new == (unsigned char*) nextNode - new->size
					&& (unsigned char*) new ==  (unsigned char*) prevNode + prevNode->size) { // if all 3 nodes are adjacent

				unsigned int totalSize = prevNode->size + new->size + nextNode->size;
								prevNode->size = totalSize;
								prevNode->next = nextNode->next;
								return 0;



			}
			return 0;
		}

		else if(traverse->next == NULL && traverse != head) { //this is case where free memory is last block of the memory region
			unsigned int totalSize = traverse->size + new->size;
			traverse -> size = totalSize;
					return 0;
		}
			traverse = traverse -> next;

	}

	if(head == NULL) { // all memory is allocated in fragments and we are now  freeing
		head = new;
		head -> next = NULL;

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
