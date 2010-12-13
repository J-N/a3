#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <iostream>
#include <unistd.h>
#include <cerrno>
using namespace std;

#define IOCTL_TEST _IOW(0, 6, struct ioctl_test_t)

/* attribute structures */
	struct ioctl_test_t {
	int field1;
	string field2;
	int field3; //pid
	char *field4; //addresses
	int field5;
	int field6; //return
	int field7; //return
	} ioctl_test;

int iosetup()
{
	int pid = (int) getpid();
	ioctl_test.field3=pid;
	int fd = open ("/proc/ioctl_test", O_RDONLY);
	return fd;
}
void io()
{
	int fd = iosetup();
	int io2=ioctl (fd, IOCTL_TEST, &ioctl_test);
	if(io2==-1)
	{
		cout<<"ioctl failed: "<<errno<<endl;
		perror(NULL);
	}
}

int rd_mkdir(char *pathname)
{
	ioctl_test.field1=1;
	ioctl_test.field2=pathname;
	io();
	
	return ioctl_test.field6;
}

int rd_unlink(char *pathname)
{
	ioctl_test.field1=2;
	ioctl_test.field2=pathname;
	io();
	return ioctl_test.field6;
}

int rd_creat(char *pathname)
{
	ioctl_test.field1=3;
	ioctl_test.field2=pathname;
	io();
	return ioctl_test.field6;
}
int rd_open(char *pathname)
{
	ioctl_test.field1=4;
	ioctl_test.field2=pathname;
	io();
	return ioctl_test.field6;
}
int rd_close(int fd)
{
	ioctl_test.field1=6;
	ioctl_test.field5=fd;
	io();
	return ioctl_test.field6;
}
int rd_readdir(int fd, char *address)
{
	ioctl_test.field1=5;
	ioctl_test.field5=fd;
	ioctl_test.field4=address;
	io();
	printf("Name: %s\n",((char *)(address + 2)));
	return ioctl_test.field6;
}
int rd_read(int fd, char *address, int num_bytes)
{
	ioctl_test.field1=7;
	ioctl_test.field5=fd;
	ioctl_test.field4=address;
	ioctl_test.field7=num_bytes;
	io();

	return ioctl_test.field6;
}
int rd_lseek(int fd, int offset)
{
	ioctl_test.field1=9;
	ioctl_test.field5=fd;
	ioctl_test.field7=offset;
	io();

	return ioctl_test.field6;
}
int rd_write(int fd, char *address, int num_bytes)
{
	ioctl_test.field1=8;
	ioctl_test.field5=fd;
	ioctl_test.field4=address;
	ioctl_test.field7=num_bytes;
	io();

	return ioctl_test.field6;
}

int main () 
{
	int result = rd_mkdir("/hi");
	if(result != 0)
	{
		cout<<"Error Creating Directory test"<<endl;
	}
	result = rd_creat("/hi/test");
	if(result != 0)
	{
		cout<<"Error Creating file test"<<endl;
	}

	int fd = rd_open("/hi");
	if(result != -1)
	{
		cout<<"file descriptor for /hi is: "<<fd<<endl;
	}
	int fd2 = rd_open("/hi/test");
	
	if(result != -1)
	{
		cout<<"file descriptor for /test is: "<<fd2<<endl;
	}
	int fd3 = rd_open("/");
	if(result != -1)
	{
		cout<<"file descriptor for / is: "<<fd3<<endl;
	}
	
	char *testspace = (char *)malloc(20);
	rd_readdir(fd,testspace);
	free(testspace);
	
	int seek = rd_lseek(fd2,0);
	cout<<"seek1 return "<<seek<<endl;
	char *data = (char *)malloc(10);
	sprintf(data,"this test");
	result = rd_write(fd2,data,10);
	
		
		cout<<"write return "<<result<<endl;
		
	seek = rd_lseek(fd2,0);
		cout<<"seek return "<<seek<<endl;
char *rresult = (char *)malloc(10);
	//sprintf(data,"this test");
	result = rd_read(fd2,rresult,10);
		cout<<"bytes read: "<<result<<endl;
		cout<<"read returned: "<<rresult<<endl;
		
	
	return 0;
}
