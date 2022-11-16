#include <angel/httplib.h>

using namespace angel::httplib;

int main(int argc, char *argv[])
{
    const char *base_dir = ".";
    if (argc > 1 && strncmp(argv[1], "--base-dir=", 11) == 0) {
        base_dir = argv[1] + 11;
    }

    angel::evloop loop;

    http_server server(&loop, angel::inet_addr(8000));
    server.set_base_dir(base_dir);

    server.File("/", [](request& req, Headers& headers){
            headers.emplace("Cache-Control", "max-age=30");
            });
    server.Get("/hello", [](request& req, response& res){
            res.set_status_code(Ok);
            res.set_content("Hello~~");
            });
    server.Post("/login", [](request& req, response& res){
            res.set_status_code(Ok);
            res.set_content("login successfully");
            });
    server.start();
    loop.run();
}
