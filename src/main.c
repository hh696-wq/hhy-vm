#define _POSIX_C_SOURCE 200809L

#include "hhy/ast.h"
#include "hhy/bytecode.h"
#include "hhy/common.h"
#include "hhy/checker.h"
#include "hhy/contracts.h"
#include "hhy/formatter.h"
#include "hhy/extensions.h"
#include "hhy/package.h"
#include "hhy/parser.h"
#include "hhy/runtime.h"
#include "hhy/token.h"

#include <jansson.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

typedef enum {
    COMMAND_CHECK,
    COMMAND_AST,
    COMMAND_BYTECODE,
    COMMAND_TOKENS,
    COMMAND_FMT,
    COMMAND_PROFILE,
    COMMAND_RUN
} Command;

static void usage(FILE *stream) {
    fputs(
        "HHY " HHY_VERSION "\n"
        "Pipe Everything.\n\n"
        "Usage:\n"
        "  hhy check <file.hhy>...   Validate syntax and core semantics\n"
        "  hhy check --format json <files>  Emit versioned JSON diagnostics\n"
        "  hhy ast <file.hhy>        Print the parsed AST\n"
        "  hhy bytecode <file.hhy>   Compile, verify, and disassemble experimental Bytecode\n"
        "  hhy tokens <file.hhy>     Print lexer tokens\n"
        "  hhy fmt <file.hhy>...     Format source files in place\n"
        "  hhy fmt --check <files>   Verify canonical formatting\n"
        "  hhy contracts --format json  Print callable Contract Registry\n"
        "  hhy install [options] <source> Install a local or signed Registry extension\n"
        "  hhy lock [options] <package> Resolve and write hhy.lock\n"
        "  hhy fetch --locked [options] Cache a locked dependency graph\n"
        "  hhy rollback <package>      Restore the previous verified version\n"
        "  hhy doctor extensions [options] Verify lock, cache, and active installs\n"
        "  hhy list                  List installed extensions\n"
        "  hhy remove <package>      Remove an installed extension\n"
        "  hhy <file.hhy> [args]     Execute an HHY script\n"
        "  hhy run <file.hhy> [args] Execute an HHY script\n"
        "  hhy run --dry-run <file>   Plan without external side effects\n"
        "  hhy run --limit NAME=VALUE <file>  Override a RuntimeLimit\n"
        "  hhy profile [options] <file.hhy> [args]  Analyze CPU and heap usage\n"
        "    --cpu | --heap       Collect only the selected profile\n"
        "    --format text|json   Select report format (default: text)\n"
        "    --output <path>      Write the report to a file\n"
        "  hhy --version             Print version\n"
        "  hhy --help                Print this help\n\n"
        "Flow-first system scripting language runtime.\n",
        stream);
}

static int process_file(const char *path, Command command, bool quiet_success,
                        int script_argc, char **script_argv, bool dry_run,
                        const HhyRuntimeLimits *limits,
                        const HhyProfileOptions *profile);

static void append_captured_diagnostic(json_t *diagnostics, const char *path,
                                       const char *line) {
    size_t path_length = strlen(path);
    unsigned int source_line = 1, source_column = 1;
    const char *level = "error", *message = line;
    if (strncmp(line, path, path_length) == 0 && line[path_length] == ':') {
        const char *cursor = line + path_length + 1;
        char *end = NULL;
        unsigned long parsed_line = strtoul(cursor, &end, 10);
        if (end != cursor && *end == ':') {
            cursor = end + 1;
            unsigned long parsed_column = strtoul(cursor, &end, 10);
            if (end != cursor && strncmp(end, ": ", 2) == 0) {
                source_line = parsed_line > 0 && parsed_line <= UINT32_MAX ?
                    (unsigned int)parsed_line : 1;
                source_column = parsed_column > 0 && parsed_column <= UINT32_MAX ?
                    (unsigned int)parsed_column : 1;
                level = end + 2;
                const char *separator = strstr(level, ": ");
                if (separator != NULL) message = separator + 2;
            }
        }
    }
    bool warning = strncmp(level, "warning", 7) == 0;
    const char *code = strstr(level, "check") != NULL ? "HHY_CHECK" :
                       strstr(level, "warning") != NULL ? "HHY_CHECK_WARNING" :
                       strncmp(line, path, path_length) == 0 ? "HHY_SYNTAX" : "HHY_IO";
    json_t *item = json_pack("{s:s,s:s,s:s,s:s,s:i,s:{s:i,s:i},s:{s:i,s:i}}",
                             "path", path, "severity", warning ? "warning" : "error",
                             "code", code, "message", message,
                             "stage", strstr(level, "check") != NULL ? 3 :
                                      strncmp(line, path, path_length) == 0 ? 2 : 1,
                             "start", "line", source_line - 1,
                                      "character", source_column - 1,
                             "end", "line", source_line - 1,
                                    "character", source_column);
    json_array_append_new(diagnostics, item);
}

