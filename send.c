#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "link_emulator/lib.h"
#include <math.h>
#include "list.h"

#define HOST "127.0.0.1"
#define PORT 10000
#define FRAME_LEN 1396
#define MAXSIZE 1380

#define file 1
#define speed 2
#define delay 3

typedef struct {
  int type;
  int no;
  int parity;
  char payload[FRAME_LEN];
} frame;

// returns checksum of a string
int getCRC(char *string, int size){
  int sum = 0;
  for(int i = 0; i < size; i++)
  {
    for (int j = 0; j < 8; j++)
      sum += (string[i]&1 << j) >> j;
  }
  return sum;
}

// returns checksum of a message
int getMessageCRC(msg m){
  frame f;
  memset(&f, 0, sizeof(frame)); // intialize frame
  f = *((frame *)m.payload);
  int sum = 0;
  for(int i = 0; i < (m.len - 12); i++){
    for (int j = 0; j < 8; j++)  // for each bit
      sum += (f.payload[i]&1 << j) >> j;
  }
  return sum;   
}

// returns message containing file's name
msg sendFileName(char *filename){
  msg m;
  frame f;
  memset(&m, 0, sizeof(msg)); //    initialize message
  memset(&f, 0, sizeof(frame)); //    intialize frame
  f.type = 3; // type 3 for name
  f.no = -2; // first message for name
  f.parity = getCRC(filename, strlen(filename));
  memcpy(f.payload, filename, strlen(filename));
  m.len = 3*sizeof(int) + strlen(f.payload);  // message length  
  memcpy(m.payload, &f, m.len);
  return m;
}

// returns message containing file's length
msg sendFileLength(char *length){
  msg m;
  frame f;
  memset(&m, 0, sizeof(msg)); //    initialize message
  memset(&f, 0, sizeof(frame)); //    intialize frame
  f.type = 2; // type 2 for length
  f.no = -1; // second message for length
  f.parity = getCRC(length, strlen(length));
  memcpy(f.payload, length, strlen(length));
  m.len = 3*sizeof(int) + strlen(f.payload);  // message length  
  memcpy(m.payload, &f, m.len);
  return m;
}

// returns a new message, created according to parameters 
msg createMessage(char *readBuffer, int size, int secv_no){
  msg m;
  frame f;
  memset(&m, 0, sizeof(msg)); //    initialize message
  memset(&f, 0, sizeof(frame)); //    intialize frame
  f.type = 4; // type 4 for data
  f.no = secv_no;
  f.parity = getCRC(readBuffer, size);
  memcpy(f.payload, readBuffer, size);
  m.len = 3*sizeof(int) + size;  // message length  
  memcpy(m.payload, &f, m.len);
  return m;
}

// returns no. of ACK and checksum
int receiveACK(int *checksum){ 
  msg m;
  frame f;
  memset(&m, 0, sizeof(msg));
  memset(&f, 0, sizeof(frame));
  if (recv_message(&m) < 0){
    perror("Receive message");
    return 0;
  }
  else {
    f = *((frame *)m.payload);
  }
  *checksum = f.parity;
  return f.no;  
}


