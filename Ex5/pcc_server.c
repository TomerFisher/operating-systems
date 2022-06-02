#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <assert.h>

uint64_t ppc_total[95] = {0};

int main(int argc, char *argv[])
{
  int listenfd, connfd, byte_index, buffer_size;
  char recv_buff[1024], *data_buff;
  uint64_t file_size, file_size_net, total_pc, total_pc_net;
  struct sockaddr_in serv_addr;
  //validate that the correct number of command line arguments id passed
  if(argc != 2) {
    fprintf(stderr, "Error: invalid number of arguments\n");
    exit(1);
  }
  //create socket
  listenfd = socket( AF_INET, SOCK_STREAM, 0);
  if (listenfd < 0) {
    perror("Error");
    exit(1);
  }
  printf("socket success\n");
  //set server details
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(atoi(argv[1]));
  //bind server
  if(0 != bind(listenfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr))) {
    perror("Error");
    exit(1);
  }
  printf("bind success\n");
  //listen for connections
  if(0 != listen(listenfd, 10)) {
    perror("Error");
    exit(1);
  }
  printf("listen success\n");

  while(1) {
    //accept connection
    connfd = accept(listenfd, (struct sockaddr*) NULL, NULL);
    if(connfd < 0) {
      perror("Error");
      exit(1);
    }
    printf("accept connection\n");
    //read file's size from the client
    if(read(connfd, &file_size_net, sizeof(uint64_t)) < 0) {
      perror("Error");
      exit(1);
    }
    file_size = ntohl(file_size_net);
    printf("read file size: %lu\n", file_size);
    //read file's data from the client
    buffer_size = 1024;
    data_buff = malloc(file_size);
    for(byte_index=0; byte_index<file_size; byte_index+=buffer_size) {
      if(file_size-byte_index < 1024)
        buffer_size = file_size-byte_index;
      if(read(connfd, recv_buff, buffer_size) < 0) {
        perror("Error");
        exit(1);
      }
      strcpy(data_buff+byte_index, recv_buff);
    }
    //calc total printable characters
    total_pc = 0;
    for(int byte_index=0; byte_index<file_size; byte_index++) {
      if(32 <= data_buff[byte_index] && data_buff[byte_index] <= 126) {
        total_pc++;
        ppc_total[(int)data_buff[byte_index]]++;
      }
    }
    printf("finish write file. byte_index: %d", byte_index);
    //write total printable characters to the client
    total_pc_net = htonl(total_pc);
    if(write(connfd, &total_pc_net, sizeof(uint64_t)) < 0) {
      perror("Error");
      exit(1);
    }
    printf("write total pc: %lu\n", total_pc);
  }
}
