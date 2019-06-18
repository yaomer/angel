#include <signal.h>
#include <unistd.h>
#include <functional>
#include <mutex>
#include <memory>
#include "EventLoop.h"
#include "Channel.h"
#include "Signaler.h"
#include "LogStream.h"
#include "Socket.h"

using namespace Angel;
using namespace std::placeholders;

namespace Angel{
    // 保护__signalerPtr，使之正确初始化
    std::mutex _SYNC_INIT_LOCK;
    Angel::Signaler *__signalerPtr = nullptr;

    static int _signalFd = -1;
}

Signaler::Signaler(EventLoop *loop)
    : _loop(loop),
    _sigChannel(new Channel(loop))
{
    if (_signalFd != -1)
        LOG_FATAL << "Only have one Signaler in one process";

    Socket::socketpair(_pairFd);
    _sigChannel->setFd(_pairFd[0]);
    _signalFd = _pairFd[1];
    _sigChannel->setReadCb(
            [this]{ this->_sigChannel->handleRead(); });
    _sigChannel->setMessageCb(
            std::bind(&Signaler::sigCatch, this, _1, _2));
}

Signaler::~Signaler()
{

}

void Signaler::start()
{
    _loop->addChannel(_sigChannel);
}

void Signaler::sigHandler(int signo)
{
    ssize_t n = write(_signalFd,
                      reinterpret_cast<void *>(&signo), 1);
    LOG_INFO << "Sig" << sys_signame[signo] << " is triggered";
    if (n != 1)
        LOG_ERROR << "write " << n << " bytes instead of 1";
}

// if _cb == nullptr, ignore signo
void Signaler::add(int signo, const SignalerCallback _cb)
{
    struct sigaction sa;
    bzero(&sa, sizeof(sa));
    if (_cb) {
        sa.sa_handler = &sigHandler;
        auto it = std::pair<int, SignalerCallback>(signo, _cb);
        _sigCallbackMaps.insert(it);
    } else {
        sa.sa_handler = SIG_IGN;
    }
    // 重启被信号中断的Syscall
    sa.sa_flags |= SA_RESTART;
    // 建立SigHandler之前屏蔽所有信号
    sigfillset(&sa.sa_mask);
    if (sigaction(signo, &sa, nullptr) < 0)
        LOG_ERROR << "sigaction: " << strerrno();
}

// 恢复信号signo的默认语义
void Signaler::cancel(int signo)
{
    struct sigaction sa;
    bzero(&sa, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    _sigCallbackMaps.erase(signo);
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    if (sigaction(signo, &sa, nullptr) < 0)
        LOG_ERROR << "sigaction: " << strerrno();
}

void Signaler::sigCatch(std::shared_ptr<Channel> chl, Buffer& buf)
{
    const char *sig = buf.c_str();
    for (int i = 0; i < buf.readable(); i++) {
        auto it = _sigCallbackMaps.find(sig[i]);
        if (it != _sigCallbackMaps.end())
            it->second();
    }
    buf.retrieveAll();
}

void Angel::addSignal(int signo, const Signaler::SignalerCallback _cb)
{
    if (__signalerPtr)
        __signalerPtr->add(signo, _cb);
}

void Angel::cancelSignal(int signo)
{
    if (__signalerPtr)
        __signalerPtr->cancel(signo);
}
