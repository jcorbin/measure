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

#include <signal.h>
#include <stdlib.h>
#include <string.h>

static struct sigaction old_sigterm, old_sigint;

void exit_cleanly_from_signal(int signo);

void setup_signal_handlers(void) {
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = exit_cleanly_from_signal;

    sigaddset(&act.sa_mask, SIGTERM);
    sigaddset(&act.sa_mask, SIGINT);
    sigaddset(&act.sa_mask, SIGHUP);
    sigaddset(&act.sa_mask, SIGPIPE);

    sigaction(SIGTERM, &act, &old_sigterm);
    sigaction(SIGINT,  &act, &old_sigint);
    sigaction(SIGHUP,  &act, NULL);
    sigaction(SIGPIPE, &act, NULL);
}

void exit_cleanly_from_signal(int signo) {
    // Restore default signal action for and unblock SIGTERM and SIGINT so
    // that repeated signal during teardown will cause immediate exit; in
    // other words, obey insistent user request to gtfo.
    sigaction(SIGTERM, &old_sigterm, NULL);
    sigaction(SIGINT,  &old_sigint,  NULL);
    sigset_t sigmask;
    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGTERM);
    sigaddset(&sigmask, SIGINT);
    sigprocmask(SIG_UNBLOCK, &sigmask, NULL);

    fflush(stdout);
    fflush(stderr);

    // Cleanup happens via atexit(3)
    exit(0);
}
