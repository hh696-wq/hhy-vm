#define _POSIX_C_SOURCE 200809L

#include <jansson.h>
#include <lexbor/css/css.h>
#include <lexbor/html/html.h>
#include <lexbor/html/serialize.h>
#include <lexbor/selectors/selectors.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    MAX_MESSAGE = 1024 * 1024,
    MAX_HTML_BYTES = 768 * 1024,
    MAX_SELECTOR_BYTES = 4096,
    MAX_ATTRIBUTE_BYTES = 256,
    MAX_FIELDS = 64,
    DEFAULT_MAX_RESULTS = 1000,
    HARD_MAX_RESULTS = 10000
};

typedef struct {
    const char *code;
    char message[256];
} HtmlError;

typedef struct {
    lxb_css_parser_t *parser;
    lxb_css_selector_list_t *list;
} CompiledSelector;

typedef struct {
    const char *name;
    const char *selector;
    const char *attribute;
    bool text;
    bool trim;
    CompiledSelector compiled;
} FieldSpec;

typedef struct {
    json_t *values;
    const char *attribute;
    bool text;
    bool trim;
    size_t limit;
    HtmlError *error;
} CollectContext;

typedef struct {
    lxb_dom_node_t *node;
} FirstContext;

typedef struct {
    lxb_dom_node_t **nodes;
    size_t count;
    size_t capacity;
    size_t limit;
    HtmlError *error;
} NodeListContext;

static void set_error(HtmlError *error, const char *code, const char *message) {
    if (error == NULL || error->code != NULL) return;
    error->code = code;
    snprintf(error->message, sizeof(error->message), "%s", message);
}

static bool emit(json_t *message) {
    char *text = json_dumps(message, JSON_COMPACT | JSON_ENSURE_ASCII | JSON_SORT_KEYS);
    if (text == NULL) return false;
    size_t length = strlen(text);
    bool ok = length <= MAX_MESSAGE && puts(text) != EOF && fflush(stdout) == 0;
    free(text);
    return ok;
}

static json_t *read_message(void) {
    char *line = malloc(MAX_MESSAGE + 2);
    if (line == NULL) return NULL;
    if (fgets(line, MAX_MESSAGE + 2, stdin) == NULL) {
        free(line);
        return NULL;
    }
    size_t length = strlen(line);
    json_error_t error;
    json_t *message = length <= MAX_MESSAGE + 1
        ? json_loadb(line, length, JSON_REJECT_DUPLICATES, &error) : NULL;
    free(line);
    return json_is_object(message) ? message : NULL;
}

static json_t *envelope(const char *type, const char *request_id) {
    return json_pack("{s:s,s:s,s:s,s:s}", "type", type, "request_id", request_id,
                     "extension_id", "html", "protocol_version", "1.0");
}

static json_t *contract(const char *name, size_t minimum, size_t maximum,
                        const char *input, const char *output) {
    return json_pack("{s:s,s:i,s:i,s:s,s:s,s:s,s:b,s:b,s:b,s:b,s:s}",
        "name", name, "minimum_arity", (json_int_t) minimum,
        "maximum_arity", (json_int_t) maximum, "input", input, "output", output,
        "effect", "none", "lazy", 0, "cancel", 0, "sendable", 1,
        "action", 0, "threading", "isolated_process");
}

static bool register_callables(void) {
    json_t *message = envelope("register", "register");
    json_t *items = json_array();
    if (message == NULL || items == NULL) return false;
    json_array_append_new(items, contract("html.text", 2, 3,
        "String, String, Map?", "String?"));
    json_array_append_new(items, contract("html.text_all", 2, 3,
        "String, String, Map?", "List<String>"));
    json_array_append_new(items, contract("html.attr", 3, 4,
        "String, String, String, Map?", "String?"));
    json_array_append_new(items, contract("html.attr_all", 3, 4,
        "String, String, String, Map?", "List<String>"));
    json_array_append_new(items, contract("html.exists", 2, 2,
        "String, String", "Bool"));
    json_array_append_new(items, contract("html.extract", 3, 4,
        "String, String, Map, Map?", "List<Map>"));
    json_object_set_new(message, "callables", items);
    bool ok = emit(message);
    json_decref(message);
    return ok;
}

