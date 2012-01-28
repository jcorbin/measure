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

#ifndef _PROGRAM_H
#define _PROGRAM_H

#include <sys/resource.h>
#include <time.h>

#include "error.h"

struct program {
    const char *path;
    const char **argv;
    const char *stdin;
    const char *stdout;
    const char *stderr;
};

struct program_result {
    const struct program *prog;
    pid_t pid;
    struct timespec start;
    struct timespec end;
    int status;
    struct rusage rusage;
    const char *stdin;
    const char *stdout;
    const char *stderr;
};

#define program_init() {NULL, NULL, NULL, NULL, NULL}

#define program_result_init() {\
    NULL, 0, {0, 0}, {0, 0}, 0, \
    {{0, 0}, {0, 0}, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, \
    NULL, NULL, NULL}

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
