#define _POSIX_C_SOURCE 200809L
#include "hhy/package.h"

#include <jansson.h>
#include <errno.h>
#include <limits.h>
#include <openssl/evp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef HHY_VERSION
#define HHY_VERSION "unknown"
#endif

#define MAX_PACKAGES 64
#define MAX_PLAN 64

typedef struct {
    json_t *json;
    const char *identity;
    const char *version;
    const char *runtime_name;
    const char *source;
    const char *target;
} RegistryPackage;

typedef struct {
    RegistryPackage packages[MAX_PACKAGES];
    size_t count;
    RegistryPackage *plan[MAX_PLAN];
    size_t plan_count;
    const unsigned char *public_key;
    size_t public_key_length;
    const char *key_id;
    char registry[PATH_MAX];
} Registry;

static bool safe_relative(const char *value) {
    return value != NULL && value[0] != '\0' && value[0] != '/' && strstr(value, "..") == NULL;
}

static bool safe_identity(const char *value) {
    if (value == NULL || strchr(value, '/') == NULL || value[0] == '/') return false;
    for (const unsigned char *p = (const unsigned char *)value; *p; p++)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-' || *p == '/')) return false;
    return strstr(value, "//") == NULL && strstr(value, "..") == NULL;
}

static bool safe_runtime_name(const char *value) {
    if (value == NULL || value[0] == '\0' || strcmp(value, "hhy") == 0 || strcmp(value, "std") == 0) return false;
    for (const unsigned char *p = (const unsigned char *)value; *p; p++)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-')) return false;
    return true;
}

static const char *current_target(void) {
#if defined(__APPLE__)
#if defined(__aarch64__) || defined(__arm64__)
    return "darwin-arm64";
#elif defined(__x86_64__)
    return "darwin-x86_64";
#endif
#elif defined(_WIN32)
#if defined(__x86_64__) || defined(_M_X64)
    return "windows-x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "windows-arm64";
#endif
#elif defined(__linux__)
#if defined(__aarch64__)
    return "linux-arm64";
#elif defined(__x86_64__)
    return "linux-x86_64";
#endif
#endif
    return NULL;
}

static bool safe_target(const char *value) {
    static const char *targets[] = {
        "darwin-arm64", "darwin-x86_64", "linux-arm64", "linux-x86_64",
        "windows-arm64", "windows-x86_64"
    };
    if (value == NULL) return false;
    for (size_t i = 0; i < sizeof(targets) / sizeof(targets[0]); i++)
        if (strcmp(value, targets[i]) == 0) return true;
    return false;
}

static bool parse_version(const char *text, int out[3]) {
    char tail = '\0';
    return text != NULL && sscanf(text, "%d.%d.%d%c", &out[0], &out[1], &out[2], &tail) == 3 &&
        out[0] >= 0 && out[1] >= 0 && out[2] >= 0;
}

static int compare_version(const char *left, const char *right) {
    int a[3], b[3];
    if (!parse_version(left, a) || !parse_version(right, b)) return 0;
    for (size_t i = 0; i < 3; i++) if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    return 0;
}

static bool version_matches(const char *version, const char *constraint) {
    int actual[3], required[3];
    if (constraint == NULL || strcmp(constraint, "*") == 0) return true;
    if (!parse_version(version, actual)) return false;
    if (constraint[0] == '^') {
        return parse_version(constraint + 1, required) && actual[0] == required[0] &&
            compare_version(version, constraint + 1) >= 0;
    }
    if (strncmp(constraint, ">=", 2) == 0)
        return parse_version(constraint + 2, required) && compare_version(version, constraint + 2) >= 0;
    if (constraint[0] == '=') constraint++;
    return parse_version(constraint, required) && compare_version(version, constraint) == 0;
}

static bool decode_base64(const char *text, unsigned char **output, size_t *length) {
    if (text == NULL) return false;
    size_t input_length = strlen(text);
    unsigned char *buffer = malloc(input_length + 1);
    if (buffer == NULL) return false;
    int decoded = EVP_DecodeBlock(buffer, (const unsigned char *)text, (int)input_length);
    if (decoded < 0) { free(buffer); return false; }
    while (input_length > 0 && text[input_length - 1] == '=') { decoded--; input_length--; }
    *output = buffer; *length = (size_t)decoded; return true;
}

