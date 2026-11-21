/* Public domain. Matt Seabrook's refactor of Haruhiko Okumura's 1989 LZSS.
   12-bit ring offsets, 4-bit lengths, and least-significant flag bit first. */
#define _CRT_SECURE_NO_WARNINGS
#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include "lzss.h"

static bool input_size(lzss_input *in, uintmax_t size) {
  if (size > LZSS_CURSOR_MAX || size > SIZE_MAX) {
    fputs("Input exceeds the cursor or host address limit\n", stderr);
    return false;
  }
  in->size = (lzss_cursor)size;
  return true;
}

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static bool map_input(lzss_input *in, const char *path) {
  HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
  if (file == INVALID_HANDLE_VALUE) return false;
  LARGE_INTEGER size;
  bool ok = GetFileSizeEx(file, &size) && size.QuadPart >= 0 && input_size(in, size.QuadPart);
  if (ok && in->size) {
    HANDLE map = CreateFileMappingA(file, NULL, PAGE_READONLY, 0, 0, NULL);
    if (map) {
      in->data = MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0);
      CloseHandle(map);
    }
    ok = in->data != NULL;
  }
  CloseHandle(file);
  return ok;
}
#define unmap_input(in) UnmapViewOfFile((in).data)
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static bool map_input(lzss_input *in, const char *path) {
  int fd = open(path, O_RDONLY | O_NONBLOCK);
  if (fd < 0) return false;
  struct stat st;
  bool ok = fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && st.st_size >= 0 && input_size(in, st.st_size);
  if (ok && in->size) {
    void *data = mmap(NULL, in->size, PROT_READ, MAP_PRIVATE, fd, 0);
    ok = data != MAP_FAILED;
    if (ok) {
      in->data = data;
      madvise(data, in->size, MADV_SEQUENTIAL);
    }
  }
  close(fd);
  return ok;
}
#define unmap_input(in) munmap((void *)(in).data, (in).size)
#endif

void lzss_cli_progress(size_t pos, size_t size);

void lzss_cli_progress(size_t pos, size_t size) {
  fprintf(stderr, "\rEncoding: %zu / %zu bytes (%.1f%%)", pos, size, 100.0 * pos / size);
}

#ifdef LZSS_USE_ASM
#include <stddef.h>

/* The assembly receives caller-owned scratch space and the same output layout. */
static_assert(LZSS_WINDOW == 4096 && LZSS_MATCH == 18 && LZSS_THRESHOLD == 2);
static_assert(offsetof(lzss_buffer, data) == 0 && offsetof(lzss_buffer, size) == 8 &&
              offsetof(lzss_buffer, capacity) == 16);
static_assert(offsetof(lzss_tree, ring) == 0 && offsetof(lzss_tree, left) == 4114 &&
              offsetof(lzss_tree, right) == 12308 && offsetof(lzss_tree, parent) == 21014 &&
              offsetof(lzss_tree, match) == 29208 && offsetof(lzss_tree, length) == 29212);

bool lzss_asm_encode(const uint8_t *data, size_t size, lzss_buffer *out, lzss_tree *tree);
bool lzss_asm_decode(const uint8_t *data, size_t size, lzss_buffer *out);
bool lzss_cli_reserve(lzss_buffer *out, size_t extra);

bool lzss_cli_reserve(lzss_buffer *out, size_t extra) {
  return lzss_reserve(out, extra);
}

static bool encode(lzss_input in, lzss_buffer *out) {
  if (!in.size) return true;
  lzss_tree tree;
  return lzss_asm_encode(in.data, (size_t)in.size, out, &tree);
}

static bool decode(lzss_input in, lzss_buffer *out) {
  return lzss_asm_decode(in.data, (size_t)in.size, out);
}
#else
static bool encode(lzss_input in, lzss_buffer *out) {
  return lzss_encode_impl(in, out, lzss_cli_progress);
}

static bool decode(lzss_input in, lzss_buffer *out) {
  return lzss_decode_impl(in, out);
}
#endif

int main(int argc, char **argv) {
  if (argc != 4 || (argv[1][0] != 'e' && argv[1][0] != 'd')) {
    fprintf(stderr, "LZSS — C23 (N=%d F=%d THR=%d)\nUsage: %s e|d <in> <out>\n",
            LZSS_WINDOW, LZSS_MATCH, LZSS_THRESHOLD, argv[0]);
    return 1;
  }
  lzss_input in = {0};
  if (!map_input(&in, argv[2])) {
    fprintf(stderr, "Cannot read %s\n", argv[2]);
    return 1;
  }
  bool encoding = argv[1][0] == 'e';
  lzss_buffer out = {0};
  bool ok = encoding ? encode(in, &out) : decode(in, &out);
  if (ok && encoding && in.size) fputs("\r\033[K", stderr);
  if (in.data) unmap_input(in);
  if (!ok) {
    free(out.data);
    fputs("\nCannot allocate output buffer\n", stderr);
    return 1;
  }

  FILE *file = fopen(argv[3], "wb");
  if (file) {
    ok = !out.size || fwrite(out.data, 1, out.size, file) == out.size;
    ok = fclose(file) == 0 && ok;
  } else ok = false;
  free(out.data);
  if (!ok) {
    fprintf(stderr, "Cannot write %s\n", argv[3]);
    return 1;
  }
  fprintf(stderr, "%s: %zu → %zu bytes\n", encoding ? "Encoded" : "Decoded",
          (size_t)in.size, out.size);
  return 0;
}
