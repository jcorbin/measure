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

int program_set_cwd(
    struct program *prog,
    const char *path,
    struct error_buffer *errbuf) {

    char buf[PATH_MAX];
    if (realpath(path, buf) == NULL) {
        snprintf(errbuf->s, errbuf->n,
            "failed to resolve %s: %s", path, strerror(errno));
        return -1;
    }

    prog->cwd = strdup(buf);

    return 0;
}

int program_set_path(
    struct program *prog,
    const char *path,
    struct error_buffer *errbuf) {

    char buf[PATH_MAX];
    if (strchr(path, '/') != NULL) {
        if (realpath(path, buf) == NULL) {
            snprintf(errbuf->s, errbuf->n,
                "failed to resolve %s: %s", path, strerror(errno));
            return -1;
        }

        struct stat s;
        if (stat(buf, &s) != 0) {
            snprintf(errbuf->s, errbuf->n,
                "failed to stat() %s: %S", buf, strerror(errno));
            return -1;
        }

        if (! S_ISREG(s.st_mode)) {
            snprintf(errbuf->s, errbuf->n,
                "%s isn't a regular file", buf);
            return -1;
        }

        if (access(buf, X_OK) != 0) {
            snprintf(errbuf->s, errbuf->n,
                "cannot access %s: %s", buf, strerror(errno));
            return -1;
        }
        prog->path = strdup(buf);
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

    prog->argv = malloc(sizeof(char *) * (argc + 1));
    if (prog->argv == NULL) {
        strncpy(errbuf->s, "malloc() failed", errbuf->n);
        return -1;
    }

    const char **p = prog->argv;
    for (int i=0; i<argc; i++)
        *(p++) = argv[i];
    *p = NULL;

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

struct run_state {
    struct program_result *res;
    pid_t pid;
    int comm[2];
    int stdinfd;
    int stdoutfd;
    int stderrfd;
};

int _open_run_file(
    const char *path, int oflag,
    const char **dst, int *fd,
    struct error_buffer *errbuf) {

    if (path == NULL) path = nullfile;
    int res = open(path, oflag | O_CLOEXEC);
    if (res < 0) {
        snprintf(errbuf->s, errbuf->n,
            "failed to open %s: %s", path, strerror(errno));
        return -1;
    }

    *dst = path;
    *fd  = res;
    return 0;
}

int _open_run_tempfile(
    const char *path, const char **dst, int *fd,
    struct error_buffer *errbuf) {

    char *buf = strdup(path);
    int res = mkostemp(buf, O_WRONLY | O_CLOEXEC);
    if (res < 0) {
        snprintf(errbuf->s, errbuf->n,
            "mkstemp() failed for %s: %s", path, strerror(errno));
        free(buf);
        return -1;
    }

    *dst = buf;
    *fd  = res;
    return 0;
}

#define _open_run_input_file(path, dst, fd, errbuf) \
    _open_run_file(path, O_RDONLY, dst, fd, errbuf)

int _open_run_output_file(
    const char *path, const char **dst, int *fd,
    struct error_buffer *errbuf) {

    if (path == NULL || strcmp(path, nullfile) == 0)
        return _open_run_file(nullfile, O_WRONLY, dst, fd, errbuf);
    else
        return _open_run_tempfile(path, dst, fd, errbuf);
}

void _child_run(struct run_state *run) {
    if (close(run->comm[0]) < -1) {
        fprintf(stderr,
            "child failed to close() read half of comm pipe: %s",
            strerror(errno));
        exit(CHILD_EXIT_COMMERROR);
    }

    struct error_buffer errbuf;

    if (dup2(run->stdinfd, 0) < 0) {
        snprintf(errbuf.s, errbuf.n,
            "stdin dup2 failed: %s", strerror(errno));
        child_die(errbuf.s);
    }

    if (dup2(run->stdoutfd, 1) < 0) {
        snprintf(errbuf.s, errbuf.n,
            "stdout dup2 failed: %s", strerror(errno));
        child_die(errbuf.s);
    }

    if (dup2(run->stderrfd, 2) < 0) {
        snprintf(errbuf.s, errbuf.n,
            "stderr dup2 failed: %s", strerror(errno));
        child_die(errbuf.s);
    }

    const char *path  = run->res->prog->path;
    const char **argv = run->res->prog->argv;

    struct timespec t;
    struct child_comm c;
    c.id   = CHILD_COMM_ID_STARTTIME;
    c.len  = sizeof(struct timespec);
    c.data = &t;

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &t) != 0) {
        snprintf(errbuf.s, errbuf.n,
            "clock_gettime(CLOCK_MONOTONIC_RAW) failed: %s",
            strerror(errno));
        child_die(errbuf.s);
    }
    if (child_comm_write(run->comm[1], &c) < 0)
        exit(CHILD_EXIT_COMMERROR);

    if (execv(path, (char * const*) argv) < 0) {
        snprintf(errbuf.s, errbuf.n,
            "execv() failed: %s", strerror(errno));
        child_die(errbuf.s);
    }
}

