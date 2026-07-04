#include "option.h"
#include "logger.h"
#include "gitool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct OptionTable *search_short_option(const struct OptionTable option_table[], const int option_table_size, int short_opt) {
	for (int i = 0; i < option_table_size; ++i) {
		if (short_opt == option_table[i].shortopt) return &option_table[i];
	}
	return NULL;
}

static const struct OptionTable *search_long_option(const struct OptionTable option_table[], const int option_table_size, const char *long_opt, size_t len) {
	for (int i = 0; i < option_table_size; ++i) {
        if (strncmp(long_opt, option_table[i].longopt, len) == 0 && option_table[i].longopt[len] == '\0') return &option_table[i];
	}
	return NULL;
}

static void handle_option(struct Option *option, int short_opt, const char *arg)
{
	switch (short_opt) {
    case 'V':
        global_verbosity = VERBOSITY_VERBOSE;
        option->verbosity = VERBOSITY_VERBOSE;
        break;
    case 'q':
        global_verbosity = VERBOSITY_QUIET;
        option->verbosity = VERBOSITY_QUIET;
        break;
    case 'b':
        option->branch = strdup(arg);
        break;
    case 't':
        option->token = strdup(arg);
        break;
    case 'v':
        option->show_version = true;
        print_version();
        break;
    case 'h':
        option->show_help = true;
        print_help();
        break;
	}
}

void parse_option(struct Option *option, const struct OptionTable option_table[], const int option_table_size, int *argc, char **argv[]) {
    const struct OptionTable *opt = NULL;
    int write_idx = 1;

    for (int i = 1; i < *argc; i++) {
        const char *arg = (*argv)[i];
        int arg_len = strlen(arg);

        // may be command or command argument
        if (arg[0] != '-') {
            (*argv)[write_idx++] = (*argv)[i];
            continue;
        }

        // argument is '-' only
        if (arg_len == 1 && arg[0] == '-') {
            fprintf(stderr, "gitool error: End with option indicator '-'\n");
            exit(EXIT_FAILURE);
        }

        // argument is '--' only
        if (arg_len == 2 && arg[0] == '-' && arg[1] == '-') {
            fprintf(stderr, "gitool error: End with option indicator '--'\n");
            exit(EXIT_FAILURE);
        }

        // argument is '-x'
        if (arg_len == 2 && arg[0] == '-' && arg[1] != '-') {
            opt = search_short_option(option_table, option_table_size, arg[1]);

            if (opt == NULL) {
                fprintf(stderr, "gitool error: Invalid option '%c'\n", arg[1]);
                exit(EXIT_FAILURE);
            }

            const char *opt_arg = NULL;
            if (opt->argument) {
                if (i+1 < *argc) {
                    const char *next_arg = (*argv)[i + 1];
                    if (next_arg[0] == '-' && strlen(next_arg) > 1) {
                        fprintf(stderr, "gitool error: Option '%c' requires an argument, but got another option '%s'\n", arg[1], next_arg);
                        exit(EXIT_FAILURE);
                    }
                    opt_arg = (*argv)[++i];
                }
                else {
                    fprintf(stderr, "gitool error: Option '%c' requires an argument\n", arg[1]);
                    exit(EXIT_FAILURE);
                }
            }
            handle_option(option, opt->shortopt, opt_arg);

            continue;
        }

        // argument is '-xyz'
        if (arg_len > 2 && arg[0] == '-' && arg[1] != '-') {
            fprintf(stderr, "gitool error: Combination option are not allow, e.g: try -v -h -b main\n");
            exit(EXIT_FAILURE);
        }

        // argument is '--x'
        if (arg_len > 2 && arg[0] == '-' && arg[1] == '-') {
            opt = search_long_option(option_table, option_table_size, arg + 2, arg_len-2);

            if (opt == NULL) {
                fprintf(stderr, "gitool error: Invalid option '%s'\n", arg + 2);
                exit(EXIT_FAILURE);
            }

            const char *opt_arg = NULL;
            if (opt->argument) {
                if (i + 1 < *argc) {
                    const char *next_arg = (*argv)[i + 1];
                    if (next_arg[0] == '-' && strlen(next_arg) > 1) {
                        fprintf(stderr, "gitool error: Option '%s' requires an argument, but got another option '%s'\n", arg, next_arg);
                        exit(EXIT_FAILURE);
                    }
                    opt_arg = (*argv)[++i];
                }
                else {
                    fprintf(stderr, "gitool error: Option '%s' requires an argument\n", arg);
                    exit(EXIT_FAILURE);
                }
            }

            handle_option(option, opt->shortopt, opt_arg);

            continue;
        }

    }

    for (int k = write_idx; k < *argc; k++) {
        (*argv)[k] = NULL;
    }

    *argc = write_idx;
    (*argv)[write_idx] = NULL;

}
