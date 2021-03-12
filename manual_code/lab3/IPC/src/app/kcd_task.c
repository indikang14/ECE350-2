/* The KCD Task Template File */
#include "Serial.h"
#include "k_msg.h"
#include "rtx.h"

// there are 62 alphanumeric case sensitive commands which can be registered
#define MAX_KEYS 62

// decimal value of ENTER, although I'm not sure this is how to do it
#define ENTER 10

// holds each key in the hashtable
struct item {
   task_t tid;   
   char key;
};

void display();
struct item * search( char key );
int insert( char key, task_t tid );
int hashCode(char char_key);

struct item hashTable[MAX_KEYS];
int keys = 0;

void display() {
	
   for(int i = 0; i < MAX_KEYS; i++) {
      if( hashTable[i].key != NULL )
         printf(" (%d, %c)", hashTable[i].tid, hashTable[i].key);
      else
         printf(" ~~ ");
   }
	
   printf("\n");
}

// returns 1 if successful, False if not
// int delete( struct item* item ) {

//    char key = item->key;

//    // get the hash 
//    int i = hashCode(key);

//    if( hashTable[i].key == key ) {
//       hashTable[i] = NULL; 
//       return TRUE;
//    } 
	
//    return FALSE;        
// }


struct item * search( char key ) {
   
   int i = hashCode(key);
   int j = 0;

   while( hashTable[i].key != key ) {
      ++i;
      ++j;

      i %= MAX_KEYS;

      if ( j > MAX_KEYS ) {
         return FALSE;
      }
   }  
   
   return &hashTable[i];
}

// return 0 if failure, 1 if successful
int insert( char key, task_t tid ) {

   if ( keys == MAX_KEYS ) {
      return FALSE;
   }

   struct item item;
   item.key = key;
   item.tid = tid;

   // get the hash 
   int i = hashCode(item.key);

   // move around to avoid collisions
   while( hashTable[i].key != NULL ) {
      ++i;
      i %= MAX_KEYS;
   }
	
   hashTable[i] = item;
   return TRUE;
}

int hashCode(char char_key) {

   int int_key = char_key - '0';

   return int_key % MAX_KEYS;
}

void kcd_task(void)
{
    // mbx_create() 
    if ( mbx_create( KCD_MBX_SIZE ) == 0 ) {
        return;
    } 

      // globals to the KCD
    size_t msg_hdr_size = sizeof(struct rtx_msg_hdr);
    U8 str_buf[64];
    U32 tot_chars_received = 0;
    U32 chars_received = 0;


    // I don't believe the KCD terminates but I may be wrong
    while ( TRUE ) {

        // Create a buf for command & keyboard
        U32 len = 0;
        U8 msg_buf[100]; // arbitrary long size, I don't want to deal with malloc
        U8 * msg;
        task_t sender_tid;

        // recv_msg() 
        while ( recv_msg( &sender_tid, &msg_buf, len) == -1 ) {
            printf("Failure in the KCD task");
        }

        /**
         * Format of buf 
         * 
         * | rtx_msg_header |
         * -----------------
         * |     data       |
         * ------------------
         * 
         * **/



        printf("KCD woke up from being blocked");
        struct rtx_msg_hdr * msg_hdr = (struct rtx_msg_hdr *) &msg_buf;
        msg = msg_buf + msg_hdr_size;

        if ( msg_hdr->type == KCD_REG ) {
            
            // phrasing was confusing
            if ( len > msg_hdr_size + 1 ){
                printf("Invalid KCD REG");
                continue;
            }

            insert( *(msg), sender_tid );

        } else if ( msg_hdr->type == KEY_IN ) { 

            chars_received = len - msg_hdr_size;
            
            if ( ( ( tot_chars_received + ( chars_received ) )  > 64 ) || ( *(msg + msg_hdr_size) != '%' && tot_chars_received == 0 ) ) {
               
               SER_PutStr(0, "Invalid command\n");

               // clear the string
               tot_chars_received = 0;
            } 
            
            else {

               // we may have multiple chars sent as per the specifiation, add each to str
               for ( int i = 0; i < chars_received; i++ ) {

                  str_buf[tot_chars_received + i] = *(msg + i);

                  if ( *(msg + i) == '\r' ) { // we've got the enter key

                     // do we have this command registered?
                     struct item * res = search( *(str_buf + 1) );

                     // would be cool if I could have a check_tid_function from indraj
                     if ( !res || !isTcbActive( res->tid ) ) {

                        SER_PutStr(0, "Command cannot be processed");
                     
                     } else { // send that puppy

                        size_t tot_len = msg_hdr_size + tot_chars_received;
                        char buff[tot_len];
                        struct rtx_msg_hdr * hdr_ptr = (void *) &buff;    

                        hdr_ptr->type = KCD_CMD;
                        hdr_ptr->length = tot_len; 

                        hdr_ptr += msg_hdr_size;

                        for ( int i = 0;  i < tot_chars_received; i++)
                           *(buff + i) = str_buf[i];

                        send_msg( res->tid, (void *)&buff );

                        // clear the current string buf
                        tot_chars_received = 0;
                     }

                  }
               }

               tot_chars_received += chars_received;
            }
        }
    }
}
