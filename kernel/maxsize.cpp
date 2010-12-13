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
	int i=0;

	int result = rd_creat("/bigfile");
	if(result != 0)
	{
		cout<<"Error creating file"<<endl;
	}
	int fd = rd_open("/bigfile");
	char* big = (char*)malloc(350);

	//2013184
	int write;
	big[0]='j';

	rd_lseek(fd,0);
	
	sprintf(big,"123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890");
  int writenum;
  for(i=0;i<61;i++)
    {
      writenum = rd_write(fd,big,300);
    }
  writenum = rd_write(fd,big,100);
	
	char* bigr = (char*) malloc(2013184);
	rd_lseek(fd,0);
	int read = rd_read(fd,bigr,2013184);
	cout<<"read "<<read<<" bytes"<<endl;
	int t;
	cout<<"Enter 1 to see the results of read or 0 to skip"<<endl;
	cin>>t;
	if(t==1)
	{
		for(i=0;i<2000;i++)
			printf("%c",*bigr++);
	}
	cout<<endl;
	
	result = rd_unlink("/bigfile");
	if(result != 0)
	{
		cout<<"Error deleting file"<<endl;
	}
	
	return 0;
}
