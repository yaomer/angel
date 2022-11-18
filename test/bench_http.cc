#include <angel/client.h>
#include <angel/util.h>
#include <angel/resolver.h>
#include <angel/config.h>

#if defined (ANGEL_USE_OPENSSL)
#include <angel/ssl_client.h>
#endif

static int num_requests = 10000;
static int concurrency  = 1;
static int max_timeout  = 30 * 1000;

static int send_requests = 0, complete_requests = 0;
static size_t total_bytes = 0;
static long long total_latency = 0;

static bool successful = true;

struct request_info {
    std::unique_ptr<angel::client> cli;
    long long start = angel::util::get_cur_time_us(); // request start time
    size_t len = 0; // response content length
    int idx = 0;
};

struct bench_http {
    angel::evloop *loop;
    std::unordered_map<int, std::unique_ptr<request_info>> requests;
    std::string scheme, host, ip, path;
    int port;

    bool parse_url(std::string_view url);
    bool resolve();
    long bench();
private:
    void launch_request();
    void close_handler(request_info *ri);
    void check_timeout();
};

void bench_http::launch_request()
{
    auto ri = std::make_unique<request_info>();
    ri->idx = send_requests++;
#if defined (ANGEL_USE_OPENSSL)
    if (scheme == "https") {
        ri->cli.reset(new angel::ssl_client(loop, angel::inet_addr(ip, port)));
    } else {
        ri->cli.reset(new angel::client(loop, angel::inet_addr(ip, port)));
    }
#else
    ri->cli.reset(new angel::client(loop, angel::inet_addr(ip, port)));
#endif
    ri->cli->set_connection_handler([this](const angel::connection_ptr& conn){
            conn->format_send("GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", path.c_str(), host.c_str());
            });
    ri->cli->set_message_handler([ri = ri.get()](const angel::connection_ptr& conn, angel::buffer& buf){
            // We don't parse http response.
            ri->len += buf.readable();
            buf.retrieve_all();
            });
    ri->cli->set_close_handler([this, ri = ri.get()](const angel::connection_ptr& conn){
            this->close_handler(ri);
            });
    ri->cli->start();
    requests.emplace(ri->idx, std::move(ri));
}

void bench_http::close_handler(request_info *ri)
{
    total_bytes += ri->len;
    total_latency += angel::util::get_cur_time_us() - ri->start;
    complete_requests++;
    if ((num_requests / 10) && complete_requests % (num_requests / 10) == 0)
        printf("Completed %d requests\n", complete_requests);
    loop->queue_in_loop([this, idx = ri->idx]{ this->requests.erase(idx); });
    if (complete_requests == num_requests)
        loop->quit();
    if (send_requests < num_requests)
        launch_request();
}

void bench_http::check_timeout()
{
    auto now = angel::util::get_cur_time_us();
    for (auto& [idx, request] : requests) {
        if (now - request->start >= max_timeout * 1000) {
            fprintf(stderr, "Benchmarking timed out.\n");
            successful = false;
            loop->quit();
            break;
        }
    }
}

bool bench_http::parse_url(std::string_view url)
{
    const char *p = url.data();
    const char *end = url.data() + url.size();

    auto sep = angel::util::search(url, "://");
    if (!sep) return false;
    scheme.assign(p, sep);
    p = sep + 3;

    if (scheme == "http") port = 80;
    else if (scheme == "https") port = 443;
    else return false;

    sep = std::find(p, end, ':');
    if (sep != end) { // http://host:port/
        host.assign(p, sep);
        p = sep + 1;
        sep = std::find(p, end, '/');
        port = angel::util::svtoi({p, (size_t)(sep - p)}).value_or(-1);
        if (port <= 0) return false;
    } else { // http://host/
        sep = std::find(p, end, '/');
        host.assign(p, sep);
    }
    // http://host => http://host/
    if (sep == end) {
        path = "/";
    } else {
        path.assign(sep, end);
    }

    return true;
}

bool bench_http::resolve()
{
    auto *resolver = angel::dns::resolver::get_resolver();
    auto res = resolver->get_addr_list(host, 5000);
    if (res.empty()) return false;
    ip = res.front();
    return true;
}

long bench_http::bench()
{
    loop->run_every(300, [this]{ this->check_timeout(); });

    auto t1 = angel::util::get_cur_time_ms();

    concurrency = std::min(concurrency, num_requests);
    for (int i = 0; i < concurrency; i++) {
        launch_request();
    }
    loop->run();

    auto t2 = angel::util::get_cur_time_ms();
    return t2 - t1;
}

static void usage()
{
    fprintf(stderr,
            "Usage: ./bench_http [options] http[s]://hostname[:port]/path\n"
            "    -c <concurrency> Number of multiple requests to perform at a time.\n"
            "    -n <requests>    Number of requests to perform for the benchmarking session.\n"
            "    -t <timeout>     Wait for each response for <secs>. Default is 30 seconds.\n"
           );
    exit(1);
}

int main(int argc, char *argv[])
{
    int c;
    while ((c = getopt(argc, argv, "c:kn:t:")) != -1) {
        switch (c) {
        case 'c':
            concurrency = atoi(optarg);
            break;
        case 'n':
            num_requests = atoi(optarg);
            break;
        case 't':
            max_timeout = atoi(optarg) * 1000;
            break;
        default:
            fprintf(stderr, "Illegal argument \"%c\"\n", c);
            exit(1);
        }
    }
    if (optind != argc - 1) {
        usage();
    }

    angel::evloop loop;

    bench_http bench;
    bench.loop = &loop;

    if (!bench.parse_url(argv[optind])) {
        fprintf(stderr, "%s is not a valid URL\n", argv[optind]);
        exit(1);
    }

    if (!bench.resolve()) {
        fprintf(stderr, "Could not resolve hostname: %s\n", bench.host.c_str());
        exit(1);
    }

    auto total_cost = bench.bench();

    auto total_secs = (double)total_cost / 1000;
    auto total_latency_ms = (double)total_latency / 1000;

    printf("Total of %zu bytes received.\n", total_bytes);
    printf("Total of %d requests completed in %.3f secs.\n", complete_requests, total_secs);
    if (successful) {
        printf("Throughput: %.2f [Kbytes/sec]\n", total_bytes / 1024 / total_secs);
        printf("Requests per second: %.2f [#/sec]\n", complete_requests / total_secs);
        printf("Latency per request: %.2f [ms]\n", total_latency_ms / complete_requests);
    }
}
