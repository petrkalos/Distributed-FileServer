#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>


#include "mynfs.h"


#define HELLO_PORT 36486
#define HELLO_GROUP "225.0.0.37"
#define MSGBUFSIZE 64000
#define TIMEOUT 4

typedef struct{
  char *fname;
  unsigned short fp;
  unsigned short seek;
}FILEC;

int filecounter=0;

FILEC *fcatal;

struct sockaddr_in addr;
socklen_t addrlen;
int fd;

char *fs_ip;
unsigned short fs_port; 

void ser_search(unsigned char *message,char *name){
  unsigned short type = 0x07;
  
  message[0] = 0 | type;
  message[1] = 0 | type>>8;
  
  strcpy((char *)&message[2],name);
}

void unser_search_res(unsigned char *message){
  int i,step;
  char name[64];
  char ip[16];
  unsigned short port;
  char prop[8];
  
  unsigned short count = (short)message[2];
  //printf("Number of regs found: %hd\n",count);
  for(i=0,step=0;i<count;i++,step+=90){
    strcpy(name,(char *)&message[step+4]);
    strcpy(ip,(char *)&message[step+68]);
    port = message[step+84] | message[step+85]<<8;
    strcpy(prop,(char *)&message[step+86]);
    
    //printf("Name: %s\tIP: %s\tPort: %hd\tProp: %s\n",name,ip,port,prop);
  }
  fs_ip = strdup(ip);
  fs_port = port;
}

void ser_discovery(unsigned char *message){
  unsigned short type=0x01;
  
  message[0] = 0 | type;
  message[1] = 0 | type>>8;
  
  char *service = "directory service";
  strcpy((char *)&message[2],service);
}

void unser_discovery_res(unsigned char *message){
  //unsigned short port = message[18] | message[19]<<8;
  //printf("Directory service run at %s:%d\n",&message[2],port);
}

int find_fs(){
  int nbytes;
  u_int yes=1;

  unsigned char msgbuf[MSGBUFSIZE];
  
  /* create what looks like an ordinary UDP socket */
  if ((fd=socket(AF_INET,SOCK_DGRAM,0)) < 0) {
    perror("socket");
    exit(0);
  }
  
  if (setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes)) < 0) {
    perror("Reusing ADDR failed");
    exit(1);
  }
  
  
  struct timeval tv;
  tv.tv_sec  = TIMEOUT;
  tv.tv_usec = 0;

    // Set Timeout for recv call
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,(char *)&tv,sizeof(struct timeval)); 


  /* set up destination address  */
  memset(&addr,0,sizeof(addr));
  addr.sin_family=AF_INET;
  addr.sin_addr.s_addr=inet_addr(HELLO_GROUP);
  addr.sin_port=htons(HELLO_PORT);
  
  
  //send discovery request to multicast
  do{
    //printf("Try to find server...\n");
    ser_discovery(msgbuf);
    if (sendto(fd,msgbuf,MSGBUFSIZE,0,(struct sockaddr *) &addr,sizeof(addr)) < 0) {
      perror("sendto");
      exit(1);
    }
  // multicast sends back the server ip 
    addrlen=sizeof(addr);
    if ((nbytes=recvfrom(fd,msgbuf,MSGBUFSIZE,0,(struct sockaddr *) &addr,&addrlen)) < 0){
	perror("Time out");
    }else{
      //printf("Server found\n");
      break;
    }
  }while(1);
  //unserialize the message - gets ip & port from directory service 
  unser_discovery_res(msgbuf);

  ser_search(msgbuf,"fileservice");
    
  do{
    if (sendto(fd,msgbuf,MSGBUFSIZE,0,(struct sockaddr *) &addr,sizeof(addr)) < 0) {
      perror("sendto");
      exit(1);
    }
    
    addrlen=sizeof(addr);
    if ((nbytes=recvfrom(fd,msgbuf,MSGBUFSIZE,0,(struct sockaddr *) &addr,&addrlen)) < 0){
      perror("Time out");
    }else{
      break;
    }
  }while(1);
    
  unser_search_res(msgbuf);
  
  return 1;
}

