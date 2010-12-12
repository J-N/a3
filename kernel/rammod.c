#include <linux/module.h>
#include <linux/config.h>
#include <linux/init.h>
#include <linux/errno.h> /* error codes */
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/tty.h>
#include <linux/vmalloc.h>
#include <linux/string.h>

/* c-code -- Cant use in Kernel
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
*/

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
#define BITMAP(start) start + BLOCKSIZE + INODEBLOCKS * BLOCKSIZE
#define ISALLOC(start, block) *((unsigned char *)BITMAP(start) + (block / 8)) & (0x01 << (block % 8))
#define ALLOCZERO(start, block) *((unsigned char *)BITMAP(start) + (block / 8)) &= ~(0x01 << (block % 8))  
#define ALLOCONE(start, block) *((unsigned char *)BITMAP(start) + (block / 8)) |= (0x01 << (block % 8))  

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

/* attribute structures */
struct ioctl_test_t {
  int field1;
  int field2;
};

#define IOCTL_TEST _IOW(0, 6, struct ioctl_test_t)
static int pseudo_device_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
static struct file_operations pseudo_dev_proc_operations;
static struct proc_dir_entry *proc_entry;
//done with defines and stuff

//function declerations
void *initialize();
int rd_mkdir(char *pathname);
int rd_creat(char *pathname);
int rd_unlink(char *pathname);

void *test; //our filesystem in main

void *GETINODELOC(void *start, int num, int locnum)
{
  if(locnum < 8)
    {  
      return *((void **)(INODE(start,num) + 8 + locnum*4 ));
    }
  else
    {
    if(locnum < 72)
      {
      void *temp = *((void **)(INODE(start,num) +8 + 4*8)) + 4*(locnum - 8);
      return *((void **)temp);
      //return temp;
      }
    else
      {
	void *lvl1 = *((void **) (INODE(start,num) +8 + 4*9)) + 4*((locnum -72)/64);
	void *lvl2 = *((void **) lvl1) + 4*((locnum-72)%64);
	return *((void **)lvl2);
      }
    }

}

void *SETINODELOC(void *start, int num, int locnum, void *ptr)
{
  if(locnum < 8)
    *((void **)(INODE(start,num) + 8 + locnum*4 )) = ptr;
  else
    {
      if(locnum < 72)
	{
	  if((GETINODEIND(test,num,8) == NULL))
	    {
	      int i;
	      void *fblock = NULL;
	      for(i=0;i<BITMAPBLOCKS * BLOCKSIZE * 8;i++)
		{
		  if(~ISALLOC(test,i))
		    {
		      fblock= DATABLOCK(test,i);
		      ALLOCONE(test,i);
		      break;
		    }
		}
	      if(fblock == NULL)
		{
		  printk("<1> Can't allocate index block in SETINODELOC.\n");
		 // exit(1);
		}
	      SETINODEIND(test,num,8,fblock);
	      SETSUPERBLOCK(test,GETSUPERBLOCK(test) - 1);
	    }
	  
	  void *temp = *((void **)(INODE(start,num) +8 + 4*8)) + 4*(locnum - 8);
	  *((void **)temp) = ptr;
	  //return temp;
	}
      else
	{
	  if((GETINODEIND(test,num,9) == NULL))
	    {
	      int i;
	      void *fblock = NULL;
	      for(i=0;i<BITMAPBLOCKS * BLOCKSIZE * 8;i++)
		{
		  if(~ISALLOC(test,i))
		    {
		      fblock= DATABLOCK(test,i);
		      ALLOCONE(test,i);
		      break;
		    }
		}
	      if(fblock == NULL)
		{
		  printk("<1> Can't allocate top lvl double index block in SETINODELOC.\n");
		  //exit(1);
		}
	      SETINODEIND(test,num,9,fblock);
	      SETSUPERBLOCK(test,GETSUPERBLOCK(test) - 1);
	    }
	  
	void *lvl1 = *((void **) (INODE(start,num) +8 + 4*9)) + 4*((locnum -72)/64);
	if(*((void **) lvl1) == NULL)
	  {
	    
	      int i;
	      void *fblock = NULL;
	      for(i=0;i<BITMAPBLOCKS * BLOCKSIZE * 8;i++)
		{
		  if(~ISALLOC(test,i))
		    {
		      fblock= DATABLOCK(test,i);
		      ALLOCONE(test,i);
		      break;
		    }
		}
	      if(fblock == NULL)
		{
		  printk("<1> Can't allocate 2nd lvl double index block in SETINODELOC.\n");
		 //exit(1);
		}
	      *((void **) lvl1) = fblock;
	      SETSUPERBLOCK(test,GETSUPERBLOCK(test) - 1);
	  }
	  void *lvl2 = *((void **) lvl1) + 4*((locnum-72)%64);
	  *((void **)lvl2) = ptr;
	}
     
    }
	
}

