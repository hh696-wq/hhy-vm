#define _POSIX_C_SOURCE 200809L
#include "hhy/extensions.h"
#include "hhy/contracts.h"
#include "hhy/package.h"

#include <jansson.h>

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

enum { MAX_PROTOCOL_MESSAGE = 1024 * 1024 };

typedef struct Extension Extension;
struct Extension {
    char *name;
    pid_t pid;
    FILE *input;
    FILE *output;
    unsigned long long next_request;
    Extension *next;
    pid_t owner;
    bool scoped;
    bool broken;
    char *receive_buffer;
    size_t receive_used;
};

static Extension *extensions = NULL;
static _Thread_local unsigned long long current_scope;
static atomic_ullong next_scope;
static pthread_mutex_t transport_mutex = PTHREAD_MUTEX_INITIALIZER;
static _Thread_local bool (*call_cancelled)(void *);
static _Thread_local void *call_context;
static _Thread_local const char *pending_request;
static _Thread_local int client_socket = -1;
void hhy_extensions_client_socket(int socket_fd) { client_socket = socket_fd; }
static bool client_disconnected(void) {
    if (client_socket < 0) return false;
    struct pollfd p = {.fd = client_socket, .events = POLLIN};
    if (poll(&p, 1, 0) <= 0) return false;
    char byte; return recv(client_socket, &byte, 1, MSG_PEEK) == 0;
}
static void scope_text(char *out, size_t size) {
    snprintf(out, size, "%ld:%llu", (long)getpid(), current_scope);
}
static _Thread_local char extension_error[256];

static void set_error(const char **error, const char *message) {
    snprintf(extension_error, sizeof(extension_error), "%s", message);
    *error = extension_error;
}

static char *copy_text(const char *text, size_t length) {
    char *copy = malloc(length + 1);
    if (copy != NULL) { memcpy(copy, text, length); copy[length] = '\0'; }
    return copy;
}

static bool copy_fixed_text(char *target, size_t capacity, const char *source) {
    size_t length = strlen(source);
    if (length >= capacity) return false;
    memcpy(target, source, length + 1);
    return true;
}

static Extension *find_extension(const char *name, size_t length) {
    for (Extension *item = extensions; item != NULL; item = item->next)
        if (strlen(item->name) == length && memcmp(item->name, name, length) == 0) return item;
    return NULL;
}

static bool manifest_command(const char *path, const char *expected_name,
                             char *command, size_t command_size, const char **error) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) { set_error(error, "installed extension has no hhy.toml"); return false; }
    char section[32] = ""; char package_name[128] = "", kind[32] = "", protocol[32] = "";
    char line[1024]; bool valid = true;
    while (fgets(line, sizeof(line), file) != NULL) {
        char *start = line; while (*start == ' ' || *start == '\t') start++;
        if (*start == '#' || *start == '\n' || *start == '\0') continue;
        if (*start == '[') {
            char *end = strchr(start, ']');
            if (end == NULL || (size_t)(end - start - 1) >= sizeof(section)) continue;
            memcpy(section, start + 1, (size_t)(end - start - 1));
            section[end - start - 1] = '\0'; continue;
        }
        char key[64], value[PATH_MAX];
        if (sscanf(start, "%63[^ =] = \"%1023[^\"]\"", key, value) != 2) continue;
        if (strcmp(section, "package") == 0 && strcmp(key, "name") == 0)
            valid = copy_fixed_text(package_name, sizeof(package_name), value) && valid;
        else if (strcmp(section, "extension") == 0 && strcmp(key, "kind") == 0)
            valid = copy_fixed_text(kind, sizeof(kind), value) && valid;
        else if (strcmp(section, "extension") == 0 && strcmp(key, "command") == 0)
            valid = copy_fixed_text(command, command_size, value) && valid;
        else if (strcmp(section, "extension") == 0 && strcmp(key, "protocol") == 0)
            valid = copy_fixed_text(protocol, sizeof(protocol), value) && valid;
    }
    fclose(file);
    if (!valid || strcmp(package_name, expected_name) != 0 || strcmp(kind, "process") != 0 ||
        strcmp(protocol, "1") != 0 || command[0] == '\0') {
        set_error(error, "extension manifest is incompatible with Protocol v1"); return false;
    }
    if (strncmp(command, "bin/", 4) != 0 || strchr(command + 4, '/') != NULL ||
        strstr(command, "..") != NULL) {
        set_error(error, "extension command must be a safe bin/ path"); return false;
    }
    return true;
}

