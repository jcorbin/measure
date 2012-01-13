#ifndef _ERROR_H
#define _ERROR_H

// Simplistic error handling
// TODO:
// * is there a better standardized option?
// * convenience functions/macros?

struct error_buffer {
    size_t n;
    char *s;
};

#endif // _ERROR_H
