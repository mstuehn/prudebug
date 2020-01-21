CC ?= $(CROSS_COMPILE)gcc
CXX ?= $(CROSS_COMPILE)g++

OBJS = prudbg.o cmdinput.o cmd.o printhelp.o da.o uio.o console.o dbgfile.o

DEFINES = -DUSE_EDITLINE
LIBS = -ledit
CFLAGS += -std=gnu99
CXXFLAGS += -std=c++11

all: prudebug

clean :
	rm -f *.o prudebug


prudebug: $(OBJS)
	$(CXX) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -g -c -o $@ $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(DEFINES) $(INCLUDES) -g -c -o $@ $<