static int __init initialization_routine(void)
{
	printk("<1> Inserting RamDisk Module\n");
	printk("<1> Setting up ioctal \n");
	pseudo_dev_proc_operations.ioctl = pseudo_device_ioctl;

	/* Start create proc entry */
	proc_entry = create_proc_entry("ioctl_test", 0444, &proc_root);
	if(!proc_entry)
	{
		printk("<1> Error creating /proc entry.\n");
		return 1;
	}

	proc_entry->owner = THIS_MODULE;
	proc_entry->proc_fops = &pseudo_dev_proc_operations;

	//printk("<1> Setting up Filesystem\n");

	return 0;
}

/***
* ioctl() entry point...
*/
static int pseudo_device_ioctl(struct inode *inode, struct file *file,
unsigned int cmd, unsigned long arg)
{
	struct ioctl_test_t ioc;

	switch (cmd){

	case IOCTL_TEST:
		copy_from_user(&ioc, (struct ioctl_test_t *)arg, 
		sizeof(struct ioctl_test_t));
		int f1 = ioc.field1;
		int f2 = ioc.field2;
		
		printk("<1> Recieved Ioctol call\n");

		printk("<1> Initializing filesystem\n");

		test = initialize();
		if(test == NULL)
		{
			printk("<1> Error Initializing Filesystem\n");
			//exit(1);
		}
		printk("<1> Superblock:\t%x\nFirst Inode:\t%x\nBitmap Start:\t%x\n",(unsigned int)test,(unsigned int)INODE(test,0),(unsigned int)BITMAP(test));

		printk("<1> Second Inode:\t%x\n",(unsigned int)INODE(test,1));

		ALLOCZERO(test, 200);

		printk("<1> ISALLOC TEST:\t%d\n",ISALLOC(test,200));

		ALLOCONE(test, 200);

		printk("<1> ISALLOC TEST:\t%d\n",ISALLOC(test,200));

		printk("<1> Free Blocks:\t%d\nFree Inodes:\t%d\n",GETSUPERBLOCK(test),GETSUPERINODE(test));

		SETINODETYPE(test,100,"reg");

		SETINODESIZE(test,100,1337);

		printk("<1> Inode 100 type:\t%s\nInode 100 size:\t%d\n",GETINODETYPE(test,100),GETINODESIZE(test,100));
		
		printk("<1> Root directory printout:\n");

		printk("<1> Inode type:\t%s\n",GETINODETYPE(test,0));

		printk("<1> Inode size:\t%d\n",GETINODESIZE(test,0));

		printk("<1> Inode location pointer:\t%x\n",(unsigned int)GETINODELOC(test,0,0));

		printk("<1> First Data Block:\t%x\n",(unsigned int)DATABLOCK(test,3));

		SETINODELOC(test,0,8,test);

		printk("<1> This should be superblock:\t%x\n%x\n",((unsigned int ) GETINODELOC(test,0,8)),(unsigned int)(&(*((unsigned int * )DATABLOCK(test,3)))));

		printk("<1> Here goes nothing...\n");

		char *hurp = vmalloc(200);
		int i;
		
		sprintf(hurp,"/test");
	  rd_mkdir(hurp);
	  
	  for(i=0;i<2010;i++)
		{
		  sprintf(hurp,"/test/file%d",i);
		  if(rd_mkdir(hurp) == -1)
		printk("<1> error\n");
		}

	   for(i=2010;i>=0;i--)
		{
		  sprintf(hurp,"/test/file%d",i);
		  if(rd_unlink(hurp) == -1)
		printk("<1> error\n");
		}
		sprintf(hurp,"/test");
   if(rd_unlink(hurp) == -1)
     printk("<1> error\n");
	   /*

	sprintf(hurp,"/test/test2");
	  rd_mkdir(hurp);
	  for(i=0;i<2010;i++)
		{
		  sprintf(hurp,"/test/test2/file%d",i);
		  if(rd_mkdir(hurp) == -1)
		printk("<1> error\n");
		}

	   for(i=2010;i>=0;i--)
		{
		  sprintf(hurp,"/test/test2/file%d",i);
		  if(rd_unlink(hurp) == -1)
		printk("<1> error\n");
		}
	   */
	  printk("<1> Free Blocks:\t%d\nFree Inodes:\t%d\n",GETSUPERBLOCK(test),GETSUPERINODE(test));
/*
	     sprintf(hurp,"/test/test2");
   if(rd_unlink(hurp) == -1)
    printk("<1> error\n");

   sprintf(hurp,"/test");
   if(rd_unlink(hurp) == -1)
     printk("<1> error\n");
	  printk("<1> Free Blocks:\t%d\nFree Inodes:\t%d\n",GETSUPERBLOCK(test),GETSUPERINODE(test));
	 
	  
		strcpy(hurp,"/test/har");

		rd_creat(hurp);
	
		printk("<1> Size of file for inode 1:\t%d\ntype:\t%s\n",GETINODESIZE(test,1),GETINODETYPE(test,1));

		printk("<1> Root direntry for test:\t%s\nInode:\t%hd\n",GETDIRENTNAME(GETINODELOC(test,0,0),0),GETDIRENTINODE(GETINODELOC(test,0,0),0));

		printk("<1> Size of file for inode 2:\t%d\ntype:\t%s\n",GETINODESIZE(test,2),GETINODETYPE(test,2));

		printk("<1> test Directory Entry for har:\t%s\nInode:\t%hd\n",GETDIRENTNAME(GETINODELOC(test,1,0),0),GETDIRENTINODE(GETINODELOC(test,1,0),0));

		printk("<1> Free Blocks:\t%d\nFree Inodes:\t%d\n",GETSUPERBLOCK(test),GETSUPERINODE(test));
		*/
		break;

	default:
		return -EINVAL;
		break;
	}

	return 0;
}