static bool valid_string(json_t *value, size_t maximum) {
    return json_is_string(value) && json_string_length(value) <= maximum;
}

static size_t option_limit(json_t *options, HtmlError *error) {
    if (options == NULL) return DEFAULT_MAX_RESULTS;
    if (!json_is_object(options)) {
        set_error(error, "HTML_INVALID_OPTIONS", "options must be a Map");
        return 0;
    }
    json_t *value = json_object_get(options, "max_results");
    if (value == NULL) return DEFAULT_MAX_RESULTS;
    if (!json_is_integer(value)) {
        set_error(error, "HTML_INVALID_OPTIONS", "max_results must be an Int");
        return 0;
    }
    json_int_t limit = json_integer_value(value);
    if (limit <= 0 || limit > HARD_MAX_RESULTS) {
        set_error(error, "HTML_LIMIT", "max_results must be between 1 and 10000");
        return 0;
    }
    return (size_t) limit;
}

static bool option_trim(json_t *options, bool fallback, HtmlError *error) {
    if (options == NULL) return fallback;
    if (!json_is_object(options)) {
        set_error(error, "HTML_INVALID_OPTIONS", "options must be a Map");
        return fallback;
    }
    json_t *value = json_object_get(options, "trim");
    if (value == NULL) return fallback;
    if (!json_is_boolean(value)) {
        set_error(error, "HTML_INVALID_OPTIONS", "trim must be a Bool");
        return fallback;
    }
    return json_is_true(value);
}

static bool is_ascii_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

static json_t *text_json(const lxb_char_t *data, size_t length, bool trim,
                         HtmlError *error) {
    if (!trim) {
        json_t *value = json_stringn((const char *) data, length);
        if (value == NULL) set_error(error, "HTML_ENCODING", "extracted text is not valid UTF-8");
        return value;
    }
    char *normalized = malloc(length + 1);
    if (normalized == NULL) {
        set_error(error, "HTML_MEMORY", "unable to allocate normalized text");
        return NULL;
    }
    size_t out = 0;
    bool pending_space = false;
    for (size_t i = 0; i < length; i++) {
        unsigned char c = data[i];
        if (is_ascii_space(c)) {
            if (out > 0) pending_space = true;
            continue;
        }
        if (pending_space) normalized[out++] = ' ';
        pending_space = false;
        normalized[out++] = (char) c;
    }
    json_t *value = json_stringn(normalized, out);
    free(normalized);
    if (value == NULL) set_error(error, "HTML_ENCODING", "extracted text is not valid UTF-8");
    return value;
}

static bool compile_selector(const char *selector, CompiledSelector *compiled,
                             HtmlError *error) {
    size_t length = strlen(selector);
    if (length == 0 || length > MAX_SELECTOR_BYTES) {
        set_error(error, "HTML_INVALID_SELECTOR", "selector must contain 1 to 4096 bytes");
        return false;
    }
    compiled->parser = lxb_css_parser_create();
    if (compiled->parser == NULL ||
        lxb_css_parser_init(compiled->parser, NULL) != LXB_STATUS_OK) {
        set_error(error, "HTML_MEMORY", "unable to initialize the CSS parser");
        return false;
    }
    compiled->list = lxb_css_selectors_parse(compiled->parser,
        (const lxb_char_t *) selector, length);
    if (compiled->list == NULL || compiled->parser->status != LXB_STATUS_OK) {
        set_error(error, "HTML_INVALID_SELECTOR", "CSS selector is invalid or unsupported");
        return false;
    }
    return true;
}

static void destroy_selector(CompiledSelector *compiled) {
    if (compiled->parser != NULL) lxb_css_parser_destroy(compiled->parser, true);
    if (compiled->list != NULL) lxb_css_selector_list_destroy_memory(compiled->list);
    compiled->parser = NULL;
    compiled->list = NULL;
}