static int process_check_json(int count, char **paths) {
    json_t *diagnostics = json_array();
    int result = 0;
    for (int i = 0; i < count; i++) {
        FILE *capture = tmpfile();
        if (capture == NULL) { result = 4; continue; }
        fflush(stderr);
        int saved_stderr = dup(STDERR_FILENO);
        if (saved_stderr < 0 || dup2(fileno(capture), STDERR_FILENO) < 0) {
            if (saved_stderr >= 0) close(saved_stderr);
            fclose(capture); result = 4; continue;
        }
        int file_result = process_file(paths[i], COMMAND_CHECK, true, 0, NULL,
                                       false, NULL, NULL);
        fflush(stderr);
        dup2(saved_stderr, STDERR_FILENO);
        close(saved_stderr);
        rewind(capture);
        char line[4096];
        while (fgets(line, sizeof(line), capture) != NULL) {
            size_t length = strlen(line);
            while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r'))
                line[--length] = '\0';
            if (length == 0 || strncmp(line, "  ", 2) == 0 || line[0] == '^') continue;
            append_captured_diagnostic(diagnostics, paths[i], line);
        }
        fclose(capture);
        if (file_result != 0) result = file_result;
    }
    json_t *root = json_pack("{s:i,s:s,s:o}", "schema_version", 1,
                             "tool", "hhy", "diagnostics", diagnostics);
    json_dumpf(root, stdout, JSON_INDENT(2) | JSON_SORT_KEYS);
    fputc('\n', stdout);
    json_decref(root);
    return result;
}

static int print_contracts_json(void) {
    json_t *items = json_array();
    for (size_t i = 0; i < hhy_contract_count(); i++) {
        const HhyCallableContract *contract = hhy_contract_at(i);
        json_t *item = json_pack("{s:s,s:i,s:s,s:b,s:b,s:b,s:b,s:s,s:s,s:s}",
                                 "name", contract->name,
                                 "minimum_arity", (int)contract->minimum_arity,
                                 "effect", hhy_effect_name(contract->effect),
                                 "lazy", contract->lazy,
                                 "cancellable", contract->cancellable,
                                 "sendable", contract->sendable,
                                 "action", contract->action,
                                 "input", contract->input_contract,
                                 "output", contract->output_contract,
                                 "threading", contract->threading);
        if (contract->maximum_arity == SIZE_MAX)
            json_object_set_new(item, "maximum_arity", json_null());
        else
            json_object_set_new(item, "maximum_arity", json_integer((json_int_t)contract->maximum_arity));
        json_array_append_new(items, item);
    }
    json_t *root = json_pack("{s:i,s:s,s:o}", "schema_version", 1,
                             "tool", "hhy", "contracts", items);
    json_dumpf(root, stdout, JSON_INDENT(2) | JSON_SORT_KEYS);
    fputc('\n', stdout);
    json_decref(root);
    return 0;
}

static bool has_hhy_suffix(const char *path) {
    size_t length = strlen(path);
    return length >= 4 && strcmp(path + length - 4, ".hhy") == 0;
}

static bool write_formatted(const char *path, const char *text, size_t length) {
    struct stat info;
    mode_t mode = stat(path, &info) == 0 ? info.st_mode & 0777 : 0644;
    size_t temporary_length = strlen(path) + 48;
    char *temporary = hhy_alloc(temporary_length);
    snprintf(temporary, temporary_length, "%s.hhy-fmt-%ld", path, (long)getpid());
    FILE *file = fopen(temporary, "wb");
    bool ok = file != NULL;
    if (ok) ok = fwrite(text, 1, length, file) == length && fflush(file) == 0;
    if (file != NULL && fclose(file) != 0) ok = false;
    if (ok) ok = chmod(temporary, mode) == 0 && rename(temporary, path) == 0;
    if (!ok) { unlink(temporary); fprintf(stderr, "hhy: cannot format %s\n", path); }
    free(temporary);
    return ok;
}