static char *canonical_without(json_t *object, const char *field) {
    json_t *copy = json_deep_copy(object);
    if (copy == NULL || json_object_del(copy, field) != 0) { json_decref(copy); return NULL; }
    char *result = json_dumps(copy, JSON_COMPACT | JSON_SORT_KEYS | JSON_ENSURE_ASCII);
    json_decref(copy); return result;
}

static bool verify_signature(json_t *object, const char *field,
                             const unsigned char *key, size_t key_length) {
    const char *encoded = json_string_value(json_object_get(object, field));
    unsigned char *signature = NULL; size_t signature_length = 0;
    char *payload = canonical_without(object, field);
    EVP_PKEY *public_key = key_length == 32
        ? EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, key, key_length) : NULL;
    EVP_MD_CTX *context = EVP_MD_CTX_new();
    bool ok = payload != NULL && public_key != NULL && context != NULL &&
        decode_base64(encoded, &signature, &signature_length) &&
        EVP_DigestVerifyInit(context, NULL, NULL, NULL, public_key) == 1 &&
        EVP_DigestVerify(context, signature, signature_length,
                         (const unsigned char *)payload, strlen(payload)) == 1;
    free(signature); free(payload); EVP_MD_CTX_free(context); EVP_PKEY_free(public_key); return ok;
}

static bool sha256_file(const char *path, char output[65]) {
    FILE *file = fopen(path, "rb"); EVP_MD_CTX *context = EVP_MD_CTX_new();
    bool ok = file != NULL && context != NULL && EVP_DigestInit_ex(context, EVP_sha256(), NULL) == 1;
    unsigned char buffer[65536], digest[32]; unsigned int length = 0;
    while (ok) {
        size_t count = fread(buffer, 1, sizeof(buffer), file);
        if (count > 0 && EVP_DigestUpdate(context, buffer, count) != 1) ok = false;
        if (count < sizeof(buffer)) { if (ferror(file)) ok = false; break; }
    }
    if (ok) ok = EVP_DigestFinal_ex(context, digest, &length) == 1 && length == 32;
    if (file != NULL) fclose(file);
    EVP_MD_CTX_free(context);
    if (!ok) return false;
    for (size_t i = 0; i < 32; i++) snprintf(output + i * 2, 3, "%02x", digest[i]);
    output[64] = '\0'; return true;
}

static const char *lock_path(const HhyPackageInstallOptions *options) {
    return options && options->lockfile ? options->lockfile : "hhy.lock";
}

static bool make_path(const char *path) {
    char copy[PATH_MAX];
    if (snprintf(copy, sizeof(copy), "%s", path) >= (int)sizeof(copy)) return false;
    for (char *p = copy + 1; *p; p++) if (*p == '/') {
        *p = '\0'; if (mkdir(copy, 0755) != 0 && errno != EEXIST) return false; *p = '/';
    }
    return mkdir(copy, 0755) == 0 || errno == EEXIST;
}

static bool copy_cached_file(const char *source, const char *target) {
    char parent[PATH_MAX];
    if (snprintf(parent, sizeof(parent), "%s", target) >= (int)sizeof(parent)) return false;
    char *slash = strrchr(parent, '/'); if (slash == NULL) return false; *slash = '\0';
    if (!make_path(parent)) return false;
    FILE *in = fopen(source, "rb"), *out = in ? fopen(target, "wb") : NULL; bool ok = out != NULL; char buffer[65536];
    while (ok) { size_t count = fread(buffer, 1, sizeof(buffer), in); if (count && fwrite(buffer, 1, count, out) != count) ok = false; if (count < sizeof(buffer)) { if (ferror(in)) ok = false; break; } }
    if (in) fclose(in);
    if (out && fclose(out) != 0) ok = false;
    struct stat info; if (ok && stat(source, &info) == 0 && chmod(target, info.st_mode & 0777) != 0) ok = false;
    if (!ok) unlink(target);
    return ok;
}