static lxb_status_t first_callback(lxb_dom_node_t *node,
                                   lxb_css_selector_specificity_t specificity,
                                   void *context) {
    (void) specificity;
    FirstContext *first = context;
    if (first->node == NULL) first->node = node;
    return LXB_STATUS_OK;
}

static lxb_dom_node_t *find_first(lxb_selectors_t *selectors, lxb_dom_node_t *root,
                                  CompiledSelector *compiled, HtmlError *error) {
    FirstContext context = {0};
    lxb_status_t status = lxb_selectors_find(selectors, root, compiled->list,
                                             first_callback, &context);
    if (status != LXB_STATUS_OK) {
        set_error(error, "HTML_SELECT_FAILED", "CSS selector evaluation failed");
        return NULL;
    }
    return context.node;
}

static json_t *node_value(lxb_dom_node_t *node, bool text, const char *attribute,
                          bool trim, HtmlError *error) {
    if (node == NULL) return json_null();
    if (text) {
        size_t length = 0;
        lxb_char_t *content = lxb_dom_node_text_content(node, &length);
        if (content == NULL) return json_string("");
        return text_json(content, length, trim, error);
    }
    if (node->type != LXB_DOM_NODE_TYPE_ELEMENT) return json_null();
    size_t length = 0;
    const lxb_char_t *value = lxb_dom_element_get_attribute(
        lxb_dom_interface_element(node), (const lxb_char_t *) attribute,
        strlen(attribute), &length);
    return value == NULL ? json_null() : text_json(value, length, trim, error);
}

static lxb_status_t collect_callback(lxb_dom_node_t *node,
                                     lxb_css_selector_specificity_t specificity,
                                     void *context) {
    (void) specificity;
    CollectContext *collect = context;
    if (json_array_size(collect->values) >= collect->limit) return LXB_STATUS_OK;
    json_t *value = node_value(node, collect->text, collect->attribute,
                               collect->trim, collect->error);
    if (value == NULL) return LXB_STATUS_ERROR;
    if (!json_is_null(value)) {
        int status = json_array_append(collect->values, value);
        json_decref(value);
        if (status != 0) {
            set_error(collect->error, "HTML_MEMORY", "unable to append extracted value");
            return LXB_STATUS_ERROR;
        }
    } else json_decref(value);
    return collect->error->code == NULL ? LXB_STATUS_OK : LXB_STATUS_ERROR;
}

static json_t *collect_values(lxb_selectors_t *selectors, lxb_dom_node_t *root,
                              CompiledSelector *compiled, const char *attribute,
                              bool text, bool trim, size_t limit, HtmlError *error) {
    json_t *values = json_array();
    if (values == NULL) {
        set_error(error, "HTML_MEMORY", "unable to allocate result list");
        return NULL;
    }
    CollectContext context = {.values = values, .attribute = attribute, .text = text,
        .trim = trim, .limit = limit, .error = error};
    lxb_status_t status = lxb_selectors_find(selectors, root, compiled->list,
                                             collect_callback, &context);
    if (status != LXB_STATUS_OK && error->code == NULL)
        set_error(error, "HTML_SELECT_FAILED", "CSS selector evaluation failed");
    if (error->code != NULL) {
        json_decref(values);
        return NULL;
    }
    return values;
}

