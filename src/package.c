#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include "hhy/package.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    char name[128];
    char author[256];
    char version[64];
    char requires_hhy[64];
    char command[PATH_MAX];
    char protocol[32];
    char capabilities[2048];
} Manifest;

bool hhy_package_home(char *out, unsigned long size) {
    const char *configured = getenv("HHY_EXTENSION_HOME");
    if (configured != NULL && configured[0] != '\0')
        return snprintf(out, size, "%s", configured) < (int)size;
    const char *home = getenv("HOME");
    return home != NULL && snprintf(out, size, "%s/.hhy/extensions", home) < (int)size;
}

static bool safe_name(const char *name) {
    if (name[0] == '\0' || strcmp(name, "hhy") == 0 || strcmp(name, "std") == 0) return false;
    for (const unsigned char *p = (const unsigned char *)name; *p; p++)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-')) return false;
    return true;
}

static bool read_manifest(const char *path, Manifest *manifest) {
    memset(manifest, 0, sizeof(*manifest)); FILE *file = fopen(path, "rb");
    if (file == NULL) return false;
    char section[32] = "", line[2048]; bool valid = true;
    while (fgets(line, sizeof(line), file) != NULL) {
        char *start = line; while (*start == ' ' || *start == '\t') start++;
        if (*start == '#' || *start == '\n' || *start == '\0') continue;
        if (*start == '[') {
            char *end = strchr(start, ']');
            if (end && (size_t)(end - start - 1) < sizeof(section)) {
                memcpy(section, start + 1, (size_t)(end - start - 1)); section[end - start - 1] = '\0';
            }
            continue;
        }
        char key[64], value[PATH_MAX];
        if (sscanf(start, "%63[^ =] = \"%1023[^\"]\"", key, value) == 2) {
            char *target = NULL; size_t capacity = 0;
            if (strcmp(section, "package") == 0 && strcmp(key, "name") == 0)
                target = manifest->name, capacity = sizeof(manifest->name);
            else if (strcmp(section, "package") == 0 && strcmp(key, "author") == 0)
                target = manifest->author, capacity = sizeof(manifest->author);
            else if (strcmp(section, "package") == 0 && strcmp(key, "version") == 0)
                target = manifest->version, capacity = sizeof(manifest->version);
            else if (strcmp(section, "package") == 0 && strcmp(key, "requires_hhy") == 0)
                target = manifest->requires_hhy, capacity = sizeof(manifest->requires_hhy);
            else if (strcmp(section, "extension") == 0 && strcmp(key, "command") == 0)
                target = manifest->command, capacity = sizeof(manifest->command);
            else if (strcmp(section, "extension") == 0 && strcmp(key, "protocol") == 0)
                target = manifest->protocol, capacity = sizeof(manifest->protocol);
            if (target != NULL) {
                size_t length = strlen(value);
                if (length >= capacity) valid = false;
                else memcpy(target, value, length + 1);
            }
        }
        if (strcmp(section, "capabilities") == 0) {
            size_t used = strlen(manifest->capabilities), available = sizeof(manifest->capabilities) - used;
            if (available > 1) snprintf(manifest->capabilities + used, available, "  %s", start);
        }
    }
    fclose(file);
    return valid && safe_name(manifest->name) && manifest->version[0] && manifest->requires_hhy[0] &&
        strcmp(manifest->protocol, "1") == 0 && manifest->command[0] != '\0' &&
        strncmp(manifest->command, "bin/", 4) == 0 &&
        strchr(manifest->command + 4, '/') == NULL && strstr(manifest->command, "..") == NULL;
}

static bool make_directory(const char *path) {
    if (mkdir(path, 0755) == 0) return true;
    return errno == EEXIST;
}

static bool make_home(const char *home) {
    char copy[PATH_MAX];
    if (snprintf(copy, sizeof(copy), "%s", home) >= (int)sizeof(copy)) return false;
    for (char *p = copy + 1; *p; p++) {
        if (*p != '/') continue;
        *p = '\0';
        if (!make_directory(copy)) return false;
        *p = '/';
    }
    return make_directory(copy);
}

static bool copy_file(const char *source, const char *target, mode_t mode) {
    int input = open(source, O_RDONLY), output = -1; bool ok = input >= 0;
    if (ok) output = open(target, O_WRONLY | O_CREAT | O_EXCL, mode), ok = output >= 0;
    char buffer[65536];
    while (ok) {
        ssize_t count = read(input, buffer, sizeof(buffer));
        if (count == 0) break;
        if (count < 0) { ok = false; break; }
        size_t offset = 0;
        while (offset < (size_t)count) {
            ssize_t written = write(output, buffer + offset, (size_t)count - offset);
            if (written <= 0) { ok = false; break; }
            offset += (size_t)written;
        }
    }
    if (input >= 0) close(input);
    if (output >= 0 && close(output) != 0) ok = false;
    if (!ok) unlink(target);
    return ok;
}

