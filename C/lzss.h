/* Public domain. Matt Seabrook's refactor of Haruhiko Okumura's 1989 LZSS.
   12-bit ring offsets, 4-bit lengths, and least-significant flag bit first. */
#ifndef LZSS_H
#define LZSS_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef LZSS_CURSOR_BITS
#define LZSS_CURSOR_BITS 64
#endif
#if LZSS_CURSOR_BITS == 32
typedef uint32_t lzss_cursor;
#define LZSS_CURSOR_MAX UINT32_MAX
#elif LZSS_CURSOR_BITS == 64
typedef uint64_t lzss_cursor;
#define LZSS_CURSOR_MAX UINT64_MAX
#else
#error LZSS_CURSOR_BITS must be 32 or 64
#endif

typedef struct {
  const uint8_t *data;
  lzss_cursor size;
} lzss_input;

enum { LZSS_WINDOW = 4096, LZSS_MATCH = 18, LZSS_THRESHOLD = 2, LZSS_NIL = LZSS_WINDOW };
static_assert(LZSS_WINDOW == (1 << 12) && LZSS_MATCH - LZSS_THRESHOLD == (1 << 4));

typedef struct {
  uint8_t *data;
  size_t size, capacity;
} lzss_buffer;

static inline bool lzss_reserve(lzss_buffer *out, size_t extra) {
  if (extra <= out->capacity - out->size) return true;
  if (extra > SIZE_MAX - out->size) return false;
  size_t need = out->size + extra, half = out->capacity / 2;
  size_t capacity = out->capacity > SIZE_MAX - half ? SIZE_MAX : out->capacity + half;
  if (capacity < 256) capacity = 256;
  if (capacity < need) capacity = need;
  uint8_t *data = realloc(out->data, capacity);
  if (!data) return false;
  out->data = data;
  out->capacity = capacity;
  return true;
}

typedef struct {
  uint8_t ring[LZSS_WINDOW + LZSS_MATCH - 1];
  uint16_t left[LZSS_WINDOW + 1], right[LZSS_WINDOW + 257], parent[LZSS_WINDOW + 1];
  unsigned match, length;
} lzss_tree;

static inline uint16_t *lzss_parent_link(lzss_tree *t, unsigned node) {
  unsigned parent = t->parent[node];
  return t->right[parent] == node ? &t->right[parent] : &t->left[parent];
}

static inline void lzss_insert(lzss_tree *t, unsigned node) {
  const uint8_t *key = t->ring + node;
  unsigned p = LZSS_WINDOW + 1 + key[0];
  t->left[node] = t->right[node] = LZSS_NIL;
  t->length = 0;
  int cmp = 1;
  for (;;) {
    uint16_t *link = cmp >= 0 ? &t->right[p] : &t->left[p];
    if (*link == LZSS_NIL) {
      *link = node;
      t->parent[node] = p;
      return;
    }
    p = *link;
    unsigned n = 1;
    while (n < LZSS_MATCH && key[n] == t->ring[p + n]) ++n;
    if (n > t->length) {
      t->match = p;
      t->length = n;
      if (n == LZSS_MATCH) break;
    }
    cmp = key[n] - t->ring[p + n];
  }
  *lzss_parent_link(t, p) = node;
  t->parent[node] = t->parent[p];
  t->left[node] = t->left[p];
  t->right[node] = t->right[p];
  t->parent[t->left[p]] = t->parent[t->right[p]] = node;
  t->parent[p] = LZSS_NIL;
}

static inline void lzss_erase(lzss_tree *t, unsigned p) {
  if (t->parent[p] == LZSS_NIL) return;
  unsigned q = t->left[p];
  if (q == LZSS_NIL) q = t->right[p];
  else if (t->right[p] != LZSS_NIL) {
    while (t->right[q] != LZSS_NIL) q = t->right[q];
    if (q != t->left[p]) {
      t->right[t->parent[q]] = t->left[q];
      t->parent[t->left[q]] = t->parent[q];
      t->left[q] = t->left[p];
      t->parent[t->left[q]] = q;
    }
    t->right[q] = t->right[p];
    t->parent[t->right[q]] = q;
  }
  *lzss_parent_link(t, p) = q;
  t->parent[q] = t->parent[p];
  t->parent[p] = LZSS_NIL;
}

