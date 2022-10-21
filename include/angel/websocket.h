#ifndef __ANGEL_WEBSOCKET_H
#define __ANGEL_WEBSOCKET_H

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
    // Send message directly without buffering.
    void send(std::string_view message);
    // Send a fragment, must end with send_fragment(,true).
    void send_fragment(std::string_view fragment, bool final_fragment = false);
    // Async send a file, return false if can't open path.
    bool send_file(const std::string& path);
    // 1) Indicates the received message type.
    // 2) It is set by the user to indicate the type of sent message.
    bool is_binary_type; // binary or text
    std::string decoded_buffer;
    std::string origin;
    std::string host;
private:
    enum { Handshake, HandshakeOK, HandshakeError, Establish };
    enum { Ok, Error, NotEnough, Ping, Pong, Close };
    static void message_handler(const connection_ptr& conn, buffer& buf);
    int handshake(const connection_ptr& conn, buffer& buf);
    int handshake(buffer& buf, size_t crlf);
    int decode(buffer& raw_buf);
    void encode(uint64_t raw_size, uint8_t first_byte);
    void sec_websocket_accept(std::string_view key);
    void handshake_ok(const connection_ptr& conn);
    void handshake_error(const connection_ptr& conn);

    int state;
    WebSocketServer *ws;
    connection *conn;
    std::string SecWebSocketAccept;
    std::string encoded_buffer;
    bool read_request_line;
    int required_request_headers;
    bool rcvfragment;
    enum { FirstFragment, MiddleFragment, FinalFragment };
    int sndfragment;
    friend class WebSocketServer;
};

}

#endif // __ANGEL_WEBSOCKET_H
