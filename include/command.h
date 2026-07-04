#ifndef COMMAND_H
#define COMMAND_H

#include <stdbool.h>

struct Command {
    const char *command_name;
    const char *local_path;
    const char *remote_path;
    const char *username;
    const char *repository;
    const char *path;
    const char *branch;
    const char *token;
};

struct CommandTable {
    const char *command_name;
    const int min_argc;
    const int max_argc;
    const char *argument;
    const char *description;
};

void parse_command(struct Command *command, const struct CommandTable command_table[], const int command_table_size, int *argc, char **argv[]);

void upload(const struct Command command);
void delete(const struct Command command);
void list(const struct Command command);
void download(const struct Command command);

#endif
