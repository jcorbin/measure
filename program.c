#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
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

struct program_result *program_run(
    const struct program *prog,
    struct program_result *res,
    struct error_buffer *errbuf) {

    res->prog = prog;

    strncpy(errbuf->s, "program_run unimplemented", errbuf->n);
    return NULL;
}
