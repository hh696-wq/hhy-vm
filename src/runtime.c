#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "hhy/runtime.h"
#include "bytecode_runtime.h"
#include "runtime_ownership.h"
#include "hhy/contracts.h"
#include "hhy/extensions.h"
#include "hhy/parser.h"
#include "hhy/platform_watch.h"
#include "hhy/profiler.h"
#include "hhy/fuzz.h"
#include <jansson.h>

#include <errno.h>
#include <ctype.h>
#include <curl/curl.h>
#include <fts.h>
#include <fnmatch.h>
#include <inttypes.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <gc/gc.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#include <sys/stdio.h>
#elif defined(__linux__)
#include <fcntl.h>
#endif
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

extern char **environ;

enum {
    HHY_MAX_TEXT_OUTPUT = 256 * 1024 * 1024,
    HHY_MAX_COLLECTION_ITEMS = 1000000
};

typedef HhyRuntimeLimits RuntimeLimits;

typedef enum {
    V_NULL, V_BOOL, V_INT, V_FLOAT, V_STRING, V_REGEX, V_LIST, V_RANGE, V_MAP, V_FUNCTION,
    V_STREAM, V_PATH, V_BYTES, V_DURATION, V_PERCENT,
    V_RESULT, V_FILE, V_DIRECTORY, V_FILE_EVENT, V_PROCESS, V_COMMAND_RESULT,
    V_HTTP_REQUEST, V_HTTP_RESPONSE, V_BYTES_BUFFER, V_ERROR, V_DATETIME
} ValueKind;

typedef struct Value Value;
typedef struct Env Env;
typedef struct Stream Stream;
typedef struct Module Module;
typedef struct WatchItem WatchItem;
typedef struct Runtime Runtime;
typedef struct RuntimeCleanup RuntimeCleanup;
struct RuntimeCleanup {
    FILE *file;
    char *temporary_path;
    RuntimeCleanup *next;
};
struct WatchItem {
    char *path;
    uint64_t modified_ns;
    off_t size;
    dev_t device;
    ino_t inode;
};
typedef struct { pid_t pid; FILE *file; bool active; } ParallelJob;
typedef struct { size_t count; Value *items; } List;
typedef struct MapStorage {
    size_t count;
    char **keys;
    size_t *key_lengths;
    Value *values;
    size_t *slots;
    size_t slot_count;
    uint64_t index_magic;
} MapStorage;
typedef struct {
    const char *builtin;
    const HhyNode *node;
    Env *closure;
    const HhySource *source;
    bool is_closure;
    bool is_bytecode;
} Function;

typedef struct {
    const HhyBytecodeChunk *chunk;
    size_t instruction;
    size_t fast_expression;
    uint32_t parameter_constant;
    const HhyStreamKernel *stream_kernel;
    bool has_fast_argument_expression;
} BytecodeFunctionTarget;

typedef enum {
    BYTECODE_SPECIALIZATION_SELECTED = 0,
    BYTECODE_SPECIALIZATION_DISABLED,
    BYTECODE_SPECIALIZATION_NOT_BYTECODE,
    BYTECODE_SPECIALIZATION_UNSUPPORTED_STREAM,
    BYTECODE_SPECIALIZATION_TOO_MANY_STAGES,
    BYTECODE_SPECIALIZATION_DYNAMIC_CALLABLE,
    BYTECODE_SPECIALIZATION_UNSUPPORTED_EXPRESSION,
    BYTECODE_SPECIALIZATION_UNSUPPORTED_SOURCE,
    BYTECODE_SPECIALIZATION_RUNTIME_FALLBACK
} BytecodeSpecializationReason;

struct Value {
    ValueKind kind;
    size_t string_length;
    union {
        bool boolean;
        int64_t integer;
        double number;
        char *string;
        struct { char *pattern; uint32_t flags; } regex;
        List list;
        struct { int64_t start; int64_t end; } range;
        MapStorage *map;
        Function function;
        Stream *stream;
        struct { unsigned char *data; size_t length; } bytes_buffer;
        struct { int64_t nanoseconds; int32_t offset_minutes; } datetime;
    } as;
};

_Static_assert(sizeof(Value) <= 56, "Value layout regression: Map storage must remain indirect");

typedef enum {
    STREAM_LIST, STREAM_RANGE, STREAM_MAP, STREAM_WHERE, STREAM_TAKE,
    STREAM_SKIP, STREAM_FLAT_MAP, STREAM_INSPECT, STREAM_DISTINCT, STREAM_SORT, STREAM_GROUP,
    STREAM_CSV_PARSE, STREAM_CSV_ENCODE, STREAM_ON_ERROR, STREAM_PARALLEL, STREAM_DEBOUNCE,
    STREAM_EVERY, STREAM_WATCH, STREAM_PROCESSES,
    STREAM_FILE_LINES, STREAM_FILES
} StreamKind;
struct Stream {
    StreamKind kind;
    bool claimed;
    bool closed;
    size_t index;
    size_t remaining;
    Value source;
    Value function;
    Env *env;
    FILE *file;
    bool owns_file;
    char *path;
    char *line;
    size_t line_capacity;
    FTS *fts;
    bool fts_counted;
    char **fts_paths;
    char *pattern;
    Value inner;
    Value materialized;
    bool initialized;
    bool descending;
    Value *seen;
    size_t *seen_slots;
    size_t seen_slot_count;
    uint64_t *seen_times;
    size_t seen_count;
    size_t seen_capacity;
    Value csv_headers;
    bool csv_header_emitted;
    bool csv_use_header;
    char delimiter;
    char quote;
    double interval_ns;
    WatchItem *watch_items;
    size_t watch_count;
    HhyPlatformWatch *platform_watch;
    size_t platform_watch_handles;
    Value pending_events;
    size_t pending_index;
    bool recursive;
    bool follow_symlinks;
    ParallelJob *jobs;
    size_t job_count;
    size_t max_parallel;
    bool upstream_done;
    Runtime *runtime;
    Stream *runtime_next;
};

typedef struct { const char *name; size_t name_length; Value value; bool mutable; } Binding;
struct Env {
    Env *parent;
    Binding *items;
    size_t count;
    size_t capacity;
    bool escaped;
    Env *free_next;
};
struct Module {
    char *path;
    HhySource source;
    HhyTokenList tokens;
    HhyNode *program;
    HhyPreparedBytecode *bytecode;
    Env *environment;
    Value exports;
    bool loading;
    Module *next;
};
typedef enum { SIGNAL_NONE, SIGNAL_RETURN, SIGNAL_BREAK, SIGNAL_CONTINUE, SIGNAL_EXIT } Signal;
typedef struct {
    const char *name;
    size_t name_length;
    const char *path;
    uint32_t line;
    uint32_t column;
} RuntimeStackFrame;

#define HHY_RUNTIME_STACK_TRACE_LIMIT 128u

struct Runtime {
    const HhySource *source;
    HhyExecutionEngine engine;
    bool failed;
    int exit_code;
    Signal signal;
    Value signal_value;
    Value error_value;
    uint32_t error_line;
    uint32_t error_column;
    Env *core;
    Env *free_call_frames;
    Module *modules;
    bool dry_run;
    bool effect_allowed;
    bool cancelled;
    size_t call_depth;
    RuntimeStackFrame call_stack[HHY_RUNTIME_STACK_TRACE_LIMIT];
    size_t call_stack_count;
    RuntimeLimits limits;
    struct timespec started_at;
    size_t active_processes;
    size_t active_open_files;
    Stream *streams;
    jmp_buf memory_jump;
    bool memory_jump_ready;
    bool gc_stress;
    size_t memory_baseline;
    size_t memory_check_budget;
    size_t memory_observed_local;
    uint32_t safepoint_ticks;
    char *emergency_error_keys[8];
    size_t emergency_error_key_lengths[8];
    Value emergency_error_values[8];
    MapStorage emergency_error_map;
    RuntimeCleanup *cleanups;
    const HhyCallableContract *current_contract;
    HhyProfiler *profiler;
};

static volatile sig_atomic_t hhy_interrupt_requested = 0;
static _Thread_local const HhyBytecodeChunk *hhy_active_bytecode_chunk = NULL;

static void hhy_signal_handler(int signal_number) {
    (void)signal_number;
    hhy_interrupt_requested = 1;
}

static Value null_value(void) { Value v = {.kind = V_NULL}; return v; }
static Value bool_value(bool b) { Value v = {.kind = V_BOOL}; v.as.boolean = b; return v; }
static Value int_value(int64_t n) { Value v = {.kind = V_INT}; v.as.integer = n; return v; }
static Value float_value(double n) { Value v = {.kind = V_FLOAT}; v.as.number = n; return v; }
static void *rt_alloc(Runtime *rt, size_t size);
static MapStorage *map_storage_new(Runtime *rt, size_t count);
static void map_build_index(Runtime *rt, MapStorage *map);
static char *rt_strndup(Runtime *rt, const char *text, size_t length);
static Value string_n(Runtime *rt, const char *text, size_t length);
static Value list_new(Runtime *rt, size_t count);
static void runtime_type_error(Runtime *rt, const HhyNode *node, const char *message);
static void runtime_value_error(Runtime *rt, const HhyNode *node, const char *message);

static json_t *value_to_protocol_json(Runtime *rt, const HhyNode *site, Value value) {
    switch (value.kind) {
        case V_NULL: return json_null();
        case V_BOOL: return json_boolean(value.as.boolean);
        case V_INT: return json_integer(value.as.integer);
        case V_FLOAT: return json_real(value.as.number);
        case V_STRING: return json_stringn(value.as.string, value.string_length);
        case V_LIST: {
            json_t *array = json_array();
            for (size_t i = 0; i < value.as.list.count; i++) {
                json_t *item = value_to_protocol_json(rt, site, value.as.list.items[i]);
                if (item == NULL || json_array_append_new(array, item) != 0) {
                    if (item != NULL) json_decref(item);
                    json_decref(array);
                    return NULL;
                }
            }
            return array;
        }
        case V_MAP: {
            json_t *object = json_object();
            for (size_t i = 0; i < value.as.map->count; i++) {
                char *key = hhy_strndup(value.as.map->keys[i], value.as.map->key_lengths[i]);
                json_t *item = value_to_protocol_json(rt, site, value.as.map->values[i]);
                int result = item == NULL ? -1 : json_object_set_new(object, key, item);
                free(key);
                if (result != 0) {
                    if (item != NULL) json_decref(item);
                    json_decref(object);
                    return NULL;
                }
            }
            return object;
        }
        default:
            runtime_type_error(rt, site, "extension arguments must be protocol-serializable values");
            return NULL;
    }
}

static Value protocol_json_to_value(Runtime *rt, const HhyNode *site, json_t *value) {
    if (json_is_null(value)) return null_value();
    if (json_is_boolean(value)) return bool_value(json_is_true(value));
    if (json_is_integer(value)) return int_value(json_integer_value(value));
    if (json_is_real(value)) return float_value(json_real_value(value));
    if (json_is_string(value)) return string_n(rt, json_string_value(value), json_string_length(value));
    if (json_is_array(value)) {
        size_t count = json_array_size(value); Value result = list_new(rt, count);
        for (size_t i = 0; i < count; i++)
            result.as.list.items[i] = protocol_json_to_value(rt, site, json_array_get(value, i));
        return result;
    }
    if (json_is_object(value)) {
        size_t count = json_object_size(value); Value result = {.kind = V_MAP};
        result.as.map = map_storage_new(rt, count);
        result.as.map->count = count; result.as.map->keys = count ? rt_alloc(rt, count * sizeof(char *)) : NULL;
        result.as.map->key_lengths = count ? rt_alloc(rt, count * sizeof(size_t)) : NULL;
        result.as.map->values = count ? rt_alloc(rt, count * sizeof(Value)) : NULL;
        const char *key; json_t *item; size_t index = 0;
        json_object_foreach(value, key, item) {
            result.as.map->key_lengths[index] = strlen(key);
            result.as.map->keys[index] = rt_strndup(rt, key, result.as.map->key_lengths[index]);
            result.as.map->values[index] = protocol_json_to_value(rt, site, item); index++;
        }
        map_build_index(rt, result.as.map);
        return result;
    }
    runtime_value_error(rt, site, "extension returned an unsupported protocol value");
    return null_value();
}
static void runtime_error_kind(Runtime *rt, const HhyNode *node, const char *kind,
                               const char *code, const char *message);

static void runtime_memory_limit(Runtime *rt) {
    static const char *names[] = {
        "kind", "code", "message", "source", "stage", "cause", "stack", "context"
    };
    rt->failed = true;
    rt->exit_code = 1;
    rt->error_value.kind = V_ERROR;
    rt->error_value.as.map = &rt->emergency_error_map;
    rt->error_value.as.map->count = 8;
    rt->error_value.as.map->keys = rt->emergency_error_keys;
    rt->error_value.as.map->key_lengths = rt->emergency_error_key_lengths;
    rt->error_value.as.map->values = rt->emergency_error_values;
    for (size_t i = 0; i < 8; i++) {
        rt->emergency_error_keys[i] = (char *)names[i];
        rt->emergency_error_key_lengths[i] = strlen(names[i]);
    }
    rt->emergency_error_values[0] = (Value){.kind = V_STRING, .as.string = "ResourceLimitError"};
    rt->emergency_error_values[1] = (Value){.kind = V_STRING, .as.string = "HHY_MEMORY_LIMIT"};
    rt->emergency_error_values[2] = (Value){.kind = V_STRING,
        .as.string = "operation exceeds RuntimeLimits.max_memory"};
    rt->emergency_error_values[3] = (Value){.kind = V_STRING,
        .as.string = (char *)(rt->source == NULL ? "<runtime>" : rt->source->path)};
    rt->emergency_error_values[4] = (Value){.kind = V_STRING, .as.string = "Runtime"};
    for (size_t i = 0; i < 5; i++)
        rt->emergency_error_values[i].string_length = strlen(rt->emergency_error_values[i].as.string);
    rt->emergency_error_values[5] = null_value();
    rt->emergency_error_values[6] = null_value();
    rt->emergency_error_values[7] = null_value();
    rt->error_line = 0;
    rt->error_column = 0;
    if (rt->memory_jump_ready) longjmp(rt->memory_jump, 1);
    fputs("hhy: managed heap exceeds RuntimeLimits.max_memory\n", stderr);
    exit(70);
}

static bool runtime_memory_available(Runtime *rt, size_t size) {
    if (size > rt->limits.max_memory) return false;
    if (size <= rt->memory_check_budget) {
        rt->memory_check_budget -= size;
        if (rt->memory_observed_local <= SIZE_MAX - size) rt->memory_observed_local += size;
        return true;
    }
    size_t used = GC_get_memory_use();
    size_t local = used > rt->memory_baseline ? used - rt->memory_baseline : 0;
    if (local > rt->limits.max_memory - size) {
        GC_gcollect();
        used = GC_get_memory_use();
        local = used > rt->memory_baseline ? used - rt->memory_baseline : 0;
        if (local > rt->limits.max_memory - size) return false;
    }
    size_t available = rt->limits.max_memory - local - size;
    const size_t check_step = 256 * 1024;
    rt->memory_check_budget = available < check_step ? available : check_step;
    rt->memory_observed_local = local + size;
    return true;
}

static HHY_MANAGED_SCANNED void *rt_alloc(Runtime *rt, size_t size) {
    size_t requested = size == 0 ? 1 : size;
    if (rt->gc_stress) GC_gcollect();
    if (!runtime_memory_available(rt, requested)) runtime_memory_limit(rt);
    void *pointer = GC_malloc(requested);
    if (pointer == NULL) runtime_memory_limit(rt);
    if (hhy_profiler_tracks_heap(rt->profiler))
        hhy_profiler_allocation(rt->profiler, requested,
                                rt->memory_baseline + rt->memory_observed_local);
    memset(pointer, 0, size);
    return pointer;
}

static MapStorage *map_storage_new(Runtime *rt, size_t count) {
    MapStorage *map = rt_alloc(rt, sizeof(*map));
    map->count = count;
    return map;
}

static HHY_MANAGED_ATOMIC void *rt_alloc_atomic(Runtime *rt, size_t size) {
    size_t requested = size == 0 ? 1 : size;
    if (!runtime_memory_available(rt, requested)) runtime_memory_limit(rt);
    void *pointer = GC_malloc_atomic(requested);
    if (pointer == NULL) runtime_memory_limit(rt);
    if (hhy_profiler_tracks_heap(rt->profiler))
        hhy_profiler_allocation(rt->profiler, requested,
                                rt->memory_baseline + rt->memory_observed_local);
    memset(pointer, 0, size);
    return pointer;
}

/* Managed references must live in scanned storage. These named helpers make
 * the ownership rule explicit at every grow site. */
static HHY_MANAGED_SCANNED void *rt_scanned_array_grow(Runtime *rt, HHY_BORROWED const void *old, size_t old_count,
                                   size_t new_count, size_t item_size) {
    if (new_count > SIZE_MAX / item_size) runtime_memory_limit(rt);
    void *result = rt_alloc(rt, new_count * item_size);
    if (old != NULL && old_count != 0) memcpy(result, old, old_count * item_size);
    return result;
}

static Value *rt_value_array_grow(Runtime *rt, Value *old, size_t old_count,
                                  size_t new_count) {
    return rt_scanned_array_grow(rt, old, old_count, new_count, sizeof(Value));
}

static char **rt_pointer_array_grow(Runtime *rt, char **old, size_t old_count,
                                    size_t new_count) {
    return rt_scanned_array_grow(rt, old, old_count, new_count, sizeof(char *));
}

static Value bytes_buffer_value(Runtime *rt, const void *data, size_t length) {
    Value value = {.kind = V_BYTES_BUFFER};
    value.as.bytes_buffer.length = length;
    value.as.bytes_buffer.data = length ? rt_alloc_atomic(rt, length) : NULL;
    if (length && data != NULL) memcpy(value.as.bytes_buffer.data, data, length);
    return value;
}

static char *rt_strndup(Runtime *rt, const char *text, size_t length) {
    char *copy = rt_alloc_atomic(rt, length + 1);
    memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

static Value string_n(Runtime *rt, const char *text, size_t length) {
    Value v = {.kind = V_STRING, .string_length = length};
    v.as.string = rt_strndup(rt, text, length);
    return v;
}

static Value string_value(Runtime *rt, const char *text) { return string_n(rt, text, strlen(text)); }

static Value path_value_normalized(Runtime *rt, const char *input) {
    bool absolute = input[0] == '/';
    size_t length = strlen(input), count = 0;
    char *copy = hhy_strndup(input, length);
    char **parts = hhy_alloc((length + 1) * sizeof(char *));
    char *cursor = copy;
    while (*cursor) {
        while (*cursor == '/') cursor++;
        if (!*cursor) break;
        char *part = cursor;
        while (*cursor && *cursor != '/') cursor++;
        if (*cursor) *cursor++ = '\0';
        if (strcmp(part, ".") == 0 || *part == '\0') continue;
        if (strcmp(part, "..") == 0) {
            if (count > 0 && strcmp(parts[count - 1], "..") != 0) { count--; continue; }
            if (absolute) continue;
        }
        parts[count++] = part;
    }
    size_t output_length = absolute ? 1 : 0;
    for (size_t i = 0; i < count; i++) output_length += strlen(parts[i]) + (i > 0 ? 1 : 0);
    if (!absolute && count == 0) output_length = 1;
    char *output = rt_alloc(rt, output_length + 1), *out = output;
    if (absolute) *out++ = '/';
    for (size_t i = 0; i < count; i++) {
        if (i > 0) *out++ = '/';
        size_t part_length = strlen(parts[i]);
        memcpy(out, parts[i], part_length); out += part_length;
    }
    if (!absolute && count == 0) *out++ = '.';
    *out = '\0';
    free(parts); free(copy);
    Value value = {.kind = V_PATH, .string_length = output_length}; value.as.string = output; return value;
}

static bool utf8_count(const char *text, size_t length, size_t *count) {
    const unsigned char *bytes = (const unsigned char *)text;
    size_t offset = 0, total = 0;
    while (offset < length) {
        unsigned char first = bytes[offset];
        size_t width;
        if (first <= 0x7f) width = 1;
        else if (first >= 0xc2 && first <= 0xdf) width = 2;
        else if (first >= 0xe0 && first <= 0xef) width = 3;
        else if (first >= 0xf0 && first <= 0xf4) width = 4;
        else return false;
        if (offset + width > length) return false;
        for (size_t i = 1; i < width; i++)
            if ((bytes[offset + i] & 0xc0) != 0x80) return false;
        if ((first == 0xe0 && bytes[offset + 1] < 0xa0) ||
            (first == 0xed && bytes[offset + 1] >= 0xa0) ||
            (first == 0xf0 && bytes[offset + 1] < 0x90) ||
            (first == 0xf4 && bytes[offset + 1] >= 0x90)) return false;
        offset += width;
        total++;
    }
    *count = total;
    return true;
}

static Value utf8_case(Runtime *rt, const HhyNode *site, const char *text,
                       size_t input_length, bool upper) {
    mbstate_t input_state = {0}, output_state = {0};
    size_t input_offset = 0, output_length = 0;
    size_t capacity = input_length + 16;
    char *output = rt_alloc(rt, capacity);
    while (input_offset < input_length) {
        wchar_t codepoint;
        size_t consumed = mbrtowc(&codepoint, text + input_offset,
                                  input_length - input_offset, &input_state);
        if (consumed == (size_t)-1 || consumed == (size_t)-2) {
            runtime_error_kind(rt, site, "EncodingError", "HHY_UTF8", "String is not valid UTF-8");
            return null_value();
        }
        if (consumed == 0) consumed = 1;
        codepoint = upper ? towupper(codepoint) : towlower(codepoint);
        char encoded[MB_LEN_MAX];
        size_t produced = wcrtomb(encoded, codepoint, &output_state);
        if (produced == (size_t)-1) {
            runtime_error_kind(rt, site, "EncodingError", "HHY_UTF8_CASE",
                               "Unicode case conversion failed"); return null_value();
        }
        if (output_length + produced + 1 > capacity) {
            size_t next = capacity * 2;
            while (next < output_length + produced + 1) next *= 2;
            char *grown = rt_alloc(rt, next);
            memcpy(grown, output, output_length); output = grown; capacity = next;
        }
        memcpy(output + output_length, encoded, produced);
        output_length += produced; input_offset += consumed;
    }
    output[output_length] = '\0';
    Value result = {.kind = V_STRING, .string_length = output_length}; result.as.string = output; return result;
}

static void runtime_error_kind(Runtime *rt, const HhyNode *node, const char *kind,
                               const char *code, const char *message) {
    if (rt->failed) return;
    rt->failed = true;
    if (rt->exit_code == 0) {
        if (strcmp(kind, "IoError") == 0 || strcmp(kind, "ProcessError") == 0 ||
            strcmp(kind, "HttpError") == 0 || strcmp(kind, "HttpStatusError") == 0)
            rt->exit_code = 4;
        else if (strcmp(kind, "TimeoutError") == 0 || strcmp(kind, "CancelledError") == 0)
            rt->exit_code = 5;
        else rt->exit_code = 1;
    }
    const char *names[] = {"kind", "code", "message", "source", "stage", "cause", "stack", "context"};
    rt->error_value.kind = V_ERROR;
    rt->error_value.as.map = map_storage_new(rt, 8);
    rt->error_value.as.map->count = 8;
    rt->error_value.as.map->keys = rt_alloc(rt, 8 * sizeof(char *));
    rt->error_value.as.map->key_lengths = rt_alloc(rt, 8 * sizeof(size_t));
    rt->error_value.as.map->values = rt_alloc(rt, 8 * sizeof(Value));
    for (size_t i = 0; i < 8; i++) {
        rt->error_value.as.map->keys[i] = rt_strndup(rt, names[i], strlen(names[i]));
        rt->error_value.as.map->key_lengths[i] = strlen(names[i]);
    }
    rt->error_value.as.map->values[0] = string_value(rt, kind);
    rt->error_value.as.map->values[1] = string_value(rt, code);
    rt->error_value.as.map->values[2] = string_value(rt, message);
    rt->error_value.as.map->values[3] = string_value(rt, rt->source ? rt->source->path : "<runtime>");
    rt->error_value.as.map->values[4] = string_value(rt,
        rt->current_contract != NULL ? rt->current_contract->name :
        (node == NULL ? "Runtime" : hhy_node_kind_name(node->kind)));
    rt->error_value.as.map->values[5] = null_value();
    size_t stack_count = rt->call_stack_count + (node == NULL ? 0 : 1);
    Value stack = list_new(rt, stack_count);
    for (size_t i = 0; i < rt->call_stack_count; i++) {
        RuntimeStackFrame frame = rt->call_stack[i];
        int needed = snprintf(NULL, 0, "%s:%u:%u in %.*s",
                              frame.path, frame.line, frame.column,
                              (int)frame.name_length, frame.name);
        char *text = rt_alloc_atomic(rt, (size_t)needed + 1);
        snprintf(text, (size_t)needed + 1, "%s:%u:%u in %.*s",
                 frame.path, frame.line, frame.column,
                 (int)frame.name_length, frame.name);
        stack.as.list.items[i] = (Value){.kind = V_STRING,
            .string_length = (size_t)needed, .as.string = text};
    }
    if (node != NULL) {
        const char *path = rt->source == NULL ? "<runtime>" : rt->source->path;
        const char *name = hhy_node_kind_name(node->kind);
        int needed = snprintf(NULL, 0, "%s:%u:%u in %s", path,
                              node->token.line, node->token.column, name);
        char *text = rt_alloc_atomic(rt, (size_t)needed + 1);
        snprintf(text, (size_t)needed + 1, "%s:%u:%u in %s", path,
                 node->token.line, node->token.column, name);
        stack.as.list.items[stack_count - 1] = (Value){.kind = V_STRING,
            .string_length = (size_t)needed, .as.string = text};
    }
    rt->error_value.as.map->values[6] = stack;
    rt->error_value.as.map->values[7] = null_value();
    rt->error_line = node == NULL ? 0 : node->token.line;
    rt->error_column = node == NULL ? 0 : node->token.column;
}

static void runtime_extension_error(Runtime *rt, const HhyNode *node,
                                    const HhyExtensionError *error) {
    runtime_error_kind(rt, node, error->kind, error->code, error->message);
    if (!rt->failed || rt->error_value.kind != V_ERROR || rt->error_value.as.map == NULL) return;
    if (error->stage != NULL)
        rt->error_value.as.map->values[4] = string_value(rt, error->stage);
    if (error->cause != NULL)
        rt->error_value.as.map->values[5] = string_value(rt, error->cause);
    if (error->operation != NULL)
        rt->error_value.as.map->values[7] = string_value(rt, error->operation);
}

static void runtime_type_error(Runtime *rt, const HhyNode *node, const char *message) {
    runtime_error_kind(rt, node, "TypeError", "HHY_TYPE", message);
}

static void runtime_value_error(Runtime *rt, const HhyNode *node, const char *message) {
    runtime_error_kind(rt, node, "ValueError", "HHY_VALUE", message);
}

static void runtime_index_error(Runtime *rt, const HhyNode *node, const char *message) {
    runtime_error_kind(rt, node, "IndexError", "HHY_INDEX", message);
}

static void runtime_check_error(Runtime *rt, const HhyNode *node, const char *message) {
    runtime_error_kind(rt, node, "CheckError", "HHY_CHECK", message);
}

static void runtime_io_error(Runtime *rt, const HhyNode *node, const char *message) {
    runtime_error_kind(rt, node, "IoError", "HHY_IO", message);
}

static void runtime_process_error(Runtime *rt, const HhyNode *node, const char *message) {
    runtime_error_kind(rt, node, "ProcessError", "HHY_PROCESS", message);
}

static bool runtime_reserve_file(Runtime *rt, const HhyNode *site) {
    if (rt->active_open_files >= rt->limits.max_open_files) {
        runtime_error_kind(rt, site, "ResourceLimitError", "HHY_OPEN_FILE_LIMIT",
                           "operation exceeds RuntimeLimits.max_open_files");
        return false;
    }
    rt->active_open_files++;
    return true;
}

static FILE *runtime_fopen(Runtime *rt, const HhyNode *site,
                           const char *path, const char *mode) {
    if (!runtime_reserve_file(rt, site)) return NULL;
    FILE *file = fopen(path, mode);
    if (file == NULL) rt->active_open_files--;
    return file;
}

static FILE *runtime_tmpfile(Runtime *rt, const HhyNode *site) {
    if (!runtime_reserve_file(rt, site)) return NULL;
    FILE *file = tmpfile();
    if (file == NULL) rt->active_open_files--;
    return file;
}

static FILE *runtime_fdopen(Runtime *rt, const HhyNode *site,
                            int descriptor, const char *mode) {
    if (!runtime_reserve_file(rt, site)) return NULL;
    FILE *file = fdopen(descriptor, mode);
    if (file == NULL) rt->active_open_files--;
    return file;
}

static int runtime_fclose(Runtime *rt, FILE *file) {
    if (file == NULL) return 0;
    int result = fclose(file);
    if (rt->active_open_files > 0) rt->active_open_files--;
    return result;
}

static bool runtime_check_cancel(Runtime *rt, const HhyNode *node) {
    if (!rt->failed && rt->limits.max_runtime_ns > 0) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        int64_t seconds = (int64_t)now.tv_sec - (int64_t)rt->started_at.tv_sec;
        int64_t nanoseconds = (int64_t)now.tv_nsec - (int64_t)rt->started_at.tv_nsec;
        uint64_t elapsed = seconds <= 0 && nanoseconds <= 0 ? 0 :
            (uint64_t)(seconds * 1000000000LL + nanoseconds);
        if (elapsed >= rt->limits.max_runtime_ns) {
            rt->cancelled = true;
            rt->exit_code = 5;
            runtime_error_kind(rt, node, "TimeoutError", "HHY_RUNTIME_TIMEOUT",
                               "execution exceeded max_runtime");
            return true;
        }
    }
    if (!hhy_interrupt_requested && !rt->cancelled) return false;
    if (!rt->failed) {
        rt->cancelled = true;
        rt->exit_code = 5;
        runtime_error_kind(rt, node, "CancelledError", "HHY_CANCELLED", "execution cancelled");
    }
    return true;
}

static bool runtime_safepoint(Runtime *rt, const HhyNode *node) {
    rt->safepoint_ticks++;
    return (rt->safepoint_ticks & 1023U) == 0 && runtime_check_cancel(rt, node);
}

static bool runtime_wait_ns(Runtime *rt, const HhyNode *node, uint64_t nanoseconds) {
    const uint64_t quantum = 10000000ULL;
    while (nanoseconds > 0) {
        if (runtime_check_cancel(rt, node)) return false;
        uint64_t slice = nanoseconds < quantum ? nanoseconds : quantum;
        struct timespec wait = {
            .tv_sec = (time_t)(slice / 1000000000ULL),
            .tv_nsec = (long)(slice % 1000000000ULL)
        };
        while (nanosleep(&wait, &wait) != 0) {
            if (errno != EINTR) {
                runtime_io_error(rt, node, "timer wait failed"); return false;
            }
            if (runtime_check_cancel(rt, node)) return false;
        }
        nanoseconds -= slice;
    }
    return !runtime_check_cancel(rt, node);
}

static Env *env_new_with_capacity(Runtime *rt, Env *parent, size_t capacity) {
    if (capacity > (SIZE_MAX - sizeof(Env)) / sizeof(Binding)) runtime_memory_limit(rt);
    Env *env = rt_alloc(rt, sizeof(*env) + capacity * sizeof(Binding));
    env->parent = parent;
    if (capacity > 0) {
        env->items = (Binding *)(env + 1);
        env->capacity = capacity;
    }
    return env;
}

static Env *env_new(Runtime *rt, Env *parent) { return env_new_with_capacity(rt, parent, 0); }

static void env_mark_escaped(Env *env) {
    for (; env != NULL && !env->escaped; env = env->parent) env->escaped = true;
}

static Env *call_frame_acquire(Runtime *rt, Env *parent, size_t capacity) {
    Env **link = &rt->free_call_frames;
    while (*link != NULL && (*link)->capacity < capacity) link = &(*link)->free_next;
    Env *env = *link;
    if (env == NULL) return env_new_with_capacity(rt, parent, capacity);
    *link = env->free_next;
    env->parent = parent;
    env->count = 0;
    env->escaped = false;
    env->free_next = NULL;
    return env;
}

static void call_frame_release(Runtime *rt, Env *env) {
    if (env->escaped) return;
    if (env->items != NULL) memset(env->items, 0, env->capacity * sizeof(*env->items));
    env->parent = NULL;
    env->count = 0;
    env->free_next = rt->free_call_frames;
    rt->free_call_frames = env;
}

static Binding *env_local_n(Env *env, const char *name, size_t name_length) {
    for (size_t i = 0; i < env->count; i++)
        if (env->items[i].name_length == name_length &&
            memcmp(env->items[i].name, name, name_length) == 0) return &env->items[i];
    return NULL;
}

static Binding *env_local(Env *env, const char *name) {
    return env_local_n(env, name, strlen(name));
}

static Binding *env_find_n(Env *env, const char *name, size_t name_length) {
    for (Env *scope = env; scope != NULL; scope = scope->parent) {
        Binding *binding = env_local_n(scope, name, name_length);
        if (binding != NULL) return binding;
    }
    return NULL;
}

static Binding *env_find_node(Env *env, const HhyNode *node) {
    if (node->local_slot_resolved) {
        Env *scope = env;
        for (size_t depth = 0; scope != NULL && depth < node->local_env_depth; depth++)
            scope = scope->parent;
        if (scope != NULL && node->local_binding_slot < scope->count)
            return &scope->items[node->local_binding_slot];
    }
    HhyNode *mutable_node = (HhyNode *)node;
    if (node->binding_cache_valid) {
        Env *scope = env;
        for (size_t depth = 0; scope != NULL && depth < node->cached_env_depth; depth++)
            scope = scope->parent;
        if (scope != NULL && node->cached_binding_slot < scope->count) {
            Binding *binding = &scope->items[node->cached_binding_slot];
            if (binding->name_length == node->token.length &&
                memcmp(binding->name, node->token.start, node->token.length) == 0)
                return binding;
        }
        mutable_node->binding_cache_valid = false;
    }
    size_t depth = 0;
    for (Env *scope = env; scope != NULL; scope = scope->parent, depth++) {
        for (size_t slot = 0; slot < scope->count; slot++) {
            Binding *binding = &scope->items[slot];
            if (binding->name_length == node->token.length &&
                memcmp(binding->name, node->token.start, node->token.length) == 0) {
                mutable_node->cached_env_depth = depth;
                mutable_node->cached_binding_slot = slot;
                mutable_node->binding_cache_valid = true;
                return binding;
            }
        }
    }
    return NULL;
}

static Binding *env_find(Env *env, const char *name) {
    return env_find_n(env, name, strlen(name));
}

static bool env_define_n(Runtime *rt, Env *env, const HhyNode *node,
                         const char *name, size_t name_length, bool copy_name,
                         Value value, bool mutable) {
    if (env_local_n(env, name, name_length) != NULL) {
        runtime_check_error(rt, node, "name is already declared in this scope");
        return false;
    }
    if (env->count == env->capacity) {
        size_t capacity = env->capacity < 8 ? 8 : env->capacity * 2;
        Binding *items = rt_alloc(rt, capacity * sizeof(*items));
        if (env->items != NULL) memcpy(items, env->items, env->count * sizeof(*items));
        env->items = items;
        env->capacity = capacity;
    }
    Binding *binding = &env->items[env->count++];
    binding->name = copy_name ? rt_strndup(rt, name, name_length) : name;
    binding->name_length = name_length;
    binding->value = value;
    binding->mutable = mutable;
    return true;
}

static bool env_define(Runtime *rt, Env *env, const HhyNode *node,
                       const char *name, Value value, bool mutable) {
    return env_define_n(rt, env, node, name, strlen(name), true, value, mutable);
}

static bool env_define_token(Runtime *rt, Env *env, const HhyNode *node,
                             HhyToken token, Value value, bool mutable) {
    return env_define_n(rt, env, node, token.start, token.length, false, value, mutable);
}

static char *token_text(Runtime *rt, HhyToken token) { return rt_strndup(rt, token.start, token.length); }
static bool numeric(Value v) { return v.kind == V_INT || v.kind == V_FLOAT; }
static double as_double(Value v) { return v.kind == V_INT ? (double)v.as.integer : v.as.number; }

static const char *value_type(Value value) {
    static const char *names[] = {
        "Null","Bool","Int","Float","String","Regex","List","Range","Map","Function",
        "Stream","Path","Bytes","Duration","Percent","Result","File","Directory","FileEvent","Process",
        "CommandResult","HttpRequest","HttpResponse","BytesBuffer","Error","DateTime"
    };
    return names[value.kind];
}

static bool record_kind(ValueKind kind) {
    return kind == V_MAP || kind == V_RESULT || kind == V_FILE || kind == V_DIRECTORY ||
           kind == V_FILE_EVENT ||
           kind == V_PROCESS || kind == V_COMMAND_RESULT || kind == V_HTTP_REQUEST ||
           kind == V_HTTP_RESPONSE || kind == V_ERROR;
}

static Value map_get(Value map, const char *key);
static void print_value(FILE *stream, Value value, bool json);

static void print_string_json(FILE *stream, const char *text, size_t length) {
    fputc('"', stream);
    for (size_t i = 0; i < length; i++) {
        unsigned char c = (unsigned char)text[i];
        switch (c) {
            case '"': fputs("\\\"", stream); break;
            case '\\': fputs("\\\\", stream); break;
            case '\b': fputs("\\b", stream); break;
            case '\f': fputs("\\f", stream); break;
            case '\n': fputs("\\n", stream); break;
            case '\r': fputs("\\r", stream); break;
            case '\t': fputs("\\t", stream); break;
            default:
                if (c < 0x20) fprintf(stream, "\\u%04x", c);
                else fputc(c, stream);
                break;
        }
    }
    fputc('"', stream);
}

static void print_datetime(FILE *stream, Value value, bool json) {
    int64_t seconds = value.as.datetime.nanoseconds / 1000000000LL;
    int64_t local_seconds = seconds + (int64_t)value.as.datetime.offset_minutes * 60;
    time_t timestamp = (time_t)local_seconds;
    struct tm parts;
    gmtime_r(&timestamp, &parts);
    char text[64];
    int offset = value.as.datetime.offset_minutes;
    char zone[16];
    if (offset == 0) strcpy(zone, "Z");
    else snprintf(zone, sizeof(zone), "%c%02d:%02d", offset < 0 ? '-' : '+',
                  abs(offset) / 60, abs(offset) % 60);
    snprintf(text, sizeof(text), "%04d-%02d-%02dT%02d:%02d:%02d%s",
             parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday,
             parts.tm_hour, parts.tm_min, parts.tm_sec, zone);
    if (json) print_string_json(stream, text, strlen(text)); else fputs(text, stream);
}

static void print_value(FILE *stream, Value value, bool json) {
    switch (value.kind) {
        case V_NULL: fputs("null", stream); break;
        case V_BOOL: fputs(value.as.boolean ? "true" : "false", stream); break;
        case V_INT: fprintf(stream, "%" PRId64, value.as.integer); break;
        case V_FLOAT: fprintf(stream, "%.15g", value.as.number); break;
        case V_STRING: case V_PATH:
            if (json) print_string_json(stream, value.as.string, value.string_length);
            else fwrite(value.as.string, 1, value.string_length, stream);
            break;
        case V_REGEX:
            fprintf(stream, "/%s/", value.as.regex.pattern);
            break;
        case V_BYTES: fprintf(stream, "%.15gb", value.as.number); break;
        case V_DURATION: fprintf(stream, "%.15gns", value.as.number); break;
        case V_PERCENT: fprintf(stream, "%.15g%%", value.as.number); break;
        case V_BYTES_BUFFER: fprintf(stream, "<BytesBuffer %zu>", value.as.bytes_buffer.length); break;
        case V_DATETIME: print_datetime(stream, value, json); break;
        case V_LIST:
            fputc('[', stream);
            for (size_t i = 0; i < value.as.list.count; i++) {
                if (i > 0) fputs(", ", stream);
                print_value(stream, value.as.list.items[i], true);
            }
            fputc(']', stream);
            break;
        case V_RANGE:
            fprintf(stream, "%" PRId64 "..%" PRId64, value.as.range.start, value.as.range.end);
            break;
        case V_MAP: case V_RESULT: case V_FILE: case V_DIRECTORY: case V_FILE_EVENT: case V_PROCESS:
        case V_COMMAND_RESULT: case V_HTTP_REQUEST: case V_HTTP_RESPONSE: case V_ERROR:
            if (value.kind == V_ERROR && !json) {
                Value kind = map_get(value, "kind"), message = map_get(value, "message");
                if (kind.kind == V_STRING) { fputs(kind.as.string, stream); fputs(": ", stream); }
                if (message.kind == V_STRING) fputs(message.as.string, stream);
                break;
            }
            fputc('{', stream);
            for (size_t i = 0; i < value.as.map->count; i++) {
                if (i > 0) fputs(", ", stream);
                print_string_json(stream, value.as.map->keys[i], value.as.map->key_lengths[i]);
                fputs(": ", stream);
                print_value(stream, value.as.map->values[i], true);
            }
            fputc('}', stream);
            break;
        case V_FUNCTION: fputs("<function>", stream); break;
        case V_STREAM: fputs("<stream>", stream); break;
    }
}

static void json_indent(FILE *stream, int depth) {
    for (int i = 0; i < depth * 2; i++) fputc(' ', stream);
}

static bool json_encode_value(FILE *stream, Value value, bool pretty, int depth) {
    switch (value.kind) {
        case V_NULL: fputs("null", stream); return true;
        case V_BOOL: fputs(value.as.boolean ? "true" : "false", stream); return true;
        case V_INT: fprintf(stream, "%" PRId64, value.as.integer); return true;
        case V_FLOAT:
            if (!isfinite(value.as.number)) return false;
            fprintf(stream, "%.15g", value.as.number); return true;
        case V_STRING: print_string_json(stream, value.as.string, value.string_length); return true;
        case V_LIST:
            fputc('[', stream);
            for (size_t i = 0; i < value.as.list.count; i++) {
                if (i) fputc(',', stream);
                if (pretty) { fputc('\n', stream); json_indent(stream, depth + 1); }
                else if (i) fputc(' ', stream);
                if (!json_encode_value(stream, value.as.list.items[i], pretty, depth + 1)) return false;
            }
            if (pretty && value.as.list.count) { fputc('\n', stream); json_indent(stream, depth); }
            fputc(']', stream); return true;
        case V_MAP:
            fputc('{', stream);
            for (size_t i = 0; i < value.as.map->count; i++) {
                if (i) fputc(',', stream);
                if (pretty) { fputc('\n', stream); json_indent(stream, depth + 1); }
                else if (i) fputc(' ', stream);
                print_string_json(stream, value.as.map->keys[i], value.as.map->key_lengths[i]);
                fputs(": ", stream);
                if (!json_encode_value(stream, value.as.map->values[i], pretty, depth + 1)) return false;
            }
            if (pretty && value.as.map->count) { fputc('\n', stream); json_indent(stream, depth); }
            fputc('}', stream); return true;
        default: return false;
    }
}

static Value eval(Runtime *rt, Env *env, const HhyNode *node);
static Value exec_node(Runtime *rt, Env *env, const HhyNode *node);
static Value exec_block_contents(Runtime *rt, Env *env, const HhyNode *node);
static Value import_module(Runtime *rt, Env *target, const HhyNode *node);

typedef struct {
    const HhyBytecodeChunk *chunk;
    size_t instruction;
} BytecodeCursor;

static Value bytecode_eval(Runtime *rt, Env *env, BytecodeCursor node);
static Value bytecode_exec(Runtime *rt, Env *env, BytecodeCursor node);
static Value bytecode_exec_contents(Runtime *rt, Env *env, BytecodeCursor node);
static bool bytecode_eval_argument_expression(Runtime *rt, BytecodeCursor node,
                                              uint32_t parameter_constant, Value argument,
                                              Value *result);
static Value bytecode_import_module(Runtime *rt, Env *target, BytecodeCursor node);
static Module *module_load(Runtime *rt, const HhyNode *site, const char *requested);

static HhyNode bytecode_site(BytecodeCursor cursor) {
    HhyInstruction instruction = cursor.chunk->code[cursor.instruction];
    const char *text = instruction.constant == HHY_BYTECODE_NO_CONSTANT
        ? "" : cursor.chunk->constants[instruction.constant];
    return (HhyNode){
        .kind = (HhyNodeKind)instruction.opcode,
        .token = {.kind = instruction.token_kind, .start = text,
                  .length = instruction.token_length, .line = instruction.line,
                  .column = instruction.column},
        .child_count = instruction.child_count,
        .local_env_depth = instruction.local_env_depth,
        .local_binding_slot = instruction.local_binding_slot,
        .local_slot_resolved = instruction.local_slot_resolved,
        .frame_slot_count = instruction.frame_slot_count
    };
}

static BytecodeCursor bytecode_child_cursor(BytecodeCursor parent, uint32_t index) {
    assert(index < parent.chunk->code[parent.instruction].child_count);
    size_t child = parent.instruction + 1;
    for (uint32_t current = 0; current < index; current++)
        child += parent.chunk->code[child].subtree_size;
    return (BytecodeCursor){.chunk = parent.chunk, .instruction = child};
}

static Value decode_string(Runtime *rt, HhyToken token) {
    if (token.length < 2) return string_value(rt, "");
    char *result = rt_alloc(rt, token.length);
    size_t out = 0;
    for (size_t i = 1; i + 1 < token.length; i++) {
        char c = token.start[i];
        if (c == '\\' && i + 2 < token.length) {
            char escaped = token.start[++i];
            switch (escaped) {
                case 'n': c = '\n'; break; case 'r': c = '\r'; break;
                case 't': c = '\t'; break; case '"': c = '"'; break;
                case 'b': c = '\b'; break; case 'f': c = '\f'; break;
                case '0': c = '\0'; break; case '\\': c = '\\'; break;
                default:
                    runtime_value_error(rt, NULL, "invalid String escape");
                    return null_value();
            }
        }
        result[out++] = c;
    }
    result[out] = '\0';
    Value value = {.kind = V_STRING, .string_length = out}; value.as.string = result; return value;
}

static double unit_multiplier(const char *suffix, ValueKind *kind) {
    *kind = V_BYTES;
    if (strcmp(suffix, "b") == 0) return 1;
    if (strcmp(suffix, "kb") == 0) return 1000.0;
    if (strcmp(suffix, "mb") == 0) return 1000000.0;
    if (strcmp(suffix, "gb") == 0) return 1000000000.0;
    if (strcmp(suffix, "tb") == 0) return 1000000000000.0;
    if (strcmp(suffix, "kib") == 0) return 1024.0;
    if (strcmp(suffix, "mib") == 0) return 1048576.0;
    if (strcmp(suffix, "gib") == 0) return 1073741824.0;
    if (strcmp(suffix, "tib") == 0) return 1099511627776.0;
    *kind = V_DURATION;
    if (strcmp(suffix, "ns") == 0) return 1;
    if (strcmp(suffix, "us") == 0) return 1000.0;
    if (strcmp(suffix, "ms") == 0) return 1000000.0;
    if (strcmp(suffix, "s") == 0) return 1000000000.0;
    if (strcmp(suffix, "min") == 0) return 60000000000.0;
    if (strcmp(suffix, "h") == 0) return 3600000000000.0;
    if (strcmp(suffix, "d") == 0) return 86400000000000.0;
    return 1;
}

static bool parse_int64_literal(const char *text, int64_t *out) {
    unsigned base = 10;
    size_t index = 0;
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16; index = 2;
    } else if (text[0] == '0' && (text[1] == 'b' || text[1] == 'B')) {
        base = 2; index = 2;
    }
    uint64_t value = 0;
    bool saw_digit = false;
    for (; text[index] != '\0'; index++) {
        unsigned char c = (unsigned char)text[index];
        if (c == '_') continue;
        unsigned digit;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else return false;
        if (digit >= base || value > ((uint64_t)INT64_MAX - digit) / base) return false;
        value = value * base + digit;
        saw_digit = true;
    }
    if (!saw_digit) return false;
    *out = (int64_t)value;
    return true;
}

static bool parse_decimal_literal(Runtime *rt, const char *text, size_t length, double *out) {
    char *normalized = rt_alloc_atomic(rt, length + 1);
    size_t written = 0;
    for (size_t i = 0; i < length; i++)
        if (text[i] != '_') normalized[written++] = text[i];
    normalized[written] = '\0';
    errno = 0;
    char *end = NULL;
    double value = strtod(normalized, &end);
    if (errno == ERANGE || end == normalized || *end != '\0' || !isfinite(value)) return false;
    *out = value;
    return true;
}

static Value literal(Runtime *rt, const HhyNode *node) {
    HhyToken token = node->token;
    if (token.kind == HHY_T_NULL) return null_value();
    if (token.kind == HHY_T_TRUE) return bool_value(true);
    if (token.kind == HHY_T_FALSE) return bool_value(false);
    if (token.kind == HHY_T_STRING) return decode_string(rt, token);
    if (token.kind == HHY_T_REGEX) {
        size_t closing = token.length - 1;
        while (closing > 0 && token.start[closing] != '/') closing--;
        if (closing == 0) { runtime_value_error(rt, node, "invalid Regex literal"); return null_value(); }
        Value value = {.kind = V_REGEX};
        value.as.regex.pattern = rt_strndup(rt, token.start + 1, closing - 1);
        value.as.regex.flags = 0;
        for (size_t i = closing + 1; i < token.length; i++) {
            if (token.start[i] == 'i') value.as.regex.flags |= PCRE2_CASELESS;
            else if (token.start[i] == 'm') value.as.regex.flags |= PCRE2_MULTILINE;
            else if (token.start[i] == 's') value.as.regex.flags |= PCRE2_DOTALL;
            else if (token.start[i] == 'u') value.as.regex.flags |= PCRE2_UTF | PCRE2_UCP;
        }
        return value;
    }
    char inline_text[128];
    char *text;
    if (token.length < sizeof(inline_text)) {
        memcpy(inline_text, token.start, token.length);
        inline_text[token.length] = '\0';
        text = inline_text;
    } else text = token_text(rt, token);
    if (token.kind == HHY_T_INT) {
        int64_t number;
        if (!parse_int64_literal(text, &number)) {
            runtime_error_kind(rt, node, "ValueError", "HHY_INT_LITERAL",
                               "integer literal is outside Int64 range");
            return null_value();
        }
        return int_value(number);
    }
    if (token.kind == HHY_T_FLOAT) {
        double number;
        if (!parse_decimal_literal(rt, text, token.length, &number)) {
            runtime_value_error(rt, node, "invalid Float literal"); return null_value();
        }
        return float_value(number);
    }
    if (token.kind == HHY_T_PERCENT) {
        double number;
        if (!parse_decimal_literal(rt, text, token.length - 1, &number)) {
            runtime_value_error(rt, node, "invalid Percent literal"); return null_value();
        }
        Value v = {.kind = V_PERCENT}; v.as.number = number; return v;
    }
    char *suffix = text;
    while (*suffix != '\0' && !isalpha((unsigned char)*suffix)) suffix++;
    ValueKind kind;
    Value v = {.kind = V_BYTES};
    double number;
    if (!parse_decimal_literal(rt, text, (size_t)(suffix - text), &number)) {
        runtime_value_error(rt, node, "invalid native unit literal"); return null_value();
    }
    v.as.number = number * unit_multiplier(suffix, &kind);
    v.kind = kind;
    return v;
}

static bool equal_values_depth(Value a, Value b, size_t depth) {
    if (depth > 128) return false;
    if (numeric(a) && numeric(b)) return as_double(a) == as_double(b);
    if (a.kind != b.kind) return false;
    switch (a.kind) {
        case V_NULL: return true;
        case V_BOOL: return a.as.boolean == b.as.boolean;
        case V_INT: return a.as.integer == b.as.integer;
        case V_FLOAT: case V_BYTES: case V_DURATION: case V_PERCENT: return a.as.number == b.as.number;
        case V_STRING: case V_PATH:
            return a.string_length == b.string_length &&
                   memcmp(a.as.string, b.as.string, a.string_length) == 0;
        case V_BYTES_BUFFER:
            return a.as.bytes_buffer.length == b.as.bytes_buffer.length &&
                   (a.as.bytes_buffer.length == 0 ||
                    memcmp(a.as.bytes_buffer.data, b.as.bytes_buffer.data,
                           a.as.bytes_buffer.length) == 0);
        case V_DATETIME: return a.as.datetime.nanoseconds == b.as.datetime.nanoseconds;
        case V_RANGE:
            return a.as.range.start == b.as.range.start && a.as.range.end == b.as.range.end;
        case V_LIST:
            if (a.as.list.count != b.as.list.count) return false;
            for (size_t i = 0; i < a.as.list.count; i++)
                if (!equal_values_depth(a.as.list.items[i], b.as.list.items[i], depth + 1))
                    return false;
            return true;
        case V_MAP: case V_RESULT: case V_FILE: case V_DIRECTORY: case V_FILE_EVENT: case V_PROCESS:
        case V_COMMAND_RESULT:
            if (a.as.map->count != b.as.map->count) return false;
            for (size_t i = 0; i < a.as.map->count; i++) {
                bool found = false;
                for (size_t j = 0; j < b.as.map->count; j++) {
                    if (a.as.map->key_lengths[i] != b.as.map->key_lengths[j] ||
                        memcmp(a.as.map->keys[i], b.as.map->keys[j], a.as.map->key_lengths[i]) != 0)
                        continue;
                    if (!equal_values_depth(a.as.map->values[i], b.as.map->values[j], depth + 1))
                        return false;
                    found = true;
                    break;
                }
                if (!found) return false;
            }
            return true;
        default: return false;
    }
}

static bool equal_values(Value a, Value b) { return equal_values_depth(a, b, 0); }

static bool hashable_scalar(Value value) {
    return value.kind == V_NULL || value.kind == V_BOOL || value.kind == V_INT ||
           value.kind == V_FLOAT || value.kind == V_STRING || value.kind == V_PATH ||
           value.kind == V_BYTES || value.kind == V_DURATION || value.kind == V_PERCENT;
}

static uint64_t hash_bytes(uint64_t hash, const void *data, size_t length) {
    const unsigned char *bytes = data;
    for (size_t i = 0; i < length; i++) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t hash_scalar(Value value) {
    uint64_t hash = UINT64_C(1469598103934665603);
    if (numeric(value)) {
        unsigned char numeric_tag = 0x7f;
        double number = as_double(value);
        if (number == 0.0) number = 0.0;
        uint64_t bits;
        memcpy(&bits, &number, sizeof(bits));
        hash = hash_bytes(hash, &numeric_tag, sizeof(numeric_tag));
        return hash_bytes(hash, &bits, sizeof(bits));
    }
    unsigned char kind = (unsigned char)value.kind;
    hash = hash_bytes(hash, &kind, sizeof(kind));
    switch (value.kind) {
        case V_NULL: return hash;
        case V_BOOL: return hash_bytes(hash, &value.as.boolean, sizeof(value.as.boolean));
        case V_STRING: case V_PATH:
            return hash_bytes(hash, value.as.string, value.string_length);
        case V_BYTES: case V_DURATION: case V_PERCENT: {
            double number = value.as.number;
            if (number == 0.0) number = 0.0;
            uint64_t bits;
            memcpy(&bits, &number, sizeof(bits));
            return hash_bytes(hash, &bits, sizeof(bits));
        }
        default: return hash;
    }
}

static bool sortable_scalar(Value value) {
    return numeric(value) || value.kind == V_STRING || value.kind == V_PATH ||
           value.kind == V_BYTES || value.kind == V_DURATION || value.kind == V_PERCENT ||
           value.kind == V_DATETIME;
}

static bool compare_sort_keys(Runtime *rt, const HhyNode *site, Value left, Value right,
                              int *comparison) {
    bool same_unit = left.kind == right.kind &&
        (left.kind == V_BYTES || left.kind == V_DURATION || left.kind == V_PERCENT);
    bool same_datetime = left.kind == V_DATETIME && right.kind == V_DATETIME;
    bool comparable = (numeric(left) && numeric(right)) ||
        ((left.kind == V_STRING || left.kind == V_PATH) && left.kind == right.kind) ||
        same_unit || same_datetime;
    if (!comparable) {
        runtime_type_error(rt, site, "sort_by keys must be comparable scalars");
        return false;
    }
    if (numeric(left)) {
        double a = as_double(left), b = as_double(right);
        *comparison = a < b ? -1 : a > b ? 1 : 0;
    } else if (same_unit) {
        *comparison = left.as.number < right.as.number ? -1 :
            left.as.number > right.as.number ? 1 : 0;
    } else if (same_datetime) {
        *comparison = left.as.datetime.nanoseconds < right.as.datetime.nanoseconds ? -1 :
            left.as.datetime.nanoseconds > right.as.datetime.nanoseconds ? 1 : 0;
    } else {
        size_t common = left.string_length < right.string_length
            ? left.string_length : right.string_length;
        *comparison = memcmp(left.as.string, right.as.string, common);
        if (*comparison == 0)
            *comparison = left.string_length < right.string_length ? -1 :
                left.string_length > right.string_length ? 1 : 0;
    }
    return true;
}

static bool stable_sort_values(Runtime *rt, const HhyNode *site, Value *items, Value *keys,
                               size_t count, bool descending) {
    if (count < 2) return true;
    Value *temporary_items = rt_value_array_grow(rt, NULL, 0, count);
    Value *temporary_keys = rt_value_array_grow(rt, NULL, 0, count);
    Value *source_items = items, *source_keys = keys;
    Value *target_items = temporary_items, *target_keys = temporary_keys;
    for (size_t width = 1; width < count; width = width > count / 2 ? count : width * 2) {
        for (size_t start = 0; start < count; start += width * 2) {
            size_t middle = start + width < count ? start + width : count;
            size_t end = middle + width < count ? middle + width : count;
            size_t left = start, right = middle, output = start;
            while (left < middle || right < end) {
                bool take_left = right >= end;
                if (left < middle && right < end) {
                    int comparison = 0;
                    if (!compare_sort_keys(rt, site, source_keys[left], source_keys[right],
                                           &comparison)) {
                        return false;
                    }
                    take_left = descending ? comparison >= 0 : comparison <= 0;
                }
                size_t selected = take_left ? left++ : right++;
                target_items[output] = source_items[selected];
                target_keys[output++] = source_keys[selected];
            }
        }
        Value *swap = source_items; source_items = target_items; target_items = swap;
        swap = source_keys; source_keys = target_keys; target_keys = swap;
        if (runtime_check_cancel(rt, site)) {
            return false;
        }
    }
    if (source_items != items) {
        memcpy(items, source_items, count * sizeof(Value));
        memcpy(keys, source_keys, count * sizeof(Value));
    }
    return true;
}

static const char *bytes_find(const char *text, size_t text_length,
                              const char *part, size_t part_length) {
    if (part_length == 0) return text;
    if (part_length > text_length) return NULL;
    for (size_t i = 0; i <= text_length - part_length; i++)
        if (memcmp(text + i, part, part_length) == 0) return text + i;
    return NULL;
}

static bool string_has_nul(Value value) {
    return (value.kind == V_STRING || value.kind == V_PATH) &&
           memchr(value.as.string, '\0', value.string_length) != NULL;
}

static bool string_equals_c(Value value, const char *text) {
    size_t length = strlen(text);
    return value.kind == V_STRING && value.string_length == length &&
           memcmp(value.as.string, text, length) == 0;
}

static bool require_bool(Runtime *rt, const HhyNode *node, Value value, bool *out) {
    if (value.kind != V_BOOL) { runtime_type_error(rt, node, "condition must be Bool"); return false; }
    *out = value.as.boolean; return true;
}

static Value binary_value(Runtime *rt, const HhyNode *node, Value a, Value b) {
    HhyTokenKind op = node->token.kind;
    if (op == HHY_T_EQUAL_EQUAL) return bool_value(equal_values(a, b));
    if (op == HHY_T_BANG_EQUAL) return bool_value(!equal_values(a, b));
    if (op == HHY_T_AND || op == HHY_T_OR) {
        if (a.kind != V_BOOL || b.kind != V_BOOL) { runtime_type_error(rt, node, "logical operands must be Bool"); return null_value(); }
        return bool_value(op == HHY_T_AND ? a.as.boolean && b.as.boolean : a.as.boolean || b.as.boolean);
    }
    if (op == HHY_T_PLUS && a.kind == V_STRING && b.kind == V_STRING) {
        size_t alen = a.string_length, blen = b.string_length;
        char *joined = rt_alloc(rt, alen + blen + 1);
        memcpy(joined, a.as.string, alen); memcpy(joined + alen, b.as.string, blen + 1);
        Value v = {.kind = V_STRING, .string_length = alen + blen}; v.as.string = joined; return v;
    }
    if (a.kind == V_DATETIME && b.kind == V_DURATION &&
        (op == HHY_T_PLUS || op == HHY_T_MINUS)) {
        Value result = a;
        double delta = op == HHY_T_PLUS ? b.as.number : -b.as.number;
        if (delta > (double)INT64_MAX || delta < (double)INT64_MIN) {
            runtime_value_error(rt, node, "DateTime arithmetic overflow"); return null_value();
        }
        int64_t integer_delta = (int64_t)delta;
        int64_t current = result.as.datetime.nanoseconds;
        if ((integer_delta > 0 && current > INT64_MAX - integer_delta) ||
            (integer_delta < 0 && current < INT64_MIN - integer_delta)) {
            runtime_value_error(rt, node, "DateTime arithmetic overflow"); return null_value();
        }
        result.as.datetime.nanoseconds = current + integer_delta;
        return result;
    }
    if (a.kind == V_DATETIME && b.kind == V_DATETIME) {
        int64_t av = a.as.datetime.nanoseconds, bv = b.as.datetime.nanoseconds;
        if (op == HHY_T_LT) return bool_value(av < bv);
        if (op == HHY_T_LTE) return bool_value(av <= bv);
        if (op == HHY_T_GT) return bool_value(av > bv);
        if (op == HHY_T_GTE) return bool_value(av >= bv);
        if (op == HHY_T_MINUS) {
            Value result = {.kind = V_DURATION};
            result.as.number = (double)av - (double)bv;
            return result;
        }
        runtime_type_error(rt, node, "DateTime values only support comparison and subtraction");
        return null_value();
    }
    bool same_unit = a.kind == b.kind && (a.kind == V_BYTES || a.kind == V_DURATION || a.kind == V_PERCENT);
    if ((!numeric(a) || !numeric(b)) && !same_unit) { runtime_type_error(rt, node, "operator requires compatible numeric operands"); return null_value(); }
    if (a.kind == V_INT && b.kind == V_INT) {
        int64_t av = a.as.integer, bv = b.as.integer, integer_result = 0;
        if (op == HHY_T_LT) return bool_value(av < bv);
        if (op == HHY_T_LTE) return bool_value(av <= bv);
        if (op == HHY_T_GT) return bool_value(av > bv);
        if (op == HHY_T_GTE) return bool_value(av >= bv);
        if (op == HHY_T_PLUS || op == HHY_T_MINUS || op == HHY_T_STAR) {
            bool overflow = op == HHY_T_PLUS ? __builtin_add_overflow(av, bv, &integer_result) :
                            op == HHY_T_MINUS ? __builtin_sub_overflow(av, bv, &integer_result) :
                                               __builtin_mul_overflow(av, bv, &integer_result);
            if (overflow) {
                runtime_error_kind(rt, node, "ValueError", "HHY_INT_OVERFLOW",
                                   "Int arithmetic overflow");
                return null_value();
            }
            return int_value(integer_result);
        }
        if (op == HHY_T_MOD) {
            if (bv == 0) { runtime_value_error(rt, node, "division by zero"); return null_value(); }
            if (av == INT64_MIN && bv == -1) return int_value(0);
            return int_value(av % bv);
        }
    }
    double av = numeric(a) ? as_double(a) : a.as.number;
    double bv = numeric(b) ? as_double(b) : b.as.number;
    if ((op == HHY_T_SLASH || op == HHY_T_MOD) && bv == 0) { runtime_value_error(rt, node, "division by zero"); return null_value(); }
    if (op == HHY_T_LT) return bool_value(av < bv);
    if (op == HHY_T_LTE) return bool_value(av <= bv);
    if (op == HHY_T_GT) return bool_value(av > bv);
    if (op == HHY_T_GTE) return bool_value(av >= bv);
    if (same_unit && op != HHY_T_PLUS && op != HHY_T_MINUS) {
        runtime_type_error(rt, node, "unit values only support comparison, addition and subtraction");
        return null_value();
    }
    double result = 0;
    if (op == HHY_T_PLUS) result = av + bv;
    else if (op == HHY_T_MINUS) result = av - bv;
    else if (op == HHY_T_STAR) result = av * bv;
    else if (op == HHY_T_SLASH) result = av / bv;
    else if (op == HHY_T_MOD) result = fmod(av, bv);
    else { runtime_type_error(rt, node, "unsupported binary operator"); return null_value(); }
    if (same_unit) {
        if (!isfinite(result)) {
            runtime_value_error(rt, node, "native unit arithmetic overflow"); return null_value();
        }
        Value v = {.kind = a.kind}; v.as.number = result; return v;
    }
    return float_value(result);
}

static uint64_t hash_key_bytes(const char *key, size_t key_length) {
    uint64_t hash = UINT64_C(1469598103934665603);
    for (size_t i = 0; i < key_length; i++) {
        hash ^= (unsigned char)key[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static void map_build_index(Runtime *rt, MapStorage *map) {
    if (map->count < 8) return;
    size_t slots = 16;
    while (slots < map->count * 2) slots *= 2;
    map->slots = rt_alloc_atomic(rt, slots * sizeof(size_t));
    map->slot_count = slots;
    map->index_magic = UINT64_C(0x4848594d41504958);
    for (size_t i = 0; i < map->count; i++) {
        size_t slot = (size_t)(hash_key_bytes(map->keys[i], map->key_lengths[i]) & (slots - 1));
        while (map->slots[slot] != 0) slot = (slot + 1) & (slots - 1);
        map->slots[slot] = i + 1;
    }
}

static bool map_lookup_n(Value map, const char *key, size_t key_length, Value *out) {
    if (!record_kind(map.kind))
        return false;
    if (map.as.map->index_magic == UINT64_C(0x4848594d41504958)) {
        size_t slot = (size_t)(hash_key_bytes(key, key_length) & (map.as.map->slot_count - 1));
        while (map.as.map->slots[slot] != 0) {
            size_t i = map.as.map->slots[slot] - 1;
            if (map.as.map->key_lengths[i] == key_length &&
                memcmp(map.as.map->keys[i], key, key_length) == 0) {
                *out = map.as.map->values[i];
                return true;
            }
            slot = (slot + 1) & (map.as.map->slot_count - 1);
        }
        return false;
    }
    for (size_t i = 0; i < map.as.map->count; i++) {
        if (map.as.map->key_lengths[i] == key_length &&
            memcmp(map.as.map->keys[i], key, key_length) == 0) {
            *out = map.as.map->values[i];
            return true;
        }
    }
    return false;
}

static Value map_get_n(Value map, const char *key, size_t key_length) {
    Value result = null_value();
    (void)map_lookup_n(map, key, key_length, &result);
    return result;
}

static Value map_get(Value map, const char *key) { return map_get_n(map, key, strlen(key)); }

static Value call_value(Runtime *rt, Env *env, const HhyNode *site, Value callee,
                        size_t argc, Value *argv);
static Value call_closure(Runtime *rt, const HhyNode *site, Value callee,
                          size_t argc, Value *argv);
static Value call_stream_value(Runtime *rt, Env *env, const HhyNode *site, Value callee,
                               size_t argc, Value *argv) {
    if (rt->profiler == NULL && callee.kind == V_FUNCTION &&
        callee.as.function.builtin == NULL && callee.as.function.is_closure &&
        callee.as.function.is_bytecode) {
        if (rt->call_depth >= rt->limits.max_recursion) {
            runtime_error_kind(rt, site, "ResourceLimitError", "HHY_RECURSION_LIMIT",
                               "maximum call depth exceeded");
            return null_value();
        }
        rt->call_depth++;
        Value result = call_closure(rt, site, callee, argc, argv);
        rt->call_depth--;
        return result;
    }
    return call_value(rt, env, site, callee, argc, argv);
}
static bool bytecode_eval_argument_expression(Runtime *rt, BytecodeCursor node,
                                              uint32_t parameter_constant, Value argument,
                                              Value *result);
static bool require_bool(Runtime *rt, const HhyNode *node, Value value, bool *out);
static Value list_new(Runtime *rt, size_t count);
static Value file_value(Runtime *rt, const HhyNode *site, const char *path,
                        const struct stat *info);
static Value map_with_entries(Runtime *rt, ValueKind kind, size_t count,
                              const char **keys, Value *values);
static Value csv_parse_row(Runtime *rt, const HhyNode *site,
                           const char *record, size_t record_length, char delimiter, char quote);
static Value csv_read_record(Runtime *rt, const HhyNode *site, Stream *source,
                             char delimiter, char quote);
static Value csv_encode_row(Runtime *rt, const HhyNode *site, Value fields,
                            char delimiter, char quote);

static bool binary_write(FILE *file, const void *data, size_t size) {
    return fwrite(data, 1, size, file) == size;
}

static bool binary_read(FILE *file, void *data, size_t size) {
    return fread(data, 1, size, file) == size;
}

static bool value_write(FILE *file, Value value) {
    uint8_t kind = (uint8_t)value.kind;
    if (!binary_write(file, &kind, sizeof(kind))) return false;
    switch (value.kind) {
        case V_NULL: return true;
        case V_BOOL: return binary_write(file, &value.as.boolean, sizeof(value.as.boolean));
        case V_INT: return binary_write(file, &value.as.integer, sizeof(value.as.integer));
        case V_FLOAT: case V_BYTES: case V_DURATION: case V_PERCENT:
            return binary_write(file, &value.as.number, sizeof(value.as.number));
        case V_STRING: case V_PATH: {
            uint64_t length = (uint64_t)value.string_length;
            return binary_write(file, &length, sizeof(length)) &&
                   binary_write(file, value.as.string, (size_t)length);
        }
        case V_REGEX: {
            uint64_t length = (uint64_t)strlen(value.as.regex.pattern);
            return binary_write(file, &length, sizeof(length)) &&
                   binary_write(file, value.as.regex.pattern, (size_t)length) &&
                   binary_write(file, &value.as.regex.flags, sizeof(value.as.regex.flags));
        }
        case V_BYTES_BUFFER: {
            uint64_t length = (uint64_t)value.as.bytes_buffer.length;
            return binary_write(file, &length, sizeof(length)) &&
                   binary_write(file, value.as.bytes_buffer.data, value.as.bytes_buffer.length);
        }
        case V_DATETIME:
            return binary_write(file, &value.as.datetime, sizeof(value.as.datetime));
        case V_RANGE:
            return binary_write(file, &value.as.range, sizeof(value.as.range));
        case V_LIST: {
            uint64_t count = (uint64_t)value.as.list.count;
            if (!binary_write(file, &count, sizeof(count))) return false;
            for (size_t i = 0; i < value.as.list.count; i++)
                if (!value_write(file, value.as.list.items[i])) return false;
            return true;
        }
        case V_MAP: case V_RESULT: case V_FILE: case V_DIRECTORY: case V_FILE_EVENT: case V_PROCESS:
        case V_COMMAND_RESULT: case V_HTTP_REQUEST: case V_HTTP_RESPONSE: case V_ERROR: {
            uint64_t count = (uint64_t)value.as.map->count;
            if (!binary_write(file, &count, sizeof(count))) return false;
            for (size_t i = 0; i < value.as.map->count; i++) {
                uint64_t length = (uint64_t)value.as.map->key_lengths[i];
                if (!binary_write(file, &length, sizeof(length)) ||
                    !binary_write(file, value.as.map->keys[i], (size_t)length) ||
                    !value_write(file, value.as.map->values[i])) return false;
            }
            return true;
        }
        case V_FUNCTION: case V_STREAM:
            return false;
    }
    return false;
}

static bool value_read(Runtime *rt, FILE *file, Value *out, size_t depth) {
    if (depth > 128) return false;
    uint8_t kind;
    if (!binary_read(file, &kind, sizeof(kind)) || kind > V_DATETIME) return false;
    out->kind = (ValueKind)kind;
    switch (out->kind) {
        case V_NULL: return true;
        case V_BOOL: return binary_read(file, &out->as.boolean, sizeof(out->as.boolean));
        case V_INT: return binary_read(file, &out->as.integer, sizeof(out->as.integer));
        case V_FLOAT: case V_BYTES: case V_DURATION: case V_PERCENT:
            return binary_read(file, &out->as.number, sizeof(out->as.number));
        case V_STRING: case V_PATH: {
            uint64_t length;
            if (!binary_read(file, &length, sizeof(length)) || length > 64 * 1024 * 1024) return false;
            out->as.string = rt_alloc(rt, (size_t)length + 1);
            if (!binary_read(file, out->as.string, (size_t)length)) return false;
            out->as.string[length] = '\0'; out->string_length = (size_t)length; return true;
        }
        case V_REGEX: {
            uint64_t length;
            if (!binary_read(file, &length, sizeof(length)) || length > 1024 * 1024) return false;
            out->as.regex.pattern = rt_alloc(rt, (size_t)length + 1);
            if (!binary_read(file, out->as.regex.pattern, (size_t)length) ||
                !binary_read(file, &out->as.regex.flags, sizeof(out->as.regex.flags))) return false;
            out->as.regex.pattern[length] = '\0'; return true;
        }
        case V_BYTES_BUFFER: {
            uint64_t length;
            if (!binary_read(file, &length, sizeof(length)) || length > 64 * 1024 * 1024) return false;
            *out = bytes_buffer_value(rt, NULL, (size_t)length);
            return !length || binary_read(file, out->as.bytes_buffer.data, (size_t)length);
        }
        case V_DATETIME:
            return binary_read(file, &out->as.datetime, sizeof(out->as.datetime));
        case V_RANGE: return binary_read(file, &out->as.range, sizeof(out->as.range));
        case V_LIST: {
            uint64_t count;
            if (!binary_read(file, &count, sizeof(count)) || count > 1000000) return false;
            *out = list_new(rt, (size_t)count);
            for (size_t i = 0; i < (size_t)count; i++)
                if (!value_read(rt, file, &out->as.list.items[i], depth + 1)) return false;
            return true;
        }
        case V_MAP: case V_RESULT: case V_FILE: case V_DIRECTORY: case V_FILE_EVENT: case V_PROCESS:
        case V_COMMAND_RESULT: case V_HTTP_REQUEST: case V_HTTP_RESPONSE: case V_ERROR: {
            ValueKind map_kind = out->kind;
            uint64_t count;
            if (!binary_read(file, &count, sizeof(count)) || count > 1000000) return false;
            out->kind = map_kind; out->as.map = map_storage_new(rt, (size_t)count);
            out->as.map->keys = count ? rt_alloc(rt, (size_t)count * sizeof(char *)) : NULL;
            out->as.map->key_lengths = count ? rt_alloc(rt, (size_t)count * sizeof(size_t)) : NULL;
            out->as.map->values = count ? rt_alloc(rt, (size_t)count * sizeof(Value)) : NULL;
            for (size_t i = 0; i < (size_t)count; i++) {
                uint64_t length;
                if (!binary_read(file, &length, sizeof(length)) || length > 1024 * 1024) return false;
                out->as.map->keys[i] = rt_alloc(rt, (size_t)length + 1);
                if (!binary_read(file, out->as.map->keys[i], (size_t)length)) return false;
                out->as.map->keys[i][length] = '\0';
                out->as.map->key_lengths[i] = (size_t)length;
                if (!value_read(rt, file, &out->as.map->values[i], depth + 1)) return false;
            }
            return true;
        }
        case V_FUNCTION: case V_STREAM: return false;
    }
    return false;
}

static Value stream_value(Runtime *rt, StreamKind kind, Value source,
                          Value function, Env *env) {
    env_mark_escaped(env);
    Stream *stream = rt_alloc(rt, sizeof(*stream));
    stream->kind = kind;
    stream->source = source;
    stream->function = function;
    stream->env = env;
    stream->runtime = rt;
    stream->runtime_next = rt->streams;
    rt->streams = stream;
    if (kind != STREAM_LIST && source.kind == V_STREAM)
        source.as.stream->claimed = true;
    Value value = {.kind = V_STREAM};
    value.as.stream = stream;
    return value;
}

static bool parallel_next(Runtime *rt, const HhyNode *site, Stream *stream, Value *out);
static Value process_snapshot(Runtime *rt, const HhyNode *site);

static uint64_t stat_modified_ns(const struct stat *info) {
#ifdef __APPLE__
    return (uint64_t)info->st_mtimespec.tv_sec * 1000000000ULL +
           (uint64_t)info->st_mtimespec.tv_nsec;
#else
    return (uint64_t)info->st_mtim.tv_sec * 1000000000ULL +
           (uint64_t)info->st_mtim.tv_nsec;
#endif
}

static void watch_items_free(WatchItem *items, size_t count) {
    for (size_t i = 0; i < count; i++) free(items[i].path);
    free(items);
}

static WatchItem *watch_scan(Runtime *rt, const HhyNode *site, const char *path,
                             bool recursive, size_t *out_count) {
    *out_count = 0;
    struct stat root;
    if (lstat(path, &root) != 0) {
        runtime_io_error(rt, site, "watch target does not exist"); return NULL;
    }
    if (!S_ISDIR(root.st_mode)) {
        WatchItem *one = hhy_alloc(sizeof(*one));
        one->path = hhy_strndup(path, strlen(path));
        one->modified_ns = stat_modified_ns(&root);
        one->size = root.st_size;
        one->device = root.st_dev;
        one->inode = root.st_ino;
        *out_count = 1;
        return one;
    }
    size_t count = 0, capacity = 16;
    WatchItem *items = hhy_alloc(capacity * sizeof(*items));
    char *paths[] = {(char *)path, NULL};
    if (!runtime_reserve_file(rt, site)) { free(items); return NULL; }
    FTS *tree = fts_open(paths, FTS_PHYSICAL | FTS_NOCHDIR, NULL);
    if (tree == NULL) {
        rt->active_open_files--;
        free(items); runtime_io_error(rt, site, "cannot scan watch directory"); return NULL;
    }
    FTSENT *entry;
    while ((entry = fts_read(tree)) != NULL) {
        if (!recursive && entry->fts_level >= 1 && entry->fts_info == FTS_D) {
            fts_set(tree, entry, FTS_SKIP);
            continue;
        }
        if (entry->fts_level == 0 ||
            (entry->fts_info != FTS_F && entry->fts_info != FTS_SL)) continue;
        size_t path_codepoints = 0, entry_path_length = strlen(entry->fts_path);
        if (!utf8_count(entry->fts_path, entry_path_length, &path_codepoints)) {
            fts_close(tree); rt->active_open_files--;
            watch_items_free(items, count);
            runtime_error_kind(rt, site, "EncodingError", "HHY_PATH_UTF8",
                               "watched filesystem path is not valid UTF-8");
            return NULL;
        }
        const char *entry_name = strrchr(entry->fts_path, '/');
        entry_name = entry_name == NULL ? entry->fts_path : entry_name + 1;
        if (strstr(entry_name, ".hhy-tmp-") != NULL) continue;
        if (count == capacity) {
            capacity *= 2;
            items = hhy_realloc(items, capacity * sizeof(*items));
        }
        items[count].path = hhy_strndup(entry->fts_path, strlen(entry->fts_path));
        items[count].modified_ns = stat_modified_ns(entry->fts_statp);
        items[count].size = entry->fts_statp->st_size;
        items[count].device = entry->fts_statp->st_dev;
        items[count].inode = entry->fts_statp->st_ino;
        count++;
    }
    fts_close(tree);
    rt->active_open_files--;
    *out_count = count;
    return items;
}

static ssize_t watch_find(WatchItem *items, size_t count, const char *path) {
    for (size_t i = 0; i < count; i++)
        if (strcmp(items[i].path, path) == 0) return (ssize_t)i;
    return -1;
}

static Value watch_event(Runtime *rt, const char *kind, const char *path,
                         const char *old_path) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    Value event_path = string_value(rt, path); event_path.kind = V_PATH;
    const char *keys[] = {"kind", "path", "old_path", "timestamp"};
    Value timestamp = {.kind = V_DATETIME};
    timestamp.as.datetime.nanoseconds = (int64_t)now.tv_sec * 1000000000LL + now.tv_nsec;
    Value old = old_path == NULL ? null_value() : string_value(rt, old_path);
    if (old.kind == V_STRING) old.kind = V_PATH;
    Value values[] = {string_value(rt, kind), event_path, old, timestamp};
    return map_with_entries(rt, V_FILE_EVENT, 4, keys, values);
}

static void stream_close(Stream *stream) {
    if (stream == NULL || stream->closed) return;
    stream->closed = true;
    if (stream->file != NULL && stream->owns_file) {
        runtime_fclose(stream->runtime, stream->file);
    }
    stream->file = NULL;
    if (stream->fts != NULL) {
        fts_close(stream->fts);
        if (stream->fts_counted && stream->runtime != NULL &&
            stream->runtime->active_open_files > 0)
            stream->runtime->active_open_files--;
        stream->fts = NULL;
        stream->fts_counted = false;
    }
    if (stream->inner.kind == V_STREAM) stream_close(stream->inner.as.stream);
    stream->seen = NULL;
    free(stream->seen_slots);
    stream->seen_slots = NULL;
    stream->seen_slot_count = 0;
    free(stream->seen_times);
    stream->seen_times = NULL;
    for (size_t i = 0; i < stream->watch_count; i++) free(stream->watch_items[i].path);
    free(stream->watch_items);
    stream->watch_items = NULL;
    stream->watch_count = 0;
    if (stream->platform_watch != NULL) {
        hhy_platform_watch_close(stream->platform_watch);
        if (stream->runtime != NULL) {
            size_t count = stream->platform_watch_handles;
            stream->runtime->active_open_files = stream->runtime->active_open_files > count
                ? stream->runtime->active_open_files - count : 0;
        }
        stream->platform_watch = NULL;
        stream->platform_watch_handles = 0;
    }
    for (size_t i = 0; i < stream->job_count; i++) {
        if (stream->jobs[i].active) {
            kill(stream->jobs[i].pid, SIGTERM);
            bool reaped = false;
            for (size_t attempt = 0; attempt < 50; attempt++) {
                pid_t waited = waitpid(stream->jobs[i].pid, NULL, WNOHANG);
                if (waited == stream->jobs[i].pid || (waited < 0 && errno == ECHILD)) {
                    reaped = true; break;
                }
                if (waited < 0 && errno != EINTR) break;
                struct timespec pause = {.tv_sec = 0, .tv_nsec = 10000000L};
                nanosleep(&pause, NULL);
            }
            if (!reaped) {
                kill(stream->jobs[i].pid, SIGKILL);
                while (waitpid(stream->jobs[i].pid, NULL, 0) < 0 && errno == EINTR) {}
            }
            stream->jobs[i].active = false;
            if (stream->runtime != NULL && stream->runtime->active_processes > 0)
                stream->runtime->active_processes--;
        }
        if (stream->jobs[i].file != NULL) {
            runtime_fclose(stream->runtime, stream->jobs[i].file);
            stream->jobs[i].file = NULL;
        }
    }
    stream->job_count = 0;
    free(stream->line);
    stream->line = NULL;
    if (stream->kind != STREAM_LIST && stream->kind != STREAM_RANGE &&
        stream->kind != STREAM_FILE_LINES &&
        stream->kind != STREAM_FILES &&
        stream->source.kind == V_STREAM)
        stream_close(stream->source.as.stream);
}

static bool stream_next(Runtime *rt, const HhyNode *site, Stream *stream, Value *out) {
    if (stream->closed || rt->failed || runtime_check_cancel(rt, site)) return false;
    if (stream->kind == STREAM_PROCESSES) {
        if (!stream->initialized) {
            stream->initialized = true;
            stream->materialized = process_snapshot(rt, site);
            if (rt->failed) { stream_close(stream); return false; }
        }
        if (stream->index >= stream->materialized.as.list.count) {
            stream_close(stream);
            return false;
        }
        *out = stream->materialized.as.list.items[stream->index++];
        return true;
    }
    if (stream->kind == STREAM_EVERY) {
        uint64_t nanoseconds = stream->interval_ns > 0 ? (uint64_t)stream->interval_ns : 1;
        if (!runtime_wait_ns(rt, site, nanoseconds)) { stream_close(stream); return false; }
        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        const char *keys[] = {"index", "timestamp_ns"};
        Value values[] = {
            int_value((int64_t)stream->index++),
            int_value((int64_t)now.tv_sec * 1000000000LL + now.tv_nsec)
        };
        *out = map_with_entries(rt, V_MAP, 2, keys, values);
        return true;
    }
    if (stream->kind == STREAM_WATCH) {
        if (!stream->initialized) {
            stream->watch_items = watch_scan(rt, site, stream->path, stream->recursive,
                                             &stream->watch_count);
            if (rt->failed) { stream_close(stream); return false; }
            size_t available = rt->limits.max_open_files > rt->active_open_files
                ? rt->limits.max_open_files - rt->active_open_files : 0;
            const char *watch_error = NULL;
            stream->platform_watch = hhy_platform_watch_open(stream->path,
                stream->recursive, available, &watch_error);
            if (stream->platform_watch == NULL) {
                runtime_error_kind(rt, site, "IoError", "HHY_WATCH_OPEN",
                                   watch_error == NULL ? "cannot start platform watcher" : watch_error);
                stream_close(stream); return false;
            }
            stream->platform_watch_handles =
                hhy_platform_watch_handle_count(stream->platform_watch);
            rt->active_open_files += stream->platform_watch_handles;
            stream->initialized = true;
        }
        for (;;) {
            if (stream->pending_events.kind == V_LIST &&
                stream->pending_index < stream->pending_events.as.list.count) {
                *out = stream->pending_events.as.list.items[stream->pending_index++];
                return true;
            }
            stream->pending_events = null_value();
            stream->pending_index = 0;
            const char *watch_error = NULL;
            int ready = hhy_platform_watch_wait(stream->platform_watch, 50, &watch_error);
            if (ready < 0) {
                runtime_error_kind(rt, site, "IoError", "HHY_WATCH_WAIT",
                                   watch_error == NULL ? "platform watcher failed" : watch_error);
                stream_close(stream); return false;
            }
            if (runtime_check_cancel(rt, site)) { stream_close(stream); return false; }
            if (ready == 0) continue;
            size_t next_count = 0;
            WatchItem *next = watch_scan(rt, site, stream->path, stream->recursive, &next_count);
            if (rt->failed) { watch_items_free(next, next_count); stream_close(stream); return false; }
            Value events = list_new(rt, stream->watch_count + next_count);
            size_t event_count = 0;
            bool *renamed_next = next_count == 0 ? NULL : hhy_alloc(next_count * sizeof(bool));
            for (size_t i = 0; i < stream->watch_count; i++) {
                ssize_t found = watch_find(next, next_count, stream->watch_items[i].path);
                if (found < 0) {
                    ssize_t renamed = -1;
                    for (size_t j = 0; j < next_count; j++) {
                        if (!renamed_next[j] && next[j].device == stream->watch_items[i].device &&
                            next[j].inode == stream->watch_items[i].inode) {
                            renamed = (ssize_t)j; break;
                        }
                    }
                    if (renamed >= 0) {
                        renamed_next[renamed] = true;
                        events.as.list.items[event_count++] = watch_event(rt, "renamed",
                            next[renamed].path, stream->watch_items[i].path);
                    } else {
                        events.as.list.items[event_count++] =
                            watch_event(rt, "removed", stream->watch_items[i].path, NULL);
                    }
                } else if (next[found].modified_ns != stream->watch_items[i].modified_ns ||
                           next[found].size != stream->watch_items[i].size) {
                    events.as.list.items[event_count++] =
                        watch_event(rt, "modified", next[found].path, NULL);
                }
            }
            for (size_t i = 0; i < next_count; i++) {
                if (!renamed_next[i] &&
                    watch_find(stream->watch_items, stream->watch_count, next[i].path) < 0)
                    events.as.list.items[event_count++] =
                        watch_event(rt, "created", next[i].path, NULL);
            }
            free(renamed_next);
            watch_items_free(stream->watch_items, stream->watch_count);
            stream->watch_items = next;
            stream->watch_count = next_count;
            size_t old_handles = stream->platform_watch_handles;
            if (!hhy_platform_watch_rebuild(stream->platform_watch, &watch_error)) {
                runtime_error_kind(rt, site, "IoError", "HHY_WATCH_REBUILD",
                                   watch_error == NULL ? "cannot refresh platform watcher" : watch_error);
                stream_close(stream); return false;
            }
            stream->platform_watch_handles =
                hhy_platform_watch_handle_count(stream->platform_watch);
            rt->active_open_files = rt->active_open_files >= old_handles
                ? rt->active_open_files - old_handles + stream->platform_watch_handles
                : stream->platform_watch_handles;
            events.as.list.count = event_count;
            if (event_count > 0) stream->pending_events = events;
        }
    }
    if (stream->kind == STREAM_ON_ERROR) {
        if (stream->inner.kind == V_STREAM) {
            if (stream_next(rt, site, stream->inner.as.stream, out)) return true;
            stream_close(stream);
            return false;
        }
        if (stream_next(rt, site, stream->source.as.stream, out)) return true;
        if (!rt->failed) { stream_close(stream); return false; }
        Value error = rt->error_value;
        rt->failed = false;
        rt->exit_code = 0;
        Value replacement = call_stream_value(rt, stream->env, site, stream->function, 1, &error);
        if (rt->failed) { stream_close(stream); return false; }
        if (replacement.kind != V_STREAM || replacement.as.stream->claimed) {
            runtime_type_error(rt, site, "Stream on_error handler must return a fresh Stream");
            stream_close(stream);
            return false;
        }
        replacement.as.stream->claimed = true;
        stream->inner = replacement;
        return stream_next(rt, site, replacement.as.stream, out);
    }
    if (stream->kind == STREAM_PARALLEL)
        return parallel_next(rt, site, stream, out);
    if (stream->kind == STREAM_FILE_LINES) {
        if (stream->file == NULL) {
            stream->file = runtime_fopen(rt, site, stream->path, "rb");
            if (stream->file == NULL) {
                runtime_io_error(rt, site, "cannot open file for reading");
                stream_close(stream);
                return false;
            }
            stream->owns_file = true;
        }
        ssize_t length = getline(&stream->line, &stream->line_capacity, stream->file);
        if (length < 0) {
            stream_close(stream);
            return false;
        }
        while (length > 0 && (stream->line[length - 1] == '\n' || stream->line[length - 1] == '\r'))
            length--;
        *out = string_n(rt, stream->line, (size_t)length);
        return true;
    }
    if (stream->kind == STREAM_FILES) {
        if (stream->fts == NULL) {
            if (!runtime_reserve_file(rt, site)) {
                stream_close(stream);
                return false;
            }
            stream->fts_paths = rt_alloc(rt, 2 * sizeof(char *));
            stream->fts_paths[0] = stream->path;
            stream->fts_paths[1] = NULL;
            int flags = (stream->follow_symlinks ? FTS_LOGICAL : FTS_PHYSICAL) | FTS_NOCHDIR;
            stream->fts = fts_open(stream->fts_paths, flags, NULL);
            if (stream->fts == NULL) {
                rt->active_open_files--;
                runtime_io_error(rt, site, "cannot open directory traversal");
                stream_close(stream);
                return false;
            }
            stream->fts_counted = true;
        }
        FTSENT *entry;
        size_t root_length = strlen(stream->path);
        while ((entry = fts_read(stream->fts)) != NULL) {
            if (entry->fts_info == FTS_DC) { fts_set(stream->fts, entry, FTS_SKIP); continue; }
            if (entry->fts_level == 0 || (entry->fts_info != FTS_F && entry->fts_info != FTS_SL &&
                                          entry->fts_info != FTS_D))
                continue;
            const char *relative = entry->fts_path + root_length;
            if (*relative == '/') relative++;
            if (fnmatch(stream->pattern, relative, FNM_PATHNAME) != 0 &&
                !(strncmp(stream->pattern, "**/", 3) == 0 &&
                  fnmatch(stream->pattern + 3, relative, FNM_PATHNAME) == 0))
                continue;
            size_t path_codepoints = 0, entry_path_length = strlen(entry->fts_path);
            if (!utf8_count(entry->fts_path, entry_path_length, &path_codepoints)) {
                runtime_error_kind(rt, site, "EncodingError", "HHY_PATH_UTF8",
                                   "filesystem path is not valid UTF-8");
                stream_close(stream);
                return false;
            }
            *out = file_value(rt, site, entry->fts_path, entry->fts_statp);
            return !rt->failed;
        }
        stream_close(stream);
        return false;
    }
    if (stream->kind == STREAM_CSV_PARSE) {
        if (!stream->initialized) {
            stream->initialized = true;
            if (stream->csv_use_header) {
                stream->csv_headers = csv_read_record(rt, site, stream->source.as.stream,
                                                      stream->delimiter, stream->quote);
                if (rt->failed || stream->csv_headers.kind == V_NULL) {
                    stream_close(stream);
                    return false;
                }
            }
        }
        Value fields = csv_read_record(rt, site, stream->source.as.stream,
                                       stream->delimiter, stream->quote);
        if (fields.kind == V_NULL || rt->failed) {
            stream_close(stream);
            return false;
        }
        if (!stream->csv_use_header) {
            *out = fields;
            return true;
        }
        if (fields.as.list.count != stream->csv_headers.as.list.count) {
            runtime_value_error(rt, site, "CSV record field count does not match header");
            stream_close(stream);
            return false;
        }
        Value row = {.kind = V_MAP}; row.as.map = map_storage_new(rt, fields.as.list.count);
        row.as.map->keys = row.as.map->count ? rt_alloc(rt, row.as.map->count * sizeof(char *)) : NULL;
        row.as.map->key_lengths = row.as.map->count ? rt_alloc(rt, row.as.map->count * sizeof(size_t)) : NULL;
        row.as.map->values = row.as.map->count ? rt_alloc(rt, row.as.map->count * sizeof(Value)) : NULL;
        for (size_t i = 0; i < row.as.map->count; i++) {
            row.as.map->keys[i] = stream->csv_headers.as.list.items[i].as.string;
            row.as.map->key_lengths[i] = stream->csv_headers.as.list.items[i].string_length;
            row.as.map->values[i] = fields.as.list.items[i];
        }
        *out = row;
        return true;
    }
    if (stream->kind == STREAM_CSV_ENCODE) {
        Value row;
        if (stream->inner.kind == V_MAP) {
            row = stream->inner;
            stream->inner = null_value();
        } else if (!stream_next(rt, site, stream->source.as.stream, &row)) {
            stream_close(stream); return false;
        }
        if (row.kind != V_MAP) {
            runtime_type_error(rt, site, "encode_csv Stream items must be Map");
            stream_close(stream);
            return false;
        }
        if (!stream->initialized) {
            stream->initialized = true;
            stream->csv_headers = list_new(rt, row.as.map->count);
            for (size_t i = 0; i < row.as.map->count; i++)
                stream->csv_headers.as.list.items[i] = string_n(rt, row.as.map->keys[i],
                                                                 row.as.map->key_lengths[i]);
            if (stream->csv_use_header && !stream->csv_header_emitted) {
                stream->csv_header_emitted = true;
                stream->inner = row;
                *out = csv_encode_row(rt, site, stream->csv_headers,
                                      stream->delimiter, stream->quote);
                return !rt->failed;
            }
        }
        Value values = list_new(rt, stream->csv_headers.as.list.count);
        for (size_t i = 0; i < values.as.list.count; i++)
            values.as.list.items[i] = map_get(row, stream->csv_headers.as.list.items[i].as.string);
        *out = csv_encode_row(rt, site, values, stream->delimiter, stream->quote);
        return !rt->failed;
    }
    if (stream->kind == STREAM_LIST) {
        if (stream->index >= stream->source.as.list.count) {
            stream_close(stream);
            return false;
        }
        *out = stream->source.as.list.items[stream->index++];
        return true;
    }
    if (stream->kind == STREAM_RANGE) {
        int64_t current = stream->source.as.range.start + (int64_t)stream->index;
        if (current >= stream->source.as.range.end) {
            stream_close(stream);
            return false;
        }
        stream->index++;
        *out = int_value(current);
        return true;
    }
    if (stream->kind == STREAM_SORT || stream->kind == STREAM_GROUP) {
        if (!stream->initialized) {
            stream->initialized = true;
            size_t count = 0, capacity = 8;
            /* These temporary Values can contain GC-managed pointers and remain
               live across selector/list allocations, so their backing arrays
               must also be visible to the collector. */
            Value *items = rt_alloc(rt, capacity * sizeof(Value));
            Value *keys = rt_alloc(rt, capacity * sizeof(Value));
            Value item;
            while (stream_next(rt, site, stream->source.as.stream, &item)) {
                if (count >= HHY_MAX_COLLECTION_ITEMS) {
                    runtime_error_kind(rt, site, "ResourceLimitError", "HHY_COLLECTION_LIMIT",
                                       "Flow materialization exceeds 1000000 item limit");
                    break;
                }
                if (count == capacity) {
                    capacity *= 2;
                    Value *grown_items = rt_alloc(rt, capacity * sizeof(Value));
                    Value *grown_keys = rt_alloc(rt, capacity * sizeof(Value));
                    memcpy(grown_items, items, count * sizeof(Value));
                    memcpy(grown_keys, keys, count * sizeof(Value));
                    items = grown_items;
                    keys = grown_keys;
                }
                items[count] = item;
                keys[count] = call_stream_value(rt, stream->env, site, stream->function, 1, &item);
                if (rt->failed) break;
                if (stream->kind == STREAM_SORT && !sortable_scalar(keys[count])) {
                    runtime_type_error(rt, site, "sort_by keys must be comparable scalars");
                    break;
                }
                if (stream->kind == STREAM_GROUP && !hashable_scalar(keys[count])) {
                    runtime_type_error(rt, site, "group_by keys must be hashable scalars");
                    break;
                }
                count++;
            }
            stream_close(stream->source.as.stream);
            if (stream->kind == STREAM_SORT && !rt->failed) {
                if (stable_sort_values(rt, site, items, keys, count, stream->descending)) {
                    stream->materialized = list_new(rt, count);
                    if (count)
                        memcpy(stream->materialized.as.list.items, items, count * sizeof(Value));
                }
            } else if (stream->kind == STREAM_GROUP && !rt->failed) {
                size_t group_count = 0;
                Value *group_keys = rt_alloc(rt, (count ? count : 1) * sizeof(Value));
                Value *group_lists = rt_alloc(rt, (count ? count : 1) * sizeof(Value));
                size_t *group_sizes = hhy_alloc((count ? count : 1) * sizeof(size_t));
                size_t *item_groups = hhy_alloc((count ? count : 1) * sizeof(size_t));
                size_t slot_count = 8;
                while (slot_count < count * 2 && slot_count <= SIZE_MAX / 2) slot_count *= 2;
                size_t *slots = hhy_alloc(slot_count * sizeof(size_t));
                for (size_t slot = 0; slot < slot_count; slot++) slots[slot] = SIZE_MAX;
                for (size_t i = 0; i < count; i++) {
                    size_t slot = (size_t)(hash_scalar(keys[i]) & (uint64_t)(slot_count - 1));
                    while (slots[slot] != SIZE_MAX &&
                           !equal_values(group_keys[slots[slot]], keys[i]))
                        slot = (slot + 1) & (slot_count - 1);
                    size_t group = slots[slot];
                    if (group == SIZE_MAX) {
                        group = group_count;
                        slots[slot] = group_count;
                        group_keys[group] = keys[i];
                        group_sizes[group] = 0;
                        group_count++;
                    }
                    item_groups[i] = group;
                    group_sizes[group]++;
                }
                free(slots);
                for (size_t group = 0; group < group_count; group++) {
                    group_lists[group] = list_new(rt, group_sizes[group]);
                    group_lists[group].as.list.count = 0;
                }
                for (size_t i = 0; i < count; i++) {
                    Value *target = group_lists[item_groups[i]].as.list.items;
                    target[group_lists[item_groups[i]].as.list.count++] = items[i];
                }
                stream->materialized = list_new(rt, group_count);
                for (size_t i = 0; i < group_count; i++) {
                    const char *group_names[] = {"key", "values"};
                    Value group_values[] = {group_keys[i], group_lists[i]};
                    stream->materialized.as.list.items[i] =
                        map_with_entries(rt, V_MAP, 2, group_names, group_values);
                }
                free(group_sizes); free(item_groups);
            }
            if (rt->failed) return false;
        }
        if (stream->index >= stream->materialized.as.list.count) {
            stream_close(stream);
            return false;
        }
        *out = stream->materialized.as.list.items[stream->index++];
        return true;
    }
    if (stream->kind == STREAM_TAKE && stream->remaining == 0) {
        stream_close(stream);
        return false;
    }
    if (stream->kind == STREAM_SKIP) {
        Value skipped;
        while (stream->remaining > 0) {
            if (!stream_next(rt, site, stream->source.as.stream, &skipped)) {
                stream_close(stream);
                return false;
            }
            stream->remaining--;
        }
    }
    if (stream->kind == STREAM_FLAT_MAP) {
        for (;;) {
            if (stream->inner.kind == V_STREAM) {
                if (stream_next(rt, site, stream->inner.as.stream, out)) return true;
                stream_close(stream->inner.as.stream);
                stream->inner = null_value();
            }
            Value outer;
            if (!stream_next(rt, site, stream->source.as.stream, &outer)) {
                stream_close(stream);
                return false;
            }
            Value inner = call_stream_value(rt, stream->env, site, stream->function, 1, &outer);
            if (inner.kind != V_STREAM || inner.as.stream->claimed) {
                runtime_type_error(rt, site, "flat_map closure must return a fresh Stream");
                stream_close(stream);
                return false;
            }
            inner.as.stream->claimed = true;
            stream->inner = inner;
        }
    }
    Value item;
    while (stream_next(rt, site, stream->source.as.stream, &item)) {
        if (stream->kind == STREAM_MAP) {
            *out = call_stream_value(rt, stream->env, site, stream->function, 1, &item);
            return !rt->failed;
        }
        if (stream->kind == STREAM_INSPECT) {
            (void)call_stream_value(rt, stream->env, site, stream->function, 1, &item);
            if (rt->failed) return false;
            *out = item;
            return true;
        }
        if (stream->kind == STREAM_WHERE) {
            Value accepted = call_stream_value(rt, stream->env, site, stream->function, 1, &item);
            bool keep = false;
            if (!require_bool(rt, site, accepted, &keep)) return false;
            if (keep) { *out = item; return true; }
            continue;
        }
        if (stream->kind == STREAM_DISTINCT) {
            bool duplicate = false;
            if (!hashable_scalar(item)) {
                runtime_type_error(rt, site, "distinct accepts hashable scalar values only");
                return false;
            }
            if (stream->seen_slot_count == 0 ||
                (stream->seen_count + 1) * 10 >= stream->seen_slot_count * 7) {
                size_t next = stream->seen_slot_count == 0 ? 16 : stream->seen_slot_count * 2;
                size_t *slots = hhy_alloc(next * sizeof(size_t));
                memset(slots, 0, next * sizeof(size_t));
                for (size_t i = 0; i < stream->seen_count; i++) {
                    size_t existing = (size_t)(hash_scalar(stream->seen[i]) & (uint64_t)(next - 1));
                    while (slots[existing] != 0) existing = (existing + 1) & (next - 1);
                    slots[existing] = i + 1;
                }
                free(stream->seen_slots);
                stream->seen_slots = slots;
                stream->seen_slot_count = next;
            }
            size_t slot = (size_t)(hash_scalar(item) & (uint64_t)(stream->seen_slot_count - 1));
            while (stream->seen_slots[slot] != 0) {
                size_t index = stream->seen_slots[slot] - 1;
                if (equal_values(stream->seen[index], item)) { duplicate = true; break; }
                slot = (slot + 1) & (stream->seen_slot_count - 1);
            }
            if (duplicate) continue;
            if (stream->seen_count == stream->seen_capacity) {
                size_t old_capacity = stream->seen_capacity;
                stream->seen_capacity = old_capacity < 8 ? 8 : old_capacity * 2;
                stream->seen = rt_value_array_grow(rt, stream->seen, old_capacity,
                                                   stream->seen_capacity);
            }
            stream->seen[stream->seen_count] = item;
            stream->seen_slots[slot] = ++stream->seen_count;
            *out = item;
            return true;
        }
        if (stream->kind == STREAM_DEBOUNCE) {
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            uint64_t timestamp = (uint64_t)now.tv_sec * 1000000000ULL + (uint64_t)now.tv_nsec;
            size_t found = stream->seen_count;
            for (size_t i = 0; i < stream->seen_count; i++) {
                bool same = equal_values(stream->seen[i], item);
                if (!same && stream->seen[i].kind == V_MAP && item.kind == V_MAP) {
                    Value old_path = map_get(stream->seen[i], "path"), new_path = map_get(item, "path");
                    Value old_kind = map_get(stream->seen[i], "kind"), new_kind = map_get(item, "kind");
                    same = old_path.kind != V_NULL && equal_values(old_path, new_path) &&
                           equal_values(old_kind, new_kind);
                }
                if (same) { found = i; break; }
            }
            if (found < stream->seen_count) {
                uint64_t elapsed = timestamp - stream->seen_times[found];
                stream->seen[found] = item;
                stream->seen_times[found] = timestamp;
                if ((double)elapsed < stream->interval_ns) continue;
            } else {
                if (stream->seen_count >= 65536) {
                    runtime_error_kind(rt, site, "ResourceLimitError", "HHY_DEBOUNCE_KEY_LIMIT",
                                       "debounce key limit of 65536 exceeded");
                    stream_close(stream); return false;
                }
                if (stream->seen_count == stream->seen_capacity) {
                    stream->seen_capacity = stream->seen_capacity < 8 ? 8 : stream->seen_capacity * 2;
                    stream->seen = rt_value_array_grow(rt, stream->seen,
                                                       stream->seen_count,
                                                       stream->seen_capacity);
                    stream->seen_times = hhy_realloc(stream->seen_times,
                                                     stream->seen_capacity * sizeof(uint64_t));
                }
                stream->seen[stream->seen_count] = item;
                stream->seen_times[stream->seen_count++] = timestamp;
            }
            *out = item;
            return true;
        }
        if (stream->kind == STREAM_TAKE) {
            stream->remaining--;
            *out = item;
            return true;
        }
        if (stream->kind == STREAM_SKIP) {
            *out = item;
            return true;
        }
    }
    stream_close(stream);
    return false;
}

static bool parallel_launch(Runtime *rt, const HhyNode *site, Stream *stream) {
    Value input;
    if (!stream_next(rt, site, stream->source.as.stream, &input)) {
        if (!rt->failed) stream->upstream_done = true;
        return false;
    }
    if (rt->active_processes >= rt->limits.max_processes) {
        runtime_error_kind(rt, site, "ResourceLimitError", "HHY_PROCESS_LIMIT",
                           "parallel worker exceeds RuntimeLimits.max_processes");
        return false;
    }
    FILE *file = runtime_tmpfile(rt, site);
    if (file == NULL) {
        runtime_io_error(rt, site, "cannot create parallel result buffer");
        return false;
    }
    pid_t pid = fork();
    if (pid < 0) {
        runtime_fclose(rt, file);
        runtime_process_error(rt, site, "cannot start parallel worker");
        return false;
    }
    if (pid == 0) {
        hhy_interrupt_requested = 0;
        struct sigaction terminate_action = {0};
        terminate_action.sa_handler = hhy_signal_handler;
        sigemptyset(&terminate_action.sa_mask);
        sigaction(SIGTERM, &terminate_action, NULL);
        rt->failed = false;
        rt->exit_code = 0;
        rt->signal = SIGNAL_NONE;
        Value result = call_stream_value(rt, stream->env, site, stream->function, 1, &input);
        uint8_t status = rt->failed ? 1 : 0;
        rewind(file);
        bool written = binary_write(file, &status, sizeof(status));
        if (status == 0) written = written && value_write(file, result);
        else written = written && value_write(file, rt->error_value);
        fflush(file);
        _exit(written ? 0 : 125);
    }
    ParallelJob *job = &stream->jobs[stream->job_count++];
    job->pid = pid;
    job->file = file;
    job->active = true;
    rt->active_processes++;
    return true;
}

static bool parallel_next(Runtime *rt, const HhyNode *site, Stream *stream, Value *out) {
    while (!stream->upstream_done && stream->job_count < stream->max_parallel) {
        if (!parallel_launch(rt, site, stream)) {
            if (rt->failed) { stream_close(stream); return false; }
            break;
        }
    }
    if (stream->job_count == 0) {
        stream_close(stream);
        return false;
    }
    ParallelJob job = stream->jobs[0];
    int child_status = 0;
    for (;;) {
        pid_t waited = waitpid(job.pid, &child_status, WNOHANG);
        if (waited == job.pid) break;
        if (waited < 0 && errno != EINTR) {
            runtime_process_error(rt, site, "cannot wait for parallel worker");
            stream_close(stream);
            return false;
        }
        if (runtime_check_cancel(rt, site)) {
            stream_close(stream);
            return false;
        }
        struct timespec pause = {.tv_sec = 0, .tv_nsec = 10000000L};
        nanosleep(&pause, NULL);
    }
    stream->jobs[0].active = false;
    if (rt->active_processes > 0) rt->active_processes--;
    for (size_t i = 1; i < stream->job_count; i++) stream->jobs[i - 1] = stream->jobs[i];
    stream->job_count--;
    rewind(job.file);
    uint8_t result_status = 1;
    bool read_ok = WIFEXITED(child_status) && WEXITSTATUS(child_status) == 0 &&
                   binary_read(job.file, &result_status, sizeof(result_status)) &&
                   value_read(rt, job.file, out, 0);
    runtime_fclose(rt, job.file);
    if (!read_ok) {
        if (WIFEXITED(child_status) && WEXITSTATUS(child_status) == 125)
            runtime_type_error(rt, site, "parallel worker returned a non-sendable value");
        else
            runtime_process_error(rt, site, "parallel worker returned an invalid result");
        stream_close(stream);
        return false;
    }
    if (result_status != 0) {
        rt->failed = true;
        rt->exit_code = 1;
        rt->error_value = *out;
        rt->error_line = site->token.line;
        rt->error_column = site->token.column;
        stream_close(stream);
        return false;
    }
    while (!stream->upstream_done && stream->job_count < stream->max_parallel) {
        if (!parallel_launch(rt, site, stream)) break;
    }
    return !rt->failed;
}

static bool stream_claim(Runtime *rt, const HhyNode *site, Value value) {
    if (value.kind != V_STREAM) {
        runtime_type_error(rt, site, "terminal Flow operator expects Stream");
        return false;
    }
    if (value.as.stream->claimed) {
        runtime_value_error(rt, site, "Stream has already been consumed");
        return false;
    }
    value.as.stream->claimed = true;
    return true;
}

static bool stream_known_unbounded(Stream *stream) {
    if (stream == NULL) return false;
    if (stream->kind == STREAM_EVERY || stream->kind == STREAM_WATCH) return true;
    if (stream->kind == STREAM_TAKE) return false;
    return stream->source.kind == V_STREAM && stream_known_unbounded(stream->source.as.stream);
}

static bool require_bounded_stream(Runtime *rt, const HhyNode *site, Value value,
                                   const char *operation) {
    if (value.kind == V_STREAM && stream_known_unbounded(value.as.stream)) {
        char message[192];
        snprintf(message, sizeof(message),
                 "%s requires a bounded Stream; apply take(n) before this barrier", operation);
        runtime_error_kind(rt, site, "PlanError", "HHY_UNBOUNDED_BARRIER", message);
        return false;
    }
    return true;
}

static const char *bytecode_specialization_reason_name(BytecodeSpecializationReason reason) {
    static const char *const names[] = {
        [BYTECODE_SPECIALIZATION_SELECTED] = "selected",
        [BYTECODE_SPECIALIZATION_DISABLED] = "disabled",
        [BYTECODE_SPECIALIZATION_NOT_BYTECODE] = "not_bytecode",
        [BYTECODE_SPECIALIZATION_UNSUPPORTED_STREAM] = "unsupported_stream",
        [BYTECODE_SPECIALIZATION_TOO_MANY_STAGES] = "too_many_stages",
        [BYTECODE_SPECIALIZATION_DYNAMIC_CALLABLE] = "dynamic_callable",
        [BYTECODE_SPECIALIZATION_UNSUPPORTED_EXPRESSION] = "unsupported_expression",
        [BYTECODE_SPECIALIZATION_UNSUPPORTED_SOURCE] = "unsupported_source",
        [BYTECODE_SPECIALIZATION_RUNTIME_FALLBACK] = "runtime_fallback"
    };
    size_t count = sizeof(names) / sizeof(names[0]);
    return (size_t)reason < count && names[reason] != NULL ? names[reason] : "unknown";
}

static void bytecode_specialization_report(Runtime *rt,
                                           BytecodeSpecializationReason reason,
                                           Stream *const *stages, size_t stage_count) {
    HhyProfileOptimizationStage profile_stages[HHY_PROFILE_MAX_OPTIMIZATION_STAGES];
    memset(profile_stages, 0, sizeof(profile_stages));
    for (size_t output = 0; output < stage_count; output++) {
        Stream *stage = stages[stage_count - output - 1];
        HhyProfileOptimizationStage *profile_stage = &profile_stages[output];
        profile_stage->operation = "distinct_stable";
        if (stage->kind == STREAM_MAP || stage->kind == STREAM_WHERE) {
            const BytecodeFunctionTarget *target =
                (const BytecodeFunctionTarget *)stage->function.as.function.node;
            if (target == NULL || target->stream_kernel == NULL) {
                profile_stage->operation = "unverified";
                continue;
            }
            const HhyStreamKernel *kernel = target->stream_kernel;
            profile_stage->operation = kernel->result == HHY_KERNEL_RESULT_BOOL ?
                "verified_bool_kernel" : "verified_int_kernel";
            profile_stage->kernel_version = kernel->version;
            profile_stage->opcode_count = kernel->instruction_count;
            for (size_t opcode = 0; opcode < profile_stage->opcode_count; opcode++)
                profile_stage->opcodes[opcode] =
                    hhy_stream_kernel_opcode_name(kernel->instructions[opcode].opcode);
        }
    }
    hhy_profiler_optimization(rt->profiler,
                              reason == BYTECODE_SPECIALIZATION_SELECTED,
                              bytecode_specialization_reason_name(reason),
                              profile_stages, stage_count);
    const char *enabled = getenv("HHY_BYTECODE_SPECIALIZATION_REPORT");
    if (enabled == NULL || strcmp(enabled, "json") != 0) return;
    fprintf(stderr,
            "{\"schema_version\":1,\"event\":\"stream_specialization\","
            "\"selected\":%s,\"reason\":\"%s\",\"stages\":%zu,"
            "\"operations\":[",
            reason == BYTECODE_SPECIALIZATION_SELECTED ? "true" : "false",
            bytecode_specialization_reason_name(reason), stage_count);
    for (size_t reverse = stage_count; reverse > 0; reverse--) {
        Stream *stage = stages[reverse - 1];
        const char *name = "distinct_stable";
        if (stage->kind == STREAM_MAP || stage->kind == STREAM_WHERE) {
            const BytecodeFunctionTarget *target =
                (const BytecodeFunctionTarget *)stage->function.as.function.node;
            if (target == NULL || target->stream_kernel == NULL)
                name = "unverified";
            else
                name = target->stream_kernel->result == HHY_KERNEL_RESULT_BOOL ?
                    "verified_bool_kernel" : "verified_int_kernel";
        }
        fprintf(stderr, "%s\"%s\"", reverse == stage_count ? "" : ",", name);
    }
    fputs("]}\n", stderr);
}

static bool bytecode_execute_stream_kernel(const HhyStreamKernel *kernel, Value item,
                                           Value *result) {
    if (kernel == NULL || item.kind != V_INT || result == NULL ||
        kernel->version != HHY_STREAM_KERNEL_VERSION || kernel->max_stack == 0 ||
        kernel->max_stack > HHY_STREAM_KERNEL_MAX_INSTRUCTIONS ||
        kernel->instruction_count == 0 ||
        kernel->instruction_count > HHY_STREAM_KERNEL_MAX_INSTRUCTIONS) return false;
    int64_t stack[HHY_STREAM_KERNEL_MAX_INSTRUCTIONS];
    size_t depth = 0;
    for (uint32_t i = 0; i < kernel->instruction_count; i++) {
        HhyStreamKernelInstruction instruction = kernel->instructions[i];
        switch (instruction.opcode) {
            case HHY_KERNEL_LOAD_ITEM:
                if (depth >= kernel->max_stack) return false;
                stack[depth++] = item.as.integer;
                break;
            case HHY_KERNEL_LOAD_INT:
                if (depth >= kernel->max_stack) return false;
                stack[depth++] = instruction.immediate;
                break;
            case HHY_KERNEL_MUL_INT_CHECKED: {
                int64_t product;
                if (depth < 2 || __builtin_mul_overflow(stack[depth - 2], stack[depth - 1],
                                                        &product)) return false;
                depth--;
                stack[depth - 1] = product;
                break;
            }
            case HHY_KERNEL_MOD_INT_CHECKED: {
                if (depth < 2 || stack[depth - 1] == 0 ||
                    (stack[depth - 2] == INT64_MIN && stack[depth - 1] == -1)) return false;
                int64_t remainder = stack[depth - 2] % stack[depth - 1];
                depth--;
                stack[depth - 1] = remainder;
                break;
            }
            case HHY_KERNEL_EQ_INT: {
                if (depth < 2) return false;
                bool equal = stack[depth - 2] == stack[depth - 1];
                depth--;
                stack[depth - 1] = equal ? 1 : 0;
                break;
            }
            case HHY_KERNEL_RETURN:
                if (depth != 1) return false;
                *result = kernel->result == HHY_KERNEL_RESULT_BOOL ?
                    bool_value(stack[0] != 0) : int_value(stack[0]);
                return true;
            case HHY_KERNEL_OP_COUNT:
                return false;
        }
        if (depth > kernel->max_stack) return false;
    }
    return false;
}

static Value stream_collect(Runtime *rt, const HhyNode *site, Value value) {
    if (!require_bounded_stream(rt, site, value, "collect")) return null_value();
    if (!stream_claim(rt, site, value)) return null_value();
    /* A bytecode pipeline whose closures are self-contained argument expressions can
       be pulled in one loop.  This retains the ordinary Stream objects (and thus
       claim/close/error semantics), but removes recursive stream_next/call dispatch
       from the per-item hot path.  Unknown/effectful closures and every non-local
       Stream kind fall through to the general implementation below. */
    Stream *stages[16];
    size_t stage_count = 0;
    Stream *base = value.kind == V_STREAM ? value.as.stream : NULL;
    BytecodeSpecializationReason specialization_reason = BYTECODE_SPECIALIZATION_SELECTED;
    const char *specialization_setting = getenv("HHY_BYTECODE_SPECIALIZATION");
    bool specialization_disabled = specialization_setting != NULL &&
                                   strcmp(specialization_setting, "off") == 0;
    bool fused = rt->engine == HHY_ENGINE_BYTECODE && !specialization_disabled;
    if (specialization_disabled)
        specialization_reason = BYTECODE_SPECIALIZATION_DISABLED;
    else if (rt->engine != HHY_ENGINE_BYTECODE)
        specialization_reason = BYTECODE_SPECIALIZATION_NOT_BYTECODE;
    while (fused && base != NULL &&
           (base->kind == STREAM_MAP || base->kind == STREAM_WHERE ||
            base->kind == STREAM_DISTINCT)) {
        if (stage_count == sizeof(stages) / sizeof(stages[0])) {
            specialization_reason = BYTECODE_SPECIALIZATION_TOO_MANY_STAGES;
            fused = false;
            break;
        }
        if (base->kind != STREAM_DISTINCT) {
            if (base->function.kind != V_FUNCTION || !base->function.as.function.is_bytecode ||
                !base->function.as.function.is_closure ||
                base->function.as.function.builtin != NULL) {
                specialization_reason = BYTECODE_SPECIALIZATION_DYNAMIC_CALLABLE;
                fused = false;
                break;
            }
            const BytecodeFunctionTarget *target =
                (const BytecodeFunctionTarget *)base->function.as.function.node;
            if (target == NULL || !target->has_fast_argument_expression ||
                target->stream_kernel == NULL) {
                specialization_reason = BYTECODE_SPECIALIZATION_UNSUPPORTED_EXPRESSION;
                fused = false;
                break;
            }
        }
        stages[stage_count++] = base;
        if (base->source.kind != V_STREAM) {
            specialization_reason = BYTECODE_SPECIALIZATION_UNSUPPORTED_SOURCE;
            fused = false;
            break;
        }
        base = base->source.as.stream;
    }
    if (base == NULL || (base->kind != STREAM_RANGE && base->kind != STREAM_LIST)) {
        if (fused) specialization_reason = BYTECODE_SPECIALIZATION_UNSUPPORTED_SOURCE;
        fused = false;
    }
    if (!fused && specialization_reason == BYTECODE_SPECIALIZATION_SELECTED)
        specialization_reason = BYTECODE_SPECIALIZATION_UNSUPPORTED_STREAM;

    size_t count = 0, capacity = 8;
    Value *temporary = NULL;
    Value item;
    if (fused) {
        size_t kernel_profile_entries[HHY_PROFILE_MAX_OPTIMIZATION_STAGES];
        for (size_t i = 0; i < stage_count; i++) kernel_profile_entries[i] = SIZE_MAX;
        for (size_t i = 0; i < stage_count; i++) {
            Stream *stage = stages[i];
            if (stage->kind != STREAM_MAP && stage->kind != STREAM_WHERE) continue;
            const BytecodeFunctionTarget *target =
                (const BytecodeFunctionTarget *)stage->function.as.function.node;
            const HhyStreamKernel *kernel = target->stream_kernel;
            HhyInstruction source = target->chunk->code[kernel->source_instruction];
            kernel_profile_entries[i] = hhy_profiler_register(
                rt->profiler,
                kernel->result == HHY_KERNEL_RESULT_BOOL ?
                    "<stream-kernel:Bool>" : "<stream-kernel:Int>",
                rt->source->path, source.line, source.column);
        }
        size_t collect_profile_entry = hhy_profiler_register(
            rt->profiler, "<stream-collect>", rt->source->path,
            site->token.line, site->token.column);
        size_t maximum_output = HHY_MAX_COLLECTION_ITEMS;
        if (base->kind == STREAM_LIST && base->source.as.list.count < maximum_output)
            maximum_output = base->source.as.list.count;
        else if (base->kind == STREAM_RANGE &&
                 base->source.as.range.end > base->source.as.range.start) {
            uint64_t range_count = (uint64_t)base->source.as.range.end -
                                   (uint64_t)base->source.as.range.start;
            if (range_count < maximum_output) maximum_output = (size_t)range_count;
        }
        /* The source cardinality is an exact upper bound for map/where/distinct.
           Start with a bounded chunk so highly selective pipelines do not reserve
           a million Values, then grow the final list directly without a second
           materialization buffer. */
        size_t result_capacity = maximum_output < 4096 ? maximum_output : 4096;
        size_t collect_previous = hhy_profiler_enter_registered(rt->profiler,
                                                                collect_profile_entry);
        Value result = list_new(rt, result_capacity);
        hhy_profiler_leave(rt->profiler, collect_previous);
        result.as.list.count = 0;
        for (;;) {
            /* Match loop execution's bounded safepoint cadence.  Calling
               clock_gettime for every scalar would dominate a fused pipeline. */
            if (runtime_safepoint(rt, site)) break;
            if (base->kind == STREAM_RANGE) {
                int64_t current = base->source.as.range.start + (int64_t)base->index;
                if (current >= base->source.as.range.end) break;
                base->index++;
                item = int_value(current);
            } else {
                if (base->index >= base->source.as.list.count) break;
                item = base->source.as.list.items[base->index++];
            }
            bool keep = true;
            for (size_t reverse = stage_count; reverse > 0 && keep && !rt->failed; reverse--) {
                Stream *stage = stages[reverse - 1];
                if (stage->kind == STREAM_MAP || stage->kind == STREAM_WHERE) {
                    const BytecodeFunctionTarget *target =
                        (const BytecodeFunctionTarget *)stage->function.as.function.node;
                    Value evaluated;
                    size_t stage_index = reverse - 1;
                    size_t kernel_previous = hhy_profiler_enter_registered(
                        rt->profiler, kernel_profile_entries[stage_index]);
                    bool used_kernel = bytecode_execute_stream_kernel(target->stream_kernel,
                                                                       item, &evaluated);
                    hhy_profiler_leave(rt->profiler, kernel_previous);
                    if (!used_kernel && !bytecode_eval_argument_expression(rt,
                            (BytecodeCursor){.chunk = target->chunk,
                                             .instruction = target->fast_expression},
                            target->parameter_constant, item, &evaluated)) {
                        specialization_reason = BYTECODE_SPECIALIZATION_RUNTIME_FALLBACK;
                        fused = false;
                        break;
                    }
                    if (stage->kind == STREAM_MAP) item = evaluated;
                    else {
                        bool accepted = false;
                        if (!require_bool(rt, site, evaluated, &accepted)) break;
                        keep = accepted;
                    }
                    continue;
                }
                if (!hashable_scalar(item)) {
                    runtime_type_error(rt, site, "distinct accepts hashable scalar values only");
                    break;
                }
                if (stage->seen_slot_count == 0 ||
                    (stage->seen_count + 1) * 10 >= stage->seen_slot_count * 7) {
                    size_t next = stage->seen_slot_count == 0 ? 16 : stage->seen_slot_count * 2;
                    size_t *slots = hhy_alloc(next * sizeof(size_t));
                    memset(slots, 0, next * sizeof(size_t));
                    for (size_t i = 0; i < stage->seen_count; i++) {
                        size_t slot = (size_t)(hash_scalar(stage->seen[i]) & (uint64_t)(next - 1));
                        while (slots[slot] != 0) slot = (slot + 1) & (next - 1);
                        slots[slot] = i + 1;
                    }
                    free(stage->seen_slots);
                    stage->seen_slots = slots;
                    stage->seen_slot_count = next;
                }
                size_t slot = (size_t)(hash_scalar(item) &
                                       (uint64_t)(stage->seen_slot_count - 1));
                while (stage->seen_slots[slot] != 0) {
                    size_t index = stage->seen_slots[slot] - 1;
                    if (equal_values(stage->seen[index], item)) { keep = false; break; }
                    slot = (slot + 1) & (stage->seen_slot_count - 1);
                }
                if (!keep) continue;
                if (stage->seen_count == stage->seen_capacity) {
                    size_t old_capacity = stage->seen_capacity;
                    stage->seen_capacity = old_capacity < 8 ? 8 : old_capacity * 2;
                    stage->seen = rt_value_array_grow(rt, stage->seen, old_capacity,
                                                      stage->seen_capacity);
                }
                stage->seen[stage->seen_count] = item;
                stage->seen_slots[slot] = ++stage->seen_count;
            }
            if (!fused || rt->failed) break;
            if (!keep) continue;
            if (count >= HHY_MAX_COLLECTION_ITEMS) {
                runtime_error_kind(rt, site, "ResourceLimitError", "HHY_COLLECTION_LIMIT",
                                   "collection exceeds 1000000 item limit");
                break;
            }
            if (count == result_capacity) {
                size_t next = result_capacity < 8 ? 8 : result_capacity * 2;
                if (next > maximum_output) next = maximum_output;
                collect_previous = hhy_profiler_enter_registered(rt->profiler,
                                                                 collect_profile_entry);
                Value *grown = rt_value_array_grow(rt, result.as.list.items, count, next);
                hhy_profiler_leave(rt->profiler, collect_previous);
                result.as.list.items = grown;
                result_capacity = next;
            }
            result.as.list.items[count++] = item;
        }
        bytecode_specialization_report(rt,
            fused && !rt->failed ? BYTECODE_SPECIALIZATION_SELECTED : specialization_reason,
            stages, stage_count);
        stream_close(value.as.stream);
        result.as.list.count = count;
        return result;
    }
    bytecode_specialization_report(rt, specialization_reason, stages, stage_count);
    temporary = rt_value_array_grow(rt, NULL, 0, capacity);
    while (stream_next(rt, site, value.as.stream, &item)) {
        if (count >= HHY_MAX_COLLECTION_ITEMS) {
            runtime_error_kind(rt, site, "ResourceLimitError", "HHY_COLLECTION_LIMIT",
                               "collection exceeds 1000000 item limit");
            break;
        }
        if (count == capacity) {
            capacity *= 2;
            temporary = rt_value_array_grow(rt, temporary, count, capacity);
        }
        temporary[count++] = item;
    }
    stream_close(value.as.stream);
    Value result = list_new(rt, count);
    if (count > 0) memcpy(result.as.list.items, temporary, count * sizeof(Value));
    return result;
}

static const char *require_path(Runtime *rt, const HhyNode *site, Value value) {
    if (value.kind != V_PATH) {
        runtime_type_error(rt, site, "file operation requires Path");
        return NULL;
    }
    return value.as.string;
}

static Value read_text_file(Runtime *rt, const HhyNode *site, const char *path) {
    FILE *file = runtime_fopen(rt, site, path, "rb");
    if (file == NULL) { if (!rt->failed) runtime_io_error(rt, site, "cannot open file for reading"); return null_value(); }
    if (fseek(file, 0, SEEK_END) != 0) { runtime_fclose(rt, file); runtime_io_error(rt, site, "cannot seek file"); return null_value(); }
    long length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0) { runtime_fclose(rt, file); runtime_io_error(rt, site, "cannot read file size"); return null_value(); }
    char *text = rt_alloc(rt, (size_t)length + 1);
    size_t read = fread(text, 1, (size_t)length, file);
    bool failed = ferror(file) != 0;
    runtime_fclose(rt, file);
    if (failed || read != (size_t)length) { runtime_io_error(rt, site, "cannot read complete file"); return null_value(); }
    text[read] = '\0';
    size_t codepoints;
    if (!utf8_count(text, read, &codepoints)) {
        runtime_error_kind(rt, site, "EncodingError", "HHY_UTF8", "text file is not valid UTF-8");
        return null_value();
    }
    Value value = {.kind = V_STRING, .string_length = read}; value.as.string = text; return value;
}

static Value read_bytes_file(Runtime *rt, const HhyNode *site, const char *path) {
    FILE *file = runtime_fopen(rt, site, path, "rb");
    if (file == NULL) { if (!rt->failed) runtime_io_error(rt, site, "cannot open file for reading"); return null_value(); }
    if (fseek(file, 0, SEEK_END) != 0) { runtime_fclose(rt, file); runtime_io_error(rt, site, "cannot seek file"); return null_value(); }
    long length = ftell(file);
    if (length < 0 || (uintmax_t)length > 256 * 1024 * 1024 || fseek(file, 0, SEEK_SET) != 0) {
        runtime_fclose(rt, file); runtime_error_kind(rt, site, "ResourceLimitError", "HHY_FILE_SIZE",
                                        "binary file exceeds 256 MiB limit"); return null_value();
    }
    Value value = bytes_buffer_value(rt, NULL, (size_t)length);
    size_t read = fread(value.as.bytes_buffer.data, 1, (size_t)length, file);
    bool failed = ferror(file) != 0; runtime_fclose(rt, file);
    if (failed || read != (size_t)length) { runtime_io_error(rt, site, "cannot read complete file"); return null_value(); }
    return value;
}

typedef struct {
    bool overwrite;
    bool create_parents;
} OutputOptions;

static bool output_options(Runtime *rt, const HhyNode *site, int argc, Value value,
                           OutputOptions *options) {
    *options = (OutputOptions){.overwrite = true, .create_parents = false};
    if (argc == 0) return true;
    if (value.kind != V_MAP) {
        runtime_type_error(rt, site, "output options must be a Map");
        return false;
    }
    Value overwrite = map_get(value, "overwrite");
    Value create_parents = map_get(value, "create_parents");
    if (overwrite.kind != V_NULL && overwrite.kind != V_BOOL) {
        runtime_type_error(rt, site, "overwrite option must be Bool");
        return false;
    }
    if (create_parents.kind != V_NULL && create_parents.kind != V_BOOL) {
        runtime_type_error(rt, site, "create_parents option must be Bool");
        return false;
    }
    if (overwrite.kind == V_BOOL) options->overwrite = overwrite.as.boolean;
    if (create_parents.kind == V_BOOL) options->create_parents = create_parents.as.boolean;
    return true;
}

static bool ensure_parent_directories(Runtime *rt, const HhyNode *site, const char *path) {
    char *copy = hhy_strndup(path, strlen(path));
    char *slash = strrchr(copy, '/');
    if (slash == NULL || slash == copy) { free(copy); return true; }
    *slash = '\0';
    for (char *cursor = copy + (copy[0] == '/' ? 1 : 0); ; cursor++) {
        if (*cursor != '/' && *cursor != '\0') continue;
        char saved = *cursor; *cursor = '\0';
        if (*copy != '\0' && mkdir(copy, 0777) != 0 && errno != EEXIST) {
            free(copy); runtime_io_error(rt, site, "cannot create output parent directory");
            return false;
        }
        struct stat info;
        if (*copy != '\0' && (stat(copy, &info) != 0 || !S_ISDIR(info.st_mode))) {
            free(copy); runtime_io_error(rt, site, "output parent is not a directory");
            return false;
        }
        *cursor = saved;
        if (saved == '\0') break;
    }
    free(copy);
    return true;
}

static RuntimeCleanup *runtime_register_temporary(Runtime *rt, FILE *file, char *path) {
    RuntimeCleanup *cleanup = hhy_alloc(sizeof(*cleanup));
    cleanup->file = file;
    cleanup->temporary_path = path;
    cleanup->next = rt->cleanups;
    rt->cleanups = cleanup;
    return cleanup;
}

static void runtime_unregister_temporary(Runtime *rt, RuntimeCleanup *cleanup) {
    RuntimeCleanup **link = &rt->cleanups;
    while (*link != NULL && *link != cleanup) link = &(*link)->next;
    if (*link == cleanup) *link = cleanup->next;
    free(cleanup);
}

static int atomic_rename(const char *source, const char *target, bool overwrite) {
    if (overwrite) return rename(source, target);
#ifdef __APPLE__
    return renamex_np(source, target, RENAME_EXCL);
#elif defined(__linux__)
    return renameat2(AT_FDCWD, source, AT_FDCWD, target, RENAME_NOREPLACE);
#else
    struct stat existing;
    if (lstat(target, &existing) == 0) { errno = EEXIST; return -1; }
    return rename(source, target);
#endif
}

static bool write_bytes_atomic(Runtime *rt, const HhyNode *site,
                               const char *path, const void *data, size_t length,
                               OutputOptions options) {
    struct stat existing;
    if (!options.overwrite && lstat(path, &existing) == 0) {
        runtime_io_error(rt, site, "output target already exists");
        return false;
    }
    if (options.create_parents && !ensure_parent_directories(rt, site, path)) return false;
    size_t path_length = strlen(path);
    char *temporary = hhy_alloc(path_length + 18);
    snprintf(temporary, path_length + 18, "%s.hhy-tmp-XXXXXX", path);
    int descriptor = mkstemp(temporary);
    if (descriptor < 0) {
        free(temporary); runtime_io_error(rt, site, "cannot create temporary output file"); return false;
    }
    FILE *file = runtime_fdopen(rt, site, descriptor, "wb");
    if (file == NULL) {
        close(descriptor); unlink(temporary); free(temporary);
        if (!rt->failed) runtime_io_error(rt, site, "cannot open temporary output file");
        return false;
    }
    bool ok = fwrite(data, 1, length, file) == length && fflush(file) == 0 &&
              fsync(descriptor) == 0 && runtime_fclose(rt, file) == 0;
    if (ok) ok = atomic_rename(temporary, path, options.overwrite) == 0;
    if (!ok) {
        unlink(temporary); free(temporary);
        runtime_io_error(rt, site, "cannot atomically write output file"); return false;
    }
    free(temporary);
    return true;
}

static bool write_text_atomic(Runtime *rt, const HhyNode *site,
                              const char *path, const char *text, size_t length,
                              OutputOptions options) {
    return write_bytes_atomic(rt, site, path, text, length, options);
}

static bool write_string_stream_atomic(Runtime *rt, const HhyNode *site, Value input,
                                       const char *path, bool lines, OutputOptions options) {
    if (!require_bounded_stream(rt, site, input, lines ? "save_lines" : "save_text") ||
        !stream_claim(rt, site, input)) return false;
    struct stat existing;
    if (!options.overwrite && lstat(path, &existing) == 0) {
        stream_close(input.as.stream);
        runtime_io_error(rt, site, "output target already exists");
        return false;
    }
    if (options.create_parents && !ensure_parent_directories(rt, site, path)) {
        stream_close(input.as.stream);
        return false;
    }
    size_t path_length = strlen(path);
    char *temporary = hhy_alloc(path_length + 18);
    snprintf(temporary, path_length + 18, "%s.hhy-tmp-XXXXXX", path);
    int descriptor = mkstemp(temporary);
    if (descriptor < 0) {
        free(temporary); stream_close(input.as.stream);
        runtime_io_error(rt, site, "cannot create temporary output file");
        return false;
    }
    FILE *file = runtime_fdopen(rt, site, descriptor, "wb");
    if (file == NULL) {
        close(descriptor); unlink(temporary); free(temporary); stream_close(input.as.stream);
        if (!rt->failed) runtime_io_error(rt, site, "cannot open temporary output file");
        return false;
    }
    RuntimeCleanup *cleanup = runtime_register_temporary(rt, file, temporary);
    bool ok = true;
    Value item;
    while (stream_next(rt, site, input.as.stream, &item)) {
        if (item.kind != V_STRING) {
            runtime_type_error(rt, site, lines ? "save_lines Stream items must be String" :
                                                "save_text Stream items must be String");
            ok = false; break;
        }
        size_t length = item.string_length;
        if (fwrite(item.as.string, 1, length, file) != length ||
            (lines && fputc('\n', file) == EOF)) {
            runtime_io_error(rt, site, "cannot write output stream");
            ok = false; break;
        }
    }
    if (rt->failed) ok = false;
    stream_close(input.as.stream);
    if (ok && (fflush(file) != 0 || fsync(descriptor) != 0)) ok = false;
    runtime_unregister_temporary(rt, cleanup);
    if (runtime_fclose(rt, file) != 0) ok = false;
    if (ok && atomic_rename(temporary, path, options.overwrite) != 0) ok = false;
    if (!ok) {
        unlink(temporary); free(temporary);
        if (!rt->failed) runtime_io_error(rt, site, "cannot atomically save output stream");
        return false;
    }
    free(temporary);
    return true;
}

static bool copy_file_atomic(Runtime *rt, const HhyNode *site,
                             const char *source, const char *target, bool overwrite) {
    struct stat target_info;
    if (!overwrite && lstat(target, &target_info) == 0) {
        runtime_io_error(rt, site, "copy target already exists"); return false;
    }
    FILE *input = runtime_fopen(rt, site, source, "rb");
    if (input == NULL) { if (!rt->failed) runtime_io_error(rt, site, "cannot open copy source"); return false; }
    size_t target_length = strlen(target);
    char *temporary = hhy_alloc(target_length + 18);
    snprintf(temporary, target_length + 18, "%s.hhy-tmp-XXXXXX", target);
    int descriptor = mkstemp(temporary);
    if (descriptor < 0) {
        runtime_fclose(rt, input); free(temporary); runtime_io_error(rt, site, "cannot create copy target"); return false;
    }
    FILE *output = runtime_fdopen(rt, site, descriptor, "wb");
    if (output == NULL) {
        runtime_fclose(rt, input); close(descriptor); unlink(temporary); free(temporary);
        if (!rt->failed) runtime_io_error(rt, site, "cannot open copy target");
        return false;
    }
    char buffer[65536]; bool ok = true; size_t read;
    while ((read = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        if (runtime_check_cancel(rt, site) || fwrite(buffer, 1, read, output) != read) {
            ok = false; break;
        }
    }
    if (ferror(input)) ok = false;
    if (fflush(output) != 0 || fsync(descriptor) != 0) ok = false;
    if (runtime_fclose(rt, input) != 0 || runtime_fclose(rt, output) != 0) ok = false;
    if (ok) ok = atomic_rename(temporary, target, overwrite) == 0;
    if (!ok) {
        unlink(temporary); free(temporary);
        runtime_io_error(rt, site, "copy failed"); return false;
    }
    free(temporary); return true;
}

typedef struct {
    Runtime *rt;
    const HhyNode *site;
    const char *start;
    const char *current;
    const char *end;
    size_t line;
    size_t column;
} JsonParser;

static void json_space(JsonParser *p) {
    while (p->current < p->end) {
        char c = *p->current;
        if (c == ' ' || c == '\t' || c == '\r') { p->current++; p->column++; }
        else if (c == '\n') { p->current++; p->line++; p->column = 1; }
        else break;
    }
}

static void json_error(JsonParser *p, const char *message) {
    if (p->rt->failed) return;
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "JSON line %zu column %zu: %s",
             p->line, p->column, message);
    runtime_error_kind(p->rt, p->site, "ValueError", "HHY_JSON_PARSE", buffer);
}

static bool json_take(JsonParser *p, char expected) {
    if (p->current >= p->end || *p->current != expected) return false;
    p->current++; p->column++; return true;
}

static void json_utf8(char **out, unsigned codepoint) {
    if (codepoint <= 0x7f) *(*out)++ = (char)codepoint;
    else if (codepoint <= 0x7ff) {
        *(*out)++ = (char)(0xc0 | (codepoint >> 6));
        *(*out)++ = (char)(0x80 | (codepoint & 0x3f));
    } else if (codepoint <= 0xffff) {
        *(*out)++ = (char)(0xe0 | (codepoint >> 12));
        *(*out)++ = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        *(*out)++ = (char)(0x80 | (codepoint & 0x3f));
    } else {
        *(*out)++ = (char)(0xf0 | (codepoint >> 18));
        *(*out)++ = (char)(0x80 | ((codepoint >> 12) & 0x3f));
        *(*out)++ = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        *(*out)++ = (char)(0x80 | (codepoint & 0x3f));
    }
}

static int json_hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static Value json_parse_string(JsonParser *p) {
    if (!json_take(p, '"')) { json_error(p, "expected string"); return null_value(); }
    size_t maximum = (size_t)(p->end - p->current) * 3 + 1;
    char *text = rt_alloc(p->rt, maximum), *out = text;
    while (p->current < p->end && *p->current != '"') {
        unsigned char c = (unsigned char)*p->current++;
        p->column++;
        if (c < 0x20) { json_error(p, "control byte in string"); return null_value(); }
        if (c != '\\') { *out++ = (char)c; continue; }
        if (p->current >= p->end) { json_error(p, "unterminated escape"); return null_value(); }
        char escaped = *p->current++; p->column++;
        switch (escaped) {
            case '"': case '\\': case '/': *out++ = escaped; break;
            case 'b': *out++ = '\b'; break; case 'f': *out++ = '\f'; break;
            case 'n': *out++ = '\n'; break; case 'r': *out++ = '\r'; break;
            case 't': *out++ = '\t'; break;
            case 'u': {
                if (p->end - p->current < 4) { json_error(p, "short Unicode escape"); return null_value(); }
                unsigned codepoint = 0;
                for (int i = 0; i < 4; i++) {
                    int digit = json_hex(*p->current++);
                    p->column++;
                    if (digit < 0) { json_error(p, "invalid Unicode escape"); return null_value(); }
                    codepoint = codepoint * 16 + (unsigned)digit;
                }
                if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
                    if (p->end - p->current < 6 || p->current[0] != '\\' ||
                        p->current[1] != 'u') {
                        json_error(p, "high surrogate must be followed by a low surrogate");
                        return null_value();
                    }
                    p->current += 2; p->column += 2;
                    unsigned low = 0;
                    for (int i = 0; i < 4; i++) {
                        int digit = json_hex(*p->current++);
                        p->column++;
                        if (digit < 0) {
                            json_error(p, "invalid low surrogate escape"); return null_value();
                        }
                        low = low * 16 + (unsigned)digit;
                    }
                    if (low < 0xdc00 || low > 0xdfff) {
                        json_error(p, "high surrogate must be followed by a low surrogate");
                        return null_value();
                    }
                    codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
                } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
                    json_error(p, "isolated low surrogate is invalid"); return null_value();
                }
                json_utf8(&out, codepoint);
                break;
            }
            default: json_error(p, "invalid string escape"); return null_value();
        }
    }
    if (!json_take(p, '"')) { json_error(p, "unterminated string"); return null_value(); }
    *out = '\0';
    Value value = {.kind = V_STRING, .string_length = (size_t)(out - text)}; value.as.string = text; return value;
}

static Value json_value(JsonParser *p, size_t depth);

static Value json_parse_array(JsonParser *p, size_t depth) {
    json_take(p, '['); json_space(p);
    size_t count = 0, capacity = 8;
    Value *items = rt_value_array_grow(p->rt, NULL, 0, capacity);
    if (json_take(p, ']')) return list_new(p->rt, 0);
    while (!p->rt->failed) {
        if (count >= HHY_MAX_COLLECTION_ITEMS) {
            json_error(p, "JSON array exceeds 1000000 item limit"); break;
        }
        if (count == capacity) { capacity *= 2; items = rt_value_array_grow(p->rt, items, count, capacity); }
        items[count++] = json_value(p, depth + 1); json_space(p);
        if (json_take(p, ']')) break;
        if (!json_take(p, ',')) { json_error(p, "expected comma or closing bracket"); break; }
        json_space(p);
    }
    Value result = list_new(p->rt, count);
    if (count > 0) memcpy(result.as.list.items, items, count * sizeof(Value));
    return result;
}

static Value json_parse_object(JsonParser *p, size_t depth) {
    json_take(p, '{'); json_space(p);
    size_t count = 0, capacity = 8;
    char **keys = rt_pointer_array_grow(p->rt, NULL, 0, capacity);
    size_t *key_lengths = hhy_alloc(capacity * sizeof(size_t));
    Value *values = rt_value_array_grow(p->rt, NULL, 0, capacity);
    size_t slot_count = 16;
    size_t *key_slots = hhy_alloc(slot_count * sizeof(size_t));
    memset(key_slots, 0, slot_count * sizeof(size_t));
    if (json_take(p, '}')) {
        free(key_lengths); free(key_slots);
        Value empty = {.kind = V_MAP}; empty.as.map = map_storage_new(p->rt, 0); return empty;
    }
    while (!p->rt->failed) {
        if (count >= HHY_MAX_COLLECTION_ITEMS) {
            json_error(p, "JSON object exceeds 1000000 entry limit"); break;
        }
        if (count == capacity) {
            capacity *= 2; keys = rt_pointer_array_grow(p->rt, keys, count, capacity);
            key_lengths = hhy_realloc(key_lengths, capacity * sizeof(size_t));
            values = rt_value_array_grow(p->rt, values, count, capacity);
        }
        Value key = json_parse_string(p); json_space(p);
        if (!json_take(p, ':')) { json_error(p, "expected colon after object key"); break; }
        if ((count + 1) * 10 >= slot_count * 7) {
            size_t next_count = slot_count * 2;
            size_t *next_slots = hhy_alloc(next_count * sizeof(size_t));
            memset(next_slots, 0, next_count * sizeof(size_t));
            for (size_t i = 0; i < count; i++) {
                size_t old_slot = (size_t)(hash_key_bytes(keys[i], key_lengths[i]) & (next_count - 1));
                while (next_slots[old_slot] != 0) old_slot = (old_slot + 1) & (next_count - 1);
                next_slots[old_slot] = i + 1;
            }
            free(key_slots); key_slots = next_slots; slot_count = next_count;
        }
        size_t key_slot = (size_t)(hash_key_bytes(key.as.string, key.string_length) & (slot_count - 1));
        while (key_slots[key_slot] != 0) {
            size_t i = key_slots[key_slot] - 1;
            if (key_lengths[i] == key.string_length &&
                memcmp(keys[i], key.as.string, key.string_length) == 0) {
                json_error(p, "duplicate object key"); break;
            }
            key_slot = (key_slot + 1) & (slot_count - 1);
        }
        if (p->rt->failed) break;
        json_space(p); keys[count] = key.as.string; key_lengths[count] = key.string_length;
        values[count] = json_value(p, depth + 1);
        key_slots[key_slot] = ++count; json_space(p);
        if (json_take(p, '}')) break;
        if (!json_take(p, ',')) { json_error(p, "expected comma or closing brace"); break; }
        json_space(p);
    }
    Value result = {.kind = V_MAP}; result.as.map = map_storage_new(p->rt, count);
    result.as.map->keys = count ? rt_alloc(p->rt, count * sizeof(char *)) : NULL;
    result.as.map->key_lengths = count ? rt_alloc(p->rt, count * sizeof(size_t)) : NULL;
    result.as.map->values = count ? rt_alloc(p->rt, count * sizeof(Value)) : NULL;
    if (count > 0) {
        memcpy(result.as.map->keys, keys, count * sizeof(char *));
        memcpy(result.as.map->key_lengths, key_lengths, count * sizeof(size_t));
        memcpy(result.as.map->values, values, count * sizeof(Value));
    }
    map_build_index(p->rt, result.as.map);
    free(key_lengths); free(key_slots); return result;
}

static Value json_value(JsonParser *p, size_t depth) {
    if (depth > 128) { json_error(p, "maximum nesting depth exceeded"); return null_value(); }
    json_space(p);
    if (p->current >= p->end) { json_error(p, "expected value"); return null_value(); }
    if (*p->current == '"') return json_parse_string(p);
    if (*p->current == '[') return json_parse_array(p, depth);
    if (*p->current == '{') return json_parse_object(p, depth);
    if (p->end - p->current >= 4 && memcmp(p->current, "true", 4) == 0) {
        p->current += 4; p->column += 4; return bool_value(true);
    }
    if (p->end - p->current >= 5 && memcmp(p->current, "false", 5) == 0) {
        p->current += 5; p->column += 5; return bool_value(false);
    }
    if (p->end - p->current >= 4 && memcmp(p->current, "null", 4) == 0) {
        p->current += 4; p->column += 4; return null_value();
    }
    const char *number_start = p->current;
    if (*p->current == '-') { p->current++; p->column++; }
    if (p->current >= p->end || *p->current < '0' || *p->current > '9') {
        json_error(p, "invalid value"); return null_value();
    }
    if (*p->current == '0') { p->current++; p->column++; }
    else while (p->current < p->end && *p->current >= '0' && *p->current <= '9') { p->current++; p->column++; }
    bool floating = false;
    if (p->current < p->end && *p->current == '.') {
        floating = true; p->current++; p->column++;
        if (p->current >= p->end || *p->current < '0' || *p->current > '9') { json_error(p, "invalid fraction"); return null_value(); }
        while (p->current < p->end && *p->current >= '0' && *p->current <= '9') { p->current++; p->column++; }
    }
    if (p->current < p->end && (*p->current == 'e' || *p->current == 'E')) {
        floating = true; p->current++; p->column++;
        if (p->current < p->end && (*p->current == '+' || *p->current == '-')) { p->current++; p->column++; }
        if (p->current >= p->end || *p->current < '0' || *p->current > '9') { json_error(p, "invalid exponent"); return null_value(); }
        while (p->current < p->end && *p->current >= '0' && *p->current <= '9') { p->current++; p->column++; }
    }
    char *number = rt_strndup(p->rt, number_start, (size_t)(p->current - number_start));
    if (floating) {
        errno = 0;
        double parsed = strtod(number, NULL);
        if (errno == ERANGE || !isfinite(parsed)) {
            json_error(p, "number out of range"); return null_value();
        }
        return float_value(parsed);
    }
    errno = 0; long long integer = strtoll(number, NULL, 10);
    if (errno == ERANGE) { json_error(p, "integer out of range"); return null_value(); }
    return int_value((int64_t)integer);
}

static Value parse_json_text(Runtime *rt, const HhyNode *site, const char *text, size_t length) {
    JsonParser parser = {.rt = rt, .site = site, .start = text, .current = text,
        .end = text + length, .line = 1, .column = 1};
    Value result = json_value(&parser, 0);
    json_space(&parser);
    if (!rt->failed && parser.current != parser.end) json_error(&parser, "trailing content");
    return result;
}

static char *read_open_file(Runtime *rt, const HhyNode *site, FILE *file, size_t maximum,
                            size_t *out_length) {
    if (fflush(file) != 0 || fseek(file, 0, SEEK_END) != 0) {
        runtime_process_error(rt, site, "cannot read command output"); return NULL;
    }
    long length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
        runtime_process_error(rt, site, "cannot read command output size"); return NULL;
    }
    if ((uintmax_t)length > maximum) {
        runtime_error_kind(rt, site, "ResourceLimitError", "HHY_PROCESS_OUTPUT_LIMIT",
                           "command output exceeds configured limit"); return NULL;
    }
    char *text = rt_alloc(rt, (size_t)length + 1);
    size_t read = fread(text, 1, (size_t)length, file);
    if (read != (size_t)length && ferror(file)) {
        runtime_process_error(rt, site, "cannot read command output"); return NULL;
    }
    text[read] = '\0';
    *out_length = read;
    return text;
}

static void terminate_process(pid_t pid, int *status) {
    kill(-pid, SIGTERM);
    kill(pid, SIGTERM);
    for (int attempt = 0; attempt < 50; attempt++) {
        pid_t waited = waitpid(pid, status, WNOHANG);
        if (waited == pid || (waited < 0 && errno == ECHILD)) return;
        struct timespec pause = {.tv_sec = 0, .tv_nsec = 10000000L};
        nanosleep(&pause, NULL);
    }
    kill(-pid, SIGKILL);
    kill(pid, SIGKILL);
    while (waitpid(pid, status, 0) < 0 && errno == EINTR) {}
}

static Value command_run(Runtime *rt, const HhyNode *site, Value arguments, Value options) {
    if (arguments.kind != V_LIST || arguments.as.list.count == 0) {
        runtime_type_error(rt, site, "run expects a non-empty List<String>"); return null_value();
    }
    size_t count = arguments.as.list.count;
    char **command = hhy_alloc((count + 1) * sizeof(char *));
    for (size_t i = 0; i < count; i++) {
        if (arguments.as.list.items[i].kind != V_STRING) {
            free(command); runtime_type_error(rt, site, "run arguments must all be String"); return null_value();
        }
        if (string_has_nul(arguments.as.list.items[i])) {
            free(command); runtime_value_error(rt, site, "run arguments cannot contain U+0000");
            return null_value();
        }
        command[i] = arguments.as.list.items[i].as.string;
    }
    command[count] = NULL;
    const char *cwd = NULL;
    const char *stdin_text = NULL;
    size_t stdin_length = 0;
    double timeout_ns = 0;
    size_t max_output = 16 * 1024 * 1024;
    Value environment = null_value();
    if (options.kind != V_NULL) {
        Value option = map_get(options, "cwd");
        if (option.kind != V_NULL) {
            if (option.kind != V_PATH && option.kind != V_STRING) {
                free(command); runtime_type_error(rt, site, "run cwd must be Path or String"); return null_value();
            }
            cwd = option.as.string;
            if (string_has_nul(option)) {
                free(command); runtime_value_error(rt, site, "run cwd cannot contain U+0000"); return null_value();
            }
        }
        option = map_get(options, "stdin");
        if (option.kind != V_NULL) {
            if (option.kind != V_STRING) {
                free(command); runtime_type_error(rt, site, "run stdin must be String"); return null_value();
            }
            stdin_text = option.as.string;
            stdin_length = option.string_length;
        }
        option = map_get(options, "timeout");
        if (option.kind != V_NULL) {
            if (option.kind != V_DURATION || option.as.number <= 0) {
                free(command); runtime_value_error(rt, site, "run timeout must be a positive Duration"); return null_value();
            }
            timeout_ns = option.as.number;
        }
        option = map_get(options, "max_output");
        if (option.kind != V_NULL) {
            if (option.kind != V_BYTES || option.as.number < 0 || option.as.number > 268435456.0) {
                free(command); runtime_value_error(rt, site, "run max_output must be Bytes up to 256 MiB"); return null_value();
            }
            max_output = (size_t)option.as.number;
        }
        environment = map_get(options, "env");
        if (environment.kind != V_NULL && environment.kind != V_MAP) {
            free(command); runtime_type_error(rt, site, "run env must be Map<String, String>"); return null_value();
        }
        if (environment.kind == V_MAP) {
            for (size_t i = 0; i < environment.as.map->count; i++) {
                Value value = environment.as.map->values[i];
                if (value.kind != V_STRING) {
                    free(command); runtime_type_error(rt, site, "run env values must be String"); return null_value();
                }
                if (memchr(environment.as.map->keys[i], '\0', environment.as.map->key_lengths[i]) != NULL ||
                    string_has_nul(value)) {
                    free(command); runtime_value_error(rt, site, "run env cannot contain U+0000"); return null_value();
                }
            }
        }
    }
    if (!rt->effect_allowed) {
        const char *keys[] = {"exit_code", "stdout", "stderr", "duration", "dry_run"};
        Value duration = {.kind = V_DURATION};
        Value values[] = {int_value(0), string_value(rt, ""), string_value(rt, ""),
                          duration, bool_value(true)};
        free(command);
        return map_with_entries(rt, V_COMMAND_RESULT, 5, keys, values);
    }
    if (rt->active_processes >= rt->limits.max_processes) {
        free(command);
        runtime_error_kind(rt, site, "ResourceLimitError", "HHY_PROCESS_LIMIT",
                           "run exceeds RuntimeLimits.max_processes");
        return null_value();
    }
    FILE *stdout_file = runtime_tmpfile(rt, site);
    FILE *stderr_file = rt->failed ? NULL : runtime_tmpfile(rt, site);
    FILE *stdin_file = stdin_text == NULL || rt->failed ? NULL : runtime_tmpfile(rt, site);
    if (stdout_file == NULL || stderr_file == NULL || (stdin_text != NULL && stdin_file == NULL)) {
        runtime_fclose(rt, stdout_file);
        runtime_fclose(rt, stderr_file);
        runtime_fclose(rt, stdin_file);
        free(command); if (!rt->failed) runtime_process_error(rt, site, "cannot create command output buffers"); return null_value();
    }
    if (stdin_file != NULL) {
        if (fwrite(stdin_text, 1, stdin_length, stdin_file) != stdin_length || fflush(stdin_file) != 0 ||
            fseek(stdin_file, 0, SEEK_SET) != 0) {
            runtime_fclose(rt, stdin_file); runtime_fclose(rt, stdout_file);
            runtime_fclose(rt, stderr_file); free(command);
            runtime_process_error(rt, site, "cannot prepare command stdin"); return null_value();
        }
    }
    struct timespec started, finished;
    clock_gettime(CLOCK_MONOTONIC, &started);
    pid_t pid = fork();
    if (pid < 0) {
        runtime_fclose(rt, stdin_file); runtime_fclose(rt, stdout_file);
        runtime_fclose(rt, stderr_file); free(command);
        runtime_process_error(rt, site, "cannot start process"); return null_value();
    }
    if (pid == 0) {
        setpgid(0, 0);
        if (cwd != NULL && chdir(cwd) != 0) _exit(126);
        if (stdin_file != NULL && dup2(fileno(stdin_file), STDIN_FILENO) < 0) _exit(126);
        if (dup2(fileno(stdout_file), STDOUT_FILENO) < 0 ||
            dup2(fileno(stderr_file), STDERR_FILENO) < 0)
            _exit(126);
        if (environment.kind == V_MAP) {
            for (size_t i = 0; i < environment.as.map->count; i++) {
                Value value = environment.as.map->values[i];
                if (value.kind != V_STRING || setenv(environment.as.map->keys[i], value.as.string, 1) != 0)
                    _exit(126);
            }
        }
        execvp(command[0], command);
        _exit(127);
    }
    rt->active_processes++;
    setpgid(pid, pid);
    int status = 0;
    for (;;) {
        pid_t waited = waitpid(pid, &status, WNOHANG);
        if (waited == pid) break;
        if (waited < 0 && errno != EINTR) {
            terminate_process(pid, &status);
            if (rt->active_processes > 0) rt->active_processes--;
            runtime_fclose(rt, stdin_file); runtime_fclose(rt, stdout_file);
            runtime_fclose(rt, stderr_file); free(command);
            runtime_process_error(rt, site, "cannot wait for process"); return null_value();
        }
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double elapsed = (double)(now.tv_sec - started.tv_sec) * 1000000000.0 +
                         (double)(now.tv_nsec - started.tv_nsec);
        if (timeout_ns > 0 && elapsed >= timeout_ns) {
            terminate_process(pid, &status);
            if (rt->active_processes > 0) rt->active_processes--;
            runtime_fclose(rt, stdin_file); runtime_fclose(rt, stdout_file);
            runtime_fclose(rt, stderr_file); free(command);
            rt->exit_code = 5;
            runtime_error_kind(rt, site, "TimeoutError", "HHY_PROCESS_TIMEOUT", "process timed out");
            return null_value();
        }
        if (runtime_check_cancel(rt, site)) {
            terminate_process(pid, &status);
            if (rt->active_processes > 0) rt->active_processes--;
            runtime_fclose(rt, stdin_file); runtime_fclose(rt, stdout_file);
            runtime_fclose(rt, stderr_file); free(command);
            return null_value();
        }
        struct timespec pause = {.tv_sec = 0, .tv_nsec = 10000000L};
        nanosleep(&pause, NULL);
    }
    if (rt->active_processes > 0) rt->active_processes--;
    clock_gettime(CLOCK_MONOTONIC, &finished);
    runtime_fclose(rt, stdin_file);
    free(command);
    size_t stdout_length = 0, stderr_length = 0;
    char *stdout_text = read_open_file(rt, site, stdout_file, max_output, &stdout_length);
    char *stderr_text = rt->failed ? NULL : read_open_file(rt, site, stderr_file, max_output, &stderr_length);
    runtime_fclose(rt, stdout_file); runtime_fclose(rt, stderr_file);
    if (rt->failed) return null_value();
    size_t ignored_codepoints;
    if (!utf8_count(stdout_text, stdout_length, &ignored_codepoints) ||
        !utf8_count(stderr_text, stderr_length, &ignored_codepoints)) {
        runtime_error_kind(rt, site, "EncodingError", "HHY_PROCESS_UTF8",
                           "command output is not valid UTF-8"); return null_value();
    }
    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
    double duration = (double)(finished.tv_sec - started.tv_sec) * 1000000000.0 +
                      (double)(finished.tv_nsec - started.tv_nsec);
    Value result = {.kind = V_COMMAND_RESULT}; result.as.map = map_storage_new(rt, 4);
    result.as.map->keys = rt_alloc(rt, 4 * sizeof(char *));
    result.as.map->key_lengths = rt_alloc(rt, 4 * sizeof(size_t));
    result.as.map->values = rt_alloc(rt, 4 * sizeof(Value));
    result.as.map->keys[0] = rt_strndup(rt, "exit_code", 9);
    result.as.map->values[0] = int_value(exit_code);
    result.as.map->keys[1] = rt_strndup(rt, "stdout", 6);
    result.as.map->values[1] = string_n(rt, stdout_text, stdout_length);
    result.as.map->keys[2] = rt_strndup(rt, "stderr", 6);
    result.as.map->values[2] = string_n(rt, stderr_text, stderr_length);
    result.as.map->keys[3] = rt_strndup(rt, "duration", 8);
    result.as.map->values[3].kind = V_DURATION;
    result.as.map->values[3].as.number = duration;
    result.as.map->key_lengths[0] = 9; result.as.map->key_lengths[1] = 6;
    result.as.map->key_lengths[2] = 6; result.as.map->key_lengths[3] = 8;
    return result;
}

static Value string_lines(Runtime *rt, const char *text, size_t text_length) {
    size_t count = 0;
    for (size_t i = 0; i < text_length; i++) if (text[i] == '\n') count++;
    if (text_length > 0 && text[text_length - 1] != '\n') count++;
    Value lines = list_new(rt, count);
    size_t index = 0;
    size_t start = 0;
    for (size_t i = 0; i <= text_length; i++) {
        if (i == text_length || text[i] == '\n') {
            size_t end = i;
            if (end > start && text[end - 1] == '\r') end--;
            if (i > start || i != text_length)
                lines.as.list.items[index++] = string_n(rt, text + start, end - start);
            if (i == text_length) break;
            start = i + 1;
        }
    }
    lines.as.list.count = index;
    return stream_value(rt, STREAM_LIST, lines, null_value(), NULL);
}

static Value csv_read_record(Runtime *rt, const HhyNode *site, Stream *source,
                             char delimiter, char quote) {
    size_t length = 0, capacity = 128;
    char *record = hhy_alloc(capacity);
    bool received = false, quoted = false;
    do {
        Value line;
        if (!stream_next(rt, site, source, &line)) {
            if (!received) { free(record); return null_value(); }
            if (quoted) { free(record); runtime_value_error(rt, site, "unterminated quoted CSV field"); return null_value(); }
            break;
        }
        if (line.kind != V_STRING) {
            free(record); runtime_type_error(rt, site, "parse_csv Stream items must be String"); return null_value();
        }
        size_t line_length = line.string_length;
        size_t needed = length + line_length + (received ? 1 : 0) + 1;
        while (capacity < needed) capacity *= 2;
        record = hhy_realloc(record, capacity);
        if (received) record[length++] = '\n';
        memcpy(record + length, line.as.string, line_length);
        for (size_t i = 0; i < line_length; i++) {
            if (line.as.string[i] != quote) continue;
            if (quoted && i + 1 < line_length && line.as.string[i + 1] == quote) { i++; continue; }
            quoted = !quoted;
        }
        length += line_length;
        received = true;
    } while (quoted);
    record[length] = '\0';
    Value parsed = csv_parse_row(rt, site, record, length, delimiter, quote);
    free(record);
    return parsed;
}

static Value csv_parse_row(Runtime *rt, const HhyNode *site,
                           const char *record, size_t record_length, char delimiter, char quote) {
    size_t count = 0, capacity = 8;
    Value *fields = rt_value_array_grow(rt, NULL, 0, capacity);
    const char *cursor = record, *limit = record + record_length;
    for (;;) {
        size_t maximum = (size_t)(limit - cursor) + 1;
        char *field = hhy_alloc(maximum), *out = field;
        bool quoted = *cursor == quote;
        if (quoted) cursor++;
        while (cursor < limit) {
            if (quoted) {
                if (*cursor == quote && cursor + 1 < limit && cursor[1] == quote) { *out++ = quote; cursor += 2; continue; }
                if (*cursor == quote) { quoted = false; cursor++; break; }
                *out++ = *cursor++;
            } else {
                if (*cursor == delimiter) break;
                *out++ = *cursor++;
            }
        }
        if (quoted) {
            free(field); runtime_value_error(rt, site, "unterminated quoted CSV field"); return null_value();
        }
        while (cursor < limit && *cursor != delimiter) {
            if (*cursor != ' ' && *cursor != '\t') {
                free(field); runtime_value_error(rt, site, "unexpected content after quoted CSV field"); return null_value();
            }
            cursor++;
        }
        *out = '\0';
        if (count == capacity) { capacity *= 2; fields = rt_value_array_grow(rt, fields, count, capacity); }
        fields[count++] = string_n(rt, field, (size_t)(out - field));
        free(field);
        if (cursor < limit && *cursor == delimiter) { cursor++; continue; }
        break;
    }
    Value result = list_new(rt, count);
    memcpy(result.as.list.items, fields, count * sizeof(Value));
    return result;
}

static Value csv_encode_row(Runtime *rt, const HhyNode *site, Value fields,
                            char delimiter, char quote_char) {
    if (fields.kind != V_LIST) { runtime_type_error(rt, site, "CSV row must be List"); return null_value(); }
    size_t capacity = 128, length = 0;
    char *output = hhy_alloc(capacity);
    for (size_t i = 0; i < fields.as.list.count; i++) {
        char number[128];
        const char *text = NULL;
        size_t text_length = 0;
        Value field = fields.as.list.items[i];
        if (field.kind == V_NULL) { text = ""; text_length = 0; }
        else if (field.kind == V_STRING || field.kind == V_PATH) {
            text = field.as.string; text_length = field.string_length;
        }
        else if (field.kind == V_BOOL) {
            text = field.as.boolean ? "true" : "false"; text_length = field.as.boolean ? 4 : 5;
        }
        else if (field.kind == V_INT) {
            snprintf(number, sizeof(number), "%" PRId64, field.as.integer);
            text = number; text_length = strlen(number);
        }
        else if (field.kind == V_FLOAT) {
            snprintf(number, sizeof(number), "%.15g", field.as.number);
            text = number; text_length = strlen(number);
        }
        else { free(output); runtime_type_error(rt, site, "CSV fields must be scalar"); return null_value(); }
        bool quote = memchr(text, delimiter, text_length) != NULL ||
                     memchr(text, quote_char, text_length) != NULL ||
                     memchr(text, '\n', text_length) != NULL || memchr(text, '\r', text_length) != NULL;
        size_t extra_quotes = 0;
        for (size_t p = 0; p < text_length; p++) if (text[p] == quote_char) extra_quotes++;
        size_t needed = length + text_length + extra_quotes + (quote ? 2 : 0) +
                        (i + 1 < fields.as.list.count ? 1 : 0) + 1;
        while (capacity < needed) capacity *= 2;
        output = hhy_realloc(output, capacity);
        if (quote) output[length++] = quote_char;
        for (size_t p = 0; p < text_length; p++) {
            if (text[p] == quote_char) output[length++] = quote_char;
            output[length++] = text[p];
        }
        if (quote) output[length++] = quote_char;
        if (i + 1 < fields.as.list.count) output[length++] = delimiter;
    }
    output[length] = '\0';
    Value result = string_n(rt, output, length);
    free(output); return result;
}

typedef struct { char *data; size_t length; size_t maximum; bool too_large; } CurlBuffer;
typedef struct { FILE *file; size_t length; size_t maximum; bool too_large; bool failed; } CurlFileSink;
typedef struct { Runtime *rt; const HhyNode *site; } CurlProgress;
typedef struct { bool blocked_private; } CurlNetworkGuard;

static bool private_socket_address(const struct sockaddr *address) {
    if (address->sa_family == AF_INET) {
        uint32_t ip = ntohl(((const struct sockaddr_in *)address)->sin_addr.s_addr);
        return (ip >> 24) == 0 || (ip >> 24) == 10 || (ip >> 24) == 127 ||
               (ip >> 22) == 0x0191 || (ip >> 16) == 0xa9fe ||
               (ip >> 20) == 0xac1 || (ip >> 16) == 0xc0a8 ||
               (ip & 0xfffe0000U) == 0xc6120000U || (ip >> 24) >= 224;
    }
    if (address->sa_family == AF_INET6) {
        const uint8_t *ip = ((const struct sockaddr_in6 *)address)->sin6_addr.s6_addr;
        bool zero_prefix = true;
        for (size_t i = 0; i < 15; i++) zero_prefix = zero_prefix && ip[i] == 0;
        bool mapped_v4 = true;
        for (size_t i = 0; i < 10; i++) mapped_v4 = mapped_v4 && ip[i] == 0;
        mapped_v4 = mapped_v4 && ip[10] == 0xff && ip[11] == 0xff;
        if (mapped_v4) {
            uint32_t v4 = ((uint32_t)ip[12] << 24) | ((uint32_t)ip[13] << 16) |
                          ((uint32_t)ip[14] << 8) | ip[15];
            struct sockaddr_in mapped = {.sin_family = AF_INET, .sin_addr.s_addr = htonl(v4)};
            return private_socket_address((const struct sockaddr *)&mapped);
        }
        return zero_prefix || (ip[0] & 0xfe) == 0xfc ||
               (ip[0] == 0xfe && (ip[1] & 0xc0) == 0x80) || ip[0] == 0xff;
    }
    return false;
}

static curl_socket_t guarded_open_socket(void *context, curlsocktype purpose,
                                         struct curl_sockaddr *address) {
    (void)purpose;
    CurlNetworkGuard *guard = context;
    if (private_socket_address(&address->addr)) {
        guard->blocked_private = true;
        return CURL_SOCKET_BAD;
    }
    return socket(address->family, address->socktype, address->protocol);
}

static size_t hhy_curl_write(char *data, size_t size, size_t count, void *context) {
    CurlBuffer *buffer = context;
    if (size != 0 && count > SIZE_MAX / size) return 0;
    size_t incoming = size * count;
    if (incoming > buffer->maximum || buffer->length > buffer->maximum - incoming) {
        buffer->too_large = true;
        return 0;
    }
    char *next = realloc(buffer->data, buffer->length + incoming + 1);
    if (next == NULL) return 0;
    buffer->data = next;
    memcpy(buffer->data + buffer->length, data, incoming);
    buffer->length += incoming;
    buffer->data[buffer->length] = '\0';
    return incoming;
}

static size_t hhy_curl_write_file(char *data, size_t size, size_t count, void *context) {
    CurlFileSink *sink = context;
    if (size != 0 && count > SIZE_MAX / size) return 0;
    size_t incoming = size * count;
    if (incoming > sink->maximum || sink->length > sink->maximum - incoming) {
        sink->too_large = true;
        return 0;
    }
    if (incoming != 0 && fwrite(data, 1, incoming, sink->file) != incoming) {
        sink->failed = true;
        return 0;
    }
    sink->length += incoming;
    return incoming;
}

static int hhy_curl_progress(void *context, curl_off_t download_total,
                             curl_off_t downloaded, curl_off_t upload_total,
                             curl_off_t uploaded) {
    (void)download_total; (void)downloaded; (void)upload_total; (void)uploaded;
    CurlProgress *progress = context;
    return runtime_check_cancel(progress->rt, progress->site) ? 1 : 0;
}

static Value map_with_entries(Runtime *rt, ValueKind kind, size_t count,
                              const char **keys, Value *values) {
    Value result = {.kind = kind}; result.as.map = map_storage_new(rt, count);
    /* Root pointer-bearing input Values before allocating key metadata. Callers
       commonly pass a short stack array containing freshly allocated strings. */
    result.as.map->values = count ? rt_alloc(rt, count * sizeof(Value)) : NULL;
    if (count) memcpy(result.as.map->values, values, count * sizeof(Value));
    result.as.map->keys = count ? rt_alloc(rt, count * sizeof(char *)) : NULL;
    result.as.map->key_lengths = count ? rt_alloc(rt, count * sizeof(size_t)) : NULL;
    for (size_t i = 0; i < count; i++) {
        result.as.map->keys[i] = rt_strndup(rt, keys[i], strlen(keys[i]));
        result.as.map->key_lengths[i] = strlen(keys[i]);
    }
    map_build_index(rt, result.as.map);
    return result;
}

static Value file_value(Runtime *rt, const HhyNode *site, const char *path,
                        const struct stat *info) {
    size_t codepoints = 0, path_length = strlen(path);
    if (!utf8_count(path, path_length, &codepoints)) {
        runtime_error_kind(rt, site, "EncodingError", "HHY_PATH_UTF8",
                           "filesystem path is not valid UTF-8");
        return null_value();
    }
    const char *name = strrchr(path, '/');
    name = name == NULL ? path : name + 1;
    const char *extension = strrchr(name, '.');
    if (extension == NULL || extension == name) extension = "";
    Value path_value = string_value(rt, path); path_value.kind = V_PATH;
    Value size = {.kind = V_BYTES}; size.as.number = (double)info->st_size;
    Value created = null_value(), modified = {.kind = V_DATETIME};
#ifdef __APPLE__
    created.kind = V_DATETIME;
    created.as.datetime.nanoseconds = (int64_t)info->st_birthtimespec.tv_sec * 1000000000LL +
                                      info->st_birthtimespec.tv_nsec;
    modified.as.datetime.nanoseconds = (int64_t)info->st_mtimespec.tv_sec * 1000000000LL +
                                       info->st_mtimespec.tv_nsec;
#else
    modified.as.datetime.nanoseconds = (int64_t)info->st_mtim.tv_sec * 1000000000LL + info->st_mtim.tv_nsec;
#endif
    const char *keys[] = {"path", "name", "extension", "size", "created", "modified",
                          "is_file", "is_dir", "is_symlink"};
    Value values[] = {
        path_value, string_value(rt, name), string_value(rt, extension), size, created, modified,
        bool_value(S_ISREG(info->st_mode)), bool_value(S_ISDIR(info->st_mode)),
        bool_value(S_ISLNK(info->st_mode))
    };
    return map_with_entries(rt, S_ISDIR(info->st_mode) ? V_DIRECTORY : V_FILE, 9, keys, values);
}

static Value map_put_runtime_n(Runtime *rt, Value source, const char *key, size_t key_length,
                               Value value) {
    size_t existing = source.as.map->count;
    bool replace = false;
    size_t replace_index = 0;
    for (size_t i = 0; i < existing; i++) {
        if (source.as.map->key_lengths[i] == key_length &&
            memcmp(source.as.map->keys[i], key, key_length) == 0) {
            replace = true; replace_index = i; break;
        }
    }
    Value result = {.kind = source.kind};
    result.as.map = map_storage_new(rt, existing + (replace ? 0 : 1));
    result.as.map->count = existing + (replace ? 0 : 1);
    result.as.map->keys = rt_alloc(rt, result.as.map->count * sizeof(char *));
    result.as.map->key_lengths = rt_alloc(rt, result.as.map->count * sizeof(size_t));
    result.as.map->values = rt_alloc(rt, result.as.map->count * sizeof(Value));
    for (size_t i = 0; i < existing; i++) {
        result.as.map->keys[i] = source.as.map->keys[i];
        result.as.map->key_lengths[i] = source.as.map->key_lengths[i];
        result.as.map->values[i] = i == replace_index && replace ? value : source.as.map->values[i];
    }
    if (!replace) {
        result.as.map->keys[existing] = rt_strndup(rt, key, key_length);
        result.as.map->key_lengths[existing] = key_length;
        result.as.map->values[existing] = value;
    }
    return result;
}

static Value map_put_runtime(Runtime *rt, Value source, const char *key, Value value) {
    return map_put_runtime_n(rt, source, key, strlen(key), value);
}

static bool query_scalar(Value value) {
    return value.kind == V_STRING || value.kind == V_INT || value.kind == V_FLOAT ||
           value.kind == V_BOOL || value.kind == V_NULL;
}

static char *query_text(Runtime *rt, Value value) {
    char buffer[96];
    if (value.kind == V_STRING) return value.as.string;
    if (value.kind == V_NULL) return "";
    if (value.kind == V_BOOL) return value.as.boolean ? "true" : "false";
    if (value.kind == V_INT) snprintf(buffer, sizeof(buffer), "%" PRId64, value.as.integer);
    else snprintf(buffer, sizeof(buffer), "%.15g", value.as.number);
    return rt_strndup(rt, buffer, strlen(buffer));
}

static size_t url_encoded_length(const char *text, size_t text_length) {
    size_t length = 0;
    for (size_t i = 0; i < text_length; i++) {
        unsigned char c = (unsigned char)text[i];
        length += (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') ? 1 : 3;
    }
    return length;
}

static char *url_encode_into(char *out, const char *text, size_t text_length) {
    static const char hex[] = "0123456789ABCDEF";
    for (size_t i = 0; i < text_length; i++) {
        unsigned char c = (unsigned char)text[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') *out++ = (char)c;
        else { *out++ = '%'; *out++ = hex[c >> 4]; *out++ = hex[c & 15]; }
    }
    return out;
}

static Value url_add_query(Runtime *rt, const HhyNode *site, Value url, Value query) {
    if (query.kind == V_NULL) return url;
    if (query.kind != V_MAP) {
        runtime_type_error(rt, site, "HTTP query must be Map"); return null_value();
    }
    size_t length = url.string_length + 1;
    bool has_query = memchr(url.as.string, '?', url.string_length) != NULL;
    for (size_t i = 0; i < query.as.map->count; i++) {
        if (!query_scalar(query.as.map->values[i])) {
            runtime_type_error(rt, site, "HTTP query values must be scalar"); return null_value();
        }
        char *value = query_text(rt, query.as.map->values[i]);
        size_t value_length = query.as.map->values[i].kind == V_STRING
            ? query.as.map->values[i].string_length : strlen(value);
        length += url_encoded_length(query.as.map->keys[i], query.as.map->key_lengths[i]) + 1 +
                  url_encoded_length(value, value_length) + (i ? 1 : 0);
    }
    char *text = rt_alloc(rt, length + 1);
    char *out = text;
    size_t base = url.string_length; memcpy(out, url.as.string, base); out += base;
    *out++ = has_query ? '&' : '?';
    for (size_t i = 0; i < query.as.map->count; i++) {
        if (i) *out++ = '&';
        out = url_encode_into(out, query.as.map->keys[i], query.as.map->key_lengths[i]); *out++ = '=';
        char *value = query_text(rt, query.as.map->values[i]);
        size_t value_length = query.as.map->values[i].kind == V_STRING
            ? query.as.map->values[i].string_length : strlen(value);
        out = url_encode_into(out, value, value_length);
    }
    *out = '\0';
    Value result = {.kind = V_STRING, .string_length = length}; result.as.string = text; return result;
}

static Value http_request(Runtime *rt, const HhyNode *site, const char *method,
                          size_t argc, Value *argv) {
    if (argc < 1 || argc > 2 || argv[0].kind != V_STRING ||
        (argc == 2 && argv[1].kind != V_STRING && argv[1].kind != V_MAP)) {
        runtime_type_error(rt, site, "HTTP method expects URL and optional String or Map body");
        return null_value();
    }
    if (strncmp(argv[0].as.string, "http://", 7) != 0 &&
        strncmp(argv[0].as.string, "https://", 8) != 0) {
        runtime_value_error(rt, site, "HTTP URL must use http or https");
        return null_value();
    }
    if (string_has_nul(argv[0])) {
        runtime_value_error(rt, site, "HTTP URL cannot contain U+0000"); return null_value();
    }
    Value options = argc == 2 && argv[1].kind == V_MAP ? argv[1] : null_value();
    Value body = argc == 2 && argv[1].kind == V_STRING ? argv[1] : map_get(options, "body");
    Value headers = map_get(options, "headers");
    Value query = map_get(options, "query");
    Value redirects = map_get(options, "follow_redirects");
    Value proxy = map_get(options, "proxy");
    Value maximum = map_get(options, "max_body");
    Value allow_private = map_get(options, "allow_private_networks");
    if (body.kind != V_NULL && body.kind != V_STRING) {
        runtime_type_error(rt, site, "HTTP body must be String"); return null_value();
    }
    if (headers.kind != V_NULL && headers.kind != V_MAP) {
        runtime_type_error(rt, site, "HTTP headers must be Map<String, String>"); return null_value();
    }
    if (redirects.kind != V_NULL && redirects.kind != V_BOOL) {
        runtime_type_error(rt, site, "follow_redirects must be Bool"); return null_value();
    }
    if (proxy.kind != V_NULL && proxy.kind != V_STRING) {
        runtime_type_error(rt, site, "HTTP proxy must be String"); return null_value();
    }
    if (proxy.kind == V_STRING && string_has_nul(proxy)) {
        runtime_value_error(rt, site, "HTTP proxy cannot contain U+0000"); return null_value();
    }
    if (maximum.kind != V_NULL && (maximum.kind != V_BYTES || maximum.as.number < 0 ||
                                   maximum.as.number > (double)rt->limits.max_http_body)) {
        runtime_value_error(rt, site, "HTTP max_body must be Bytes up to 16 MiB"); return null_value();
    }
    if (allow_private.kind != V_NULL && allow_private.kind != V_BOOL) {
        runtime_type_error(rt, site, "HTTP allow_private_networks must be Bool"); return null_value();
    }
    Value final_url = url_add_query(rt, site, argv[0], query);
    if (rt->failed) return null_value();
    const char *keys[] = {"method", "url", "body", "headers", "timeout_ns", "retry_count",
                          "retry_backoff_ns", "follow_redirects", "proxy", "max_body",
                          "allow_private_networks"};
    Value timeout_value = {.kind = V_DURATION};
    timeout_value.as.number = 30000000000.0;
    Value backoff_value = {.kind = V_DURATION};
    Value values[] = {
        string_value(rt, method), final_url, body, headers, timeout_value,
        int_value(0),
        backoff_value,
        redirects.kind == V_NULL ? bool_value(true) : redirects,
        proxy,
        maximum.kind == V_NULL
            ? (Value){.kind = V_BYTES, .as.number = (double)rt->limits.max_http_body}
            : maximum,
        allow_private.kind == V_NULL ? bool_value(true) : allow_private
    };
    return map_with_entries(rt, V_HTTP_REQUEST, 11, keys, values);
}

static Value redact_http_url(Runtime *rt, Value url) {
    const char *text = url.as.string;
    size_t length = url.string_length;
    const char *scheme = bytes_find(text, length, "://", 3);
    size_t authority_start = scheme == NULL ? 0 : (size_t)(scheme - text) + 3;
    size_t query = length;
    for (size_t i = authority_start; i < length; i++)
        if (text[i] == '?' || text[i] == '#') { query = i; break; }
    size_t authority_end = query;
    for (size_t i = authority_start; i < query; i++)
        if (text[i] == '/') { authority_end = i; break; }
    size_t host_start = authority_start;
    for (size_t i = authority_start; i < authority_end; i++)
        if (text[i] == '@') host_start = i + 1;
    const char redacted_query[] = "?<redacted>";
    bool had_query = query < length && text[query] == '?';
    size_t prefix = authority_start, host_and_path = query - host_start;
    size_t output_length = prefix + host_and_path + (had_query ? sizeof(redacted_query) - 1 : 0);
    char *output = rt_alloc_atomic(rt, output_length + 1);
    memcpy(output, text, prefix);
    memcpy(output + prefix, text + host_start, host_and_path);
    if (had_query) memcpy(output + prefix + host_and_path, redacted_query,
                          sizeof(redacted_query) - 1);
    output[output_length] = '\0';
    Value result = {.kind = V_STRING, .string_length = output_length};
    result.as.string = output;
    return result;
}

static Value http_send(Runtime *rt, const HhyNode *site, Value request, Value output) {
    if (request.kind != V_HTTP_REQUEST) {
        runtime_type_error(rt, site, "send expects HttpRequest"); return null_value();
    }
    Value method = map_get(request, "method"), url = map_get(request, "url");
    Value body = map_get(request, "body"), timeout = map_get(request, "timeout_ns");
    Value retry = map_get(request, "retry_count");
    Value backoff = map_get(request, "retry_backoff_ns");
    Value headers = map_get(request, "headers"), redirects = map_get(request, "follow_redirects");
    Value proxy = map_get(request, "proxy"), maximum = map_get(request, "max_body");
    Value allow_private = map_get(request, "allow_private_networks");
    bool streaming = output.kind == V_PATH;
    if (output.kind != V_NULL && !streaming) {
        runtime_type_error(rt, site, "send_to output must be Path"); return null_value();
    }
    if (!rt->effect_allowed) {
        const char *keys[] = {"status", "bytes", "ok", "dry_run", "method", "url",
                              "timeout", "retry_count"};
        Value values[] = {int_value(0), bytes_buffer_value(rt, NULL, 0),
                          bool_value(true), bool_value(true), method, redact_http_url(rt, url),
                          timeout, retry};
        return map_with_entries(rt, V_HTTP_RESPONSE, 8, keys, values);
    }
    /* Validate before opening a streaming temporary file. An error may be
       caught by `attempt`, so cleanup cannot be deferred until Runtime exit. */
    if (headers.kind == V_MAP) {
        for (size_t i = 0; i < headers.as.map->count; i++) {
            Value value = headers.as.map->values[i];
            if (value.kind != V_STRING) {
                runtime_type_error(rt, site, "HTTP header values must be String");
                return null_value();
            }
            if (memchr(headers.as.map->keys[i], '\0', headers.as.map->key_lengths[i]) != NULL ||
                memchr(headers.as.map->keys[i], '\r', headers.as.map->key_lengths[i]) != NULL ||
                memchr(headers.as.map->keys[i], '\n', headers.as.map->key_lengths[i]) != NULL ||
                memchr(value.as.string, '\r', value.string_length) != NULL ||
                memchr(value.as.string, '\n', value.string_length) != NULL || string_has_nul(value)) {
                runtime_value_error(rt, site, "HTTP headers cannot contain U+0000, CR, or LF");
                return null_value();
            }
        }
    }
    int attempts = retry.kind == V_INT && retry.as.integer > 0 ? (int)retry.as.integer + 1 : 1;
    CURLcode code = CURLE_OK;
    long status = 0;
    CurlBuffer buffer = {.maximum = maximum.kind == V_BYTES
        ? (size_t)maximum.as.number : rt->limits.max_http_body};
    CurlFileSink file_sink = {.maximum = buffer.maximum};
    char *temporary = NULL;
    CurlProgress progress = {.rt = rt, .site = site};
    for (int attempt = 0; attempt < attempts; attempt++) {
        free(buffer.data); buffer.data = NULL; buffer.length = 0; buffer.too_large = false;
        file_sink = (CurlFileSink){.maximum = buffer.maximum};
        RuntimeCleanup *file_cleanup = NULL;
        int file_descriptor = -1;
        if (streaming) {
            if (!ensure_parent_directories(rt, site, output.as.string)) return null_value();
            size_t path_length = output.string_length;
            temporary = hhy_alloc(path_length + 18);
            snprintf(temporary, path_length + 18, "%s.hhy-tmp-XXXXXX", output.as.string);
            file_descriptor = mkstemp(temporary);
            if (file_descriptor < 0) {
                free(temporary); temporary = NULL;
                runtime_io_error(rt, site, "cannot create HTTP output file"); return null_value();
            }
            file_sink.file = runtime_fdopen(rt, site, file_descriptor, "wb");
            if (file_sink.file == NULL) {
                close(file_descriptor); unlink(temporary); free(temporary); temporary = NULL;
                if (!rt->failed) runtime_io_error(rt, site, "cannot open HTTP output file");
                return null_value();
            }
            file_cleanup = runtime_register_temporary(rt, file_sink.file, temporary);
        }
        CURL *curl = curl_easy_init();
        if (curl == NULL) {
            if (streaming) {
                runtime_unregister_temporary(rt, file_cleanup);
                runtime_fclose(rt, file_sink.file);
                unlink(temporary); free(temporary); temporary = NULL;
            }
            runtime_error_kind(rt, site, "HttpError", "HHY_HTTP_INIT", "cannot initialize HTTP client");
            return null_value();
        }
        curl_easy_setopt(curl, CURLOPT_URL, url.as.string);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION,
                         redirects.kind == V_BOOL && redirects.as.boolean ? 1L : 0L);
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, streaming ? hhy_curl_write_file : hhy_curl_write);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, streaming ? (void *)&file_sink : (void *)&buffer);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "HHY/" HHY_VERSION);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, hhy_curl_progress);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress);
        CurlNetworkGuard network_guard = {0};
        if (allow_private.kind != V_BOOL || !allow_private.as.boolean) {
            curl_easy_setopt(curl, CURLOPT_OPENSOCKETFUNCTION, guarded_open_socket);
            curl_easy_setopt(curl, CURLOPT_OPENSOCKETDATA, &network_guard);
        }
        long timeout_ms = timeout.kind == V_DURATION ? (long)(timeout.as.number / 1000000.0) : 30000L;
        if (timeout_ms < 1) timeout_ms = 1;
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);
        if (proxy.kind == V_STRING) curl_easy_setopt(curl, CURLOPT_PROXY, proxy.as.string);
        struct curl_slist *header_list = NULL;
        if (headers.kind == V_MAP) {
            for (size_t i = 0; i < headers.as.map->count; i++) {
                Value value = headers.as.map->values[i];
                size_t needed = headers.as.map->key_lengths[i] + value.string_length + 3;
                char *line = hhy_alloc(needed);
                snprintf(line, needed, "%s: %s", headers.as.map->keys[i], value.as.string);
                header_list = curl_slist_append(header_list, line); free(line);
            }
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
        }
        if (string_equals_c(method, "POST")) curl_easy_setopt(curl, CURLOPT_POST, 1L);
        else if (!string_equals_c(method, "GET")) curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.as.string);
        if (body.kind == V_STRING) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.as.string);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.string_length);
        }
        code = curl_easy_perform(curl);
        if (code == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        curl_slist_free_all(header_list);
        curl_easy_cleanup(curl);
        if (streaming) {
            bool file_ok = !file_sink.failed && fflush(file_sink.file) == 0 &&
                           fsync(file_descriptor) == 0;
            runtime_unregister_temporary(rt, file_cleanup);
            if (runtime_fclose(rt, file_sink.file) != 0) file_ok = false;
            file_sink.file = NULL;
            if (!file_ok && code == CURLE_OK) code = CURLE_WRITE_ERROR;
        }
        if (network_guard.blocked_private) {
            free(buffer.data);
            if (temporary != NULL) { unlink(temporary); free(temporary); temporary = NULL; }
            runtime_error_kind(rt, site, "HttpError", "HHY_HTTP_PRIVATE_NETWORK",
                               "HTTP connection to a private, loopback, or link-local address is blocked");
            return null_value();
        }
        if (code == CURLE_OK && status < 500 && status != 429) break;
        if (temporary != NULL) { unlink(temporary); free(temporary); temporary = NULL; }
        if (attempt + 1 < attempts && backoff.kind == V_DURATION && backoff.as.number > 0) {
            (void)runtime_wait_ns(rt, site, (uint64_t)backoff.as.number);
            if (rt->failed) break;
        }
    }
    if (code != CURLE_OK) {
        free(buffer.data);
        if (temporary != NULL) { unlink(temporary); free(temporary); }
        if (buffer.too_large || file_sink.too_large)
            runtime_error_kind(rt, site, "ResourceLimitError", "HHY_HTTP_BODY_LIMIT",
                               "HTTP response body exceeds 16 MiB limit");
        else
            runtime_error_kind(rt, site, "HttpError", "HHY_HTTP_TRANSPORT", curl_easy_strerror(code));
        return null_value();
    }
    const char *keys[] = {"status", "bytes", "ok"};
    Value values[] = {
        int_value(status),
        bytes_buffer_value(rt, buffer.data, buffer.length),
        bool_value(status >= 200 && status < 300)
    };
    if (streaming) {
        if (atomic_rename(temporary, output.as.string, true) != 0) {
            unlink(temporary); free(temporary); free(buffer.data);
            runtime_io_error(rt, site, "cannot atomically publish HTTP output file");
            return null_value();
        }
        free(temporary);
        const char *stream_keys[] = {"status", "bytes", "ok", "path", "size"};
        Value stream_values[] = {int_value(status), bytes_buffer_value(rt, NULL, 0),
            bool_value(status >= 200 && status < 300), output, int_value((int64_t)file_sink.length)};
        free(buffer.data);
        return map_with_entries(rt, V_HTTP_RESPONSE, 5, stream_keys, stream_values);
    }
    free(buffer.data);
    return map_with_entries(rt, V_HTTP_RESPONSE, 3, keys, values);
}

static Value process_snapshot(Runtime *rt, const HhyNode *site) {
    Value command = list_new(rt, 3);
    command.as.list.items[0] = string_value(rt, "/bin/ps");
    command.as.list.items[1] = string_value(rt, "-axo");
    command.as.list.items[2] = string_value(rt, "pid=,pcpu=,rss=,state=,comm=");
    Value result = command_run(rt, site, command, null_value());
    if (rt->failed) return null_value();
    Value exit_code = map_get(result, "exit_code");
    if (exit_code.kind != V_INT || exit_code.as.integer != 0) {
        Value diagnostic = map_get(result, "stderr");
        runtime_error_kind(rt, site, "ProcessError", "HHY_PROCESS_SNAPSHOT",
                           diagnostic.kind == V_STRING && diagnostic.string_length != 0
                               ? diagnostic.as.string
                               : "process snapshot command failed");
        return null_value();
    }
    Value output = map_get(result, "stdout");
    Value lines = string_lines(rt, output.as.string, output.string_length);
    Value collected = stream_collect(rt, site, lines);
    if (rt->failed) return null_value();
    Value processes = list_new(rt, collected.as.list.count);
    size_t count = 0;
    for (size_t i = 0; i < collected.as.list.count; i++) {
        const char *line = collected.as.list.items[i].as.string;
        long pid = 0, rss = 0;
        double cpu = 0;
        char status[32] = {0};
        int offset = 0;
        if (sscanf(line, " %ld %lf %ld %31s %n", &pid, &cpu, &rss, status, &offset) < 4)
            continue;
        const char *command_name = line + offset;
        while (*command_name == ' ') command_name++;
        const char *short_name = strrchr(command_name, '/');
        short_name = short_name == NULL ? command_name : short_name + 1;
        Value memory = {.kind = V_BYTES}; memory.as.number = (double)rss * 1024.0;
        Value percent = {.kind = V_PERCENT}; percent.as.number = cpu;
        const char *keys[] = {"pid", "name", "cpu", "memory", "status", "command"};
        Value values[] = {
            int_value(pid), string_value(rt, short_name), percent, memory,
            string_value(rt, status), string_value(rt, command_name)
        };
        processes.as.list.items[count++] = map_with_entries(rt, V_PROCESS, 6, keys, values);
    }
    processes.as.list.count = count;
    return processes;
}

static int64_t civil_days(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    int era = (year >= 0 ? year : year - 399) / 400;
    unsigned yoe = (unsigned)(year - era * 400);
    unsigned shifted_month = month > 2 ? month - 3 : month + 9;
    unsigned doy = (153 * shifted_month + 2) / 5 + day - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

static bool timezone_offset(const char *text, int32_t *minutes) {
    if (strcmp(text, "UTC") == 0 || strcmp(text, "Z") == 0) { *minutes = 0; return true; }
    if ((text[0] != '+' && text[0] != '-') || strlen(text) != 6 || text[3] != ':' ||
        !isdigit((unsigned char)text[1]) || !isdigit((unsigned char)text[2]) ||
        !isdigit((unsigned char)text[4]) || !isdigit((unsigned char)text[5])) return false;
    int hours = (text[1] - '0') * 10 + text[2] - '0';
    int mins = (text[4] - '0') * 10 + text[5] - '0';
    if (hours > 23 || mins > 59) return false;
    *minutes = (hours * 60 + mins) * (text[0] == '-' ? -1 : 1);
    return true;
}

static Value datetime_parse(Runtime *rt, const HhyNode *site, size_t argc, Value *argv) {
    if (argc != 3 || argv[0].kind != V_STRING || argv[1].kind != V_STRING ||
        argv[2].kind != V_STRING) {
        runtime_type_error(rt, site, "datetime.parse expects text, format and timezone Strings");
        return null_value();
    }
    struct tm parts = {0};
    char *end = strptime(argv[0].as.string, argv[1].as.string, &parts);
    if (end == NULL || *end != '\0') {
        runtime_error_kind(rt, site, "ValueError", "HHY_DATETIME_PARSE",
                           "DateTime text does not match format"); return null_value();
    }
    int32_t offset = 0;
    if (!timezone_offset(argv[2].as.string, &offset)) {
        runtime_error_kind(rt, site, "ValueError", "HHY_TIMEZONE",
                           "timezone must be UTC, Z or a fixed offset such as +08:00");
        return null_value();
    }
    int year = parts.tm_year + 1900;
    unsigned month = (unsigned)parts.tm_mon + 1, day = (unsigned)parts.tm_mday;
    int64_t seconds = civil_days(year, month, day) * 86400 + parts.tm_hour * 3600 +
                      parts.tm_min * 60 + parts.tm_sec - (int64_t)offset * 60;
    if (seconds > INT64_MAX / 1000000000LL || seconds < INT64_MIN / 1000000000LL) {
        runtime_value_error(rt, site, "DateTime is outside supported range"); return null_value();
    }
    Value result = {.kind = V_DATETIME};
    result.as.datetime.nanoseconds = seconds * 1000000000LL;
    result.as.datetime.offset_minutes = offset;
    return result;
}

static Value call_function(Runtime *rt, const HhyNode *site, Value callee,
                           size_t argc, Value *argv) {
    if (callee.as.function.is_bytecode) {
        const BytecodeFunctionTarget *target = (const BytecodeFunctionTarget *)callee.as.function.node;
        BytecodeCursor function = {.chunk = target->chunk, .instruction = target->instruction};
        HhyNode function_site = bytecode_site(function);
        size_t param_count = function_site.child_count - 2;
        if (argc != param_count) { runtime_type_error(rt, site, "wrong number of function arguments"); return null_value(); }
        Env *call_env = call_frame_acquire(rt, callee.as.function.closure,
                                           function_site.frame_slot_count > param_count
                                               ? function_site.frame_slot_count : param_count);
        for (size_t i = 0; i < param_count; i++) {
            HhyNode parameter = bytecode_site(bytecode_child_cursor(function, (uint32_t)i + 1));
            env_define_token(rt, call_env, site, parameter.token, argv[i], false);
        }
        BytecodeCursor body = bytecode_child_cursor(function, function_site.child_count - 1);
        Value result = body.chunk->code[body.instruction].opcode == HHY_OP_BLOCK
            ? bytecode_exec_contents(rt, call_env, body)
            : bytecode_exec(rt, call_env, body);
        if (rt->signal == SIGNAL_RETURN) { result = rt->signal_value; rt->signal = SIGNAL_NONE; }
        call_frame_release(rt, call_env);
        return result;
    }
    const HhyNode *function = callee.as.function.node;
    size_t param_count = function->child_count - 2;
    if (argc != param_count) { runtime_type_error(rt, site, "wrong number of function arguments"); return null_value(); }
    Env *call_env = call_frame_acquire(rt, callee.as.function.closure,
                                      function->frame_slot_count > param_count
                                          ? function->frame_slot_count : param_count);
    for (size_t i = 0; i < param_count; i++) {
        env_define_token(rt, call_env, site, function->children[i + 1]->token, argv[i], false);
    }
    const HhyNode *body = function->children[function->child_count - 1];
    Value result = body->kind == HHY_N_BLOCK
        ? exec_block_contents(rt, call_env, body) : exec_node(rt, call_env, body);
    if (rt->signal == SIGNAL_RETURN) { result = rt->signal_value; rt->signal = SIGNAL_NONE; }
    call_frame_release(rt, call_env);
    return result;
}

static Value call_closure(Runtime *rt, const HhyNode *site, Value callee,
                          size_t argc, Value *argv) {
    if (callee.as.function.is_bytecode) {
        const BytecodeFunctionTarget *target = (const BytecodeFunctionTarget *)callee.as.function.node;
        if (target->has_fast_argument_expression) {
            if (argc != 1) { runtime_type_error(rt, site, "closure requires one argument"); return null_value(); }
            Value result;
            bool evaluated = bytecode_eval_argument_expression(
                rt, (BytecodeCursor){.chunk = target->chunk, .instruction = target->fast_expression},
                target->parameter_constant, argv[0], &result);
            if (evaluated) return result;
        }
        BytecodeCursor closure = {.chunk = target->chunk, .instruction = target->instruction};
        HhyNode closure_site = bytecode_site(closure);
        bool explicit_param = closure_site.child_count > 0 &&
            closure.chunk->code[bytecode_child_cursor(closure, 0).instruction].opcode == HHY_OP_IDENTIFIER;
        if (argc != 1) { runtime_type_error(rt, site, "closure requires one argument"); return null_value(); }
        if (explicit_param && closure_site.child_count == 2) {
            BytecodeCursor parameter_cursor = bytecode_child_cursor(closure, 0);
            BytecodeCursor body = bytecode_child_cursor(closure, 1);
            if (body.chunk->code[body.instruction].opcode == HHY_OP_EXPR_STMT) {
                Value result;
                if (bytecode_eval_argument_expression(rt, bytecode_child_cursor(body, 0),
                                                      parameter_cursor.chunk->code[parameter_cursor.instruction].constant,
                                                      argv[0], &result))
                    return result;
            }
        }
        Env *call_env = call_frame_acquire(rt, callee.as.function.closure,
                                           closure_site.frame_slot_count > 1
                                               ? closure_site.frame_slot_count : 1);
        size_t body_start = 0;
        if (explicit_param) {
            HhyNode parameter = bytecode_site(bytecode_child_cursor(closure, 0));
            env_define_token(rt, call_env, site, parameter.token, argv[0], false);
            body_start = 1;
        } else env_define(rt, call_env, site, "it", argv[0], false);
        Value result = null_value();
        for (size_t i = body_start; i < closure_site.child_count && !rt->failed && rt->signal == SIGNAL_NONE; i++)
            result = bytecode_exec(rt, call_env, bytecode_child_cursor(closure, (uint32_t)i));
        if (rt->signal == SIGNAL_RETURN) { result = rt->signal_value; rt->signal = SIGNAL_NONE; }
        call_frame_release(rt, call_env);
        return result;
    }
    const HhyNode *closure = callee.as.function.node;
    bool explicit_param = closure->child_count > 0 && closure->children[0]->kind == HHY_N_IDENTIFIER;
    if (argc != 1) { runtime_type_error(rt, site, "closure requires one argument"); return null_value(); }
    Env *call_env = call_frame_acquire(rt, callee.as.function.closure,
                                      closure->frame_slot_count > 1
                                          ? closure->frame_slot_count : 1);
    const char *name = "it";
    size_t body_start = 0;
    if (explicit_param) {
        env_define_token(rt, call_env, site, closure->children[0]->token, argv[0], false);
        body_start = 1;
    } else env_define(rt, call_env, site, name, argv[0], false);
    Value result = null_value();
    for (size_t i = body_start; i < closure->child_count && !rt->failed && rt->signal == SIGNAL_NONE; i++)
        result = exec_node(rt, call_env, closure->children[i]);
    if (rt->signal == SIGNAL_RETURN) { result = rt->signal_value; rt->signal = SIGNAL_NONE; }
    call_frame_release(rt, call_env);
    return result;
}

static Value list_new(Runtime *rt, size_t count) {
    Value value = {.kind = V_LIST};
    value.as.list.count = count;
    value.as.list.items = count == 0 ? NULL : rt_alloc(rt, count * sizeof(Value));
    return value;
}

static Value url_resolve_value(Runtime *rt, const HhyNode *site, size_t argc, Value *argv) {
    if (argc < 1 || argc > 2 || argv[0].kind != V_STRING ||
        (argc == 2 && argv[1].kind != V_STRING)) {
        runtime_type_error(rt, site, "url_resolve expects URL and optional base URL");
        return null_value();
    }
    if (string_has_nul(argv[0]) || (argc == 2 && string_has_nul(argv[1]))) {
        runtime_value_error(rt, site, "URL cannot contain U+0000"); return null_value();
    }
    CURLU *url = curl_url();
    CURLUcode code = CURLUE_OK;
    if (url == NULL) {
        runtime_error_kind(rt, site, "ResourceLimitError", "HHY_URL_MEMORY",
                           "cannot allocate URL parser"); return null_value();
    }
    if (argc == 2) code = curl_url_set(url, CURLUPART_URL, argv[1].as.string, 0);
    if (code == CURLUE_OK)
        code = curl_url_set(url, CURLUPART_URL, argv[0].as.string, 0);
    char *scheme = NULL, *host = NULL, *port = NULL, *path = NULL, *query = NULL;
    char *user = NULL, *password = NULL, *normalized = NULL;
    if (code == CURLUE_OK) code = curl_url_get(url, CURLUPART_SCHEME, &scheme, 0);
    if (code == CURLUE_OK) code = curl_url_get(url, CURLUPART_HOST, &host, 0);
    if (code == CURLUE_OK) {
        for (char *p = host; *p != '\0'; p++) *p = (char)tolower((unsigned char)*p);
        code = curl_url_set(url, CURLUPART_HOST, host, 0);
    }
    if (code == CURLUE_OK && strcmp(scheme, "http") != 0 && strcmp(scheme, "https") != 0)
        code = CURLUE_UNSUPPORTED_SCHEME;
    if (code == CURLUE_OK &&
        (curl_url_get(url, CURLUPART_USER, &user, 0) == CURLUE_OK ||
         curl_url_get(url, CURLUPART_PASSWORD, &password, 0) == CURLUE_OK))
        code = CURLUE_USER_NOT_ALLOWED;
    if (code == CURLUE_OK) {
        (void)curl_url_set(url, CURLUPART_FRAGMENT, NULL, 0);
        if (curl_url_get(url, CURLUPART_PORT, &port, 0) == CURLUE_OK &&
            ((strcmp(scheme, "http") == 0 && strcmp(port, "80") == 0) ||
             (strcmp(scheme, "https") == 0 && strcmp(port, "443") == 0))) {
            curl_free(port); port = NULL;
            (void)curl_url_set(url, CURLUPART_PORT, NULL, 0);
        }
        (void)curl_url_get(url, CURLUPART_PATH, &path, 0);
        (void)curl_url_get(url, CURLUPART_QUERY, &query, 0);
        code = curl_url_get(url, CURLUPART_URL, &normalized, 0);
    }
    if (code != CURLUE_OK) {
        curl_free(scheme); curl_free(host); curl_free(port); curl_free(path);
        curl_free(query); curl_free(user); curl_free(password); curl_free(normalized);
        curl_url_cleanup(url);
        runtime_error_kind(rt, site, "ValueError", "HHY_URL_INVALID",
                           "URL must resolve to an absolute HTTP(S) URL without credentials");
        return null_value();
    }
    const char *keys[] = {"url", "scheme", "host", "port", "path", "query", "fingerprint"};
    Value values[] = {
        string_value(rt, normalized), string_value(rt, scheme), string_value(rt, host),
        port == NULL ? null_value() : string_value(rt, port),
        path == NULL ? string_value(rt, "/") : string_value(rt, path),
        query == NULL ? null_value() : string_value(rt, query), string_value(rt, normalized)
    };
    Value result = map_with_entries(rt, V_MAP, 7, keys, values);
    curl_free(scheme); curl_free(host); curl_free(port); curl_free(path);
    curl_free(query); curl_free(user); curl_free(password); curl_free(normalized);
    curl_url_cleanup(url);
    return result;
}

static Value builtin(Runtime *rt, Env *env, const HhyNode *site, const char *name,
                     size_t argc, Value *argv) {
    if (strcmp(name, "url_resolve") == 0) return url_resolve_value(rt, site, argc, argv);
    if (strcmp(name, "print") == 0 || strcmp(name, "print_error") == 0) {
        FILE *stream = strcmp(name, "print_error") == 0 ? stderr : stdout;
        if (argc == 1 && argv[0].kind == V_STREAM) {
            if (!stream_claim(rt, site, argv[0])) return null_value();
            Value item;
            while (stream_next(rt, site, argv[0].as.stream, &item)) {
                print_value(stream, item, false);
                fputc('\n', stream);
                fflush(stream);
            }
            stream_close(argv[0].as.stream);
            return null_value();
        }
        for (size_t i = 0; i < argc; i++) {
            if (i > 0) fputc(' ', stream);
            if (argv[i].kind == V_STREAM) {
                runtime_type_error(rt, site, "Stream must be the only argument to print");
                return null_value();
            } else {
                print_value(stream, argv[i], false);
            }
        }
        fputc('\n', stream); return argc == 0 ? null_value() : argv[argc - 1];
    }
    if (strcmp(name, "exit") == 0) {
        if (argc > 1 || (argc == 1 && argv[0].kind != V_INT)) { runtime_type_error(rt, site, "exit expects zero arguments or one Int"); return null_value(); }
        rt->exit_code = argc == 0 ? 0 : (int)argv[0].as.integer;
        rt->signal = SIGNAL_EXIT; return null_value();
    }
    if (strcmp(name, "throw") == 0) {
        if (argc != 1) { runtime_type_error(rt, site, "throw expects one argument"); return null_value(); }
        rt->failed = true;
        rt->exit_code = 1;
        rt->error_value = argv[0];
        rt->error_line = site->token.line;
        rt->error_column = site->token.column;
        return null_value();
    }
    if (strcmp(name, "length") == 0) {
        if (argc != 1) { runtime_type_error(rt, site, "length expects one argument"); return null_value(); }
        if (argv[0].kind == V_STRING) {
            size_t count;
            if (!utf8_count(argv[0].as.string, argv[0].string_length, &count)) {
                runtime_error_kind(rt, site, "EncodingError", "HHY_UTF8", "String is not valid UTF-8");
                return null_value();
            }
            return int_value((int64_t)count);
        }
        if (argv[0].kind == V_LIST) return int_value((int64_t)argv[0].as.list.count);
        if (argv[0].kind == V_MAP) return int_value((int64_t)argv[0].as.map->count);
        runtime_type_error(rt, site, "length expects String, List or Map"); return null_value();
    }
    if (strcmp(name, "type") == 0) {
        if (argc != 1) { runtime_type_error(rt, site, "type expects one argument"); return null_value(); }
        return string_value(rt, value_type(argv[0]));
    }
    if (strcmp(name, "is_type") == 0) {
        if (argc != 2 || argv[1].kind != V_STRING) {
            runtime_type_error(rt, site, "is_type expects value and type name String"); return null_value();
        }
        return bool_value(string_equals_c(argv[1], value_type(argv[0])));
    }
    if (strcmp(name, "byte_length") == 0) {
        if (argc != 1 || (argv[0].kind != V_STRING && argv[0].kind != V_BYTES_BUFFER)) {
            runtime_type_error(rt, site, "byte_length expects String or BytesBuffer"); return null_value();
        }
        return int_value((int64_t)(argv[0].kind == V_STRING
            ? argv[0].string_length : argv[0].as.bytes_buffer.length));
    }
    if (strcmp(name, "require_env") == 0) {
        if (argc != 1 || argv[0].kind != V_STRING) {
            runtime_type_error(rt, site, "require_env expects environment variable name String");
            return null_value();
        }
        if (string_has_nul(argv[0])) {
            runtime_value_error(rt, site, "environment variable name cannot contain U+0000");
            return null_value();
        }
        const char *value = getenv(argv[0].as.string);
        if (value == NULL) {
            runtime_error_kind(rt, site, "KeyError", "HHY_ENV_KEY",
                               "required environment variable is missing");
            return null_value();
        }
        return string_value(rt, value);
    }
    if (strcmp(name, "stdin_lines") == 0) {
        if (argc != 0) {
            runtime_type_error(rt, site, "stdin_lines does not accept arguments"); return null_value();
        }
        Value value = stream_value(rt, STREAM_FILE_LINES, null_value(), null_value(), env);
        value.as.stream->file = stdin;
        value.as.stream->owns_file = false;
        return value;
    }
    if (strcmp(name, "stream") == 0) {
        if (argc != 1 || (argv[0].kind != V_LIST && argv[0].kind != V_RANGE &&
                          argv[0].kind != V_MAP)) {
            runtime_type_error(rt, site, "stream expects List, Map or Range"); return null_value();
        }
        if (argv[0].kind == V_MAP) {
            Value entries = list_new(rt, argv[0].as.map->count);
            for (size_t i = 0; i < argv[0].as.map->count; i++) {
                const char *keys[] = {"key", "value"};
                Value values[] = {string_n(rt, argv[0].as.map->keys[i],
                                           argv[0].as.map->key_lengths[i]),
                                  argv[0].as.map->values[i]};
                entries.as.list.items[i] = map_with_entries(rt, V_MAP, 2, keys, values);
            }
            return stream_value(rt, STREAM_LIST, entries, null_value(), env);
        }
        return stream_value(rt, argv[0].kind == V_LIST ? STREAM_LIST : STREAM_RANGE,
                            argv[0], null_value(), env);
    }
    if (strcmp(name, "range") == 0) {
        if (argc != 2 || argv[0].kind != V_INT || argv[1].kind != V_INT) {
            runtime_type_error(rt, site, "range expects two Int arguments"); return null_value();
        }
        Value value = {.kind = V_RANGE};
        value.as.range.start = argv[0].as.integer;
        value.as.range.end = argv[1].as.integer;
        return value;
    }
    if (strcmp(name, "every") == 0) {
        if (argc != 1 || argv[0].kind != V_DURATION || argv[0].as.number <= 0) {
            runtime_value_error(rt, site, "every expects a positive Duration"); return null_value();
        }
        Value value = stream_value(rt, STREAM_EVERY, null_value(), null_value(), env);
        value.as.stream->interval_ns = argv[0].as.number;
        return value;
    }
    if (strcmp(name, "now") == 0) {
        if (argc != 0) { runtime_type_error(rt, site, "now does not accept arguments"); return null_value(); }
        struct timespec current;
        if (clock_gettime(CLOCK_REALTIME, &current) != 0) {
            runtime_io_error(rt, site, "cannot read system clock"); return null_value();
        }
        Value value = {.kind = V_DATETIME};
        value.as.datetime.nanoseconds = (int64_t)current.tv_sec * 1000000000LL + current.tv_nsec;
        return value;
    }
    if (strcmp(name, "sleep") == 0) {
        if (argc != 1 || argv[0].kind != V_DURATION || argv[0].as.number < 0) {
            runtime_value_error(rt, site, "sleep expects a non-negative Duration"); return null_value();
        }
        if (!runtime_wait_ns(rt, site, (uint64_t)argv[0].as.number)) return null_value();
        return null_value();
    }
    if (strcmp(name, "watch") == 0) {
        if (argc < 1 || argc > 2 || (argc == 2 && argv[1].kind != V_MAP)) {
            runtime_type_error(rt, site, "watch expects Path and optional options Map"); return null_value();
        }
        const char *path = require_path(rt, site, argv[0]); if (path == NULL) return null_value();
        bool recursive = false;
        if (argc == 2) {
            Value option = map_get(argv[1], "recursive");
            if (option.kind != V_NULL) {
                if (option.kind != V_BOOL) {
                    runtime_type_error(rt, site, "watch recursive option must be Bool"); return null_value();
                }
                recursive = option.as.boolean;
            }
        }
        Value value = stream_value(rt, STREAM_WATCH, null_value(), null_value(), env);
        value.as.stream->path = rt_strndup(rt, path, strlen(path));
        value.as.stream->recursive = recursive;
        return value;
    }
    if (strcmp(name, "cancel") == 0) {
        if (argc != 0) { runtime_type_error(rt, site, "cancel expects no arguments"); return null_value(); }
        rt->cancelled = true;
        runtime_check_cancel(rt, site);
        return null_value();
    }
    if (strcmp(name, "collect") == 0) {
        if (argc != 1) { runtime_type_error(rt, site, "collect expects one Stream"); return null_value(); }
        return stream_collect(rt, site, argv[0]);
    }
    if (strcmp(name, "count") == 0) {
        if (argc != 1) { runtime_type_error(rt, site, "count expects one Stream"); return null_value(); }
        Value collected = stream_collect(rt, site, argv[0]);
        return rt->failed ? null_value() : int_value((int64_t)collected.as.list.count);
    }
    if (strcmp(name, "take") == 0) {
        if (argc != 2 || argv[0].kind != V_STREAM || argv[1].kind != V_INT) { runtime_type_error(rt, site, "take expects Stream and Int"); return null_value(); }
        if (argv[0].as.stream->claimed) { runtime_value_error(rt, site, "Stream has already been consumed or attached"); return null_value(); }
        Value result = stream_value(rt, STREAM_TAKE, argv[0], null_value(), env);
        result.as.stream->remaining = argv[1].as.integer < 0 ? 0 : (size_t)argv[1].as.integer;
        return result;
    }
    if (strcmp(name, "skip") == 0) {
        if (argc != 2 || argv[0].kind != V_STREAM || argv[1].kind != V_INT) {
            runtime_type_error(rt, site, "skip expects Stream and Int"); return null_value();
        }
        if (argv[0].as.stream->claimed) { runtime_value_error(rt, site, "Stream has already been consumed or attached"); return null_value(); }
        Value result = stream_value(rt, STREAM_SKIP, argv[0], null_value(), env);
        result.as.stream->remaining = argv[1].as.integer < 0 ? 0 : (size_t)argv[1].as.integer;
        return result;
    }
    if (strcmp(name, "flat_map") == 0 || strcmp(name, "inspect") == 0) {
        if (argc != 2 || argv[0].kind != V_STREAM || argv[1].kind != V_FUNCTION) {
            runtime_type_error(rt, site, "flat_map or inspect expects Stream and closure"); return null_value();
        }
        if (argv[0].as.stream->claimed) { runtime_value_error(rt, site, "Stream has already been consumed or attached"); return null_value(); }
        StreamKind kind = strcmp(name, "flat_map") == 0 ? STREAM_FLAT_MAP : STREAM_INSPECT;
        return stream_value(rt, kind, argv[0], argv[1], env);
    }
    if (strcmp(name, "distinct") == 0) {
        if (argc != 1 || argv[0].kind != V_STREAM) {
            runtime_type_error(rt, site, "distinct expects Stream"); return null_value();
        }
        if (argv[0].as.stream->claimed) { runtime_value_error(rt, site, "Stream has already been consumed or attached"); return null_value(); }
        return stream_value(rt, STREAM_DISTINCT, argv[0], null_value(), env);
    }
    if (strcmp(name, "on_error") == 0) {
        if (argc != 2 || argv[0].kind != V_STREAM || argv[1].kind != V_FUNCTION) {
            runtime_type_error(rt, site, "on_error expects Stream and handler closure"); return null_value();
        }
        if (argv[0].as.stream->claimed) {
            runtime_value_error(rt, site, "Stream has already been consumed or attached"); return null_value();
        }
        return stream_value(rt, STREAM_ON_ERROR, argv[0], argv[1], env);
    }
    if (strcmp(name, "parallel") == 0) {
        if (argc != 3 || argv[0].kind != V_STREAM || argv[1].kind != V_INT ||
            argv[2].kind != V_FUNCTION) {
            runtime_type_error(rt, site, "parallel expects Stream, positive worker count and closure");
            return null_value();
        }
        if (argv[1].as.integer <= 0 ||
            (size_t)argv[1].as.integer > rt->limits.max_parallelism) {
            runtime_error_kind(rt, site, "ResourceLimitError", "HHY_PARALLEL_LIMIT",
                               "parallel worker count exceeds RuntimeLimits.max_parallelism");
            return null_value();
        }
        if (argv[0].as.stream->claimed) {
            runtime_value_error(rt, site, "Stream has already been consumed or attached"); return null_value();
        }
        /* Plan inspection must not create worker processes. Sequential lazy map
           preserves output order and still lets nested effects report their plan. */
        if (rt->dry_run)
            return stream_value(rt, STREAM_MAP, argv[0], argv[2], env);
        Value result = stream_value(rt, STREAM_PARALLEL, argv[0], argv[2], env);
        result.as.stream->max_parallel = (size_t)argv[1].as.integer;
        result.as.stream->jobs = rt_alloc(rt, result.as.stream->max_parallel * sizeof(ParallelJob));
        return result;
    }
    if (strcmp(name, "debounce") == 0) {
        if (argc != 2 || argv[0].kind != V_STREAM || argv[1].kind != V_DURATION ||
            argv[1].as.number <= 0) {
            runtime_type_error(rt, site, "debounce expects Stream and positive Duration");
            return null_value();
        }
        if (argv[0].as.stream->claimed) {
            runtime_value_error(rt, site, "Stream has already been consumed or attached"); return null_value();
        }
        Value result = stream_value(rt, STREAM_DEBOUNCE, argv[0], null_value(), env);
        result.as.stream->interval_ns = argv[1].as.number;
        return result;
    }
    if (strcmp(name, "sort_by") == 0) {
        if (argc != 3 || argv[0].kind != V_STREAM || argv[1].kind != V_MAP ||
            argv[2].kind != V_FUNCTION) {
            runtime_type_error(rt, site, "sort_by expects Stream, options Map and closure"); return null_value();
        }
        if (argv[0].as.stream->claimed) { runtime_value_error(rt, site, "Stream has already been consumed or attached"); return null_value(); }
        if (!require_bounded_stream(rt, site, argv[0], "sort_by")) return null_value();
        Value order = map_get(argv[1], "order");
        bool descending = false;
        if (order.kind != V_NULL) {
            if (order.kind != V_STRING ||
                (!string_equals_c(order, "asc") && !string_equals_c(order, "desc"))) {
                runtime_value_error(rt, site, "sort_by order must be asc or desc"); return null_value();
            }
            descending = string_equals_c(order, "desc");
        }
        Value result = stream_value(rt, STREAM_SORT, argv[0], argv[2], env);
        result.as.stream->descending = descending;
        return result;
    }
    if (strcmp(name, "group_by") == 0) {
        if (argc != 2 || argv[0].kind != V_STREAM || argv[1].kind != V_FUNCTION) {
            runtime_type_error(rt, site, "group_by expects Stream and closure"); return null_value();
        }
        if (argv[0].as.stream->claimed) { runtime_value_error(rt, site, "Stream has already been consumed or attached"); return null_value(); }
        if (!require_bounded_stream(rt, site, argv[0], "group_by")) return null_value();
        return stream_value(rt, STREAM_GROUP, argv[0], argv[1], env);
    }
    if (strcmp(name, "map") == 0 || strcmp(name, "where") == 0 || strcmp(name, "for_each") == 0) {
        if (argc != 2 || argv[0].kind != V_STREAM || argv[1].kind != V_FUNCTION) { runtime_type_error(rt, site, "Flow operator expects Stream and closure"); return null_value(); }
        if (argv[0].as.stream->claimed) { runtime_value_error(rt, site, "Stream has already been consumed or attached"); return null_value(); }
        if (strcmp(name, "map") == 0) return stream_value(rt, STREAM_MAP, argv[0], argv[1], env);
        if (strcmp(name, "where") == 0) return stream_value(rt, STREAM_WHERE, argv[0], argv[1], env);
        if (!stream_claim(rt, site, argv[0])) return null_value();
        Value item;
        while (stream_next(rt, site, argv[0].as.stream, &item)) {
            (void)call_value(rt, env, site, argv[1], 1, &item);
            if (rt->failed || rt->signal != SIGNAL_NONE) break;
        }
        return null_value();
    }
    if (strcmp(name, "sum") == 0) {
        if (argc != 1) { runtime_type_error(rt, site, "sum expects one Stream"); return null_value(); }
        if (!require_bounded_stream(rt, site, argv[0], "sum")) return null_value();
        if (!stream_claim(rt, site, argv[0])) return null_value();
        Value total = int_value(0);
        Value item;
        while (stream_next(rt, site, argv[0].as.stream, &item)) {
            HhyNode fake = {.kind = HHY_N_BINARY, .token = {.kind = HHY_T_PLUS, .line = site->token.line, .column = site->token.column}};
            total = binary_value(rt, &fake, total, item);
        }
        return total;
    }
    if (strcmp(name, "reduce") == 0) {
        if (argc != 3 || argv[0].kind != V_STREAM || argv[2].kind != V_FUNCTION) {
            runtime_type_error(rt, site, "reduce expects Stream, initial value and closure"); return null_value();
        }
        if (!require_bounded_stream(rt, site, argv[0], "reduce") ||
            !stream_claim(rt, site, argv[0])) return null_value();
        Value accumulator = argv[1], item;
        int64_t index = 0;
        while (stream_next(rt, site, argv[0].as.stream, &item)) {
            const char *keys[] = {"acc", "item", "index"};
            Value values[] = {accumulator, item, int_value(index++)};
            Value state = map_with_entries(rt, V_MAP, 3, keys, values);
            accumulator = call_value(rt, env, site, argv[2], 1, &state);
            if (rt->failed) break;
        }
        stream_close(argv[0].as.stream);
        return rt->failed ? null_value() : accumulator;
    }
    if (strcmp(name, "first") == 0 || strcmp(name, "last") == 0) {
        if (argc != 1) { runtime_type_error(rt, site, "first or last expects one Stream"); return null_value(); }
        if (strcmp(name, "last") == 0 && !require_bounded_stream(rt, site, argv[0], "last"))
            return null_value();
        if (!stream_claim(rt, site, argv[0])) return null_value();
        Value result = null_value(), item;
        if (strcmp(name, "first") == 0) {
            if (stream_next(rt, site, argv[0].as.stream, &item)) result = item;
        } else {
            while (stream_next(rt, site, argv[0].as.stream, &item)) result = item;
        }
        stream_close(argv[0].as.stream);
        return result;
    }
    if (strcmp(name, "min") == 0 || strcmp(name, "max") == 0) {
        if (argc != 1) { runtime_type_error(rt, site, "min or max expects one Stream"); return null_value(); }
        if (!require_bounded_stream(rt, site, argv[0], name) ||
            !stream_claim(rt, site, argv[0])) return null_value();
        Value result = null_value(), item; bool found = false;
        while (stream_next(rt, site, argv[0].as.stream, &item)) {
            if (!numeric(item)) { runtime_type_error(rt, site, "min or max currently expects numeric Stream"); break; }
            if (!found || (strcmp(name, "min") == 0 ? as_double(item) < as_double(result) :
                                                    as_double(item) > as_double(result))) {
                result = item; found = true;
            }
        }
        stream_close(argv[0].as.stream); return result;
    }
    if (strcmp(name, "any") == 0 || strcmp(name, "all") == 0) {
        if (argc != 2 || argv[1].kind != V_FUNCTION || !stream_claim(rt, site, argv[0]))
            return null_value();
        bool result = strcmp(name, "all") == 0;
        Value item;
        while (stream_next(rt, site, argv[0].as.stream, &item)) {
            Value tested = call_value(rt, env, site, argv[1], 1, &item); bool accepted = false;
            if (!require_bool(rt, site, tested, &accepted)) break;
            if (strcmp(name, "any") == 0 && accepted) { result = true; break; }
            if (strcmp(name, "all") == 0 && !accepted) { result = false; break; }
        }
        stream_close(argv[0].as.stream); return rt->failed ? null_value() : bool_value(result);
    }
    if (strcmp(name, "contains") == 0) {
        if (argc != 2) { runtime_type_error(rt, site, "contains expects collection and value"); return null_value(); }
        if (argv[0].kind == V_STRING && argv[1].kind == V_STRING)
            return bool_value(bytes_find(argv[0].as.string, argv[0].string_length,
                                         argv[1].as.string, argv[1].string_length) != NULL);
        if (argv[0].kind == V_LIST) {
            for (size_t i = 0; i < argv[0].as.list.count; i++)
                if (equal_values(argv[0].as.list.items[i], argv[1])) return bool_value(true);
            return bool_value(false);
        }
        runtime_type_error(rt, site, "contains expects String/String or List/value"); return null_value();
    }
    if (strcmp(name, "regex_match") == 0 || strcmp(name, "regex_captures") == 0) {
        if (argc != 2 || argv[0].kind != V_STRING || argv[1].kind != V_REGEX) {
            runtime_type_error(rt, site, "regex function expects String and Regex"); return null_value();
        }
        if (strlen(argv[1].as.regex.pattern) > 65536) {
            runtime_error_kind(rt, site, "ResourceLimitError", "HHY_REGEX_PATTERN_LIMIT",
                               "Regex pattern exceeds 64 KiB limit"); return null_value();
        }
        if (argv[0].string_length > 16 * 1024 * 1024) {
            runtime_error_kind(rt, site, "ResourceLimitError", "HHY_REGEX_INPUT_LIMIT",
                               "Regex input exceeds 16 MiB limit"); return null_value();
        }
        int compile_error = 0;
        PCRE2_SIZE error_offset = 0;
        pcre2_code *compiled = pcre2_compile((PCRE2_SPTR)argv[1].as.regex.pattern,
                                             PCRE2_ZERO_TERMINATED,
                                             argv[1].as.regex.flags,
                                             &compile_error, &error_offset, NULL);
        if (compiled == NULL) {
            PCRE2_UCHAR detail[192];
            pcre2_get_error_message(compile_error, detail, sizeof(detail));
            char message[256];
            snprintf(message, sizeof(message), "Regex error at byte %zu: %s",
                     (size_t)error_offset, (char *)detail);
            runtime_error_kind(rt, site, "ValueError", "HHY_REGEX_PATTERN", message); return null_value();
        }
        uint32_t capture_count = 0;
        pcre2_pattern_info(compiled, PCRE2_INFO_CAPTURECOUNT, &capture_count);
        if (capture_count > 128) {
            pcre2_code_free(compiled);
            runtime_error_kind(rt, site, "ResourceLimitError", "HHY_REGEX_CAPTURE_LIMIT",
                               "Regex exceeds 128 capture group limit"); return null_value();
        }
        pcre2_match_context *context = pcre2_match_context_create(NULL);
        pcre2_set_match_limit(context, rt->limits.max_regex_steps);
        pcre2_set_depth_limit(context, 1000);
        pcre2_set_heap_limit(context, 16 * 1024);
        pcre2_match_data *match_data = pcre2_match_data_create_from_pattern(compiled, NULL);
        int matched = pcre2_match(compiled, (PCRE2_SPTR)argv[0].as.string,
                                  argv[0].string_length, 0, 0, match_data, context);
        pcre2_match_context_free(context);
        if (matched < 0 && matched != PCRE2_ERROR_NOMATCH) {
            pcre2_match_data_free(match_data); pcre2_code_free(compiled);
            if (matched == PCRE2_ERROR_MATCHLIMIT || matched == PCRE2_ERROR_DEPTHLIMIT ||
                matched == PCRE2_ERROR_HEAPLIMIT) {
                runtime_error_kind(rt, site, "ResourceLimitError", "HHY_REGEX_EXECUTION_LIMIT",
                                   "Regex execution limit exceeded");
            } else {
                runtime_error_kind(rt, site, "ValueError", "HHY_REGEX_EXECUTION",
                                   "Regex execution failed");
            }
            return null_value();
        }
        if (strcmp(name, "regex_match") == 0) {
            pcre2_match_data_free(match_data); pcre2_code_free(compiled);
            return bool_value(matched >= 0);
        }
        if (matched == PCRE2_ERROR_NOMATCH) {
            pcre2_match_data_free(match_data); pcre2_code_free(compiled); return null_value();
        }
        PCRE2_SIZE *matches = pcre2_get_ovector_pointer(match_data);
        const char *keys[] = {"match", "start", "end"};
        Value values[] = {
            string_n(rt, argv[0].as.string + matches[0], (size_t)(matches[1] - matches[0])),
            int_value((int64_t)matches[0]), int_value((int64_t)matches[1])
        };
        Value result = map_with_entries(rt, V_MAP, 3, keys, values);
        if (capture_count > 0) {
            Value groups = list_new(rt, capture_count);
            for (size_t i = 1; i <= capture_count; i++) {
                PCRE2_SIZE start = matches[i * 2], end = matches[i * 2 + 1];
                if (start == PCRE2_UNSET) { groups.as.list.items[i - 1] = null_value(); continue; }
                const char *group_keys[] = {"match", "start", "end"};
                Value group_values[] = {
                    string_n(rt, argv[0].as.string + start, (size_t)(end - start)),
                    int_value((int64_t)start), int_value((int64_t)end)
                };
                groups.as.list.items[i - 1] = map_with_entries(rt, V_MAP, 3, group_keys, group_values);
            }
            result = map_put_runtime(rt, result, "groups", groups);
        }
        uint32_t name_count = 0, entry_size = 0;
        PCRE2_SPTR name_table = NULL;
        pcre2_pattern_info(compiled, PCRE2_INFO_NAMECOUNT, &name_count);
        if (name_count > 0) {
            pcre2_pattern_info(compiled, PCRE2_INFO_NAMETABLE, &name_table);
            pcre2_pattern_info(compiled, PCRE2_INFO_NAMEENTRYSIZE, &entry_size);
            Value named = {.kind = V_MAP}; named.as.map = map_storage_new(rt, name_count);
            named.as.map->keys = rt_alloc(rt, name_count * sizeof(char *));
            named.as.map->key_lengths = rt_alloc(rt, name_count * sizeof(size_t));
            named.as.map->values = rt_alloc(rt, name_count * sizeof(Value));
            for (uint32_t i = 0; i < name_count; i++) {
                PCRE2_SPTR entry = name_table + i * entry_size;
                uint32_t group = ((uint32_t)entry[0] << 8) | entry[1];
                named.as.map->keys[i] = rt_strndup(rt, (const char *)(entry + 2),
                                                  strlen((const char *)(entry + 2)));
                named.as.map->key_lengths[i] = strlen((const char *)(entry + 2));
                PCRE2_SIZE start = matches[group * 2], end = matches[group * 2 + 1];
                if (start == PCRE2_UNSET) named.as.map->values[i] = null_value();
                else {
                    const char *group_keys[] = {"match", "start", "end"};
                    Value group_values[] = {
                        string_n(rt, argv[0].as.string + start, (size_t)(end - start)),
                        int_value((int64_t)start), int_value((int64_t)end)
                    };
                    named.as.map->values[i] = map_with_entries(rt, V_MAP, 3,
                                                              group_keys, group_values);
                }
            }
            result = map_put_runtime(rt, result, "named", named);
        }
        pcre2_match_data_free(match_data); pcre2_code_free(compiled);
        return result;
    }
    if (strcmp(name, "starts_with") == 0 || strcmp(name, "ends_with") == 0) {
        if (argc != 2 || argv[0].kind != V_STRING || argv[1].kind != V_STRING) {
            runtime_type_error(rt, site, "starts_with or ends_with expects two Strings"); return null_value();
        }
        size_t text_length = argv[0].string_length, part_length = argv[1].string_length;
        bool matches = false;
        if (part_length <= text_length) {
            const char *start = strcmp(name, "starts_with") == 0
                ? argv[0].as.string : argv[0].as.string + text_length - part_length;
            matches = memcmp(start, argv[1].as.string, part_length) == 0;
        }
        return bool_value(matches);
    }
    if (strcmp(name, "split") == 0) {
        if (argc != 2 || argv[0].kind != V_STRING || argv[1].kind != V_STRING ||
            argv[1].string_length == 0) {
            runtime_type_error(rt, site, "split expects String and non-empty separator"); return null_value();
        }
        const char *text = argv[0].as.string, *separator = argv[1].as.string;
        size_t separator_length = argv[1].string_length, count = 1;
        const char *limit = text + argv[0].string_length;
        for (const char *p = text; p <= limit; p += separator_length) {
            p = bytes_find(p, (size_t)(limit - p), separator, separator_length);
            if (p == NULL) break;
            count++;
        }
        Value result = list_new(rt, count); size_t index = 0;
        const char *start = text, *found;
        while ((found = bytes_find(start, (size_t)(limit - start), separator,
                                   separator_length)) != NULL) {
            result.as.list.items[index++] = string_n(rt, start, (size_t)(found - start));
            start = found + separator_length;
        }
        result.as.list.items[index] = string_n(rt, start, (size_t)(limit - start));
        return result;
    }
    if (strcmp(name, "join") == 0) {
        if (argc != 2 || argv[0].kind != V_LIST || argv[1].kind != V_STRING) {
            runtime_type_error(rt, site, "join expects List<String> and separator"); return null_value();
        }
        size_t separator_length = argv[1].string_length, length = 0;
        for (size_t i = 0; i < argv[0].as.list.count; i++) {
            if (argv[0].as.list.items[i].kind != V_STRING) {
                runtime_type_error(rt, site, "join List items must be String"); return null_value();
            }
            length += argv[0].as.list.items[i].string_length;
            if (i + 1 < argv[0].as.list.count) length += separator_length;
        }
        char *text = rt_alloc(rt, length + 1), *out = text;
        for (size_t i = 0; i < argv[0].as.list.count; i++) {
            size_t item_length = argv[0].as.list.items[i].string_length;
            memcpy(out, argv[0].as.list.items[i].as.string, item_length); out += item_length;
            if (i + 1 < argv[0].as.list.count) {
                memcpy(out, argv[1].as.string, separator_length); out += separator_length;
            }
        }
        *out = '\0'; Value value = {.kind = V_STRING, .string_length = length}; value.as.string = text; return value;
    }
    if (strcmp(name, "trim") == 0) {
        if (argc != 1 || argv[0].kind != V_STRING) { runtime_type_error(rt, site, "trim expects String"); return null_value(); }
        const char *start = argv[0].as.string;
        const char *end = start + argv[0].string_length;
        while (start < end && (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r')) start++;
        while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r')) end--;
        return string_n(rt, start, (size_t)(end - start));
    }
    if (strcmp(name, "trim_start") == 0 || strcmp(name, "trim_end") == 0) {
        if (argc != 1 || argv[0].kind != V_STRING) {
            runtime_type_error(rt, site, "trim_start or trim_end expects String"); return null_value();
        }
        const char *start = argv[0].as.string, *end = start + argv[0].string_length;
        if (strcmp(name, "trim_start") == 0)
            while (start < end && (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r')) start++;
        else
            while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r')) end--;
        return string_n(rt, start, (size_t)(end - start));
    }
    if (strcmp(name, "replace") == 0) {
        if (argc != 3 || argv[0].kind != V_STRING || argv[1].kind != V_STRING || argv[2].kind != V_STRING) {
            runtime_type_error(rt, site, "replace expects three Strings"); return null_value();
        }
        const char *text = argv[0].as.string, *from = argv[1].as.string, *to = argv[2].as.string;
        size_t from_length = argv[1].string_length, to_length = argv[2].string_length, count = 0;
        if (from_length == 0) return argv[0];
        const char *limit = text + argv[0].string_length;
        for (const char *p = text; p <= limit; p += from_length) {
            p = bytes_find(p, (size_t)(limit - p), from, from_length);
            if (p == NULL) break;
            count++;
        }
        size_t result_length = argv[0].string_length - count * from_length + count * to_length;
        char *result = rt_alloc(rt, result_length + 1), *out = result;
        const char *cursor = text, *found;
        while ((found = bytes_find(cursor, (size_t)(limit - cursor), from, from_length)) != NULL) {
            size_t prefix = (size_t)(found - cursor); memcpy(out, cursor, prefix); out += prefix;
            memcpy(out, to, to_length); out += to_length; cursor = found + from_length;
        }
        memcpy(out, cursor, (size_t)(limit - cursor)); out += limit - cursor; *out = '\0';
        Value value = {.kind = V_STRING, .string_length = result_length}; value.as.string = result; return value;
    }
    if (strcmp(name, "upper") == 0 || strcmp(name, "lower") == 0) {
        if (argc != 1 || argv[0].kind != V_STRING) { runtime_type_error(rt, site, "upper or lower expects String"); return null_value(); }
        return utf8_case(rt, site, argv[0].as.string, argv[0].string_length,
                         strcmp(name, "upper") == 0);
    }
    if (strcmp(name, "get") == 0) {
        if (argc != 2) { runtime_type_error(rt, site, "get expects collection and key or index"); return null_value(); }
        if (argv[0].kind == V_LIST && argv[1].kind == V_INT) {
            if (argv[1].as.integer < 0 || (size_t)argv[1].as.integer >= argv[0].as.list.count) {
                runtime_error_kind(rt, site, "IndexError", "HHY_INDEX", "list index out of bounds");
                return null_value();
            }
            return argv[0].as.list.items[argv[1].as.integer];
        }
        if (argv[1].kind == V_STRING)
            return map_get_n(argv[0], argv[1].as.string, argv[1].string_length);
        runtime_type_error(rt, site, "get expects Map/String or List/Int"); return null_value();
    }
    if (strcmp(name, "pick") == 0) {
        if (argc != 2 || argv[0].kind != V_MAP || argv[1].kind != V_LIST) {
            runtime_type_error(rt, site, "pick expects Map and List<String>"); return null_value();
        }
        Value result = {.kind = V_MAP};
        result.as.map = map_storage_new(rt, 0);
        result.as.map->keys = argv[1].as.list.count
            ? rt_alloc(rt, argv[1].as.list.count * sizeof(char *)) : NULL;
        result.as.map->key_lengths = argv[1].as.list.count
            ? rt_alloc(rt, argv[1].as.list.count * sizeof(size_t)) : NULL;
        result.as.map->values = argv[1].as.list.count
            ? rt_alloc(rt, argv[1].as.list.count * sizeof(Value)) : NULL;
        for (size_t i = 0; i < argv[1].as.list.count; i++) {
            Value key = argv[1].as.list.items[i];
            if (key.kind != V_STRING) {
                runtime_type_error(rt, site, "pick keys must be String"); return null_value();
            }
            Value selected;
            if (!map_lookup_n(argv[0], key.as.string, key.string_length, &selected)) continue;
            result.as.map->keys[result.as.map->count] = rt_strndup(rt, key.as.string, key.string_length);
            result.as.map->key_lengths[result.as.map->count] = key.string_length;
            result.as.map->values[result.as.map->count++] = selected;
        }
        return result;
    }
    if (strcmp(name, "require") == 0) {
        if (argc != 2 || argv[0].kind != V_MAP || argv[1].kind != V_STRING) {
            runtime_type_error(rt, site, "require expects Map and String key"); return null_value();
        }
        for (size_t i = 0; i < argv[0].as.map->count; i++)
            if (argv[0].as.map->key_lengths[i] == argv[1].string_length &&
                memcmp(argv[0].as.map->keys[i], argv[1].as.string, argv[1].string_length) == 0)
                return argv[0].as.map->values[i];
        runtime_error_kind(rt, site, "KeyError", "HHY_MAP_KEY",
                           "required Map key is missing"); return null_value();
    }
    if (strcmp(name, "put") == 0) {
        if (argc != 3 || argv[0].kind != V_MAP || argv[1].kind != V_STRING) {
            runtime_type_error(rt, site, "put expects Map, String key and value"); return null_value();
        }
        return map_put_runtime_n(rt, argv[0], argv[1].as.string, argv[1].string_length, argv[2]);
    }
    if (strcmp(name, "remove_key") == 0) {
        if (argc != 2 || argv[0].kind != V_MAP || argv[1].kind != V_STRING) {
            runtime_type_error(rt, site, "remove_key expects Map and String key"); return null_value();
        }
        size_t count = argv[0].as.map->count;
        for (size_t i = 0; i < argv[0].as.map->count; i++)
            if (argv[0].as.map->key_lengths[i] == argv[1].string_length &&
                memcmp(argv[0].as.map->keys[i], argv[1].as.string, argv[1].string_length) == 0) {
                count--; break;
            }
        Value result = {.kind = V_MAP}; result.as.map = map_storage_new(rt, count);
        result.as.map->keys = count ? rt_alloc(rt, count * sizeof(char *)) : NULL;
        result.as.map->key_lengths = count ? rt_alloc(rt, count * sizeof(size_t)) : NULL;
        result.as.map->values = count ? rt_alloc(rt, count * sizeof(Value)) : NULL;
        size_t out = 0;
        for (size_t i = 0; i < argv[0].as.map->count; i++) {
            if (argv[0].as.map->key_lengths[i] == argv[1].string_length &&
                memcmp(argv[0].as.map->keys[i], argv[1].as.string, argv[1].string_length) == 0) continue;
            result.as.map->keys[out] = argv[0].as.map->keys[i];
            result.as.map->key_lengths[out] = argv[0].as.map->key_lengths[i];
            result.as.map->values[out++] = argv[0].as.map->values[i];
        }
        return result;
    }
    if (strcmp(name, "append") == 0) {
        if (argc != 2 || argv[0].kind != V_LIST) {
            runtime_type_error(rt, site, "append expects List and value"); return null_value();
        }
        Value result = list_new(rt, argv[0].as.list.count + 1);
        if (argv[0].as.list.count) memcpy(result.as.list.items, argv[0].as.list.items,
                                          argv[0].as.list.count * sizeof(Value));
        result.as.list.items[argv[0].as.list.count] = argv[1]; return result;
    }
    if (strcmp(name, "remove_at") == 0) {
        if (argc != 2 || argv[0].kind != V_LIST || argv[1].kind != V_INT ||
            argv[1].as.integer < 0 || (size_t)argv[1].as.integer >= argv[0].as.list.count) {
            runtime_index_error(rt, site, "remove_at expects List and valid non-negative index"); return null_value();
        }
        size_t removed = (size_t)argv[1].as.integer;
        Value result = list_new(rt, argv[0].as.list.count - 1);
        for (size_t i = 0, out = 0; i < argv[0].as.list.count; i++)
            if (i != removed) result.as.list.items[out++] = argv[0].as.list.items[i];
        return result;
    }
    if (strcmp(name, "path") == 0) {
        if (argc != 1 || argv[0].kind != V_STRING) { runtime_type_error(rt, site, "path expects String"); return null_value(); }
        if (string_has_nul(argv[0])) {
            runtime_value_error(rt, site, "Path cannot contain U+0000"); return null_value();
        }
        return path_value_normalized(rt, argv[0].as.string);
    }
    if (strcmp(name, "path_join") == 0) {
        if (argc != 2 || argv[0].kind != V_PATH ||
            (argv[1].kind != V_STRING && argv[1].kind != V_PATH)) {
            runtime_type_error(rt, site, "path_join expects Path and String or Path"); return null_value();
        }
        if (string_has_nul(argv[1])) {
            runtime_value_error(rt, site, "Path component cannot contain U+0000"); return null_value();
        }
        size_t left = argv[0].string_length, right = argv[1].string_length;
        bool separator = left > 0 && argv[0].as.string[left - 1] != '/';
        char *joined = rt_alloc(rt, left + right + (separator ? 2 : 1));
        memcpy(joined, argv[0].as.string, left);
        size_t offset = left;
        if (separator) joined[offset++] = '/';
        memcpy(joined + offset, argv[1].as.string, right + 1);
        Value value = path_value_normalized(rt, joined);
        return value;
    }
    if (strcmp(name, "read_text") == 0) {
        if (argc != 1) { runtime_type_error(rt, site, "read_text expects one Path"); return null_value(); }
        const char *path = require_path(rt, site, argv[0]);
        return path == NULL ? null_value() : read_text_file(rt, site, path);
    }
    if (strcmp(name, "read_bytes") == 0) {
        if (argc != 1) { runtime_type_error(rt, site, "read_bytes expects one Path"); return null_value(); }
        const char *path = require_path(rt, site, argv[0]);
        return path == NULL ? null_value() : read_bytes_file(rt, site, path);
    }
    if (strcmp(name, "read_lines") == 0) {
        if (argc != 1) { runtime_type_error(rt, site, "read_lines expects one Path"); return null_value(); }
        const char *path = require_path(rt, site, argv[0]);
        if (path == NULL) return null_value();
        Value value = stream_value(rt, STREAM_FILE_LINES, null_value(), null_value(), env);
        value.as.stream->path = rt_strndup(rt, path, strlen(path));
        return value;
    }
    if (strcmp(name, "files") == 0) {
        if (argc < 2 || argc > 3 || argv[1].kind != V_STRING ||
            (argc == 3 && argv[2].kind != V_MAP)) {
            runtime_type_error(rt, site, "files expects Path, glob String and optional options Map"); return null_value();
        }
        if (string_has_nul(argv[1])) {
            runtime_value_error(rt, site, "file glob cannot contain U+0000"); return null_value();
        }
        const char *path = require_path(rt, site, argv[0]); if (path == NULL) return null_value();
        struct stat info;
        if (stat(path, &info) != 0 || !S_ISDIR(info.st_mode)) {
            runtime_io_error(rt, site, "files source must be an existing directory"); return null_value();
        }
        Value value = stream_value(rt, STREAM_FILES, null_value(), null_value(), env);
        value.as.stream->path = rt_strndup(rt, path, strlen(path));
        value.as.stream->pattern = rt_strndup(rt, argv[1].as.string, argv[1].string_length);
        if (argc == 3) {
            Value follow = map_get(argv[2], "follow_symlinks");
            if (follow.kind != V_NULL && follow.kind != V_BOOL) {
                runtime_type_error(rt, site, "files follow_symlinks option must be Bool"); return null_value();
            }
            value.as.stream->follow_symlinks = follow.kind == V_BOOL && follow.as.boolean;
        }
        return value;
    }
    if (strcmp(name, "write_text") == 0) {
        if (argc < 2 || argc > 3 || argv[1].kind != V_STRING) { runtime_type_error(rt, site, "write_text expects Path, String and optional options Map"); return null_value(); }
        const char *path = require_path(rt, site, argv[0]);
        OutputOptions options;
        if (!output_options(rt, site, argc - 2, argc == 3 ? argv[2] : null_value(), &options)) return null_value();
        if (!rt->effect_allowed) return argv[0];
        if (path == NULL || !write_text_atomic(rt, site, path, argv[1].as.string,
                                               argv[1].string_length, options)) return null_value();
        return argv[0];
    }
    if (strcmp(name, "write_bytes") == 0) {
        if (argc < 2 || argc > 3 || argv[1].kind != V_BYTES_BUFFER) {
            runtime_type_error(rt, site, "write_bytes expects Path, BytesBuffer and optional options Map"); return null_value();
        }
        const char *path = require_path(rt, site, argv[0]);
        OutputOptions options;
        if (!output_options(rt, site, argc - 2, argc == 3 ? argv[2] : null_value(), &options)) return null_value();
        if (!rt->effect_allowed) return argv[0];
        if (path == NULL || !write_bytes_atomic(rt, site, path, argv[1].as.bytes_buffer.data,
                                                argv[1].as.bytes_buffer.length, options)) return null_value();
        return argv[0];
    }
    if (strcmp(name, "append_text") == 0) {
        if (argc != 2 || argv[1].kind != V_STRING) { runtime_type_error(rt, site, "append_text expects Path and String"); return null_value(); }
        const char *path = require_path(rt, site, argv[0]); if (path == NULL) return null_value();
        if (!rt->effect_allowed) return argv[0];
        FILE *file = runtime_fopen(rt, site, path, "ab");
        if (file == NULL) { if (!rt->failed) runtime_io_error(rt, site, "cannot open file for append"); return null_value(); }
        size_t text_length = argv[1].string_length;
        bool written = fwrite(argv[1].as.string, 1, text_length, file) == text_length;
        bool closed = runtime_fclose(rt, file) == 0;
        if (!written || !closed) {
            runtime_io_error(rt, site, "cannot append text"); return null_value();
        }
        return argv[0];
    }
    if (strcmp(name, "copy") == 0 || strcmp(name, "move") == 0) {
        if (argc < 2 || argc > 3 || (argc == 3 && argv[2].kind != V_MAP)) {
            runtime_type_error(rt, site, "copy or move expects source Path, target Path and optional options"); return null_value();
        }
        const char *source = require_path(rt, site, argv[0]);
        const char *target = require_path(rt, site, argv[1]);
        if (source == NULL || target == NULL) return null_value();
        if (!rt->effect_allowed) return argv[1];
        bool overwrite = false;
        if (argc == 3) {
            Value option = map_get(argv[2], "overwrite");
            if (option.kind != V_NULL) {
                if (option.kind != V_BOOL) { runtime_type_error(rt, site, "overwrite option must be Bool"); return null_value(); }
                overwrite = option.as.boolean;
            }
        }
        struct stat target_info;
        if (!overwrite && lstat(target, &target_info) == 0) {
            runtime_io_error(rt, site, "target already exists"); return null_value();
        }
        if (strcmp(name, "copy") == 0) {
            if (!copy_file_atomic(rt, site, source, target, overwrite)) return null_value();
        } else if (atomic_rename(source, target, overwrite) != 0) {
            if (errno != EXDEV || !copy_file_atomic(rt, site, source, target, overwrite) ||
                unlink(source) != 0) {
                runtime_io_error(rt, site, "move failed"); return null_value();
            }
        }
        return argv[1];
    }
    if (strcmp(name, "remove") == 0) {
        if (argc != 1) { runtime_type_error(rt, site, "remove expects one Path"); return null_value(); }
        const char *path = require_path(rt, site, argv[0]); if (path == NULL) return null_value();
        if (!rt->effect_allowed) return argv[0];
        struct stat info;
        if (lstat(path, &info) != 0) { runtime_io_error(rt, site, "remove target does not exist"); return null_value(); }
        int removed = S_ISDIR(info.st_mode) ? rmdir(path) : unlink(path);
        if (removed != 0) { runtime_io_error(rt, site, "remove failed"); return null_value(); }
        return argv[0];
    }
    if (strcmp(name, "save_text") == 0) {
        if (argc < 2 || argc > 3 || (argv[0].kind != V_STRING && argv[0].kind != V_STREAM)) {
            runtime_type_error(rt, site, "save_text expects String or Stream<String>, Path and optional options Map");
            return null_value();
        }
        const char *path = require_path(rt, site, argv[1]);
        OutputOptions options;
        if (!output_options(rt, site, argc - 2, argc == 3 ? argv[2] : null_value(), &options)) return null_value();
        if (path == NULL) return null_value();
        if (!rt->effect_allowed) return argv[1];
        if (argv[0].kind == V_STREAM) {
            if (!write_string_stream_atomic(rt, site, argv[0], path, false, options)) return null_value();
            return argv[1];
        }
        if (!write_text_atomic(rt, site, path, argv[0].as.string,
                               argv[0].string_length, options)) return null_value();
        return argv[1];
    }
    if (strcmp(name, "save_lines") == 0) {
        if (argc < 2 || argc > 3 || argv[0].kind != V_STREAM) { runtime_type_error(rt, site, "save_lines expects Stream, Path and optional options Map"); return null_value(); }
        const char *path = require_path(rt, site, argv[1]); if (path == NULL) return null_value();
        OutputOptions options;
        if (!output_options(rt, site, argc - 2, argc == 3 ? argv[2] : null_value(), &options)) return null_value();
        if (!rt->effect_allowed) return argv[1];
        if (!write_string_stream_atomic(rt, site, argv[0], path, true, options)) return null_value();
        return argv[1];
    }
    if (strcmp(name, "encode_json") == 0) {
        if (argc < 1 || argc > 2 || (argc == 2 && argv[1].kind != V_MAP)) {
            runtime_type_error(rt, site, "encode_json expects a value and optional options Map"); return null_value();
        }
        bool pretty = false;
        if (argc == 2) {
            Value option = map_get(argv[1], "pretty");
            if (option.kind != V_NULL) {
                if (option.kind != V_BOOL) {
                    runtime_type_error(rt, site, "encode_json pretty option must be Bool"); return null_value();
                }
                pretty = option.as.boolean;
            }
        }
        FILE *file = runtime_tmpfile(rt, site);
        if (file == NULL) { runtime_error_kind(rt, site, "ResourceLimitError", "HHY_JSON_BUFFER", "cannot allocate JSON buffer"); return null_value(); }
        if (!json_encode_value(file, argv[0], pretty, 0)) {
            runtime_fclose(rt, file);
            runtime_error_kind(rt, site, "TypeError", "HHY_JSON_ENCODE",
                               "value cannot be encoded as JSON"); return null_value();
        }
        long size = ftell(file); rewind(file);
        if (size < 0) { runtime_fclose(rt, file); runtime_value_error(rt, site, "cannot encode JSON"); return null_value(); }
        char *text = rt_alloc(rt, (size_t)size + 1);
        size_t read = fread(text, 1, (size_t)size, file); runtime_fclose(rt, file); text[read] = '\0';
        Value result = {.kind = V_STRING, .string_length = read}; result.as.string = text; return result;
    }
    if (strcmp(name, "parse_json") == 0) {
        if (argc != 1 || argv[0].kind != V_STRING) {
            runtime_type_error(rt, site, "parse_json expects String"); return null_value();
        }
        return parse_json_text(rt, site, argv[0].as.string, argv[0].string_length);
    }
    if (strcmp(name, "parse_csv") == 0 || strcmp(name, "encode_csv") == 0) {
        if (argc < 1 || argc > 2 || (argc == 2 && argv[1].kind != V_MAP)) {
            runtime_type_error(rt, site, "CSV operator expects input and optional options Map"); return null_value();
        }
        Value source = argv[0];
        if (strcmp(name, "parse_csv") == 0 && source.kind == V_STRING)
            source = string_lines(rt, source.as.string, source.string_length);
        if (source.kind != V_STREAM || source.as.stream->claimed) {
            runtime_value_error(rt, site, "CSV operator expects a fresh Stream"); return null_value();
        }
        bool header = true;
        char delimiter = ',';
        char quote = '"';
        if (argc == 2) {
            Value header_option = map_get(argv[1], "header");
            Value delimiter_option = map_get(argv[1], "delimiter");
            Value quote_option = map_get(argv[1], "quote");
            if (header_option.kind != V_NULL) {
                if (header_option.kind != V_BOOL) { runtime_type_error(rt, site, "CSV header option must be Bool"); return null_value(); }
                header = header_option.as.boolean;
            }
            if (delimiter_option.kind != V_NULL) {
                if (delimiter_option.kind != V_STRING || delimiter_option.string_length != 1) {
                    runtime_value_error(rt, site, "CSV delimiter must be one-character String"); return null_value();
                }
                if (string_has_nul(delimiter_option)) {
                    runtime_value_error(rt, site, "CSV delimiter cannot be U+0000"); return null_value();
                }
                delimiter = delimiter_option.as.string[0];
            }
            if (quote_option.kind != V_NULL) {
                if (quote_option.kind != V_STRING || quote_option.string_length != 1) {
                    runtime_value_error(rt, site, "CSV quote must be one-character String"); return null_value();
                }
                if (string_has_nul(quote_option)) {
                    runtime_value_error(rt, site, "CSV quote cannot be U+0000"); return null_value();
                }
                quote = quote_option.as.string[0];
            }
            if (delimiter == quote || delimiter == '\n' || delimiter == '\r' ||
                quote == '\n' || quote == '\r') {
                runtime_value_error(rt, site, "CSV delimiter and quote must be distinct non-newline characters");
                return null_value();
            }
        }
        StreamKind kind = strcmp(name, "parse_csv") == 0 ? STREAM_CSV_PARSE : STREAM_CSV_ENCODE;
        Value result = stream_value(rt, kind, source, null_value(), env);
        result.as.stream->csv_use_header = header;
        result.as.stream->delimiter = delimiter;
        result.as.stream->quote = quote;
        return result;
    }
    if (strcmp(name, "run") == 0) {
        if (argc < 1 || argc > 2) {
            runtime_type_error(rt, site, "run expects argument List and optional options Map"); return null_value();
        }
        if (argc == 2 && argv[1].kind != V_MAP) {
            runtime_type_error(rt, site, "run options must be Map"); return null_value();
        }
        return command_run(rt, site, argv[0], argc == 2 ? argv[1] : null_value());
    }
    if (strcmp(name, "shell") == 0) {
        if (argc < 1 || argc > 2 || argv[0].kind != V_STRING ||
            (argc == 2 && argv[1].kind != V_MAP)) {
            runtime_type_error(rt, site, "shell expects command String and optional options Map");
            return null_value();
        }
        Value command = list_new(rt, 3);
        command.as.list.items[0] = string_value(rt, "/bin/sh");
        command.as.list.items[1] = string_value(rt, "-c");
        command.as.list.items[2] = argv[0];
        return command_run(rt, site, command, argc == 2 ? argv[1] : null_value());
    }
    if (strcmp(name, "processes") == 0) {
        if (argc != 0) { runtime_type_error(rt, site, "processes does not accept arguments"); return null_value(); }
        return stream_value(rt, STREAM_PROCESSES, null_value(), null_value(), env);
    }
    if (strcmp(name, "stdout_lines") == 0) {
        if (argc != 1 || argv[0].kind != V_COMMAND_RESULT) {
            runtime_type_error(rt, site, "stdout_lines expects CommandResult"); return null_value();
        }
        Value output = map_get(argv[0], "stdout");
        if (output.kind != V_STRING) {
            runtime_value_error(rt, site, "CommandResult has no stdout String"); return null_value();
        }
        return string_lines(rt, output.as.string, output.string_length);
    }
    if (strcmp(name, "http.get") == 0) return http_request(rt, site, "GET", argc, argv);
    if (strcmp(name, "http.post") == 0) return http_request(rt, site, "POST", argc, argv);
    if (strcmp(name, "http.put") == 0) return http_request(rt, site, "PUT", argc, argv);
    if (strcmp(name, "http.delete") == 0) return http_request(rt, site, "DELETE", argc, argv);
    if (strcmp(name, "datetime.parse") == 0) return datetime_parse(rt, site, argc, argv);
    if (strcmp(name, "timeout") == 0) {
        if (argc != 2 || argv[0].kind != V_HTTP_REQUEST || argv[1].kind != V_DURATION) {
            runtime_type_error(rt, site, "timeout expects HttpRequest and Duration"); return null_value();
        }
        return map_put_runtime(rt, argv[0], "timeout_ns", argv[1]);
    }
    if (strcmp(name, "retry") == 0) {
        if (argc != 2 || argv[0].kind != V_HTTP_REQUEST || argv[1].kind != V_MAP) {
            runtime_type_error(rt, site, "retry expects HttpRequest and options Map"); return null_value();
        }
        Value count = map_get(argv[1], "count");
        if (count.kind != V_INT || count.as.integer < 0) {
            runtime_value_error(rt, site, "retry count must be non-negative Int"); return null_value();
        }
        Value method = map_get(argv[0], "method");
        if (string_equals_c(method, "POST") && count.as.integer > 0) {
            runtime_value_error(rt, site, "POST requests are not retried by default"); return null_value();
        }
        Value backoff = map_get(argv[1], "backoff");
        if (backoff.kind != V_NULL && (backoff.kind != V_DURATION || backoff.as.number < 0)) {
            runtime_value_error(rt, site, "retry backoff must be a non-negative Duration"); return null_value();
        }
        Value result = map_put_runtime(rt, argv[0], "retry_count", count);
        if (backoff.kind != V_NULL) result = map_put_runtime(rt, result, "retry_backoff_ns", backoff);
        return result;
    }
    if (strcmp(name, "send") == 0) {
        if (argc != 1) { runtime_type_error(rt, site, "send expects one HttpRequest"); return null_value(); }
        return http_send(rt, site, argv[0], null_value());
    }
    if (strcmp(name, "send_to") == 0) {
        if (argc != 2 || argv[1].kind != V_PATH) {
            runtime_type_error(rt, site, "send_to expects HttpRequest and Path"); return null_value();
        }
        return http_send(rt, site, argv[0], argv[1]);
    }
    if (strcmp(name, "response_body") == 0) {
        if (argc != 1 || argv[0].kind != V_HTTP_RESPONSE) {
            runtime_type_error(rt, site, "response_body expects HttpResponse"); return null_value();
        }
        Value status = map_get(argv[0], "status");
        Value ok = map_get(argv[0], "ok");
        if (status.kind != V_INT || ok.kind != V_BOOL) {
            runtime_value_error(rt, site, "invalid HttpResponse"); return null_value();
        }
        if (!ok.as.boolean) {
            char message[96];
            snprintf(message, sizeof(message), "HTTP request failed with status %" PRId64, status.as.integer);
            runtime_error_kind(rt, site, "HttpStatusError", "HHY_HTTP_STATUS", message); return null_value();
        }
        Value body = map_get(argv[0], "bytes");
        size_t codepoints = 0;
        if (body.kind != V_BYTES_BUFFER ||
            !utf8_count((const char *)body.as.bytes_buffer.data,
                        body.as.bytes_buffer.length, &codepoints)) {
            runtime_error_kind(rt, site, "EncodingError", "HHY_HTTP_UTF8",
                               "HTTP response body is not valid UTF-8; use response_bytes");
            return null_value();
        }
        return string_n(rt, body.as.bytes_buffer.data == NULL ? "" :
                        (const char *)body.as.bytes_buffer.data, body.as.bytes_buffer.length);
    }
    if (strcmp(name, "response_bytes") == 0) {
        if (argc != 1 || argv[0].kind != V_HTTP_RESPONSE) {
            runtime_type_error(rt, site, "response_bytes expects HttpResponse"); return null_value();
        }
        Value status = map_get(argv[0], "status"), ok = map_get(argv[0], "ok");
        if (status.kind != V_INT || ok.kind != V_BOOL) {
            runtime_value_error(rt, site, "invalid HttpResponse"); return null_value();
        }
        if (!ok.as.boolean) {
            char message[96];
            snprintf(message, sizeof(message), "HTTP request failed with status %" PRId64, status.as.integer);
            runtime_error_kind(rt, site, "HttpStatusError", "HHY_HTTP_STATUS", message); return null_value();
        }
        return map_get(argv[0], "bytes");
    }
    if (strcmp(name, "to_float") == 0) {
        if (argc != 1) { runtime_type_error(rt, site, "to_float expects one argument"); return null_value(); }
        if (numeric(argv[0])) return float_value(as_double(argv[0]));
        if (argv[0].kind == V_STRING) {
            errno = 0; char *end; double n = strtod(argv[0].as.string, &end);
            if (errno == 0 && isfinite(n) && end != argv[0].as.string &&
                (size_t)(end - argv[0].as.string) == argv[0].string_length)
                return float_value(n);
        }
        runtime_value_error(rt, site, "value cannot be converted to Float"); return null_value();
    }
    if (strcmp(name, "to_int") == 0) {
        if (argc != 1) { runtime_type_error(rt, site, "to_int expects one argument"); return null_value(); }
        if (argv[0].kind == V_INT) return argv[0];
        if (argv[0].kind == V_FLOAT && isfinite(argv[0].as.number) &&
            argv[0].as.number >= (double)INT64_MIN && argv[0].as.number <= (double)INT64_MAX)
            return int_value((int64_t)argv[0].as.number);
        if (argv[0].kind == V_STRING) {
            errno = 0; char *end; long long number = strtoll(argv[0].as.string, &end, 10);
            if (errno == 0 && end != argv[0].as.string &&
                (size_t)(end - argv[0].as.string) == argv[0].string_length)
                return int_value((int64_t)number);
        }
        runtime_value_error(rt, site, "value cannot be converted to Int"); return null_value();
    }
    runtime_check_error(rt, site, "unknown function");
    return null_value();
}

static bool effect_dispatch(Runtime *rt, const HhyCallableContract *contract) {
    if (rt->dry_run)
        fprintf(stderr, "plan: %s effect=%s lazy=%s cancel=%s\n", contract->name,
                hhy_effect_name(contract->effect), contract->lazy ? "true" : "false",
                contract->cancellable ? "true" : "false");
    return !(rt->dry_run && contract->action &&
             (contract->effect == HHY_EFFECT_FILESYSTEM ||
              contract->effect == HHY_EFFECT_PROCESS ||
              contract->effect == HHY_EFFECT_NETWORK));
}

static Value call_value_impl(Runtime *rt, Env *env, const HhyNode *site, Value callee,
                             size_t argc, Value *argv) {
    if (callee.kind != V_FUNCTION) { runtime_type_error(rt, site, "value is not callable"); return null_value(); }
    if (callee.as.function.builtin != NULL) {
        const HhyCallableContract *contract = hhy_contract_lookup(callee.as.function.builtin);
        if (contract == NULL) {
            runtime_check_error(rt, site, "callable is missing from the internal registry");
            return null_value();
        }
        if (argc < contract->minimum_arity || argc > contract->maximum_arity) {
            runtime_type_error(rt, site, "wrong number of builtin arguments");
            return null_value();
        }
        const HhyCallableContract *previous = rt->current_contract;
        bool previous_effect_allowed = rt->effect_allowed;
        rt->current_contract = contract;
        rt->effect_allowed = effect_dispatch(rt, contract);
        Value result;
        if (hhy_extension_owns_callable(callee.as.function.builtin)) {
            if (!rt->effect_allowed && contract->action) result = null_value();
            else {
                json_t *arguments = json_array();
                for (size_t i = 0; i < argc && !rt->failed; i++) {
                    json_t *argument = value_to_protocol_json(rt, site, argv[i]);
                    if (argument == NULL || json_array_append_new(arguments, argument) != 0) {
                        if (argument != NULL) json_decref(argument);
                        if (!rt->failed) runtime_value_error(rt, site, "cannot encode extension argument");
                    }
                }
                if (rt->failed) result = null_value();
                else {
                    HhyExtensionError extension_error;
                    json_t *response = hhy_extension_call(callee.as.function.builtin,
                                                          arguments, &extension_error);
                    if (response == NULL) {
                        runtime_extension_error(rt, site, &extension_error);
                        result = null_value();
                    } else {
                        result = protocol_json_to_value(rt, site, response);
                        json_decref(response);
                    }
                }
                json_decref(arguments);
            }
        } else result = builtin(rt, env, site, callee.as.function.builtin, argc, argv);
        rt->current_contract = previous;
        rt->effect_allowed = previous_effect_allowed;
        return result;
    }
    if (rt->call_depth >= rt->limits.max_recursion) {
        runtime_error_kind(rt, site, "ResourceLimitError", "HHY_RECURSION_LIMIT",
                           "maximum call depth exceeded"); return null_value();
    }
    rt->call_depth++;
    Value result = callee.as.function.is_closure
        ? call_closure(rt, site, callee, argc, argv)
        : call_function(rt, site, callee, argc, argv);
    rt->call_depth--;
    return result;
}

static Value call_value(Runtime *rt, Env *env, const HhyNode *site, Value callee,
                        size_t argc, Value *argv) {
    const char *name = callee.kind == V_FUNCTION ? callee.as.function.builtin : "<call>";
    size_t name_length = name == NULL ? 0 : strlen(name);
    const HhySource *source = rt->source;
    uint32_t line = site->token.line, column = site->token.column;
    if (callee.kind == V_FUNCTION && name == NULL && callee.as.function.is_bytecode) {
        const BytecodeFunctionTarget *target = (const BytecodeFunctionTarget *)callee.as.function.node;
        BytecodeCursor cursor = {.chunk = target->chunk, .instruction = target->instruction};
        HhyNode function = bytecode_site(cursor);
        source = callee.as.function.source == NULL ? rt->source : callee.as.function.source;
        if (callee.as.function.is_closure) { name = "<closure>"; name_length = 9; }
        else if (function.child_count > 0) {
            HhyNode identifier = bytecode_site(bytecode_child_cursor(cursor, 0));
            name = identifier.token.start; name_length = identifier.token.length;
            line = function.token.line; column = function.token.column;
        }
    } else if (callee.kind == V_FUNCTION && name == NULL && callee.as.function.node != NULL) {
        const HhyNode *function = callee.as.function.node;
        source = callee.as.function.source == NULL ? rt->source : callee.as.function.source;
        if (callee.as.function.is_closure) { name = "<closure>"; name_length = 9; }
        else if (function->child_count > 0) {
            name = function->children[0]->token.start;
            name_length = function->children[0]->token.length;
            line = function->token.line; column = function->token.column;
        }
    }
    if (name == NULL) { name = "<call>"; name_length = 6; }
    const char *path = source == NULL || source->path == NULL ? "<runtime>" : source->path;
    bool pushed = rt->call_stack_count < HHY_RUNTIME_STACK_TRACE_LIMIT;
    if (pushed) rt->call_stack[rt->call_stack_count++] = (RuntimeStackFrame){
        .name = name, .name_length = name_length, .path = path,
        .line = line, .column = column
    };
    size_t previous = rt->profiler == NULL ? SIZE_MAX :
        hhy_profiler_enter_n(rt->profiler, name, name_length, path, line, column);
    Value result = call_value_impl(rt, env, site, callee, argc, argv);
    hhy_profiler_leave(rt->profiler, previous);
    if (pushed) rt->call_stack_count--;
    return result;
}

static BytecodeFunctionTarget *bytecode_function_target(Runtime *rt, BytecodeCursor cursor) {
    BytecodeFunctionTarget *target = rt_alloc(rt, sizeof(*target));
    target->chunk = cursor.chunk;
    target->instruction = cursor.instruction;
    HhyInstruction instruction = cursor.chunk->code[cursor.instruction];
    if (instruction.opcode == HHY_OP_CLOSURE && instruction.child_count == 2) {
        BytecodeCursor parameter = bytecode_child_cursor(cursor, 0);
        BytecodeCursor body = bytecode_child_cursor(cursor, 1);
        if (parameter.chunk->code[parameter.instruction].opcode == HHY_OP_IDENTIFIER &&
            body.chunk->code[body.instruction].opcode == HHY_OP_EXPR_STMT) {
            target->fast_expression = bytecode_child_cursor(body, 0).instruction;
            target->parameter_constant = parameter.chunk->code[parameter.instruction].constant;
            target->has_fast_argument_expression = true;
            target->stream_kernel = hhy_bytecode_stream_kernel(cursor.chunk,
                                                               cursor.instruction);
        }
    }
    return target;
}

static bool bytecode_eval_argument_expression(Runtime *rt, BytecodeCursor node,
                                              uint32_t parameter_constant, Value argument,
                                              Value *result) {
    HhyInstruction instruction = node.chunk->code[node.instruction];
    switch (instruction.opcode) {
        case HHY_OP_IDENTIFIER:
            if (instruction.constant != parameter_constant) return false;
            *result = argument; return true;
        case HHY_OP_LITERAL: {
            if (instruction.token_kind == HHY_T_INT && instruction.token_length > 0 &&
                instruction.token_length <= 18) {
                const char *text = node.chunk->constants[instruction.constant];
                int64_t integer = 0; bool digits = true;
                for (uint32_t i = 0; i < instruction.token_length; i++) {
                    if (text[i] < '0' || text[i] > '9') { digits = false; break; }
                    integer = integer * 10 + (text[i] - '0');
                }
                if (digits) { *result = int_value(integer); return true; }
            }
            HhyNode site = bytecode_site(node);
            *result = literal(rt, &site); return true;
        }
        case HHY_OP_UNARY: {
            HhyNode site = bytecode_site(node);
            Value value;
            if (!bytecode_eval_argument_expression(rt, bytecode_child_cursor(node, 0),
                                                   parameter_constant, argument, &value)) return false;
            if (site.token.kind == HHY_T_NOT) {
                bool boolean; if (!require_bool(rt, &site, value, &boolean)) { *result = null_value(); return true; }
                *result = bool_value(!boolean); return true;
            }
            if (!numeric(value)) { runtime_type_error(rt, &site, "unary numeric operator requires number"); *result = null_value(); return true; }
            if (site.token.kind == HHY_T_MINUS) {
                if (value.kind == V_INT) {
                    if (value.as.integer == INT64_MIN) { runtime_error_kind(rt, &site, "ValueError", "HHY_INT_OVERFLOW", "Int negation overflow"); *result = null_value(); return true; }
                    *result = int_value(-value.as.integer); return true;
                }
                *result = float_value(-value.as.number); return true;
            }
            *result = value; return true;
        }
        case HHY_OP_BINARY: {
            Value left, right;
            if (!bytecode_eval_argument_expression(rt, bytecode_child_cursor(node, 0),
                                                   parameter_constant, argument, &left)) return false;
            if (instruction.token_kind == HHY_T_AND && left.kind == V_BOOL && !left.as.boolean) { *result = left; return true; }
            if (instruction.token_kind == HHY_T_OR && left.kind == V_BOOL && left.as.boolean) { *result = left; return true; }
            if (!bytecode_eval_argument_expression(rt, bytecode_child_cursor(node, 1),
                                                   parameter_constant, argument, &right)) return false;
            if (left.kind == V_INT && right.kind == V_INT) {
                int64_t integer;
                if (instruction.token_kind == HHY_T_STAR &&
                    !__builtin_mul_overflow(left.as.integer, right.as.integer, &integer)) {
                    *result = int_value(integer); return true;
                }
                if (instruction.token_kind == HHY_T_PLUS &&
                    !__builtin_add_overflow(left.as.integer, right.as.integer, &integer)) {
                    *result = int_value(integer); return true;
                }
                if (instruction.token_kind == HHY_T_MINUS &&
                    !__builtin_sub_overflow(left.as.integer, right.as.integer, &integer)) {
                    *result = int_value(integer); return true;
                }
                if (instruction.token_kind == HHY_T_MOD && right.as.integer != 0 &&
                    !(left.as.integer == INT64_MIN && right.as.integer == -1)) {
                    *result = int_value(left.as.integer % right.as.integer); return true;
                }
                if (instruction.token_kind == HHY_T_EQUAL_EQUAL) {
                    *result = bool_value(left.as.integer == right.as.integer); return true;
                }
                if (instruction.token_kind == HHY_T_BANG_EQUAL) {
                    *result = bool_value(left.as.integer != right.as.integer); return true;
                }
            }
            HhyNode site = bytecode_site(node);
            *result = binary_value(rt, &site, left, right); return true;
        }
        default: return false;
    }
}

static Value bytecode_eval_call(Runtime *rt, Env *env, BytecodeCursor node, Value *injected) {
    HhyNode site = bytecode_site(node);
    Value callee = bytecode_eval(rt, env, bytecode_child_cursor(node, 0));
    size_t explicit_count = site.child_count - 1;
    size_t argc = explicit_count + (injected != NULL ? 1 : 0);
    Value *args = argc == 0 ? NULL : rt_alloc(rt, argc * sizeof(Value));
    size_t offset = 0;
    if (injected != NULL) args[offset++] = *injected;
    for (size_t i = 0; i < explicit_count && !rt->failed; i++)
        args[offset + i] = bytecode_eval(rt, env, bytecode_child_cursor(node, (uint32_t)i + 1));
    return rt->failed ? null_value() : call_value(rt, env, &site, callee, argc, args);
}

static Value bytecode_eval(Runtime *rt, Env *env, BytecodeCursor node) {
    if (rt->failed) return null_value();
    HhyNode site = bytecode_site(node);
    switch (node.chunk->code[node.instruction].opcode) {
        case HHY_OP_LITERAL: return literal(rt, &site);
        case HHY_OP_IDENTIFIER: {
            Binding *binding = env_find_node(env, &site);
            if (binding != NULL) return binding->value;
            char *name = token_text(rt, site.token);
            if (strcmp(name, "processes") == 0) return builtin(rt, env, &site, "processes", 0, NULL);
            if (hhy_contract_lookup(name) == NULL) {
                runtime_check_error(rt, &site, "use of undeclared name"); return null_value();
            }
            Value value = {.kind = V_FUNCTION}; value.as.function.builtin = name; return value;
        }
        case HHY_OP_LIST: {
            Value value = list_new(rt, site.child_count);
            for (size_t i = 0; i < site.child_count; i++)
                value.as.list.items[i] = bytecode_eval(rt, env, bytecode_child_cursor(node, (uint32_t)i));
            return value;
        }
        case HHY_OP_MAP: {
            Value value = {.kind = V_MAP}; value.as.map = map_storage_new(rt, site.child_count);
            value.as.map->keys = site.child_count ? rt_alloc(rt, site.child_count * sizeof(char *)) : NULL;
            value.as.map->key_lengths = site.child_count ? rt_alloc(rt, site.child_count * sizeof(size_t)) : NULL;
            value.as.map->values = site.child_count ? rt_alloc(rt, site.child_count * sizeof(Value)) : NULL;
            for (size_t i = 0; i < site.child_count; i++) {
                BytecodeCursor entry_cursor = bytecode_child_cursor(node, (uint32_t)i);
                HhyNode entry = bytecode_site(entry_cursor); HhyToken key = entry.token;
                if (key.kind == HHY_T_STRING) {
                    Value decoded = decode_string(rt, key); value.as.map->keys[i] = decoded.as.string;
                    value.as.map->key_lengths[i] = decoded.string_length;
                } else {
                    value.as.map->keys[i] = token_text(rt, key); value.as.map->key_lengths[i] = key.length;
                }
                for (size_t previous = 0; previous < i; previous++) {
                    if (value.as.map->key_lengths[previous] == value.as.map->key_lengths[i] &&
                        memcmp(value.as.map->keys[previous], value.as.map->keys[i], value.as.map->key_lengths[i]) == 0) {
                        runtime_value_error(rt, &entry, "duplicate Map key"); return null_value();
                    }
                }
                value.as.map->values[i] = bytecode_eval(rt, env, bytecode_child_cursor(entry_cursor, 0));
            }
            map_build_index(rt, value.as.map); return value;
        }
        case HHY_OP_MEMBER: {
            Value object = bytecode_eval(rt, env, bytecode_child_cursor(node, 0));
            char *key = token_text(rt, site.token);
            if (object.kind == V_PATH) {
                const char *path_text = object.as.string; const char *name = strrchr(path_text, '/');
                name = name == NULL ? path_text : name + 1;
                if (strcmp(key, "name") == 0) return string_value(rt, name);
                if (strcmp(key, "extension") == 0) {
                    const char *extension = strrchr(name, '.');
                    if (extension == NULL || extension == name) extension = "";
                    return string_value(rt, extension);
                }
                if (strcmp(key, "parent") == 0) {
                    const char *slash = strrchr(path_text, '/');
                    if (slash == NULL) return path_value_normalized(rt, ".");
                    if (slash == path_text) return path_value_normalized(rt, "/");
                    char *parent = hhy_strndup(path_text, (size_t)(slash - path_text));
                    Value result = path_value_normalized(rt, parent); free(parent); return result;
                }
                runtime_error_kind(rt, &site, "KeyError", "HHY_PATH_MEMBER", "unknown Path member");
                return null_value();
            }
            if (!record_kind(object.kind)) {
                runtime_type_error(rt, &site, "member access expects Map or system object"); return null_value();
            }
            return map_get(object, key);
        }
        case HHY_OP_INDEX: {
            Value object = bytecode_eval(rt, env, bytecode_child_cursor(node, 0));
            Value index = bytecode_eval(rt, env, bytecode_child_cursor(node, 1));
            if (object.kind == V_LIST && index.kind == V_INT) {
                if (index.as.integer < 0 || (size_t)index.as.integer >= object.as.list.count) {
                    runtime_index_error(rt, &site, "list index out of bounds"); return null_value();
                }
                return object.as.list.items[index.as.integer];
            }
            if (object.kind == V_STRING && index.kind == V_INT) {
                if (index.as.integer < 0) { runtime_index_error(rt, &site, "String index out of bounds"); return null_value(); }
                size_t byte = 0, codepoint = 0;
                while (byte < object.string_length && codepoint < (size_t)index.as.integer) {
                    unsigned char lead = (unsigned char)object.as.string[byte];
                    byte += lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4; codepoint++;
                }
                if (byte >= object.string_length) { runtime_index_error(rt, &site, "String index out of bounds"); return null_value(); }
                unsigned char lead = (unsigned char)object.as.string[byte];
                size_t width = lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
                return string_n(rt, object.as.string + byte, width);
            }
            if (record_kind(object.kind) && index.kind == V_STRING)
                return map_get_n(object, index.as.string, index.string_length);
            runtime_type_error(rt, &site, "invalid index operation"); return null_value();
        }
        case HHY_OP_UNARY: {
            Value value = bytecode_eval(rt, env, bytecode_child_cursor(node, 0));
            if (site.token.kind == HHY_T_NOT) { bool b; if (!require_bool(rt,&site,value,&b)) return null_value(); return bool_value(!b); }
            if (!numeric(value)) { runtime_type_error(rt, &site, "unary numeric operator requires number"); return null_value(); }
            if (site.token.kind == HHY_T_MINUS) {
                if (value.kind == V_INT) {
                    if (value.as.integer == INT64_MIN) { runtime_error_kind(rt, &site, "ValueError", "HHY_INT_OVERFLOW", "Int negation overflow"); return null_value(); }
                    return int_value(-value.as.integer);
                }
                return float_value(-value.as.number);
            }
            return value;
        }
        case HHY_OP_BINARY: {
            Value left = bytecode_eval(rt, env, bytecode_child_cursor(node, 0));
            if (site.token.kind == HHY_T_AND && left.kind == V_BOOL && !left.as.boolean) return left;
            if (site.token.kind == HHY_T_OR && left.kind == V_BOOL && left.as.boolean) return left;
            Value right = bytecode_eval(rt, env, bytecode_child_cursor(node, 1));
            return binary_value(rt, &site, left, right);
        }
        case HHY_OP_RANGE: {
            Value start = bytecode_eval(rt, env, bytecode_child_cursor(node, 0));
            Value end = bytecode_eval(rt, env, bytecode_child_cursor(node, 1));
            if (start.kind != V_INT || end.kind != V_INT) { runtime_type_error(rt, &site, "Range bounds must be Int"); return null_value(); }
            if (end.as.integer < start.as.integer) { runtime_value_error(rt, &site, "descending Range is not supported in v1.0"); return null_value(); }
            Value value = {.kind = V_RANGE}; value.as.range.start = start.as.integer; value.as.range.end = end.as.integer; return value;
        }
        case HHY_OP_CALL: return bytecode_eval_call(rt, env, node, NULL);
        case HHY_OP_CLOSURE: {
            Value value = {.kind = V_FUNCTION};
            value.as.function.node = (const HhyNode *)bytecode_function_target(rt, node);
            value.as.function.is_bytecode = true; value.as.function.is_closure = true;
            env_mark_escaped(env); value.as.function.closure = env; value.as.function.source = rt->source;
            return value;
        }
        case HHY_OP_PIPE: {
            BytecodeCursor stage = bytecode_child_cursor(node, 1);
            HhyNode stage_site = bytecode_site(stage);
            bool catches_error = stage.chunk->code[stage.instruction].opcode == HHY_OP_CALL && stage_site.child_count >= 2;
            if (catches_error) {
                BytecodeCursor callee = bytecode_child_cursor(stage, 0); HhyNode callee_site = bytecode_site(callee);
                catches_error = callee.chunk->code[callee.instruction].opcode == HHY_OP_IDENTIFIER &&
                    callee_site.token.length == 8 && memcmp(callee_site.token.start, "on_error", 8) == 0;
            }
            Value left = bytecode_eval(rt, env, bytecode_child_cursor(node, 0));
            if (catches_error && rt->failed && !rt->cancelled) {
                Value error = rt->error_value; rt->failed = false; rt->exit_code = 0;
                Value handler = bytecode_eval(rt, env, bytecode_child_cursor(stage, stage_site.child_count - 1));
                return rt->failed ? null_value() : call_value(rt, env, &stage_site, handler, 1, &error);
            }
            if (catches_error && !rt->failed && left.kind != V_STREAM) return left;
            if (stage.chunk->code[stage.instruction].opcode == HHY_OP_CALL)
                return bytecode_eval_call(rt, env, stage, &left);
            Value callee = bytecode_eval(rt, env, stage); return call_value(rt, env, &stage_site, callee, 1, &left);
        }
        case HHY_OP_ASSIGN: {
            BytecodeCursor target_cursor = bytecode_child_cursor(node, 0); HhyNode target = bytecode_site(target_cursor);
            if (target_cursor.chunk->code[target_cursor.instruction].opcode != HHY_OP_IDENTIFIER) {
                runtime_check_error(rt, &site, "assignment target must be a variable"); return null_value();
            }
            Binding *binding = env_find_node(env, &target);
            if (binding == NULL) { runtime_check_error(rt, &site, "assignment to undeclared variable"); return null_value(); }
            if (!binding->mutable) { runtime_check_error(rt, &site, "cannot assign to immutable binding"); return null_value(); }
            binding->value = bytecode_eval(rt, env, bytecode_child_cursor(node, 1)); return binding->value;
        }
        case HHY_OP_ATTEMPT: {
            bool outer_failed = rt->failed; Value outer_error = rt->error_value; int outer_exit = rt->exit_code;
            rt->failed = false; rt->exit_code = 0;
            Value result = bytecode_exec(rt, env, bytecode_child_cursor(node, 0));
            bool failed = rt->failed; Value error = rt->error_value;
            rt->failed = outer_failed; rt->error_value = outer_error; rt->exit_code = outer_exit;
            Value map = {.kind = V_RESULT}; map.as.map = map_storage_new(rt, 3);
            map.as.map->keys = rt_alloc(rt, 3 * sizeof(char *)); map.as.map->values = rt_alloc(rt, 3 * sizeof(Value));
            map.as.map->key_lengths = rt_alloc(rt, 3 * sizeof(size_t));
            map.as.map->keys[0] = rt_strndup(rt,"ok",2); map.as.map->values[0] = bool_value(!failed);
            map.as.map->keys[1] = rt_strndup(rt,"value",5); map.as.map->values[1] = failed ? null_value() : result;
            map.as.map->keys[2] = rt_strndup(rt,"error",5); map.as.map->values[2] = failed ? error : null_value();
            map.as.map->key_lengths[0] = 2; map.as.map->key_lengths[1] = 5; map.as.map->key_lengths[2] = 5;
            return map;
        }
        default: return bytecode_exec(rt, env, node);
    }
}

static Value bytecode_exec_contents(Runtime *rt, Env *env, BytecodeCursor node) {
    HhyNode site = bytecode_site(node); Value result = null_value();
    for (size_t i = 0; i < site.child_count && !rt->failed && rt->signal == SIGNAL_NONE; i++)
        result = bytecode_exec(rt, env, bytecode_child_cursor(node, (uint32_t)i));
    return result;
}

static Value bytecode_import_module(Runtime *rt, Env *target, BytecodeCursor node) {
    HhyNode site = bytecode_site(node);
    if (site.child_count == 0) { runtime_check_error(rt, &site, "import requires a module"); return null_value(); }
    BytecodeCursor first_cursor = bytecode_child_cursor(node, 0);
    HhyNode first = bytecode_site(first_cursor);
    bool has_path = false; BytecodeCursor path_cursor = first_cursor;
    for (size_t i = 0; i < site.child_count; i++) {
        BytecodeCursor child_cursor = bytecode_child_cursor(node, (uint32_t)i);
        HhyNode child = bytecode_site(child_cursor);
        if (child.token.kind == HHY_T_STRING) { has_path = true; path_cursor = child_cursor; break; }
    }
    if (!has_path) {
        char *standard_name = token_text(rt, first.token);
        if (site.child_count == 1 && env_find(target, standard_name) == NULL) {
            const char *extension_error = NULL;
            if (hhy_extension_prepare_namespace(standard_name, strlen(standard_name), &extension_error)) {
                size_t count = 0, prefix_length = strlen(standard_name);
                for (size_t i = 0; i < hhy_contract_count(); i++) {
                    const HhyCallableContract *contract = hhy_contract_at(i);
                    if (strncmp(contract->name, standard_name, prefix_length) == 0 &&
                        contract->name[prefix_length] == '.') count++;
                }
                const char **keys = count ? rt_alloc(rt, count * sizeof(char *)) : NULL;
                Value *functions = count ? rt_alloc(rt, count * sizeof(Value)) : NULL;
                size_t index = 0;
                for (size_t i = 0; i < hhy_contract_count(); i++) {
                    const HhyCallableContract *contract = hhy_contract_at(i);
                    if (strncmp(contract->name, standard_name, prefix_length) != 0 ||
                        contract->name[prefix_length] != '.') continue;
                    keys[index] = contract->name + prefix_length + 1;
                    functions[index] = (Value){.kind = V_FUNCTION};
                    functions[index].as.function.builtin = contract->name; index++;
                }
                env_define(rt, target, &site, standard_name,
                           map_with_entries(rt, V_MAP, count, keys, functions), false);
            }
        }
        if (site.child_count != 1 || env_find(target, standard_name) == NULL)
            runtime_error_kind(rt, &site, "ModuleNotFoundError", "HHY_MODULE_NOT_FOUND", "unknown standard module");
        return null_value();
    }
    HhyNode path_node = bytecode_site(path_cursor);
    Value decoded_path = decode_string(rt, path_node.token);
    if (rt->failed) return null_value();
    if (string_has_nul(decoded_path)) { runtime_value_error(rt, &site, "module path cannot contain U+0000"); return null_value(); }
    Module *module = module_load(rt, &site, decoded_path.as.string);
    if (module == NULL) return null_value();
    if (first.token.kind == HHY_T_STRING) {
        if (site.child_count >= 3) {
            HhyNode marker = bytecode_site(bytecode_child_cursor(node, 1));
            if (marker.token.kind == HHY_T_AS) {
                HhyNode alias_node = bytecode_site(bytecode_child_cursor(node, 2));
                char *alias = token_text(rt, alias_node.token);
                env_define(rt, target, &site, alias, module->exports, false);
            }
        }
        return module->exports;
    }
    size_t close_brace = 0;
    for (size_t i = 0; i < site.child_count; i++) {
        HhyNode child = bytecode_site(bytecode_child_cursor(node, (uint32_t)i));
        if (child.token.kind == HHY_T_RBRACE) { close_brace = i; break; }
    }
    for (size_t i = 1; i < close_brace; i++) {
        HhyNode imported_node = bytecode_site(bytecode_child_cursor(node, (uint32_t)i));
        if (imported_node.token.kind != HHY_T_IDENTIFIER) continue;
        char *source_name = token_text(rt, imported_node.token); char *local_name = source_name;
        if (i + 2 < close_brace) {
            HhyNode marker = bytecode_site(bytecode_child_cursor(node, (uint32_t)i + 1));
            if (marker.token.kind == HHY_T_AS) {
                HhyNode alias_node = bytecode_site(bytecode_child_cursor(node, (uint32_t)i + 2));
                local_name = token_text(rt, alias_node.token); i += 2;
            }
        }
        Value imported;
        if (!map_lookup_n(module->exports, source_name, strlen(source_name), &imported)) {
            runtime_error_kind(rt, &site, "KeyError", "HHY_MODULE_EXPORT", "module does not export requested name");
            return null_value();
        }
        env_define(rt, target, &site, local_name, imported, false);
    }
    return module->exports;
}

static Value bytecode_exec(Runtime *rt, Env *env, BytecodeCursor node) {
    if (rt->failed) return null_value();
    HhyNode site = bytecode_site(node);
    switch (node.chunk->code[node.instruction].opcode) {
        case HHY_OP_PROGRAM: return bytecode_exec_contents(rt, env, node);
        case HHY_OP_BLOCK:
            return bytecode_exec_contents(rt, env_new_with_capacity(rt, env, site.frame_slot_count), node);
        case HHY_OP_EXPR_STMT: return bytecode_eval(rt, env, bytecode_child_cursor(node, 0));
        case HHY_OP_LET_DECL: {
            bool mutable = site.child_count == 3; size_t name_index = mutable ? 1 : 0;
            HhyNode name = bytecode_site(bytecode_child_cursor(node, (uint32_t)name_index));
            Value value = bytecode_eval(rt, env, bytecode_child_cursor(node, (uint32_t)name_index + 1));
            env_define_token(rt, env, &site, name.token, value, mutable); return value;
        }
        case HHY_OP_FN_DECL: {
            Value value = {.kind = V_FUNCTION};
            value.as.function.node = (const HhyNode *)bytecode_function_target(rt, node);
            value.as.function.is_bytecode = true; env_mark_escaped(env);
            value.as.function.closure = env; value.as.function.source = rt->source;
            HhyNode name = bytecode_site(bytecode_child_cursor(node, 0));
            env_define_token(rt, env, &site, name.token, value, false); return value;
        }
        case HHY_OP_IF: {
            Value condition = bytecode_eval(rt, env, bytecode_child_cursor(node, 0)); bool yes;
            if (!require_bool(rt, &site, condition, &yes)) return null_value();
            if (yes) return bytecode_exec(rt, env, bytecode_child_cursor(node, 1));
            if (site.child_count > 2) return bytecode_exec(rt, env, bytecode_child_cursor(node, 2));
            return null_value();
        }
        case HHY_OP_WHILE: {
            Value result = null_value();
            for (;;) {
                if (runtime_safepoint(rt, &site)) break;
                Value condition = bytecode_eval(rt, env, bytecode_child_cursor(node, 0)); bool yes;
                if (!require_bool(rt,&site,condition,&yes) || !yes || rt->failed) break;
                result = bytecode_exec(rt, env, bytecode_child_cursor(node, 1));
                if (rt->signal == SIGNAL_BREAK) { rt->signal = SIGNAL_NONE; break; }
                if (rt->signal == SIGNAL_CONTINUE) rt->signal = SIGNAL_NONE;
                if (rt->signal == SIGNAL_RETURN) break;
            }
            return result;
        }
        case HHY_OP_FOR: {
            Value iterable = bytecode_eval(rt, env, bytecode_child_cursor(node, 1));
            if (iterable.kind == V_RANGE) iterable = stream_value(rt, STREAM_RANGE, iterable, null_value(), env);
            Value result = null_value(); HhyNode name = bytecode_site(bytecode_child_cursor(node, 0));
            size_t index = 0; Value item;
            if (iterable.kind == V_STREAM && !stream_claim(rt, &site, iterable)) return null_value();
            if (iterable.kind != V_LIST && iterable.kind != V_STREAM) { runtime_type_error(rt,&site,"for expects List or Stream"); return null_value(); }
            for (;;) {
                if (runtime_safepoint(rt, &site)) break;
                if (iterable.kind == V_LIST) { if (index >= iterable.as.list.count) break; item = iterable.as.list.items[index++]; }
                else if (!stream_next(rt, &site, iterable.as.stream, &item)) break;
                Env *iteration = env_new_with_capacity(rt, env, 1);
                env_define_token(rt, iteration, &site, name.token, item, false);
                result = bytecode_exec(rt, iteration, bytecode_child_cursor(node, 2));
                if (rt->signal == SIGNAL_BREAK) { rt->signal = SIGNAL_NONE; break; }
                if (rt->signal == SIGNAL_CONTINUE) rt->signal = SIGNAL_NONE;
                if (rt->signal == SIGNAL_RETURN || rt->failed) break;
            }
            return result;
        }
        case HHY_OP_RETURN:
            rt->signal_value = site.child_count ? bytecode_eval(rt, env, bytecode_child_cursor(node, 0)) : null_value();
            rt->signal = SIGNAL_RETURN; return rt->signal_value;
        case HHY_OP_BREAK: rt->signal = SIGNAL_BREAK; return null_value();
        case HHY_OP_CONTINUE: rt->signal = SIGNAL_CONTINUE; return null_value();
        case HHY_OP_TRY: {
            Value result = bytecode_exec(rt, env, bytecode_child_cursor(node, 0));
            if (!rt->failed) return result;
            Value error = rt->error_value; rt->failed = false; rt->exit_code = 0;
            Env *catch_env = env_new_with_capacity(rt, env, 1);
            HhyNode name = bytecode_site(bytecode_child_cursor(node, 1));
            env_define_token(rt, catch_env, &site, name.token, error, false);
            return bytecode_exec(rt, catch_env, bytecode_child_cursor(node, 2));
        }
        case HHY_OP_IMPORT_DECL:
            return bytecode_import_module(rt, env, node);
        case HHY_OP_EXPORT_DECL:
            if (site.child_count != 1) { runtime_check_error(rt, &site, "invalid export declaration"); return null_value(); }
            return bytecode_exec(rt, env, bytecode_child_cursor(node, 0));
        default: return bytecode_eval(rt, env, node);
    }
}

static Value bytecode_exec_active_program(Runtime *rt, Env *env) {
    BytecodeCursor root = {.chunk = hhy_active_bytecode_chunk, .instruction = 0};
    return bytecode_exec(rt, env, root);
}

static Value eval_call(Runtime *rt, Env *env, const HhyNode *node, Value *injected) {
    Value callee = eval(rt, env, node->children[0]);
    size_t explicit_count = node->child_count - 1;
    size_t argc = explicit_count + (injected != NULL ? 1 : 0);
    /* Calls can allocate before every argument has been consumed. Keep argument
       arrays in GC-managed memory: optimized native builds are not required to
       retain every pointer-bearing Value in a conservatively scanned stack slot. */
    Value *args = argc == 0 ? NULL : rt_alloc(rt, argc * sizeof(Value));
    size_t offset = 0;
    if (injected != NULL) args[offset++] = *injected;
    for (size_t i = 0; i < explicit_count && !rt->failed; i++) args[offset + i] = eval(rt, env, node->children[i + 1]);
    return rt->failed ? null_value() : call_value(rt, env, node, callee, argc, args);
}

static Value eval(Runtime *rt, Env *env, const HhyNode *node) {
    if (rt->failed) return null_value();
    switch (node->kind) {
        case HHY_N_LITERAL: return literal(rt, node);
        case HHY_N_IDENTIFIER: {
            Binding *binding = env_find_node(env, node);
            if (binding != NULL) return binding->value;
            char *name = token_text(rt, node->token);
            if (strcmp(name, "processes") == 0)
                return builtin(rt, env, node, "processes", 0, NULL);
            if (hhy_contract_lookup(name) == NULL) {
                runtime_check_error(rt, node, "use of undeclared name");
                return null_value();
            }
            Value value = {.kind = V_FUNCTION}; value.as.function.builtin = name; return value;
        }
        case HHY_N_LIST: {
            Value value = list_new(rt, node->child_count);
            for (size_t i = 0; i < node->child_count; i++) value.as.list.items[i] = eval(rt, env, node->children[i]);
            return value;
        }
        case HHY_N_MAP: {
            Value value = {.kind = V_MAP}; value.as.map = map_storage_new(rt, node->child_count);
            value.as.map->keys = node->child_count ? rt_alloc(rt, node->child_count * sizeof(char *)) : NULL;
            value.as.map->key_lengths = node->child_count ? rt_alloc(rt, node->child_count * sizeof(size_t)) : NULL;
            value.as.map->values = node->child_count ? rt_alloc(rt, node->child_count * sizeof(Value)) : NULL;
            for (size_t i = 0; i < node->child_count; i++) {
                const HhyNode *entry = node->children[i]; HhyToken key = entry->token;
                if (key.kind == HHY_T_STRING) {
                    Value decoded = decode_string(rt, key); value.as.map->keys[i] = decoded.as.string;
                    value.as.map->key_lengths[i] = decoded.string_length;
                } else {
                    value.as.map->keys[i] = token_text(rt, key);
                    value.as.map->key_lengths[i] = key.length;
                }
                for (size_t previous = 0; previous < i; previous++) {
                    if (value.as.map->key_lengths[previous] == value.as.map->key_lengths[i] &&
                        memcmp(value.as.map->keys[previous], value.as.map->keys[i],
                               value.as.map->key_lengths[i]) == 0) {
                        runtime_value_error(rt, entry, "duplicate Map key");
                        return null_value();
                    }
                }
                value.as.map->values[i] = eval(rt, env, entry->children[0]);
            }
            map_build_index(rt, value.as.map);
            return value;
        }
        case HHY_N_MEMBER: {
            Value object = eval(rt, env, node->children[0]); char *key = token_text(rt, node->token);
            if (object.kind == V_PATH) {
                const char *path_text = object.as.string;
                const char *name = strrchr(path_text, '/');
                name = name == NULL ? path_text : name + 1;
                if (strcmp(key, "name") == 0) return string_value(rt, name);
                if (strcmp(key, "extension") == 0) {
                    const char *extension = strrchr(name, '.');
                    if (extension == NULL || extension == name) extension = "";
                    return string_value(rt, extension);
                }
                if (strcmp(key, "parent") == 0) {
                    const char *slash = strrchr(path_text, '/');
                    if (slash == NULL) return path_value_normalized(rt, ".");
                    if (slash == path_text) return path_value_normalized(rt, "/");
                    char *parent = hhy_strndup(path_text, (size_t)(slash - path_text));
                    Value result = path_value_normalized(rt, parent); free(parent); return result;
                }
                runtime_error_kind(rt, node, "KeyError", "HHY_PATH_MEMBER",
                                   "unknown Path member");
                return null_value();
            }
            if (!record_kind(object.kind)) {
                runtime_type_error(rt, node, "member access expects Map or system object"); return null_value();
            }
            return map_get(object, key);
        }
        case HHY_N_INDEX: {
            Value object = eval(rt, env, node->children[0]); Value index = eval(rt, env, node->children[1]);
            if (object.kind == V_LIST && index.kind == V_INT) {
                if (index.as.integer < 0 || (size_t)index.as.integer >= object.as.list.count) { runtime_index_error(rt, node, "list index out of bounds"); return null_value(); }
                return object.as.list.items[index.as.integer];
            }
            if (object.kind == V_STRING && index.kind == V_INT) {
                if (index.as.integer < 0) {
                    runtime_index_error(rt, node, "String index out of bounds"); return null_value();
                }
                size_t byte = 0, codepoint = 0;
                while (byte < object.string_length && codepoint < (size_t)index.as.integer) {
                    unsigned char lead = (unsigned char)object.as.string[byte];
                    byte += lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
                    codepoint++;
                }
                if (byte >= object.string_length) {
                    runtime_index_error(rt, node, "String index out of bounds"); return null_value();
                }
                unsigned char lead = (unsigned char)object.as.string[byte];
                size_t width = lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
                return string_n(rt, object.as.string + byte, width);
            }
            if (record_kind(object.kind) && index.kind == V_STRING)
                return map_get_n(object, index.as.string, index.string_length);
            runtime_type_error(rt, node, "invalid index operation"); return null_value();
        }
        case HHY_N_UNARY: {
            Value value = eval(rt, env, node->children[0]);
            if (node->token.kind == HHY_T_NOT) { bool b; if (!require_bool(rt,node,value,&b)) return null_value(); return bool_value(!b); }
            if (!numeric(value)) { runtime_type_error(rt, node, "unary numeric operator requires number"); return null_value(); }
            if (node->token.kind == HHY_T_MINUS) {
                if (value.kind == V_INT) {
                    if (value.as.integer == INT64_MIN) {
                        runtime_error_kind(rt, node, "ValueError", "HHY_INT_OVERFLOW",
                                           "Int negation overflow");
                        return null_value();
                    }
                    return int_value(-value.as.integer);
                }
                return float_value(-value.as.number);
            }
            return value;
        }
        case HHY_N_BINARY: {
            Value left = eval(rt, env, node->children[0]);
            if (node->token.kind == HHY_T_AND && left.kind == V_BOOL && !left.as.boolean) return left;
            if (node->token.kind == HHY_T_OR && left.kind == V_BOOL && left.as.boolean) return left;
            Value right = eval(rt, env, node->children[1]); return binary_value(rt, node, left, right);
        }
        case HHY_N_RANGE: {
            Value start = eval(rt, env, node->children[0]);
            Value end = eval(rt, env, node->children[1]);
            if (start.kind != V_INT || end.kind != V_INT) {
                runtime_type_error(rt, node, "Range bounds must be Int"); return null_value();
            }
            if (end.as.integer < start.as.integer) {
                runtime_value_error(rt, node, "descending Range is not supported in v1.0"); return null_value();
            }
            Value value = {.kind = V_RANGE};
            value.as.range.start = start.as.integer;
            value.as.range.end = end.as.integer;
            return value;
        }
        case HHY_N_CALL: return eval_call(rt, env, node, NULL);
        case HHY_N_CLOSURE: {
            Value value = {.kind = V_FUNCTION}; value.as.function.node = node;
            env_mark_escaped(env);
            value.as.function.closure = env; value.as.function.source = rt->source;
            value.as.function.is_closure = true; return value;
        }
        case HHY_N_PIPE: {
            const HhyNode *stage = node->children[1];
            bool catches_error = stage->kind == HHY_N_CALL && stage->child_count >= 2 &&
                stage->children[0]->kind == HHY_N_IDENTIFIER &&
                stage->children[0]->token.length == 8 &&
                memcmp(stage->children[0]->token.start, "on_error", 8) == 0;
            Value left = eval(rt, env, node->children[0]);
            if (catches_error && rt->failed && !rt->cancelled) {
                Value error = rt->error_value;
                rt->failed = false;
                rt->exit_code = 0;
                Value handler = eval(rt, env, stage->children[stage->child_count - 1]);
                return rt->failed ? null_value() :
                    call_value(rt, env, stage, handler, 1, &error);
            }
            if (catches_error && !rt->failed && left.kind != V_STREAM) return left;
            if (stage->kind == HHY_N_CALL) return eval_call(rt, env, stage, &left);
            Value callee = eval(rt, env, stage); return call_value(rt, env, stage, callee, 1, &left);
        }
        case HHY_N_ASSIGN: {
            const HhyNode *target = node->children[0];
            if (target->kind != HHY_N_IDENTIFIER) { runtime_check_error(rt, node, "assignment target must be a variable"); return null_value(); }
            Binding *binding = env_find_node(env, target);
            if (binding == NULL) { runtime_check_error(rt, node, "assignment to undeclared variable"); return null_value(); }
            if (!binding->mutable) { runtime_check_error(rt, node, "cannot assign to immutable binding"); return null_value(); }
            binding->value = eval(rt, env, node->children[1]); return binding->value;
        }
        case HHY_N_ATTEMPT: {
            bool outer_failed = rt->failed; Value outer_error = rt->error_value;
            int outer_exit = rt->exit_code; rt->failed = false; rt->exit_code = 0;
            Value result = exec_node(rt, env, node->children[0]); bool failed = rt->failed; Value error = rt->error_value;
            rt->failed = outer_failed; rt->error_value = outer_error; rt->exit_code = outer_exit;
            Value map = {.kind = V_RESULT}; map.as.map = map_storage_new(rt, 3);
            map.as.map->keys = rt_alloc(rt, 3 * sizeof(char *)); map.as.map->values = rt_alloc(rt, 3 * sizeof(Value));
            map.as.map->key_lengths = rt_alloc(rt, 3 * sizeof(size_t));
            map.as.map->keys[0] = rt_strndup(rt,"ok",2); map.as.map->values[0] = bool_value(!failed);
            map.as.map->keys[1] = rt_strndup(rt,"value",5); map.as.map->values[1] = failed ? null_value() : result;
            map.as.map->keys[2] = rt_strndup(rt,"error",5); map.as.map->values[2] = failed ? error : null_value();
            map.as.map->key_lengths[0] = 2; map.as.map->key_lengths[1] = 5; map.as.map->key_lengths[2] = 5;
            return map;
        }
        default: return exec_node(rt, env, node);
    }
}

static Value exec_block_contents(Runtime *rt, Env *env, const HhyNode *node) {
    Value result = null_value();
    for (size_t i = 0; i < node->child_count && !rt->failed && rt->signal == SIGNAL_NONE; i++)
        result = exec_node(rt, env, node->children[i]);
    return result;
}

static Value exec_block(Runtime *rt, Env *parent, const HhyNode *node) {
    return exec_block_contents(rt, env_new_with_capacity(rt, parent, node->frame_slot_count), node);
}

static Value exec_node(Runtime *rt, Env *env, const HhyNode *node) {
    if (rt->failed) return null_value();
    switch (node->kind) {
        case HHY_N_PROGRAM: {
            Value result = null_value();
            for (size_t i = 0; i < node->child_count && !rt->failed && rt->signal == SIGNAL_NONE; i++)
                result = exec_node(rt, env, node->children[i]);
            return result;
        }
        case HHY_N_BLOCK: return exec_block(rt, env, node);
        case HHY_N_EXPR_STMT: return eval(rt, env, node->children[0]);
        case HHY_N_LET_DECL: {
            bool mutable = node->child_count == 3; size_t name_index = mutable ? 1 : 0;
            Value value = eval(rt, env, node->children[name_index + 1]);
            env_define_token(rt, env, node, node->children[name_index]->token,
                             value, mutable); return value;
        }
        case HHY_N_FN_DECL: {
            Value value = {.kind = V_FUNCTION}; value.as.function.node = node;
            env_mark_escaped(env);
            value.as.function.closure = env; value.as.function.source = rt->source;
            env_define_token(rt, env, node, node->children[0]->token, value, false); return value;
        }
        case HHY_N_IF: {
            Value condition = eval(rt, env, node->children[0]); bool yes;
            if (!require_bool(rt,node,condition,&yes)) return null_value();
            if (yes) return exec_node(rt, env, node->children[1]);
            if (node->child_count > 2) return exec_node(rt, env, node->children[2]);
            return null_value();
        }
        case HHY_N_WHILE: {
            Value result = null_value();
            for (;;) {
                if (runtime_safepoint(rt, node)) break;
                Value condition = eval(rt, env, node->children[0]); bool yes;
                if (!require_bool(rt,node,condition,&yes) || !yes || rt->failed) break;
                result = exec_node(rt, env, node->children[1]);
                if (rt->signal == SIGNAL_BREAK) { rt->signal = SIGNAL_NONE; break; }
                if (rt->signal == SIGNAL_CONTINUE) rt->signal = SIGNAL_NONE;
                if (rt->signal == SIGNAL_RETURN) break;
            }
            return result;
        }
        case HHY_N_FOR: {
            Value iterable = eval(rt, env, node->children[1]);
            if (iterable.kind == V_RANGE)
                iterable = stream_value(rt, STREAM_RANGE, iterable, null_value(), env);
            Value result = null_value(); HhyToken name = node->children[0]->token;
            size_t index = 0;
            Value item;
            if (iterable.kind == V_STREAM && !stream_claim(rt, node, iterable)) return null_value();
            if (iterable.kind != V_LIST && iterable.kind != V_STREAM) {
                runtime_type_error(rt,node,"for expects List or Stream"); return null_value();
            }
            for (;;) {
                if (runtime_safepoint(rt, node)) break;
                if (iterable.kind == V_LIST) {
                    if (index >= iterable.as.list.count) break;
                    item = iterable.as.list.items[index++];
                } else if (!stream_next(rt, node, iterable.as.stream, &item)) {
                    break;
                }
                Env *iteration = env_new_with_capacity(rt, env, 1);
                env_define_token(rt, iteration, node, name, item, false);
                result = exec_node(rt, iteration, node->children[2]);
                if (rt->signal == SIGNAL_BREAK) { rt->signal = SIGNAL_NONE; break; }
                if (rt->signal == SIGNAL_CONTINUE) rt->signal = SIGNAL_NONE;
                if (rt->signal == SIGNAL_RETURN || rt->failed) break;
            }
            return result;
        }
        case HHY_N_RETURN:
            rt->signal_value = node->child_count ? eval(rt, env, node->children[0]) : null_value();
            rt->signal = SIGNAL_RETURN; return rt->signal_value;
        case HHY_N_BREAK: rt->signal = SIGNAL_BREAK; return null_value();
        case HHY_N_CONTINUE: rt->signal = SIGNAL_CONTINUE; return null_value();
        case HHY_N_TRY: {
            Value result = exec_node(rt, env, node->children[0]);
            if (!rt->failed) return result;
            Value error = rt->error_value; rt->failed = false; rt->exit_code = 0;
            Env *catch_env = env_new_with_capacity(rt, env, 1);
            env_define_token(rt, catch_env, node, node->children[1]->token, error, false);
            return exec_node(rt, catch_env, node->children[2]);
        }
        case HHY_N_IMPORT_DECL:
            return import_module(rt, env, node);
        case HHY_N_EXPORT_DECL:
            if (node->child_count != 1) {
                runtime_check_error(rt, node, "invalid export declaration"); return null_value();
            }
            return exec_node(rt, env, node->children[0]);
        default: return eval(rt, env, node);
    }
}

static Module *module_find(Runtime *rt, const char *path) {
    for (Module *module = rt->modules; module != NULL; module = module->next)
        if (strcmp(module->path, path) == 0) return module;
    return NULL;
}

static char *module_resolve_path(Runtime *rt, const HhyNode *site, const char *requested) {
    char candidate[PATH_MAX];
    if (requested[0] == '/') {
        if (snprintf(candidate, sizeof(candidate), "%s", requested) >= (int)sizeof(candidate)) {
            runtime_value_error(rt, site, "module path is too long"); return NULL;
        }
    } else {
        const char *current_path = rt->source->path;
        const char *slash = strrchr(current_path, '/');
        size_t directory_length = slash == NULL ? 0 : (size_t)(slash - current_path);
        if (directory_length == 0) {
            if (snprintf(candidate, sizeof(candidate), "%s", requested) >= (int)sizeof(candidate)) {
                runtime_value_error(rt, site, "module path is too long"); return NULL;
            }
        } else {
            if (snprintf(candidate, sizeof(candidate), "%.*s/%s", (int)directory_length,
                         current_path, requested) >= (int)sizeof(candidate)) {
                runtime_value_error(rt, site, "module path is too long"); return NULL;
            }
        }
    }
    char resolved[PATH_MAX];
    if (realpath(candidate, resolved) == NULL) {
        runtime_error_kind(rt, site, "ModuleNotFoundError", "HHY_MODULE_NOT_FOUND",
                           "module file was not found"); return NULL;
    }
    return rt_strndup(rt, resolved, strlen(resolved));
}

static Module *module_load(Runtime *rt, const HhyNode *site, const char *requested) {
    char *resolved = module_resolve_path(rt, site, requested);
    if (resolved == NULL) return NULL;
    Module *cached = module_find(rt, resolved);
    if (cached != NULL) {
        if (cached->loading) runtime_check_error(rt, site, "circular module import");
        return rt->failed ? NULL : cached;
    }
    Module *module = rt_alloc(rt, sizeof(*module));
    module->path = resolved;
    module->source.path = module->path;
    if (!hhy_source_load(module->path, &module->source)) {
        runtime_io_error(rt, site, "cannot read module"); return NULL;
    }
    module->source.path = module->path;
    module->loading = true;
    module->next = rt->modules;
    rt->modules = module;
    if (!hhy_lex(&module->source, &module->tokens)) {
        rt->failed = true; rt->exit_code = 2; return NULL;
    }
    HhyParseResult parsed = hhy_parse(&module->source, &module->tokens, &module->program);
    if (!parsed.ok) { rt->failed = true; rt->exit_code = 2; return NULL; }
    hhy_resolve_slots(module->program);
    module->environment = env_new(rt, rt->core);
    const HhySource *previous_source = rt->source;
    rt->source = &module->source;
    if (rt->engine == HHY_ENGINE_BYTECODE) {
        HhyBytecodeResult prepared = hhy_bytecode_runtime_prepare(module->program, NULL,
                                                                   &module->bytecode);
        if (!prepared.ok) {
            runtime_error_kind(rt, site, "BytecodeError", "HHY_BYTECODE_MODULE",
                               prepared.message);
        } else {
            const HhyBytecodeChunk *chunk = hhy_bytecode_runtime_chunk(module->bytecode);
            bytecode_exec(rt, module->environment,
                          (BytecodeCursor){.chunk = chunk, .instruction = 0});
        }
    } else {
        exec_node(rt, module->environment, module->program);
    }
    rt->source = previous_source;
    if (rt->failed) return NULL;
    size_t export_count = 0;
    for (size_t i = 0; i < module->program->child_count; i++)
        if (module->program->children[i]->kind == HHY_N_EXPORT_DECL) export_count++;
    module->exports.kind = V_MAP;
    module->exports.as.map = map_storage_new(rt, export_count);
    module->exports.as.map->count = export_count;
    module->exports.as.map->keys = export_count ? rt_alloc(rt, export_count * sizeof(char *)) : NULL;
    module->exports.as.map->key_lengths = export_count ? rt_alloc(rt, export_count * sizeof(size_t)) : NULL;
    module->exports.as.map->values = export_count ? rt_alloc(rt, export_count * sizeof(Value)) : NULL;
    size_t exported = 0;
    for (size_t i = 0; i < module->program->child_count; i++) {
        const HhyNode *export_node = module->program->children[i];
        if (export_node->kind != HHY_N_EXPORT_DECL || export_node->child_count != 1) continue;
        const HhyNode *declaration = export_node->children[0];
        size_t name_index = declaration->kind == HHY_N_LET_DECL && declaration->child_count == 3 ? 1 : 0;
        char *name = token_text(rt, declaration->children[name_index]->token);
        Binding *binding = env_local(module->environment, name);
        if (binding == NULL) { runtime_check_error(rt, export_node, "exported binding was not created"); return NULL; }
        module->exports.as.map->keys[exported] = name;
        module->exports.as.map->key_lengths[exported] = declaration->children[name_index]->token.length;
        module->exports.as.map->values[exported] = binding->value;
        exported++;
    }
    module->loading = false;
    return module;
}

static Value import_module(Runtime *rt, Env *target, const HhyNode *node) {
    if (node->child_count == 0) {
        runtime_check_error(rt, node, "import requires a module"); return null_value();
    }
    const HhyNode *path_node = NULL;
    for (size_t i = 0; i < node->child_count; i++)
        if (node->children[i]->token.kind == HHY_T_STRING) path_node = node->children[i];
    if (path_node == NULL) {
        char *standard_name = token_text(rt, node->children[0]->token);
        if (node->child_count == 1 && env_find(target, standard_name) == NULL) {
            const char *extension_error = NULL;
            if (hhy_extension_prepare_namespace(standard_name, strlen(standard_name),
                                                &extension_error)) {
                size_t count = 0, prefix_length = strlen(standard_name);
                for (size_t i = 0; i < hhy_contract_count(); i++) {
                    const HhyCallableContract *contract = hhy_contract_at(i);
                    if (strncmp(contract->name, standard_name, prefix_length) == 0 &&
                        contract->name[prefix_length] == '.') count++;
                }
                const char **keys = count ? rt_alloc(rt, count * sizeof(char *)) : NULL;
                Value *functions = count ? rt_alloc(rt, count * sizeof(Value)) : NULL;
                size_t index = 0;
                for (size_t i = 0; i < hhy_contract_count(); i++) {
                    const HhyCallableContract *contract = hhy_contract_at(i);
                    if (strncmp(contract->name, standard_name, prefix_length) != 0 ||
                        contract->name[prefix_length] != '.') continue;
                    keys[index] = contract->name + prefix_length + 1;
                    functions[index] = (Value){.kind = V_FUNCTION};
                    functions[index].as.function.builtin = contract->name; index++;
                }
                env_define(rt, target, node, standard_name,
                           map_with_entries(rt, V_MAP, count, keys, functions), false);
            }
        }
        if (node->child_count != 1 || env_find(target, standard_name) == NULL)
            runtime_error_kind(rt, node, "ModuleNotFoundError", "HHY_MODULE_NOT_FOUND",
                               "unknown standard module");
        return null_value();
    }
    Value decoded_path = decode_string(rt, path_node->token);
    if (rt->failed) return null_value();
    if (string_has_nul(decoded_path)) {
        runtime_value_error(rt, node, "module path cannot contain U+0000");
        return null_value();
    }
    Module *module = module_load(rt, node, decoded_path.as.string);
    if (module == NULL) return null_value();
    if (node->children[0]->token.kind == HHY_T_STRING) {
        if (node->child_count >= 3 && node->children[1]->token.kind == HHY_T_AS) {
            char *alias = token_text(rt, node->children[2]->token);
            env_define(rt, target, node, alias, module->exports, false);
        }
        return module->exports;
    }
    size_t close_brace = 0;
    for (size_t i = 0; i < node->child_count; i++)
        if (node->children[i]->token.kind == HHY_T_RBRACE) { close_brace = i; break; }
    for (size_t i = 1; i < close_brace; i++) {
        if (node->children[i]->token.kind != HHY_T_IDENTIFIER) continue;
        char *source_name = token_text(rt, node->children[i]->token);
        char *local_name = source_name;
        if (i + 2 < close_brace && node->children[i + 1]->token.kind == HHY_T_AS) {
            local_name = token_text(rt, node->children[i + 2]->token);
            i += 2;
        }
        Value imported;
        if (!map_lookup_n(module->exports, source_name, strlen(source_name), &imported)) {
            runtime_error_kind(rt, node, "KeyError", "HHY_MODULE_EXPORT",
                               "module does not export requested name"); return null_value();
        }
        env_define(rt, target, node, local_name, imported, false);
    }
    return module->exports;
}

static Env *runtime_core_environment(Runtime *rt, const HhyNode *site, int argc, char **argv) {
    Env *global = env_new(rt, NULL);
    rt->core = global;
    Value args = list_new(rt, (size_t)argc);
    for (int i = 0; i < argc; i++) {
        size_t codepoints = 0, length = strlen(argv[i]);
        if (!utf8_count(argv[i], length, &codepoints)) {
            runtime_error_kind(rt, site, "EncodingError", "HHY_ARG_UTF8",
                               "script argument is not valid UTF-8");
            return global;
        }
        args.as.list.items[i] = string_n(rt, argv[i], length);
    }
    env_define(rt, global, site, "args", args, false);
    const char *http_keys[] = {"get", "post", "put", "delete"};
    const char *http_names[] = {"http.get", "http.post", "http.put", "http.delete"};
    Value http_functions[4];
    for (size_t i = 0; i < 4; i++) {
        http_functions[i].kind = V_FUNCTION;
        http_functions[i].as.function.builtin = http_names[i];
    }
    env_define(rt, global, site, "http",
               map_with_entries(rt, V_MAP, 4, http_keys, http_functions), false);
    const char *datetime_keys[] = {"parse"};
    Value datetime_functions[1] = {{.kind = V_FUNCTION}};
    datetime_functions[0].as.function.builtin = "datetime.parse";
    env_define(rt, global, site, "datetime",
               map_with_entries(rt, V_MAP, 1, datetime_keys, datetime_functions), false);
    size_t environment_count = 0;
    while (environ[environment_count] != NULL) environment_count++;
    Value environment = {.kind = V_MAP}; environment.as.map = map_storage_new(rt, environment_count);
    environment.as.map->keys = environment_count ? rt_alloc(rt, environment_count * sizeof(char *)) : NULL;
    environment.as.map->key_lengths = environment_count ? rt_alloc(rt, environment_count * sizeof(size_t)) : NULL;
    environment.as.map->values = environment_count ? rt_alloc(rt, environment_count * sizeof(Value)) : NULL;
    for (size_t i = 0; i < environment_count; i++) {
        const char *separator = strchr(environ[i], '=');
        size_t key_length = separator == NULL ? strlen(environ[i]) : (size_t)(separator - environ[i]);
        const char *environment_value = separator == NULL ? "" : separator + 1;
        size_t value_length = strlen(environment_value), codepoints = 0;
        if (!utf8_count(environ[i], key_length, &codepoints) ||
            !utf8_count(environment_value, value_length, &codepoints)) {
            runtime_error_kind(rt, site, "EncodingError", "HHY_ENV_UTF8",
                               "environment variable is not valid UTF-8");
            return global;
        }
        environment.as.map->keys[i] = rt_strndup(rt, environ[i], key_length);
        environment.as.map->key_lengths[i] = key_length;
        environment.as.map->values[i] = string_n(rt, environment_value, value_length);
    }
    env_define(rt, global, site, "env", environment, false);
    struct utsname system_name;
    if (uname(&system_name) != 0) {
        strcpy(system_name.sysname, "unknown");
        strcpy(system_name.machine, "unknown");
    }
    char *working_directory = getcwd(NULL, 0);
    int64_t cpu_count = 0;
    uint64_t memory_bytes = 0;
#ifdef __APPLE__
    size_t cpu_size = sizeof(cpu_count), memory_size = sizeof(memory_bytes);
    if (sysctlbyname("hw.logicalcpu", &cpu_count, &cpu_size, NULL, 0) != 0) cpu_count = 0;
    if (sysctlbyname("hw.memsize", &memory_bytes, &memory_size, NULL, 0) != 0) memory_bytes = 0;
#else
#ifdef _SC_NPROCESSORS_ONLN
    cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
#endif
#if defined(_SC_PHYS_PAGES) && defined(_SC_PAGESIZE)
    long pages = sysconf(_SC_PHYS_PAGES), page_size = sysconf(_SC_PAGESIZE);
    if (pages > 0 && page_size > 0) memory_bytes = (uint64_t)pages * (uint64_t)page_size;
#endif
#endif
    Value memory = {.kind = V_BYTES}; memory.as.number = (double)memory_bytes;
    const char *temporary_path = getenv("TMPDIR");
    if (temporary_path == NULL || temporary_path[0] == '\0') temporary_path = "/tmp";
    const char *cwd_text = working_directory == NULL ? "" : working_directory;
    const char *system_texts[] = {cwd_text, temporary_path, system_name.sysname,
                                  system_name.machine, system_name.nodename};
    for (size_t i = 0; i < sizeof(system_texts) / sizeof(system_texts[0]); i++) {
        size_t codepoints = 0;
        if (!utf8_count(system_texts[i], strlen(system_texts[i]), &codepoints)) {
            free(working_directory);
            runtime_error_kind(rt, site, "EncodingError", "HHY_SYSTEM_UTF8",
                               "system metadata is not valid UTF-8");
            return global;
        }
    }
    Value temporary = string_value(rt, temporary_path); temporary.kind = V_PATH;
    const char *system_keys[] = {"cwd", "os", "arch", "hostname", "cpu", "cpu_count",
                                 "memory", "temp"};
    Value cwd = string_value(rt, cwd_text); cwd.kind = V_PATH;
    Value system_values[] = {
        cwd, string_value(rt, system_name.sysname), string_value(rt, system_name.machine),
        string_value(rt, system_name.nodename), int_value(cpu_count > 0 ? cpu_count : 0),
        int_value(cpu_count > 0 ? cpu_count : 0), memory, temporary
    };
    free(working_directory);
    env_define(rt, global, site, "system",
               map_with_entries(rt, V_MAP, 8, system_keys, system_values), false);
    return global;
}

static void runtime_release(HHY_BORROWED Runtime *rt) {
    while (rt->cleanups != NULL) {
        RuntimeCleanup *cleanup = rt->cleanups;
        rt->cleanups = cleanup->next;
        if (cleanup->file != NULL) runtime_fclose(rt, cleanup->file);
        if (cleanup->temporary_path != NULL) unlink(cleanup->temporary_path);
        free(cleanup->temporary_path);
        free(cleanup);
    }
    for (Stream *stream = rt->streams; stream != NULL; stream = stream->runtime_next)
        stream_close(stream);
    for (Module *module = rt->modules; module != NULL; module = module->next) {
        hhy_bytecode_runtime_free(module->bytecode);
        hhy_node_free(module->program);
        hhy_tokens_free(&module->tokens);
        hhy_source_free(&module->source);
    }
    rt->core = NULL;
    rt->modules = NULL;
    rt->streams = NULL;
}

typedef struct ReplChunk ReplChunk;
struct ReplChunk {
    HhySource source;
    HhyTokenList tokens;
    HhyNode *program;
    char *path;
    ReplChunk *next;
};

static bool repl_input_incomplete(const char *text) {
    int depth = 0;
    bool string = false, escaped = false, comment = false;
    char previous = '\0', last = '\0';
    for (const char *p = text; *p; p++) {
        char c = *p;
        if (comment) { if (c == '\n') comment = false; continue; }
        if (!string && c == '#') { comment = true; continue; }
        if (c == '"' && !escaped) { string = !string; continue; }
        if (string) { escaped = c == '\\' && !escaped; if (c != '\\') escaped = false; continue; }
        if (c == '{' || c == '[' || c == '(') depth++;
        else if (c == '}' || c == ']' || c == ')') depth--;
        if (!isspace((unsigned char)c)) { previous = last; last = c; }
    }
    return depth > 0 || (previous == '|' && last == '>');
}

static bool repl_should_print(const HhyNode *program) {
    if (!program->child_count) return false;
    const HhyNode *last = program->children[program->child_count - 1];
    if (last->kind != HHY_N_EXPR_STMT || !last->child_count) return false;
    const HhyNode *expression = last->children[0];
    if (expression->kind == HHY_N_CALL && expression->child_count &&
        expression->children[0]->kind == HHY_N_IDENTIFIER &&
        ((expression->children[0]->token.length == 5 &&
          memcmp(expression->children[0]->token.start, "print", 5) == 0) ||
         (expression->children[0]->token.length == 11 &&
          memcmp(expression->children[0]->token.start, "print_error", 11) == 0))) return false;
    return true;
}

int hhy_repl(void) {
    if (!hhy_contract_registry_valid()) {
        fputs("hhy: invalid internal callable registry\n", stderr);
        return 1;
    }
    GC_INIT();
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        fputs("hhy: cannot initialize HTTP runtime\n", stderr); return 4;
    }
    Runtime rt = {.limits = hhy_runtime_limits_default(), .effect_allowed = true};
    clock_gettime(CLOCK_MONOTONIC, &rt.started_at);
    GC_gcollect();
    rt.memory_baseline = GC_get_memory_use();
    HhyToken site_token = {.start = "repl", .length = 4, .line = 1, .column = 1};
    HhyNode site = {.kind = HHY_N_PROGRAM, .token = site_token};
    Env *session = env_new(&rt, runtime_core_environment(&rt, &site, 0, NULL));
    struct sigaction action = {0}, previous_interrupt;
    action.sa_handler = hhy_signal_handler; sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, &previous_interrupt);
    bool interactive = isatty(STDIN_FILENO);
    if (interactive) fputs("HHY " HHY_VERSION " — Pipe Everything.\n", stdout);
    char *buffer = NULL, *line = NULL;
    size_t capacity = 0, length = 0, line_capacity = 0, chunk_number = 0;
    ReplChunk *chunks = NULL;
    for (;;) {
        if (interactive) { fputs(length ? "... " : ">>> ", stdout); fflush(stdout); }
        ssize_t read = getline(&line, &line_capacity, stdin);
        if (read < 0) break;
        if (length == 0 && (strcmp(line, ":quit\n") == 0 || strcmp(line, ":exit\n") == 0)) break;
        size_t needed = length + (size_t)read + 1;
        if (needed > capacity) {
            capacity = capacity < 256 ? 256 : capacity;
            while (capacity < needed) capacity *= 2;
            buffer = hhy_realloc(buffer, capacity);
        }
        memcpy(buffer + length, line, (size_t)read); length += (size_t)read; buffer[length] = '\0';
        if (repl_input_incomplete(buffer)) continue;
        ReplChunk *chunk = hhy_alloc(sizeof(*chunk));
        char path_buffer[64]; snprintf(path_buffer, sizeof(path_buffer), "<repl:%zu>", ++chunk_number);
        chunk->path = hhy_strndup(path_buffer, strlen(path_buffer));
        chunk->source.path = chunk->path;
        chunk->source.text = hhy_strndup(buffer, length);
        chunk->source.length = length;
        bool ok = hhy_lex(&chunk->source, &chunk->tokens);
        HhyParseResult parsed = {.ok = false};
        if (ok) parsed = hhy_parse(&chunk->source, &chunk->tokens, &chunk->program);
        if (ok && parsed.ok) {
            hhy_resolve_slots(chunk->program);
            rt.source = &chunk->source; rt.failed = false; rt.exit_code = 0; rt.signal = SIGNAL_NONE;
            Value result = exec_node(&rt, session, chunk->program);
            if (rt.failed) {
                fprintf(stderr, "%s:%u:%u: runtime error: ", chunk->source.path,
                        rt.error_line, rt.error_column);
                print_value(stderr, rt.error_value, false); fputc('\n', stderr);
            } else if (repl_should_print(chunk->program)) {
                print_value(stdout, result, false); fputc('\n', stdout);
            }
        }
        chunk->next = chunks; chunks = chunk;
        length = 0; hhy_interrupt_requested = 0;
    }
    free(line); free(buffer);
    while (chunks) {
        ReplChunk *next = chunks->next;
        hhy_node_free(chunks->program); hhy_tokens_free(&chunks->tokens);
        hhy_source_free(&chunks->source); free(chunks->path); free(chunks); chunks = next;
    }
    runtime_release(&rt); curl_global_cleanup(); sigaction(SIGINT, &previous_interrupt, NULL);
    return 0;
}

HhyRunResult hhy_profile_program(const HhySource *source, const HhyNode *program,
                                 int argc, char **argv, bool dry_run,
                                 const HhyRuntimeLimits *limits,
                                 const HhyProfileOptions *profile) {
    if (!hhy_contract_registry_valid()) {
        HhyRunResult failed = {.ok = false, .exit_code = 1};
        fputs("hhy: invalid internal callable registry\n", stderr);
        return failed;
    }
    GC_INIT();
    hhy_resolve_slots((HhyNode *)program);
    /* Runtime owns GC-managed Stream/Module/Frame chains that must remain
       traceable until explicit teardown. Uncollectable Boehm memory is scanned
       like a root while retaining deterministic GC_free ownership here. */
    Runtime *rt = GC_malloc_uncollectable(sizeof(*rt));
    if (rt == NULL) {
        HhyRunResult failed = {.ok = false, .exit_code = 70};
        fputs("hhy: out of memory\n", stderr);
        return failed;
    }
    memset(rt, 0, sizeof(*rt));
    rt->source = source;
    rt->engine = hhy_active_bytecode_chunk == NULL ? HHY_ENGINE_AST : HHY_ENGINE_BYTECODE;
    rt->dry_run = dry_run;
    rt->effect_allowed = true;
    rt->limits = limits == NULL ? hhy_runtime_limits_default() : *limits;
    const char *gc_stress = getenv("HHY_GC_STRESS");
    rt->gc_stress = gc_stress != NULL && strcmp(gc_stress, "0") != 0;
    clock_gettime(CLOCK_MONOTONIC, &rt->started_at);
    GC_gcollect();
    rt->memory_baseline = GC_get_memory_use();
    if (profile != NULL) {
        rt->profiler = hhy_profiler_start(profile, source->path, rt->memory_baseline);
        if (rt->profiler == NULL) {
            HhyRunResult failed = {.ok = false, .exit_code = 4};
            fputs("hhy: cannot initialize profiler\n", stderr);
            GC_free(rt);
            return failed;
        }
    }
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        HhyRunResult failed = {.ok = false, .exit_code = 4};
        fputs("hhy: cannot initialize HTTP runtime\n", stderr);
        hhy_profiler_free(rt->profiler); GC_free(rt);
        return failed;
    }
    hhy_interrupt_requested = 0;
    struct sigaction action = {0}, previous_interrupt;
    action.sa_handler = hhy_signal_handler;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, &previous_interrupt);
    rt->memory_jump_ready = true;
    if (setjmp(rt->memory_jump) == 0) {
        const char *top_level = profile != NULL && profile->engine != NULL &&
            strcmp(profile->engine, "bytecode") == 0
            ? "<bytecode-top-level>" : "<top-level>";
        size_t profile_previous = hhy_profiler_enter(rt->profiler, top_level,
                                                     source->path, 1, 1);
        Env *global = runtime_core_environment(rt, program, argc, argv);
        Env *main_environment = env_new(rt, global);
        if (rt->engine == HHY_ENGINE_BYTECODE)
            bytecode_exec_active_program(rt, main_environment);
        else
            exec_node(rt, main_environment, program);
        if (rt->signal == SIGNAL_BREAK || rt->signal == SIGNAL_CONTINUE)
            runtime_check_error(rt, program, "loop control used outside a loop");
        if (rt->signal == SIGNAL_RETURN) rt->signal = SIGNAL_NONE;
        hhy_profiler_leave(rt->profiler, profile_previous);
    }
    rt->memory_jump_ready = false;
    if (rt->failed) {
        fprintf(stderr, "%s:%u:%u: runtime error: ", source->path,
                rt->error_line, rt->error_column);
        print_value(stderr, rt->error_value, false);
        fputc('\n', stderr);
    }
    HhyRunResult result = {.ok = !rt->failed,
        .exit_code = rt->failed ? (rt->exit_code ? rt->exit_code : 1) : rt->exit_code};
    if (rt->profiler != NULL) {
        GC_gcollect();
        hhy_profiler_stop(rt->profiler, GC_get_memory_use());
        hhy_profiler_free(rt->profiler);
        rt->profiler = NULL;
    }
    runtime_release(rt);
    curl_global_cleanup();
    sigaction(SIGINT, &previous_interrupt, NULL);
    GC_free(rt);
    return result;
}

HhyRunResult hhy_run_program(const HhySource *source, const HhyNode *program,
                             int argc, char **argv, bool dry_run,
                             const HhyRuntimeLimits *limits) {
    return hhy_run_program_engine(source, program, argc, argv, dry_run, limits,
                                  HHY_ENGINE_BYTECODE);
}

HhyRunResult hhy_profile_program_engine(const HhySource *source, const HhyNode *program,
                                        int argc, char **argv, bool dry_run,
                                        const HhyRuntimeLimits *limits,
                                        const HhyProfileOptions *profile,
                                        HhyExecutionEngine engine) {
    if (engine == HHY_ENGINE_AST) {
        HhyProfileOptions selected = profile == NULL ? (HhyProfileOptions){0} : *profile;
        selected.engine = "ast";
        return hhy_profile_program(source, program, argc, argv, dry_run, limits,
                                   profile == NULL ? NULL : &selected);
    }
    if (engine != HHY_ENGINE_BYTECODE) {
        fputs("hhy: unknown execution engine\n", stderr);
        return (HhyRunResult){.ok = false, .exit_code = 3};
    }
    hhy_resolve_slots((HhyNode *)program);
    HhyPreparedBytecode *bytecode = NULL;
    HhyBytecodeResult prepared = hhy_bytecode_runtime_prepare(
        program, getenv("HHY_TEST_BYTECODE_FAULT"), &bytecode);
    if (!prepared.ok) {
        fprintf(stderr, "%s:%u:%u: bytecode runtime error at instruction %zu: %s\n",
                source->path, program->token.line, program->token.column,
                prepared.instruction, prepared.message);
        hhy_bytecode_runtime_free(bytecode);
        return (HhyRunResult){.ok = false, .exit_code = 2};
    }
    HhyProfileOptions selected = profile == NULL ? (HhyProfileOptions){0} : *profile;
    selected.engine = "bytecode";
    hhy_active_bytecode_chunk = hhy_bytecode_runtime_chunk(bytecode);
    HhyRunResult run = hhy_profile_program(source, program, argc, argv, dry_run,
                                           limits, profile == NULL ? NULL : &selected);
    hhy_active_bytecode_chunk = NULL;
    hhy_bytecode_runtime_free(bytecode);
    return run;
}

HhyRunResult hhy_run_program_engine(const HhySource *source, const HhyNode *program,
                                    int argc, char **argv, bool dry_run,
                                    const HhyRuntimeLimits *limits,
                                    HhyExecutionEngine engine) {
    return hhy_profile_program_engine(source, program, argc, argv, dry_run, limits,
                                      NULL, engine);
}

void hhy_fuzz_runtime_input(const uint8_t *data, size_t size, unsigned mode) {
    GC_INIT();
    if (size > 1024 * 1024) return;
    HhySource source = {.path = "<fuzz-runtime>", .text = "", .length = 0};
    HhyToken token = {.start = "fuzz", .length = 4, .line = 1, .column = 1};
    HhyNode site = {.kind = HHY_N_PROGRAM, .token = token};
    Runtime rt = {.source = &source, .limits = hhy_runtime_limits_default()};
    rt.limits.max_regex_steps = 10000;
    rt.memory_baseline = GC_get_memory_use();
    clock_gettime(CLOCK_MONOTONIC, &rt.started_at);
    char *text = rt_alloc_atomic(&rt, size + 1);
    if (size > 0) memcpy(text, data, size);
    text[size] = '\0';
    if (mode == 0) {
        (void)parse_json_text(&rt, &site, text, size);
    } else if (mode == 1) {
        size_t codepoints = 0;
        if (!utf8_count(text, size, &codepoints)) {
            runtime_release(&rt);
            return;
        }
        Value lines = string_lines(&rt, text, size);
        Value record;
        size_t count = 0;
        while (!rt.failed && count++ < 4096 &&
               (record = csv_read_record(&rt, &site, lines.as.stream, ',', '"')).kind != V_NULL) {}
        stream_close(lines.as.stream);
    } else {
        size_t split = size / 2;
        char *pattern = rt_strndup(&rt, text, split);
        for (size_t i = 0; i < split; i++) if (pattern[i] == '\0') pattern[i] = 'x';
        size_t codepoints = 0;
        if (!utf8_count(text + split, size - split, &codepoints)) {
            runtime_release(&rt);
            return;
        }
        Value arguments[2] = {string_n(&rt, text + split, size - split),
            {.kind = V_REGEX, .as.regex = {.pattern = pattern, .flags = PCRE2_UTF | PCRE2_UCP}}};
        (void)builtin(&rt, NULL, &site, "regex_captures", 2, arguments);
    }
    runtime_release(&rt);
}
