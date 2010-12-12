#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <iostream>
#include <unistd.h>
#include <cerrno>
using namespace std;

#define IOCTL_TEST _IOW(0, 6, struct ioctl_test_t)
int main () {

	/* attribute structures */
	struct ioctl_test_t {
	int field1;
	string field2;
	} ioctl_test;

	int fd = open ("/proc/ioctl_test", O_RDONLY);
	string cmd,arg;
	cout<<"Welcome to the RAM File System"<<endl;
	//cout<<""<<endl;
	while(1)
	{
		cin>>cmd>>ioctl_test.field2;
		//switch to space based
		size_t found=cmd.find(" "); 
		//if (found!=string::npos)
		//arg=cmd.substr(found);
		//cmd=cmd.substr(0,found);
		int valid=0,io=-1;
		if(cmd=="mkdir")
		{
			ioctl_test.field1=1;
			valid=1;
		}
		if(cmd=="rmdir")
		{
			ioctl_test.field1=2;
			valid=1;
		}
		if(cmd=="rm")
		{
			ioctl_test.field1=2;
			valid=1;
		}
		if(cmd=="creat")
		{
			ioctl_test.field1=3;
			valid=1;
		}
		if(valid==1)
		{
			io=ioctl (fd, IOCTL_TEST, &ioctl_test);
			valid=0;
			if(io==-1)
			{
				cout<<"ioctl failed: "<<errno<<endl;
				perror(NULL);
			}
		}
		else
		{
			cout<<cmd<<" is not a valid command"<<endl;
		}
	}
  return 0;
}
