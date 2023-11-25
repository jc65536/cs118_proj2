#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>

#include <stdio.h>
#include <string.h>

#include "compression.h"

#define ALPHABET_SIZE (UCHAR_MAX + 1)
#define GIVE_UP_CODE ALPHABET_SIZE
#define FIRST_DICT_CODE (ALPHABET_SIZE + 1)
#define MAX_NUM_CODES 1000000
#define MIN_CODE_WIDTH (CHAR_BIT + 1)
#define BUF_SIZE 8192
#define RAND_RATIO 1.35

typedef uint32_t code_t;

// Assumes little endian!
struct bitbuf {
    uint64_t buf; // High bits are end of queue, low bits are front of queue
    unsigned num_bits;
};

unsigned write_wrapper(void (*write)(const char *, size_t), struct bitbuf *b,
                       code_t c, unsigned w) {
    if (b->num_bits < 64)
        b->buf |= (uint64_t) c << b->num_bits;

    if (b->num_bits + w < 64) {
        b->num_bits += w;
        return 0;
    } else {
        write((char *) &b->buf, sizeof(b->buf));
        unsigned extra_bits = b->num_bits + w - 64;
        b->buf = c >> (w - extra_bits);
        b->num_bits = extra_bits;
        return sizeof(b->buf);
    }
}

void flush_wrapper(void (*write)(const char *, size_t), struct bitbuf *b) {
    write((char *) &b->buf, (b->num_bits - 1) / CHAR_BIT + 1);
    b->num_bits = 0;
}

struct read_result {
    code_t code;
    bool done;
};

struct read_result read_wrapper(size_t (*read)(char *, size_t), struct bitbuf *b, unsigned w) {
    code_t c = b->buf;

    if (w < b->num_bits) {
        b->buf >>= w;
        b->num_bits -= w;
    } else {
        unsigned missing_bits = w - b->num_bits;
        b->num_bits = read((char *) &b->buf, sizeof(b->buf)) * CHAR_BIT;

        if (b->num_bits < missing_bits)
            return (struct read_result){'\0', true};

        c |= b->buf << (w - missing_bits);
        b->buf >>= missing_bits;
        b->num_bits -= missing_bits;
    }

    c &= ~(~(code_t) 0 << w);
    return (struct read_result){c, false};
}

struct trie_node {
    code_t code;
    struct trie_node *children[ALPHABET_SIZE];
};

struct trie_node *tnode_new(code_t code) {
    struct trie_node *node = calloc(1, sizeof(struct trie_node));
    node->code = code;
    return node;
}

void tnode_free(struct trie_node *t) {
    if (!t)
        return;

    for (int i = 0; i < ALPHABET_SIZE; i++)
        tnode_free(t->children[i]);

    free(t);
}

struct match_result {
    struct trie_node *node;
    const char *next;
};

struct match_result match(struct trie_node *node, const char *next, const char *end) {
    struct trie_node *next_node;
    if (next != end && (next_node = node->children[(unsigned char) *next]))
        return match(next_node, next + 1, end);
    else
        return (struct match_result){node, next};
}

void compress(size_t (*read)(char *, size_t), void (*write)(const char *, size_t)) {
    char *buf = malloc(BUF_SIZE);
    size_t buf_size = read(buf, BUF_SIZE);
    struct trie_node *root = tnode_new(0);
    struct trie_node **refs = calloc(MAX_NUM_CODES, sizeof(refs[0]));
    code_t next_code = FIRST_DICT_CODE;
    unsigned code_width = MIN_CODE_WIDTH;
    bool need_alloc = true;
    struct bitbuf bitbuf = {};

    // Initialize the dictionary to contain all strings of length one
    for (int i = 0; i < ALPHABET_SIZE; i++)
        root->children[i] = tnode_new(i);

    struct match_result m = {root, buf};
    const char *end = buf + buf_size;

    size_t comp_size = 0;
    size_t in_size = buf_size;

    while (true) {
        m = match(root, m.next, end);

        // If match reached the end, refill buffer and continue matching if possible
        while (m.next == end) {
            if (comp_size > in_size * RAND_RATIO) {
                write_wrapper(write, &bitbuf, m.node->code, code_width);

                // Correctly set code_width for give-up code
                if (next_code >= MAX_NUM_CODES)
                    code_width = MIN_CODE_WIDTH;
                else if (next_code >> code_width)
                    code_width++;

                write_wrapper(write, &bitbuf, GIVE_UP_CODE, code_width);
                // Fill bitbuf so uncompressed text isn't read into the
                // decompression bitbuf
                bitbuf.num_bits = 64;
                flush_wrapper(write, &bitbuf);

                copy(read, write);
                goto cleanup;
            }

            if ((buf_size = read(buf, BUF_SIZE))) {
                in_size += buf_size;
                end = buf + buf_size;
                m = match(m.node, buf, end);
            } else {
                // If buffer can't be refilled, write the match and finish
                write_wrapper(write, &bitbuf, m.node->code, code_width);
                flush_wrapper(write, &bitbuf);
                goto cleanup;
            }
        }

        comp_size += write_wrapper(write, &bitbuf, m.node->code, code_width);

        if (next_code < MAX_NUM_CODES) {
            // There's space, so create a new leaf node representing a dictionary entry
            if (need_alloc) {
                refs[next_code] = tnode_new(next_code);
            } else {
                // We don't even need to update the code, since we're getting
                // the nodes in the same order as they were created
                memset(refs[next_code]->children, 0, sizeof(root->children));
            }

            m.node->children[(unsigned char) *m.next] = refs[next_code];

            if (next_code >> code_width)
                code_width++;

            next_code++;
        } else {
            // We have exausted all possible codes, so clear the dictionary
            for (int i = 0; i < ALPHABET_SIZE; i++)
                memset(root->children[i]->children, 0, sizeof(root->children));

            code_width = MIN_CODE_WIDTH;
            next_code = FIRST_DICT_CODE;
            need_alloc = false;
        }
    }

cleanup:
    free(buf);
    free(root);
    free(refs);
}

