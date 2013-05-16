#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include "cmynfs.h"

#define MUL 10000
#define SIZE 8

int main(int argc,char *argv[]){
  char mina[2000];
  int fd = cmynfs_open("test");
  
  cmynfs_write(fd,mina,5000);
  
  cmynfs_read(fd,mina,2000);
  
  
  cmynfs_seek(fd,1500);
  cmynfs_read(fd,mina,2000);
  
  cmynfs_seek(fd,2000);
  cmynfs_read(fd,mina,2000);
  
  printf("%s\n",mina);
  
  return 0;
}