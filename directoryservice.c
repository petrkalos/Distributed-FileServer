#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>


#define HELLO_PORT 36486
#define HELLO_GROUP "225.0.0.37"

#define MSGBUFSIZE 64000
#define TIMEOUT 4

char *DBFILE = "database.txt";



char *get_ip(){
  struct hostent *he;
  struct in_addr **addr_list;
  char *ip=NULL;
  char hostname[16];
  
  gethostname(hostname,sizeof(hostname));
  he = gethostbyname(hostname);
  
  addr_list = (struct in_addr **)he->h_addr_list;
  
  ip = strdup(inet_ntoa(*addr_list[0]));  
  return ip;
}

short unser_discovery(unsigned char *message){
  char *service;
  service = strdup((char *)&message[2]);
  
  if(strcmp(service,"directory service")==0){
     printf("Everything ok\n");
    return 1;
  }else{
     printf("Wrong service\n");
    return 0;
  }
}

void ser_discovery_res(unsigned char *message){
  
  unsigned short type=0x02;
  char *ip = "127.0.0.1";
  unsigned short port = HELLO_PORT;
  
  message[0] = 0 | type;
  message[1] = 0 | type>>8;
  
  strcpy((char *)&message[2],ip);
  
  message[18] = 0 | port;
  message[19] = 0 | port>>8;
}

unsigned short unser_add(unsigned char *message){
  
  char *name = strdup((char *)&message[2]);
  
  char *ip = strdup((char *)&message[64]);
  
  unsigned short port = message[80] | message[81]<<8;
  
  char *prop = strdup((char *)&message[82]);
  
  printf("Add, Name: %s\tIP: %s\tPort: %hd \tProp: %s\n",name,ip,port,prop);
  
  FILE *fp;
  
  fp = fopen(DBFILE,"a+");
  fprintf(fp,"%64s\t%16s\t%hd\t%8s\n",name,ip,port,prop);
  
  if(fflush(fp)!=0) return 0x01;
  else{
    fclose(fp);
    return 0x02;
  }
}

void ser_add_res(unsigned char *message,short result){
  
  unsigned short type = 0x04;
  
  message[0] = 0 | type;
  message[1] = 0 | type>>8;
  
  message[2] = 0 | result;
  message[3] = 0 | result>>8;
  
}

unsigned short unser_remove(unsigned char *message){
  char sname[16];
  char sip[64];
  unsigned short sport;
  char sprop[8];
  
  char *name = strdup((char *)&message[2]);
  
  char *ip = strdup((char *)&message[64]);
  
  unsigned short port = message[80] | message[81]<<8;
  
   printf("Remove Name: %s\tIP: %s\tPort: %hd\n",name,ip,port);
  
  FILE *fp,*fpt;
  
  fp = fopen(DBFILE,"r");
  fpt = fopen("test.txt","w");
  
  while(feof(fp)==0){
    fscanf(fp,"%64s\t%16s\t%hd\t%8s\n",sname,sip,&sport,sprop);

    if(strcmp(sname,name)==0 && strcmp(sip,ip)==0 && sport==port){
       printf("%s Removed\n",name);
    }else{
      fprintf(fpt,"%64s\t%16s\t%hd\t%8s\n",sname,sip,sport,sprop);
    }
  }
  if(fflush(fpt)!=0) return 0x01;
  else{
    fclose(fp);
    remove(DBFILE);
    rename("test.txt",DBFILE);
    fclose(fpt);
    return 0x02;
  }
}

void ser_remove_res(unsigned char *message,short result){
  
  unsigned short type = 0x06;
  
  message[0] = 0 | type;
  message[1] = 0 | type>>8;
   
  message[2] = 0 | result;
  message[3] = 0 | result>>8;
  
}

void unser_search(unsigned char *message,char *fname){
  
  char *name = strdup((char *)&message[2]);
  strcpy(fname,name);
}

