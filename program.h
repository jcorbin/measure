#ifndef _PROGRAM_H
#define _PROGRAM_H

#include "error.h"

struct program {
    const char *path;
    const char **argv;
    const char *cwd;
    const char *stdin;
    const char *stdout;
    const char *stderr;
};

#define program_init() {NULL, NULL, NULL, NULL, NULL, NULL}

int program_set_path(
    struct program *prog,
    const char *path,
    struct error_buffer *errbuf);

int program_set_argv(
    struct program *prog,
    unsigned int argc,
    const char *argv[],
    struct error_buffer *errbuf);

#endif // _PROGRAM_H