static bool parse_field(const char *name, json_t *definition, FieldSpec *field,
                        HtmlError *error) {
    if (!json_is_object(definition)) {
        set_error(error, "HTML_INVALID_SCHEMA", "each schema field must be a Map");
        return false;
    }
    json_t *selector = json_object_get(definition, "selector");
    json_t *value = json_object_get(definition, "value");
    json_t *attribute = json_object_get(definition, "name");
    if (!json_is_string(selector) || !json_is_string(value)) {
        set_error(error, "HTML_INVALID_SCHEMA", "field selector and value must be Strings");
        return false;
    }
    field->name = name;
    field->selector = json_string_value(selector);
    field->text = strcmp(json_string_value(value), "text") == 0;
    if (!field->text && strcmp(json_string_value(value), "attr") != 0) {
        set_error(error, "HTML_INVALID_SCHEMA", "field value must be text or attr");
        return false;
    }
    if (!field->text) {
        if (!valid_string(attribute, MAX_ATTRIBUTE_BYTES) || json_string_length(attribute) == 0) {
            set_error(error, "HTML_INVALID_SCHEMA", "attr fields require a non-empty name");
            return false;
        }
        field->attribute = json_string_value(attribute);
    }
    json_t *trim = json_object_get(definition, "trim");
    if (trim != NULL && !json_is_boolean(trim)) {
        set_error(error, "HTML_INVALID_SCHEMA", "field trim must be a Bool");
        return false;
    }
    field->trim = trim == NULL || json_is_true(trim);
    if (strlen(field->selector) > 0 &&
        !compile_selector(field->selector, &field->compiled, error)) return false;
    return true;
}

static void destroy_fields(FieldSpec *fields, size_t count) {
    if (fields == NULL) return;
    for (size_t i = 0; i < count; i++) destroy_selector(&fields[i].compiled);
    free(fields);
}

static lxb_status_t node_list_callback(lxb_dom_node_t *node,
                                      lxb_css_selector_specificity_t specificity,
                                      void *context) {
    (void) specificity;
    NodeListContext *list = context;
    if (list->count >= list->limit) return LXB_STATUS_OK;
    if (list->count == list->capacity) {
        size_t capacity = list->capacity == 0 ? 16 : list->capacity * 2;
        if (capacity > list->limit) capacity = list->limit;
        lxb_dom_node_t **nodes = realloc(list->nodes, capacity * sizeof(*nodes));
        if (nodes == NULL) {
            set_error(list->error, "HTML_MEMORY", "unable to allocate matched node list");
            return LXB_STATUS_ERROR;
        }
        list->nodes = nodes;
        list->capacity = capacity;
    }
    list->nodes[list->count++] = node;
    return LXB_STATUS_OK;
}

static json_t *extract_node(lxb_dom_node_t *node, FieldSpec *fields,
                            size_t field_count, lxb_selectors_t *selectors,
                            HtmlError *error) {
    json_t *row = json_object();
    if (row == NULL) {
        set_error(error, "HTML_MEMORY", "unable to allocate extracted row");
        return NULL;
    }
    for (size_t i = 0; i < field_count; i++) {
        FieldSpec *field = &fields[i];
        lxb_dom_node_t *target = node;
        if (strlen(field->selector) > 0)
            target = find_first(selectors, node, &field->compiled, error);
        if (error->code != NULL) {
            json_decref(row);
            return NULL;
        }
        json_t *value = node_value(target, field->text, field->attribute,
                                   field->trim, error);
        if (value == NULL) {
            json_decref(row);
            if (error->code == NULL)
                set_error(error, "HTML_MEMORY", "unable to append extracted field");
            return NULL;
        }
        int status = json_object_set(row, field->name, value);
        json_decref(value);
        if (status != 0) {
            json_decref(row);
            set_error(error, "HTML_MEMORY", "unable to append extracted field");
            return NULL;
        }
    }
    return row;
}

static bool append_extracted_node(json_t *rows, lxb_dom_node_t *node,
                                  FieldSpec *fields, size_t field_count,
                                  lxb_selectors_t *selectors, HtmlError *error) {
    json_t *row = extract_node(node, fields, field_count, selectors, error);
    if (row == NULL) return false;
    int status = json_array_append(rows, row);
    json_decref(row);
    if (status != 0) {
        set_error(error, "HTML_MEMORY", "unable to append extracted row");
        return false;
    }
    return true;
}