static bool sha256_file(const char *path, char output[65]) {
    FILE *file = fopen(path, "rb"); EVP_MD_CTX *context = EVP_MD_CTX_new();
    bool ok = file != NULL && context != NULL && EVP_DigestInit_ex(context, EVP_sha256(), NULL) == 1;
    unsigned char buffer[65536], digest[EVP_MAX_MD_SIZE]; unsigned int length = 0;
    while (ok) {
        size_t count = fread(buffer, 1, sizeof(buffer), file);
        if (count && EVP_DigestUpdate(context, buffer, count) != 1) ok = false;
        if (count < sizeof(buffer)) { if (ferror(file)) ok = false; break; }
    }
    if (ok) ok = EVP_DigestFinal_ex(context, digest, &length) == 1 && length == 32;
    if (file) fclose(file);
    EVP_MD_CTX_free(context);
    if (!ok) return false;
    for (unsigned int i = 0; i < length; i++) snprintf(output + i * 2, 3, "%02x", digest[i]);
    output[64] = '\0'; return true;
}

static void clear_flat_directory(const char *path) {
    DIR *directory = opendir(path);
    if (directory == NULL) return;
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char file[PATH_MAX];
        if (snprintf(file, sizeof(file), "%s/%s", path, entry->d_name) < (int)sizeof(file))
            (void)unlink(file);
    }
    closedir(directory); (void)rmdir(path);
}

static bool copy_package_libraries(const char *source, const char *target, FILE *record) {
    DIR *directory = opendir(source);
    if (directory == NULL) return errno == ENOENT;
    bool ok = make_directory(target); struct dirent *entry;
    while (ok && (entry = readdir(directory)) != NULL) {
        if (entry->d_name[0] == '.' || strchr(entry->d_name, '/') != NULL) continue;
        char source_file[PATH_MAX], target_file[PATH_MAX], source_hash[65], target_hash[65];
        struct stat info;
        if (snprintf(source_file, sizeof(source_file), "%s/%s", source, entry->d_name) >= (int)sizeof(source_file) ||
            snprintf(target_file, sizeof(target_file), "%s/%s", target, entry->d_name) >= (int)sizeof(target_file) ||
            stat(source_file, &info) != 0 || !S_ISREG(info.st_mode) ||
            !copy_file(source_file, target_file, 0755) ||
            !sha256_file(source_file, source_hash) || !sha256_file(target_file, target_hash) ||
            strcmp(source_hash, target_hash) != 0 ||
            fprintf(record, "%s  lib/%s\n", target_hash, entry->d_name) < 0) ok = false;
    }
    closedir(directory);
    if (!ok) clear_flat_directory(target);
    return ok;
}

static void print_capability_value(const char *label, const char *value) {
    const char *display = value;
    if (strcmp(value, "[]") == 0) display = "none";
    else if (strcmp(value, "false") == 0) display = "no";
    else if (strcmp(value, "true") == 0) display = "yes";
    printf("    %-9s %s\n", label, display);
}

static void print_capabilities(const char *capabilities) {
    if (capabilities[0] == '\0') { puts("    none"); return; }
    char copy[2048]; snprintf(copy, sizeof(copy), "%s", capabilities);
    char *save = NULL;
    for (char *line = strtok_r(copy, "\n", &save); line != NULL;
         line = strtok_r(NULL, "\n", &save)) {
        while (*line == ' ' || *line == '\t') line++;
        char *equal = strstr(line, " = ");
        if (equal == NULL) continue;
        *equal = '\0'; const char *value = equal + 3;
        if (strcmp(line, "read") == 0) print_capability_value("Read", value);
        else if (strcmp(line, "write") == 0) print_capability_value("Write", value);
        else if (strcmp(line, "network") == 0) print_capability_value("Network", value);
        else if (strcmp(line, "process") == 0) print_capability_value("Process", value);
        else print_capability_value(line, value);
    }
}

