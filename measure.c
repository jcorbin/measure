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
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "error.h"
#include "program.h"
#include "sighandler.h"

// TODO:
// * variable arguments per execution for, e.g., output filenames as arguments

void print_result(struct program_result *res) {
    printf("%us,%uns", res->start.tv_sec, res->start.tv_nsec);
    putchar(' ');

    printf("%us,%uns", res->end.tv_sec, res->end.tv_nsec);
    putchar(' ');

    struct rusage *r = &res->rusage;

    printf("%us,%uus", r->ru_utime.tv_sec, r->ru_utime.tv_usec);
    putchar(' ');

    printf("%us,%uus", r->ru_stime.tv_sec, r->ru_stime.tv_usec);
    putchar(' ');

    printf("%u %u %u %u %u %u %u %u %u %u %u %u %u %u",
        r->ru_maxrss, r->ru_ixrss,
        r->ru_idrss, r->ru_isrss,
        r->ru_minflt, r->ru_majflt, r->ru_nswap,
        r->ru_inblock, r->ru_oublock,
        r->ru_msgsnd, r->ru_msgrcv, r->ru_nsignals,
        r->ru_nvcsw, r->ru_nivcsw);

    printf(" %d %s %s", res->status, res->stdout, res->stderr);
}

// for placing error messages in
#define ERRBUF_SIZE 4096

// for reading stdin
#define BUFFER_SIZE 4096

static const char *calledname = NULL;

void usage(unsigned int longhelp) {
    fprintf(stderr,
        "Usage: %s [options] [--] command [command arguments]\n"
        "Run a command %s and collect various measurements.\n"
        "\n",
        calledname,
        strcmp(calledname, "sample") == 0 ? "repeatedly" : "once");
    fprintf(stderr,
        "  -h          Show short usage screen.\n"
        "  --help      Show usage with explanatory epilog.\n"
        "  --usage     Print resource usage of the measuring process before\n"
        "              each command run.\n"
        "  --compress-stdout\n"
        "              Compress stdout files with gzip\n"
        "  --compress-stderr\n"
        "              Compress stderr files with gzip\n");
    if (strcmp(calledname, "sample") == 0)
        fprintf(stderr,
            "  -n <N>      Only sample N times rather than indefinately.\n");

    if (! longhelp) {
        fprintf(stderr,
            "\nTry '%s --help' for more information.\n", calledname);
        exit(0);
    }

    fprintf(stderr,
        "\nOutput format:\n"
        "  - Fields are space-separated, the first row is a header.\n"
        "  - If --usage is specified then the measuring process's resource\n"
        "    usage is taken and output before each command run; only the\n"
        "    resource usages fields are meaningful on these lines, all other\n"
        "    fields are zero or null.\n"
        "  - There-after the command is run (possibly many times) and the\n"
        "    fields described below are collected and output.\n"

        "\nOutput fields:\n"
        "  - start and end time from the monotonic clock as reported by\n"
        "    clock_gettime(3); times are represented as 'Ns,Mns' pairs where N\n"
        "    and M are integral counts of seconds and nanoseconds respectively.\n"
        "  - process exit status and resource usage (see wait4(2)).\n"
        "  - temporary files stdout_XXXXXX and stderr_XXXXXX are created in the\n"
        "    current working directory (for random values of XXXXXX) for each\n"
        "    command run; the corresponding filenames are in the final fields.\n");

    exit(0);
}

static struct program_result res = program_result_init();

// Flag controlling whether res has been shipped down stdout, or not;
// if not then we'll cleanup in response to a SIG{TERM,INT,HUP,PIPE} by
// unlinking the std{out,err} files we created and killing the child
// process.
static int result_sent;

void cleanup_current_result(void) {
    if (result_sent)
        return;
    if (res.stdout != NULL)
        unlink(res.stdout);
    if (res.stderr != NULL)
        unlink(res.stderr);
    if (res.pid != 0)
        polite_kill(res.pid);
}

int buffer_stdin(
    struct program *prog,
    struct error_buffer *errbuf) {

    char path[] = "stdin_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) {
        snprintf(errbuf->s, errbuf->n,
            "mkstemp() failed for stdin_XXXXXX, %s", strerror(errno));
        return -1;
    }
    prog->stdin = strdup(path);
    if (prog->stdin == NULL) {
        strncpy(errbuf->s, "strdup() failed", errbuf->n);
        return -1;
    }

    if (fchmod(fd, S_IRUSR) < 0) {
        snprintf(errbuf->s, errbuf->n, "fchmod() failed for %s, %s",
            prog->path, strerror(errno));
        close(fd);
        return -1;
    }

    unsigned char buffer[BUFFER_SIZE];
    for (;;) {
        ssize_t got = read(STDIN_FILENO, buffer, BUFFER_SIZE);
        if (got == 0) break;
        if (fd < 0) {
            snprintf(errbuf->s, errbuf->n,
                "read failed, %s", strerror(errno));
            close(fd);
            return -1;
        }

        ssize_t wrote = write(fd, buffer, got);
        if (wrote < got) {
            snprintf(errbuf->s, errbuf->n,
                "write failed, %s", strerror(errno));
            close(fd);
            return -1;
        }
    }
    close(fd);

    return 0;
}