static void __exit cleanup_routine(void)
{
	printk("<1> Unloading RamDisk Module\n");
	remove_proc_entry("ioctl_test", &proc_root);
	return;
}

void *initialize()
{
	void *filesys = vmalloc(FSSIZE);
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
	ALLOCONE(filesys,0);
	//SETINODEIND(filesys,0,8,DATABLOCK(filesys,3));  //test indirect ref
	return filesys;
}

int rd_creat(char *pathname)
{
  const char *delim = "/";
  char *result = NULL;
  char *filename = NULL;
  char *path2 = vmalloc(400);
  strcpy(path2,pathname);
  result = strsep(&path2, delim);
  while(result != NULL)
    {
      filename = result;
      result = strsep(&path2,delim);
    }
  printk("<1> Filename:\t%s\n",filename); // debug
  result = strsep(&pathname, delim);
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
      result = strsep(&pathname,delim);
      if(new == NULL && result != NULL)
	return -1;
      new = NULL;
    }
  printk("<1> Inode where it will be created:%d\n",inode);
  //find a free block for the new file
  for(i=0;i<BITMAPBLOCKS * BLOCKSIZE * 8;i++)
    {
      if(~ISALLOC(test,i))
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
  //printk("<1> %x\n",(unsigned int)GETINODELOC(test,inode,GETINODESIZE(test,inode) / BLOCKSIZE));
  SETDIRENTINODE(GETINODELOC(test,inode,(GETINODESIZE(test,inode) / BLOCKSIZE)),((GETINODESIZE(test,inode)/DIRENTSIZE)%16),newinode);
  SETDIRENTNAME(GETINODELOC(test,inode,(GETINODESIZE(test,inode) / BLOCKSIZE)),((GETINODESIZE(test,inode)/DIRENTSIZE)% 16),filename);
  //  printf("%d\n",GETINODESIZE(test,inode));
  printk("<1> %d %d %s\n",GETINODESIZE(test,inode)/256,((GETINODESIZE(test,inode)/16)% 16),GETDIRENTNAME(GETINODELOC(test,inode,(GETINODESIZE(test,inode)/256)),(GETINODESIZE(test,inode)/16) % 16));

  SETINODESIZE(test,inode,GETINODESIZE(test,inode)+DIRENTSIZE);
  return 0;
}      

