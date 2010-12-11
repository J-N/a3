#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>

//Constants
#define FSSIZE 2097152  //2MB
#define BLOCKSIZE 256
#define INODEBLOCKS 256
#define BITMAPBLOCKS 4
#define INODESIZE 64
#define DIRENTSIZE 16

//Inode macros
#define INODE(start,num) start + BLOCKSIZE + num * INODESIZE  //zero-indexed
#define GETINODETYPE(start,num) *((char **)((INODE(start,num)) + 4))
#define SETINODETYPE(start,num,val) *((char **)((INODE(start,num)) + 4))= val
#define GETINODESIZE(start,num) *((int *)(INODE(start,num)))
#define SETINODESIZE(start,num,val) *((int *)(INODE(start,num))) = val
//#define GETINODELOC(start,num,locnum) ((struct inode *)INODE(start,num))->location[locnum]
#define SETINODEIND(start,num,locnum,ptr) *((void**)(INODE(start,num)+8+4*locnum)) = ptr
#define GETINODEIND(start,num,locnum) *((void**)(INODE(start,num)+8+4*locnum))

//Bitmap macros
#define BITMAP(start) start + INODEBLOCKS * BLOCKSIZE
#define ISALLOC(start, block) *((int *)(BITMAP(start) + (block / 8))) & (0x01 << (block % 8))
#define ALLOCZERO(start, block) *((int *)(BITMAP(start) + (block / 8))) &= ~(0x01 << (block % 8))  
#define ALLOCONE(start, block) *((int *)(BITMAP(start) + (block / 8))) |= (0x01 << (block % 8))  

//Superblock macros
#define SETSUPERBLOCK(start, num) *((int *)(start)) = num
#define SETSUPERINODE(start,num)  *((int *)(start + 4)) = num
#define GETSUPERBLOCK(start) *((int *)(start))
#define GETSUPERINODE(start) *((int *)(start + 4))

//data block macros
#define DATABLOCK(start,num) (start + BLOCKSIZE * (1 + INODEBLOCKS + BITMAPBLOCKS)) + num * BLOCKSIZE
#define GETDIRENTNAME(block,num) *((char **)(block + num * DIRENTSIZE + 2))
#define GETDIRENTINODE(block,num) *((unsigned short *)(block + num * DIRENTSIZE))
#define SETDIRENTNAME(block,num,val) *((char **)(block + num * DIRENTSIZE + 2)) = val
#define SETDIRENTINODE(block,num,val) *((unsigned short *)(block + num * DIRENTSIZE)) = val
#define GETBLOCKFROMPTR(start,ptr) ((ptr - DATABLOCK(start,0)) / BLOCKSIZE) 

void *test; //our filesystem in main

struct superblock
{
  int freeblocks;
  int freeinodes;
};

struct inode
{
  
  int size;
  void *location[10];
  char *type;
};

struct dirent
{
  unsigned short inode;
  char *filename;
};

void *GETINODELOC(void *start, int num, int locnum)
{
  if(locnum < 8)
    {
      /* if(((void **)(INODE(start,num) + 8 + locnum*4 )) == NULL)
	{
	  int i;
	    void *fblock = NULL;
	    for(i=0;i<BITMAPBLOCKS * BLOCKSIZE * 8;i++)
	      {
		if(!ISALLOC(test,i))
		  {
		    fblock= DATABLOCK(test,i);
		    ALLOCONE(test,i);
		    break;
		  }
	      }
	    if(fblock == NULL)
	      {
		printf("Can't allocate index block in SETINODELOC.\n");
		exit(1);
	      }
    	    SETINODEIND(test,num,locnum,fblock);
	    SETSUPERBLOCK(test,GETSUPERBLOCK(test) - 1);
	}
      */  
      return *((void **)(INODE(start,num) + 8 + locnum*4 ));
    }
  else
    if(locnum < 71)
      {
      void *temp = *((void **)(INODE(start,num) +8 + 4*8)) + 4*(locnum - 8);
      return *((void **)temp);
      //return temp;
      }

}

