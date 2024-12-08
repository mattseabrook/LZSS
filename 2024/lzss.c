// lzss.c

/*

    Author: Matt Seabrook
    Date: 2024-12-08
    Email: info@mattseabrook.net

    Description: This program is a simple refactoring of the LZSS compression algorithm.

*/

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

// LZSS Parameters
#define RING_BUFFER_SIZE 4096
#define MATCH_MAX_LEN 18
#define MATCH_THRESHOLD 2
#define NODE_UNUSED RING_BUFFER_SIZE

// Binary tree node for compression
typedef struct
{
    int32_t left, right, parent;
} TreeNode;

// Secure file opening with error handling
static FILE *safe_fopen(const char *filename, const char *mode)
{
    FILE *file = fopen(filename, mode);
    if (!file)
    {
        fprintf(stderr, "Error opening file %s: %s\n", filename, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return file;
}

// Initialize binary search tree
static void initialize_tree(TreeNode *tree)
{
    for (int32_t i = RING_BUFFER_SIZE + 1; i <= RING_BUFFER_SIZE + 256; i++)
    {
        tree[i].right = NODE_UNUSED;
    }
    for (int32_t i = 0; i < RING_BUFFER_SIZE; i++)
    {
        tree[i].parent = NODE_UNUSED;
    }
}

// Insert node into binary search tree
static void insert_node(TreeNode *tree, uint8_t *buffer, int32_t pos,
                        int32_t *match_pos, int32_t *match_len)
{
    int32_t current = RING_BUFFER_SIZE + 1 + buffer[pos];
    int32_t comparison, depth = 0;

    *match_len = 0;
    *match_pos = current;

    while (depth < MATCH_MAX_LEN)
    {
        comparison = buffer[pos + depth] - buffer[current + depth];

        if (comparison == 0)
        {
            depth++;
            continue;
        }

        if (comparison > 0)
        {
            if (tree[current].right == NODE_UNUSED)
            {
                tree[current].right = pos;
                tree[pos].parent = current;
                return;
            }
            current = tree[current].right;
        }
        else
        {
            if (tree[current].left == NODE_UNUSED)
            {
                tree[current].left = pos;
                tree[pos].parent = current;
                return;
            }
            current = tree[current].left;
        }

        if (depth > *match_len)
        {
            *match_len = depth;
            *match_pos = current;
        }
    }
}

// Encode input file
static size_t encode(FILE *input, FILE *output)
{
    TreeNode *tree = calloc(RING_BUFFER_SIZE + 257, sizeof(TreeNode));
    uint8_t *ring_buffer = malloc(RING_BUFFER_SIZE + MATCH_MAX_LEN);
    uint8_t *code_buffer = malloc(17);

    if (!tree || !ring_buffer || !code_buffer)
    {
        free(tree);
        free(ring_buffer);
        free(code_buffer);
        fprintf(stderr, "Memory allocation failed\n");
        return 0;
    }

    // Initialize
    initialize_tree(tree);
    memset(ring_buffer, ' ', RING_BUFFER_SIZE);

    size_t bytes_read = fread(ring_buffer + RING_BUFFER_SIZE, 1, MATCH_MAX_LEN, input);
    if (bytes_read == 0)
    {
        free(tree);
        free(ring_buffer);
        free(code_buffer);
        return 0;
    }

    size_t encoded_size = 0;
    uint32_t code_mask = 1;
    size_t code_index = 1;
    code_buffer[0] = 0;
    size_t read_pos = RING_BUFFER_SIZE;

    for (size_t i = 1; i <= MATCH_MAX_LEN; i++)
    {
        insert_node(tree, ring_buffer, read_pos - i, &(int32_t){0}, &(int32_t){0});
    }

    insert_node(tree, ring_buffer, read_pos, &(int32_t){0}, &(int32_t){0});

    while (bytes_read > 0)
    {
        int32_t match_pos, match_len;
        insert_node(tree, ring_buffer, read_pos, &match_pos, &match_len);

        if (match_len <= MATCH_THRESHOLD)
        {
            code_buffer[0] |= code_mask;
            code_buffer[code_index++] = ring_buffer[read_pos];
            match_len = 1;
        }
        else
        {
            code_buffer[code_index++] = match_pos;
            code_buffer[code_index++] = ((match_pos >> 4) & 0xF0) | (match_len - (MATCH_THRESHOLD + 1));
        }

        code_mask <<= 1;
        if (code_mask == 0)
        {
            fwrite(code_buffer, 1, code_index, output);
            encoded_size += code_index;
            code_buffer[0] = 0;
            code_index = 1;
            code_mask = 1;
        }

        // Rewrite processing loop with more explicit EOF handling
        bool should_continue = false;
        for (int i = 0; i < match_len; i++)
        {
            int c = fgetc(input);
            if (c == EOF)
            {
                bytes_read = 0;
                break;
            }

            ring_buffer[read_pos] = c;
            read_pos = (read_pos + 1) % RING_BUFFER_SIZE;
            insert_node(tree, ring_buffer, read_pos, &(int32_t){0}, &(int32_t){0});

            // Check if we still have bytes to read
            if (bytes_read > 0)
            {
                should_continue = true;
            }
        }

        // If no more bytes to read, break out of the main loop
        if (!should_continue)
        {
            break;
        }
    }

    // Flush remaining code buffer
    if (code_index > 1)
    {
        fwrite(code_buffer, 1, code_index, output);
        encoded_size += code_index;
    }

    // Cleanup
    free(tree);
    free(ring_buffer);
    free(code_buffer);

    return encoded_size;
}

// Decode compressed file
static size_t decode(FILE *input, FILE *output)
{
    uint8_t *ring_buffer = malloc(RING_BUFFER_SIZE + MATCH_MAX_LEN);
    size_t decoded_size = 0;
    uint8_t flags = 0;
    int32_t read_pos = RING_BUFFER_SIZE - MATCH_MAX_LEN;

    if (!ring_buffer)
    {
        fprintf(stderr, "Memory allocation failed\n");
        return 0;
    }

    // Initialize buffer
    memset(ring_buffer, ' ', read_pos);

    while (1)
    {
        if ((flags >>= 1) == 0)
        {
            int c = fgetc(input);
            if (c == EOF)
                break;
            flags = c | 0xFF00;
        }

        if (flags & 1)
        {
            int c = fgetc(input);
            if (c == EOF)
                break;

            fputc(c, output);
            ring_buffer[read_pos] = c;
            read_pos = (read_pos + 1) % RING_BUFFER_SIZE;
            decoded_size++;
        }
        else
        {
            int pos = fgetc(input);
            int len = fgetc(input);
            if (pos == EOF || len == EOF)
                break;

            pos |= (len & 0xF0) << 4;
            len = (len & 0x0F) + MATCH_THRESHOLD;

            for (int i = 0; i <= len; i++)
            {
                uint8_t c = ring_buffer[(pos + i) % RING_BUFFER_SIZE];
                fputc(c, output);
                ring_buffer[read_pos] = c;
                read_pos = (read_pos + 1) % RING_BUFFER_SIZE;
                decoded_size++;
            }
        }
    }

    free(ring_buffer);
    return decoded_size;
}

int main(int argc, char *argv[])
{
    // Validate arguments
    if (argc != 4)
    {
        fprintf(stderr, "Usage:\n  %s e input_file output_file\n  %s d input_file output_file\n",
                argv[0], argv[0]);
        return EXIT_FAILURE;
    }

    // Open files
    FILE *input_file = safe_fopen(argv[2], "rb");
    FILE *output_file = safe_fopen(argv[3], "wb");

    // Process based on mode
    size_t processed_size = 0;
    switch (argv[1][0])
    {
    case 'e':
        processed_size = encode(input_file, output_file);
        break;
    case 'd':
        processed_size = decode(input_file, output_file);
        break;
    default:
        fprintf(stderr, "Invalid mode: %c\n", argv[1][0]);
        fclose(input_file);
        fclose(output_file);
        return EXIT_FAILURE;
    }

    // Cleanup
    fclose(input_file);
    fclose(output_file);

    return processed_size > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}