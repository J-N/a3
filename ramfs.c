#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>

#define SIZE 2097152  //2MB
#define BLOCKSIZE 256
#define INODEBLOCKS 256
#define BITMAPBLOCKS 4
#define INODESIZE 64
#define INODE(start,num) start +BLOCKSIZE + num * BLOCKSIZE  //zero-indexed
#define BITMAP(start) start + INODEBLOCKS * BLOCKSIZE
#define ISALLOC(start, block) *((int *)(BITMAP(start) + (block / 8))) & (0x01 << (block % 8))
#define ALLOCZERO(start, block) *((int *)(BITMAP(start) + (block / 8))) &= ~(0x01 << (block % 8))  
#define ALLOCONE(start, block) *((int *)(BITMAP(start) + (block / 8))) |= (0x01 << (block % 8))  
#define SETSUPERBLOCK(start, num) ((struct superblock *) start)->freeblocks = num
#define SETSUPERINODE(start,num)  ((struct superblock *) start)->freeinodes = num
#define GETSUPERBLOCK(start) ((struct superblock *) start)->freeblocks
#define GETSUPERINODE(start)  ((struct superblock *) start)->freeinodes



struct superblock
{
  int freeblocks;
  int freeinodes;
};

void *initialize()
{
  void *filesys = malloc(SIZE);
  SETSUPERBLOCK(filesys, (SIZE / BLOCKSIZE) - (1 + INODEBLOCKS + BITMAPBLOCKS));
  SETSUPERINODE(filesys, INODEBLOCKS * (INODEBLOCKS / INODESIZE)); 
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
}