static inline bool lzss_encode_impl(lzss_input in, lzss_buffer *out,
                                   void (*report)(size_t, size_t)) {
  if (!in.size) return true;
  lzss_tree tree;
  memset(tree.ring, ' ', LZSS_WINDOW - LZSS_MATCH);
  memset(tree.ring + LZSS_WINDOW - LZSS_MATCH, 0, 2 * LZSS_MATCH - 1);
  for (unsigned i = 0; i < LZSS_WINDOW; ++i) tree.parent[i] = LZSS_NIL;
  for (unsigned i = LZSS_WINDOW + 1; i <= LZSS_WINDOW + 256; ++i) tree.right[i] = LZSS_NIL;

  unsigned head = 0, tail = LZSS_WINDOW - LZSS_MATCH;
  unsigned count = in.size < LZSS_MATCH ? (unsigned)in.size : LZSS_MATCH;
  memcpy(tree.ring + tail, in.data, count);
  for (unsigned i = 1; i <= LZSS_MATCH; ++i) lzss_insert(&tree, tail - i);
  lzss_insert(&tree, tail);

  lzss_cursor pos = count, progress = 0;
  size_t flag_at = 0;
  uint8_t mask = 1;
  while (count) {
    if (report && (!progress || pos - progress >= (1u << 20))) {
      report((size_t)pos, (size_t)in.size);
      progress = pos;
    }
    if (mask == 1) {
      if (!lzss_reserve(out, 17)) return false;
      flag_at = out->size++;
      out->data[flag_at] = 0;
    }
    unsigned run = tree.length < count ? tree.length : count;
    if (run <= LZSS_THRESHOLD) {
      run = 1;
      out->data[flag_at] |= mask;
      out->data[out->size++] = tree.ring[tail];
    } else {
      out->data[out->size++] = (uint8_t)tree.match;
      out->data[out->size++] = ((tree.match >> 4) & 0xf0) | (run - LZSS_THRESHOLD - 1);
    }
    if (!(mask <<= 1)) mask = 1;
    while (run--) {
      lzss_erase(&tree, head);
      if (pos < in.size) {
        uint8_t byte = in.data[pos++];
        tree.ring[head] = byte;
        if (head < LZSS_MATCH - 1) tree.ring[head + LZSS_WINDOW] = byte;
      } else --count;
      head = (head + 1) & (LZSS_WINDOW - 1);
      tail = (tail + 1) & (LZSS_WINDOW - 1);
      if (count) lzss_insert(&tree, tail);
    }
  }
  return true;
}

static inline bool lzss_decode_impl(lzss_input in, lzss_buffer *out) {
  lzss_cursor pos = 0;
  while (pos < in.size) {
    unsigned flags = in.data[pos++];
    for (unsigned bit = 0; bit < 8 && pos < in.size; ++bit, flags >>= 1) {
      if (flags & 1) {
        if (!lzss_reserve(out, 1)) return false;
        out->data[out->size++] = in.data[pos++];
        continue;
      }
      if (in.size - pos < 2) return true;
      unsigned lo = in.data[pos++], hi = in.data[pos++];
      unsigned offset = lo | ((hi & 0xf0) << 4), count = (hi & 0xf) + LZSS_THRESHOLD + 1;
      size_t distance = ((out->size + LZSS_WINDOW - LZSS_MATCH - 1 - offset) & (LZSS_WINDOW - 1)) + 1;
      if (!lzss_reserve(out, count)) return false;

      // Reuse the output as history. Before byte zero, the ring starts with spaces and zero padding.
      size_t gap = distance > out->size ? distance - out->size : 0;
      while (gap && count) {
        out->data[out->size++] = gap-- <= LZSS_WINDOW - LZSS_MATCH ? ' ' : 0;
        --count;
      }
      if (count) {
        uint8_t *dst = out->data + out->size;
        const uint8_t *src = dst - distance;
        out->size += count;
        while (count--) *dst++ = *src++;
      }
    }
  }
  return true;
}

/* Use a fresh or released result. On success it owns data allocated by
   realloc. On failure it is empty. A null input is valid with size zero.
   The buffer's capacity is reserved for internal use. */
static inline void lzss_free(lzss_buffer *result) {
  free(result->data);
  *result = (lzss_buffer){0};
}

static inline bool lzss_encode(const void *data, size_t size, lzss_buffer *result) {
  *result = (lzss_buffer){0};
  if (size > LZSS_CURSOR_MAX || (!data && size)) return false;
  if (lzss_encode_impl((lzss_input){data, (lzss_cursor)size}, result, NULL)) return true;
  lzss_free(result);
  return false;
}

static inline bool lzss_decode(const void *data, size_t size, lzss_buffer *result) {
  *result = (lzss_buffer){0};
  if (size > LZSS_CURSOR_MAX || (!data && size)) return false;
  if (lzss_decode_impl((lzss_input){data, (lzss_cursor)size}, result)) return true;
  lzss_free(result);
  return false;
}

#endif
