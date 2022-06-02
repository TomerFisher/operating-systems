#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

int main(int argc, char *argv[])
{
  int fd, sockfd, byte_index, buffer_size = 1024;
  uint64_t file_size, file_size_net, total_pc, total_pc_net;
  char send_buff[1024];
  struct stat st;
  struct sockaddr_in serv_addr;
  //validate that the correct number of command line arguments id passed
  if(argc != 4) {
    fprintf(stderr, "Error: invalid number of arguments\n");
    exit(1);
  }
  //validate that the file exists and readable
  fd = open(argv[3], O_RDONLY);
  if (fd < 0) {
    perror("Error");
    exit(1);
  }
  //create socket
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("Error");
    exit(1);
  }
  printf("socket success\n");
  //set server details
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(atoi(argv[2]));
  serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
  //connect socket to the target address
  if(connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
    perror("Error");
    exit(1);
  }
  printf("connect success\n");
  //get file's size
  stat(argv[3], &st);
  printf("write file size: %lu\n", st.st_size);
  file_size = st.st_size;
  //write file's size to the server
  file_size_net = htonl(file_size);
  if(write(sockfd, &file_size_net, sizeof(uint64_t)) < 0) {
    perror("Error");
    exit(1);
  }
  printf("write file size: %lu\n", file_size);
  //write file's data to the server
  for(byte_index=0; byte_index<file_size; byte_index+=buffer_size) {
    if(file_size-byte_index < 1024)
      buffer_size = file_size-byte_index;
    if(read(fd, send_buff, buffer_size) < 0) {
      perror("Error");
      exit(1);
    }
    if(write(sockfd, send_buff, buffer_size) < 0) {
      perror("Error");
      exit(1);
    }
  }
  printf("finish write file. byte_index: %d", byte_index);
  //read the number of printable characters from the server
  if(read(sockfd, &total_pc_net, sizeof(uint64_t)) < 0) {
    perror("Error");
    exit(1);
  }
  close(sockfd);
  total_pc = ntohl(total_pc_net);
  printf("# of printable characters: %lu\n", total_pc);
  exit(0);
}
