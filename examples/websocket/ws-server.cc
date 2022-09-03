#include <iostream>
#include <numeric>

#include "ws-server.h"

WebSocketServer::WebSocketServer(angel::evloop *loop, angel::inet_addr listen_addr)
    : server(loop, listen_addr)
{
    server.set_connection_handler([this](const angel::connection_ptr& conn){
            conn->set_context(WebSocketContext(this, conn.get()));
            });
    server.set_message_handler(WebSocketContext::message_handler);
}

WebSocketContext::WebSocketContext(WebSocketServer *ws, angel::connection *conn)
    : state(Handshake), ws(ws), conn(conn), read_request_line(true),
    required_request_headers(6)
{
}

// 主要程序逻辑
void WebSocketContext::message_handler(const angel::connection_ptr& conn, angel::buffer& buf)
{
    unsigned char frame;
    WebSocketContext& context = std::any_cast<WebSocketContext&>(conn->get_context());
    WebSocketServer *ws = context.ws;
    while (buf.readable() > 0) {
        switch (context.state) {
        case WebSocketContext::Handshake:
            switch (context.handshake(conn, buf)) {
            case WebSocketContext::HandshakeOK:
                if (ws->on_connection) ws->on_connection(context);
                break;
            case WebSocketContext::HandshakeError:
                return;
            }
            break;
        case WebSocketContext::Establish:
            switch (context.decode(buf)) {
            case WebSocketContext::Ok:
                if (ws->on_message) ws->on_message(context);
                break;
            case WebSocketContext::Ping:
                frame = 0x8A;
                conn->send(&frame, 1);
                break;
            case WebSocketContext::Close:
                if (ws->on_close) ws->on_close(context);
                frame = 0x88;
                conn->send(&frame, 1);
                conn->close();
                return;
            case WebSocketContext::Error:
                conn->close();
                return;
            case WebSocketContext::NotEnough:
                return;
            }
            break;
        }
    }
}

// Client:
// GET / HTTP/1.1
// Host: www.example.com
// Upgrade: websocket
// Connection: Upgrade
// Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
// Origin: http://example.com
// Sec-WebSocket-Protocol: chat # Optional
// Sec-WebSocket-Version: 13
// ...
int WebSocketContext::handshake(const char *line, const char *end)
{
    if (read_request_line) {
        read_request_line = false;
        if (strncmp(line, "GET ", 4)) return HandshakeError;
        std::string uri;
        const char *p = std::find_if(line + 4, end, isspace);
        uri.assign(line + 4, p);
        if (strncmp(p + 1, "HTTP/1.1", 8))
            return HandshakeError;
    }
    if (strncmp(line, "\r\n", 2) == 0) {
        return (required_request_headers == 0) ? HandshakeOK : HandshakeError;
    }
    if (strncasecmp(line, "Host: ", 6) == 0) {
        required_request_headers--;
        host.assign(line + 6, end);
    } else if (strncasecmp(line, "Connection: ", 12) == 0) {
        required_request_headers--;
        if (strncasecmp(line + 12, "Upgrade", 7))
            return HandshakeError;
    } else if (strncasecmp(line, "Upgrade: ", 9) == 0) {
        required_request_headers--;
        if (strncasecmp(line + 9, "websocket", 9))
            return HandshakeError;
    } else if (strncasecmp(line, "Sec-WebSocket-Key: ", 19) == 0) {
        required_request_headers--;
        sec_websocket_accept(std::string(line + 19, std::find_if(line + 19, end, isspace)));
    } else if (strncasecmp(line, "Sec-WebSocket-Version: ", 23) == 0) {
        required_request_headers--;
        if (atoi(line + 23) != 13)
            return HandshakeError;
    } else if (strncasecmp(line, "Origin: ", 8) == 0) {
        required_request_headers--;
        origin.assign(line + 8, end);
    } else {
        // Other headers
    }

    return Handshake;
}

void WebSocketContext::sec_websocket_accept(std::string key)
{
    static const char *guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    key = sha1(key + guid, true);
    SecWebSocketAccept = base64_encode(key.data(), key.size());
}

// Server:
// HTTP/1.1 101 Switching Protocols
// Connection: Upgrade
// Upgrade: websocket
// Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
void WebSocketContext::handshake_ok(const angel::connection_ptr& conn)
{
    std::string buf;
    buf += "HTTP/1.1 101 Switching Protocols\r\n";
    buf += "Connection: Upgrade\r\n";
    buf += "Upgrade: websocket\r\n";
    buf += "Sec-WebSocket-Accept: " + SecWebSocketAccept + "\r\n\r\n";
    conn->send(buf);
}

void WebSocketContext::handshake_error(const angel::connection_ptr& conn)
{
    conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
}

int WebSocketContext::handshake(const angel::connection_ptr& conn, angel::buffer& buf)
{
    auto& context = std::any_cast<WebSocketContext&>(conn->get_context());
    while (buf.readable() >= 2) {
        int crlf = buf.find_crlf();
        if (crlf < 0) break;
        context.state = context.handshake(buf.peek(), buf.peek() + crlf);
        switch (context.state) {
        case WebSocketContext::HandshakeOK:
            buf.retrieve(2);
            context.state = WebSocketContext::Establish;
            context.handshake_ok(conn);
            return HandshakeOK;
        case WebSocketContext::HandshakeError:
            context.handshake_error(conn);
            conn->close();
            return HandshakeError;
        }
        buf.retrieve(crlf + 2);
    }
    return Handshake;
}

