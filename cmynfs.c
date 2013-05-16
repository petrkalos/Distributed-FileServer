#include "cmynfs.h"
#include "mynfs.h"

#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#define DATASIZE 512
#define CACHESIZE 6
#define T 2



//declarations
typedef struct {                //cache 

unsigned int fd;
unsigned int version;
unsigned int start;
unsigned int end;
unsigned int timestamp;
char data[DATASIZE];
int use;              // declares if cache block is usable
unsigned int piece;
}CACHE;

unsigned int last;
int init = 0;
CACHE cache[CACHESIZE];

//functions
void init_cache(){
  int i;
  for(i=0;i<CACHESIZE;i++){
    cache[i].fd=0;
    cache[i].version=0;
    cache[i].start=0;
    cache[i].end=0;
    cache[i].timestamp=0;
    cache[i].use=0;
    cache[i].piece=0;
  }
}

void update_cache(int fd,char *msgbuf,int seek,int n){
  int i,ver;
  int count = 0;
  int index = seek;
  struct timeval time;
  
  ver=mynfs_version(fd);
  
  do{
    for(i=0;i<CACHESIZE;i++){
      if(cache[i].fd==fd && cache[i].use == 1 && ver == cache[i].version + 1){
	gettimeofday(&time,NULL);
	cache[i].timestamp = time.tv_sec+(time.tv_usec/1000000.0);   //renew timestamp
	cache[i].version++;      //renew version
	if(index>=cache[i].start){
	  if(i==0){
	    memcpy(&cache[i].data[index],msgbuf,cache[i].end-index);
	    count += cache[i].end-index;
	    index = cache[i].end;
	    break;
	  }else{
	    if(DATASIZE-count>=0){
	      memcpy(cache[i].data,msgbuf,DATASIZE);
	      count += DATASIZE;
	      index = cache[i].end;
	    }else{
	      memcpy(cache[i].data,msgbuf,n-count);
	      count += n-count;
	      index = cache[i].end;
	    }
	    last=i;
	    break;
	  }
	}
      }
    }
    if(i==CACHESIZE){
      index += cache[i].end+DATASIZE;
    }
    if(index>=n) break;
  }while(1);
}

int cmynfs_open(char *fname){
  unsigned int fd;
  
  fd=mynfs_open(fname);
  if(init==0){
    init_cache();
    init=1;
  }
  return(fd);
  
}

void check_cache(int fd, char buf[], int n){
  int i,seek,fpiece,tpiece,version;
  
  struct timeval time;
  
  seek = mynfs_get_pos(fd);
 
  fpiece=floor((double)(seek)/DATASIZE);
  tpiece=floor((double)(seek+n)/DATASIZE);
  
  for(i=0;i<CACHESIZE;i++){
    if(cache[i].fd==fd && cache[i].use==1){
      if(cache[i].piece>=fpiece && cache[i].piece<=tpiece){
	printf("Piece found in cache\n");
	gettimeofday(&time,NULL);
	if(cache[i].timestamp+T<=time.tv_sec+(time.tv_usec/1000000.0)){    
	  printf("...and it's old :(\n");
	  version = mynfs_version(fd);
	  if(version!=cache[i].version){
	    printf("update piece\n");
	    mynfs_seek(fd,cache[i].piece*DATASIZE);
	    mynfs_read(fd,cache[i].data,DATASIZE);
	    mynfs_seek(fd,seek);
	    cache[i].version=version;
	    cache[i].timestamp=time.tv_sec+(time.tv_usec/1000000.0);
	    last=i;
	  }
	}
	
      }
    }
  }
}

int cmynfs_read(int fd, char buf[], int n){
  int k,i,count=0,s,fpiece,tpiece,seek;
  struct timeval time;
  
  check_cache(fd,buf,n);
  
  seek = mynfs_get_pos(fd);
  k=ceil((double)n/DATASIZE);
  fpiece=floor((double)(seek)/DATASIZE);
  tpiece=floor((double)(seek+n)/DATASIZE);

  s=fpiece;
  
  do{
    
    for(i=0;i<CACHESIZE;i++){
      
      if(cache[i].fd==fd){
	
	if(cache[i].piece==s){
	  if(s==fpiece){
	    memcpy(&buf[count],&cache[i].data[seek],DATASIZE*(fpiece+1)-seek);
	    count+=DATASIZE*(fpiece+1)-seek;
	  }else if(s==tpiece){
	    memcpy(&buf[count],&cache[i].data[0],n-count);
	    count+=n-count;
	  }else{
	    memcpy(&buf[count],&cache[i].data[0],DATASIZE);
	    count+=DATASIZE;
	  }
	  printf("Cache hit %d\n",s);
	  break;
	}
      }
    }
    
    if(i==CACHESIZE){
      //Add block at last recently used place or unused one of cache
      
      for(i=0;i<CACHESIZE;i++){
	if(cache[i].use==0){
	  printf("Add to cache %d\n",s);
	  last=i; break;
	}
      }
      if(i==CACHESIZE) printf("Last recently used %d\n",last);
      
      
      cache[last].fd = fd;
      
      cache[last].version = mynfs_version(fd);
      
      
      cache[last].start = s*DATASIZE;
      cache[last].end = s*DATASIZE+DATASIZE;
      
      gettimeofday(&time,NULL);
      cache[last].timestamp=time.tv_sec+(time.tv_usec/1000000.0);
      
      mynfs_seek(fd,s*DATASIZE);
      mynfs_read(fd,cache[last].data,DATASIZE);
      mynfs_seek(fd,seek);
      
      cache[last].use=1;
      
      cache[last].piece = s;
     
      if(s==fpiece){
	printf("%d bytes copied from cache\n",DATASIZE*(fpiece+1)-seek);
	memcpy(&buf[count],&cache[last].data[seek],DATASIZE*(fpiece+1)-seek);
	count+=DATASIZE*(fpiece+1)-seek;
      }else if(s==tpiece){
	printf("%d bytes copied from cache\n",n-count);
	memcpy(&buf[count],&cache[last].data[0],n-count);
	count+=n-count;
      }else{
	printf("%d bytes copied from cache\n",DATASIZE);
	memcpy(&buf[count],&cache[last].data[0],DATASIZE);
	count+=DATASIZE;
      }
    }
    if(count==n) break;
    s++;
  }while(1);
  
  return 1;
}

int cmynfs_write(int fd, char buf[], int n){
  
  int k,i,seek;
  
  k=ceil((double)n/DATASIZE);      
  
  seek=mynfs_get_pos(fd);
  for (i=0;i<k;i++){
    mynfs_seek(fd,i*512+seek);
    if(i+1==k){                                            //if it the last send
      mynfs_write(fd,&buf[i*512],(1-k)*DATASIZE+n);
      printf("Write from %d to %d\n",i*512,n);
    }else{
      mynfs_write(fd,&buf[i*512],DATASIZE);
      printf("Write from %d to %d\n",i*DATASIZE,i*DATASIZE+DATASIZE);
    }
  }
  mynfs_seek(fd,seek);          //returns the seek in its original position
  
  update_cache(fd,buf,seek,n);
  
  return 1;
}

int cmynfs_seek(int fd, int pos){
  return mynfs_seek(fd,pos);
}

int cmynfs_close(int fd){
  int i;
  for(i=0;i<CACHESIZE;i++){
    if(cache[i].fd==fd && cache[i].use == 1){
      cache[i].fd=0;
      cache[i].version=0;
      cache[i].start=0;
      cache[i].end=0;
      cache[i].timestamp=0;
      cache[i].use=0;
    }
  }
  return mynfs_close(fd);
}
