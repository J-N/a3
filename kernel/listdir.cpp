#include <iostream>

using namespace std;


int rd_mkdir(char *pathname);
int rd_creat(char *pathname);
int rd_unlink(char *pathname);
int rd_open(char * pathname);
int rd_close(int fd, int pid);
int rd_lseek(int fd, int offset);
int rd_readdir(int fd, char *address);
int rd_write(int fd, char *address, int num_bytes);
int rd_read(int fd, char *address, int num_bytes);

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
	cout<<"Contents of /"<<endl;
	char *testspace = (char *)malloc(20);
	rd_readdir(fd,testspace);
	rd_readdir(fd,testspace);
	free(testspace);
	
	

	
	return 0;
}