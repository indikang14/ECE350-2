/**
 * @file:   k_msg.c
 * @brief:  kernel message passing routines
 * @author: Yiqing Huang
 * @date:   2020/10/09
 */

#include "k_msg.h"
#include "k_task.h"

#ifdef DEBUG_0
#include "printf.h"
#endif /* ! DEBUG_0 */

int isTcbActive(TCB* traverse , int searchTid) {

	traverse = TCBhead;

	while (traverse->tid != searchTid) {
	    	if(traverse == NULL) {
	    		return RTX_ERR;
	    	}
	    	traverse = traverse->next;
	    }
	printf("search tid is : %d \r\n", traverse->tid);

	return RTX_OK;


}

int cq_isEmpty(CQ* circq) {
    //returns 1 = true, 0 = false
    return (circq->head == NULL);
}

mbx_metamsg* getMetaMessage(U8* mm_p, CQ *circq) {
    if (mm_p == NULL) return NULL;
    //if the location of the pointer is too close to the end of the mailbox memory block, need special handling
    // specifically, within sizeof(mbx_metamsg) to edge
    U8 *start = circq->memblock_p;
    U8 *end = start + circq->size;
    U32 length;

    mbx_metamsg *ret;
    U8 *temp = mm_p;
    if (mm_p + sizeof(mbx_metamsg) > end) {
        //not enough bytes to get message metadata + header; have to go byte by byte to build the mbx_metamessage not including data
    	kernelOwnedMemory = 1;
    	U8 *meta_header = k_mem_alloc(sizeof(mbx_metamsg));
    	kernelOwnedMemory = 0;
        U8 *readaddr;
        for (int i = 0; i < sizeof(mbx_metamsg); i++) {
            readaddr = temp + i;
            readaddr = (readaddr >= circq->memblock_p + circq->size ? circq->memblock_p + ((U64)readaddr % (U64)(circq->memblock_p + circq->size)) : readaddr);
            meta_header[i] = *readaddr;
        }
        //meta_header has everything but not the data
        length = ((mbx_metamsg *)meta_header)->msg.header.length;
        kernelOwnedMemory = 1;
        ret = k_mem_alloc(sizeof(mbx_metamsg) + length - sizeof(RTX_MSG_HDR));
        kernelOwnedMemory = 0;
        if (ret == NULL) return NULL;
        mbx_message *msg_p = (mbx_message *) &(ret->msg);
        RTX_MSG_HDR *hdr_p = (RTX_MSG_HDR *) &(msg_p->header);
        ret->senderTID = ((mbx_metamsg *)meta_header)->senderTID;
        hdr_p->length = length;
        hdr_p->type = ((mbx_metamsg *)meta_header)->msg.header.type;
        kernelOwnedMemory = 1;
        k_mem_dealloc(meta_header);
        kernelOwnedMemory = 0;
    } else {
        //can get mbx_metamessage, not including data
        mbx_metamsg tempmm = *((mbx_metamsg *) mm_p);
        length = tempmm.msg.header.length;
        kernelOwnedMemory = 1;
        ret = k_mem_alloc(sizeof(mbx_metamsg) + length - sizeof(RTX_MSG_HDR));
        kernelOwnedMemory = 0;
        if (ret == NULL) return NULL;
        mbx_message *msg_p = (mbx_message *) &(ret->msg);
        RTX_MSG_HDR *hdr_p = (RTX_MSG_HDR *) &(msg_p->header);
        ret->senderTID = tempmm.senderTID;
        hdr_p->length = length;
        hdr_p->type = tempmm.msg.header.type;
    }
    //last thing to get is data, which might wrap around to start of array
    temp = mm_p + sizeof(mbx_metamsg);
    for (int i = 0; i < length - sizeof(RTX_MSG_HDR); i++) {
        U8 *readaddr = temp + i;
        readaddr = (readaddr >= circq->memblock_p + circq->size ? circq->memblock_p + ((U64)readaddr % (U64)(circq->memblock_p + circq->size)) : readaddr);
        ret->msg.data[i] = *readaddr;
    }
    return ret; //after calling this function, need to free memory.
}

