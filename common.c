#include <stdarg.h>

#include "common.h"

bool is_final(const struct packet *p) {
    return p->flags & FLAG_FINAL;
}

const char *format(const char *fmt, ...) {
#ifdef DEBUG
    static char buf[512];
    va_list args;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    return buf;
#endif
}