static json_t *read_lock(const HhyPackageInstallOptions *options, char digest[65]) {
    json_error_t error; json_t *lock = json_load_file(lock_path(options), JSON_REJECT_DUPLICATES, &error);
    const char *target = lock ? json_string_value(json_object_get(lock, "target")) : NULL;
    const char *stored = lock ? json_string_value(json_object_get(lock, "index_sha256")) : NULL;
    if (!lock || json_integer_value(json_object_get(lock, "schema_version")) != 1 || !target ||
        !current_target() || strcmp(target, current_target()) != 0 || !stored || strlen(stored) != 64 ||
        !json_is_object(json_object_get(lock, "root")) || !json_is_object(json_object_get(lock, "index"))) {
        fputs("hhy: lockfile is invalid or targets another platform\n", stderr); json_decref(lock); return NULL;
    }
    snprintf(digest, 65, "%s", stored); return lock;
}

static RegistryPackage *select_package(Registry *registry, const char *identity, const char *constraint) {
    RegistryPackage *selected = NULL; const char *target = current_target();
    if (target == NULL) return NULL;
    for (size_t i = 0; i < registry->count; i++) {
        RegistryPackage *candidate = &registry->packages[i];
        if (strcmp(candidate->target, target) == 0 && strcmp(candidate->identity, identity) == 0 &&
            version_matches(candidate->version, constraint) &&
            (selected == NULL || compare_version(candidate->version, selected->version) > 0)) selected = candidate;
    }
    return selected;
}

static bool in_plan(Registry *registry, const char *identity, RegistryPackage **found) {
    for (size_t i = 0; i < registry->plan_count; i++)
        if (strcmp(registry->plan[i]->identity, identity) == 0) { *found = registry->plan[i]; return true; }
    return false;
}

static bool resolve(Registry *registry, const char *identity, const char *constraint,
                    const char **stack, size_t depth) {
    if (depth >= MAX_PLAN) { fputs("hhy: registry dependency graph is too deep\n", stderr); return false; }
    RegistryPackage *existing = NULL;
    if (in_plan(registry, identity, &existing)) {
        if (!version_matches(existing->version, constraint)) fputs("hhy: registry dependency version conflict\n", stderr);
        return version_matches(existing->version, constraint);
    }
    for (size_t i = 0; i < depth; i++) if (strcmp(stack[i], identity) == 0) {
        fputs("hhy: registry dependency cycle detected\n", stderr); return false;
    }
    RegistryPackage *package = select_package(registry, identity, constraint);
    if (package == NULL) {
        fprintf(stderr, "hhy: no registry package satisfies %s %s for target %s\n",
                identity, constraint, current_target() ? current_target() : "unsupported");
        return false;
    }
    stack[depth] = identity;
    json_t *dependencies = json_object_get(package->json, "dependencies");
    if (!json_is_object(dependencies)) return false;
    const char *dependency; json_t *requirement;
    json_object_foreach(dependencies, dependency, requirement) {
        const char *value = json_string_value(requirement);
        if (!safe_identity(dependency) || value == NULL || !resolve(registry, dependency, value, stack, depth + 1)) return false;
    }
    if (registry->plan_count >= MAX_PLAN) return false;
    registry->plan[registry->plan_count++] = package; return true;
}

static bool verify_package_files(const Registry *registry, const RegistryPackage *package) {
    json_t *files = json_object_get(package->json, "files");
    if (!json_is_object(files) || json_object_size(files) < 2) return false;
    const char *relative; json_t *digest;
    json_object_foreach(files, relative, digest) {
        const char *expected = json_string_value(digest); char path[PATH_MAX], actual[65];
        if (!safe_relative(relative) || expected == NULL || strlen(expected) != 64 ||
            snprintf(path, sizeof(path), "%s/%s/%s", registry->registry, package->source, relative) >= (int)sizeof(path) ||
            !sha256_file(path, actual) || strcmp(actual, expected) != 0) return false;
    }
    return true;
}