int _program_run(
    struct error_buffer *errbuf,
    struct run_state *run) {

    if (_open_run_input_file(
            run->res->prog->stdin, &run->res->stdin, &run->stdinfd,
            errbuf) < 0) return -1;

    if (_open_run_output_file(
            run->res->prog->stdout, &run->res->stdout, &run->stdoutfd,
            errbuf) < 0) return -1;

    if (_open_run_output_file(
            run->res->prog->stderr, &run->res->stderr, &run->stderrfd,
            errbuf) < 0) return -1;

    if (pipe(run->comm) < 0) {
        snprintf(errbuf->s, errbuf->n,
            "pipe() failed: %s", strerror(errno));
        return -1;
    }

    run->pid = fork();
    if (run->pid == 0) {
        _child_run(run);
        // shouldn't happen, _child_run execv()s or exit()s
        exit(0xfe);
    } else if (run->pid < 0) {
        snprintf(errbuf->s, errbuf->n,
            "fork() failed: %s", strerror(errno));
        return -1;
    }

    if (close(run->comm[1]) < -1) {
        snprintf(errbuf->s, errbuf->n,
            "failed to close child write pipe: %s", strerror(errno));
        return -1;
    }

    // TODO: try using a SIGCHLD handler rather than blocking wait
    //       * pause(3P)
    //       * sigaction(3P)

    pid_t r = wait4(run->pid, &run->res->status, 0, &run->res->rusage);
    if (r < 0) {
        snprintf(errbuf->s, errbuf->n,
            "wait4 failed: %s", strerror(errno));
        return -1;
    }

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &run->res->end) != 0) {
        snprintf(errbuf->s, errbuf->n,
            "clock_gettime(CLOCK_MONOTONIC_RAW) failed: %s",
            strerror(errno));
        return -1;
    }

    unsigned char got_start = 0;
    struct child_comm comm = {0, 0, NULL};
    while (child_comm_read(run->comm[0], &comm) == 0) {
        if (comm.id == CHILD_COMM_ID_MESS) {
            *((char *) comm.data + comm.len - 1) = '\0';
            strncpy(errbuf->s, "child failed: ", errbuf->n);
            size_t n = errbuf->n - 14;
            if (comm.len < n) n = comm.len;
            strncpy(errbuf->s + 14, comm.data, n);
            free((void *) comm.data);
            return -1;
        } else if (comm.id == CHILD_COMM_ID_STARTTIME) {
            if (comm.len != sizeof(struct timespec)) {
                snprintf(errbuf->s, errbuf->n,
                    "expected %i bytes for start time, got %i",
                    sizeof(struct timespec), comm.len);
                free((void *) comm.data);
                return -1;
            }
            memcpy(&run->res->start, comm.data, sizeof(struct timespec));
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
        if (WIFEXITED(run->res->status)) {
            unsigned char exitval = WEXITSTATUS(run->res->status);
            if (exitval == CHILD_EXIT_COMMERROR)
                strncpy(errbuf->s, "chlid communication error", errbuf->n);
            else
                snprintf(errbuf->s, errbuf->n,
                    "unknown child failure, exited %i", run->res->status);
        } else if (WIFSIGNALED(run->res->status)) {
            snprintf(errbuf->s, errbuf->n,
                "child exited due to signal %i", WTERMSIG(run->res->status));
            return -1;
        } else {
            snprintf(errbuf->s, errbuf->n,
                "unknown child failure, exit status %x", run->res->status);
        }
        return -1;
    }

    if (close(run->comm[0]) < -1) {
        snprintf(errbuf->s, errbuf->n,
            "failed to close child read pipe: %s", strerror(errno));
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

    struct run_state run = {res, 0, {-1, -1}, -1, -1, -1};

    if (_program_run(errbuf, &run) < 0)
        res = NULL;

    // TODO: close() returns <0, do we care?
    if (run.stdinfd >= 0)  close(run.stdinfd);
    if (run.stdoutfd >= 0) close(run.stdoutfd);
    if (run.stderrfd >= 0) close(run.stderrfd);

    return res;
}