int cq_enqueue(mbx_metamsg *metamsg, CQ *receiver) {
    if (receiver->memblock_p == NULL) return -1; //mailbox not created
    //error if not enough space
    U64 bytesrequired = sizeof(mbx_metamsg) + metamsg->msg.header.length - sizeof(RTX_MSG_HDR);
    printf("enqueue -- bytes required: %lu\n", bytesrequired);
    if (bytesrequired > receiver->remainingSize) return -1; //not enough space in mailbox for new message
    if (!cq_isEmpty(receiver)) {
        //get tail, add bytes corresponding to size of tail metadata + message
        mbx_metamsg *tail = getMetaMessage((U8 *) receiver->tail, receiver);
        U8 *temp = (U8 *) receiver->tail + sizeof(mbx_metamsg) + tail->msg.header.length - sizeof(RTX_MSG_HDR);
        kernelOwnedMemory = 1;
        k_mem_dealloc(tail);
        kernelOwnedMemory = 0;
        //wrap back if necessary
        temp = (temp >= receiver->memblock_p + receiver->size ? receiver->memblock_p + ((U64)temp % (U64)(receiver->memblock_p + receiver->size)) : temp);
        //now temp is at address where we want to start copying bytes from message to mailbox
        //ie temp is at the new tail for our cq

        kernelOwnedMemory = 1;
        mbx_metamsg *mm_p = k_mem_alloc(bytesrequired);
        kernelOwnedMemory = 0;
        if(mm_p == NULL) return -1;
        mbx_message *msg_p = (mbx_message *) &(mm_p->msg);
        RTX_MSG_HDR *hdr_p = (RTX_MSG_HDR *) &(msg_p->header);
        mm_p->senderTID = metamsg->senderTID;
        hdr_p->length = metamsg->msg.header.length;
        hdr_p->type = metamsg->msg.header.type;
        for (int i = 0; i < hdr_p->length - sizeof(RTX_MSG_HDR); i++) {
            msg_p->data[i] = metamsg->msg.data[i];
        }
        U8 *mm_pu8 = (U8 *) mm_p;
        //now we have the actual message in mm_p
        //copy mm_p bytes individually to mailbox
        for (int i = 0; i < bytesrequired; i++) {
            U8 *writeaddr = temp + i;
            //check for overflow on writeaddr, wrap back if necessary
            writeaddr = (writeaddr >= receiver->memblock_p + receiver->size ? receiver->memblock_p + ((U64)writeaddr % (U64)(receiver->memblock_p + receiver->size)) : writeaddr);
            *writeaddr = mm_pu8[i];
        }
        //update cq struct values
        receiver->tail = (mbx_metamsg *) temp;
        receiver->remainingSize -= bytesrequired;
        kernelOwnedMemory = 1;
        k_mem_dealloc(mm_p);
        kernelOwnedMemory = 0;
    } else { //mailbox is empty, enough bytes to write the message in mailbox, so we can just use struct pointers to set that memory
        mbx_metamsg *temp = (mbx_metamsg *) receiver->memblock_p;
        mbx_message *msg_p = (mbx_message *) &(temp->msg);
        RTX_MSG_HDR *hdr_p = (RTX_MSG_HDR *) &(msg_p->header);
        temp->senderTID = metamsg->senderTID;
        hdr_p->length = metamsg->msg.header.length;
        hdr_p->type = metamsg->msg.header.type;
        for (int i = 0; i < hdr_p->length - sizeof(RTX_MSG_HDR); i++) {
            msg_p->data[i] = metamsg->msg.data[i];
        }
        //copied data, now update cq struct values
        receiver->head = (mbx_metamsg *) temp;
        receiver->tail = (mbx_metamsg *) temp;
        receiver->remainingSize -= bytesrequired;
    }
    return 0;
}

mbx_metamsg* cq_dequeue() {
    if (gp_current_task->mbx_cq.memblock_p == NULL) return NULL; //mailbox not created
    //empty
    if (cq_isEmpty(&(gp_current_task->mbx_cq))) return NULL;
    printf("dequeueing...\n");
    U8 *mm_pu8 = (U8 *) gp_current_task->mbx_cq.head;
    //one element
    if (gp_current_task->mbx_cq.head == gp_current_task->mbx_cq.tail) {
        gp_current_task->mbx_cq.head = NULL;
        gp_current_task->mbx_cq.tail = NULL;
        gp_current_task->mbx_cq.remainingSize = gp_current_task->mbx_cq.size;
        return getMetaMessage(mm_pu8, &(gp_current_task->mbx_cq));
    } else { //more than one element in cq
        //move front to next message in mailbox
        mbx_metamsg *head = getMetaMessage((U8 *) gp_current_task->mbx_cq.head, &(gp_current_task->mbx_cq));
        U64 bytesfreed = sizeof(mbx_metamsg) + head->msg.header.length - sizeof(RTX_MSG_HDR);
        kernelOwnedMemory = 1;
        k_mem_dealloc(head);
        kernelOwnedMemory = 0;
        gp_current_task->mbx_cq.head = (void *) (mm_pu8 + bytesfreed);
        gp_current_task->mbx_cq.remainingSize += bytesfreed;
        return getMetaMessage(mm_pu8, &(gp_current_task->mbx_cq));
    }
}

int k_mbx_create(size_t size) {
#ifdef DEBUG_0
    printf("k_mbx_create: size = %d\r\n", size);
#endif /* DEBUG_0 */

    //check that size > min_size
    if (size < MIN_MBX_SIZE) return -1;
    //check that calling task doesn't already have a mailbox
    if (gp_current_task->mbx_cq.memblock_p != NULL) return -1;

    //allocate memory for mailbox
    kernelOwnedMemory = 1;
    U8 *p_mbx = k_mem_alloc(size);
    kernelOwnedMemory = 0;
    //check that allocation was successful
    if (p_mbx == NULL) return -1;
    gp_current_task->mbx_cq.memblock_p = p_mbx;
    gp_current_task->mbx_cq.size = size;
    gp_current_task->mbx_cq.remainingSize = size;
    return 0;
}

