#ifndef _ANGEL_DECLS_H
#define _ANGEL_DECLS_H

#include <functional>
#include <memory>

namespace Angel {

class Channel;
class TcpConnection;
class Buffer;

// for TcpConnection
typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;
typedef std::function<void(const TcpConnectionPtr&)> ConnectionCallback;
typedef std::function<void(const TcpConnectionPtr&, Buffer&)> MessageCallback;
typedef std::function<void(const TcpConnectionPtr&)> CloseCallback;
typedef std::function<void()> WriteCompleteCallback;
typedef std::function<void()> ErrorCallback;
// for Acceptor and Connector
typedef std::function<void(int)> NewConnectionCallback;
// for Channel
typedef std::shared_ptr<Channel> ChannelPtr;
typedef std::function<void()> EventReadCallback;
typedef std::function<void()> EventWriteCallback;
typedef std::function<void()> EventCloseCallback;
typedef std::function<void()> EventErrorCallback;
// for Signaler
typedef std::function<void()> SignalerCallback;
// for TimerTask
typedef std::function<void()> TimerCallback;
// for ThreadPool
typedef std::function<void()> TaskCallback;
// for EventLoop
typedef std::function<void()> Functor;

}

#endif // _ANGEL_DECLS_H