static bool send_message(Extension *extension, json_t *message, const char **error) {
    char *encoded = json_dumps(message, JSON_COMPACT | JSON_ENSURE_ASCII | JSON_SORT_KEYS);
    if (encoded == NULL) { set_error(error, "cannot encode extension message"); return false; }
    size_t length = strlen(encoded);
    sigset_t blocked, previous, pending;
    sigemptyset(&blocked); sigaddset(&blocked, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &blocked, &previous);
    sigpending(&pending); bool already_pending = sigismember(&pending, SIGPIPE) == 1;
    bool ok = length <= MAX_PROTOCOL_MESSAGE && fwrite(encoded, 1, length, extension->input) == length &&
              fputc('\n', extension->input) != EOF && fflush(extension->input) == 0;
    if (!already_pending) {
        sigpending(&pending);
        if (sigismember(&pending, SIGPIPE) == 1) { int ignored; (void)sigwait(&blocked, &ignored); }
    }
    pthread_sigmask(SIG_SETMASK, &previous, NULL);
    free(encoded);
    if (!ok) set_error(error, "extension protocol write failed");
    return ok;
}

static json_t *receive_message(Extension *extension, const char **error) {
    if (extension->receive_buffer == NULL)
        extension->receive_buffer = malloc(MAX_PROTOCOL_MESSAGE + 2);
    if (extension->receive_buffer == NULL) { set_error(error, "out of memory"); return NULL; }
    struct timespec start; clock_gettime(CLOCK_MONOTONIC, &start);
    bool cancelled = false;
    for (;;) {
        char *end = memchr(extension->receive_buffer, '\n', extension->receive_used);
        if (end != NULL) {
            size_t length = (size_t)(end - extension->receive_buffer);
            json_error_t parse_error;
            json_t *message = json_loadb(extension->receive_buffer, length, JSON_REJECT_DUPLICATES, &parse_error);
            memmove(extension->receive_buffer, end + 1, extension->receive_used - length - 1);
            extension->receive_used -= length + 1;
            if (!json_is_object(message)) { json_decref(message); set_error(error, "extension returned invalid protocol JSON"); return NULL; }
            return message;
        }
        if (extension->receive_used > MAX_PROTOCOL_MESSAGE) { set_error(error, "extension protocol message exceeds 1 MiB"); return NULL; }
        if (!cancelled && ((call_cancelled != NULL && call_cancelled(call_context)) || client_disconnected())) {
            cancelled = true;
            if (extension->scoped && pending_request != NULL) {
                json_t *m = json_pack("{s:s,s:s,s:s,s:s,s:s}", "type", "cancel", "request_id", "cancel",
                    "extension_id", extension->name, "protocol_version", "1.0", "target", pending_request);
                (void)send_message(extension, m, error); json_decref(m);
            } else { kill(extension->pid, SIGTERM); set_error(error, "extension operation cancelled"); return NULL; }
        }
        struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec - start.tv_sec > 310) { kill(extension->pid, SIGTERM); set_error(error, "extension protocol deadline exceeded"); return NULL; }
        struct pollfd fd = {.fd = fileno(extension->output), .events = POLLIN};
        int ready = poll(&fd, 1, 25);
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0) { set_error(error, "extension protocol poll failed"); return NULL; }
        if (ready == 0) continue;
        ssize_t count = read(fd.fd, extension->receive_buffer + extension->receive_used,
            MAX_PROTOCOL_MESSAGE + 1 - extension->receive_used);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { set_error(error, "extension exited or closed its protocol stream"); return NULL; }
        extension->receive_used += (size_t)count;
    }
}

