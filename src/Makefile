PLATFORM = $(shell ./systype.sh)
LIB = libangel.a
CXXFLAGS = -std=c++11 -Wall -O2

ifeq "$(PLATFORM)" "linux"
	CXX = g++
	OPTION_OBJS = Poll.o Epoll.o Select.o
endif
ifeq "$(PLATFORM)" "macos"
	CXX = clang++
	OPTION_OBJS = Poll.o Kqueue.o Select.o
endif

COMMON_OBJS = Acceptor.o \
			  Buffer.o \
			  Channel.o \
			  Connector.o \
			  EventLoop.o \
			  InetAddr.o \
			  LogStream.o \
			  Logger.o \
			  Signaler.o \
			  Socket.o \
			  TcpClient.o \
			  TcpServer.o \
			  TimeStamp.o \
			  Timer.o \
			  ThreadPool.o \
			  EventLoopThread.o \
			  EventLoopThreadPool.o \
			  SocketOps.o \
			  TcpConnection.o \
			  $(OPTION_OBJS)

%.o : %.cc
	@echo "\033[32m-- [Building CXX object $@]\033[0m"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(LIB) : $(COMMON_OBJS)
	@echo "\033[32m-- [Creating static library $(LIB)]\033[0m"
	@ar -r $(LIB) $(COMMON_OBJS)

.PHONY : clean
clean:
	-rm *.o