void ser_search_res(unsigned char *message,char *fname){
  int step,count,maxreq;
  unsigned short type = 0x08;
  char sname[16];
  char sip[64];
  unsigned short sport;
  char sprop[8];
  
  maxreq = (MSGBUFSIZE-4)/90;
  
  
  message[0] = 0 | type;
  message[1] = 0 | type>>8;
  
  FILE *fp;
  
  fp = fopen(DBFILE,"r");
  count=0;
  step=0;
   printf("Search for name %64s\n",fname);
  while(feof(fp)==0){
    fscanf(fp,"%64s\t%16s\t%hd\t%8s\n",sname,sip,&sport,sprop);
    if(strcmp(sname,fname)==0){
      
       printf("Name: %s\tIP: %s\tPort: %hd\tProp: %s\n",sname,sip,sport,sprop);
      
      strcpy((char *)&message[step+4],sname);
  
      strcpy((char *)&message[step+68],sip);
  
      message[step+84] = 0 | sport;
      message[step+85] = 0 | sport>>8;
  
      strcpy((char *)&message[step+86],sprop);
      
      count++;
      step+=90;
      if(count==maxreq) break;
    }
  }
  fclose(fp);
  
  message[2] = 0 | count;
  message[3] = 0 | count>>8;
  
}

short unser_message(unsigned char *message){ 
  short type = message[0] | message[1]<<8;
   printf("Request type: %hd\n",type);
  if(type==0x01){
    //discovery request && discovery response
    if(unser_discovery(message)){
      ser_discovery_res(message);
      return 1;
    }else return 0;
  }else if(type==0x03){
    //add request
    unsigned short result = unser_add(message);
    
    //add response
    ser_add_res(message,result);
  }else if(type==0x05){
    //remove request
    unsigned short result = unser_remove(message);
    
    //remove response
    ser_remove_res(message,result);
  }else if(type==0x07){
    //find request
    char fname[64];
    unser_search(message,fname);
    
    //find response
    ser_search_res(message,fname);
  }
  return 0;
}

int main(int argc, char *argv[]){

  struct sockaddr_in addr; 				//stoixeia gia socket
  socklen_t addrlen;
  int fd, nbytes;
  struct ip_mreq mreq;     				  //stoixeia gia multicast
  unsigned char msgbuf[MSGBUFSIZE];
     
  u_int yes=1;            /*** MODIFICATION TO ORIGINAL */

  /* create what looks like an ordinary UDP socket */
  if ((fd=socket(AF_INET,SOCK_DGRAM,0)) < 0) {
    perror("socket");
    exit(1);
  }
     
  /**** MODIFICATION TO ORIGINAL */
  /* allow multiple sockets to use the same PORT number */
  if (setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes)) < 0) {
    perror("Reusing ADDR failed");
    exit(1);
  }
  /*** END OF MODIFICATION TO ORIGINAL */

  /* set up destination address */
  memset(&addr,0,sizeof(addr));
  addr.sin_family=AF_INET;
  addr.sin_addr.s_addr=htonl(INADDR_ANY); /* N.B.: differs from sender */
  addr.sin_port=htons(HELLO_PORT);
   
  /* bind to receive address */
  if (bind(fd,(struct sockaddr *) &addr,sizeof(addr)) < 0) {
    perror("bind");
    exit(1);
  }
     
  /* use setsockopt() to request that the kernel join a multicast group */
  mreq.imr_multiaddr.s_addr=inet_addr(HELLO_GROUP);
  mreq.imr_interface.s_addr=htonl(INADDR_ANY);
  if (setsockopt(fd,IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq)) < 0) {
    perror("setsockopt");
    exit(1);
  }
  
  /* now just enter a read-print loop */
  
  while (1) { 
    addrlen=sizeof(addr);
    if ((nbytes=recvfrom(fd,msgbuf,MSGBUFSIZE,0,(struct sockaddr *) &addr,&addrlen)) < 0){
      perror("recvfrom");
      exit(1);
    }
    
    
    unser_message(msgbuf);
    

    if(sendto(fd,msgbuf,MSGBUFSIZE,0,(struct sockaddr *) &addr,sizeof(addr)) < 0) {
      perror("sendto");
      exit(1);
    }
  }
  return 0;
}
    
