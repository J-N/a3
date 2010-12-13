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
//1023
	int i=0;
	char* name= (char*)malloc(20);
	for(i=0;i<1024;i++)
	{
		sprintf(name,"/dir%d",i);
		int result = rd_mkdir(name);
		if(result != 0)
		{
			cout<<"Error creating dir"<<i<<endl;
		}
	}
	for(i=0;i<1024;i++)
	{
		sprintf(name,"/dir%d",i);
		int result = rd_unlink(name);
		if(result != 0)
		{
			cout<<"Error deleting dir"<<i<<endl;
		}
	}
	
	return 0;
}
