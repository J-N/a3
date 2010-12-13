#include <linux/module.h>
#include <linux/config.h>
#include <linux/init.h>
#include <linux/errno.h> /* error codes */
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/tty.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/sched.h>

/* c-code -- Cant use in Kernel lol
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
struct fileobj
{
  int offset;
  void *inode;
  int inodenum;
  
};

struct fdt
{
  int pid;
  struct fileobj table[20];
};

struct ioctl_test_t {
	int field1;
	int field2;
	int field3;
	char *field4;
	int field5;
	int field6;
	int field7;
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
int rd_open(char * pathname, int pid);
int rd_close(int fd, int pid);
int rd_lseek(int fd, int offset, int pid);
int rd_readdir(int fd, char *address, int pid);
int rd_write(int fd, char *address, int num_bytes, int pid);
int rd_read(int fd, char *address, int num_bytes, int pid);

void *test; //our filesystem in main
struct fdt tablearray[20];

void my_printk(char *string) 
{ 
  struct tty_struct *my_tty;

  my_tty = current->signal->tty;

  if (my_tty != NULL) { 
    (*my_tty->driver->write)(my_tty, string, strlen(string)); 
    (*my_tty->driver->write)(my_tty, "\015\012", 2); 
  } 
} 

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


	printk("<1> Initializing filesystem\n");
	test = initialize();
	if(test == NULL)
	{
		printk("<1> Error Initializing Filesystem\n");
	}
	printk("<1> Superblock:\t%x\nFirst Inode:\t%x\nBitmap Start:\t%x\n",(unsigned int)test,(unsigned int)INODE(test,0),(unsigned int)BITMAP(test));
	printk("<1> Second Inode:\t%x\n",(unsigned int)INODE(test,1));
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
		copy_from_user(&ioc, (struct ioctl_test_t *)arg,sizeof(struct ioctl_test_t));
		
		int f1 = ioc.field1;
		char *f2 = (char *)ioc.field2;
		int pid = ioc.field3;
		char *f4 = ioc.field4;
		int f5 = ioc.field5;
		int f6 = ioc.field6;
		int f7 = ioc.field7;
		
		printk("<1> Recieved ioctol call from pid:%d\n",pid);

		int tableent=-1;
		int i;
		char *hurp = vmalloc(200);
		//search to see if current pid already has an entry
		for(i=0;i<20;i++)
		{
			if(tablearray[i].pid!=-31337)//check to see if process is still alive
			{
				struct task_struct *old = find_task_by_pid(tablearray[i].pid);
				if(old==NULL)//process died
				{
					printk("<1> Process: %d has died... Clearing slot: %d\n",tablearray[i].pid,i);
					tablearray[i].pid=-31337;
				}
			}
			if(tablearray[i].pid==pid) //found entry
			{
				tableent=i;
				printk("<1> Pid already has table at:%d\n",tableent);
				break;
			}
		}
		if(tableent==-1)//check to see if we already have entry, if not find an entry
		{
			//printk("<1> table ent:%d\n",tableent);
			for(i=0;i<20;i++)
			{
				if(tablearray[i].pid==-31337) //empty entry
				{
					printk("<1> table created at:%d\n",i);
					tablearray[i].pid=pid; //set pid
					break;
				}
			}
		}
		
		if(f1==1)
		{
			//sprintf(hurp,"/%s",f2);
			printk("<1> Creating dir %s\n",f2);
			f6=rd_mkdir(f2);
			if(f6 == -1)
			{
				printk("<1> error\n");
			}
		}
		if(f1==2)
		{
			//sprintf(hurp,"/%s",f2);
			printk("<1> Removing %s\n",f2);
			f6=rd_unlink(f2);
			if(f6 == -1)
			{
				printk("<1> error\n");
			}
		}
		if(f1==3)
		{
			//sprintf(hurp,"/%s",f2);
			printk("<1> Creating file %s\n",f2);
			f6=rd_creat(f2);
			if(f6 == -1)
			{
				printk("<1> error\n");
			}
		}
		if(f1==4)//open
		{
			sprintf(hurp,"%s",f2);
			f6=rd_open(hurp,pid);
		}
		if(f1==5) //readdir
		{
			int test2 = f5;
			char* temp = (char*) vmalloc(20);
			f6=rd_readdir(test2,temp,pid);
			if(f6 == -1)
			{
				printk("<1> error\n");
			}
			
			copy_to_user(f4,temp,16);
			
			printk("<1> inode: %hu\n name: %s\n",*((unsigned short *) temp), ((char *)(temp + 2)));
			vfree(temp);
		}
		if(f1==6) //close
		{
			f6=rd_close(f5,pid);
		}
		if(f1==7) //read
		{
			int fd = f5;
			int bytes = f7;
			char* temp = (char*) vmalloc(bytes);
			
			f6=rd_read(fd,temp,bytes,pid);
			printk("<1> read: %d\n",f6);
			copy_to_user(f4,temp,bytes);

			//printk("<1> inode: %hu\n name: %s\n",*((unsigned short *) temp), ((char *)(temp + 2)));
			vfree(temp);
		}
		if(f1==8) //write
		{
			int fd = f5;
			int bytes = f7;
			char* temp = (char*) vmalloc(bytes);
			copy_from_user(temp,f4,bytes);
			printk("<1> write: %s \n",temp);
			f6=rd_write(fd,temp,bytes,pid);
			
			//printk("<1> inode: %hu\n name: %s\n",*((unsigned short *) temp), ((char *)(temp + 2)));
			vfree(temp);
		}
		if(f1==8) //lseek
		{
			int fd = f5;
			int offset = f7;
			f6=rd_lseek(fd,offset,pid);
		}
		printk("<1> Free Blocks:\t%d\nFree Inodes:\t%d\n",GETSUPERBLOCK(test),GETSUPERINODE(test));
		ioc.field6=f6; //set the return
		copy_to_user((struct ioctl_test_t *)arg,&ioc,sizeof(struct ioctl_test_t));
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
	vfree(test);
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
	for(i=0;i<20;i++)
	{
		tablearray[i].pid = -31337;
	}
	return filesys;
}

int rd_open(char * pathname, int pid)
{
  
  const char *delim = "/";
  char *result = NULL;
  char *filename = NULL;
  char *path2 = vmalloc(400);
  int root = 0;
  if(strcmp(pathname, "/") == 0)
    root = 1;
  int k =0;
  strcpy(path2,pathname);
  result = strsep(&path2, delim);
  result = strsep(&path2, delim);
  while(result != NULL)
    {
      k++;
      filename = result;
      result = strsep(&path2, delim);
    }
  printk("<1> Filename:\t%s\n",filename); // debug
  vfree(path2);
  result = strsep(&pathname, delim);
  result = strsep(&pathname, delim);
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

      result = strsep(&pathname, delim);
      
      if(new == NULL && result != NULL) //Then we have no directory match
	return -1;
      new = NULL;
    }
  //printf("have directory inodes\n");
  //now we have a directory in place and its inode in inode
      numdirent = GETINODESIZE(test,inode) / DIRENTSIZE;
      //printf("inode:%d\n",inode);
      //printf("numdirent:%d\n",numdirent);
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
	   if(root)
	     break;
	   //printf("filename again:%s\n",GETDIRENTNAME(removeplace,i+2));
	    if(strcmp(GETDIRENTNAME(removeplace,i%16),filename) == 0)
	    {
	      removeinode = GETDIRENTINODE(removeplace,i%16);
	      removeplace = GETINODELOC(test,GETDIRENTINODE(removeplace,i%16),0);
	      new = removeplace;
	      break;
	    }
	}
      if(new == NULL && !root)
	{
	  printk("<1> it is here\n");
	  return -1;
	}
      struct fdt *freeptr = NULL;
      struct fdt *ourptr = NULL;
      for(i=0;i<20;i++)
	{
	  if(tablearray[i].pid == pid)
	    {  
	      ourptr = &tablearray[i];
	      break;
	    }
	  if(tablearray[i].pid == -31337)
	    {
	      freeptr = &tablearray[i];
	      //printf("table entry:\t%i\n",i);
	    }
	}
      if(freeptr == NULL && ourptr == NULL)
	return -1;
      if(ourptr != NULL)
	{
	  for(i=0;i<20;i++)
	    {
	      if(ourptr->table[i].inode == NULL)
		{
			printk("<1> inode:%d\n",removeinode);
		  ourptr->table[i].offset = 0;
		  ourptr->table[i].inode = INODE(test,removeinode);
		  ourptr->table[i].inodenum = removeinode;
		  return i;
		}
	    }
	  return -1;
	}
      else
	{
	  freeptr->pid = pid;
	  freeptr->table[0].offset = 0;
	  freeptr->table[0].inode = INODE(test,removeinode);
	  freeptr->table[0].inodenum = removeinode;
	  return 0;
	}

}
int rd_close(int fd, int pid)
{
  int i;
  struct fdt *ourptr = NULL;
  for(i=0;i<20;i++)
	{
	  if(tablearray[i].pid == pid)
	    {  
	      ourptr = &tablearray[i];
	      break;
	    }	  
	}
  if(ourptr == NULL)
    return -1;
  if(ourptr->table[fd].inode == NULL)
    return -1;
  else
    {
      ourptr->table[fd].inode = NULL;
      return 0;
    }
}

int rd_lseek(int fd, int offset, int pid)
{
  int i;
  struct fdt *ourptr = NULL;
  for(i=0;i<20;i++)
	{
	  if(tablearray[i].pid == pid)
	    {  
	      ourptr = &tablearray[i];
	      break;
	    }	  
	}
  if(ourptr == NULL)
   return -1;
  if(offset > *((int *) (ourptr->table[fd].inode)))
    {
      ourptr->table[fd].offset = *((int *) (ourptr->table[fd].inode));
      return ourptr->table[fd].offset;
    }
  else
    {
      ourptr->table[fd].offset = offset;
      return ourptr->table[fd].offset;
    }
}

int rd_read(int fd, char *address, int num_bytes, int pid)
{
  int i;
  int bytes_left = num_bytes;
  int size;
  void *blk;
  struct fdt *ourptr = NULL;
  for(i=0;i<20;i++)
	{
	  if(tablearray[i].pid == pid)
	    {  
	      ourptr = &tablearray[i];
	      break;
	    }	  
	}
  if(ourptr == NULL)
   return 0;
  if(strcmp(GETINODETYPE(test,ourptr->table[fd].inodenum),"reg") != 0)
    return 0;
   while(bytes_left > 0)
    {
      size = GETINODESIZE(test,ourptr->table[fd].inodenum);
      //do we need to allocate a new block?
      if((ourptr->table[fd].offset / 256) > (size - 1) / 256)
	{
	  return num_bytes - bytes_left;
	}
      //ok, we're set up with a new block
	  
      blk = GETINODELOC(test,ourptr->table[fd].inodenum,(ourptr->table[fd].offset / 256));
      int blockpos = ourptr->table[fd].offset % 256;
      for(i=blockpos;i<256;i++)
	{
	  *((unsigned char *) address++) = *((unsigned char *) blk + i);
	  bytes_left --;
	  ourptr->table[fd].offset ++;
	  if(ourptr->table[fd].offset > GETINODESIZE(test,ourptr->table[fd].inodenum) - 1)
	    {
	      return num_bytes - bytes_left;
	    }
	  if(bytes_left == 0)
	    return num_bytes;
	}
    }
  return num_bytes;
}

int rd_readdir(int fd, char *address, int pid)
{
	int i;
	void *blk;
	struct fdt *ourptr = NULL;
	for(i=0;i<20;i++)
	{
		if(tablearray[i].pid == pid)
	    {  
			ourptr = &tablearray[i];
			break;
	    }	  
	}
	if(ourptr == NULL)
		return -1;
	if(strcmp(GETINODETYPE(test,ourptr->table[fd].inodenum),"dir") != 0)
		return -1;
	if(ourptr->table[fd].offset > GETINODESIZE(test,ourptr->table[fd].inodenum))
		return 0;
	blk = GETINODELOC(test,ourptr->table[fd].inodenum,ourptr->table[fd].offset / 256);
	int blockpos = ourptr->table[fd].offset % 256;
	int dirpos = blockpos / 16;
	*((unsigned short *)address) = GETDIRENTINODE(blk,dirpos);
	//*((char **)(address + 2)) = GETDIRENTNAME(blk,dirpos);
	strcpy((char *) address + 2,GETDIRENTNAME(blk,dirpos));
	ourptr->table[fd].offset += 16;
	return 1;
}

int rd_creat(char *pathname)
{
	const char *delim = "/";
	char *result = NULL;
	int k = 0;
	char *filename = NULL;
	char *path2 = vmalloc(400);
	strcpy(path2,pathname);
	result = strsep(&path2, delim);
	while(result != NULL)
    {
		k++;
		filename = result;
		result = strsep(&path2,delim);
    }
	printk("<1> Filename: %s\n",filename); // debug
	result = strsep(&pathname, delim);
	result = strsep(&pathname, delim);
	int inode = 0;
	void *fblock = NULL;
	void *place = GETINODELOC(test,0,0);
	int newinode = -1;
	int numdirent;
	int l = 0;
	void *new = NULL;
	int i;
	int j = 1;
	//check path, store inode # of dir to put file into in inode, and place as its current location.
	while(result != NULL)
    {
		l++;
		numdirent = GETINODESIZE(test,inode) / DIRENTSIZE;
		j=1;
		for(i=0;i<numdirent;i++)
		{	
			if(i>= j*(BLOCKSIZE/DIRENTSIZE) && l!=k)
			{
				place = GETINODELOC(test,inode,j);
				j++;
			}
			if(strcmp(GETDIRENTNAME(place,i%16),result) == 0 && strcmp(GETINODETYPE(test,GETDIRENTINODE(place,i%16)),"dir") == 0 && l!=k)
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
	 numdirent = GETINODESIZE(test,inode) / DIRENTSIZE;
      //printf("inode:%d\n",inode);
      //printf("numdirent:%d\n",numdirent);
      j=1;
      void *fileplace = place;
      int fileinode = inode;
      new = NULL;
      for(i=0;i<numdirent;i++)
	{
	   if(i>= j*(BLOCKSIZE/DIRENTSIZE))
	    {
	      fileplace = GETINODELOC(test,fileinode,j);
	      j++;
	    }
	   //printf("filename again:%s\n",GETDIRENTNAME(removeplace,i+2));
	    if(strcmp(GETDIRENTNAME(place,i%16),filename) == 0)
	    {
	      fileinode = GETDIRENTINODE(fileplace,i%16);
	      fileplace = GETINODELOC(test,GETDIRENTINODE(fileplace,i%16),0);
	      new = fileplace;
	      break;
	    }
	}
      if(new != NULL)
	{
	  printk("<1> it is here\n");
	  return -1;
	}
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
  vfree(path2);
  return 0;
}      

int rd_mkdir(char *pathname)
{
	const char *delim = "/";
	char *result = NULL;
	char *filename = NULL;
	char *path2 = vmalloc(400);
	int k = 0;
	strcpy(path2,pathname);
	result = strsep(&path2, delim);
  
	while(result != NULL)
    {
		k++;
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
  int l = 0;
  int j = 1;
  //check path, store inode # of dir to put file into in inode, and place as its current location.
  while(result != NULL)
    {
		l++;
		numdirent = GETINODESIZE(test,inode) / DIRENTSIZE;
		j=1;
		for(i=0;i<numdirent;i++)
		{
			if(i>= j*(BLOCKSIZE/DIRENTSIZE) && l!=k)
			{
				place = GETINODELOC(test,inode,j);
				j++;
			}
	  if(strcmp(GETDIRENTNAME(place,i%16),result) == 0 && strcmp(GETINODETYPE(test,GETDIRENTINODE(place,i%16)),"dir") == 0 && l!=k)
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
	numdirent = GETINODESIZE(test,inode) / DIRENTSIZE;
      //printf("inode:%d\n",inode);
      //printf("numdirent:%d\n",numdirent);
      j=1;
      void *fileplace = place;
      int fileinode = inode;
      new = NULL;
      for(i=0;i<numdirent;i++)
	{
	   if(i>= j*(BLOCKSIZE/DIRENTSIZE))
	    {
	      fileplace = GETINODELOC(test,fileinode,j);
	      j++;
	    }
	   //printf("filename again:%s\n",GETDIRENTNAME(removeplace,i+2));
	    if(strcmp(GETDIRENTNAME(place,i%16),filename) == 0)
	    {
	      fileinode = GETDIRENTINODE(fileplace,i%16);
	      fileplace = GETINODELOC(test,GETDIRENTINODE(fileplace,i%16),0);
	      new = fileplace;
	      break;
	    }
	}
      if(new != NULL)
	{
	  printk("<1> it is here\n");
	  return -1;
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
  vfree(path2);
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
		if(i >= j*nc)
	    {
			removeplace = GETINODELOC(test,removeinode,j);
			j++;
	    }
		
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
      int removeblocks = (GETINODESIZE(test,removeinode)+255) / BLOCKSIZE;
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
    if(removeblocks > 8)
	{
		ALLOCZERO(test,GETBLOCKFROMPTR(test,GETINODEIND(test,removeinode,8)));
		SETSUPERBLOCK(test,GETSUPERBLOCK(test) + 1);
	}
    //remove double indirect index blocks
    if(removeblocks > 72)
	{
		void * doubleind = GETINODEIND(test,removeinode,9);
		for(i=0;i<64;i++)
		{
			if(i <= (((GETINODESIZE(test,removeinode) - 1)/256) - 72)/64)
			{
				ALLOCZERO(test,GETBLOCKFROMPTR(test,(void *)(*((unsigned int *) doubleind + i))));
				SETSUPERBLOCK(test,GETSUPERBLOCK(test) + 1);
 				printk("<1> remove blk ptr %d\n",i);
 		    }
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
	vfree(path2);
    return 0;
}

int rd_write(int fd, char *address, int num_bytes, int pid)
{
  int i;
  int bytes_left = num_bytes;
  int size;
  void *blk;
  struct fdt *ourptr = NULL;
  for(i=0;i<20;i++)
	{
	  if(tablearray[i].pid == pid)
	    {  
	      ourptr = &tablearray[i];
	      break;
	    }	  
	}
  if(ourptr == NULL)
   return 0;
  if(strcmp(GETINODETYPE(test,ourptr->table[fd].inodenum),"reg") != 0)
    return 0;
  while(bytes_left > 0)
    {
      size = GETINODESIZE(test,ourptr->table[fd].inodenum);
      //do we need to allocate a new block?
      if((ourptr->table[fd].offset / 256) > (size - 1) / 256)
	{
	        void *newdirblock = NULL;
      for(i=0;i<BITMAPBLOCKS * BLOCKSIZE * 8;i++)
	{
	  if(~ISALLOC(test,i))
	    {
	      newdirblock= DATABLOCK(test,i);
	      ALLOCONE(test,i);
	      SETINODELOC(test,ourptr->table[fd].inodenum,(GETINODESIZE(test,ourptr->table[fd].inodenum) / 256),newdirblock);
	      break;
	    }
	}
      if(newdirblock == NULL)
	return num_bytes - bytes_left;
      SETSUPERBLOCK(test,GETSUPERBLOCK(test) - 1);
	}
      //ok, we're set up with a new block
	  
      blk = GETINODELOC(test,ourptr->table[fd].inodenum,(ourptr->table[fd].offset / 256));
      int blockpos = ourptr->table[fd].offset % 256;
      for(i=blockpos;i<256;i++)
	{
	  *((unsigned char *) blk + i) = *((unsigned char *) address++);
	  bytes_left --;
	  ourptr->table[fd].offset ++;
	  if(ourptr->table[fd].offset > GETINODESIZE(test,ourptr->table[fd].inodenum) - 1)
	    {
	      SETINODESIZE(test,ourptr->table[fd].inodenum,GETINODESIZE(test,ourptr->table[fd].inodenum) + 1);
	    }
	  if(bytes_left == 0)
	    return num_bytes;
	}
    }
  return num_bytes;
}

module_init(initialization_routine);
module_exit(cleanup_routine);
