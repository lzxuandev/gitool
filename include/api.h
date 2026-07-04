#ifndef API_H
#define API_H

#include <cjson/cJSON.h>

struct MemoryBuffer {
    char *memory;
    size_t size;
};

cJSON *github_request(const char *method, const char *username, const char *repo, const char *remote_path, const char *branch, const char *token, const char *json_data);
cJSON *build_sha_map(const char *username, const char *repo, const char *branch, const char *token);

#endif