static bool load_registry(Registry *registry, const HhyPackageInstallOptions *options,
                          json_t **root_out, json_t **index_out) {
    memset(registry, 0, sizeof(*registry)); json_error_t error;
    if (options == NULL || options->registry == NULL || options->trust_root == NULL ||
        snprintf(registry->registry, sizeof(registry->registry), "%s", options->registry) >= (int)sizeof(registry->registry)) {
        fputs("hhy: signed Registry install requires --registry and --trust-root\n", stderr); return false;
    }
    json_t *root = json_load_file(options->trust_root, JSON_REJECT_DUPLICATES, &error);
    char index_path[PATH_MAX];
    if (snprintf(index_path, sizeof(index_path), "%s/index.json", registry->registry) >= (int)sizeof(index_path)) return false;
    json_t *index = json_load_file(index_path, JSON_REJECT_DUPLICATES, &error);
    const char *algorithm = root ? json_string_value(json_object_get(root, "algorithm")) : NULL;
    const char *public_key = root ? json_string_value(json_object_get(root, "public_key")) : NULL;
    const char *key_id = root ? json_string_value(json_object_get(root, "key_id")) : NULL;
    const char *index_key_id = index ? json_string_value(json_object_get(index, "key_id")) : NULL;
    unsigned char *decoded = NULL; size_t decoded_length = 0;
    bool ok = root != NULL && index != NULL && json_integer_value(json_object_get(root, "schema_version")) == 1 &&
        algorithm != NULL && strcmp(algorithm, "ed25519") == 0 && key_id != NULL && index_key_id != NULL &&
        strcmp(key_id, index_key_id) == 0 && decode_base64(public_key, &decoded, &decoded_length) &&
        decoded_length == 32 && verify_signature(index, "signature", decoded, decoded_length);
    if (!ok) {
        fputs("hhy: Registry trust root or index signature is invalid\n", stderr);
        free(decoded); json_decref(root); json_decref(index); return false;
    }
    registry->public_key = decoded; registry->public_key_length = decoded_length; registry->key_id = key_id;
    json_t *packages = json_object_get(index, "packages"); size_t count = json_array_size(packages);
    if (!json_is_array(packages) || count == 0 || count > MAX_PACKAGES) ok = false;
    for (size_t i = 0; ok && i < count; i++) {
        json_t *item = json_array_get(packages, i); RegistryPackage *package = &registry->packages[registry->count];
        package->json = item; package->identity = json_string_value(json_object_get(item, "identity"));
        package->version = json_string_value(json_object_get(item, "version"));
        package->runtime_name = json_string_value(json_object_get(item, "runtime_name"));
        package->source = json_string_value(json_object_get(item, "source"));
        package->target = json_string_value(json_object_get(item, "target")); int version[3];
        const char *package_key_id = json_string_value(json_object_get(item, "key_id"));
        if (!json_is_object(item) || !safe_identity(package->identity) || !parse_version(package->version, version) ||
            !safe_runtime_name(package->runtime_name) || !safe_relative(package->source) ||
            !safe_target(package->target) ||
            package_key_id == NULL || strcmp(package_key_id, registry->key_id) != 0 ||
            !verify_signature(item, "signature", decoded, decoded_length)) ok = false;
        else {
            for (size_t previous = 0; previous < registry->count; previous++)
                if (strcmp(registry->packages[previous].identity, package->identity) == 0 &&
                    strcmp(registry->packages[previous].version, package->version) == 0 &&
                    strcmp(registry->packages[previous].target, package->target) == 0) ok = false;
            if (ok) registry->count++;
        }
    }
    if (!ok) {
        fputs("hhy: Registry package metadata or publisher signature is invalid\n", stderr);
        free(decoded); json_decref(root); json_decref(index); return false;
    }
    *root_out = root; *index_out = index; return true;
}

