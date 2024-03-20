#pragma once

#include "types.h"

static inline int str2mac(const char *s, uint8_t *val) {
	if (sscanf(s, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &val[0], &val[1], &val[2], &val[3], &val[4], &val[5]) == 6)
	// if (sscanf(s, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &val[5], &val[4], &val[3], &val[2], &val[1], &val[0]) == 6)
		return 0;
	return -1;
}


static inline uint32_t str2ip(const char *s) {
    // convert string to ip address
    uint32_t val[4];
    if (sscanf(s, "%d.%d.%d.%d", &val[0], &val[1], &val[2], &val[3])!= 4) {
        return -1;
    }
    // big endian
    return (val[0] << 24) | (val[1] << 16) | (val[2] << 8) | val[3];
}

static inline std::string sin_to_str(struct sockaddr_in *sin) {
    char buf[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf));
    return std::string(buf) + ":" + std::to_string(ntohs(sin->sin_port));
}

static inline int try_read_msg(int fd, void *buf, size_t len, int64_t timeout) {

    ssize_t ret;
    ret = read(fd, buf, len);

    if (ret < 0) {
        if (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK) {
            return 0;
        }
        else {
            return -1;
        }
    }
    buf += ret;
    len -= ret;
    auto begin_time = std::chrono::high_resolution_clock::now();
    while (len > 0) {
        ret = read(fd, buf, len);

        if (ret < 0) {
            if (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK) {
                return 0;
            }
            else {
                return -1;
            }
        }
        buf += ret;
        len -= ret;
        if (ret > 0) {
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = end_time - begin_time;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() > timeout) {
                return -1;
            }
        }
    }
    return 1;
}
