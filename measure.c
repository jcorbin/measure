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

    printf(" \"%s\" \"%s\"", res->stdout, res->stderr);
}

#define ERRBUF_SIZE 4096

int main(unsigned int argc, const char *argv[]) {
    char _errbuf[ERRBUF_SIZE];
    struct error_buffer errbuf = {ERRBUF_SIZE, _errbuf};

    // TODO: support options for things like cwd

    struct program prog = program_init();
    prog.stdin  = NULL; // TODO: support "-";
    prog.stdout = "stdout_XXXXXX";
    prog.stderr = "stderr_XXXXXX";

    if (argc < 2) {
        fprintf(stderr, "missing program argument\n");
        exit(1);
    }

    if (program_set_argv(&prog, argc-1, argv+1, &errbuf) != 0) {
        fprintf(stderr, "invalid program: %s\n", errbuf.s);
        exit(1);
    }

    puts("start end utime stime maxrss ixrss idrss isrss minflt majflt "
         "nswap inblock oublock msgsnd msgrcv nsignals nvcsw nivcsw");

    struct program_result res = program_result_init();

    // baseline
    getrusage(RUSAGE_SELF, &res.rusage);
    print_result(&res);
    putchar('\n');

    if (prog.cwd != NULL)
        if (chdir(prog.cwd) != 0) {
            fprintf(stderr, "failed to chdir to %s: %s",
                prog.cwd, strerror(errno));
            exit(2);
        }

    // run program
    while (1) {
        if (program_run(&prog, &res, &errbuf) == NULL) {
            fputs(errbuf.s, stderr);
            fputc('\n', stderr);
            exit(2);
        }
        print_result(&res);
        putchar('\n');
        program_result_free(&res);
    }

    // TODO: free things?

    exit(0);
}