int hhy_registry_install(const char *identity, const HhyPackageInstallOptions *options) {
    Registry registry; json_t *root = NULL, *index = NULL, *lock = NULL; const char *stack[MAX_PLAN];
    HhyPackageInstallOptions effective = options ? *options : (HhyPackageInstallOptions){0};
    char digest[65], cached_registry[PATH_MAX], cached_root[PATH_MAX];
    if (effective.locked) {
        lock = read_lock(&effective, digest); if (!lock) return 3;
        const char *locked_identity = json_string_value(json_object_get(lock, "identity"));
        if (!locked_identity || strcmp(identity, locked_identity) != 0) { fputs("hhy: requested package differs from lockfile\n", stderr); json_decref(lock); return 3; }
        if (effective.offline) {
            const char *cache = effective.cache ? effective.cache : ".hhy-cache";
            if (snprintf(cached_registry, sizeof(cached_registry), "%s/%s", cache, digest) >= (int)sizeof(cached_registry) ||
                snprintf(cached_root, sizeof(cached_root), "%s/root.json", cached_registry) >= (int)sizeof(cached_root)) {
                fputs("hhy: cache path is too long\n", stderr); json_decref(lock); return 3;
            }
            effective.registry = cached_registry; effective.trust_root = cached_root;
        } else {
            char index_path[PATH_MAX], actual[65];
            if (!effective.registry || snprintf(index_path, sizeof(index_path), "%s/index.json", effective.registry) >= (int)sizeof(index_path) ||
                !sha256_file(index_path, actual) || strcmp(actual, digest) != 0) {
                fputs("hhy: Registry snapshot drifted from lockfile\n", stderr); json_decref(lock); return 3;
            }
        }
    } else if (effective.offline) { fputs("hhy: --offline requires --locked\n", stderr); return 3; }
    if (!safe_identity(identity)) { fputs("hhy: Registry package identity must be namespace/name\n", stderr); return 3; }
    if (!load_registry(&registry, &effective, &root, &index)) { json_decref(lock); return 3; }
    bool ok = resolve(&registry, identity, "*", stack, 0);
    for (size_t i = 0; ok && i < registry.plan_count; i++) {
        RegistryPackage *package = registry.plan[i];
        if (!verify_package_files(&registry, package)) {
            fprintf(stderr, "hhy: package payload integrity failed for %s %s\n", package->identity, package->version);
            ok = false;
        }
    }
    if (!ok) { free((void *)registry.public_key); json_decref(root); json_decref(index); return 3; }
    puts("Verified signed installation plan:");
    for (size_t i = 0; i < registry.plan_count; i++)
        printf("  %zu. %s %s [%s] -> %s\n", i + 1, registry.plan[i]->identity,
               registry.plan[i]->version, registry.plan[i]->target, registry.plan[i]->runtime_name);
    if (effective.dry_run) { puts("Dry run: no files changed."); goto success; }
    if (!effective.assume_yes) {
        char answer[16]; fputs("Install this verified plan? [y/N] ", stdout); fflush(stdout);
        if (fgets(answer, sizeof(answer), stdin) == NULL || (answer[0] != 'y' && answer[0] != 'Y')) goto cancelled;
    }
    size_t installed = 0; HhyPackageInstallOptions local = {.assume_yes = true, .upgrade = effective.upgrade};
    for (; installed < registry.plan_count; installed++) {
        char source[PATH_MAX]; RegistryPackage *package = registry.plan[installed];
        if (snprintf(source, sizeof(source), "%s/%s", registry.registry, package->source) >= (int)sizeof(source) ||
            hhy_package_install(source, &local) != 0) break;
    }
    if (installed != registry.plan_count) {
        while (installed > 0) {
            installed--;
            if (!local.upgrade || hhy_package_rollback(registry.plan[installed]->runtime_name) != 0)
                (void)hhy_package_remove(registry.plan[installed]->runtime_name);
        }
        fputs("hhy: transaction rolled back after install failure\n", stderr);
        free((void *)registry.public_key); json_decref(root); json_decref(index); return 4;
    }
success:
    free((void *)registry.public_key); json_decref(root); json_decref(index); json_decref(lock); return 0;
cancelled:
    free((void *)registry.public_key); json_decref(root); json_decref(index); json_decref(lock); return 3;
}

