#ifndef _ANGEL_CHATSERVER_H
#define _ANGEL_CHATSERVER_H

#include "../Angel.h"
#include <map>
#include <memory>

using namespace Angel;

class User {
public:
    User(size_t userId, size_t connId) 
        : _userId(userId), _connId(connId), _chatId(0) {  }
    ~User() {  }
    size_t getUserId() const { return _userId; }
    size_t getConnId() const { return _connId; }
    size_t getChatId() const { return _chatId; }
    void setChatId(size_t chatId) { _chatId = chatId; }
private:
    size_t _userId;
    size_t _connId;
    size_t _chatId;
};

// 一个简单的聊天服务
class ChatServer {
public:
    typedef std::shared_ptr<User> UserPtr;

    ChatServer(EventLoop *loop, InetAddr& inetAddr)
        : _loop(loop),
        _server(loop, inetAddr),
        _userId(1)
    {
        _server.setConnectionCb(
                std::bind(&ChatServer::onConnection, this, _1));
        _server.setMessageCb(
                std::bind(&ChatServer::onMessage, this, _1, _2));
        _server.setCloseCb(
                std::bind(&ChatServer::onClose, this, _1));
    }
    void onConnection(const TcpConnectionPtr& conn)
    {
        char buf[256];
        size_t clients = _server.clients();
        snprintf(buf, sizeof(buf), 
                "Welcome to chat-room, your chat-id is %zu, "
                "there %s %zu people is online\n", 
                _userId, 
                clients > 1 ? "are" : "is",
                clients);
        conn->send(buf);
        conn->send("Online Ids: ");
        for (auto it : _userMaps) {
            snprintf(buf, sizeof(buf), "%lu ", it.first);
            conn->send(buf);
        }
        conn->send("\n");
        auto usr = new User(_userId, conn->id());
        conn->setContext(UserPtr(usr));
        _userMaps.insert(std::make_pair(_userId, usr));
        _userId++;
    }
    void onMessage(const TcpConnectionPtr& conn, Buffer& buf)
    {
        char sbuf[256];
        auto curUser = boost::any_cast<UserPtr>(conn->getContext());
        snprintf(sbuf, sizeof(sbuf), "[id %zu]: ", curUser->getUserId());
        buf.skipSpaceOfStart();
        // buf.readable() >= strlen("\n")
        while (buf.readable() >= 1) {
            int lf = buf.findLf();
            if (lf >= 0) {
                if (strncmp(buf.c_str(), "help\n", 5) == 0) {
                    conn->send(_help);
                } else if (strncmp(buf.c_str(), "grp ", 4) == 0) {
                    // broadcast msg to all clients
                    for (auto& it : _server.connectionMaps()) {
                        it.second->send(sbuf);
                        it.second->send(&buf[4], lf + 1 - 4);
                    }
                } else if (strncmp(buf.c_str(), "id ", 3) == 0) {
                    // send msg to id
                    int i = 3;
                    while (isalnum(buf[i]))
                        i++;
                    buf[i++] = '\0';
                    size_t id = atol(&buf[3]);
                    curUser->setChatId(id);
                    auto usr = getUser(id);
                    if (usr) {
                        auto it = _server.getConnection(usr->getConnId());
                        it->send(sbuf);
                        it->send(&buf[i], lf + 1 - i);
                    } else
                        conn->send("The user is gone\n");
                } else {
                    if (curUser->getChatId() == 0) {
                        // echo msg
                        conn->send(buf.c_str(), lf + 1);
                        buf.retrieve(lf + 1);
                        continue;
                    }
                    // continue chat with id
                    auto c = getUser(curUser->getChatId());
                    if (c) {
                        auto it = _server.getConnection(c->getConnId());
                        it->send(sbuf);
                        it->send(buf.c_str(), lf + 1);
                    } else
                        conn->send("The user is gone\n");
                }
            } else
                break;
            buf.retrieve(lf + 1);
        }
    }
    void onClose(const TcpConnectionPtr& conn)
    {
        auto it = boost::any_cast<UserPtr>(conn->getContext());
        _userMaps.erase(it->getUserId());
    }
    User *getUser(size_t id)
    {
        auto it = _userMaps.find(id);
        if (it != _userMaps.cend())
            return it->second;
        else
            return nullptr;
    }
    void start()
    {
        // 开启4个IO线程
        _server.setIoThreadNums(4);
        _server.start();
    }
    void quit() { _loop->quit(); }
private:
    static const char *_help;
    EventLoop *_loop;
    TcpServer _server;
    size_t _userId;
    // 直接使用conn->id()可能会导致串话
    std::map<size_t, User*> _userMaps;
};

#endif // _ANGEL_CHATSERVER_H
