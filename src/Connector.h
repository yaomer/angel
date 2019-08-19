#ifndef _ANGEL_CONNECTOR
#define _ANGEL_CONNECTOR

#include <memory>
#include <functional>

#include "Socket.h"
#include "InetAddr.h"
#include "Channel.h"
#include "Buffer.h"
#include "noncopyable.h"
#include "decls.h"

namespace Angel {

class EventLoop;

class Connector : noncopyable {
public:
    Connector(EventLoop *, InetAddr&);
    ~Connector() {  };
    void connect();
    void connecting(int fd);
    void connected(int fd);
    void timeout();
    void check(int fd);
    int connectWaitTime() const { return _waitTime; }
    void setConnectWaitTime(int time) { _waitTime = time;  }
    bool isConnected() { return _connected; }
    void setNewConnectionCb(const NewConnectionCallback _cb)
    { _newConnectionCb = std::move(_cb); }

    static size_t _defaultWaitTime;
private:
    EventLoop *_loop;
    std::shared_ptr<Channel> _connectChannel;
    InetAddr _peerAddr;
    NewConnectionCallback _newConnectionCb;
    int _connected;
    int _waitTime;
};
}

#endif // _ANGEL_CONNECTOR
