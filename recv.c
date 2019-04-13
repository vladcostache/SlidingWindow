#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "link_emulator/lib.h"

#define HOST "127.0.0.1"
#define PORT 10001
#define FRAME_LEN 1396
#define MAXSIZE 1380

typedef struct {
  int type;
  int no;
  int parity;
  char payload[FRAME_LEN];
} frame;

// sends ACK with no. and checksum
msg sendAck(int no, int checksum){
  msg m;
  frame f;
  memset(&m, 0, sizeof(msg));
  memset(&f, 0, sizeof(frame));
  f.type = 1; //    type 1 for ACK
  f.no = no;
  f.parity = checksum;
  char str_ack[4] = "ack";
  memcpy(f.payload, str_ack, strlen(str_ack));
  m.len = 3*sizeof(int) + strlen(f.payload); //  message length 
  memcpy(m.payload, &f, m.len);
  return m;
}

// returns file's name
char *receiveFileName(){
  msg m;
  frame f;
  memset(&m, 0, sizeof(msg));
  memset(&f, 0, sizeof(frame));

  if (recv_message(&m) < 0){
    perror("Receive message");
    //return;
  }
  f = *((frame *)m.payload);
  static char filename[100];
  strcpy(filename, f.payload);
  return filename;  
}

// returns file's length
int receiveFileLength(){
  msg m;
  frame f;
  memset(&m, 0, sizeof(msg));
  memset(&f, 0, sizeof(frame));

  if (recv_message(&m) < 0){
    perror("Receive message");
    //return;
  }
  f = *((frame *)m.payload);
  int length = atoi(f.payload);
  return length;  
}

// returns message received, no. and checksum
msg receiveFrame(int *no, int *checksum){
  msg m;
  frame f;

  memset(&m, 0, sizeof(msg));
  memset(&f, 0, sizeof(frame));

  if (recv_message(&m) < 0){
    perror("Receive message");
  }
  f = *((frame *)m.payload);
  *no = f.no;
  *checksum = f.parity;
  return m;
}

// returns checksum of a message
int getMessageCRC(msg m){
  frame f;
  memset(&f, 0, sizeof(frame)); //    intialize frame
  f = *((frame *)m.payload);
  int sum = 0;
  for(int i = 0; i < (m.len - 12); i++){
    for (int j = 0; j < 8; j++)
      sum += (f.payload[i]&1 << j) >> j;
  }
  return sum;    
}


int main(int argc, char** argv){
  
  init(HOST,PORT);
  msg t;
  memset(&t, 0, sizeof(msg));

/////////////////  RECEIVE Filename //////////////

  char *filename;
  filename = receiveFileName();
  char recv_filename[50] = "recv_";
  strcat(recv_filename, filename);
  int fd = open(recv_filename, O_WRONLY | O_CREAT | O_TRUNC, 00644);

//////////////////  SEND ack   ///////////////////
  
  t = sendAck(-2, 0);
  send_message(&t);

/////////////////  RECEIVE Length ////////////////

  int length = receiveFileLength();
  printf("leng %d\n", length);
  t = sendAck(-1, 0);
  send_message(&t);

/////////////////  CALCULATE COUNT ///////////////

  int COUNT = length/MAXSIZE + 1;
  int bufferLEN = COUNT;

/////////////////// CREATE Array /////////////////

  msg messageBuffer[COUNT];
  memset(&messageBuffer, 0, COUNT*1404);

/////////////////  SEND MESSAGE  /////////////////

  int i;
  int secv_no;
  frame f;
  memset(&t, 0, sizeof(msg));
  int no_ack = 0, checksum = 0;

  while(1) {
    t = receiveFrame(&secv_no, &checksum);  // receive message & checksum
    messageBuffer[secv_no] = t;   // add to buffer
    if(checksum != getMessageCRC(t)) // if the message is corrupted 
      COUNT++;
    t = sendAck(secv_no, getMessageCRC(t));   // send ACK for current message
    send_message(&t);
    no_ack++;               // increment no. of sent ACK's
    if (no_ack == COUNT)    // stop condition
      break;
  }

////////////////// WRITE FILE ////////////////////
  
  for (i = 0; i < bufferLEN; i++) {
      memset(&f, 0, sizeof(frame));
      f = *((frame *)messageBuffer[i].payload);
      write(fd, f.payload, (messageBuffer[i].len - 12));
  }

  
  close(fd); // close file descriptor

  t = sendAck(-1, 0); // send final ack
  send_message(&t);
 
  return 0;
}