//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-------+-+-------------+-------------------------------+
// |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
// |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
// |N|V|V|V|       |S|             |   (if payload len==126/127)   |
// | |1|2|3|       |K|             |                               |
// +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
// |     Extended payload length continued, if payload len == 127  |
// + - - - - - - - - - - - - - - - +-------------------------------+
// |                               |Masking-key, if MASK set to 1  |
// +-------------------------------+-------------------------------+
// | Masking-key (continued)       |          Payload Data         |
// +-------------------------------- - - - - - - - - - - - - - - - +
// :                     Payload Data continued ...                :
// + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
// |                     Payload Data continued ...                |
// +---------------------------------------------------------------+
//
// opcode:
// 0x0(连续帧), 0x1(文本帧), 0x2(二进制帧), 0x3-0x7(保留帧)
// 0x8(连接关闭), 0x9(ping), 0xA(pong), 0xB-0xF(保留的控制帧)
//
int WebSocketContext::decode(angel::buffer& raw_buf)
{
    bool fin; // last frame ?
    bool rsv1, rsv2, rsv3;
    uint8_t opcode;
    bool mask;
    uint64_t payload_len;
    const char *masking_key;

    const char *b = raw_buf.peek();
    uint64_t readable = raw_buf.readable();
    const char *start = b;

    fin = b[0] >> 7;
    rsv1 = (b[0] >> 6) & 0x01;
    rsv2 = (b[0] >> 5) & 0x01;
    rsv3 = (b[0] >> 4) & 0x01;
    opcode = b[0] & 0x0f;
    switch (opcode) {
    case 0x0: break;
    case 0x1: break;
    case 0x2: break;
    case 0x3: case 0x4: case 0x5: case 0x6: case 0x7: break;
    case 0x8: return Close;
    case 0x9: return Ping;
    case 0xA: return Error;
    case 0xB: case 0xC: case 0xD: case 0xE: case 0xF: break;
    }

    uint64_t i, raw_len;
    if (readable < 2) return NotEnough;
    mask = b[1] >> 7;
    payload_len = b[1] & 0x7f;
    switch (payload_len) {
    case 127:
        i = 10;
        if (readable < i) return NotEnough;
        raw_len = *reinterpret_cast<const uint64_t*>(&b[2]);
        payload_len = angel::sockops::ntoh64(raw_len);
        break;
    case 126:
        i = 4;
        if (readable < i) return NotEnough;
        raw_len = *reinterpret_cast<const uint16_t*>(&b[2]);
        payload_len = ntohs(raw_len);
        break;
    default:
        i = 2;
        break;
    }

    if (mask) {
        if (readable < i + 4 + payload_len) return NotEnough;
        masking_key = &b[i];
        b = &b[i + 4];
    } else {
        if (readable < i + payload_len) return NotEnough;
        b = &b[i];
    }

    decoded_buffer.clear();
    decoded_buffer.reserve(payload_len);
    if (mask) {
        for (size_t i = 0; i < payload_len; i++) {
            decoded_buffer.push_back(b[i] ^ masking_key[i % 4]);
        }
    } else {
        decoded_buffer.append(b, payload_len);
    }

    raw_buf.retrieve(b - start + payload_len);
    return Ok;
}

void WebSocketContext::encode(const std::string& raw_buf)
{
    encoded_buffer.clear();

    // 1 0 0 0 0 0 0 1
    encoded_buffer.push_back(0x81);

    uint64_t raw_size = raw_buf.size();
    if (raw_size <= 125) { // 0 x x x x x x x
        encoded_buffer.push_back((unsigned char)raw_size);
    } else if (raw_size <= std::numeric_limits<uint16_t>().max()) {
        encoded_buffer.push_back(126);
        uint16_t encoded_size = htons(raw_size);
        encoded_buffer.append(reinterpret_cast<const char*>(&encoded_size), 2);
    } else {
        encoded_buffer.push_back(127);
        uint64_t encoded_size = angel::sockops::hton64(raw_size);
        encoded_buffer.append(reinterpret_cast<const char*>(&encoded_size), 8);
    }
    encoded_buffer.append(raw_buf);
}

void WebSocketContext::send(const std::string& data)
{
    encode(data);
    conn->send(encoded_buffer);
}

int main()
{
    angel::evloop loop;
    WebSocketServer ws(&loop, angel::inet_addr(8000));
    ws.set_connection_handler([](WebSocketContext& c){
            printf("new client (%s) to host (%s)\n", c.origin.c_str(), c.host.c_str());
            });
    ws.set_message_handler([](WebSocketContext& c){
            c.send(c.decoded_buffer);
            });
    ws.set_close_handler([](WebSocketContext& c){
            printf("disconnect with client (%s)\n", c.origin.c_str());
            });
    ws.start();
    loop.run();
}
