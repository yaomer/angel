#ifndef _ANGEL_CHATSERVER_H
#define _ANGEL_CHATSERVER_H

#include <Angel/EventLoop.h>
#include <Angel/TcpServer.h>

using std::placeholders::_1;
using std::placeholders::_2;

// 一个简单的聊天服务
class ChatServer {
public:
    ChatServer(Angel::EventLoop *loop, Angel::InetAddr& inetAddr)
        : _loop(loop),
        _server(loop, inetAddr)
    {
        _server.setConnectionCb(
                std::bind(&ChatServer::onConnection, this, _1));
        _server.setMessageCb(
                std::bind(&ChatServer::onMessage, this, _1, _2));
    }
    void onConnection(const Angel::TcpConnectionPtr& conn)
    {
        char buf[256];
        size_t clients = _server.clients();
        snprintf(buf, sizeof(buf), 
                "Welcome to chat-room, your chat-id is %zu, "
                "there %s %zu people is online\n", 
                conn->id(), 
                clients > 1 ? "are" : "is",
                clients);
        conn->send(buf);
        conn->send("Online Ids: ");
        for (auto& it : _server.connectionMaps()) {
            snprintf(buf, sizeof(buf), "%zu ", it.second->id());
            conn->send(buf);
        }
        conn->send("\n");
    }
    void onMessage(const Angel::TcpConnectionPtr& conn, Angel::Buffer& buf)
    {
        char sbuf[256];
        snprintf(sbuf, sizeof(sbuf), "[id %zu]: ", conn->id());
        // buf.readable() >= strlen("\n")
        while (buf.readable() >= 1) {
            int lf = buf.findLf();
            if (lf >= 0) {
                if (strncmp(buf.c_str(), "help", 4) == 0) {
                    conn->send(_help);
                    buf.retrieveAll();
                } else if (strncmp(buf.c_str(), "grp ", 4) == 0) {
                    // broadcast msg to all clients
                    buf.retrieve(4);
                    for (auto& it : _server.connectionMaps()) {
                        it.second->send(sbuf);
                        it.second->send(buf.c_str(), lf + 1 - 4);
                    }
                    buf.retrieve(lf + 1 - 4);
                } else if (strncmp(buf.c_str(), "id ", 3) == 0) {
                    // send msg to id
                    buf.retrieve(3);
                    int i = 0;
                    while (isalnum(buf[i]))
                        i++;
                    buf[i] = '\0';
                    size_t id = atol(buf.c_str());
                    conn->setContext(id);
                    buf.retrieve(i);
                    auto it = _server.getConnection(id);
                    it->send(sbuf);
                    it->send(buf.c_str(), lf + 1 - (i + 3));
                    buf.retrieve(lf + 1 - (i + 3));
                } else {
                    if (!conn->getContext().has_value()) {
                        // echo msg
                        conn->send(sbuf);
                        conn->send(buf.c_str(), lf + 1);
                        buf.retrieve(lf + 1);
                        continue;
                    }
                    // continue chat with id
                    size_t id = std::any_cast<size_t>(conn->getContext());
                    auto it = _server.getConnection(id);
                    it->send(sbuf);
                    it->send(buf.c_str(), lf + 1);
                    buf.retrieve(lf + 1);
                }
            } else
                break;
        }
    }
    void start()
    {
        _server.setIoThreadNums(4);
        _server.start();
    }
private:
    static const char *_help;
    Angel::EventLoop *_loop;
    Angel::TcpServer _server;
};

#endif // _ANGEL_CHATSERVER_H