void *SETINODELOC(void *start, int num, int locnum, void *ptr)
{
  if(locnum < 8)
    *((void **)(INODE(start,num) + 8 + locnum*4 )) = ptr;
  else
    if(locnum < 71)
      {
	if((GETINODEIND(test,num,8) == NULL))
	  {
	    int i;
	    void *fblock = NULL;
	    for(i=0;i<BITMAPBLOCKS * BLOCKSIZE * 8;i++)
	      {
		if(!ISALLOC(test,i))
		  {
		    fblock= DATABLOCK(test,i);
		    ALLOCONE(test,i);
		    break;
		  }
	      }
	    if(fblock == NULL)
	      {
		printf("Can't allocate index block in SETINODELOC.\n");
		exit(1);
	      }
	    SETINODEIND(test,num,8,fblock);
	    SETSUPERBLOCK(test,GETSUPERBLOCK(test) - 1);
	  }
	    
      void *temp = *((void **)(INODE(start,num) +8 + 4*8)) + 4*(locnum - 8);
      *((void **)temp) = ptr;
      //return temp;
      }

}

void *initialize()
{
  void *filesys = malloc(FSSIZE);
  SETSUPERBLOCK(filesys, (FSSIZE / BLOCKSIZE) - (1 + INODEBLOCKS + BITMAPBLOCKS));
  SETSUPERINODE(filesys, INODEBLOCKS * (INODEBLOCKS / INODESIZE));
  int i;
  for(i=0;i<32*BLOCKSIZE;i++)
    ALLOCZERO(filesys, i);
  for(i=0;i<INODEBLOCKS * 4;i++)
    SETINODESIZE(filesys,i,-1337);
  //set up root directory entry
  SETSUPERBLOCK(filesys,GETSUPERBLOCK(filesys) - 1);
  SETSUPERINODE(filesys, GETSUPERINODE(filesys) - 1);
  SETINODETYPE(filesys,0,"dir");
  SETINODESIZE(filesys,0,0);
  SETINODELOC(filesys,0,0,DATABLOCK(filesys,0));
  //SETINODEIND(filesys,0,8,DATABLOCK(filesys,3));  //test indirect ref
  
  return filesys;
}

