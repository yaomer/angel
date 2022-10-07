#include <angel/httplib.h>

using namespace angel::httplib;

int main()
{
    angel::evloop loop;
    HttpServer server(&loop, angel::inet_addr(8000));
    server.Get("/hello", [](request& req, response& res){
            res.set_status_code(Ok);
            res.set_status_message("Ok");
            res.add_header("Content-Type", "text/plain");
            res.set_content("Hello~~");
            });
    server.start();
    loop.run();
}

