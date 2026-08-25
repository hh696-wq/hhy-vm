#include "hhy/contracts.h"

#include <stdint.h>
#include <string.h>

#define C(name, min, max, effect, lazy, cancel, send, action, input, output, threading) \
    {name, min, max, effect, lazy, cancel, send, action, input, output, threading}
#define ONE SIZE_MAX

static const HhyCallableContract contracts[] = {
    C("print", 0, ONE, HHY_EFFECT_CUSTOM, false, true, true, true,
      "Value...", "Null", "main"),
    C("print_error", 0, ONE, HHY_EFFECT_CUSTOM, false, true, true, true,
      "Value...", "Null", "main"),
    C("exit", 0, 1, HHY_EFFECT_CUSTOM, false, true, true, true,
      "Int?", "Never", "main"),
    C("length", 1, 1, HHY_EFFECT_NONE, false, false, true, false,
      "String | List | Map", "Int", "main"),
    C("type", 1, 1, HHY_EFFECT_NONE, false, false, true, false,
      "Value", "String", "main"),
    C("stream", 1, 1, HHY_EFFECT_NONE, true, true, false, false,
      "List<T> | Map | Range", "Stream<T>", "main"),
    C("collect", 1, 1, HHY_EFFECT_NONE, false, true, true, false,
      "Stream<T>", "List<T>", "main"),
    C("count", 1, 1, HHY_EFFECT_NONE, false, true, true, false,
      "Stream<T>", "Int", "main"),
    C("take", 2, 2, HHY_EFFECT_NONE, true, true, false, false,
      "Stream<T>, Int", "Stream<T>", "main"),
    C("map", 2, 2, HHY_EFFECT_NONE, true, true, false, false,
      "Stream<T>, Function(T -> U)", "Stream<U>", "main"),
    C("where", 2, 2, HHY_EFFECT_NONE, true, true, false, false,
      "Stream<T>, Function(T -> Bool)", "Stream<T>", "main"),
    C("for_each", 2, 2, HHY_EFFECT_CUSTOM, false, true, true, true,
      "Stream<T>, Function(T -> Value)", "Null", "main"),
    C("sum", 1, 1, HHY_EFFECT_NONE, false, true, true, false,
      "Stream<Number>", "Number", "main"),
    C("reduce", 3, 3, HHY_EFFECT_NONE, false, true, true, false,
      "Stream<T>, U, Function(State<T,U> -> U)", "U", "main"),
    C("contains", 2, 2, HHY_EFFECT_NONE, false, false, true, false,
      "String | List, Value", "Bool", "main"),
    C("upper", 1, 1, HHY_EFFECT_NONE, false, false, true, false,
      "String", "String", "main"),
    C("lower", 1, 1, HHY_EFFECT_NONE, false, false, true, false,
      "String", "String", "main"),
    C("get", 2, 2, HHY_EFFECT_NONE, false, false, true, false,
      "List | Map | Record, Int | String", "Value | Null", "main"),
    C("path", 1, 1, HHY_EFFECT_NONE, false, false, true, false,
      "String", "Path", "main"),
    C("encode_json", 1, 2, HHY_EFFECT_NONE, false, true, true, false,
      "JsonValue, Map?", "String", "main"),
    C("to_float", 1, 1, HHY_EFFECT_NONE, false, false, true, false,
      "Int | Float | String", "Float", "main"),
    C("throw", 1, 1, HHY_EFFECT_CUSTOM, false, true, true, true,
      "Error", "Never", "main"),
    C("read_text", 1, 1, HHY_EFFECT_FILESYSTEM, false, true, true, false,
      "Path", "String", "main"),
    C("read_lines", 1, 1, HHY_EFFECT_FILESYSTEM, true, true, false, false,
      "Path", "Stream<String>", "main"),
    C("read_bytes", 1, 1, HHY_EFFECT_FILESYSTEM, false, true, true, false,
      "Path", "BytesBuffer", "main"),
    C("write_text", 2, 3, HHY_EFFECT_FILESYSTEM, false, true, true, true,
      "Path, String, Map?", "Path", "main"),
    C("append_text", 2, 2, HHY_EFFECT_FILESYSTEM, false, true, true, true,
      "Path, String", "Path", "main"),
    C("save_text", 2, 3, HHY_EFFECT_FILESYSTEM, false, true, true, true,
      "String | Stream<String>, Path, Map?", "Path", "main"),
    C("save_lines", 2, 3, HHY_EFFECT_FILESYSTEM, false, true, true, true,
      "Stream<String>, Path, Map?", "Path", "main"),
    C("trim", 1, 1, HHY_EFFECT_NONE, false, false, true, false,
      "String", "String", "main"),
    C("starts_with", 2, 2, HHY_EFFECT_NONE, false, false, true, false,
      "String, String", "Bool", "main"),
    C("ends_with", 2, 2, HHY_EFFECT_NONE, false, false, true, false,
      "String, String", "Bool", "main"),
    C("replace", 3, 3, HHY_EFFECT_NONE, false, false, true, false,
      "String, String, String", "String", "main"),
    C("split", 2, 2, HHY_EFFECT_NONE, false, false, true, false,
      "String, String", "List<String>", "main"),
    C("join", 2, 2, HHY_EFFECT_NONE, false, false, true, false,
      "List<String>, String", "String", "main"),
    C("parse_json", 1, 1, HHY_EFFECT_NONE, false, true, true, false,
      "String", "JsonValue", "main"),
    C("run", 1, 2, HHY_EFFECT_PROCESS, false, true, true, true,
      "List<String>, Map?", "CommandResult", "isolated_process"),
    C("shell", 1, 2, HHY_EFFECT_PROCESS, false, true, true, true,
      "String, Map?", "CommandResult", "isolated_process"),
    C("stdout_lines", 1, 1, HHY_EFFECT_NONE, true, true, false, false,
      "CommandResult", "Stream<String>", "main"),
    C("timeout", 2, 2, HHY_EFFECT_NONE, false, true, true, false,
      "HttpRequest, Duration", "HttpRequest", "main"),
    C("retry", 2, 2, HHY_EFFECT_NONE, false, true, true, false,
      "HttpRequest, Map", "HttpRequest", "main"),
    C("send", 1, 1, HHY_EFFECT_NETWORK, false, true, true, true,
      "HttpRequest", "HttpResponse", "main"),
    C("response_body", 1, 1, HHY_EFFECT_NONE, false, false, true, false,
      "HttpResponse", "String", "main"),
    C("response_bytes", 1, 1, HHY_EFFECT_NONE, false, false, true, false,
      "HttpResponse", "BytesBuffer", "main"),
    C("write_bytes", 2, 3, HHY_EFFECT_FILESYSTEM, false, true, true, true,
      "Path, BytesBuffer, Map?", "Path", "main"),
    C("files", 2, 3, HHY_EFFECT_FILESYSTEM, true, true, false, false,
      "Path, String, Map?", "Stream<File | Directory>", "main"),
    C("range", 2, 2, HHY_EFFECT_NONE, true, true, false, false,
      "Int, Int", "Stream<Int>", "main"),
    C("flat_map", 2, 2, HHY_EFFECT_NONE, true, true, false, false,
      "Stream<T>, Function(T -> Stream<U>)", "Stream<U>", "main"),
    C("skip", 2, 2, HHY_EFFECT_NONE, true, true, false, false,
      "Stream<T>, Int", "Stream<T>", "main"),
    C("inspect", 2, 2, HHY_EFFECT_CUSTOM, true, true, false, true,
      "Stream<T>, Function(T -> Value)", "Stream<T>", "main"),
    C("distinct", 1, 1, HHY_EFFECT_NONE, true, true, false, false,
      "Stream<Hashable>", "Stream<Hashable>", "main"),
    C("sort_by", 3, 3, HHY_EFFECT_NONE, true, true, false, false,
      "Stream<T>, Map, Function(T -> Comparable)", "Stream<T>", "main"),
    C("group_by", 2, 2, HHY_EFFECT_NONE, true, true, false, false,
      "Stream<T>, Function(T -> Hashable)", "Stream<Group<T>>", "main"),
    C("first", 1, 1, HHY_EFFECT_NONE, false, true, true, false,
      "Stream<T>", "T | Null", "main"),
    C("last", 1, 1, HHY_EFFECT_NONE, false, true, true, false,
      "Stream<T>", "T | Null", "main"),
    C("min", 1, 1, HHY_EFFECT_NONE, false, true, true, false,
      "Stream<Number>", "Number | Null", "main"),
    C("max", 1, 1, HHY_EFFECT_NONE, false, true, true, false,
      "Stream<Number>", "Number | Null", "main"),
    C("any", 2, 2, HHY_EFFECT_NONE, false, true, true, false,
      "Stream<T>, Function(T -> Bool)", "Bool", "main"),
    C("all", 2, 2, HHY_EFFECT_NONE, false, true, true, false,
      "Stream<T>, Function(T -> Bool)", "Bool", "main"),
    C("regex_match", 2, 2, HHY_EFFECT_NONE, false, true, true, false,
      "String, Regex", "Bool", "main"),
    C("regex_captures", 2, 2, HHY_EFFECT_NONE, false, true, true, false,
      "String, Regex", "Map | Null", "main"),
    C("parse_csv", 1, 2, HHY_EFFECT_NONE, true, true, false, false,
      "String | Stream<String>, Map?", "Stream<Map>", "main"),
    C("encode_csv", 1, 2, HHY_EFFECT_NONE, true, true, false, false,
      "Stream<Map>, Map?", "Stream<String>", "main"),
    C("processes", 0, 0, HHY_EFFECT_PROCESS, true, true, false, false,
      "()", "Stream<Process>", "main"),
    C("path_join", 2, 2, HHY_EFFECT_NONE, false, false, true, false,
      "Path, String | Path", "Path", "main"),
    C("is_type", 2, 2, HHY_EFFECT_NONE, false, false, true, false,
      "Value, String", "Bool", "main"),
    C("to_int", 1, 1, HHY_EFFECT_NONE, false, false, true, false,
      "Int | Float | String", "Int", "main"),
    C("byte_length", 1, 1, HHY_EFFECT_NONE, false, false, true, false,
      "String | BytesBuffer", "Int", "main"),
    C("append", 2, 2, HHY_EFFECT_NONE, false, false, true, false,
      "List<T>, T", "List<T>", "main"),
    C("remove_at", 2, 2, HHY_EFFECT_NONE, false, false, true, false,
      "List<T>, Int", "List<T>", "main"),
    C("require", 2, 2, HHY_EFFECT_NONE, false, false, true, false,
      "Map, String", "Value", "main"),
    C("put", 3, 3, HHY_EFFECT_NONE, false, false, true, false,
      "Map, String, Value", "Map", "main"),
    C("remove_key", 2, 2, HHY_EFFECT_NONE, false, false, true, false,
      "Map, String", "Map", "main"),
    C("trim_start", 1, 1, HHY_EFFECT_NONE, false, false, true, false,
      "String", "String", "main"),
    C("trim_end", 1, 1, HHY_EFFECT_NONE, false, false, true, false,
      "String", "String", "main"),
    C("copy", 2, 3, HHY_EFFECT_FILESYSTEM, false, true, true, true,
      "Path, Path, Map?", "Path", "main"),
    C("move", 2, 3, HHY_EFFECT_FILESYSTEM, false, true, true, true,
      "Path, Path, Map?", "Path", "main"),
    C("remove", 1, 1, HHY_EFFECT_FILESYSTEM, false, true, true, true,
      "Path", "Path", "main"),
    C("every", 1, 1, HHY_EFFECT_NONE, true, true, false, false,
      "Duration", "Stream<Int>", "main"),
    C("watch", 1, 2, HHY_EFFECT_FILESYSTEM, true, true, false, false,
      "Path, Map?", "Stream<FileEvent>", "main"),
    C("on_error", 2, 2, HHY_EFFECT_NONE, true, true, false, false,
      "Stream<T>, Function(Error -> Stream<T>)", "Stream<T>", "main"),
    C("parallel", 3, 3, HHY_EFFECT_PROCESS, true, true, false, false,
      "Stream<T>, Int, Function(T -> U)", "Stream<U>", "isolated_process"),
    C("sleep", 1, 1, HHY_EFFECT_NONE, false, true, true, false,
      "Duration", "Null", "main"),
    C("cancel", 0, 0, HHY_EFFECT_CUSTOM, false, true, true, true,
      "()", "Never", "main"),
    C("stdin_lines", 0, 0, HHY_EFFECT_CUSTOM, true, true, false, false,
      "()", "Stream<String>", "main"),
    C("require_env", 1, 1, HHY_EFFECT_NONE, false, false, true, false,
      "String", "String", "main"),
    C("pick", 2, 2, HHY_EFFECT_NONE, false, false, true, false,
      "Map, List<String>", "Map", "main"),
    C("debounce", 2, 2, HHY_EFFECT_NONE, true, true, false, false,
      "Stream<T>, Duration", "Stream<T>", "main"),
    C("now", 0, 0, HHY_EFFECT_NONE, false, false, true, false,
      "()", "DateTime", "main"),
    C("http.get", 1, 2, HHY_EFFECT_NONE, false, false, true, false,
      "String, Map?", "HttpRequest", "main"),
    C("http.post", 1, 2, HHY_EFFECT_NONE, false, false, true, false,
      "String, Map?", "HttpRequest", "main"),
    C("http.put", 1, 2, HHY_EFFECT_NONE, false, false, true, false,
      "String, Map?", "HttpRequest", "main"),
    C("http.delete", 1, 2, HHY_EFFECT_NONE, false, false, true, false,
      "String, Map?", "HttpRequest", "main"),
    C("datetime.parse", 3, 3, HHY_EFFECT_NONE, false, false, true, false,
      "String, String, String", "DateTime", "main")
};