int rd_unlink(char *pathname)
{
  
  const char *delim = "/";
  char *result = NULL;
  char *filename = NULL;
  char *path2 = malloc(400);
  int k =0;
  strcpy(path2,pathname);
  result = strtok(path2, delim);
  while(result != NULL)
    {
      k++;
      filename = result;
      result = strtok(NULL,delim);
    }
  printf("Filename:\t%s\n",filename); // debug
  result = strtok(pathname, delim);
  int inode = 0;
  void *place = GETINODELOC(test,0,0);
  void *new = NULL;
  int removeinode = 0;
  void *removeplace = NULL;
  int numdirent;
  int i;
  int l=0; //John: may have to be set to 1 in kernel
  int j = 1;
  while(result != NULL)
    {
      l++;
      j=1;
      numdirent = GETINODESIZE(test,inode) / DIRENTSIZE;
      for(i=0;i<numdirent && l!=k;i++)
	{
	  if(i>= j*(BLOCKSIZE/DIRENTSIZE))
	    {
	      place = GETINODELOC(test,inode,j);
	      j++;
	    }
	  if(strcmp(GETDIRENTNAME(place,i%16),result) == 0 && strcmp(GETINODETYPE(test,GETDIRENTINODE(place,i%16)),"dir") == 0  && l!=k)
	    {
	      inode = GETDIRENTINODE(place,i%16);
	      place = GETINODELOC(test,GETDIRENTINODE(place,i%16),0);
	      new = place;
	      break;
	    }
	}

  
      result = strtok(NULL,delim);
      
      if(new == NULL && result != NULL) //Then we have no directory match
	return -1;
      new = NULL;
    }
  printf("have directory inodes\n");
  //now we have a directory in place and its inode in inode
      numdirent = GETINODESIZE(test,inode) / DIRENTSIZE;
      printf("inode:%d\n",inode);
      printf("numdirent:%d\n",numdirent);
      j=1;
      removeplace = place;
      removeinode = inode;
      new = NULL;
      for(i=0;i<numdirent;i++)
	{
	   if(i>= j*(BLOCKSIZE/DIRENTSIZE))
	    {
	      removeplace = GETINODELOC(test,removeinode,j);
	      j++;
	    }
	   //printf("filename again:%s\n",GETDIRENTNAME(removeplace,i+2));
	    if(strcmp(GETDIRENTNAME(removeplace,i%16),filename) == 0)
	    {
	      removeinode = GETDIRENTINODE(removeplace,i%16);
	      removeplace = GETINODELOC(test,GETDIRENTINODE(removeplace,i%16),0);
	      new = removeplace;
	      break;
	    }
	}
      if(new == NULL)
	{
	  printf("it is here\n");
	  return -1;
	}
      printf("have remove and dir inodes\n");
      //now have remove and dir inodes and first blocks
      if(strcmp(GETINODETYPE(test,removeinode),"dir") == 0)
	if(GETINODESIZE(test,removeinode) != 0)
	  return -1;
      if(removeinode == 0)
	return -1;
      int removeblocks = GETINODESIZE(test,removeinode) / BLOCKSIZE;
      //remove data blocks
      if(removeblocks == 0)
	{
	  ALLOCZERO(test,GETBLOCKFROMPTR(test,GETINODELOC(test,removeinode,0)));
	  SETSUPERBLOCK(test,GETSUPERBLOCK(test) + 1);
	  printf("Removing location block\n");
	}
      for(i=0;i<removeblocks;i++)
	{
	  ALLOCZERO(test,GETBLOCKFROMPTR(test,GETINODELOC(test,removeinode,i)));
	  SETSUPERBLOCK(test,GETSUPERBLOCK(test) + 1);
	  printf("Removing location block\n");
	}
      //remove single indirect index blocks
      if(removeblocks > 7)
	{
	  ALLOCZERO(test,GETBLOCKFROMPTR(test,GETINODEIND(test,removeinode,8)));
	  SETSUPERBLOCK(test,GETSUPERBLOCK(test) + 1);
	}
      //remove double indirect index blocks
      if(removeblocks > 71)
	{
	  void * doubleind = GETINODEIND(test,removeinode,9);
	  for(i=0;i<64;i++)
	    {
	      ALLOCZERO(test,GETBLOCKFROMPTR(test,(void *)(*((unsigned int *) doubleind + i))));
	      SETSUPERBLOCK(test,GETSUPERBLOCK(test) + 1);
	    }
	  ALLOCZERO(test,GETBLOCKFROMPTR(test,doubleind));
	  SETSUPERBLOCK(test,GETSUPERBLOCK(test) + 1);
  
	}
      //remove inode
      SETINODESIZE(test,removeinode,-1337);
      SETSUPERINODE(test,GETSUPERINODE(test) + 1);
      numdirent = GETINODESIZE(test,inode) / DIRENTSIZE;
      j=1;
      //remove directory entry in parent directory
      for(i=0;i<numdirent;i++)
	{
	  if(i>= j*(BLOCKSIZE/DIRENTSIZE))
	    {
	      place = GETINODELOC(test,inode,j);
	      j++;
	    }
	  if(GETDIRENTINODE(place,i%16) == removeinode )
	    {
	      SETDIRENTNAME(place,i%16,GETDIRENTNAME(GETINODELOC(test,inode,(numdirent -1)/16),(numdirent -1)%16));
	      SETDIRENTINODE(place,i%16,GETDIRENTINODE(GETINODELOC(test,inode,(numdirent -1)/16),(numdirent -1)%16));
	      break;
	    }

	}
      //set inode size for parent directory
      	  SETINODESIZE(test,inode,GETINODESIZE(test,inode) - DIRENTSIZE);
	  printf("Dirent Size: %d\n",GETINODESIZE(test,inode));
	  if(GETINODESIZE(test,inode) % 256 == 0) //we can free the empty node
	    {
	      void *blk = GETINODELOC(test,inode,GETINODESIZE(test,inode) / 256);
	      ALLOCZERO(test,GETBLOCKFROMPTR(test,blk));
	      SETSUPERBLOCK(test,GETSUPERBLOCK(test) + 1);
	      SETINODELOC(test,inode,GETINODESIZE(test,inode)/256,NULL);
	    }

	  

      return 0;
     
}
      
