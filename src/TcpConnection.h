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

// TcpConnection抽象一个Tcp连接，它是不可再生的，连接到来时
// 被构造，连接断开时被析构
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
    void send(const char *s);
    void send(const std::string& s);
    void send(const char *s, size_t len);
    void send(const void *v, size_t len);
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
    void sendInNotIoThread(const std::string& data);
    void updateTimeoutTimer();

    size_t _id;
    EventLoop *_loop;
    std::shared_ptr<Channel> _channel;
    std::unique_ptr<Socket> _socket;
    Buffer _input;
    Buffer _output;
    InetAddr _localAddr;
    InetAddr _peerAddr;
    // 保存连接所需的上下文
    std::any _context;
    // 标识一个连接所处的状态
    std::atomic_int _state;
    // 连接的超时定时器的id
    size_t _timeoutTimerId;
    // 连接的超时值
    int64_t _connTimeout;
    // 接受一个新连接后调用
    ConnectionCallback _connectionCb;
    // 正常的消息通讯使用
    MessageCallback _messageCb;
    // 数据发送完成后调用
    WriteCompleteCallback _writeCompleteCb;
    // 关闭连接时调用
    CloseCallback _closeCb;
    // 连接出错时调用
    ErrorCallback _errorCb;
};

}

#endif // _ANGEL_TCPCONNECTION_H
