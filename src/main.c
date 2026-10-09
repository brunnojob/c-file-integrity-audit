#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    char *path;
    char digest[65];
    off_t size;
} Entry;
typedef struct {
    Entry *entries;
    size_t used, capacity;
} Manifest;

static void cleanup(Manifest *m) {
    for (size_t i = 0; i < m->used; i++)
        free(m->entries[i].path);
    free(m->entries);
}
static int append(Manifest *m, const char *path, const char *digest, off_t size) {
    if (m->used == m->capacity) {
        size_t capacity = m->capacity ? m->capacity * 2 : 32;
        if (capacity > SIZE_MAX / sizeof(Entry))
            return -1;
        Entry *next = realloc(m->entries, capacity * sizeof(Entry));
        if (!next)
            return -1;
        m->entries = next;
        m->capacity = capacity;
    }
    char *copy = strdup(path);
    if (!copy)
        return -1;
    m->entries[m->used].path = copy;
    strcpy(m->entries[m->used].digest, digest);
    m->entries[m->used++].size = size;
    return 0;
}
static int hash_file(const char *path, char hex[65], off_t *size) {
    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0)
        return -1;
    struct stat before, after;
    if (fstat(fd, &before) || !S_ISREG(before.st_mode)) {
        close(fd);
        errno = EINVAL;
        return -1;
    }
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    int ok = ctx && EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) == 1;
    unsigned char buffer[65536], digest[32];
    ssize_t n = 0;
    unsigned len = 0;
    while (ok && (n = read(fd, buffer, sizeof buffer)) != 0) {
        if (n < 0) {
            if (errno == EINTR)
                continue;
            ok = 0;
            break;
        }
        ok = EVP_DigestUpdate(ctx, buffer, (size_t)n) == 1;
    }
    if (ok)
        ok = EVP_DigestFinal_ex(ctx, digest, &len) == 1 && len == 32;
    if (fstat(fd, &after) || before.st_size != after.st_size ||
        before.st_mtim.tv_sec != after.st_mtim.tv_sec ||
        before.st_mtim.tv_nsec != after.st_mtim.tv_nsec ||
        before.st_ctim.tv_sec != after.st_ctim.tv_sec ||
        before.st_ctim.tv_nsec != after.st_ctim.tv_nsec)
        ok = 0;
    EVP_MD_CTX_free(ctx);
    close(fd);
    if (!ok) {
        errno = EIO;
        return -1;
    }
    for (unsigned i = 0; i < 32; i++)
        sprintf(hex + i * 2, "%02x", digest[i]);
    *size = before.st_size;
    return 0;
}
static int scan(Manifest *m, const char *root, const char *relative, unsigned depth) {
    if (depth > 64) {
        errno = ELOOP;
        return -1;
    }
    char full[4096];
    if (snprintf(full, sizeof full, "%s%s%s", root, *relative ? "/" : "", relative) >=
        (int)sizeof full) {
        errno = ENAMETOOLONG;
        return -1;
    }
    struct stat s;
    if (lstat(full, &s))
        return -1;
    if (S_ISLNK(s.st_mode)) {
        errno = ELOOP;
        return -1;
    }
    if (S_ISREG(s.st_mode)) {
        char digest[65];
        off_t size;
        if (hash_file(full, digest, &size))
            return -1;
        return append(m, relative, digest, size);
    }
    if (!S_ISDIR(s.st_mode)) {
        errno = EINVAL;
        return -1;
    }
    DIR *directory = opendir(full);
    if (!directory)
        return -1;
    struct dirent *item;
    int result = 0;
    errno = 0;
    while ((item = readdir(directory))) {
        if (!strcmp(item->d_name, ".") || !strcmp(item->d_name, ".."))
            continue;
        char child[4096];
        if (snprintf(child, sizeof child, "%s%s%s", relative, *relative ? "/" : "", item->d_name) >=
            (int)sizeof child) {
            result = -1;
            errno = ENAMETOOLONG;
            break;
        }
        if (scan(m, root, child, depth + 1)) {
            result = -1;
            break;
        }
        errno = 0;
    }
    if (errno)
        result = -1;
    int saved = errno;
    closedir(directory);
    errno = saved;
    return result;
}
static int compare(const void *a, const void *b) {
    return strcmp(((const Entry *)a)->path, ((const Entry *)b)->path);
}
static int nibble(char c) {
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}
static int load(const char *path, Manifest *m) {
    FILE *file = fopen(path, "r");
    if (!file)
        return -1;
    char *line = NULL;
    size_t cap = 0;
    ssize_t n;
    int result = 0;
    while ((n = getline(&line, &cap, file)) >= 0) {
        char *end;
        errno = 0;
        if (n < 68 || line[64] != ' ') {
            result = -1;
            break;
        }
        for (int i = 0; i < 64; i++)
            if (nibble(line[i]) < 0)
                result = -1;
        if (result)
            break;
        intmax_t size = strtoimax(line + 65, &end, 10);
        if (errno || size < 0 || *end != ' ') {
            result = -1;
            break;
        }
        char *encoded = end + 1;
        size_t length = strcspn(encoded, "\r\n");
        if (!length || length % 2 || length > 8190) {
            result = -1;
            break;
        }
        char decoded[4096];
        for (size_t i = 0; i < length / 2; i++) {
            int a = nibble(encoded[2 * i]), b = nibble(encoded[2 * i + 1]);
            if (a < 0 || b < 0 || !(a * 16 + b)) {
                result = -1;
                break;
            }
            decoded[i] = (char)(a * 16 + b);
        }
        if (result)
            break;
        decoded[length / 2] = 0;
        line[64] = 0;
        if (append(m, decoded, line, (off_t)size)) {
            result = -1;
            break;
        }
    }
    if (ferror(file))
        result = -1;
    free(line);
    fclose(file);
    if (result) {
        errno = EINVAL;
        return -1;
    }
    if (m->used)
        qsort(m->entries, m->used, sizeof(Entry), compare);
    for (size_t i = 1; i < m->used; i++)
        if (!strcmp(m->entries[i - 1].path, m->entries[i].path)) {
            errno = EINVAL;
            return -1;
        }
    return 0;
}
int main(int argc, char **argv) {
    if (argc < 3 || argc > 4) {
        fprintf(stderr,
                "usage: audit hash FILE | snapshot DIRECTORY | verify DIRECTORY MANIFEST\n");
        return 2;
    }
    if (!strcmp(argv[1], "hash") && argc == 3) {
        char digest[65];
        off_t size;
        if (hash_file(argv[2], digest, &size)) {
            perror("hash");
            return 2;
        }
        puts(digest);
        return 0;
    }
    if ((strcmp(argv[1], "snapshot") && strcmp(argv[1], "verify")) ||
        (!strcmp(argv[1], "verify") && argc != 4))
        return 2;
    Manifest current = {0}, baseline = {0};
    int result = 0;
    if (scan(&current, argv[2], "", 0)) {
        perror("scan");
        cleanup(&current);
        return 2;
    }
    if (current.used)
        qsort(current.entries, current.used, sizeof(Entry), compare);
    if (!strcmp(argv[1], "snapshot")) {
        for (size_t i = 0; i < current.used; i++) {
            Entry *e = &current.entries[i];
            printf("%s %jd ", e->digest, (intmax_t)e->size);
            for (const unsigned char *p = (unsigned char *)e->path; *p; p++)
                printf("%02x", *p);
            putchar('\n');
        }
    } else {
        if (load(argv[3], &baseline)) {
            perror("manifest");
            cleanup(&current);
            cleanup(&baseline);
            return 2;
        }
        size_t a = 0, b = 0, added = 0, removed = 0, changed = 0;
        while (a < current.used || b < baseline.used) {
            int c = a == current.used ? 1
                    : b == baseline.used
                        ? -1
                        : strcmp(current.entries[a].path, baseline.entries[b].path);
            if (c < 0) {
                added++;
                a++;
            } else if (c > 0) {
                removed++;
                b++;
            } else {
                if (strcmp(current.entries[a].digest, baseline.entries[b].digest) ||
                    current.entries[a].size != baseline.entries[b].size)
                    changed++;
                a++;
                b++;
            }
        }
        printf("{\"files\":%zu,\"added\":%zu,\"removed\":%zu,\"changed\":%zu,\"valid\":%s}\n",
               current.used, added, removed, changed,
               (added || removed || changed) ? "false" : "true");
        result = added || removed || changed;
    }
    cleanup(&current);
    cleanup(&baseline);
    return result;
}
