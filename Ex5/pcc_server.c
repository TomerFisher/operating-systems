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
#include <signal.h>

uint64_t pcc_total[95] = {0};
int active_server = 1, active_connection = 0;

void close_server() {
  int character_index;
  for(character_index=0; character_index < 95; character_index++) {
    printf("char '%c' : %lu times\n", character_index+32, pcc_total[character_index]);
  }
  exit(0);
}

void sigint_handler() {
  if(active_connection != 1)
    close_server();
  active_server = 0;
}

int main(int argc, char *argv[])
{
  int listenfd, connfd, byte_index, file_index, buffer_size, rc, character_index;
  char recv_buff[1024];
  uint64_t file_size, file_size_net, total_pc, total_pc_net;
  uint64_t pcc_total_per_conn[95];
  struct sockaddr_in serv_addr;
  struct sigaction sigint;
  //validate that the correct number of command line arguments id passed
  if(argc != 2) {
    fprintf(stderr, "Error: invalid number of arguments\n");
    exit(1);
  }
  //initiate SIGINT handler
  sigint.sa_handler = &sigint_handler;
  sigemptyset(&sigint.sa_mask);
  sigint.sa_flags = SA_RESTART;
  if(sigaction(SIGINT, &sigint, 0) != 0) {
    perror("Error");
    exit(1);
  }
  //create socket
  listenfd = socket( AF_INET, SOCK_STREAM, 0);
  if(listenfd < 0) {
    perror("Error");
    exit(1);
  }
  //set address to be reuseable
  if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
    perror("Error");
    exit(1);
  }
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
  //listen for connections
  if(0 != listen(listenfd, 10)) {
    perror("Error");
    exit(1);
  }

  while(active_server) {
    //accept connection
    connfd = accept(listenfd, (struct sockaddr*) NULL, NULL);
    if(connfd < 0) {
      perror("Error");
      exit(1);
    }
    active_connection = 1;
    //read file's size from the client
    rc = read(connfd, &file_size_net, sizeof(uint64_t));
    if(rc <= 0) {
      if((!rc) || errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE) {
        perror("Error");
        close(connfd);
        active_connection = 0;
        continue;
      }
      else {
        perror("Error");
        exit(1);
      }
    }
    file_size = ntohl(file_size_net);
    //reset statistics per connection (set all elements in pcc_total_per_conn to zero)
    for(character_index=0; character_index < 95; character_index++) {
      pcc_total_per_conn[character_index] = 0;
    }
    //read file's data from the client
    buffer_size = 1024;
    total_pc = 0;
    for(file_index=0; file_index<file_size; file_index+=buffer_size) {
      if(file_size-file_index < 1024)
        buffer_size = file_size-file_index;
      rc = read(connfd, recv_buff, buffer_size);
      if(rc <= 0) {
        if((!rc) || errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE) {
          perror("Error");
          close(connfd);
          active_connection = 0;
          break;
        }
        else {
          perror("Error");
          exit(1);
        }
      }
      //calc total printable characters per connection
      for(byte_index=0; byte_index<buffer_size; byte_index++) {
        if(32 <= recv_buff[byte_index] && recv_buff[byte_index] <= 126) {
          total_pc++;
          pcc_total_per_conn[(int)recv_buff[byte_index]-32]++;
        }
      }
    }
    //check if there was TCP connection error or unexpeted connection lost
    if(rc <= 0)
      continue;
    //write total printable characters to the client
    total_pc_net = htonl(total_pc);
    rc = write(connfd, &total_pc_net, sizeof(uint64_t));
    if(rc <= 0) {
      if((!rc) || errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE) {
        perror("Error");
        close(connfd);
        active_connection = 0;
        continue;
      }
      else {
        perror("Error");
        exit(1);
      }
    }
    close(connfd);
    active_connection = 0;
    //calc total printable characters of all connections
    for(character_index=0; character_index < 95; character_index++) {
      pcc_total[character_index] += pcc_total_per_conn[character_index];
    }
  }
  close_server();
}
