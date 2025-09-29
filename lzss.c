// lzss.c — 7th-Guest–style LZSS (fixed format), C23, binary-safe, GREEDY & CORRECT
// Tokens per flag byte (LSB-first): 1 = literal (1 byte), 0 = pair (2 bytes).
// Pair layout: ofs_len = ((distance - 1) << 4) | (length - 3), where distance
// is the backward match distance in bytes (1..4096). Fixed spec: LENGTH_BITS=4
// → N=4096, F=16, THR=3. History start at N-F.

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

enum
{
    LENGTH_BITS = 4,
    LENGTH_MASK = (1u << LENGTH_BITS) - 1u, // 0x0F
    N = 1 << (16 - LENGTH_BITS),            // 4096
    F = 1 << LENGTH_BITS,                   // 16
    THR = 3,                                // actual_len = stored + THR
    N_MASK = N - 1
};

static FILE *xfopen(const char *path, const char *mode)
{
    FILE *f = fopen(path, mode);
    if (!f)
    {
        fprintf(stderr, "open %s: %s\n", path, strerror(errno));
        exit(1);
    }
    return f;
}

// Find best match strictly in HISTORY (no look-ahead sources).
// r   = start of look-ahead
// s   = bytes valid in look-ahead
// hsz = bytes available in history (0..N)
static inline void find_best_match_hist_greedy(
    const uint8_t *ring,
    uint32_t r,
    int s,
    int hsz,
    int *out_dist,
    int *out_len)
{
    int max_len = s;
    int best_len = 0, best_dist = 0;
    const int max_match = F + THR - 1; // 18
    if (max_len > max_match)
        max_len = max_match;

    // Scan distances 1..hsz (history only)
    for (int dist = 1; dist <= hsz; ++dist)
    {
        uint32_t p = (r - (uint32_t)dist) & N_MASK;
        int L = 0;
        while (L < max_len)
        {
            if (ring[(r + (uint32_t)L) & N_MASK] != ring[(p + (uint32_t)L) & N_MASK])
                break;
            ++L;
        }
        if (L > best_len)
        {
            best_len = L;
            best_dist = dist;
            if (L == max_len)
                break;
        }
    }
    if (best_len > s)
        best_len = s; // EOF safety
    *out_dist = best_dist;
    *out_len = best_len;
}

static size_t encode(FILE *in, FILE *out)
{
    uint8_t *ring = (uint8_t *)malloc(N);
    if (!ring)
    {
        fprintf(stderr, "oom\n");
        exit(1);
    }
    memset(ring, 0x00, N); // zeroed history (classic 7G-compatible)

    uint32_t rpos = (uint32_t)(N - F); // start of look-ahead
    int s = 0;                         // look-ahead size
    int hsz = 0;                       // bytes in history (0..N)

    // Prime look-ahead ONLY
    while (s < F)
    {
        int ch = fgetc(in);
        if (ch == EOF)
            break;
        ring[(rpos + (uint32_t)s) & N_MASK] = (uint8_t)ch;
        ++s;
    }
    if (s == 0)
    {
        free(ring);
        return 0;
    }

    uint8_t block[1 + 2 * 8];
    uint8_t flags = 0, mask = 1;
    int bidx = 1;
    block[0] = 0;
    size_t produced = 0;

    while (s > 0)
    {
        int dist = 0, mlen = 0;
        find_best_match_hist_greedy(ring, rpos, s, hsz, &dist, &mlen);

        // Pure greedy: encode match if it beats literal threshold, else literal
        if (mlen > THR)
        {
            int len_field = mlen - THR; // 0..15
            if (dist <= 0)
                dist = 1;                               // safety
            uint32_t dist_field = (uint32_t)(dist - 1); // store as 0..4095
            uint16_t ofs_len = (uint16_t)(((dist_field & N_MASK) << LENGTH_BITS) | (uint32_t)(len_field & LENGTH_MASK));
            block[bidx++] = (uint8_t)(ofs_len & 0xFF);
            block[bidx++] = (uint8_t)(ofs_len >> 8);
        }
        else
        {
            // literal
            flags |= mask;
            block[bidx++] = ring[rpos];
            mlen = 1;
        }

        for (int i = 0; i < mlen; ++i)
        {
            if (hsz < N)
                ++hsz;

            int ch = fgetc(in);
            if (ch != EOF)
            {
                // Append at tail: rpos + s (current s)
                ring[(rpos + (uint32_t)s) & N_MASK] = (uint8_t)ch;
                // s remains the same: we are replacing the consumed byte
            }
            else
            {
                // No new byte → lookahead shrinks
                if (s > 0)
                    --s;
            }

            rpos = (rpos + 1u) & N_MASK;

            if (ch == EOF && s == 0)
                break;
        }

        // Flush every 8 tokens
        mask <<= 1;
        if (mask == 0)
        {
            block[0] = flags;
            fwrite(block, 1, (size_t)bidx, out);
            produced += (size_t)bidx;
            flags = 0;
            mask = 1;
            bidx = 1;
            block[0] = 0;
        }
    }

    // Flush remainder
    if (mask != 1)
    {
        block[0] = flags;
        fwrite(block, 1, (size_t)bidx, out);
        produced += (size_t)bidx;
    }

    free(ring);
    return produced;
}

static size_t decode(FILE *in, FILE *out)
{
    uint8_t *ring = (uint8_t *)malloc(N);
    if (!ring)
    {
        fprintf(stderr, "oom\n");
        exit(1);
    }
    memset(ring, 0x00, N); // zeroed history

    uint32_t rpos = (uint32_t)(N - F);
    size_t produced = 0;

    for (;;)
    {
        int fb = fgetc(in);
        if (fb == EOF)
            break;
        uint8_t flags = (uint8_t)fb;

        for (int i = 0; i < 8; ++i, flags >>= 1)
        {
            if (flags & 1)
            {
                int ch = fgetc(in);
                if (ch == EOF)
                    goto done;
                fputc(ch, out);
                ring[rpos] = (uint8_t)ch;
                rpos = (rpos + 1u) & N_MASK;
                ++produced;
            }
            else
            {
                int b0 = fgetc(in), b1 = fgetc(in);
                if (b0 == EOF || b1 == EOF)
                    goto done;
                uint16_t ofs_len = (uint16_t)(b0 | (b1 << 8));
                uint32_t dist_field = (uint32_t)(ofs_len >> LENGTH_BITS);
                uint32_t distance = dist_field + 1u;                    // stored value was distance - 1
                uint32_t len = (uint32_t)(ofs_len & LENGTH_MASK) + THR; // 3..18
                uint32_t offset = (rpos - distance) & N_MASK;

                for (uint32_t j = 0; j < len; ++j)
                {
                    uint8_t v = ring[(offset + j) & N_MASK];
                    fputc(v, out);
                    ring[rpos] = v;
                    rpos = (rpos + 1u) & N_MASK;
                    ++produced;
                }
            }
        }
    }
done:
    free(ring);
    return produced;
}

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        fprintf(stderr, "Usage:\n  %s e input output\n  %s d input output\n", argv[0], argv[0]);
        return 1;
    }
    FILE *in = xfopen(argv[2], "rb");
    FILE *out = xfopen(argv[3], "wb");
    size_t n = 0;
    if (argv[1][0] == 'e')
        n = encode(in, out);
    else if (argv[1][0] == 'd')
        n = decode(in, out);
    else
    {
        fprintf(stderr, "mode must be e or d\n");
        fclose(in);
        fclose(out);
        return 1;
    }
    fclose(in);
    fclose(out);
    return (n > 0) ? 0 : 2;
}
