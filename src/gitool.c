#include "gitool.h"
#include "option.h"
#include "command.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define VERSION "0.01"

static struct Command command = {
    .command_name = NULL,
    .local_path = NULL,
    .remote_path = NULL,
    .username = NULL,
    .repository = NULL,
    .path = NULL,
    .branch = NULL,
    .token = NULL
};

static struct Option option = {
    .verbosity = 1,
    .branch = NULL,
    .token= NULL,
    .show_help = false,
    .show_version = false
};

static const struct OptionTable option_table[] = {
	{ 'V', "verbose",    NULL,           "Enable verbose (debug) output mode" },
	{ 'q', "quiet",      NULL,           "Suppress status message" },
	{ 't', "token",      "<token>",      "Set remote repository Personal Access Token API (default: null)" },
    { 'b', "branch",     "<branch>",     "Set remote repository Branch (default: main)" },
    { 'v', "version",    NULL,           "Show version"},
    { 'h', "help",       NULL,           "Show help"}
};

static const struct CommandTable command_table[] = {
    {"upload",    2,   2, "<local_path> <username@repo:path>", "Upload local file to remote path"},
    {"delete",    1,   1, "<username@repo:path>",              "Delete file from remote repository"},
    {"list",      1,   1, "<username@repo:path>",              "List files in remote repository"},
    {"download",  1,   1, "<username@repo:path>",              "Download remote file to current directory"},
};

static const int option_table_size = sizeof(option_table)/sizeof(option_table[0]);
static const int command_table_size = sizeof(command_table)/sizeof(command_table[0]);

static void print_option_help() {
    int max_option_longopt_width = 0;
    int max_option_argument_width = 0;
	for (int i = 0; i < option_table_size; i++) {
        if ((int)strlen(option_table[i].longopt) > max_option_longopt_width) max_option_longopt_width = strlen(option_table[i].longopt);

        if (option_table[i].argument) {
            int option_argument_width = strlen(option_table[i].argument);
            if (option_argument_width > max_option_argument_width) max_option_argument_width = option_argument_width;
        }
    }

    max_option_longopt_width += 1;
    max_option_argument_width += 1;
    int total_width = max_option_longopt_width + max_option_argument_width;
	for (int i = 0; i < option_table_size; i++) {
        printf("  -%c, ", option_table[i].shortopt);
        if (!option_table[i].argument) printf("--%-*s", total_width, option_table[i].longopt);
        else {
            char str[256];
            snprintf(str, sizeof(str), "%s=%s", option_table[i].longopt, option_table[i].argument);
            printf("--%-*s", total_width, str);
        }
		printf("%s\n", option_table[i].description);
	}
}

static void print_command_help() {
    int max_command_name_width = 0;
    int max_command_argument_width = 0;
    for (int i = 0 ; i < command_table_size; i++) {
        if ((int)strlen(command_table[i].command_name)> max_command_name_width) max_command_name_width = strlen(command_table[i].command_name);
        if ((int)strlen(command_table[i].argument) > max_command_argument_width) max_command_argument_width = strlen(command_table[i].argument);
    }
    max_command_name_width += 1;
    max_command_argument_width +=1;
    for (int i = 0 ; i < command_table_size; i++) {
        printf(" gitool ");
        printf("%-*s%-*s", max_command_name_width, command_table[i].command_name, max_command_argument_width, command_table[i].argument);
        printf("%s\n", command_table[i].description);
    }
}

void print_version() {
    printf("gitool version: %s\n", VERSION);
}

void print_usage() {
	printf("Usage: gitool [options] <command> [--] [<arguments>]\n");
    printf("gitool version: %s\n", VERSION);
}

void print_help() {
	print_usage();
	printf("\n");
	printf("Options:\n");
	print_option_help();
	printf("\n");
	printf("Commands:\n");
    print_command_help();
	printf("\nSee man 1 gitool for more information about gitool commands and options\n\n");
}

int main(int argc, char *argv[]) {

    if (argc == 1) {
        print_usage();
        return EXIT_FAILURE;
    }

    parse_option(&option, option_table, option_table_size, &argc, &argv);

    if (argc == 1 || option.show_help || option.show_version) {
        return EXIT_SUCCESS;
    }

    command.branch = option.branch;
    command.token  = option.token;

    parse_command(&command, command_table, command_table_size, &argc, &argv);

    return EXIT_SUCCESS;
}