static int process_file(const char *path, Command command, bool quiet_success,
                        int script_argc, char **script_argv, bool dry_run,
                        const HhyRuntimeLimits *limits,
                        const HhyProfileOptions *profile) {
    HhySource source = {0};
    HhyTokenList tokens = {0};
    HhyNode *program = NULL;

    if (!hhy_source_load(path, &source)) return 4;

    bool lex_ok = hhy_lex(&source, &tokens);
    if (command == COMMAND_TOKENS) hhy_tokens_print(&tokens);

    if (lex_ok && command == COMMAND_FMT) {
        size_t formatted_length = 0;
        char *formatted = hhy_format_source(&source, &tokens, &formatted_length);
        bool changed = formatted_length != source.length ||
                       memcmp(formatted, source.text, source.length) != 0;
        bool format_ok = true;
        if (dry_run && changed) {
            fprintf(stderr, "%s: requires formatting\n", path);
            format_ok = false;
        } else if (!dry_run && changed) {
            format_ok = write_formatted(path, formatted, formatted_length);
        }
        free(formatted);
        hhy_tokens_free(&tokens);
        hhy_source_free(&source);
        return format_ok ? 0 : 2;
    }

    HhyParseResult parsed = {.ok = false, .error_count = 0};
    if (lex_ok && command != COMMAND_TOKENS) {
        parsed = hhy_parse(&source, &tokens, &program);
        if (parsed.ok && command == COMMAND_AST) hhy_ast_print(program);
        if (parsed.ok && command == COMMAND_BYTECODE) {
            HhyCheckResult checked = hhy_check(&source, program);
            parsed.ok = checked.ok;
            if (parsed.ok) {
                hhy_resolve_slots(program);
                HhyBytecodeChunk chunk;
                hhy_bytecode_chunk_init(&chunk);
                HhyBytecodeResult compiled = hhy_bytecode_compile(program, &chunk);
                if (compiled.ok) hhy_bytecode_disassemble(&chunk, stdout);
                else {
                    fprintf(stderr, "%s:%u:%u: bytecode error at instruction %zu: %s\n",
                            path, program->token.line, program->token.column,
                            compiled.instruction, compiled.message);
                    parsed.ok = false;
                }
                hhy_bytecode_chunk_free(&chunk);
            }
        }
        if (parsed.ok && command == COMMAND_CHECK) {
            HhyCheckResult checked = hhy_check(&source, program);
            parsed.ok = checked.ok;
        }
    }

    bool ok = lex_ok && (command == COMMAND_TOKENS || parsed.ok);
    int run_exit = 0;
    if (ok && command == COMMAND_RUN) {
        HhyRunResult run = hhy_run_program(&source, program, script_argc, script_argv,
                                           dry_run, limits);
        ok = run.ok;
        run_exit = run.exit_code;
    }
    if (ok && command == COMMAND_PROFILE) {
        HhyRunResult run = hhy_profile_program(&source, program, script_argc, script_argv,
                                               dry_run, limits, profile);
        ok = run.ok;
        run_exit = run.exit_code;
    }
    if (command == COMMAND_CHECK && ok && !quiet_success) printf("ok  %s\n", path);

    hhy_node_free(program);
    hhy_tokens_free(&tokens);
    hhy_source_free(&source);
    return ok ? run_exit : (run_exit != 0 ? run_exit : 2);
}

static bool parse_scaled(const char *text, double *out_number, const char **out_suffix) {
    errno = 0; char *end = NULL;
    double number = strtod(text, &end);
    if (errno == ERANGE || end == text || !isfinite(number) || number < 0) return false;
    *out_number = number; *out_suffix = end; return true;
}

