/*═══════════════════════════════════════════════════════════════════════════╗
║  LZSS — Lempel-Ziv-Storer-Szymanski Compression                            ║
║════════════════════════════════════════════════════════=═══════════════════╣
║  C23 • Zero-Copy Memory-Mapped I/O • Binary-Safe                           ║
║  Public Domain — 2026 Refactor of Haruhiko Okumura's 1989 Implementation   ║
║                                                                            ║
║  Format: 12-bit offset, 4-bit length, LSB-first flag bytes                 ║
║                                                                            ║
║  Author: Matt Seabrook (info@mattseabrook.net)                             ║ 
╚═══════════════════════════════════════════════════════════════════════════*/
#define _CRT_SECURE_NO_WARNINGS
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  typedef struct { uint8_t *data; size_t size; HANDLE fh, mh; } Map;
  static Map map_open(const char *path) {
    Map m = {0};
    m.fh = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (m.fh == INVALID_HANDLE_VALUE) return m;
    LARGE_INTEGER sz; GetFileSizeEx(m.fh, &sz); m.size = sz.QuadPart;
    if (!m.size) { CloseHandle(m.fh); return (Map){0}; }
    m.mh = CreateFileMappingA(m.fh, 0, PAGE_READONLY, 0, 0, 0);
    m.data = m.mh ? MapViewOfFile(m.mh, FILE_MAP_READ, 0, 0, 0) : 0;
    if (!m.data) { CloseHandle(m.mh); CloseHandle(m.fh); return (Map){0}; }
    return m;
  }
  static void map_close(Map *m) {
    if (m->data) UnmapViewOfFile(m->data);
    if (m->mh) CloseHandle(m->mh);
    if (m->fh) CloseHandle(m->fh);
  }
#else
  #include <sys/mman.h>
  #include <sys/stat.h>
  #include <fcntl.h>
  #include <unistd.h>
  typedef struct { uint8_t *data; size_t size; int fd; } Map;
  static Map map_open(const char *path) {
    Map m = {0}; struct stat st;
    if ((m.fd = open(path, O_RDONLY)) < 0 || fstat(m.fd, &st) < 0) return m;
    m.size = st.st_size;
    if (!m.size || (m.data = mmap(0, m.size, PROT_READ, MAP_PRIVATE, m.fd, 0)) == MAP_FAILED)
      { close(m.fd); return (Map){0}; }
    madvise(m.data, m.size, MADV_SEQUENTIAL);
    return m;
  }
  static void map_close(Map *m) { if (m->data) munmap(m->data, m->size); if (m->fd >= 0) close(m->fd); }
#endif

/*───────────────────────────────────────────────────────────────────────────╮
│ LZSS Parameters — Original 1989 Okumura used these exact values.           │
╰───────────────────────────────────────────────────────────────────────────*/
enum {
    RING_SIZE   = 4096,     // Ring buffer size in bytes (N = 2^12)
    MAX_MATCH   = 18,       // Maximum match length (F = lookahead size)
    MIN_MATCH   = 2,        // Minimum match for encoding (THRESHOLD)
    NIL         = RING_SIZE // Null node pointer for binary search tree
};

// Convenience aliases (classic LZSS naming)
#define N   RING_SIZE
#define F   MAX_MATCH
#define THR MIN_MATCH

_Static_assert(N == 4096 && F == 18 && THR == 2, "7th Guest params");

typedef struct { uint8_t *d; size_t sz, cap; } Buf;
static void buf_grow(Buf *b, size_t need) {
  if (b->sz + need <= b->cap) return;
  while (b->cap < b->sz + need) b->cap += b->cap / 2 + 64;
  b->d = realloc(b->d, b->cap);
}

typedef struct {
  uint8_t ring[N + F - 1];
  uint16_t lc[N + 1], rc[N + 257], par[N + 1];
  uint16_t mpos; uint8_t mlen;
} State;

static void tree_init(State *s) {
  for (int i = N + 1; i <= N + 256; ++i) s->rc[i] = NIL;
  for (int i = 0; i <= N; ++i) s->par[i] = NIL;  // Can't use memset: NIL=4096 is 16-bit
}

static void insert(State *s, uint32_t r) {
  uint8_t *key = &s->ring[r];
  uint32_t p = N + 1 + key[0];
  s->rc[r] = s->lc[r] = NIL; s->mlen = 0;
  int cmp = 1;
  for (;;) {
    uint16_t *branch = cmp >= 0 ? &s->rc[p] : &s->lc[p];
    if (*branch != NIL) { p = *branch; }
    else { *branch = r; s->par[r] = p; return; }
    uint32_t i = 1;
    while (i < F && key[i] == s->ring[p + i]) ++i;
    if (i > s->mlen) { s->mpos = p; s->mlen = i; if (i >= F) break; }
    cmp = key[i] - s->ring[p + i];
  }
  s->par[r] = s->par[p]; s->lc[r] = s->lc[p]; s->rc[r] = s->rc[p];
  s->par[s->lc[p]] = s->par[s->rc[p]] = r;
  *(s->rc[s->par[p]] == p ? &s->rc[s->par[p]] : &s->lc[s->par[p]]) = r;
  s->par[p] = NIL;
}