void ser_open_req(unsigned char *msgbuf,char *fname){
  unsigned short type = 0x01;
  
  msgbuf[0] = 0 | type;
  msgbuf[1] = 0 | type>>8;
  
  strcpy((char *)&msgbuf[2],fname);
}

int unser_open_res(unsigned char *msgbuf,char *fname){
  unsigned short type = msgbuf[0] | msgbuf[1]<<8;
  if(type == 0x02){
    unsigned short result = msgbuf[2] | msgbuf[3]<<8;
    unsigned short fp = msgbuf[4] | msgbuf[5]<<8;
    if(result == 0x01){
      fcatal = (FILEC *)realloc(fcatal,(filecounter+1)*sizeof(FILEC));
      fcatal[filecounter].fname = strdup(fname);
      fcatal[filecounter].fp = fp;
      fcatal[filecounter].seek = 0;
      filecounter++;
    }
    return fp;
  }
  return -1;
}

int mynfs_open(char *fname){
  unsigned char msgbuf[MSGBUFSIZE];
  int nbytes,fp;
   
  find_fs();
  

  memset(&addr,0,sizeof(addr));
  addr.sin_family=AF_INET;
  addr.sin_addr.s_addr=inet_addr(fs_ip);
  addr.sin_port=htons(fs_port);
  
  //printf("NFS IP = %s NFS = %d\n",fs_ip,fs_port);
  
  ser_open_req(msgbuf,fname);
  
  do{
      if (sendto(fd,msgbuf,MSGBUFSIZE,0,(struct sockaddr *) &addr,sizeof(addr)) < 0) {
	perror("Send to fileservice");
	exit(1);
      }
      
      addrlen=sizeof(addr);
      if ((nbytes=recvfrom(fd,msgbuf,MSGBUFSIZE,0,(struct sockaddr *) &addr,&addrlen)) < 0){
	  perror("Time out");
      }else{
	break;
      }
  }while(1);
  
  fp = unser_open_res(msgbuf,fname);
  return fp;
}

void ser_read_req(unsigned char *msgbuf,unsigned short fd,unsigned short seek,unsigned short len){
  unsigned short type = 0x03;
  
  msgbuf[0] = 0 | type;
  msgbuf[1] = 0 | type>>8;
  
  msgbuf[2] = 0 | fd;
  msgbuf[3] = 0 | fd>>8;
  
  msgbuf[4] = 0 | seek;
  msgbuf[5] = 0 | seek>>8;
  
  msgbuf[6] = 0 | len;
  msgbuf[7] = 0 | len>>8;
}

int unser_read_res(unsigned char *msgbuf,char buf[]){
  unsigned short type = msgbuf[0] | msgbuf[1]<<8;
  if(type == 0x04){
    unsigned short result = msgbuf[2] | msgbuf[3]<<8;
    if(result == 0x01){
      unsigned short len = msgbuf[4] | msgbuf[5]<<8;
      strncpy(buf,(char *)&msgbuf[6],len);
      return len;
    }else{
      return result;
    }
  }else{
    return -1;
  }
}

int mynfs_read(int fp, char buf[], int n){
  unsigned char msgbuf[MSGBUFSIZE];
  int i,nbytes;
  for(i=0;i<filecounter;i++){
      if(fcatal[i].fp == fp) break;
  }
  if(i==filecounter){
    printf("File cannot found\n");
    return -2;
  }
  
  ser_read_req(msgbuf,fp,fcatal[i].seek,n);
  do{
      if (sendto(fd,msgbuf,MSGBUFSIZE,0,(struct sockaddr *) &addr,sizeof(addr)) < 0) {
	perror("Send to fileservice");
	exit(1);
      }
      
      addrlen=sizeof(addr);
      if ((nbytes=recvfrom(fd,msgbuf,MSGBUFSIZE,0,(struct sockaddr *) &addr,&addrlen)) < 0){
	  perror("Time out");
      }else{
	break;
      }
  }while(1);
  
  return(unser_read_res(msgbuf,buf));
}