int k_send_msg(task_t receiver_tid, const void *buf) {
#ifdef DEBUG_0
    printf("k_send_msg: receiver_tid = %d, buf=0x%x\r\n", receiver_tid, buf);
#endif /* DEBUG_0 */
    //reading length and type of buf
    RTX_MSG_HDR * readHdr = (RTX_MSG_HDR *) buf ;
    //(RTX_MSG_HDR *) buf->length;

    printf("size of message + header = 0x%x \r\n", readHdr->length);

    //length of msg has to be a valid
    if(buf == NULL || readHdr->length < MIN_MSG_SIZE){
        	return RTX_ERR;
        }

    TCB* temp;
    //check if receiver TCB is active
    if(isTcbActive(temp, receiver_tid) != RTX_OK) {
    	return RTX_ERR;
    }

    printf("temp tid and address is : %d and 0x%x \r\n", temp->tid, temp);

    //check if receiving tcb has a mailbox (maybe a flag?)
//    if(temp->mbx_cq == NULL) {
//    	return RTX_ERR;
//    }
    //if receiving task state is blocked, unblock it and preempt the scheduler if prio is higher than current task
    if(temp->state == BLK_MSG) {

    	//thread_changed_p = temp;
    	//thread_changed_event = "PRIORITY";

    	TCB* p_tcb_old = gp_current_task;

    	gp_current_task = scheduler();

    	    printf("address of current task: 0x%x \r\n", gp_current_task);

    	    if ( gp_current_task == NULL  ) {
    	    	gp_current_task = p_tcb_old;        // revert back to the old task
    	    	//return;
    	    }

    	    // at this point, gp_current_task != NULL and p_tcb_old != NULL
    	    if (gp_current_task != p_tcb_old) {
    	    	gp_current_task->state = RUNNING;   // change state of the to-be-switched-in  tcb
    	        p_tcb_old->state = READY;           // change state of the to-be-switched-out tcb
    	        //g_num_active_tasks--;				//number of active tasks decreases
    	        k_tsk_switch(p_tcb_old);            // switch stacks
    	    }
    	}

    int sizeOfData =  (readHdr->length - sizeof(RTX_MSG_HDR) );

    printf("size of just Data: %d", sizeOfData);

    	mbx_metamsg *metamsg1 = k_mem_alloc(sizeof(mbx_metamsg) + sizeOfData);
        mbx_message *msg1 = (mbx_message *) &(metamsg1->msg);
        RTX_MSG_HDR *header1 = (RTX_MSG_HDR *) &(msg1->header);
        header1->length = sizeof(RTX_MSG_HDR) + sizeOfData; //size of string
        header1->type = readHdr->type;
        (U8 *)buf += sizeof(RTX_MSG_HDR);
        char * tempData = (char *)buf;


        for (int i = 0; i < header1->length - sizeof(RTX_MSG_HDR); i++) {
            msg1->data[i] = tempData[i];
        }
        metamsg1->senderTID = gp_current_task->tid;

        //address of mailbox of the receiver
        if (cq_enqueue(metamsg1, &(temp->mbx_cq)) != 0) {
                k_mem_dealloc(metamsg1);
                printf("Error in enqueue\n");
                return RTX_ERR;
            } else {
            	k_mem_dealloc(metamsg1);
                printf("enqueue successful\n");
                return RTX_OK;
            }

}

int k_recv_msg(task_t *sender_tid, void *buf, size_t len) {
#ifdef DEBUG_0
    printf("k_recv_msg: sender_tid  = 0x%x, buf=0x%x, len=%d\r\n", sender_tid, buf, len);
#endif /* DEBUG_0 */
    // if no mailbox created or null buffer
    if (buf == NULL || gp_current_task->mbx_cq.memblock_p == NULL) return -1;

    mbx_metamsg *metamsg = cq_dequeue();

    if (metamsg == NULL) { // mailbox is empty
        // change task state to BLK_MSG and run other task
        p_tcb_old = gp_current_task;
        p_tcb_old.state = BLK_MSG;

        thread_changed_event = "EXITED";
        thread_changed_p = p_tcb_old;
        gp_current_task = scheduler();
        gp_current_task->state = RUNNING
        k_tsk_switch(p_tcb_old);
        return -1;
    }

    if (metamsg->msg.header.length > len) {
        // not big enough buffer
        kernelOwnedMemory = 1;
        k_mem_dealloc(metamsg);
        kernelOwnedMemory = 0;
        return -1;
    }

    if (sender_tid != NULL) {
        *sender_tid = metamsg->senderTID;
    }

    for (int i = 0; i < metamsg->msg.header.length - sizeof(RTX_MSG_HDR); i++) {
        *buf[i] = metamsg->msq.data[i];
    }

    kernelOwnedMemory = 1;
    k_mem_dealloc(metamsg);
    kernelOwnedMemory = 0;

    return 0;
}

int k_mbx_ls(task_t *buf, int count) {
#ifdef DEBUG_0
    printf("k_mbx_ls: buf=0x%x, count=%d\r\n", buf, count);
#endif /* DEBUG_0 */
    return 0;
}
