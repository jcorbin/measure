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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "childcomm.h"
#include "child.h"

#define child_die(mess) exit( \
    child_comm_send_mess(commfd, mess) < 0 \
    ? CHILD_EXIT_COMMERROR : 1)

struct child_std {
    const char *name;
    int targetfd;
    const char *template;
    char *path;
    int fd;
};

int child_std_open(
    struct child_std *cs,
    struct error_buffer *errbuf) {

    cs->path = strdup(cs->template);
    if (cs->path == NULL) {
        strncpy(errbuf->s, "strdup() failed", errbuf->n);
        return -1;
    }

    cs->fd = mkostemp(cs->path, O_WRONLY | O_CLOEXEC);
    if (cs->fd < 0) {
        snprintf(errbuf->s, errbuf->n, "mkstemp() failed for %s, %s",
            cs->template, strerror(errno));
        return -1;
    }

    if (fchmod(cs->fd, S_IRUSR) < 0) {
        snprintf(errbuf->s, errbuf->n, "fchmod() failed for %s, %s",
            cs->path, strerror(errno));
        return -1;
    }

    return 0;
}

int child_std_setup(
    struct program_result *res,
    int commfd,
    struct error_buffer *errbuf) {

    if (res->prog->stdinfd > 0)
        if (dup2(res->prog->stdinfd, 0) < 0) {
            snprintf(errbuf->s, errbuf->n,
                "stdin dup2 failed, %s", strerror(errno));
            return -1;
        }

    struct child_std cs[] = {
        {"stdout", STDOUT_FILENO, res->prog->stdout, NULL, -1},
        {"stderr", STDERR_FILENO, res->prog->stderr, NULL, -1}};

    for (int i=0; i<sizeof(cs)/sizeof(struct child_std); i++) {
        if (child_std_open(&cs[i], errbuf) < 0) {
            if (cs[i].path != NULL) free(cs[i].path);
            return -1;
        }

        if (child_comm_send_filepath(commfd, cs[i].name, cs[i].path) < 0)
            exit(CHILD_EXIT_COMMERROR);

        free(cs[i].path);

        if (dup2(cs[i].fd, cs[i].targetfd) < 0) {
            snprintf(errbuf->s, errbuf->n,
                "%s dup2 failed, %s", cs[i].name, strerror(errno));
            return -1;
        }
    }

    return 0;
}

void child_run(struct program_result *res, int commfd) {
    struct error_buffer errbuf;

    if (child_std_setup(res, commfd, &errbuf) < 0)
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