void ser_write_req(unsigned char *msgbuf,unsigned short fd,unsigned short seek,unsigned short len,char buf[]){
  unsigned short type = 0x05;
  
  msgbuf[0] = 0 | type;
  msgbuf[1] = 0 | type>>8;
  
  msgbuf[2] = 0 | fd;
  msgbuf[3] = 0 | fd>>8;
  
  msgbuf[4] = 0 | seek;
  msgbuf[5] = 0 | seek>>8;
  
  msgbuf[6] = 0 | len;
  msgbuf[7] = 0 | len>>8;
  
  strncpy((char *)&msgbuf[8],buf,len);
}

int unser_write_res(unsigned char *msgbuf){
  unsigned short type = msgbuf[0] | msgbuf[1]<<8;
  if(type == 0x06){
    unsigned short result = msgbuf[2] | msgbuf[3]<<8;
    return result;
  }else{
    return -1;
  }
}

void ser_version_req(unsigned char *msgbuf,unsigned int fd){
  
  unsigned short type = 0x07;
  
  msgbuf[0] = 0 | type;
  msgbuf[1] = 0 | type>>8;
  
  msgbuf[2] = 0 | fd;
  msgbuf[3] = 0 | fd>>8;
  
}

int unser_version_res(unsigned char *msgbuf){
  
  unsigned short type = msgbuf[0] | msgbuf[1]<<8;
  if(type == 0x08){
    unsigned int version  = msgbuf[2] | msgbuf[3]<<8;
    return version;
  }else{
    return -1;
  }
  
}

int mynfs_write(int fp, char buf[], int n){
  unsigned char msgbuf[MSGBUFSIZE];
  int i,nbytes;
  for(i=0;i<filecounter;i++){
      if(fcatal[i].fp == fp) break;
  }
  if(i==filecounter){
    printf("File cannot found\n");
    return -2;
  }
  
  ser_write_req(msgbuf,fp,fcatal[i].seek,n,buf);
  do{
      if (sendto(fd,msgbuf,MSGBUFSIZE,0,(struct sockaddr *) &addr,sizeof(addr)) < 0) {
	perror("Send to fileservice");
	exit(1);
      }
      
      addrlen=sizeof(addr);
      if ((nbytes=recvfrom(fd,msgbuf,MSGBUFSIZE,0,(struct sockaddr *) &addr,&addrlen)) < 0){
	  perror("Time out");
      }else{
	break;
      }
  }while(1);
  return(unser_write_res(msgbuf));
}

int mynfs_version(int fp){
  unsigned char msgbuf[MSGBUFSIZE];
  int nbytes;
  ser_version_req(msgbuf,fp);
  do{
      if (sendto(fd,msgbuf,MSGBUFSIZE,0,(struct sockaddr *) &addr,sizeof(addr)) < 0) {
	perror("Send to fileservice");
	exit(1);
      }
      
      addrlen=sizeof(addr);
      if ((nbytes=recvfrom(fd,msgbuf,MSGBUFSIZE,0,(struct sockaddr *) &addr,&addrlen)) < 0){
	  perror("Time out");
      }else{
	break;
      }
  }while(1);
  return(unser_version_res(msgbuf));
}

int mynfs_seek(int fp, int pos){
  int i;
  for(i=0;i<filecounter;i++){
      if(fcatal[i].fp == fp){
	fcatal[i].seek = pos;
	break;
      }
  }
  if(i==filecounter){
    printf("File cannot found\n");
    return -2;
  }
  return 1;
}

int mynfs_close(int fp){
  int i;
  for(i=0;i<filecounter;i++){
      if(fcatal[i].fp == fp){
	fcatal[i].fp = -1;
	fcatal[i].fname = "removed";
	fcatal[i].seek = -1;
	break;
      }
  }
  if(i==filecounter){
    printf("File cannot found\n");
    return -1;
  }
  return 1;
}


int mynfs_get_pos(int fp){
    int i;
  for(i=0;i<filecounter;i++){
      if(fcatal[i].fp == fp){
	 return(fcatal[i].seek);
      }
  }
  
return -1;
}
  
  