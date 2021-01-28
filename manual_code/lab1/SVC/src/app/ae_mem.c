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
 * @file        ae_mem.c
 * @brief       memory lab auto-tester
 *
 * @version     V1.2021.01
 * @authors     Yiqing Huang
 * @date        2021 JAN
 *
 *****************************************************************************/

#include "rtx.h"
#include "Serial.h"
#include "printf.h"

#define MAX_UINT 0xFFFFFFFF

int test_mem(void) {
    void *p[32];
    U32 result = 0;
    int n;

    p[0] = mem_alloc(8);

    if (p[0] != NULL) {
        result |= BIT(0);
    }

    p[1] = mem_alloc(8);

    if (p[1] != NULL && p[1] != p[0]) {
        result |= BIT(1);
    }

    mem_dealloc(p[0]);
    n = mem_count_extfrag(128);
    if (n == 1) {
        result |= BIT(2);
    }

    mem_dealloc(p[1]);
    n = mem_count_extfrag(128);
    if (n == 0) {
        result |= BIT(3);
    }



    // start of k_mem_count_extfrag testing

    p[0] = k_mem_alloc(8);
    p[1] = k_mem_alloc(16);
    p[2] = k_mem_alloc(32);
   	p[3] = k_mem_alloc(64);

   	if (k_mem_count_extfrag(256) == 0) {
   		result |= BIT(4);
   	}

   	k_mem_dealloc(p[0]);

   	if (k_mem_count_extfrag(16) == 0 && k_mem_count_extfrag(17) == 1) {
   		result |= BIT(5);
   	}

    k_mem_dealloc(p[2]);

    if (k_mem_count_extfrag(40) == 1 && k_mem_count_extfrag(41) == 2) {
    	result |= BIT(6);
    }

    k_mem_dealloc(p[1]);

    if (k_mem_count_extfrag(80) == 0 && k_mem_count_extfrag(81) == 1) {
    	result |= BIT(7);
    }

    k_mem_dealloc(p[3]);

    if (k_mem_count_extfrag(256) == 0) {
    	result |= BIT(8);
    }

    //testing throughput and heap utilization
    //start time is 1 second and we are running 3200 alloc/dealloc operations");
   for(int i =0; i < 40; i++)    {
    void *a[40];
    for (int j =0; j<40; j++) {
    	a[j] = mem_alloc(8);

    }

    for (int k = 0; k<40; k++) {
    	mem_dealloc(a[k]);
    }


   }
	//loop ends at 45 seconds, 3200/45 is 71. Our throughput is 71.


	//testing random de allocs to make sure it's coalescing
	void *j[8];

	 j[0] = mem_alloc(8);
     j[1] = mem_alloc(8);
     j[2] = mem_alloc(8);
   j[3] = mem_alloc(8);
    j[4] = mem_alloc(8);
    j[5] = mem_alloc(8);
    j[6] = mem_alloc(8);
    j[7] = mem_alloc(8);

    mem_dealloc(j[7]);
    if (k_mem_count_extfrag(128) == 0)
    { result  |= BIT(9);}
    mem_dealloc(j[5]);
    if(k_mem_count_extfrag(128) == 1)
    { result |= BIT(10);}
    mem_dealloc(j[6]);
    if (k_mem_count_extfrag(128) == 0){ result  |= BIT(11);}
    mem_dealloc(j[3]);
    if(k_mem_count_extfrag(128) == 1) { result |= BIT(12);}
    mem_dealloc(j[0]);
    if(k_mem_count_extfrag(128) == 2) { result |= BIT(13);}
    mem_dealloc(j[2]);
    if(k_mem_count_extfrag(32) == 1) {result |= BIT(14);}
    mem_dealloc(j[1]);
    if(k_mem_count_extfrag(48) == 0){ result |= BIT(15);}
    if(k_mem_count_extfrag(128) == 1){  result |= BIT(16);}
    mem_dealloc(j[4]);
    if (k_mem_count_extfrag(128) == 0){ result  |= BIT(17);}


	//start of allocation testing
    void *a[5];
    a[0] == NULL;
    a[1] == NULL;
    a[2] == NULL;
    a[3] == NULL;
    a[4] == NULL;
    //allocating 0 should return NULL
    a[0] = mem_alloc(0);
    if (a[0] == NULL) {
        result |= BIT(9);
    }
    //allocating size greater than available should return NULL
    a[1] = mem_alloc(MAX_UINT);
    if (a[1] == NULL) {
		result |= BIT(10);
	}
    /*
    //allocate size such that splitting wouldn't leave enough space for another free_node's overhead

    a[2] = mem_alloc(2); //allocates 8 + 2 bytes
    void *temp_p2 = a[2];
    a[3] = mem_alloc(32);
    mem_dealloc(a[2]);
    a[4] = mem_alloc(1); //allocates to the above 10 bytes, but not large enough to split
    mem_dealloc(p[7]);
	if (temp_p2 == p[8]) {
		mem_dealloc(p[8]);
		if (k_mem_count_extfrag(33) == 0) {
			result |= BIT(11);
		}
	} else {
		mem_dealloc(p[8]);
	}

    //allocate size equal to total size available
	p[9] = mem_alloc(2); //allocates 8 + 2 bytes
	void *temp_p3 = p[9];
	p[10] = mem_alloc(32);
	mem_dealloc(p[9]);
	p[11] = mem_alloc(2); //allocates to the above 10 bytes
	mem_dealloc(p[10]);
	if (temp_p3 == p[11]) {
		mem_dealloc(p[11]);
		if (k_mem_count_extfrag(33) == 0) {
			result |= BIT(12);
		}
	} else {
		mem_dealloc(p[11]);
	}

    //allocate size such that memory needs to be split
	p[12] = mem_alloc(1);
	p[13] = mem_alloc(2);
	void* temp_p = p[13];
	p[14] = mem_alloc(3);
	if (p[12] != NULL && p[13] != NULL && p[14] != NULL) {
		if (mem_dealloc(p[13]) == 0) {
			p[13] = mem_alloc(2);
			//test first fit algorithm
			if (p[13] == temp_p) {
				if (mem_dealloc(p[12]) == 0 && mem_dealloc(p[13]) == 0 && mem_dealloc(p[14]) == 0) {
					result |= BIT(13);
				}
			}
		}
	}
*/
    return result;
}
/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
