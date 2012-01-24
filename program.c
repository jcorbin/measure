/* Copyright (C) 2012, Joshua T Corbin <jcorbin@wunjo.org>
 *
 * This file is part of measure, a program to measure programs.
 *
 * Measure is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Measure is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Measure.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "childcomm.h"
#include "program.h"

#define child_die(mess) exit( \
    child_comm_send_mess(commfd, mess) < 0 \
    ? CHILD_EXIT_COMMERROR : 1)

int program_set_path(
    struct program *prog,
    const char *path,
    struct error_buffer *errbuf) {

    char buf[PATH_MAX];
    if (strchr(path, '/') != NULL) {
        if (realpath(path, buf) == NULL) {
            snprintf(errbuf->s, errbuf->n,
                "failed to resolve %s, %s", path, strerror(errno));
            return -1;
        }

        struct stat s;
        if (stat(buf, &s) != 0) {
            snprintf(errbuf->s, errbuf->n,
                "failed to stat() %s, %S", buf, strerror(errno));
            return -1;
        }

        if (! S_ISREG(s.st_mode)) {
            snprintf(errbuf->s, errbuf->n,
                "%s isn't a regular file", buf);
            return -1;
        }

        if (access(buf, X_OK) != 0) {
            snprintf(errbuf->s, errbuf->n,
                "cannot access %s, %s", buf, strerror(errno));
            return -1;
        }

        prog->path = strdup(buf);
        if (prog->path == NULL) {
            strncpy(errbuf->s, "strdup() failed", errbuf->n);
            return -1;
        }

        return 0;
    }

    const char *pathcomp, *pathsep;
    size_t pathlen = strlen(path);
    char *ret = NULL;
    pathcomp = pathsep = getenv("PATH");
    while (*pathsep != '\0') {
        char *bufp = buf;
        pathsep = strchrnul(pathcomp, ':');
        size_t len = pathsep - pathcomp;
        bufp = memcpy(bufp, pathcomp, len) + len;
        *(bufp++) = '/';
        bufp = memcpy(bufp, path, pathlen) + pathlen;
        *bufp = '\0';
        if (access(buf, X_OK) == 0) {
            pathlen = bufp - buf;
            char *path = malloc(pathlen);
            if (path == NULL) {
                strncpy(errbuf->s, "malloc() failed", errbuf->n);
                return -1;
            }
            prog->path = memcpy(path, buf, pathlen);
            return 0;
        }
        pathcomp = pathsep + 1;
    }

    snprintf(errbuf->s, errbuf->n,
        "couldn't find %s in $PATH=%s",
        path, getenv("PATH"));
    return -1;
}

int program_set_argv(
    struct program *prog,
    unsigned int argc,
    const char *argv[],
    struct error_buffer *errbuf) {

    if (program_set_path(prog, argv[0], errbuf) != 0) return -1;

    prog->argv = calloc(argc + 1, sizeof(char *));
    if (prog->argv == NULL) {
        strncpy(errbuf->s, "calloc() failed", errbuf->n);
        return -1;
    }

    const char **p = prog->argv;
    for (int i=0; i<argc; i++)
        *(p++) = argv[i];

    return 0;
}

static const char *nullfile = "/dev/null";

void program_result_free(struct program_result *res) {
    if (res->stdin != NULL &&
        res->stdin != nullfile &&
        res->stdin != res->prog->stdin)
        free((char *) res->stdin);
    res->stdin = NULL;

    if (res->stdout != NULL &&
        res->stdout != nullfile &&
        res->stdout != res->prog->stdout)
        free((char *) res->stdout);
    res->stdout = NULL;

    if (res->stderr != NULL &&
        res->stderr != nullfile &&
        res->stderr != res->prog->stderr)
        free((char *) res->stderr);
    res->stderr = NULL;
}

static const char *stdname[3] = {"stdin", "stdout", "stderr"};
static const int stdflags[3] = {O_RDONLY, O_WRONLY, O_WRONLY};

int _child_std_setup(
        struct program_result *res,
        int commfd,
        struct error_buffer *errbuf) {

    const char *progpaths[3] = {
        res->prog->stdin,
        res->prog->stdout,
        res->prog->stderr};
    const char **stdpaths = progpaths;

    for (int i=0; i<3; i++) {
        if (stdpaths[i] == NULL)
            stdpaths[i] = nullfile;
        else if (stdpaths[i] != nullfile &&
                 strcmp(stdpaths[i], nullfile) == 0)
            stdpaths[i] = nullfile;

        int fd = -1;
        char *buf = NULL;
        if (stdflags[i] & O_WRONLY && stdpaths[i] != nullfile) {
            buf = strdup(stdpaths[i]);
            if (buf == NULL) {
                strncpy(errbuf->s, "strdup() failed", errbuf->n);
                return -1;
            }
            fd = mkostemp(buf, stdflags[i] | O_CLOEXEC);
            if (fd < 0) {
                snprintf(errbuf->s, errbuf->n, "mkstemp() failed for %s, %s",
                    stdpaths[i], strerror(errno));
                free(buf);
                return -1;
            }
            if (fchmod(fd, S_IRUSR) < 0) {
                snprintf(errbuf->s, errbuf->n, "fchmod() failed for %s, %s",
                    buf, strerror(errno));
                free(buf);
                return -1;
            }
            stdpaths[i] = buf;
        } else {
            fd = open(stdpaths[i], stdflags[i] | O_CLOEXEC);
            if (fd < 0) {
                snprintf(errbuf->s, errbuf->n, "failed to open %s, %s",
                    stdpaths[i], strerror(errno));
                return -1;
            }
        }

        if (child_comm_send_filepath(commfd, stdname[i], stdpaths[i]) < 0)
            exit(CHILD_EXIT_COMMERROR);

        if (buf != NULL)
            free(buf);

        if (dup2(fd, i) < 0) {
            snprintf(errbuf->s, errbuf->n,
                "%s dup2 failed, %s", stdname[i], strerror(errno));
            return -1;
        }
    }

    return 0;
}

void _child_run(struct program_result *res, int commfd) {
    struct error_buffer errbuf;

    if (_child_std_setup(res, commfd, &errbuf) < 0)
        child_die(errbuf.s);

    struct timespec t;
    struct child_comm c;
    c.id   = CHILD_COMM_ID_STARTTIME;
    c.len  = sizeof(struct timespec);
    c.data = &t;

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &t) != 0) {
        snprintf(errbuf.s, errbuf.n,
            "clock_gettime(CLOCK_MONOTONIC_RAW) failed, %s",
            strerror(errno));
        child_die(errbuf.s);
    }
    if (child_comm_write(commfd, &c) < 0)
        exit(CHILD_EXIT_COMMERROR);

    if (execv(res->prog->path, (char * const*) res->prog->argv) < 0) {
        snprintf(errbuf.s, errbuf.n,
            "execv() failed, %s", strerror(errno));
        child_die(errbuf.s);
    }
}

int _program_run(
    struct error_buffer *errbuf,
    struct program_result *res) {

    int commpipe[2];

    if (pipe(commpipe) < 0) {
        snprintf(errbuf->s, errbuf->n,
            "pipe() failed, %s", strerror(errno));
        return -1;
    }
    fcntl(commpipe[0], F_SETFD, FD_CLOEXEC);
    fcntl(commpipe[1], F_SETFD, FD_CLOEXEC);

    pid_t pid = fork();
    if (pid == 0) {
        _child_run(res, commpipe[1]);
        // shouldn't happen, _child_run execv()s or exit()s
        exit(0xfe);
    } else if (pid < 0) {
        snprintf(errbuf->s, errbuf->n,
            "fork() failed, %s", strerror(errno));
        return -1;
    }

    if (close(commpipe[1]) < -1) {
        snprintf(errbuf->s, errbuf->n,
            "failed to close child write pipe, %s", strerror(errno));
        return -1;
    }

    // TODO: try using a SIGCHLD handler rather than blocking wait
    //       * pause(3P)
    //       * sigaction(3P)

    pid_t r = wait4(pid, &res->status, 0, &res->rusage);
    if (r < 0) {
        snprintf(errbuf->s, errbuf->n,
            "wait4 failed, %s", strerror(errno));
        return -1;
    }

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &res->end) != 0) {
        snprintf(errbuf->s, errbuf->n,
            "clock_gettime(CLOCK_MONOTONIC_RAW) failed, %s",
            strerror(errno));
        return -1;
    }

    unsigned char got_start = 0;
    struct child_comm comm = {0, 0, NULL};
    while (child_comm_read(commpipe[0], &comm) == 0) {
        if (comm.id == CHILD_COMM_ID_MESS) {
            *((char *) comm.data + comm.len - 1) = '\0';
            strncpy(errbuf->s, "child failed: ", errbuf->n);
            size_t n = errbuf->n - 14;
            if (comm.len < n) n = comm.len;
            strncpy(errbuf->s + 14, comm.data, n);
            free((void *) comm.data);
            return -1;
        } else if (comm.id == CHILD_COMM_ID_FILEPATH) {
            const char *name = (char *) comm.data;
            const char *path = (char *) memchr(name, '\0', comm.len);
            if (path == NULL) {
                strncpy(errbuf->s,
                    "no null byte in filepath child message",
                    errbuf->n);
                free((void *) comm.data);
                return -1;
            }
            path++;

            const char **dst = NULL;
            if (strcmp(name, "stdin") == 0)
                dst = &res->stdin;
            else if (strcmp(name, "stdout") == 0)
                dst = &res->stdout;
            else if (strcmp(name, "stderr") == 0)
                dst = &res->stderr;
            else {
                snprintf(errbuf->s, errbuf->n,
                    "invalid filepath name \"%s\"", name);
                free((void *) comm.data);
                return -1;
            }
            path = strdup(path);
            if (path == NULL) {
                strncpy(errbuf->s, "strdup() failed", errbuf->n);
                free((void *) comm.data);
                return -1;
            }
            *dst = path;
        } else if (comm.id == CHILD_COMM_ID_STARTTIME) {
            if (comm.len != sizeof(struct timespec)) {
                snprintf(errbuf->s, errbuf->n,
                    "expected %i bytes for start time, got %i",
                    sizeof(struct timespec), comm.len);
                free((void *) comm.data);
                return -1;
            }
            memcpy(&res->start, comm.data, sizeof(struct timespec));
            got_start = 1;
        } else {
            snprintf(errbuf->s, errbuf->n,
                "received unknown message id %02x from child", comm.id);
            free((void *) comm.data);
            return -1;
        }
        free((void *) comm.data);
        comm.id   = 0;
        comm.len  = 0;
        comm.data = NULL;
    }

    if (! got_start) {
        if (WIFEXITED(res->status)) {
            unsigned char exitval = WEXITSTATUS(res->status);
            if (exitval == CHILD_EXIT_COMMERROR)
                strncpy(errbuf->s, "chlid communication error", errbuf->n);
            else
                snprintf(errbuf->s, errbuf->n,
                    "unknown child failure, exited %i", res->status);
        } else if (WIFSIGNALED(res->status)) {
            snprintf(errbuf->s, errbuf->n,
                "child exited due to signal %i", WTERMSIG(res->status));
            return -1;
        } else {
            snprintf(errbuf->s, errbuf->n,
                "unknown child failure, exit status %x", res->status);
        }
        return -1;
    }

    if (close(commpipe[0]) < -1) {
        snprintf(errbuf->s, errbuf->n,
            "failed to close child read pipe, %s", strerror(errno));
        return -1;
    }

    return 0;
}

struct program_result *program_run(
    const struct program *prog,
    struct program_result *res,
    struct error_buffer *errbuf) {

    memset(res, 0, sizeof(struct program_result));
    res->prog = prog;

    if (_program_run(errbuf, res) < 0)
        res = NULL;

    return res;
}