#undef C
#undef ONE

size_t hhy_contract_count(void) { return sizeof(contracts) / sizeof(contracts[0]); }

const HhyCallableContract *hhy_contract_at(size_t index) {
    return index < hhy_contract_count() ? &contracts[index] : NULL;
}

const HhyCallableContract *hhy_contract_lookup_n(const char *name, size_t length) {
    for (size_t i = 0; i < hhy_contract_count(); i++)
        if (strlen(contracts[i].name) == length && memcmp(contracts[i].name, name, length) == 0)
            return &contracts[i];
    return NULL;
}

const HhyCallableContract *hhy_contract_lookup(const char *name) {
    return hhy_contract_lookup_n(name, strlen(name));
}

const char *hhy_effect_name(HhyEffect effect) {
    static const char *names[] = {"none", "filesystem", "process", "network", "custom"};
    return effect <= HHY_EFFECT_CUSTOM ? names[effect] : "unknown";
}

bool hhy_contract_registry_valid(void) {
    for (size_t i = 0; i < hhy_contract_count(); i++) {
        if (contracts[i].name[0] == '\0' || contracts[i].minimum_arity > contracts[i].maximum_arity ||
            contracts[i].input_contract == NULL || contracts[i].output_contract == NULL ||
            contracts[i].threading == NULL || contracts[i].input_contract[0] == '\0' ||
            contracts[i].output_contract[0] == '\0' ||
            (strcmp(contracts[i].threading, "main") != 0 &&
             strcmp(contracts[i].threading, "worker") != 0 &&
             strcmp(contracts[i].threading, "isolated_process") != 0) ||
            strcmp(contracts[i].input_contract, "runtime-checked") == 0)
            return false;
        for (size_t j = i + 1; j < hhy_contract_count(); j++)
            if (strcmp(contracts[i].name, contracts[j].name) == 0) return false;
    }
    return true;
}

bool hhy_contract_namespace_installed(const char *name, size_t length) {
    for (size_t i = 0; i < hhy_contract_count(); i++) {
        const char *separator = strchr(contracts[i].name, '.');
        if (separator != NULL && (size_t)(separator - contracts[i].name) == length &&
            memcmp(contracts[i].name, name, length) == 0) return true;
    }
    return false;
}