int hhy_registry_lock(const char *identity, const HhyPackageInstallOptions *options) {
    Registry registry; json_t *root = NULL, *index = NULL; const char *stack[MAX_PLAN];
    if (!safe_identity(identity) || !load_registry(&registry, options, &root, &index) || !resolve(&registry, identity, "*", stack, 0)) return 3;
    for (size_t i = 0; i < registry.plan_count; i++) if (!verify_package_files(&registry, registry.plan[i])) {
        fputs("hhy: cannot lock an invalid package payload\n", stderr); free((void *)registry.public_key); json_decref(root); json_decref(index); return 3;
    }
    char index_path[PATH_MAX], digest[65], temporary[PATH_MAX]; const char *path = lock_path(options);
    if (snprintf(index_path, sizeof(index_path), "%s/index.json", registry.registry) >= (int)sizeof(index_path) || !sha256_file(index_path, digest) ||
        snprintf(temporary, sizeof(temporary), "%s.tmp-%ld", path, (long)getpid()) >= (int)sizeof(temporary)) return 4;
    json_t *lock = json_object(); json_object_set_new(lock, "schema_version", json_integer(1));
    json_object_set_new(lock, "hhy_version", json_string(HHY_VERSION)); json_object_set_new(lock, "target", json_string(current_target()));
    json_object_set_new(lock, "identity", json_string(identity)); json_object_set_new(lock, "index_sha256", json_string(digest));
    json_object_set(lock, "root", root); json_object_set(lock, "index", index);
    bool ok = json_dump_file(lock, temporary, JSON_INDENT(2) | JSON_SORT_KEYS) == 0 && rename(temporary, path) == 0;
    if (!ok) unlink(temporary); else printf("Locked %zu packages to %s (%s)\n", registry.plan_count, path, digest);
    json_decref(lock); free((void *)registry.public_key); json_decref(root); json_decref(index); return ok ? 0 : 4;
}

int hhy_registry_fetch(const HhyPackageInstallOptions *options) {
    if (!options || !options->locked || !options->registry || !options->trust_root) { fputs("hhy: fetch requires --locked, --registry, and --trust-root\n", stderr); return 3; }
    char digest[65]; json_t *lock = read_lock(options, digest); if (!lock) return 3;
    char live_index[PATH_MAX], actual[65];
    if (snprintf(live_index, sizeof(live_index), "%s/index.json", options->registry) >= (int)sizeof(live_index) || !sha256_file(live_index, actual) || strcmp(actual, digest) != 0) {
        fputs("hhy: Registry snapshot drifted from lockfile\n", stderr); json_decref(lock); return 3;
    }
    const char *cache = options->cache ? options->cache : ".hhy-cache"; char destination[PATH_MAX];
    if (snprintf(destination, sizeof(destination), "%s/%s", cache, digest) >= (int)sizeof(destination) || !make_path(destination)) { json_decref(lock); return 4; }
    json_t *index = json_object_get(lock, "index"), *root = json_object_get(lock, "root"), *packages = json_object_get(index, "packages");
    char root_path[PATH_MAX], index_path[PATH_MAX];
    bool ok = snprintf(root_path, sizeof(root_path), "%s/root.json", destination) < (int)sizeof(root_path) &&
        snprintf(index_path, sizeof(index_path), "%s/index.json", destination) < (int)sizeof(index_path) &&
        json_dump_file(root, root_path, JSON_INDENT(2) | JSON_SORT_KEYS) == 0 && copy_cached_file(live_index, index_path);
    size_t copied = 0;
    for (size_t i = 0; ok && i < json_array_size(packages); i++) {
        json_t *package = json_array_get(packages, i); const char *target = json_string_value(json_object_get(package, "target"));
        if (!target || strcmp(target, current_target()) != 0) continue;
        const char *source = json_string_value(json_object_get(package, "source")); json_t *files = json_object_get(package, "files"); const char *relative; json_t *hash;
        json_object_foreach(files, relative, hash) { char from[PATH_MAX], to[PATH_MAX], checked[65];
            if (snprintf(from, sizeof(from), "%s/%s/%s", options->registry, source, relative) >= (int)sizeof(from) ||
                snprintf(to, sizeof(to), "%s/%s/%s", destination, source, relative) >= (int)sizeof(to) || !copy_cached_file(from, to) ||
                !sha256_file(to, checked) || strcmp(checked, json_string_value(hash)) != 0) { ok = false; break; }
        } copied++;
    }
    json_decref(lock); if (!ok) { fputs("hhy: cache population failed\n", stderr); return 4; }
    printf("Cached %zu native packages under %s\n", copied, destination); return 0;
}
