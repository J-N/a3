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

.cpp.o:
	$(CXX) $(CXXFLAGS) -c $< -o $@