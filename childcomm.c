#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "childcomm.h"

#define CHILD_COMM_HDRLEN sizeof(size_t) + 1

int child_comm_send(int fd, const void *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        size_t wrote = write(fd, buf + off, len - off);
        if (wrote < 0) return -1;
        off += wrote;
    }
    return 0;
}

int child_comm_recv(int fd, void *buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        size_t got = read(fd, buf + off, len - off);
        if (got == 0)
            return -2;
        if (got < 0)
            return -1;
        off += got;
    }
    return 0;
}

int child_comm_write(int fd, const struct child_comm *comm) {
    int r = child_comm_send(fd, comm, CHILD_COMM_HDRLEN);
    if (r < 0) return r;
    r = child_comm_send(fd, comm->data, comm->len);
    if (r < 0) return r;
    return 0;
}

int child_comm_read(int fd, struct child_comm *comm) {
    memset(comm, 0, sizeof(struct child_comm));
    if (child_comm_recv(fd, comm, CHILD_COMM_HDRLEN) < 0)
        return COMM_READ_ERR_HEADER;
    void *buf = malloc(comm->len);
    if (buf == NULL)
        return COMM_READ_ERR_MALLOC;
    if (child_comm_recv(fd, buf, comm->len) < 0) {
        free(buf);
        return COMM_READ_ERR_DATA;
    }
    comm->data = buf;
    return 0;
}

int child_comm_send_mess(int fd, const char *mess) {
    struct child_comm c;
    c.id   = CHILD_COMM_ID_MESS;
    c.len  = strlen(mess)+1;
    c.data = mess;
    return child_comm_write(fd, &c);
}

int child_comm_send_filepath(int fd, const char *name, const char *path) {
    size_t namelen = strlen(name);
    size_t pathlen = strlen(path);
    void *buf;
    struct child_comm c;
    c.id   = CHILD_COMM_ID_FILEPATH;
    c.len  = namelen + 1 + pathlen + 1;
    c.data = buf = malloc(c.len);
    if (buf == NULL)
        return -1;

    memcpy(buf, name, namelen);
    buf += namelen;
    *((char *) buf++) = '\0';

    memcpy(buf, path, pathlen);
    buf += pathlen;
    *((char *) buf++) = '\0';

    int r = 0;
    if (child_comm_write(fd, &c) < 0)
        r = -1;
    free((void *) c.data);
    return r;
}
