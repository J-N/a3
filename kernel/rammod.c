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

void *test; //our filesystem in main

struct superblock
{
	int freeblocks;
	int freeinodes;
};

struct Myinode
{
	int size;
	void *location[10];
	char *type;
};

struct dirent
{
	unsigned short Myinode;
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
				printk("<1> Can't allocate index block in SETINODELOC.\n");
				//exit(1);
			}
			SETINODEIND(test,num,8,fblock);
			SETSUPERBLOCK(test,GETSUPERBLOCK(test) - 1);
		}

		void *temp = *((void **)(INODE(start,num) +8 + 4*8)) + 4*(locnum - 8);
		*((void **)temp) = ptr;
		//return temp;
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

		strcpy(hurp,"/test");

	rd_mkdir(hurp);

		strcpy(hurp,"/test/har");

		rd_creat(hurp);
/*	
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
	//SETINODEIND(filesys,0,8,DATABLOCK(filesys,3));  //test indirect ref
	return filesys;
}

int rd_creat(char *pathname)
{
	
	const char delim[] = "/";
	char *result = NULL;
	char *filename = NULL;
	/*
	result = strsep(&pathname, delim);
	
	while(result != NULL)
	{
		filename = result;
		result = strsep(&pathname,delim);
	}
	printk("<1> Filename:\t%s\n",filename); // debug
	*/
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
		for(i=0;i<numdirent;i++)
		{
			if(i> j*(BLOCKSIZE/DIRENTSIZE))
			{
				place = GETINODELOC(test,inode,j-1);
				j++;
			}
			if(strcmp(GETDIRENTNAME(place,i),result) == 0 && strcmp(GETINODETYPE(test,GETDIRENTINODE(place,i)),"dir") == 0)
			{
				inode = GETDIRENTINODE(place,i);
				place = GETINODELOC(test,GETDIRENTINODE(place,i),0);
				break;
			}
		}
		result = strsep(&pathname,delim);
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
	if((GETINODESIZE(test,inode) / DIRENTSIZE) % 16 == 0 && GETINODESIZE(test,inode) != 0)
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
		SETSUPERBLOCK(test,GETSUPERBLOCK(test) - 1);
	}

	SETSUPERBLOCK(test,GETSUPERBLOCK(test) - 1);
	SETSUPERINODE(test,GETSUPERINODE(test) - 1);
	SETDIRENTINODE(GETINODELOC(test,inode,(GETINODESIZE(test,inode) / BLOCKSIZE)),(GETINODESIZE(test,inode)/DIRENTSIZE),newinode);
	SETDIRENTNAME(GETINODELOC(test,inode,(GETINODESIZE(test,inode) / BLOCKSIZE)),(GETINODESIZE(test,inode)/DIRENTSIZE),filename);
	SETINODESIZE(test,inode,GETINODESIZE(test,inode)+DIRENTSIZE);
	
	return 0;
}

int rd_mkdir(char *pathname)
{
	const char delim[] = "/";
	char *result = NULL;
	char *filename = NULL;
	/*result = strsep(&pathname, delim);
	
	while(result != NULL)
	{
		filename = result;
		result = strsep(&pathname,delim);
	}
	
	printk("<1> Dirname: %s\n",filename); // debug
	*/
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
	printk("<1> About do checks on path and stuff\n"); // debug
	while(result != NULL)
	{
		filename = result;
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
				printk("<1> debug hurp case\n"); // debug
				inode = GETDIRENTINODE(place,i);
				place = GETINODELOC(test,GETDIRENTINODE(place,i),0);
				break;
			}
		}
		printk("<1> Dirname: %s\n",filename); // debug
		result = strsep(&pathname,delim);
		if(new == NULL && result != NULL)
		{
			printk("<1> returning -1"); // debug
			return -1;
		}
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
				SETINODELOC(test,inode,(GETINODESIZE(test,inode) / 256) + 1,newdirblock);
				break;
			}
		}
		if(newdirblock == NULL)
		return -1;
		SETSUPERBLOCK(test,GETSUPERBLOCK(test) - 1);
	}
	SETSUPERBLOCK(test,GETSUPERBLOCK(test) - 1);
	SETSUPERINODE(test,GETSUPERINODE(test) - 1);
	SETDIRENTINODE(GETINODELOC(test,inode,(GETINODESIZE(test,inode) / BLOCKSIZE)),(GETINODESIZE(test,inode)/DIRENTSIZE),newinode);
	SETDIRENTNAME(GETINODELOC(test,inode,(GETINODESIZE(test,inode) / BLOCKSIZE)),(GETINODESIZE(test,inode)/DIRENTSIZE),filename);
	SETINODESIZE(test,inode,GETINODESIZE(test,inode)+DIRENTSIZE);
	
	return 0;
}      

module_init(initialization_routine);
module_exit(cleanup_routine);