static bool parse_limit(const char *assignment, HhyRuntimeLimits *limits) {
    const char *equal = strchr(assignment, '=');
    if (equal == NULL || equal == assignment || equal[1] == '\0') return false;
    size_t name_length = (size_t)(equal - assignment);
    double number; const char *suffix;
    if (!parse_scaled(equal + 1, &number, &suffix)) return false;
    double multiplier = 1.0;
    bool bytes = name_length == 10 && memcmp(assignment, "max_memory", 10) == 0;
    bool http_bytes = name_length == 13 && memcmp(assignment, "max_http_body", 13) == 0;
    bool duration = name_length == 11 && memcmp(assignment, "max_runtime", 11) == 0;
    if (bytes || http_bytes) {
        if (strcmp(suffix, "b") == 0) multiplier = 1.0;
        else if (strcmp(suffix, "kb") == 0) multiplier = 1000.0;
        else if (strcmp(suffix, "mb") == 0) multiplier = 1000000.0;
        else if (strcmp(suffix, "gb") == 0) multiplier = 1000000000.0;
        else if (strcmp(suffix, "kib") == 0) multiplier = 1024.0;
        else if (strcmp(suffix, "mib") == 0) multiplier = 1048576.0;
        else if (strcmp(suffix, "gib") == 0) multiplier = 1073741824.0;
        else return false;
    } else if (duration) {
        if (strcmp(suffix, "ns") == 0) multiplier = 1.0;
        else if (strcmp(suffix, "us") == 0) multiplier = 1000.0;
        else if (strcmp(suffix, "ms") == 0) multiplier = 1000000.0;
        else if (strcmp(suffix, "s") == 0) multiplier = 1000000000.0;
        else if (strcmp(suffix, "min") == 0) multiplier = 60000000000.0;
        else if (strcmp(suffix, "h") == 0) multiplier = 3600000000000.0;
        else return false;
    } else if (*suffix != '\0' || floor(number) != number) return false;
    double scaled = number * multiplier;
    if (!isfinite(scaled) || scaled > (double)SIZE_MAX) return false;
    size_t value = (size_t)scaled;
    if (bytes) limits->max_memory = value;
    else if (http_bytes) limits->max_http_body = value;
    else if (duration) limits->max_runtime_ns = (uint64_t)scaled;
    else if (name_length == 14 && memcmp(assignment, "max_open_files", 14) == 0)
        limits->max_open_files = value;
    else if (name_length == 13 && memcmp(assignment, "max_processes", 13) == 0)
        limits->max_processes = value;
    else if (name_length == 15 && memcmp(assignment, "max_parallelism", 15) == 0)
        limits->max_parallelism = value;
    else if (name_length == 15 && memcmp(assignment, "max_regex_steps", 15) == 0 && value <= UINT32_MAX)
        limits->max_regex_steps = (uint32_t)value;
    else if (name_length == 13 && memcmp(assignment, "max_recursion", 13) == 0)
        limits->max_recursion = value;
    else return false;
    return value > 0;
}