static bool message_identity(json_t *message, Extension *extension, const char *request_id,
                             const char *type, const char **error) {
    const char *actual_type = json_string_value(json_object_get(message, "type"));
    const char *actual_id = json_string_value(json_object_get(message, "request_id"));
    const char *actual_extension = json_string_value(json_object_get(message, "extension_id"));
    const char *version = json_string_value(json_object_get(message, "protocol_version"));
    if (actual_type == NULL || actual_id == NULL || actual_extension == NULL || version == NULL ||
        strcmp(actual_type, type) != 0 || strcmp(actual_id, request_id) != 0 ||
        strcmp(actual_extension, extension->name) != 0 || strcmp(version, "1.0") != 0) {
        set_error(error, "extension response identity or protocol version is invalid"); return false;
    }
    return true;
}

static bool register_contracts(Extension *extension, json_t *message, bool reuse, const char **error) {
    if (!message_identity(message, extension, "register", "register", error)) return false;
    json_t *items = json_object_get(message, "callables");
    if (!json_is_array(items) || json_array_size(items) == 0) {
        set_error(error, "extension registered no callables"); return false;
    }
    size_t index; json_t *item;
    json_array_foreach(items, index, item) {
        const char *name = json_string_value(json_object_get(item, "name"));
        const char *input = json_string_value(json_object_get(item, "input"));
        const char *output = json_string_value(json_object_get(item, "output"));
        const char *effect = json_string_value(json_object_get(item, "effect"));
        const char *threading = json_string_value(json_object_get(item, "threading"));
        json_int_t minimum = json_integer_value(json_object_get(item, "minimum_arity"));
        json_int_t maximum = json_integer_value(json_object_get(item, "maximum_arity"));
        if (name == NULL || strncmp(name, extension->name, strlen(extension->name)) != 0 ||
            name[strlen(extension->name)] != '.' || input == NULL || output == NULL ||
            effect == NULL || threading == NULL || minimum < 0 || maximum < minimum) {
            set_error(error, "extension registered an invalid callable"); return false;
        }
        HhyEffect effect_value = HHY_EFFECT_CUSTOM;
        if (strcmp(effect, "none") == 0) effect_value = HHY_EFFECT_NONE;
        else if (strcmp(effect, "filesystem") == 0) effect_value = HHY_EFFECT_FILESYSTEM;
        else if (strcmp(effect, "process") == 0) effect_value = HHY_EFFECT_PROCESS;
        else if (strcmp(effect, "network") == 0) effect_value = HHY_EFFECT_NETWORK;
        else if (strcmp(effect, "custom") != 0) { set_error(error, "unknown extension effect"); return false; }
        HhyCallableContract contract = {
            .name = name, .minimum_arity = (size_t)minimum, .maximum_arity = (size_t)maximum,
            .effect = effect_value, .lazy = json_is_true(json_object_get(item, "lazy")),
            .cancellable = json_is_true(json_object_get(item, "cancel")),
            .sendable = json_is_true(json_object_get(item, "sendable")),
            .action = json_is_true(json_object_get(item, "action")),
            .input_contract = input, .output_contract = output, .threading = threading
        };
        const HhyCallableContract *existing = hhy_contract_lookup(name);
        if (existing != NULL) {
            if (!reuse || strncmp(name, extension->name, strlen(extension->name)) != 0 ||
                existing->minimum_arity != contract.minimum_arity || existing->maximum_arity != contract.maximum_arity ||
                existing->effect != contract.effect || existing->action != contract.action ||
                existing->lazy != contract.lazy || existing->cancellable != contract.cancellable ||
                existing->sendable != contract.sendable ||
                strcmp(existing->input_contract, contract.input_contract) != 0 ||
                strcmp(existing->output_contract, contract.output_contract) != 0 ||
                strcmp(existing->threading, contract.threading) != 0) {
                set_error(error, "restarted extension changed its callable contract"); return false;
            }
        } else if (strcmp(threading, "isolated_process") != 0 ||
            !hhy_contract_register_extension(&contract, error)) return false;
    }
    return true;
}

