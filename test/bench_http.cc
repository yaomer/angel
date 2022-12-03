#include <getopt.h>

#include <angel/client.h>
#include <angel/util.h>
#include <angel/resolver.h>
#include <angel/config.h>

#if defined (ANGEL_USE_OPENSSL)
#include <angel/ssl_client.h>
#endif

static int num_requests = 20000;
static int concurrency  = 1;
static int timelimit    = 60 * 1000;
static int timeout      = 15 * 1000;
static int max_timeouts = 10;

static int send_requests = 0, completions = 0, failures = 0, timeouts = 0;
static long long total_bytes = 0, total_latency = 0;

struct request_info {
    std::unique_ptr<angel::client> cli;
    long long start = angel::util::get_cur_time_us(); // request start time
    size_t timeout_timer_id;
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

    void check_finish()
    {
        if (completions + failures + timeouts == num_requests) {
            loop->quit();
        } else if (timeouts >= max_timeouts) {
            printf("### Too many requests timed out.\n");
            printf("### Benchmarking Aborted.\n");
            printf("### It may means that\n"
                   "### 1) The timeout specified by -s is too short.\n"
                   "### 2) Too many TCP connections are on the server in TIME_WAIT state,\n"
                   "       cause the server has no ports available.\n");
            max_timeouts = INT_MAX;
            loop->quit();
        } else if (send_requests < num_requests) {
            launch_request();
        }
    }
};

void bench_http::launch_request()
{
    auto *ri = new request_info();

    ri->idx = send_requests++;
    requests.emplace(ri->idx, ri);

    ri->timeout_timer_id = loop->run_after(timeout, [this, ri]{
            requests.erase(ri->idx);
            timeouts++;
            check_finish();
            });

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
    ri->cli->set_connection_failure_handler([this, ri]{
            loop->cancel_timer(ri->timeout_timer_id);
            requests.erase(ri->idx);
            failures++;
            check_finish();
            });
    ri->cli->set_message_handler([](const angel::connection_ptr& conn, angel::buffer& buf){
            // We don't parse http response.
            total_bytes += buf.readable();
            buf.retrieve_all();
            });
    ri->cli->set_close_handler([this, ri](const angel::connection_ptr& conn){
            if (!conn->is_reset_by_peer()) return;
            this->close_handler(ri);
            });
    ri->cli->start();
}

void bench_http::close_handler(request_info *ri)
{
    loop->cancel_timer(ri->timeout_timer_id);
    completions++;
    total_latency += angel::util::get_cur_time_us() - ri->start;
    if ((num_requests / 10) && completions % (num_requests / 10) == 0)
        printf("Completed %d requests\n", completions);
    requests.erase(ri->idx);
    check_finish();
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
    loop->run_after(timelimit, [this]{
            fprintf(stderr, "### Benchmarking timed out.\n");
            loop->quit();
            });

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
            "    -t <timelimit>   Seconds to max. to spend on benchmarking. Default is 60 secs.\n"
            "    -s <timeout>     Seconds to max. wait for each response. Default is 15 secs.\n"
            "    -S <number>      Maximum number of timeout requests. Default is 10.\n"
           );
    exit(1);
}

int main(int argc, char *argv[])
{
    int c;
    while ((c = getopt(argc, argv, "c:n:t:s:S:")) != -1) {
        switch (c) {
        case 'c':
            concurrency = atoi(optarg);
            break;
        case 'n':
            num_requests = atoi(optarg);
            break;
        case 't':
            timelimit = atoi(optarg) * 1000;
            break;
        case 's':
            timeout = atoi(optarg) * 1000;
            break;
        case 'S':
            max_timeouts = atoi(optarg);
            break;
        default:
            fprintf(stderr, "Illegal argument \"%c\"\n", c);
            usage();
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

    printf("Benchmarking...\n");

    auto total_cost = bench.bench();

    auto total_secs = (double)total_cost / 1000;
    auto total_latency_ms = (double)total_latency / 1000;

    printf("========================================\n");
    printf("Total of %lld bytes received.\n", total_bytes);
    printf("Total of %d requests completed, %d failed, %d timed out in %.3f secs.\n",
            completions, failures, timeouts, total_secs);
    printf("Throughput: %lld (bytes/sec)\n", (long long)(total_bytes / total_secs));
    printf("Requests per second: %.2f (#/sec)\n", completions / total_secs);
    printf("Latency per request: %.2f (ms)\n", total_latency_ms / completions);
}