int main(int argc, char **argv) {
    /* Keep Unicode behavior independent from an inherited POSIX/C locale. */
    if (setlocale(LC_CTYPE, "C.UTF-8") == NULL)
        setlocale(LC_CTYPE, "");
    if (argc == 1) {
        usage(stderr);
        return 3;
    }
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage(stdout);
        return 0;
    }
    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0) {
        puts("hhy " HHY_VERSION);
        puts("© 2026 HHY Language contributors");
        puts("Author: houhuiyang");
        puts("License: Apache License 2.0");
        puts("https://hhylang.dev/");
        puts("huiyang.hou@qq.com");
        return 0;
    }
    if (strcmp(argv[1], "repl") == 0) {
        int result = hhy_repl(); hhy_extensions_shutdown(); return result;
    }
    if (strcmp(argv[1], "install") == 0) {
        HhyPackageInstallOptions options = {0}; const char *source = NULL;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--yes") == 0) options.assume_yes = true;
            else if (strcmp(argv[i], "--dry-run") == 0) options.dry_run = true;
            else if (strcmp(argv[i], "--locked") == 0) options.locked = true;
            else if (strcmp(argv[i], "--offline") == 0) options.offline = true;
            else if (strcmp(argv[i], "--upgrade") == 0) options.upgrade = true;
            else if (strcmp(argv[i], "--lockfile") == 0 && i + 1 < argc) options.lockfile = argv[++i];
            else if (strcmp(argv[i], "--cache") == 0 && i + 1 < argc) options.cache = argv[++i];
            else if (strcmp(argv[i], "--registry") == 0 && i + 1 < argc) options.registry = argv[++i];
            else if (strcmp(argv[i], "--trust-root") == 0 && i + 1 < argc) options.trust_root = argv[++i];
            else if (argv[i][0] != '-' && source == NULL) source = argv[i];
            else { source = NULL; break; }
        }
        if (source == NULL) {
            fputs("usage: hhy install [--yes] [--dry-run] [--upgrade] [--locked] [--offline] [--lockfile FILE] [--cache DIR] [--registry DIR --trust-root FILE] <package-or-path>\n", stderr);
            return 3;
        }
        if (options.registry != NULL || options.trust_root != NULL || options.locked || options.offline)
            return hhy_registry_install(source, &options);
        return hhy_package_install(source, &options);
    }
    if (strcmp(argv[1], "lock") == 0) {
        HhyPackageInstallOptions options = {0}; const char *identity = NULL;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--registry") == 0 && i + 1 < argc) options.registry = argv[++i];
            else if (strcmp(argv[i], "--trust-root") == 0 && i + 1 < argc) options.trust_root = argv[++i];
            else if (strcmp(argv[i], "--lockfile") == 0 && i + 1 < argc) options.lockfile = argv[++i];
            else if (argv[i][0] != '-' && identity == NULL) identity = argv[i];
            else { identity = NULL; break; }
        }
        if (identity == NULL) { fputs("usage: hhy lock [--lockfile FILE] --registry DIR --trust-root FILE <package>\n", stderr); return 3; }
        return hhy_registry_lock(identity, &options);
    }
    if (strcmp(argv[1], "fetch") == 0) {
        HhyPackageInstallOptions options = {0};
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--locked") == 0) options.locked = true;
            else if (strcmp(argv[i], "--lockfile") == 0 && i + 1 < argc) options.lockfile = argv[++i];
            else if (strcmp(argv[i], "--cache") == 0 && i + 1 < argc) options.cache = argv[++i];
            else if (strcmp(argv[i], "--registry") == 0 && i + 1 < argc) options.registry = argv[++i];
            else if (strcmp(argv[i], "--trust-root") == 0 && i + 1 < argc) options.trust_root = argv[++i];
            else { fputs("usage: hhy fetch --locked [--lockfile FILE] [--cache DIR] --registry DIR --trust-root FILE\n", stderr); return 3; }
        }
        return hhy_registry_fetch(&options);
    }
    if (strcmp(argv[1], "rollback") == 0) {
        if (argc != 3) { fputs("usage: hhy rollback <package>\n", stderr); return 3; }
        return hhy_package_rollback(argv[2]);
    }
    if (strcmp(argv[1], "doctor") == 0) {
        HhyPackageInstallOptions options = {0};
        if (argc < 3 || strcmp(argv[2], "extensions") != 0) { fputs("usage: hhy doctor extensions [--lockfile FILE] [--cache DIR]\n", stderr); return 3; }
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--lockfile") == 0 && i + 1 < argc) options.lockfile = argv[++i];
            else if (strcmp(argv[i], "--cache") == 0 && i + 1 < argc) options.cache = argv[++i];
            else { fputs("usage: hhy doctor extensions [--lockfile FILE] [--cache DIR]\n", stderr); return 3; }
        }
        return hhy_package_doctor(&options);
    }
    if (strcmp(argv[1], "list") == 0) {
        if (argc != 2) { fputs("usage: hhy list\n", stderr); return 3; }
        return hhy_package_list();
    }
    if (strcmp(argv[1], "remove") == 0) {
        if (argc != 3) { fputs("usage: hhy remove <package>\n", stderr); return 3; }
        return hhy_package_remove(argv[2]);
    }
    if (strcmp(argv[1], "contracts") == 0) {
        if (argc != 4 || strcmp(argv[2], "--format") != 0 || strcmp(argv[3], "json") != 0) {
            fputs("usage: hhy contracts --format json\n", stderr); return 3;
        }
        return print_contracts_json();
    }

    Command command;
    int source_index = 2;
    bool dry_run = false;
    bool check_json = false;
    bool profile_cpu = false, profile_heap = false, profile_selection = false;
    bool profile_json = false;
    const char *profile_output_path = NULL;
    HhyRuntimeLimits limits = hhy_runtime_limits_default();
    if (has_hhy_suffix(argv[1])) { command = COMMAND_RUN; source_index = 1; }
    else if (strcmp(argv[1], "check") == 0) command = COMMAND_CHECK;
    else if (strcmp(argv[1], "ast") == 0) command = COMMAND_AST;
    else if (strcmp(argv[1], "bytecode") == 0) command = COMMAND_BYTECODE;
    else if (strcmp(argv[1], "tokens") == 0) command = COMMAND_TOKENS;
    else if (strcmp(argv[1], "fmt") == 0) command = COMMAND_FMT;
    else if (strcmp(argv[1], "profile") == 0) command = COMMAND_PROFILE;
    else if (strcmp(argv[1], "run") == 0) command = COMMAND_RUN;
    else {
        fprintf(stderr, "hhy: unknown command `%s`\n", argv[1]);
        usage(stderr);
        return 3;
    }
    if ((command == COMMAND_RUN || command == COMMAND_PROFILE) && source_index == 2) {
        while (source_index < argc) {
            if (strcmp(argv[source_index], "--dry-run") == 0) {
                dry_run = true; source_index++; continue;
            }
            if (strcmp(argv[source_index], "--limit") == 0) {
                if (source_index + 1 >= argc || !parse_limit(argv[source_index + 1], &limits)) {
                    fputs("hhy: --limit expects a valid NAME=VALUE\n", stderr); return 3;
                }
                source_index += 2; continue;
            }
            if (command == COMMAND_PROFILE && strcmp(argv[source_index], "--cpu") == 0) {
                profile_selection = true; profile_cpu = true; source_index++; continue;
            }
            if (command == COMMAND_PROFILE && strcmp(argv[source_index], "--heap") == 0) {
                profile_selection = true; profile_heap = true; source_index++; continue;
            }
            if (command == COMMAND_PROFILE && strcmp(argv[source_index], "--format") == 0) {
                if (source_index + 1 >= argc ||
                    (strcmp(argv[source_index + 1], "text") != 0 &&
                     strcmp(argv[source_index + 1], "json") != 0)) {
                    fputs("hhy: --format expects text or json\n", stderr); return 3;
                }
                profile_json = strcmp(argv[source_index + 1], "json") == 0;
                source_index += 2; continue;
            }
            if (command == COMMAND_PROFILE && strcmp(argv[source_index], "--output") == 0) {
                if (source_index + 1 >= argc || argv[source_index + 1][0] == '\0') {
                    fputs("hhy: --output expects a path\n", stderr); return 3;
                }
                profile_output_path = argv[source_index + 1];
                source_index += 2; continue;
            }
            break;
        }
    }
    if (command == COMMAND_FMT && source_index == 2 && argc > 2 &&
        strcmp(argv[2], "--check") == 0) {
        dry_run = true;
        source_index = 3;
    }
    if (command == COMMAND_CHECK && source_index == 2 && argc > 3 &&
        strcmp(argv[2], "--format") == 0 && strcmp(argv[3], "json") == 0) {
        check_json = true;
        source_index = 4;
    }

    if (argc <= source_index) {
        fprintf(stderr, "hhy: `%s` requires at least one source file\n", argv[1]);
        return 3;
    }
    if (check_json) {
        int result = process_check_json(argc - source_index, argv + source_index);
        hhy_extensions_shutdown();
        return result;
    }
    if (command != COMMAND_CHECK && command != COMMAND_FMT && command != COMMAND_RUN &&
        command != COMMAND_PROFILE &&
        argc != source_index + 1) {
        fprintf(stderr, "hhy: `%s` accepts exactly one source file\n", argv[1]);
        return 3;
    }

    int result = 0;
    int end = (command == COMMAND_RUN || command == COMMAND_PROFILE) ? source_index + 1 : argc;
    int script_start = source_index + 1;
    if ((command == COMMAND_RUN || command == COMMAND_PROFILE) && script_start < argc &&
        strcmp(argv[script_start], "--") == 0)
        script_start++;
    FILE *profile_output = NULL;
    HhyProfileOptions profile_options = {0};
    if (command == COMMAND_PROFILE) {
        if (!profile_selection) profile_cpu = profile_heap = true;
        if (profile_output_path != NULL) {
            profile_output = fopen(profile_output_path, "wb");
            if (profile_output == NULL) {
                fprintf(stderr, "hhy: cannot open profile output %s\n", profile_output_path);
                return 4;
            }
        }
        profile_options = (HhyProfileOptions){
            .cpu = profile_cpu, .heap = profile_heap, .json = profile_json,
            .output = profile_output == NULL ? stderr : profile_output
        };
    }
    for (int i = source_index; i < end; i++) {
        int file_result = process_file(argv[i], command, false,
                                       (command == COMMAND_RUN || command == COMMAND_PROFILE)
                                           ? argc - script_start : 0,
                                       (command == COMMAND_RUN || command == COMMAND_PROFILE)
                                           ? argv + script_start : NULL,
                                       dry_run,
                                       (command == COMMAND_RUN || command == COMMAND_PROFILE)
                                           ? &limits : NULL,
                                       command == COMMAND_PROFILE ? &profile_options : NULL);
        if (file_result != 0) result = file_result;
    }
    if (profile_output != NULL && fclose(profile_output) != 0 && result == 0) result = 4;
    hhy_extensions_shutdown();
    return result;
}
