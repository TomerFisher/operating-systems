#include "message_slot.h"    

#include <fcntl.h>      /* open */ 
#include <unistd.h>     /* exit */
#include <sys/ioctl.h>  /* ioctl */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv)
{
  int file_descriptor, message_length;
  unsigned long channel_id;
  char buffer[BUF_LEN];
  if (argc != 3) {
	  perror("Error: invalid number of arguments");
	  exit(1);
  }
  file_descriptor = open(argv[1], O_RDONLY);
  if (file_descriptor < 0) {
	  perror("Error: could not open file descriptor");
	  exit(1);
  }
  channel_id = atoi(argv[2]);
  if (ioctl(file_desc, MSG_SLOT_CHANNEL, channel_id) != SUCCESS) {
	  perror("Error: ioctl failed");
	  exit(1);
  }
  message_length = read(file_desc, buffer, BUF_LEN);
  if (message_length < 0) {
	  perror("Error: read failed");
	  exit(1);
  }
  close(file_descriptor);
  if (write(1, buffer, message_length) < 0) {
	  perror("Error: write message to stdout failed");
  }
  return 0;
}