static Extension *start_extension(const char *name, bool reuse, const char **error) {
    size_t contract_checkpoint = hhy_contract_extension_count();
    if (!hhy_package_verify(name, error)) return NULL;
    char home[PATH_MAX];
    if (!hhy_package_home(home, sizeof(home))) { set_error(error, "extension home is unavailable"); return NULL; }
    char package[PATH_MAX], manifest[PATH_MAX], relative[PATH_MAX], executable[PATH_MAX];
    if (snprintf(package, sizeof(package), "%s/%s", home, name) >= (int)sizeof(package) ||
        snprintf(manifest, sizeof(manifest), "%s/hhy.toml", package) >= (int)sizeof(manifest) ||
        !manifest_command(manifest, name, relative, sizeof(relative), error) ||
        snprintf(executable, sizeof(executable), "%s/%s", package, relative) >= (int)sizeof(executable)) return NULL;
    struct stat info;
    if (stat(executable, &info) != 0 || !S_ISREG(info.st_mode) || access(executable, X_OK) != 0) {
        set_error(error, "extension executable is missing or not executable"); return NULL;
    }
    int to_child[2], from_child[2];
    if (pipe(to_child) != 0 || pipe(from_child) != 0) { set_error(error, "cannot create extension pipes"); return NULL; }
    pid_t pid = fork();
    if (pid < 0) { close(to_child[0]); close(to_child[1]); close(from_child[0]); close(from_child[1]);
        set_error(error, "cannot start extension process"); return NULL; }
    if (pid == 0) {
        dup2(to_child[0], STDIN_FILENO); dup2(from_child[1], STDOUT_FILENO);
        close(to_child[0]); close(to_child[1]); close(from_child[0]); close(from_child[1]);
        char *const arguments[] = {executable, "--protocol", "1", NULL};
        char extension_path[PATH_MAX + 32];
        if (snprintf(extension_path, sizeof(extension_path), "PATH=%s/lib:/usr/bin:/bin", package) >=
            (int)sizeof(extension_path)) _exit(127);
        char *const environment[] = {extension_path, NULL};
        execve(executable, arguments, environment); _exit(127);
    }
    close(to_child[0]); close(from_child[1]);
    Extension *extension = calloc(1, sizeof(*extension));
    if (extension == NULL) { kill(pid, SIGTERM); close(to_child[1]); close(from_child[0]);
        set_error(error, "out of memory"); return NULL; }
    extension->name = copy_text(name, strlen(name)); extension->pid = pid; extension->owner = getpid(); extension->next_request = 1;
    extension->input = fdopen(to_child[1], "w"); extension->output = fdopen(from_child[0], "r");
    if (extension->name == NULL || extension->input == NULL || extension->output == NULL) {
        set_error(error, "cannot open extension protocol streams"); goto fail;
    }
    setvbuf(extension->input, NULL, _IONBF, 0);
    json_t *hello = json_pack("{s:s,s:s,s:s,s:s,s:s}", "type", "handshake",
        "request_id", "handshake", "extension_id", name, "protocol_version", "1.0",
        "runtime_version", HHY_VERSION);
    bool sent = send_message(extension, hello, error); json_decref(hello);
    if (!sent) goto fail;
    json_t *response = receive_message(extension, error);
    bool valid = response != NULL && message_identity(response, extension, "handshake",
                                                       "handshake_result", error);
    if (response != NULL) {
        extension->scoped = json_is_true(json_object_get(response, "scoped_resources"));
        json_decref(response);
    }
    if (!valid) goto fail;
    response = receive_message(extension, error);
    valid = response != NULL && register_contracts(extension, response, reuse, error);
    if (response != NULL) json_decref(response);
    if (!valid) goto fail;
    extension->next = extensions; extensions = extension; return extension;
fail:
    hhy_contract_rollback_extensions(contract_checkpoint);
    if (extension->input != NULL) fclose(extension->input); else close(to_child[1]);
    if (extension->output != NULL) fclose(extension->output); else close(from_child[0]);
    kill(pid, SIGTERM); waitpid(pid, NULL, 0); free(extension->receive_buffer); free(extension->name); free(extension); return NULL;
}

static bool prepare_namespace(const char *name, size_t length, const char **error) {
    if (length == 0 || (length >= 4 && (memcmp(name, "hhy.", 4) == 0 || memcmp(name, "std.", 4) == 0))) {
        set_error(error, "extension namespace is reserved"); return false;
    }
    if (find_extension(name, length) != NULL) return true;
    char *copy = copy_text(name, length);
    if (copy == NULL) { set_error(error, "out of memory"); return false; }
    Extension *started = start_extension(copy, false, error); free(copy); return started != NULL;
}

