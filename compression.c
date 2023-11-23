#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>

#include <stdio.h>
#include <string.h>

#include "compression.h"

#define ALPHABET_SIZE (UCHAR_MAX + 1)
#define MAX_NUM_CODES 65536
#define BUF_SIZE 8192

typedef uint32_t code_t;

size_t bytes_to_fit(code_t code) {
    return code <= UCHAR_MAX ? 1 : 1 + bytes_to_fit(code >> CHAR_BIT);
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
    size_t buf_size = 0;
    struct trie_node *root = tnode_new(0);
    struct trie_node **refs = calloc(MAX_NUM_CODES, sizeof(refs[0]));
    code_t num_codes = ALPHABET_SIZE;
    unsigned int code_width = 1;
    bool need_alloc = true;

    // Initialize the dictionary to contain all strings of length one
    for (int i = 0; i < ALPHABET_SIZE; i++)
        root->children[i] = tnode_new(i);

    struct match_result m = {root, buf};
    const char *end = buf + buf_size;

    while (true) {
        m = match(root, m.next, end);

        // If match reached the end, refill buffer and continue matching if possible
        while (m.next == end) {
            if ((buf_size = read(buf, BUF_SIZE))) {
                end = buf + buf_size;
                m = match(m.node, buf, end);
            } else {
                // If buffer can't be refilled, write the match and finish
                write((const char *) &m.node->code, code_width);
                return;
            }
        }

        write((const char *) &m.node->code, code_width);

        if (num_codes < MAX_NUM_CODES) {
            // There's space, so create a new leaf node representing a dictionary entry
            if (need_alloc) {
                refs[num_codes] = tnode_new(num_codes);
            } else {
                // We don't even need to update the code, since we're getting
                // the nodes in the same order as they were created
                memset(refs[num_codes]->children, 0, sizeof(root->children));
            }

            m.node->children[(unsigned char) *m.next] = refs[num_codes];
            code_width = bytes_to_fit(num_codes);
            num_codes++;
        } else {
            // We have exausted all possible codes, so clear the dictionary
            for (int i = 0; i < ALPHABET_SIZE; i++)
                memset(root->children[i]->children, 0, sizeof(root->children));

            code_width = 1;
            num_codes = ALPHABET_SIZE;
            need_alloc = false;
            printf("Encode: Reset!\n");
        }
    }
}

struct dict_node {
    char c;
    const struct dict_node *parent;
};

struct dict_node dnode_new(char c, const struct dict_node *parent) {
    return (struct dict_node){c, parent};
}

/* Writes the string whose tail node is node. Returns the first character of that
 * string.
 */
char process_str(const struct dict_node *node, void (*write)(const char *, size_t)) {
    char ret;
    if (node->parent)
        ret = process_str(node->parent, write);
    else
        ret = node->c;
    write(&node->c, 1);
    return ret;
}

void decompress(size_t (*read)(char *, size_t), void (*write)(const char *, size_t)) {
    char *buf = malloc(BUF_SIZE);
    size_t buf_size = 0;
    struct dict_node *dict = calloc(MAX_NUM_CODES, sizeof(dict[0]));
    code_t num_codes = ALPHABET_SIZE;
    unsigned int code_width = 1;

    // Initialize the dictionary to contain all strings of length one
    for (int i = 0; i < ALPHABET_SIZE; i++)
        dict[i] = dnode_new(i, NULL);

    const struct dict_node *prev = NULL;
    char first_char = '\0';
    const char *next = buf;
    const char *end = next;

    while (true) {
        size_t diff = end - next;
        if (diff < code_width) {
            memcpy(buf, next, diff);
            // Refill buffer when we exhaust it
            buf_size = diff + read(buf + diff, BUF_SIZE - diff);
            if (buf_size == diff)
                return;
            next = buf;
            end = next + buf_size;
        }

        code_t in_code = 0;
        memcpy(&in_code, next, code_width);
        printf("Decode: %6d (", in_code);
        for (int i = 0; i < code_width; i++)
            printf("%02x ", next[i] & 0xff);
        printf(") %d\n", num_codes);
        next += code_width;

        struct dict_node *node;
        if (in_code < num_codes) {
            // The input code is in the dictionary; we can look it up
            node = dict + in_code;
            first_char = process_str(node, write);
            if (prev) {
                dict[num_codes] = dnode_new(first_char, prev);
                num_codes++;
            }
        } else {
            // The input code is not in the dictionary; we can infer its string
            node = dict + num_codes;
            *node = dnode_new(first_char, prev);
            first_char = process_str(node, write);
            num_codes++;
        }
        code_width = bytes_to_fit(num_codes);
        prev = node;

        if (num_codes == MAX_NUM_CODES) {
            // If the dictionary is full, we need to clear it. We also know that
            // the next code will be a single-character code, since the
            // compressor also cleared its dictionary.
            num_codes = ALPHABET_SIZE;
            code_width = 1;
            prev = NULL;
            printf("Decode: Reset!\n");
        }
    }
}

void dummy(size_t (*read)(char *, size_t), void (*write)(const char *, size_t)) {
    char buf[BUF_SIZE];
    size_t buf_size;

    while ((buf_size = read(buf, sizeof(buf))))
        write(buf, buf_size);
}
