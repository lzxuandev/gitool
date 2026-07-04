#include "command.h"
#include "api.h"
#include "logger.h"

#include <stdio.h>
#include <string.h>
#include <cjson/cJSON.h>

static void list_file(cJSON *file_obj) {
    cJSON *name = cJSON_GetObjectItemCaseSensitive(file_obj, "name");
    cJSON *path = cJSON_GetObjectItemCaseSensitive(file_obj, "path");
    cJSON *size = cJSON_GetObjectItemCaseSensitive(file_obj, "size");
    cJSON *sha = cJSON_GetObjectItemCaseSensitive(file_obj, "sha");
    cJSON *type = cJSON_GetObjectItemCaseSensitive(file_obj, "type");
    cJSON *html_url = cJSON_GetObjectItemCaseSensitive(file_obj, "html_url");

    PRINT("File: %s\n", path && cJSON_IsString(path) ? path->valuestring : "unknown");

    if (name && cJSON_IsString(name))
        PRINT("  Name     : %s\n", name->valuestring);
    if (type && cJSON_IsString(type))
        PRINT("  Type     : %s\n", type->valuestring);
    if (size && cJSON_IsNumber(size))
        PRINT("  Size     : %d bytes\n", size->valueint);
    if (sha && cJSON_IsString(sha))
        PRINT("  SHA      : %s\n", sha->valuestring);
    if (html_url && cJSON_IsString(html_url))
        PRINT("  URL      : %s\n", html_url->valuestring);

    PRINT("\n");
}

static void list_directory(const struct Command command, cJSON *dir_array) {
    int file_count = 0;
    int dir_count = 0;
    size_t total_size = 0;

    PRINT("Directory: %s\n", *command.path ? command.path : "root");
    PRINT("%-50s %-10s %s\n", "File Name", "Size", "SHA");
    PRINT("-------------------------------------------------- ---------- ----------------------------------------\n");

    cJSON *item = NULL;
    cJSON_ArrayForEach(item, dir_array) {
        cJSON *type = cJSON_GetObjectItemCaseSensitive(item, "type");
        cJSON *name = cJSON_GetObjectItemCaseSensitive(item, "name");
        cJSON *size = cJSON_GetObjectItemCaseSensitive(item, "size");
        cJSON *sha = cJSON_GetObjectItemCaseSensitive(item, "sha");

        const char *type_str = type && cJSON_IsString(type) ? type->valuestring : "unknown";
        const char *name_str = name && cJSON_IsString(name) ? name->valuestring : "unknown";
        int size_val = size && cJSON_IsNumber(size) ? size->valueint : 0;
        const char *sha_str = sha && cJSON_IsString(sha) ? sha->valuestring : "-";

        char size_display[16];
        if (strcmp(type_str, "dir") == 0) {
            snprintf(size_display, sizeof(size_display), "-");
            dir_count++;
        } else {
            if (size_val < 1024) snprintf(size_display, sizeof(size_display), "%d B", size_val);
            else if (size_val < 1024 * 1024) snprintf(size_display, sizeof(size_display), "%.1f KB", size_val / 1024.0);
            else snprintf(size_display, sizeof(size_display), "%.1f MB", size_val / (1024.0 * 1024.0));
            file_count++;
            total_size += size_val;
        }

        char name_truncated[51];
        if (strlen(name_str) > 50) snprintf(name_truncated, sizeof(name_truncated), "%.47s...", name_str);
        else snprintf(name_truncated, sizeof(name_truncated), "%s", name_str);

        PRINT("%-50s %-10s %s\n", name_truncated, size_display, sha_str);
    }

    PRINT("\n");

    char total_size_display[32];
    if (total_size < 1024) snprintf(total_size_display, sizeof(total_size_display), "%zu B", total_size);
    else if (total_size < 1024 * 1024) snprintf(total_size_display, sizeof(total_size_display), "%.1f KB", total_size / 1024.0);
    else snprintf(total_size_display, sizeof(total_size_display), "%.1f MB", total_size / (1024.0 * 1024.0));

    PRINT("%d file(s), %d dir(s), total size: %s\n", file_count, dir_count, total_size_display);
    PRINT("\n");
}

void list(struct Command command) {

    PRINT("Username:   %s\n", command.username);
    PRINT("Repository: %s\n", command.repository);
    PRINT("path:       %s\n", command.path);
    PRINT("Branch:     %s\n", command.branch);
    PRINT("Token:      %s\n", command.token ? "(hidden)" : NULL);
    PRINT("\n");

    if (!command.token) printf("[WARN] %s command may required github personal access token to reach private repository\n", command.command_name);

    if (!command.path || *command.path == '\0') {
        PRINT_VERBOSE("Remote path is root\n");
        PRINT("Remote directory: %s\n", command.repository);
        PRINT("\n");

        struct Command root_command = command;
        root_command.path = "";

        cJSON *json_res = github_request("GET", command.username, command.repository, "", command.branch, command.token, NULL);
        if (json_res && cJSON_IsArray(json_res)) {
            list_directory(root_command, json_res);
            cJSON_Delete(json_res);
        } else if (json_res) {
            PRINT_ERROR("Failed to list root directory\n");
            cJSON_Delete(json_res);
        } else {
            PRINT_ERROR("Failed to access repository: %s\n", command.repository);
        }
        return;
    }

    cJSON *json_res = github_request( "GET", command.username, command.repository, command.path, command.branch, command.token, NULL);

    if (json_res == NULL) {
        PRINT_ERROR("Failed to fetch data from remote path '%s'\n", command.path);
        return;
    }

    if (cJSON_IsArray(json_res)) {
        PRINT_VERBOSE("Remote path is directory\n");
        PRINT("Remote directory: %s/%s\n", command.repository, command.path);
        PRINT("\n");
        list_directory(command, json_res);
    } else if (cJSON_IsObject(json_res)) {
        cJSON *message = cJSON_GetObjectItemCaseSensitive(json_res, "message");
        if (cJSON_IsString(message) && message->valuestring != NULL) {
            PRINT_ERROR("%s - %s\n", command.path, message->valuestring);
        } else {
            PRINT_VERBOSE("Remote path is file\n");
            PRINT("Remote file: %s\n", command.path);
            PRINT("\n");
            list_file(json_res);
        }
    } else {
        PRINT_ERROR("Unexpected JSON response type from GitHub API\n");
    }

    cJSON_Delete(json_res);
}