static void delete(State *s, uint32_t p) {
  if (s->par[p] == NIL) return;
  uint32_t q;
  if (s->rc[p] == NIL) q = s->lc[p];
  else if (s->lc[p] == NIL) q = s->rc[p];
  else {
    q = s->lc[p];
    if (s->rc[q] != NIL) {
      while (s->rc[q] != NIL) q = s->rc[q];
      s->rc[s->par[q]] = s->lc[q]; s->par[s->lc[q]] = s->par[q];
      s->lc[q] = s->lc[p]; s->par[s->lc[p]] = q;
    }
    s->rc[q] = s->rc[p]; s->par[s->rc[p]] = q;
  }
  s->par[q] = s->par[p];
  *(s->rc[s->par[p]] == p ? &s->rc[s->par[p]] : &s->lc[s->par[p]]) = q;
  s->par[p] = NIL;
}

static Buf encode(const uint8_t *in, size_t len) {
  Buf out = { malloc(len + len/8 + 256), 0, len + len/8 + 256 };
  if (!len) return out;
  State *s = calloc(1, sizeof(State));
  memset(s->ring, ' ', N - F);
  tree_init(s);

  uint8_t code[17], flags = 0, mask = 1;
  uint32_t cptr = 1, pos = 0, sid = 0, r = N - F, n = 0;
  size_t next_progress = 0;

  // Prime lookahead buffer
  while (n < F && pos < len) s->ring[r + n++] = in[pos++];
  for (uint32_t i = 1; i <= F; ++i) insert(s, r - i);
  insert(s, r);

  while (n > 0) {
    // Progress indicator every 1MB
    if (pos >= next_progress) {
      fprintf(stderr, "\rEncoding: %zu / %zu bytes (%.1f%%)", pos, len, 100.0 * pos / len);
      next_progress = pos + (1 << 20);
    }

    uint32_t ml = s->mlen > n ? n : s->mlen;
    if (ml <= THR) { ml = 1; flags |= mask; code[cptr++] = s->ring[r]; }
    else { code[cptr++] = s->mpos & 0xFF; code[cptr++] = ((s->mpos >> 4) & 0xF0) | (ml - THR - 1); }
    
    if (!(mask <<= 1)) {
      code[0] = flags; buf_grow(&out, cptr); memcpy(out.d + out.sz, code, cptr); out.sz += cptr;
      flags = 0; mask = 1; cptr = 1;
    }

    // Slide window by ml positions
    for (uint32_t i = 0; i < ml; ++i) {
      delete(s, sid);
      if (pos < len) {
        uint8_t c = in[pos++];
        s->ring[sid] = c;
        if (sid < F - 1) s->ring[sid + N] = c;
      } else {
        --n;
      }
      sid = (sid + 1) & (N - 1);
      r = (r + 1) & (N - 1);
      if (n > 0) insert(s, r);
    }
  }

  if (cptr > 1) { code[0] = flags; buf_grow(&out, cptr); memcpy(out.d + out.sz, code, cptr); out.sz += cptr; }
  fprintf(stderr, "\r\033[K"); // Clear progress line
  free(s);
  return out;
}

static Buf decode(const uint8_t *in, size_t len) {
  Buf out = { malloc(len * 4), 0, len * 4 };
  uint8_t ring[N]; memset(ring, ' ', N - F);
  uint32_t r = N - F, pos = 0;
  while (pos < len) {
    uint32_t flags = in[pos++] | 0xFF00;
    for (; (flags & 0x100) && pos < len; flags >>= 1) {
      if (flags & 1) {
        uint8_t c = in[pos++];
        buf_grow(&out, 1); out.d[out.sz++] = c;
        ring[r] = c; r = (r + 1) & (N - 1);
      } else {
        if (pos + 1 >= len) break;
        uint32_t lo = in[pos++], hi = in[pos++];
        uint32_t p = lo | ((hi & 0xF0) << 4), ml = (hi & 0x0F) + THR + 1;
        buf_grow(&out, ml);
        for (uint32_t k = 0; k < ml; ++k) {
          uint8_t c = ring[(p + k) & (N - 1)];
          out.d[out.sz++] = c; ring[r] = c; r = (r + 1) & (N - 1);
        }
      }
    }
  }
  return out;
}

int main(int argc, char **argv) {
  if (argc != 4 || (argv[1][0] != 'e' && argv[1][0] != 'd')) {
    fprintf(stderr, "LZSS — C23 (N=%d F=%d THR=%d)\nUsage: %s e|d <in> <out>\n", N, F, THR, argv[0]);
    return 1;
  }
  Map m = map_open(argv[2]);
  if (!m.data && m.size) { fprintf(stderr, "Cannot open %s\n", argv[2]); return 1; }
  Buf out = argv[1][0] == 'e' ? encode(m.data, m.size) : decode(m.data, m.size);
  fprintf(stderr, "%s: %zu → %zu bytes\n", argv[1][0] == 'e' ? "Encoded" : "Decoded", m.size, out.sz);
  map_close(&m);
  FILE *f = fopen(argv[3], "wb");
  if (!f || fwrite(out.d, 1, out.sz, f) != out.sz) { fprintf(stderr, "Write error\n"); return 1; }
  fclose(f); free(out.d);
  return 0;
}
