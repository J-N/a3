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

//Bitmap macros
#define BITMAP(start) start + INODEBLOCKS * BLOCKSIZE
#define ISALLOC(start, block) *((int *)(BITMAP(start) + (block / 8))) & (0x01 << (block % 8))
#define ALLOCZERO(start, block) *((int *)(BITMAP(start) + (block / 8))) &= ~(0x01 << (block % 8))  
#define ALLOCONE(start, block) *((int *)(BITMAP(start) + (block / 8))) |= (0x01 << (block % 8))  

//Superblock macros
#define SETSUPERBLOCK(start, num) ((struct superblock *) start)->freeblocks = num
#define SETSUPERINODE(start,num)  ((struct superblock *) start)->freeinodes = num
#define GETSUPERBLOCK(start) ((struct superblock *) start)->freeblocks
#define GETSUPERINODE(start)  ((struct superblock *) start)->freeinodes

//data block macros
#define DATABLOCK(start,num) (start + BLOCKSIZE * (1 + INODEBLOCKS + BITMAPBLOCKS)) + num * BLOCKSIZE
#define GETDIRENTNAME(block,num) *((char **)(block + num * DIRENTSIZE + 2))
#define GETDIRENTINODE(block,num) *((unsigned short *)(block + num * DIRENTSIZE))
#define SETDIRENTNAME(block,num,val) *((char **)(block + num * DIRENTSIZE + 2)) = val
#define SETDIRENTINODE(block,num,val) *((unsigned short *)(block + num * DIRENTSIZE)) = val

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
    return *((void **)(INODE(start,num) + 8 + locnum*4 ));
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
  SETINODEIND(filesys,0,8,DATABLOCK(filesys,3));  //test indirect ref
  
  return filesys;
}

int rd_creat(char *pathname)
{
  const char *delim = "/";
  char *result = NULL;
  char *filename = NULL;
  result = strtok(pathname, delim);
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
      for(i=0;i<numdirent;i++)
	{
	  if(i> j*(BLOCKSIZE/DIRENTSIZE))
	    {
	      place = GETINODELOC(test,inode,j-1);
	      j++;
	    }
	  if(strcmp("hurp",result) == 0)
	    {
	      inode = GETDIRENTINODE(place,i);
	      place = GETINODELOC(test,GETDIRENTINODE(place,i),0);
	      break;
	    }
	}
      result = strtok(NULL,delim);
      if(new == NULL && result != NULL)
	return -1;
      place = new;
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
	  SETINODETYPE(test,i,"reg");
	  SETINODELOC(test,i,0,fblock);
	  break;
	}
    }
  if(newinode==-1)
    return -1;
  //allocate a new block for the directory if we will grow past its limit
  if((GETINODESIZE(test,inode) / DIRENTSIZE) % 16 == 0)
    {
      void *newdirblock = NULL;
      for(i=0;i<BITMAPBLOCKS * BLOCKSIZE * 8;i++)
	{
	  if(!ISALLOC(test,i))
	    {
	      newdirblock= DATABLOCK(test,i);
	      ALLOCONE(test,i);
	      SETINODELOC(test,inode,(GETINODESIZE(test,inode) / 256) + 1,newdirblock);
	      break;
	    }
	}
      if(newdirblock == NULL)
	return -1;
    }
  SETDIRENTINODE(GETINODELOC(test,inode,(GETINODESIZE(test,inode) / BLOCKSIZE)),(GETINODESIZE(test,inode)/DIRENTSIZE),newinode);
  SETDIRENTNAME(GETINODELOC(test,inode,(GETINODESIZE(test,inode) / BLOCKSIZE)),(GETINODESIZE(test,inode)/DIRENTSIZE),filename);
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
  ALLOCZERO(test, 200);
  printf("ISALLOC TEST:\t%d\n",ISALLOC(test,200));
  ALLOCONE(test, 200);
  printf("ISALLOC TEST:\t%d\n",ISALLOC(test,200));
  printf("Free Blocks:\t%d\nFree Inodes:\t%d\n",GETSUPERBLOCK(test),GETSUPERINODE(test));
  SETINODETYPE(test,100,"reg");
  SETINODESIZE(test,100,1337);
  printf("Inode 100 type:\t%s\nInode 100 size:\t%d\n",GETINODETYPE(test,100),GETINODESIZE(test,100));

  printf("Root directory printout:\n");
  printf("Inode type:\t%s\n",GETINODETYPE(test,0));
  printf("Inode size:\t%d\n",GETINODESIZE(test,0));
  printf("Inode location pointer:\t%x\n",(unsigned int)GETINODELOC(test,0,0));
  printf("First Data Block:\t%x\n",(unsigned int)DATABLOCK(test,3));
  SETINODELOC(test,0,8,test);
  printf("This should be superblock:\t%x\n%x\n",((unsigned int ) GETINODELOC(test,0,8)),(unsigned int)(&(*((unsigned int * )DATABLOCK(test,3)))));

  printf("Here goes nothing...\n");
  char *hurp = malloc(200);
  strcpy(hurp,"/test");
  rd_creat(hurp);
  printf("Size of file for inode 1:\t%d\ntype:\t%s\n",GETINODESIZE(test,1),GETINODETYPE(test,1));
  printf("Root direntry for test:\t%s\nInode:\t%hd\n",GETDIRENTNAME(GETINODELOC(test,0,0),0),GETDIRENTINODE(GETINODELOC(test,0,0),0));
  return;
}
