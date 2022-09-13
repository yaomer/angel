#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <angel/util.h>
#include <angel/logger.h>

namespace angel {
namespace util {

void daemon()
{
    pid_t pid;

    if ((pid = fork()) < 0) {
        log_fatal("fork error: %s", strerrno());
    }
    if (pid > 0)
        exit(0);

    if (setsid() < 0) {
        log_fatal("setsid error: %s", strerrno());
    }
    umask(0);

    int fd = open("/dev/null", O_RDWR);
    if (fd < 0) {
        log_fatal("open /dev/null error: %s", strerrno());
    }

    if (dup2(fd, STDIN_FILENO) < 0) {
        log_fatal("dup2 stdin error: %s", strerrno());
    }
    if (dup2(fd, STDOUT_FILENO) < 0) {
        log_fatal("dup2 stdout error: %s", strerrno());
    }
    if (dup2(fd, STDERR_FILENO) < 0) {
        log_fatal("dup2 stderr error: %s", strerrno());
    }

    if (fd > STDERR_FILENO)
        close(fd);
}

}
}
