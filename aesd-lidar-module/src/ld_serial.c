#define _DEFAULT_SOURCE 
#include "ld_lidar.h"

#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

int ld_serial_configure(int fd)
{
    struct termios tio;
    if (tcgetattr(fd, &tio) != 0)
        return -1;

    cfmakeraw(&tio);
    tio.c_cflag |= (tcflag_t)(CLOCAL | CREAD);
    tio.c_cflag &= (tcflag_t)~(CSTOPB | PARENB | CRTSCTS | CSIZE);
    tio.c_cflag |= CS8;

    tio.c_cc[VMIN]  = 0;
    tio.c_cc[VTIME] = 1;

    if (cfsetispeed(&tio, B230400) != 0 || cfsetospeed(&tio, B230400) != 0)
        return -1;
    if (tcsetattr(fd, TCSANOW, &tio) != 0)
        return -1;
    (void)tcflush(fd, TCIFLUSH);
    return 0;
}

int ld_serial_open(const char *path)
{
    int fd = open(path, O_RDONLY | O_NOCTTY | O_CLOEXEC | O_NONBLOCK);
    if (fd < 0)
        return -1;
    int flags = fcntl(fd, F_GETFL);
    if (ld_serial_configure(fd) != 0 || flags < 0 ||
        fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) != 0) {
        int saved = errno;
        close(fd);
        errno = saved;
        return -1;
    }
    return fd;
}