int rd_mkdir(char *pathname)
{
  const char *delim = "/";
  char *result = NULL;
  char *filename = NULL;
  char *path2 = vmalloc(400);
  strcpy(path2,pathname);
  result = strsep(&path2, delim);
  
  while(result != NULL)
    {
      filename = result;
      result = strsep(&path2,delim);
    }
  printk("<1> Dirname:\t%s\n",filename); // debug
  result = strsep(&pathname, delim);
  result = strsep(&pathname, delim);
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
			//printk("<1> Dir type correct \n");
			inode = GETDIRENTINODE(place,i%16);
			place = GETINODELOC(test,GETDIRENTINODE(place,i%16),0);
			new = place;
			break;
	    }
	  //New code for John
		if(strcmp(GETDIRENTNAME(place,i%16),result) == 0 && strcmp(GETINODETYPE(test,GETDIRENTINODE(place,i%16)),"reg") == 0)
		{
				printk("<1> Not dir type. \n");
			return -1;
		}
	}
      result = strsep(&pathname,delim);
		if(new == NULL && result != NULL)
		{
				printk("<1> no new and reached the end \n");
				return -1;
		}
      new = NULL;
    }
  //find a free block for the new file
  for(i=0;i<BITMAPBLOCKS * BLOCKSIZE * 8;i++)
    {
		if(~ISALLOC(test,i))
		{
			printk("<1> in data block %d %x\n",i,(unsigned int) DATABLOCK(test,i));
			fblock= DATABLOCK(test,i);
			ALLOCONE(test,i);
			break;
		}
    }
	if(fblock == NULL)
    {
		printk("<1> fblock is null");
		return -1;
	}
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
    {
		printk("<1> New Inode is -1");
		return -1;
	}
  //allocate a new block for the directory if we will grow past its limit
  if((GETINODESIZE(test,inode) / DIRENTSIZE) % 16 == 0  && GETINODESIZE(test,inode) != 0)
    {
		void *newdirblock = NULL;
		for(i=0;i<BITMAPBLOCKS * BLOCKSIZE * 8;i++)
		{
			if(~ISALLOC(test,i))
			{
				newdirblock= DATABLOCK(test,i);
				ALLOCONE(test,i);
				SETINODELOC(test,inode,(GETINODESIZE(test,inode) / 256),newdirblock);
				break;
			}
		}
		if(newdirblock == NULL)
		{
			printk("<1> Newdirblock is null");
			return -1;
		}
      SETSUPERBLOCK(test,GETSUPERBLOCK(test) - 1);
    }
  //printk("<1> %d %d\n",GETINODESIZE(test,inode),(unsigned int)GETINODELOC(test,inode,(GETINODESIZE(test,inode) / BLOCKSIZE)));
  SETSUPERBLOCK(test,GETSUPERBLOCK(test) - 1);
  SETSUPERINODE(test,GETSUPERINODE(test) - 1);
  SETDIRENTINODE(GETINODELOC(test,inode,(GETINODESIZE(test,inode) / BLOCKSIZE)),(GETINODESIZE(test,inode)/DIRENTSIZE)%16,newinode);
  SETDIRENTNAME(GETINODELOC(test,inode,(GETINODESIZE(test,inode) / BLOCKSIZE)),(GETINODESIZE(test,inode)/DIRENTSIZE)%16,filename);
  SETINODESIZE(test,inode,GETINODESIZE(test,inode)+DIRENTSIZE);
  
  return 0;
}    