int rd_creat(char *pathname)
{
  const char *delim = "/";
  char *result = NULL;
  char *filename = NULL;
  char *path2 = malloc(400);
  strcpy(path2,pathname);
  result = strtok(path2, delim);
  while(result != NULL)
    {
      filename = result;
      result = strtok(NULL,delim);
    }
  printf("Filename:\t%s\n",filename); // debug
  result = strtok(pathname, delim);
  int inode = 0;
  void *fblock = NULL;
  void *place = GETINODELOC(test,0,0);
  int newinode = -1;
  int numdirent;
  void *new = NULL;
  int i;
  int j = 1;
  //check path, store inode # of dir to put file into in inode, and place as its current location.
  while(result != NULL)
    {
      numdirent = GETINODESIZE(test,inode) / DIRENTSIZE;
      j=1;
      for(i=0;i<numdirent;i++)
	{
	  if(i>= j*(BLOCKSIZE/DIRENTSIZE))
	    {
	      place = GETINODELOC(test,inode,j);
	      j++;
	    }
	  if(strcmp(GETDIRENTNAME(place,i%16),result) == 0 && strcmp(GETINODETYPE(test,GETDIRENTINODE(place,i%16)),"dir") == 0)
	    {
	      inode = GETDIRENTINODE(place,i%16);
	      place = GETINODELOC(test,GETDIRENTINODE(place,i%16),0);
	      new = place;
	      break;
	    }
	  //New code for John
	  if(strcmp(GETDIRENTNAME(place,i%16),result) == 0 && strcmp(GETINODETYPE(test,GETDIRENTINODE(place,i%16)),"reg") == 0)
	    return -1;
	}
      result = strtok(NULL,delim);
      if(new == NULL && result != NULL)
	return -1;
      new = NULL;
    }
  printf("Inode where it will be created:%d\n",inode);
  //find a free block for the new file
  for(i=0;i<BITMAPBLOCKS * BLOCKSIZE * 8;i++)
    {
      if(!ISALLOC(test,i))
	{
	  fblock= DATABLOCK(test,i);
	  ALLOCONE(test,i);
	  break;
	}
    }
  if(fblock == NULL)
    return -1;
  //find a free inode for the child
  for(i=0;i<INODEBLOCKS * 4;i++)
    {
      if(GETINODESIZE(test,i) == -1337)
	{
	  newinode = i;
	  SETINODESIZE(test,i,0);
	  SETINODETYPE(test,i,"reg");
	  SETINODELOC(test,i,0,fblock);
	  break;
	}
    }
  if(newinode==-1)
    return -1;
  //allocate a new block for the directory if we will grow past its limit
  if((GETINODESIZE(test,inode) / DIRENTSIZE) % 16 == 0 && GETINODESIZE(test,inode) != 0)
    {
      void *newdirblock = NULL;
      for(i=0;i<BITMAPBLOCKS * BLOCKSIZE * 8;i++)
	{
	  if(!ISALLOC(test,i))
	    {
	      newdirblock= DATABLOCK(test,i);
	      ALLOCONE(test,i);
	      SETINODELOC(test,inode,(GETINODESIZE(test,inode) / 256),newdirblock);
	      break;
	    }
	}
      if(newdirblock == NULL)
	return -1;
      SETSUPERBLOCK(test,GETSUPERBLOCK(test) - 1);
    }
  SETSUPERBLOCK(test,GETSUPERBLOCK(test) - 1);
  SETSUPERINODE(test,GETSUPERINODE(test) - 1);
  SETDIRENTINODE(GETINODELOC(test,inode,(GETINODESIZE(test,inode) / BLOCKSIZE)),((GETINODESIZE(test,inode)/DIRENTSIZE)%16),newinode);
  SETDIRENTNAME(GETINODELOC(test,inode,(GETINODESIZE(test,inode) / BLOCKSIZE)),((GETINODESIZE(test,inode)/DIRENTSIZE)% 16),filename);
  //  printf("%d\n",GETINODESIZE(test,inode));
  printf("%d %d %s\n",GETINODESIZE(test,inode)/256,((GETINODESIZE(test,inode)/16)% 16),GETDIRENTNAME(GETINODELOC(test,inode,(GETINODESIZE(test,inode)/256)),(GETINODESIZE(test,inode)/16) % 16));

  SETINODESIZE(test,inode,GETINODESIZE(test,inode)+DIRENTSIZE);
  return 0;
  
      
}      
int rd_mkdir(char *pathname)
{
  const char *delim = "/";
  char *result = NULL;
  char *filename = NULL;
  char *path2 = malloc(400);
  strcpy(path2,pathname);
  result = strtok(path2, delim);
  
  while(result != NULL)
    {
      filename = result;
      result = strtok(NULL,delim);
    }
  printf("Dirname:\t%s\n",filename); // debug
  result = strtok(pathname, delim);
  int inode = 0;
  void *fblock = NULL;
  void *place = GETINODELOC(test,0,0);
  int newinode = -1;
  int numdirent;
  void *new = NULL;
  int i;
  int j = 1;
  //check path, store inode # of dir to put file into in inode, and place as its current location.
  while(result != NULL)
    {
      numdirent = GETINODESIZE(test,inode) / DIRENTSIZE;
      j=1;
      for(i=0;i<numdirent;i++)
	{
	  if(i>= j*(BLOCKSIZE/DIRENTSIZE))
	    {
	      place = GETINODELOC(test,inode,j);
	      j++;
	    }
	  if(strcmp(GETDIRENTNAME(place,i%16),result) == 0 && strcmp(GETINODETYPE(test,GETDIRENTINODE(place,i%16)),"dir") == 0)
	    {
	      inode = GETDIRENTINODE(place,i%16);
	      place = GETINODELOC(test,GETDIRENTINODE(place,i%16),0);
	      new = place;
	      break;
	    }
	  //New code for John
	  if(strcmp(GETDIRENTNAME(place,i%16),result) == 0 && strcmp(GETINODETYPE(test,GETDIRENTINODE(place,i%16)),"reg") == 0)
	    return -1;
	}
      result = strtok(NULL,delim);
      if(new == NULL && result != NULL)
	return -1;
      new = NULL;
    }
  //find a free block for the new file
  for(i=0;i<BITMAPBLOCKS * BLOCKSIZE * 8;i++)
    {
      if(!ISALLOC(test,i))
	{
	  fblock= DATABLOCK(test,i);
	  ALLOCONE(test,i);
	  break;
	}
    }
  if(fblock == NULL)
    return -1;
  //find a free inode for the child
  for(i=0;i<INODEBLOCKS * 4;i++)
    {
      if(GETINODESIZE(test,i) == -1337)
	{
	  newinode = i;
	  SETINODESIZE(test,i,0);
	  SETINODETYPE(test,i,"dir");
	  SETINODELOC(test,i,0,fblock);
	  break;
	}
    }
  if(newinode==-1)
    return -1;
  //allocate a new block for the directory if we will grow past its limit
  if((GETINODESIZE(test,inode) / DIRENTSIZE) % 16 == 0  && GETINODESIZE(test,inode) != 0)
    {
      void *newdirblock = NULL;
      for(i=0;i<BITMAPBLOCKS * BLOCKSIZE * 8;i++)
	{
	  if(!ISALLOC(test,i))
	    {
	      newdirblock= DATABLOCK(test,i);
	      ALLOCONE(test,i);
	      SETINODELOC(test,inode,(GETINODESIZE(test,inode) / 256),newdirblock);
	      break;
	    }
	}
      if(newdirblock == NULL)
	return -1;
      SETSUPERBLOCK(test,GETSUPERBLOCK(test) - 1);
    }
  SETSUPERBLOCK(test,GETSUPERBLOCK(test) - 1);
  SETSUPERINODE(test,GETSUPERINODE(test) - 1);
  SETDIRENTINODE(GETINODELOC(test,inode,(GETINODESIZE(test,inode) / BLOCKSIZE)),(GETINODESIZE(test,inode)/DIRENTSIZE)%16,newinode);
  SETDIRENTNAME(GETINODELOC(test,inode,(GETINODESIZE(test,inode) / BLOCKSIZE)),(GETINODESIZE(test,inode)/DIRENTSIZE)%16,filename);
  SETINODESIZE(test,inode,GETINODESIZE(test,inode)+DIRENTSIZE);
  
  return 0;
  
      
}      