bool hhy_extension_prepare_namespace(const char *name, size_t length, const char **error) {
    pthread_mutex_lock(&transport_mutex);
    bool result = prepare_namespace(name, length, error);
    pthread_mutex_unlock(&transport_mutex); return result;
}
bool hhy_extension_owns_callable(const char *name) {
    const char *dot = strchr(name, '.');
    pthread_mutex_lock(&transport_mutex);
    const HhyCallableContract *contract = hhy_contract_lookup(name);
    bool result = dot != NULL && contract != NULL && strcmp(contract->threading, "isolated_process") == 0;
    pthread_mutex_unlock(&transport_mutex); return result;
}

static json_t *extension_call(const char *name, json_t *arguments, HhyExtensionError *call_error) {
    static _Thread_local char kind_buffer[64], code_buffer[96], message_buffer[256];
    static _Thread_local char operation_buffer[128], stage_buffer[128], cause_buffer[128];
    const char *error = NULL;
    call_error->kind = "ExtensionCrashedError"; call_error->code = "HHY_EXTENSION_PROTOCOL";
    call_error->message = "extension protocol failure";
    call_error->operation = name; call_error->stage = "extension.protocol"; call_error->cause = NULL;
    const char *dot = strchr(name, '.'); Extension *extension = dot ? find_extension(name, (size_t)(dot - name)) : NULL;
    if (extension == NULL && dot != NULL) {
        char *namespace_name = copy_text(name, (size_t)(dot-name));
        if (namespace_name != NULL) extension = start_extension(namespace_name, true, &error);
        free(namespace_name);
    }
    if (extension == NULL) {
        call_error->message = error ? error : "extension callable is not loaded"; return NULL;
    }
    bool child_ended = false;
    if (extension->owner == getpid()) {
        pid_t status = waitpid(extension->pid, NULL, WNOHANG);
        child_ended = status == extension->pid || (status < 0 && errno == ECHILD);
    }
    if (extension->owner != getpid() || extension->broken || child_ended) {
        Extension **link = &extensions;
        while (*link != extension) link = &(*link)->next;
        *link = extension->next;
        char *namespace_name = copy_text(extension->name, strlen(extension->name));
        fclose(extension->input); fclose(extension->output);
        if (extension->owner == getpid() && !child_ended) {
            kill(extension->pid, SIGTERM); while (waitpid(extension->pid, NULL, 0) < 0 && errno == EINTR) {}
        }
        free(extension->receive_buffer); free(extension->name); free(extension);
        extension = namespace_name == NULL ? NULL : start_extension(namespace_name, true, &error);
        free(namespace_name);
        if (extension == NULL) { call_error->message = error ? error : "cannot restart extension after fork"; return NULL; }
    }
    char request_id[32]; snprintf(request_id, sizeof(request_id), "%llu", extension->next_request++);
    json_t *request = json_pack("{s:s,s:s,s:s,s:s,s:s,s:O}", "type", "call",
        "request_id", request_id, "extension_id", extension->name, "protocol_version", "1.0",
        "callable", name, "arguments", arguments);
    if (extension->scoped) {
        char scope[96]; scope_text(scope, sizeof(scope));
        json_object_set_new(request, "scope", json_string(scope));
    }
    bool sent = send_message(extension, request, &error); json_decref(request);
    if (!sent) { extension->broken = true; call_error->message = error; return NULL; }
    pending_request = request_id;
    json_t *response = receive_message(extension, &error);
    pending_request = NULL;
    if (response == NULL) { extension->broken = true; call_error->message = error; return NULL; }
    const char *type = json_string_value(json_object_get(response, "type"));
    if (type != NULL && strcmp(type, "error") == 0 &&
        message_identity(response, extension, request_id, "error", &error)) {
        const char *kind = json_string_value(json_object_get(response, "kind"));
        const char *code = json_string_value(json_object_get(response, "code"));
        const char *message = json_string_value(json_object_get(response, "message"));
        const char *operation = json_string_value(json_object_get(response, "operation"));
        const char *stage = json_string_value(json_object_get(response, "stage"));
        const char *cause = json_string_value(json_object_get(response, "cause"));
        snprintf(kind_buffer, sizeof(kind_buffer), "%s", kind ? kind : "ExtensionError");
        snprintf(code_buffer, sizeof(code_buffer), "%s", code ? code : "HHY_EXTENSION_CALL");
        snprintf(message_buffer, sizeof(message_buffer), "%s", message ? message : "extension call failed");
        snprintf(operation_buffer, sizeof(operation_buffer), "%s", operation ? operation : name);
        snprintf(stage_buffer, sizeof(stage_buffer), "%s", stage ? stage : "extension.call");
        if (cause != NULL) snprintf(cause_buffer, sizeof(cause_buffer), "%s", cause);
        call_error->kind = kind_buffer; call_error->code = code_buffer; call_error->message = message_buffer;
        call_error->operation = operation_buffer; call_error->stage = stage_buffer;
        call_error->cause = cause != NULL ? cause_buffer : NULL;
        json_decref(response); return NULL;
    }
    if (!message_identity(response, extension, request_id, "call_result", &error)) {
        extension->broken = true;
        call_error->message = error;
        json_decref(response); return NULL;
    }
    json_t *value = json_object_get(response, "value");
    if (value == NULL) { set_error(&error, "extension result has no value"); call_error->message = error;
        json_decref(response); return NULL; }
    json_incref(value); json_decref(response); return value;
}

