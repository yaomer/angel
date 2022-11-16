#include <angel/httplib.h>

#include <iostream>

using namespace angel::httplib;

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "./http-client [URL]\n");
        return 1;
    }

    http_client client;
    http_request request;

    request.set_url(argv[1]).Get();

    auto res = client.send(request).get();

    if (res.err_code != ErrorCode::None) {
        std::cout << res.err_str() << "\n";
        return 1;
    }

    std::cout << res.status_code << " " << res.status_message << "\n";
    for (auto& [k, v] : res.headers()) {
        std::cout << k.key << ": " << v << "\n";
    }
    std::cout << "\n";
    std::cout << res.body() << "\n";
}
