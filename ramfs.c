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

//Inode macros
#define INODE(start,num) start +BLOCKSIZE + num * INODESIZE  //zero-indexed
#define GETINODETYPE(start,num) ((struct inode *)INODE(start,num))->type
#define SETINODETYPE(start,num,val) ((struct inode *)INODE(start,num))->type = val
#define GETINODESIZE(start,num) ((struct inode *)INODE(start,num))->size
#define SETINODESIZE(start,num,val) ((struct inode *)INODE(start,num))->size = val
#define GETINODELOC(start,num,locnum) ((struct inode *)INODE(start,num))->location[locnum]
#define SETINODELOC(start,num,locnum,ptr) ((struct inode *)INODE(start,num))->location[locnum] = ptr

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


struct superblock
{
  int freeblocks;
  int freeinodes;
};

struct inode
{
  char *type;
  int size;
  void *location[10];
};

struct direntry
{
  char *filename;
  unsigned short inode;
};

void *initialize()
{
  void *filesys = malloc(FSSIZE);
  SETSUPERBLOCK(filesys, (FSSIZE / BLOCKSIZE) - (1 + INODEBLOCKS + BITMAPBLOCKS));
  SETSUPERINODE(filesys, INODEBLOCKS * (INODEBLOCKS / INODESIZE));
  int i;
  for(i=0;i<32*BLOCKSIZE;i++)
    ALLOCZERO(filesys, i);
  //set up root directory entry
  SETSUPERBLOCK(filesys,GETSUPERBLOCK(filesys) - 1);
  SETSUPERINODE(filesys, GETSUPERINODE(filesys) - 1);
  SETINODETYPE(filesys,0,"dir");
  SETINODESIZE(filesys,0,0);
  SETINODELOC(filesys,0,0,DATABLOCK(filesys,0));
  
  return filesys;
}

int main(int argc, char** argv)
{
  printf("Initializing filesystem\n");
  void *test = initialize();
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
  printf("First Data Block:\t%x\n",(unsigned int)DATABLOCK(test,0));
  
}