static json_t *extract_rows(lxb_html_document_t *document, lxb_selectors_t *selectors,
                            const char *root_selector, json_t *schema, json_t *options,
                            HtmlError *error) {
    if (!json_is_object(schema) || json_object_size(schema) == 0 ||
        json_object_size(schema) > MAX_FIELDS) {
        set_error(error, "HTML_INVALID_SCHEMA", "schema must contain 1 to 64 fields");
        return NULL;
    }
    size_t field_count = json_object_size(schema), parsed = 0;
    FieldSpec *fields = calloc(field_count, sizeof(*fields));
    if (fields == NULL) {
        set_error(error, "HTML_MEMORY", "unable to allocate extraction schema");
        return NULL;
    }
    const char *name;
    json_t *definition;
    json_object_foreach(schema, name, definition) {
        if (!parse_field(name, definition, &fields[parsed], error)) {
            destroy_fields(fields, parsed + 1);
            return NULL;
        }
        parsed++;
    }
    CompiledSelector root = {0};
    if (!compile_selector(root_selector, &root, error)) {
        destroy_fields(fields, field_count);
        return NULL;
    }
    size_t limit = option_limit(options, error);
    json_t *rows = error->code == NULL ? json_array() : NULL;
    if (rows == NULL && error->code == NULL)
        set_error(error, "HTML_MEMORY", "unable to allocate extraction result");
    if (error->code == NULL) {
        NodeListContext context = {.limit = limit, .error = error};
        lxb_status_t status = lxb_selectors_find(selectors,
            lxb_dom_interface_node(document), root.list, node_list_callback, &context);
        if (status != LXB_STATUS_OK && error->code == NULL)
            set_error(error, "HTML_SELECT_FAILED", "root selector evaluation failed");
        for (size_t i = 0; i < context.count && error->code == NULL; i++)
            append_extracted_node(rows, context.nodes[i], fields, field_count,
                                  selectors, error);
        free(context.nodes);
    }
    destroy_selector(&root);
    destroy_fields(fields, field_count);
    if (error->code != NULL) {
        if (rows != NULL) json_decref(rows);
        return NULL;
    }
    return rows;
}

static json_t *execute(const char *callable, json_t *arguments, HtmlError *error) {
    size_t argc = json_array_size(arguments);
    json_t *html_arg = json_array_get(arguments, 0);
    json_t *selector_arg = json_array_get(arguments, 1);
    if (!valid_string(html_arg, MAX_HTML_BYTES)) {
        set_error(error, "HTML_INPUT_LIMIT", "HTML must be a String no larger than 768 KiB");
        return NULL;
    }
    if (!valid_string(selector_arg, MAX_SELECTOR_BYTES) || json_string_length(selector_arg) == 0) {
        set_error(error, "HTML_INVALID_SELECTOR", "selector must be a non-empty String up to 4096 bytes");
        return NULL;
    }
    lxb_html_document_t *document = lxb_html_document_create();
    if (document == NULL || lxb_html_document_parse(document,
        (const lxb_char_t *) json_string_value(html_arg), json_string_length(html_arg)) != LXB_STATUS_OK) {
        if (document != NULL) lxb_html_document_destroy(document);
        set_error(error, "HTML_PARSE_FAILED", "unable to parse HTML document");
        return NULL;
    }
    lxb_selectors_t *selectors = lxb_selectors_create();
    if (selectors == NULL || lxb_selectors_init(selectors) != LXB_STATUS_OK) {
        if (selectors != NULL) lxb_selectors_destroy(selectors, true);
        lxb_html_document_destroy(document);
        set_error(error, "HTML_MEMORY", "unable to initialize selector engine");
        return NULL;
    }
    json_t *result = NULL;
    if (strcmp(callable, "html.extract") == 0 && (argc == 3 || argc == 4)) {
        result = extract_rows(document, selectors, json_string_value(selector_arg),
            json_array_get(arguments, 2), argc == 4 ? json_array_get(arguments, 3) : NULL, error);
    } else {
        bool is_text = strcmp(callable, "html.text") == 0 ||
                       strcmp(callable, "html.text_all") == 0;
        bool is_attr = strcmp(callable, "html.attr") == 0 ||
                       strcmp(callable, "html.attr_all") == 0;
        bool is_all = strcmp(callable, "html.text_all") == 0 ||
                      strcmp(callable, "html.attr_all") == 0;
        bool is_exists = strcmp(callable, "html.exists") == 0;
        size_t required = is_attr ? 3 : 2;
        size_t maximum = required + ((is_text || is_attr) ? 1 : 0);
        if ((!is_text && !is_attr && !is_exists) || argc < required || argc > maximum) {
            set_error(error, "HTML_INVALID_ARGUMENT", "invalid HTML callable arguments");
        } else {
            const char *attribute = NULL;
            if (is_attr) {
                json_t *value = json_array_get(arguments, 2);
                if (!valid_string(value, MAX_ATTRIBUTE_BYTES) || json_string_length(value) == 0)
                    set_error(error, "HTML_INVALID_ARGUMENT", "attribute name must be a non-empty String");
                else attribute = json_string_value(value);
            }
            json_t *options = argc == maximum && !is_exists
                ? json_array_get(arguments, argc - 1) : NULL;
            size_t limit = is_all ? option_limit(options, error) : 1;
            bool trim = option_trim(options, true, error);
            CompiledSelector compiled = {0};
            if (error->code == NULL &&
                compile_selector(json_string_value(selector_arg), &compiled, error)) {
                if (is_exists) {
                    result = json_boolean(find_first(selectors, lxb_dom_interface_node(document),
                                                     &compiled, error) != NULL);
                } else if (is_all) {
                    result = collect_values(selectors, lxb_dom_interface_node(document),
                        &compiled, attribute, is_text, trim, limit, error);
                } else {
                    lxb_dom_node_t *node = find_first(selectors,
                        lxb_dom_interface_node(document), &compiled, error);
                    result = error->code == NULL
                        ? node_value(node, is_text, attribute, trim, error) : NULL;
                }
            }
            destroy_selector(&compiled);
        }
    }
    lxb_selectors_destroy(selectors, true);
    lxb_html_document_destroy(document);
    return result;
}

