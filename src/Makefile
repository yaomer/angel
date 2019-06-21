PLATFORM = $(shell ./systype.sh)
LIB = libangel.a
CXXFLAGS = -std=c++11 -Wall -O2
_GREEN = \033[32m
GREEN_ = \033[0m

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
			  SocketOps.o \
			  TcpConnection.o \
			  $(OPTION_OBJS)

%.o : %.cc
	@echo "$(_GREEN)-- [Building CXX object $@]$(GREEN_)"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(LIB) : $(COMMON_OBJS)
	@echo "$(_GREEN)-- [Creating static library $(LIB)]$(GREEN_)"
	@ar -r $(LIB) $(COMMON_OBJS)

.PHONY : clean
clean:
	-rm *.o
