#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>

#include <stdio.h>
#include <string.h>

#include "compression.h"

#define ALPHABET_SIZE (UCHAR_MAX + 1)
#define MAX_NUM_CODES (UINT16_MAX + 1)
#define BUF_SIZE 8192

typedef uint16_t code_t;

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
    code_t next_code = ALPHABET_SIZE;
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
                write((const char *) &m.node->code, sizeof(code_t));
                return;
            }
        }

        write((const char *) &m.node->code, sizeof(code_t));

        if (next_code > 0) {
            // There's space, so create a new leaf node representing a dictionary entry
            if (need_alloc) {
                refs[next_code] = tnode_new(next_code);
            } else {
                // We don't even need to update the code, since we're getting
                // the nodes in the same order as they were created
                memset(refs[next_code]->children, 0, sizeof(root->children));
            }

            m.node->children[(unsigned char) *m.next] = refs[next_code];
            next_code++;
        } else {
            // We have exausted all possible codes, so clear the dictionary
            for (int i = 0; i < ALPHABET_SIZE; i++)
                memset(root->children[i]->children, 0, sizeof(root->children));

            next_code = ALPHABET_SIZE;
            need_alloc = false;
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

    // Initialize the dictionary to contain all strings of length one
    for (int i = 0; i < ALPHABET_SIZE; i++)
        dict[i] = dnode_new(i, NULL);

    const struct dict_node *prev = NULL;
    char first_char = '\0';
    const code_t *next = (const code_t *) buf;
    const code_t *end = next;

    while (true) {
        if (next == end) {
            // Refill buffer when we exhaust it
            buf_size = read(buf, BUF_SIZE);
            if (!buf_size)
                return;
            next = (const code_t *) buf;
            end = next + buf_size / sizeof(code_t);
        }

        if (*next < num_codes) {
            // The next code is in the dictionary; we can look it up
            const struct dict_node *node = dict + *next;
            first_char = process_str(node, write);
            if (prev) {
                dict[num_codes] = dnode_new(first_char, prev);
                num_codes++;
            }
            prev = node;
        } else if (num_codes > 0) {
            // The next code is not in the dictionary; we can infer its string
            struct dict_node *new_node = dict + num_codes;
            *new_node = dnode_new(first_char, prev);
            first_char = process_str(new_node, write);
            prev = new_node;
            num_codes++;
        } else {
            // If the dictionary is full, we need to clear it. We also know that
            // the next code will be a single-character code, since the
            // compressor also cleared its dictionary.
            struct dict_node *node = dict + *next;
            write(&node->c, 1);
            prev = node;
            num_codes = ALPHABET_SIZE;
        }

        next++;
    }
}

void dummy(size_t (*read)(char *, size_t), void (*write)(const char *, size_t)) {
    char buf[BUF_SIZE];
    size_t buf_size;

    while ((buf_size = read(buf, sizeof(buf))))
        write(buf, buf_size);
}
