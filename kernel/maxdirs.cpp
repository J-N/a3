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
	rd_mkdir("/test");
	rd_mkdir("/test2");
	int fd = rd_open("/");
	char* dname = (char*) malloc(30);
	int result = rd_readdir(fd,dname);
	return 0;
}
