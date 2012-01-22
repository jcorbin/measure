#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "error.h"
#include "program.h"

// TODO:
// * variable arguments per execution for, e.g., output filenames as arguments
// * move to a different output format rather than introducing escaping for stdout/err

void print_result(struct program_result *res) {
    printf("%us,%uns", res->start.tv_sec, res->start.tv_nsec);
    putchar(' ');

    printf("%us,%uns", res->end.tv_sec, res->end.tv_nsec);
    putchar(' ');

    struct rusage *r = &res->rusage;

    printf("%us,%ums", r->ru_utime.tv_sec, r->ru_utime.tv_usec);
    putchar(' ');

    printf("%us,%ums", r->ru_stime.tv_sec, r->ru_stime.tv_usec);
    putchar(' ');

    printf("%u %u %u %u %u %u %u %u %u %u %u %u %u %u",
        r->ru_maxrss, r->ru_ixrss,
        r->ru_idrss, r->ru_isrss,
        r->ru_minflt, r->ru_majflt, r->ru_nswap,
        r->ru_inblock, r->ru_oublock,
        r->ru_msgsnd, r->ru_msgrcv, r->ru_nsignals,
        r->ru_nvcsw, r->ru_nivcsw);

    printf(" %s %s", res->stdout, res->stderr);
}

#define ERRBUF_SIZE 4096

static const char *calledname = NULL;

void usage(void) {
    fprintf(stderr,
        "Usage: %s [options] [--] command [command arguments]\n"
        "Run a command %s and collect various measurements.\n"
        "\n"
        "  -h, --help  Show this usage screen.\n"
        "  --usage     Print resource usage of the measuring process before\n"
        "              each command run; the default is to print only the\n"
        "              initial baseline.\n"
        "\n"
        "A header row is output first, followed by a baseline of the measuring\n"
        "process's resource usage. If --usage is specified, this usage is taken\n"
        "and output before every command result row.\n\n"
        "The output fields are:\n"
        "- start and end time from the monotonic clock (see clock_gettime(3))\n"
        "- process exit status and resource usage (see wait4(2))\n"
        "- temporary files containing stdout and stderr output\n",
        calledname,
        strcmp(calledname, "sample") == 0 ? "repeatedly" : "once");
    exit(0);
}

int main(unsigned int argc, const char *argv[]) {
    char _errbuf[ERRBUF_SIZE];
    struct error_buffer errbuf = {ERRBUF_SIZE-1, _errbuf};

    calledname = rindex(argv[0], '/');
    if (calledname != NULL)
        calledname++;
    else
        calledname = argv[0];

    unsigned int printusage = 1;
    struct program prog = program_init();
    prog.stdin  = NULL; // TODO: support "-";
    prog.stdout = "stdout_XXXXXX";
    prog.stderr = "stderr_XXXXXX";

    unsigned int i;
    for (i=1; i<argc; i++)
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-h") == 0 ||
                strcmp(argv[i], "--help") == 0) {
                usage();
            } else if (strcmp(argv[i], "--usage") == 0) {
                printusage = -1;
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

    puts("start end utime stime maxrss ixrss idrss isrss minflt majflt "
         "nswap inblock oublock msgsnd msgrcv nsignals nvcsw nivcsw "
         "stdout stderr");
    fflush(stdout);

    struct program_result res = program_result_init();

    // TODO: handle SIGPIPE and unlink output files which weren't consumed

    unsigned int issample = strcmp(calledname, "sample") == 0;

    while (1) {
        if (printusage) {
            // usage before running program
            memset(&res, 0, sizeof(struct program_result));
            getrusage(RUSAGE_SELF, &res.rusage);
            print_result(&res);
            putchar('\n');
            fflush(stdout);
            if (printusage == 1)
                printusage = 0;
        }

        // run program
        if (program_run(&prog, &res, &errbuf) == NULL) {
            fputs(errbuf.s, stderr);
            fputc('\n', stderr);
            exit(2);
        }
        print_result(&res);
        putchar('\n');
        fflush(stdout);
        program_result_free(&res);

        if (! issample)
            break;
    }

    // TODO: free things?

    exit(0);
}