int hhy_package_install(const char *source, bool assume_yes) {
    char resolved[PATH_MAX], manifest_path[PATH_MAX], executable[PATH_MAX]; Manifest manifest;
    if (realpath(source, resolved) == NULL ||
        snprintf(manifest_path, sizeof(manifest_path), "%s/hhy.toml", resolved) >= (int)sizeof(manifest_path) ||
        !read_manifest(manifest_path, &manifest)) {
        fputs("hhy: invalid local extension manifest\n", stderr); return 3;
    }
    if (manifest.author[0] == '\0') {
        fputs("hhy: extension manifest requires package.author\n", stderr); return 3;
    }
    if (strncmp(manifest.requires_hhy, ">=1.1", 5) != 0) {
        fputs("hhy: extension requires an incompatible HHY version\n", stderr); return 3;
    }
    if (snprintf(executable, sizeof(executable), "%s/%s", resolved, manifest.command) >= (int)sizeof(executable) ||
        access(executable, X_OK) != 0) {
        fputs("hhy: extension executable is missing or not executable\n", stderr); return 3;
    }
    printf("Package: %s %s\nAuthor: %s\nProtocol: %s\nCapabilities:\n%s",
           manifest.name, manifest.version, manifest.author, manifest.protocol,
           manifest.capabilities[0] ? manifest.capabilities : "  none\n");
    if (!assume_yes) {
        char answer[16]; fputs("Install this local extension? [y/N] ", stdout); fflush(stdout);
        if (fgets(answer, sizeof(answer), stdin) == NULL || (answer[0] != 'y' && answer[0] != 'Y')) return 3;
    }
    char home[PATH_MAX], target[PATH_MAX], target_bin[PATH_MAX], target_manifest[PATH_MAX];
    if (!hhy_package_home(home, sizeof(home)) || !make_home(home) ||
        snprintf(target, sizeof(target), "%s/%s", home, manifest.name) >= (int)sizeof(target) ||
        snprintf(target_bin, sizeof(target_bin), "%s/bin", target) >= (int)sizeof(target_bin) ||
        snprintf(target_manifest, sizeof(target_manifest), "%s/hhy.toml", target) >= (int)sizeof(target_manifest)) {
        fputs("hhy: cannot prepare extension home\n", stderr); return 4;
    }
    if (access(target, F_OK) == 0) { fputs("hhy: package is already installed; remove it first\n", stderr); return 3; }
    if (!make_directory(target) || !make_directory(target_bin)) { fputs("hhy: cannot create package directory\n", stderr); return 4; }
    const char *basename = strrchr(manifest.command, '/'); basename = basename ? basename + 1 : manifest.command;
    char target_executable[PATH_MAX];
    bool ok = snprintf(target_executable, sizeof(target_executable), "%s/%s", target_bin, basename) < (int)sizeof(target_executable) &&
        copy_file(manifest_path, target_manifest, 0644) && copy_file(executable, target_executable, 0755);
    char source_hash[65], installed_hash[65], manifest_hash[65];
    ok = ok && sha256_file(executable, source_hash) && sha256_file(target_executable, installed_hash) &&
         strcmp(source_hash, installed_hash) == 0 && sha256_file(target_manifest, manifest_hash);
    if (!ok) {
        unlink(target_executable); unlink(target_manifest); rmdir(target_bin); rmdir(target);
        fputs("hhy: extension copy or integrity verification failed\n", stderr); return 4;
    }
    char hash_path[PATH_MAX], source_lib[PATH_MAX], target_lib[PATH_MAX];
    if (snprintf(hash_path, sizeof(hash_path), "%s/SHA256", target) >= (int)sizeof(hash_path) ||
        snprintf(source_lib, sizeof(source_lib), "%s/lib", resolved) >= (int)sizeof(source_lib) ||
        snprintf(target_lib, sizeof(target_lib), "%s/lib", target) >= (int)sizeof(target_lib)) {
        unlink(target_executable); unlink(target_manifest); rmdir(target_bin); rmdir(target);
        fputs("hhy: extension package path is too long\n", stderr); return 4;
    }
    FILE *hash = fopen(hash_path, "wb");
    bool hash_ok = hash != NULL;
    if (hash_ok) hash_ok = fprintf(hash, "%s  hhy.toml\n%s  bin/%s\n",
                                  manifest_hash, installed_hash, basename) >= 0;
    if (hash_ok) hash_ok = copy_package_libraries(source_lib, target_lib, hash);
    if (hash != NULL && fclose(hash) != 0) hash_ok = false;
    if (!hash_ok) {
        unlink(hash_path); clear_flat_directory(target_lib); unlink(target_executable);
        unlink(target_manifest); rmdir(target_bin); rmdir(target);
        fputs("hhy: cannot write extension integrity record\n", stderr); return 4;
    }
    printf("Installed %s %s\n", manifest.name, manifest.version); return 0;
}

