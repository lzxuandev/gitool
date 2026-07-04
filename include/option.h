#ifndef OPTION_H
#define OPTION_H

#include <stdbool.h>

struct Option{
	int verbosity;
	char *branch;
    char *token;
    bool show_version;
    bool show_help;
};

struct OptionTable {
	const char shortopt;
	const char *longopt;
	const char *argument;
	const char *description;
};

void parse_option(struct Option *option,const struct OptionTable option_table[], const int option_table_size, int *argc, char **argv[]);

#endif