static bool handle_call(json_t *request) {
    const char *request_id = json_string_value(json_object_get(request, "request_id"));
    const char *callable = json_string_value(json_object_get(request, "callable"));
    json_t *arguments = json_object_get(request, "arguments");
    if (request_id == NULL || callable == NULL || !json_is_array(arguments)) return false;
    HtmlError error = {0};
    json_t *value = execute(callable, arguments, &error);
    if (value == NULL || error.code != NULL) {
        if (value != NULL) json_decref(value);
        json_t *response = envelope("error", request_id);
        json_object_set_new(response, "kind", json_string("HtmlError"));
        json_object_set_new(response, "code", json_string(
            error.code != NULL ? error.code : "HTML_OPERATION_FAILED"));
        json_object_set_new(response, "message", json_string(
            error.code != NULL ? error.message : "HTML operation failed"));
        bool ok = emit(response);
        json_decref(response);
        return ok;
    }
    json_t *response = envelope("call_result", request_id);
    json_object_set_new(response, "value", value);
    bool ok = emit(response);
    json_decref(response);
    return ok;
}

int main(int argc, char **argv) {
    if (argc != 3 || strcmp(argv[1], "--protocol") != 0 || strcmp(argv[2], "1") != 0)
        return 3;
    json_t *hello = read_message();
    if (hello == NULL || !json_is_string(json_object_get(hello, "runtime_version"))) {
        if (hello != NULL) json_decref(hello);
        return 2;
    }
    json_decref(hello);
    json_t *response = envelope("handshake_result", "handshake");
    json_object_set_new(response, "extension_version", json_string("0.1.0"));
    bool ok = emit(response);
    json_decref(response);
    if (!ok || !register_callables()) return 2;
    for (;;) {
        json_t *request = read_message();
        if (request == NULL) break;
        const char *type = json_string_value(json_object_get(request, "type"));
        if (type != NULL && strcmp(type, "shutdown") == 0) {
            json_decref(request);
            return 0;
        }
        ok = type != NULL && strcmp(type, "call") == 0 && handle_call(request);
        json_decref(request);
        if (!ok) return 2;
    }
    return 0;
}
