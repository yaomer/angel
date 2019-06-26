#ifndef _ANGEL_TCPCONNECTION_H
#define _ANGEL_TCPCONNECTION_H

#include <functional>
#include <memory>
#include <atomic>
#include "Channel.h"
#include "Buffer.h"
#include "InetAddr.h"
#include "Socket.h"
#include "noncopyable.h"
#include "decls.h"

#include <boost/any.hpp>

namespace Angel {

class EventLoop;

class TcpConnection : noncopyable,
    public std::enable_shared_from_this<TcpConnection> {
public:
    TcpConnection(size_t id,
                  EventLoop *loop, 
                  int sockfd, 
                  InetAddr localAddr,
                  InetAddr peerAddr);
    ~TcpConnection();
    enum State {
        CONNECTING,
        CONNECTED,
        CLOSING,
        CLOSED,
    };
    size_t id() const { return _id; }
    void send(const std::string& s);
    EventLoop *getLoop() { return _loop; }
    ChannelPtr& getChannel() { return _channel; }
    InetAddr& localAddr() { return _localAddr; }
    InetAddr& peerAddr() { return _peerAddr; }
    void setState(char state) { _state = state; }
    bool isConnected() { return _state == CONNECTED; }
    boost::any& context() { return _context; }
    void setContext(boost::any& context) { _context = context; }
    void close() { handleClose(); }
    void setConnectionCb(const ConnectionCallback _cb)
    { _connectionCb = _cb; }
    void setMessageCb(const MessageCallback _cb)
    { _messageCb = _cb; }
    void setWriteCompleteCb(const WriteCompleteCallback _cb)
    { _writeCompleteCb = _cb; }
    void setCloseCb(const CloseCallback _cb)
    { _closeCb = _cb; }
private:
    void handleRead();
    void handleWrite();
    void handleClose();
    void handleError();
    
    void sendInLoop(const std::string& s);

    size_t _id;
    EventLoop *_loop;
    std::shared_ptr<Channel> _channel;
    std::unique_ptr<Socket> _socket;
    Buffer _input;
    Buffer _output;
    InetAddr _localAddr;
    InetAddr _peerAddr;
    boost::any _context;
    std::atomic_char _state;
    ConnectionCallback _connectionCb;
    MessageCallback _messageCb;
    WriteCompleteCallback _writeCompleteCb;
    CloseCallback _closeCb;
    ErrorCallback _errorCb;
};
}

#endif // _ANGEL_TCPCONNECTION_H
