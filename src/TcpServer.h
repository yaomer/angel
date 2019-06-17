#ifndef _ANGEL_TCPSERVER_H
#define _ANGEL_TCPSERVER_H

#include <functional>
#include <memory>
#include "Acceptor.h"
#include "InetAddr.h"

namespace Angel {

class EventLoop;

class TcpServer {
public:
    explicit TcpServer(EventLoop *, InetAddr&);
    ~TcpServer();
    void start();
    void quit();
    void setAcceptionCb(const Acceptor::AcceptionCallback _cb)
    {
        _acceptor.setAcceptionCb(_cb);
    }
    void setMessageCb(const Acceptor::MessageCallback _cb)
    {
        _acceptor.setMessageCb(_cb);
    }
private:
    EventLoop *_loop;
    Acceptor _acceptor;
};

}

#endif // _ANGEL_TCPSERVER_H