char *gzip_file(const char *path, struct error_buffer *errbuf) {
    size_t l = strlen(path);
    char *gzpath = malloc(l + 4);
    strcpy(gzpath, path);
    strcpy((gzpath + l), ".gz");

    pid_t gzippid;
    int returncode;
    pid_t rpid;

    fflush(stdout);
    switch (gzippid = fork()) {
    case -1:
        snprintf(errbuf->s, errbuf->n,
            "fork() failed, %s", strerror(errno));
        return NULL;
    case 0:
        if (execlp("gzip", "gzip", path, NULL) < 0) {
            fprintf(stderr,
                "execlp(\"gzip\", \"gzip\", \"%s\") failed: %s\n",
                path, strerror(errno));
        }
        exit(0xfe);
    default:
        rpid = waitpid(gzippid, &returncode, 0);
        if (rpid == -1) {
            snprintf(errbuf->s, errbuf->n,
                "waitpid() failed, %s", strerror(errno));
            return NULL;
        }
        if (returncode != 0) {
            snprintf(errbuf->s, errbuf->n,
                "gzip exited non-zero %i", returncode);
            return NULL;
        }
    }

    return gzpath;
}

int main(unsigned int argc, const char *argv[]) {
    char _errbuf[ERRBUF_SIZE];
    struct error_buffer errbuf = {ERRBUF_SIZE-1, _errbuf};

    calledname = rindex(argv[0], '/');
    if (calledname != NULL)
        calledname++;
    else
        calledname = argv[0];

    unsigned int printusage = 0;
    unsigned int compressstdout = 0;
    unsigned int compressstderr = 0;
    int nrecords = -1;
    struct program prog = program_init();
    prog.stdout = "stdout_XXXXXX";
    prog.stderr = "stderr_XXXXXX";

    unsigned int issample = strcmp(calledname, "sample") == 0;
    if (! issample)
        nrecords = 1;

    unsigned int i;
    for (i=1; i<argc; i++)
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-h") == 0) {
                usage(0);
            } else if (strcmp(argv[i], "--help") == 0) {
                usage(1);
            } else if (strcmp(argv[i], "--usage") == 0) {
                printusage = 1;
            } else if (strcmp(argv[i], "--compress-stdout") == 0) {
                compressstdout = 1;
            } else if (strcmp(argv[i], "--compress-stderr") == 0) {
                compressstderr = 1;
            } else if (issample && strncmp(argv[i], "-n", 2) == 0) {
                if (strlen(argv[i]) > 2) {
                    nrecords = atoi(argv[i]+2);
                } else if (++i < argc) {
                    nrecords = atoi(argv[i]);
                } else {
                    fprintf(stderr, "%s: missing argument for -n\n",
                        calledname, argv[i]);
                    exit(1);
                }
            } else if (strcmp(argv[i], "--") == 0) {
                i++;
                break;
            } else {
                fprintf(stderr, "%s: unrecognized option '%s'\n",
                    calledname, argv[i]);
                exit(1);
            }
        } else
            break;

    if (i < argc && program_set_argv(&prog, argc-i, argv+i, &errbuf) != 0) {
        fprintf(stderr, "%s: invalid command, %s\n", calledname, errbuf.s);
        exit(1);
    }

    if (prog.path == NULL) {
        fprintf(stderr, "%s: missing command\n", calledname);
        exit(1);
    }

    if (isatty(STDIN_FILENO)) {
        prog.stdin = "/dev/null";
    } else if (lseek(STDIN_FILENO, 0, SEEK_CUR) < 0) {
        if (errno != ESPIPE) {
            perror("lseek");
            exit(1);
        }

        if (buffer_stdin(&prog, &errbuf) < 0) {
            fprintf(stderr, "%s: failed to buffer stdin: %s\n",
                calledname, errbuf.s);
            exit(1);
        }
    }

    if (prog.stdin != NULL) {
        int fd = open(prog.stdin, O_RDONLY);
        if (fd < 0) {
            perror("open");
            exit(1);
        }

        if (dup2(fd, STDIN_FILENO) < 0) {
            perror("dup2");
            exit(1);
        }

        if (close(fd) < 0) {
            perror("close");
            exit(1);
        }
    }

    atexit(cleanup_current_result);

    setup_signal_handlers();

    if (prog.stdin != NULL)
        printf("stdin=%s\n", prog.stdin);

    printf("prog=%s\n", prog.path);

    const char **args = prog.argv;
    i = 0;
    while (*args != NULL) {
        printf("argv[%i]=%s\n", i, *args);
        args++;
        i++;
    }

    if (printusage)
        printf("hasusage=true\n");

    puts("start end utime stime maxrss ixrss idrss isrss minflt majflt "
         "nswap inblock oublock msgsnd msgrcv nsignals nvcsw nivcsw "
         "status stdout stderr");
    fflush(stdout);

    int nrecord = 0;
    while (nrecords < 0 || nrecord++ < nrecords) {
        if (printusage) {
            // usage before running program
            memset(&res, 0, sizeof(struct program_result));
            getrusage(RUSAGE_SELF, &res.rusage);
            print_result(&res);
            putchar('\n');
            fflush(stdout);
        }

        result_sent = 0;
        // run program
        if (program_run(&prog, &res, &errbuf) == NULL) {
            fputs(errbuf.s, stderr);
            fputc('\n', stderr);
            exit(2);
        }

        if (compressstdout) {
            char *gzstdout = gzip_file(res.stdout, &errbuf);
            if (gzstdout == NULL) {
                fputs(errbuf.s, stderr);
                fputc('\n', stderr);
                exit(2);
            }
            free((char *) res.stdout);
            res.stdout = gzstdout;
        }

        if (compressstderr) {
            char *gzstderr = gzip_file(res.stderr, &errbuf);
            if (gzstderr == NULL) {
                fputs(errbuf.s, stderr);
                fputc('\n', stderr);
                exit(2);
            }
            free((char *) res.stderr);
            res.stderr = gzstderr;
        }

        print_result(&res);
        putchar('\n');
        fflush(stdout);
        result_sent = 1;
        program_result_free(&res);
    }

    // TODO: free things?

    exit(0);
}
