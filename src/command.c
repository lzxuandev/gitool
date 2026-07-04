#include "command.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const struct CommandTable *search_command(const struct CommandTable command_table[], const int command_table_size, const char *cmd_name, int cmd_len){
	for (int i = 0; i < command_table_size; ++i) {
        if (strncmp(cmd_name, command_table[i].command_name, cmd_len) == 0 && command_table[i].command_name[cmd_len] == '\0') return &command_table[i];
	}
	return NULL;
}

static void handle_command(struct Command command){

    if (strcmp(command.command_name, "upload") == 0) upload(command);
    if (strcmp(command.command_name, "delete") == 0) delete(command);
    if (strcmp(command.command_name, "list") == 0) list(command);
    if (strcmp(command.command_name, "download") == 0) download(command);
}

void parse_command(struct Command *command, const struct CommandTable command_table[], const int command_table_size, int *argc, char **argv[]){
    const struct CommandTable *cmd = NULL;
    int write_idx = 1;

    for (int i = 1; i < *argc; i++) {
        const char *arg = (*argv)[i];
        int arg_len = strlen(arg);

        cmd = search_command(command_table, command_table_size, arg, arg_len);
        if (cmd == NULL) {
            fprintf(stderr, "gitool error: Unknown command '%s'\n", arg);
            exit(EXIT_FAILURE);
        }

        if (*argc - 2 < cmd->min_argc ) {
            fprintf(stderr, "gitool error: '%s' requires minimum %d arguments\n", cmd->command_name, cmd->min_argc);
            exit(EXIT_FAILURE);
        } else if (*argc - 2 > cmd->max_argc) {
            int extra_arg_idx = cmd->max_argc + 2;
            if (extra_arg_idx < *argc) {
                const char *extra_arg = (*argv)[extra_arg_idx];
                int extra_len = strlen(extra_arg);
                const struct CommandTable *check_cmd = search_command(command_table, command_table_size, extra_arg, extra_len);
                if (check_cmd != NULL) {
                    fprintf(stderr, "gitool error: Multiple commands are not allowed. Found '%s' and '%s'\n", cmd->command_name, check_cmd->command_name);
                    exit(EXIT_FAILURE);
                }
            }
            fprintf(stderr, "gitool error: '%s' only needs %d arguments, but got %d\n", cmd->command_name, cmd->max_argc, *argc - 2);
            exit(EXIT_FAILURE);
        }

        command->command_name = cmd->command_name;
        if (*argc-2 == 1) {
            command->local_path = NULL;
            command->remote_path = (*argv)[i + 1];
        }
        else if (*argc-2 >= 2) {
            command->local_path = (*argv)[i + 1];
            command->remote_path = (*argv)[i + 2];

            if (command->local_path != NULL && strcmp(command->command_name, "upload") == 0) {
                struct stat st;
                if (stat(command->local_path, &st) != 0) {
                    fprintf(stderr, "gitool error: Local path '%s' does not exist\n", command->local_path);
                    exit(EXIT_FAILURE);
                }
            }

        }

        const char *at_sign = strchr(command->remote_path, '@');
        const char *colon = strchr(command->remote_path, ':');

        if (at_sign == NULL || colon == NULL) {
            fprintf(stderr, "gitool error: Remote path wrong format, e.g. try username@repository:path\n");
            exit(EXIT_FAILURE);
        }

        if (at_sign > colon) {
            fprintf(stderr, "gitool error: Remote path wrong format, e.g. try username@repository:path\n");
            exit(EXIT_FAILURE);
        }

        command->username = strndup(command->remote_path, at_sign - command->remote_path);
        command->repository = strndup(at_sign + 1, colon - (at_sign + 1));
        command->path = strdup(colon + 1);

        if (command->path && *command->path == '\0') {
            if (strcmp(command->command_name, "upload") == 0 && command->local_path != NULL) {
                const char *filename = strrchr(command->local_path, '/');
                if (filename) {
                    filename = filename + 1;
                } else {
                    filename = command->local_path;
                }

                command->path = filename;
            }
        }

        command->branch = command->branch ? command->branch: "main";
        command->token = command->token;

        handle_command(*command);

        i += *argc-2;
    }

    for (int k = write_idx; k < *argc; k++) {
        (*argv)[k] = NULL;
    }

    *argc = write_idx;
    (*argv)[write_idx] = NULL;

}