json_t *hhy_extension_call(const char *name, json_t *arguments, HhyExtensionError *error) {
    pthread_mutex_lock(&transport_mutex);
    json_t *result = extension_call(name, arguments, error);
    pthread_mutex_unlock(&transport_mutex); return result;
}
json_t *hhy_extension_call_checked(const char *name, json_t *arguments,
                                    HhyExtensionError *error,
                                    bool (*cancelled)(void *), void *context) {
    pthread_mutex_lock(&transport_mutex);
    call_cancelled = cancelled; call_context = context;
    json_t *result = extension_call(name, arguments, error);
    call_cancelled = NULL; call_context = NULL;
    pthread_mutex_unlock(&transport_mutex); return result;
}
unsigned long long hhy_extensions_scope_begin(void) {
    unsigned long long previous = current_scope; current_scope = atomic_fetch_add(&next_scope, 1) + 1; return previous;
}
void hhy_extensions_scope_end(unsigned long long previous) {
    pthread_mutex_lock(&transport_mutex);
    char scope[96]; scope_text(scope, sizeof(scope));
    for (Extension *e = extensions; e != NULL; e = e->next) {
        if (!e->scoped || e->owner != getpid() || e->broken) continue;
        json_t *m = json_pack("{s:s,s:s,s:s,s:s,s:s}", "type", "scope_end", "request_id", "scope_end",
            "extension_id", e->name, "protocol_version", "1.0", "scope", scope);
        const char *ignored = NULL;
        if (send_message(e, m, &ignored)) {
            json_t *r = receive_message(e, &ignored);
            if (r == NULL) e->broken = true;
            json_decref(r);
        } else e->broken = true;
        json_decref(m);
    }
    current_scope = previous;
    pthread_mutex_unlock(&transport_mutex);
}
void hhy_extensions_shutdown(void) {
    pthread_mutex_lock(&transport_mutex);
    while (extensions != NULL) {
        Extension *extension = extensions; extensions = extension->next;
        if (extension->owner != getpid()) {
            fclose(extension->input); fclose(extension->output);
            free(extension->receive_buffer); free(extension->name); free(extension); continue;
        }
        json_t *message = json_pack("{s:s,s:s,s:s,s:s}", "type", "shutdown",
            "request_id", "shutdown", "extension_id", extension->name, "protocol_version", "1.0");
        const char *ignored = NULL; (void)send_message(extension, message, &ignored); json_decref(message);
        fclose(extension->input); fclose(extension->output);
        int status; if (waitpid(extension->pid, &status, WNOHANG) == 0) {
            kill(extension->pid, SIGTERM); waitpid(extension->pid, &status, 0);
        }
        free(extension->receive_buffer); free(extension->name); free(extension);
    }
    hhy_contract_clear_extensions();
    pthread_mutex_unlock(&transport_mutex);
}
