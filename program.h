#ifndef _PROGRAM_H
#define _PROGRAM_H

#include <sys/resource.h>
#include <time.h>

#include "error.h"

struct program {
    const char *path;
    const char **argv;
    const char *cwd;
    const char *stdin;
    const char *stdout;
    const char *stderr;
};

struct program_result {
    const struct program *prog;
    struct timespec start;
    struct timespec end;
    int status;
    struct rusage rusage;
    const char *stdin;
    const char *stdout;
    const char *stderr;
};

#define program_init() {NULL, NULL, NULL, NULL, NULL, NULL}

#define program_result_init() {\
    NULL, {0, 0}, {0, 0}, 0, \
    {{0, 0}, {0, 0}, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, \
    NULL, NULL, NULL}

int program_set_cwd(
    struct program *prog,
    const char *path,
    struct error_buffer *errbuf);

int program_set_path(
    struct program *prog,
    const char *path,
    struct error_buffer *errbuf);

int program_set_argv(
    struct program *prog,
    unsigned int argc,
    const char *argv[],
    struct error_buffer *errbuf);

void program_result_free(struct program_result *res);

struct program_result *program_run(
    const struct program *prog,
    struct program_result *res,
    struct error_buffer *errbuf);

#endif // _PROGRAM_H
