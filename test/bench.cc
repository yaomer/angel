//
// Modified from https://github.com/libevent/libevent/blob/master/test/bench.c
//

#include <angel/evloop.h>
#include <angel/sockops.h>
#include <angel/util.h>
#include <angel/logger.h>

#include <unistd.h>
#include <sys/resource.h>

#include <iostream>

static int num_pipes, num_active, num_writes;
static int writes, count, failures, fired;
static std::vector<int> pipes;
static angel::evloop *g_loop = nullptr;

static void read_cb(int fd, int idx)
{
    int widx = idx + 1;
    unsigned char ch;
    ssize_t n;

    n = read(fd, (char*)&ch, sizeof(ch));
    if (n >= 0)
        count += n;
    else
        failures++;
    if (writes) {
        if (widx >= num_pipes)
            widx -= num_pipes;
        n = write(pipes[2 * widx + 1], "e", 1);
        if (n != 1)
            failures++;
        writes--;
        fired++;
    }

    if (count == fired) {
        g_loop->quit();
    }
}

int64_t run_once()
{
    angel::evloop loop;
    g_loop = &loop;

    for (int i = 0; i < num_pipes * 2; i += 2) {
        auto chl = std::make_shared<angel::channel>(&loop);
        chl->set_fd(pipes[i]);
        chl->set_read_handler([fd = pipes[i], idx = i]{ read_cb(fd, idx); });
        loop.add_channel(std::move(chl));
    }

    fired = 0;
    int space = num_pipes / num_active;
    space = space * 2;
    for (int i = 0; i < num_active; i++, fired++) {
        write(pipes[i * space + 1], "e", 1);
    }

    count = 0;
    writes = num_writes;
    auto t1 = angel::util::get_cur_time_us();
    loop.run();
    auto t2 = angel::util::get_cur_time_us();
    return t2 - t1;
}

int main(int argc, char *argv[])
{
    int c;
    num_pipes   = 100;
    num_active  = 1;
    num_writes  = num_pipes;
    while ((c = getopt(argc, argv, "n:a:w:")) != -1) {
        switch (c) {
        case 'n':
            num_pipes = atoi(optarg);
            break;
        case 'a':
            num_active = atoi(optarg);
            break;
        case 'w':
            num_writes = atoi(optarg);
            break;
        default:
            fprintf(stderr, "Illegal argument \"%c\"\n", c);
            exit(1);
        }
    }

    struct rlimit rl;
    rl.rlim_cur = rl.rlim_max = num_pipes * 2 + 100;
    if (setrlimit(RLIMIT_NOFILE, &rl) == -1) {
        perror("setrlimit");
        exit(1);
    }

    pipes.resize(num_pipes * 2);
    for (int i = 0; i < num_pipes * 2; i += 2) {
        angel::sockops::socketpair(&pipes[i]);
    }

    long total = 0;
    for (int i = 0; i < 25; i++) {
        long cost = run_once();
        std::cout << cost << '\n';
        total += cost;
    }
    std::cout << total / 25 << " (average)\n";

    exit(0);
}
