#ifndef _ANGEL_TCPCLIENT_H
#define _ANGEL_TCPCLIENT_H

#include "Connector.h"
#include "InetAddr.h"
#include "Channel.h"
#include "Buffer.h"

namespace Angel {

class EventLoop;

class TcpClient {
public:
    typedef std::function<void(Channel&)> ConnectionCallback;
    typedef std::function<void(std::shared_ptr<Channel>, Buffer&)> MessageCallback;
    typedef std::function<void(Channel&, Buffer&)> StdinCallback;
    TcpClient(EventLoop *, InetAddr&);
    ~TcpClient();
    void start();
    void quit();
    void listenStdin();
    void setConnectionCb(const ConnectionCallback _cb)
    { 
        _connector.setConnectionCb(_cb); 
    }
    void setMessageCb(const MessageCallback _cb)
    {
        _connector.setMessageCb(_cb);
    }
    void setStdinCb(const StdinCallback _cb)
    { _stdinCb = _cb; }
private:
    void readStdin();

    EventLoop *_loop;
    Connector _connector;
    StdinCallback _stdinCb;
};

}

#endif // _ANGEL_TCPCLIENT_H
