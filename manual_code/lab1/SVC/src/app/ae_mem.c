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

int test_mem(void) {
    void *p[4];
    int n;

    U32 result = 0;

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
    
    return result;
}
/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
