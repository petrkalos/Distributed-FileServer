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
#include <sys/stat.h>

#define HELLO_PORT 36486
#define HELLO_GROUP "225.0.0.37"
#define MSGBUFSIZE 64000
#define TIMEOUT 4

int filecounter=0;
char *DBFILE = "files.txt";
char *servicename;

//updates the file version

void inc_vers(int fd){
  printf("Inc version\n");
  char name[128];
  unsigned short id;
  unsigned int ver;
  FILE *fp,*fp1;
  fp = fopen(DBFILE,"r");
  fp1 = fopen("test","w");
  while(fscanf(fp,"%s %hd %u\n",name,&id,&ver)!=EOF){
    if(id==fd){ 
      ver++;
    }
    fprintf(fp1,"%s %hd %u\n",name,id,ver);
  }
  fclose(fp);
  remove(DBFILE);
  rename("test",DBFILE);
  fclose(fp1);
}

// returns the file version

int unser_get_vers(unsigned char *message){
  
  char name[128];
  unsigned short id;
  unsigned int ver;
  FILE *fp;
  unsigned short fd   = message[2] | message[3]<<8;
  
  fp = fopen(DBFILE,"r");
  
  while(fscanf(fp,"%s %hd %u\n",name,&id,&ver)!=EOF){
    if(id==fd){ 
      break;
    }
   
  }
 return(ver);
}

void ser_get_vers(unsigned char *message,unsigned int ver){
  unsigned short type=8;
    
  message[0] = 0 | type;
  message[1] = 0 | type>>8;
  message[2] = 0 | ver;
  message[3] = 0 | ver>>8;
}

void ser_discovery(unsigned char *message){
  unsigned short type=0x01;
  
  message[0] = 0 | type;
  message[1] = 0 | type>>8;
  
  char *service = "directory service";
  strcpy((char *)&message[2],service);
}

void unser_discovery_res(unsigned char *message){
 // unsigned short port = message[18] | message[19]<<8;
  //printf("Directory service run at %s:%d\n",&message[2],port);
}

void ser_add(unsigned char *message,char *ip,unsigned short port,char *name,char *prop){
  
  unsigned short type = 0x03;
  
  message[0] = 0 | type;
  message[1] = 0 | type>>8;
  
  strcpy((char *)&message[2],name);
  
  strcpy((char *)&message[64],ip);
  
  message[80] = 0 | port;
  message[81] = 0 | port>>8;
  
  strcpy((char *)&message[82],prop);
}

void unser_add_res(unsigned char *message){
  
 // unsigned short result = message[2] | message[3]<<8;
  
   //printf("Result: %hd\n",result);
}

void ser_remove(unsigned char *message,char *ip,unsigned short port,char *name){
  unsigned short type = 0x05;
  
  message[0] = 0 | type;
  message[1] = 0 | type>>8;
  
  strcpy((char *)&message[2],name);
  
  strcpy((char *)&message[64],ip);
  
  message[80] = 0 | port;
  message[81] = 0 | port>>8;  
}

void unser_remove_res(unsigned char *message){
 // unsigned short result = message[2] | message[3]<<8;
  //printf("Result: %hd\n",result);
}

int unser_open_req(unsigned char *message){
  char name[128];
  unsigned int ver;
  int flag = 0;
  unsigned short id=0;
  
  
  char *filename = strdup((char *)&message[2]);
  char *path = malloc(8+strlen(filename));
  
  strcpy(path,"mynfs/");
  strcpy(&path[6],filename);
  
  //printf("Path %s\n",path);
  
  fopen(path,"a+");                                //opens |  creates a file
 
  FILE *fp=fopen(DBFILE,"a+");
  
  while(fscanf(fp,"%s %hd %u\n",name,&id,&ver) != EOF){
    if(strcmp(name,filename)==0){
      flag=1;
      break;
    }
  }
  
  if(flag == 0){
    ver = 0;
    //printf("Print %s id %hd\n",filename,id+1);
    fprintf(fp,"%s %hd %u\n",filename,++id,ver);
  }
  
  filecounter++;
  fclose(fp);
  return(id);
  
}

int unser_read_req(unsigned char *message){
  char name[128];
  unsigned short id=0;
  unsigned int ver;
  int i=0;
  short returned;
  unsigned short fd   = message[2] | message[3]<<8;
  unsigned short seek = message[4] | message[5]<<8;
  unsigned short len  = message[6] | message[7]<<8;
 
  FILE *fp=fopen(DBFILE,"r");
  
  while(fscanf(fp,"%s %hd %u\n",name,&id,&ver) != EOF){
    if(id==fd){
      break;
    }
    i++;
  }
  
  
  
  if(i==filecounter){
    //printf("return\n");
    return 0x02;
  }
  
  fclose(fp);
  
  char *path = malloc(8+strlen(name));
  
  strcpy(path,"mynfs/");
  strcpy(&path[6],name);
  
  FILE *fid = fopen(path,"r");
  
  fseek(fid,seek,SEEK_SET);
  
  returned = fread(&message[6],sizeof(char),len,fid);
  
  fclose(fid);
 
  //printf("Read from file %s %d bytes position %d %s\n",name,returned,seek,&message[6]);
  
  return returned;

}