struct dict_node {
    struct dict_node *parent;
    struct dict_node *jump;
    unsigned offset;
    char buf[60];
};

struct dict_node *dnode_init(struct dict_node *node, char c, struct dict_node *parent) {
    node->parent = parent;
    node->buf[sizeof(node->buf) - 1] = c;

    unsigned offset = sizeof(node->buf) - 1;
    struct dict_node *jump = parent;
    while (offset > 0 && jump) {
        offset--;
        node->buf[offset] = jump->buf[sizeof(jump->buf) - 1];
        jump = jump->parent;
    }

    node->offset = offset;
    node->jump = jump;

    return node;
}

/* Writes the string whose tail node is node. Returns the first character of that
 * string.
 */
char process_str(struct dict_node *node, void (*write)(const char *, size_t)) {
    char ret;
    if (node->jump) {
        ret = process_str(node->jump, write);
    } else {
        ret = node->buf[node->offset];
    }
    write(node->buf + node->offset, sizeof(node->buf) - node->offset);
    return ret;
}

void decompress(size_t (*read)(char *, size_t), void (*write)(const char *, size_t)) {
    struct dict_node **dict = calloc(MAX_NUM_CODES, sizeof(dict[0]));
    code_t next_code = FIRST_DICT_CODE;
    unsigned code_width = MIN_CODE_WIDTH;
    struct bitbuf bitbuf = {};

    // Initialize the dictionary to contain all strings of length one
    for (int i = 0; i < ALPHABET_SIZE; i++)
        dict[i] = dnode_init(malloc(sizeof(struct dict_node)), i, NULL);

    struct dict_node *prev = NULL;
    char first_char = '\0';
    bool need_alloc = true;

    while (true) {
        struct read_result result = read_wrapper(read, &bitbuf, code_width);

        if (result.done)
            break;

        if (result.code == GIVE_UP_CODE) {
            copy(read, write);
            break;
        }

        /* Wab - Match W - Emit w - Add Wa (full)
         * Xb  - Match X - Emit x - Clear  (X = aY)
         * b   - Match b - Emit b
         *
         * Read w - Emit W  - Add something
         * Read x - Emit Xb - Add Wa (full) - Clear
         * Read b - Emit b
         */

        struct dict_node *node;
        if (result.code < next_code) {
            // The input code is in the dictionary; we can look it up
            node = dict[result.code];
            first_char = process_str(node, write);
            if (prev) {
                if (need_alloc)
                    dict[next_code] = malloc(sizeof(struct dict_node));

                dnode_init(dict[next_code], first_char, prev);
                next_code++;
            }
        } else {
            // The input code is not in the dictionary; we can infer it
            if (need_alloc)
                dict[next_code] = malloc(sizeof(struct dict_node));

            node = dict[next_code];
            dnode_init(node, first_char, prev);
            first_char = process_str(node, write);
            next_code++;
        }
        prev = node;

        if (next_code == MAX_NUM_CODES) {
            // If the dictionary is full, we need to clear it. We also know that
            // the next code will be a single-character code, since the
            // compressor also cleared its dictionary.
            next_code = FIRST_DICT_CODE;
            code_width = MIN_CODE_WIDTH;
            prev = NULL;
            need_alloc = false;
        } else if (next_code >> code_width) {
            code_width++;
        }
    }

    for (int i = 0; i < MAX_NUM_CODES; i++)
        free(dict[i]);

    free(dict);
}

void copy(size_t (*read)(char *, size_t), void (*write)(const char *, size_t)) {
    char buf[BUF_SIZE];
    size_t buf_size;

    while ((buf_size = read(buf, sizeof(buf))))
        write(buf, buf_size);
}
