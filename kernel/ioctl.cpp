#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <iostream>
#include <unistd.h>

using namespace std;

#define IOCTL_TEST _IOW(0, 6, struct ioctl_test_t)
int main () {

	/* attribute structures */
	struct ioctl_test_t {
	int field1;
	int field2;
	} ioctl_test;

	int fd = open ("/proc/ioctl_test", O_RDONLY);

	cout<<"Starting Filesystem..."<<endl;
	ioctl_test.field1=1;
	ioctl_test.field2=1;

	ioctl (fd, IOCTL_TEST, &ioctl_test);
	
  return 0;
}
