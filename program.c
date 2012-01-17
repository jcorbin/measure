#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "program.h"

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
    int stdinfd;
    int stdoutfd;
    int stderrfd;
};

int _open_run_file(
    const char *path, int oflag,
    const char **dst, int *fd,
    struct error_buffer *errbuf) {

    if (path == NULL) path = nullfile;
    int res = open(path, oflag);
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
    int res = mkstemp(buf);
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
    fputs("child not implemented\n", stderr);
    exit(1);
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

    pid_t r = wait4(run->pid, &run->res->status, 0, &run->res->rusage);
    if (r < 0) {
        snprintf(errbuf->s, errbuf->n,
            "wait4 failed: %s", strerror(errno));
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

    struct run_state run = {res, 0, -1, -1, -1};

    if (_program_run(errbuf, &run) < 0)
        res = NULL;

    // TODO: close() returns <0, do we care?
    if (run.stdinfd >= 0)  close(run.stdinfd);
    if (run.stdoutfd >= 0) close(run.stdoutfd);
    if (run.stderrfd >= 0) close(run.stderrfd);

    return res;
}