int hhy_package_list(void) {
    char home[PATH_MAX];
    if (!hhy_package_home(home, sizeof(home))) return 4;
    DIR *directory = opendir(home);
    if (directory == NULL && errno == ENOENT) return 0;
    if (directory == NULL) { fputs("hhy: cannot read extension home\n", stderr); return 4; }
    struct dirent *entry; bool first = true;
    while ((entry = readdir(directory)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char path[PATH_MAX]; Manifest manifest;
        if (snprintf(path, sizeof(path), "%s/%s/hhy.toml", home, entry->d_name) < (int)sizeof(path) &&
            read_manifest(path, &manifest))
            {
                if (!first) putchar('\n');
                first = false;
                printf("%s %s\n", manifest.name, manifest.version);
                printf("  %-12s %s\n", "Author", manifest.author[0]
                    ? manifest.author : "Unknown (legacy package)");
                printf("  %-12s %s\n", "Protocol", manifest.protocol);
                puts("  Permissions"); print_capabilities(manifest.capabilities);
            }
    }
    closedir(directory); return 0;
}

bool hhy_package_verify(const char *name, const char **error) {
    static char message[128]; char home[PATH_MAX], package[PATH_MAX], record[PATH_MAX];
    if (!safe_name(name) || !hhy_package_home(home, sizeof(home)) ||
        snprintf(package, sizeof(package), "%s/%s", home, name) >= (int)sizeof(package) ||
        snprintf(record, sizeof(record), "%s/SHA256", package) >= (int)sizeof(record)) {
        *error = "invalid installed package path"; return false;
    }
    FILE *file = fopen(record, "rb");
    if (file == NULL) { *error = "installed package has no integrity record"; return false; }
    bool ok = true; size_t checked = 0; char line[PATH_MAX + 80];
    while (fgets(line, sizeof(line), file) != NULL) {
        char expected[65], relative[PATH_MAX];
        if (sscanf(line, "%64[0-9a-f]  %1023s", expected, relative) != 2 ||
            relative[0] == '/' || strstr(relative, "..") != NULL) { ok = false; break; }
        char path[PATH_MAX], actual[65];
        if (snprintf(path, sizeof(path), "%s/%s", package, relative) >= (int)sizeof(path) ||
            !sha256_file(path, actual) || strcmp(expected, actual) != 0) { ok = false; break; }
        checked++;
    }
    fclose(file);
    if (!ok || checked < 2) {
        snprintf(message, sizeof(message), "installed package integrity verification failed");
        *error = message; return false;
    }
    return true;
}

int hhy_package_remove(const char *name) {
    if (!safe_name(name)) { fputs("hhy: invalid package name\n", stderr); return 3; }
    char home[PATH_MAX], target[PATH_MAX], manifest_path[PATH_MAX], hash_path[PATH_MAX]; Manifest manifest;
    if (!hhy_package_home(home, sizeof(home)) ||
        snprintf(target, sizeof(target), "%s/%s", home, name) >= (int)sizeof(target) ||
        snprintf(manifest_path, sizeof(manifest_path), "%s/hhy.toml", target) >= (int)sizeof(manifest_path) ||
        !read_manifest(manifest_path, &manifest) || strcmp(manifest.name, name) != 0) {
        fputs("hhy: package is not installed\n", stderr); return 3;
    }
    char bin[PATH_MAX], lib[PATH_MAX];
    if (snprintf(bin, sizeof(bin), "%s/bin", target) >= (int)sizeof(bin) ||
        snprintf(lib, sizeof(lib), "%s/lib", target) >= (int)sizeof(lib) ||
        snprintf(hash_path, sizeof(hash_path), "%s/SHA256", target) >= (int)sizeof(hash_path)) {
        fputs("hhy: installed package path is too long\n", stderr); return 4;
    }
    FILE *record = fopen(hash_path, "rb"); bool ok = record != NULL; char line[PATH_MAX + 80];
    while (ok && fgets(line, sizeof(line), record) != NULL) {
        char expected[65], relative[PATH_MAX], installed[PATH_MAX];
        if (sscanf(line, "%64[0-9a-f]  %1023s", expected, relative) != 2 ||
            relative[0] == '/' || strstr(relative, "..") != NULL ||
            snprintf(installed, sizeof(installed), "%s/%s", target, relative) >= (int)sizeof(installed) ||
            unlink(installed) != 0) ok = false;
    }
    if (record != NULL) fclose(record);
    if (unlink(hash_path) != 0) ok = false;
    if (rmdir(lib) != 0 && errno != ENOENT) ok = false;
    if (rmdir(bin) != 0 || rmdir(target) != 0) ok = false;
    if (!ok) { fputs("hhy: package contains unknown resources or cannot be removed\n", stderr); return 4; }
    printf("Removed %s\n", name); return 0;
}