int rd_unlink(char *pathname)
{
	const char *delim = "/";
	char *result = NULL;
	char *filename = NULL;
	char *path2 = vmalloc(400);
	int k =0;
	strcpy(path2,pathname);
	result = strsep(&path2, delim);
	while(result != NULL)
    {
		k++;
		filename = result;
		result = strsep(&path2,delim);
    }
	printk("<1> Filename:\t%s\n",filename); // debug
	result = strsep(&pathname, delim);
	result = strsep(&pathname, delim);
	int inode = 0;
	void *place = GETINODELOC(test,0,0);
	void *new = NULL;
	int removeinode = 0;
	void *removeplace = NULL;
	int numdirent;
	int i;
	int l=1; //John: may have to be set to 1 in kernel
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
		result = strsep(&pathname, delim);
		if(new == NULL && result != NULL) //Then we have no directory match
		return -1;
		new = NULL;
    }
	//printf("have directory inodes\n");
	//now we have a directory in place and its inode in inode
    numdirent = GETINODESIZE(test,inode) / DIRENTSIZE;
    printk("<1> inode:%d\n",inode);
    printk("<1> numdirent:%d\n",numdirent);
    j=1;
    removeplace = place;
    removeinode = inode;
    new = NULL;
	
	int nc = BLOCKSIZE/DIRENTSIZE;
	
    for(i=0;i<numdirent;i++)
	{
		//printk("<1> i=%d nc=%d, j=%d \n",i,nc,j);
	
		if(i >= j*nc)
	    {
			//printk("<1>  j=%d \n",j);
			removeplace = GETINODELOC(test,removeinode,j);
			j++;
	    }
		//i+2
		char* dname = GETDIRENTNAME(removeplace,i%16);
		
		//printk("<1> filename again:%c\n",dname);
	    
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
		printk("<1> it is here\n");
		return -1;
	}
	
    // printf("have remove and dir inodes\n");
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
	  printk("<1> Removing location block\n");
	}
      for(i=0;i<removeblocks;i++)
	{
	  ALLOCZERO(test,GETBLOCKFROMPTR(test,GETINODELOC(test,removeinode,i)));
	  SETSUPERBLOCK(test,GETSUPERBLOCK(test) + 1);
	  printk("<1> Removing location block\n");
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
	//printf("Dirent Size: %d\n",GETINODESIZE(test,inode));
	if(GETINODESIZE(test,inode) % 256 == 0  && GETINODESIZE(test,inode)!=0) //we can free the empty node
	{
	  
		void *blk = GETINODELOC(test,inode,GETINODESIZE(test,inode) / 256);
		ALLOCZERO(test,GETBLOCKFROMPTR(test,blk));
		SETSUPERBLOCK(test,GETSUPERBLOCK(test) + 1);
		SETINODELOC(test,inode,GETINODESIZE(test,inode)/256,NULL);
		if(GETINODESIZE(test,inode) / 256 == 8)
		{
			ALLOCZERO(test,GETBLOCKFROMPTR(test,GETINODEIND(test,inode,8)));
			SETSUPERBLOCK(test,GETSUPERBLOCK(test) + 1);
			SETINODEIND(test,inode,8,NULL);
		}
	}

    return 0;
}

module_init(initialization_routine);
module_exit(cleanup_routine);
