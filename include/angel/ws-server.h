#ifndef __ANGEL_WS_SERVER_H
#define __ANGEL_WS_SERVER_H

#include <angel/server.h>

namespace angel {

class WebSocketContext;

class WebSocketServer {
public:
    typedef std::function<void(WebSocketContext&)> WebSocketHandler;

    WebSocketServer(const WebSocketServer&) = delete;
    WebSocketServer& operator=(const WebSocketServer&) = delete;

    WebSocketServer(evloop *, inet_addr);
    void for_each(const WebSocketHandler handler);
    void start() { server.start(); }

    WebSocketHandler onopen;
    WebSocketHandler onmessage;
    WebSocketHandler onclose;
    WebSocketHandler onerror;
private:
    server server;
    friend class WebSocketContext;
};

class WebSocketContext {
public:
    WebSocketContext(WebSocketServer *, connection *);
    void send(const std::string& data);
    std::string decoded_buffer; // 解码好的数据
    bool is_binary_type;
    std::string origin;
    std::string host;
private:
    enum { Handshake, HandshakeOK, HandshakeError, Establish };
    enum { Ok, Error, NotEnough, Ping, Pong, Close };
    static void message_handler(const connection_ptr& conn, buffer& buf);
    int handshake(const connection_ptr& conn, buffer& buf);
    int handshake(const char *line, const char *end);
    int decode(buffer& raw_buf);
    void encode(const std::string& raw_buf);
    void sec_websocket_accept(std::string key);
    void handshake_ok(const connection_ptr& conn);
    void handshake_error(const connection_ptr& conn);
    // void set_status_code(int code) { status_code = code; }
    // void set_status_message(const std::string& message) { status_message = message; }

    int state;
    WebSocketServer *ws;
    connection *conn;
    std::string SecWebSocketAccept;
    std::string encoded_buffer;
    // int status_code;
    // std::string status_message;
    bool read_request_line;
    int required_request_headers;
    bool fragment;
    friend class WebSocketServer;
};

}

#endif // __ANGEL_WS_SERVER_H