int unser_write_req(unsigned char *message){
  char name[128];
  unsigned short id=0;
  unsigned int ver;
  short returned;
  int i=0;
  
  unsigned short fd   = message[2] | message[3]<<8;
  unsigned short seek = message[4] | message[5]<<8;
  unsigned short len  = message[6] | message[7]<<8;
  
  FILE *fp=fopen(DBFILE,"r");
  
  while(fscanf(fp,"%s %hd %u\n",name,&id,&ver)!=EOF){
    if(id==fd){ 
      break;
    }
    i++;
  }
  
  fclose(fp);
  
  if(i==filecounter){
    return 0x02;
  }
  
  //printf("Write to file %s %d bytes position %d %s\n",name,len,seek,&message[8]);
  
  char *path = malloc(8+strlen(name));
  
  strcpy(path,"mynfs/");
  strcpy(&path[6],name);
  
  FILE *fid = fopen(path,"r+");
  
  //call of inc_vers to update the version of the file
  inc_vers(fd);
  fseek(fid,seek,SEEK_SET);
  
  returned =fwrite(&message[8],sizeof(unsigned char),len,fid);
  
  fclose(fid);

  return returned;
}

void ser_open_res(unsigned char *message,unsigned short fd){
  unsigned short type = 0x02;
  unsigned short result; 
  if (fd<0){
    result = 0;  
  }
  else{
    result = 1; 
  }
  
  message[0] = 0 | type;
  message[1] = 0 | type>>8;
  message[2] = 0 | result;
  message[3] = 0 | result>>8;
  message[4] = 0 | fd;
  message[5] = 0 | fd>>8;

}

void ser_read_res(unsigned char *message,unsigned short result){
  unsigned short type = 0x04;
  unsigned short len = 0;
  unsigned short resultb;
  
  if(result==0x02){
    resultb = 0x02;
    
  }else if(result>0){
    resultb = 0x01;
    len = result;
  }else{
    resultb = 0x00;
  }
  
  message[0] = 0 | type;
  message[1] = 0 | type>>8;
  message[2] = 0 | resultb;
  message[3] = 0 | resultb>>8;
  message[4] = 0 | len;
  message[5] = 0 | len>>8;


}

void ser_write_res(unsigned char *message,unsigned short result){
  unsigned short type = 0x06;
  unsigned short resultb;
  
  if(result==0x02){
    resultb = 0x02;
  }else if(result>0){
    resultb = 0x01;
  }else{
    resultb = 0x00;
  }
  
  message[0] = 0 | type;
  message[1] = 0 | type>>8;
  message[2] = 0 | resultb;
  message[3] = 0 | resultb>>8;

}

int unser_message(unsigned char *message){  
  unsigned short type = message[0] | message[1]<<8;  
  printf("Request type: %hd\n",type);
  if(type==1){
    unsigned short fd;
    printf("Open Request\n");
    fd = unser_open_req(message);
    ser_open_res(message,fd);
  }else if(type==3){
    printf("Read Request\n");
    unsigned short result;
    result = unser_read_req(message);
    ser_read_res(message,result);
  }else if(type==5){
    printf("Write Request\n");
    unsigned short result;
    result = unser_write_req(message);
    ser_write_res(message,result);
  }else if(type==7){
    printf("Version Request\n");
    unsigned int version;
    version=unser_get_vers(message);  
    ser_get_vers(message,version);
  }else{
    //printf("Wrong request");
  }
  return 0;
}

int main(int argc, char *argv[]){
  struct sockaddr_in addr,addrs;
  int fd,nbytes;
  socklen_t addrlen,addrlens;
  u_int yes=1;
  unsigned char msgbuf[MSGBUFSIZE];
  
  mkdir("mynfs",0777);
  
  servicename = strdup(argv[1]);
  
  
  
  /* create what looks like an ordinary UDP socket */
  if ((fd=socket(AF_INET,SOCK_DGRAM,0)) < 0) {
    perror("socket");
    exit(0);
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
    printf("Try to find server...\n");
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
      printf("Server found\n");
      break;
    }
  }while(1);
  //unserialize the message - gets ip & port from directory service 
  unser_discovery_res(msgbuf);
  
  
  /* now just sendto() our destination! */
    
    ser_add(msgbuf,argv[2],atoi(argv[3]),argv[1],"=PO");
    
    
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
    unser_add_res(msgbuf);
    

    
    close(fd);
    
    if ((fd=socket(AF_INET,SOCK_DGRAM,0)) < 0) {
      perror("socket");
      exit(0);
    }
  
    if (setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes)) < 0) {
      perror("Reusing ADDR failed");
      exit(1);
    }
    
    memset(&addrs,0,sizeof(addrs));
    addrs.sin_family=AF_INET;
    addrs.sin_addr.s_addr=inet_addr(argv[2]);
    addrs.sin_port=htons(atoi(argv[3]));
    
    if (bind(fd,(struct sockaddr *) &addrs,sizeof(addrs)) < 0) {
      perror("bind");
      exit(1);
    }
    
    do{
      addrlens=sizeof(addrs);
      if ((nbytes=recvfrom(fd,msgbuf,MSGBUFSIZE,0,(struct sockaddr *) &addrs,&addrlens)) < 0){
	  perror("recvfrom");
      }
	
	
      unser_message(msgbuf);
      
      
      if (sendto(fd,msgbuf,MSGBUFSIZE,0,(struct sockaddr *) &addrs,sizeof(addr)) < 0) {
	perror("sendto");
	exit(1);
      }
    }while(1);
  
    close(fd);
  return 0;
}
