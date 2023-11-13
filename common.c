#include "common.h"

const int HEADER_SIZE = offsetof(struct packet, payload);

bool is_last(const struct packet *p) {
    return p->flags & FLAG_FINAL;
}
