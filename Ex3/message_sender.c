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
  if (argc != 4) {
	  perror("Error: invalid number of arguments");
	  exit(1);
  }
  file_descriptor = open(argv[1], O_WRONLY);
  if (file_descriptor < 0) {
	  perror("Error: could not open file descriptor");
	  exit(1);
  }
  channel_id = atoi(argv[2]);
  if (ioctl(file_descriptor, MSG_SLOT_CHANNEL, channel_id) != SUCCESS) {
	  perror("Error: ioctl failed");
	  exit(1);
  }
  message_length = strlen(argv[3]);
  if (write(file_descriptor, argv[3], message_length) != message_length) {
	  perror("Error: write failed");
	  exit(1);
  }
  close(file_descriptor);
  return 0;
}
