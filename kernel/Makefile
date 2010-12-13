obj-m += rammod.o
KDIR := /usr/src/kernels/kernel-2.6.18/linux-2.6.18.i686
PWD := $(shell pwd)
SRCS            := ioctl
OBJS            := $(SRCS:.cpp=.o)
CXXFLAGS        := -ggdb -Wall -pedantic
EXE             := i

all:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) clean

$(EXE): $(OBJS)
	$(CXX) $(OBJS) -o $@

userlib:
	g++ -g ioctl.cpp -c -o ioctl.o
	ar rc libramfs.a ioctl.o
	ranlib libramfs.a

usertest: libramfs.a
	g++ -g test1.cpp -L. -lramfs -o test1