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

#endif // _PROGRAM_H
