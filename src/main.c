#include "hhy/ast.h"
#include "hhy/common.h"
#include "hhy/checker.h"
#include "hhy/formatter.h"
#include "hhy/extensions.h"
#include "hhy/package.h"
#include "hhy/parser.h"
#include "hhy/runtime.h"
#include "hhy/token.h"

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
    COMMAND_TOKENS,
    COMMAND_FMT,
    COMMAND_RUN
} Command;

static void usage(FILE *stream) {
    fputs(
        "HHY " HHY_VERSION "\n"
        "Pipe Everything.\n\n"
        "Usage:\n"
        "  hhy check <file.hhy>...   Validate syntax and core semantics\n"
        "  hhy ast <file.hhy>        Print the parsed AST\n"
        "  hhy tokens <file.hhy>     Print lexer tokens\n"
        "  hhy fmt <file.hhy>...     Format source files in place\n"
        "  hhy fmt --check <files>   Verify canonical formatting\n"
        "  hhy install [--yes] <dir> Install a local process extension\n"
        "  hhy list                  List installed extensions\n"
        "  hhy remove <package>      Remove an installed extension\n"
        "  hhy <file.hhy> [args]     Execute an HHY script\n"
        "  hhy run <file.hhy> [args] Execute an HHY script\n"
        "  hhy run --dry-run <file>   Plan without external side effects\n"
        "  hhy run --limit NAME=VALUE <file>  Override a RuntimeLimit\n"
        "  hhy --version             Print version\n"
        "  hhy --help                Print this help\n\n"
        "Flow-first system scripting language runtime.\n",
        stream);
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
                        const HhyRuntimeLimits *limits) {
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
        bool assume_yes = argc == 4 && strcmp(argv[2], "--yes") == 0;
        int path_index = assume_yes ? 3 : 2;
        if (argc != path_index + 1) { fputs("usage: hhy install [--yes] <local-path>\n", stderr); return 3; }
        return hhy_package_install(argv[path_index], assume_yes);
    }
    if (strcmp(argv[1], "list") == 0) {
        if (argc != 2) { fputs("usage: hhy list\n", stderr); return 3; }
        return hhy_package_list();
    }
    if (strcmp(argv[1], "remove") == 0) {
        if (argc != 3) { fputs("usage: hhy remove <package>\n", stderr); return 3; }
        return hhy_package_remove(argv[2]);
    }

    Command command;
    int source_index = 2;
    bool dry_run = false;
    HhyRuntimeLimits limits = hhy_runtime_limits_default();
    if (has_hhy_suffix(argv[1])) { command = COMMAND_RUN; source_index = 1; }
    else if (strcmp(argv[1], "check") == 0) command = COMMAND_CHECK;
    else if (strcmp(argv[1], "ast") == 0) command = COMMAND_AST;
    else if (strcmp(argv[1], "tokens") == 0) command = COMMAND_TOKENS;
    else if (strcmp(argv[1], "fmt") == 0) command = COMMAND_FMT;
    else if (strcmp(argv[1], "run") == 0) command = COMMAND_RUN;
    else {
        fprintf(stderr, "hhy: unknown command `%s`\n", argv[1]);
        usage(stderr);
        return 3;
    }
    if (command == COMMAND_RUN && source_index == 2) {
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
            break;
        }
    }
    if (command == COMMAND_FMT && source_index == 2 && argc > 2 &&
        strcmp(argv[2], "--check") == 0) {
        dry_run = true;
        source_index = 3;
    }

    if (argc <= source_index) {
        fprintf(stderr, "hhy: `%s` requires at least one source file\n", argv[1]);
        return 3;
    }
    if (command != COMMAND_CHECK && command != COMMAND_FMT && command != COMMAND_RUN &&
        argc != source_index + 1) {
        fprintf(stderr, "hhy: `%s` accepts exactly one source file\n", argv[1]);
        return 3;
    }

    int result = 0;
    int end = command == COMMAND_RUN ? source_index + 1 : argc;
    int script_start = source_index + 1;
    if (command == COMMAND_RUN && script_start < argc && strcmp(argv[script_start], "--") == 0)
        script_start++;
    for (int i = source_index; i < end; i++) {
        int file_result = process_file(argv[i], command, false,
                                       command == COMMAND_RUN ? argc - script_start : 0,
                                       command == COMMAND_RUN ? argv + script_start : NULL,
                                       dry_run, command == COMMAND_RUN ? &limits : NULL);
        if (file_result != 0) result = file_result;
    }
    hhy_extensions_shutdown();
    return result;
}