int main(int argc, char** argv){
  
  init(HOST, PORT);

//////////////////// WINDOW SIZE ///////////////////////////

  int SPEED = atoi(argv[speed]);
  int DELAY = atoi(argv[delay]);
  int WINDOW = (SPEED*DELAY*1000)/(1404*8);

  struct stat st;
  stat(argv[file], &st);
  int file_size_int = st.st_size;   // size of file
  int COUNT = file_size_int/MAXSIZE + 1;

/////////////////// SEND Filename ///////////////////////////

  msg messageBuffer[COUNT];
  memset(&messageBuffer, 0, COUNT*1404);
  int sentMessages[COUNT];
  memset(&sentMessages, 0, COUNT);
  int checksum = 0;
  
  msg m;
  m = sendFileName(argv[file]);
  send_message(&m);
  int ok = receiveACK(&checksum); // resend message
  if(ok != -2)
    send_message(&m);

/////////////////// SEND Length /////////////////////////////

  char length[11];
  sprintf(length, "%d", file_size_int);  // convert to string
  length[11] = '\0';
  m = sendFileLength(length);
  send_message(&m);
  ok = receiveACK(&checksum); // resend message
  if(ok != -1)
    send_message(&m);

/////////////////// CREATE Array ///////////////////////////

  int fd = open(argv[file], O_RDONLY);
  int i;
  int contentSize;
  char readBuffer[MAXSIZE];

  for (i = 0; i < COUNT; i++) {
      contentSize = read(fd, readBuffer,  MAXSIZE);
      if (contentSize > 0){
          m = createMessage(readBuffer, contentSize, i); // data message
          messageBuffer[i] = m;
      }
      else
        break;
  }

////////////////// SLIDING WINDOW ///////////////////////////

  struct Node* ack_seq = NULL; // initialize list for index

  // 1) send WINDOW messages
  for (i = 0; i < WINDOW; i++) {
      if (i < COUNT){
          send_message(&messageBuffer[i]);
          append(&ack_seq, i);
        }
      else
        break;
  }

  int currentACK;
  int expected;
  // 2) receive ACK and send messages
  for (i = 0; i < (COUNT-WINDOW); i++) {
      currentACK = receiveACK(&checksum);

      if(currentACK == ack_seq->data){        // message expected
        if(checksum == getMessageCRC(messageBuffer[currentACK])){ // message not corrupted
          removeFirstNode(&ack_seq);        
          send_message(&messageBuffer[WINDOW + i]);
          append(&ack_seq, WINDOW + i);           
        }
        else{                             // message corrupted
          expected = ack_seq->data;
          removeFirstNode(&ack_seq);
          append(&ack_seq, expected);        
          send_message(&messageBuffer[expected]);
          send_message(&messageBuffer[WINDOW + i]);
          append(&ack_seq, WINDOW + i);
        }
        
      }
      else{     // message not expected
        if(checksum == getMessageCRC(messageBuffer[currentACK])){ // message not corrupted
          expected = ack_seq->data;
          removeFirstNode(&ack_seq);
          deleteNode(&ack_seq, currentACK);
          append(&ack_seq, expected);
          append(&ack_seq, WINDOW + i);
          send_message(&messageBuffer[expected]);
          send_message(&messageBuffer[WINDOW + i]);
        }
        else{   // message corrupted
          expected = ack_seq->data;
          removeFirstNode(&ack_seq);
          deleteNode(&ack_seq, currentACK);
          append(&ack_seq, expected);
          append(&ack_seq, currentACK);
          send_message(&messageBuffer[expected]);
          send_message(&messageBuffer[currentACK]);
          i--;
        }
      }
      
  }

  // 3) receive last ACKs and resend messages
  for (i = 0; i < WINDOW; i++) {
      if(i < COUNT){
        currentACK = receiveACK(&checksum);
        if(currentACK == ack_seq->data){        // message expected
          if(checksum == getMessageCRC(messageBuffer[currentACK])) // message not corrupted
              removeFirstNode(&ack_seq);
          else{  // message corrupted
            expected = ack_seq->data;
            removeFirstNode(&ack_seq);
            send_message(&messageBuffer[expected]);
            append(&ack_seq, expected);
          }  
        }
        else{     // message not expected
          if(checksum == getMessageCRC(messageBuffer[currentACK])){ // message not corrupted
              expected = ack_seq->data;
              removeFirstNode(&ack_seq);
              deleteNode(&ack_seq, currentACK);
              append(&ack_seq, expected);
              send_message(&messageBuffer[expected]);
          }
          else{   // message corrupted
            expected = ack_seq->data;
            removeFirstNode(&ack_seq);
            deleteNode(&ack_seq, currentACK);
            append(&ack_seq, expected);
            append(&ack_seq, currentACK);
            send_message(&messageBuffer[expected]);
            send_message(&messageBuffer[currentACK]);
            i--;
          }
          
        }

      }

    }

  close(fd); // close file descriptor

  expected = receiveACK(&checksum); // receive stop ack

  return 0;
}