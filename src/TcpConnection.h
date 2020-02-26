#ifndef _ANGEL_TCPCONNECTION_H
#define _ANGEL_TCPCONNECTION_H

#include <functional>
#include <memory>
#include <atomic>
#include <any>

#include "Channel.h"
#include "Buffer.h"
#include "InetAddr.h"
#include "Socket.h"
#include "noncopyable.h"
#include "decls.h"

namespace Angel {

class EventLoop;

// TcpConnection抽象一个Tcp连接，它是不可再生的，连接到来时被构造，
// 连接断开时被析构。
//
// 下面是一个TcpConnection对象的生命期：
// 通过Acceptor::handleAccept()接收到一个新连接，得到connfd
//           |
//           v
// 通过TcpServer::newConnection()，构造一个TcpConnection对象，连接进入CONNECTING状态
//           |
//           v
// 通过TcpConnection::connectEstablish()，将连接加入到事件循环中，连接进入CONNECTED状态
//           |
//           v
// 此时连接已建立完成，可进行正常通信
//           |
//           v
// 关闭连接有两种情况：
// 1）服务端主动关闭，连接进入CLOSING状态，在将Buffer中剩余的数据发送完毕后，将连接关闭
// 2）对端断开连接或连接出错，连接进入CLOSED状态，立刻关闭连接
//           |
//           v
// 最后通过调用TcpServer::removeConnection()来移除一个连接
//
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
        CONNECTING, // 连接正在建立
        CONNECTED,  // 连接建立完成
        CLOSING,    // 连接将要关闭
        CLOSED,     // 连接立刻就要关闭
    };
    size_t id() const { return _id; }
    void send(const char *s);
    void send(const std::string& s);
    void send(const char *s, size_t len);
    void send(const void *v, size_t len);
    void formatSend(const char *fmt, ...);
    EventLoop *getLoop() { return _loop; }
    ChannelPtr& getChannel() { return _channel; }
    InetAddr& localAddr() { return _localAddr; }
    InetAddr& peerAddr() { return _peerAddr; }
    void setState(char state) { _state = state; }
    bool isConnected() { return _state == CONNECTED; }
    std::any& getContext() { return _context; }
    void setContext(std::any context) { _context = std::move(context); }
    void setTimeoutTimerId(size_t id) { _timeoutTimerId = id; }
    void setConnTimeout(int64_t timeout) { _connTimeout = timeout; }
    void connectEstablish();
    void close();
    void setConnectionCb(const ConnectionCallback _cb)
    { _connectionCb = std::move(_cb); }
    void setMessageCb(const MessageCallback _cb)
    { _messageCb = std::move(_cb); }
    void setWriteCompleteCb(const WriteCompleteCallback _cb)
    { _writeCompleteCb = std::move(_cb); }
    void setCloseCb(const CloseCallback _cb)
    { _closeCb = std::move(_cb); }
private:
    void handleRead();
    void handleWrite();
    void handleClose();
    void handleError();
    void sendInLoop(const char *data, size_t len);
    void updateTimeoutTimer();
    const char *getStateString();

    size_t _id;
    EventLoop *_loop;
    std::shared_ptr<Channel> _channel;
    std::unique_ptr<Socket> _socket;
    Buffer _input;
    Buffer _output;
    InetAddr _localAddr;
    InetAddr _peerAddr;
    // 保存连接所需的上下文
    // context不应包含一个TcpConnectionPtr，否则将会造成循环引用
    std::any _context;
    std::atomic_int _state; // 标识一个连接的状态
    int64_t _connTimeout; // 连接的最大空闲时间
    size_t _timeoutTimerId; // 连接的超时定时器的id
    ConnectionCallback _connectionCb;
    MessageCallback _messageCb;
    WriteCompleteCallback _writeCompleteCb;
    CloseCallback _closeCb;
    ErrorCallback _errorCb;
};

}

#endif // _ANGEL_TCPCONNECTION_H