int main(int argc, char** argv)
{
  printf("Initializing filesystem\n");
  test = initialize();
  if(test == NULL)
    {
      printf("Error.\n");
      exit(1);
    }
  printf("Superblock:\t%x\nFirst Inode:\t%x\nBitmap Start:\t%x\n",(unsigned int)test,(unsigned int)INODE(test,0),(unsigned int)BITMAP(test));
  printf("Second Inode:\t%x\n",(unsigned int)INODE(test,1));
  
  printf("Free Blocks:\t%d\nFree Inodes:\t%d\n",GETSUPERBLOCK(test),GETSUPERINODE(test));

  printf("Root directory printout:\n");
  printf("Inode type:\t%s\n",GETINODETYPE(test,0));
  printf("Inode size:\t%d\n",GETINODESIZE(test,0));
  printf("Inode location pointer:\t%x\n",(unsigned int)GETINODELOC(test,0,0));
  printf("First Data Block:\t%x\n",(unsigned int)DATABLOCK(test,3));
  //void *hee = GETINODELOC(test,0,1);
  printf("Here goes nothing...\n");
  char *hurp = malloc(200);
  int i;
  
  for(i=0;i<200;i++)
    {
      sprintf(hurp,"/file%d",i);
      if(rd_mkdir(hurp) == -1)
	printf("error\n");
    }

   for(i=199;i>=0;i--)
    {
      sprintf(hurp,"/file%d",i);
      if(rd_unlink(hurp) == -1)
	printf("error\n");
    }
  
  /*  strcpy(hurp,"/test");
  if(rd_mkdir(hurp) == -1)
    printf("error");

  strcpy(hurp,"/test/test2");
  if(rd_mkdir(hurp) == -1)
    printf("error");
  
  strcpy(hurp,"/test/test2/har");
  if(rd_creat(hurp) == -1)
    printf("error");
  
  strcpy(hurp,"/test/har2");
  if(rd_creat(hurp) == -1)
    printf("error");
  */

  printf("Free Blocks:\t%d\nFree Inodes:\t%d\n",GETSUPERBLOCK(test),GETSUPERINODE(test));

  /* strcpy(hurp,"/test/test2/har");
  if(rd_unlink(hurp) == -1)
    printf("error");

  strcpy(hurp,"/test/har2");
  if(rd_unlink(hurp) == -1)
    printf("error");

  strcpy(hurp,"/test/test2");
  if(rd_unlink(hurp) == -1)
    printf("error");

  strcpy(hurp,"/test");
  if(rd_unlink(hurp) == -1)
    printf("error");

  */
  printf("Free Blocks:\t%d\nFree Inodes:\t%d\n",GETSUPERBLOCK(test),GETSUPERINODE(test));
  
  return;
}
