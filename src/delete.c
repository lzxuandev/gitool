#include "command.h"
#include "logger.h"
#include "api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

static void delete_file(const struct Command command, cJSON *sha_map) {

    char *sha = NULL;
    cJSON *sha_item = cJSON_GetObjectItem(sha_map, command.path);
    if (sha_item && cJSON_IsString(sha_item)) {
        sha = strdup(sha_item->valuestring);
    }

    if (!sha && cJSON_GetArraySize(sha_map) == 0) {
        cJSON *existing_file = github_request("GET", command.username, command.repository, command.path, command.branch, command.token, NULL);
        if (existing_file) {
            cJSON *sha_item_existing = cJSON_GetObjectItem(existing_file, "sha");
            if (sha_item_existing && cJSON_IsString(sha_item_existing)) {
                sha = strdup(sha_item_existing->valuestring);
            }
            cJSON_Delete(existing_file);
        }
    }

    if (!sha) {
        PRINT("✗ Error: File not found on remote: %s\n", command.path);
        return;
    }

    PRINT("Deleting %s ...\n", command.path);

    cJSON *json_data = cJSON_CreateObject();
    cJSON_AddStringToObject(json_data, "message", "Delete file via gitool");
    cJSON_AddStringToObject(json_data, "sha", sha);

    char *json_str = cJSON_PrintUnformatted(json_data);
    cJSON_Delete(json_data);
    free(sha);

    cJSON *response = github_request("DELETE", command.username, command.repository, command.path, command.branch, command.token, json_str);
    free(json_str);

    if (response) {
        cJSON *commit = cJSON_GetObjectItem(response, "commit");
        if (commit) {
            cJSON *commit_sha = cJSON_GetObjectItem(commit, "sha");
            PRINT("✓ Success : %s\n", command.path);
            if (commit_sha && cJSON_IsString(commit_sha)) PRINT("  Commit  : %s\n", commit_sha->valuestring);

            cJSON_DeleteItemFromObject(sha_map, command.path);
        } else {
            cJSON *message = cJSON_GetObjectItem(response, "message");
            if (message && cJSON_IsString(message)) PRINT("✗ Error  : %s - %s\n", command.path, message->valuestring);
            else PRINT("✓ Success : %s\n", command.path);
        }
        cJSON_Delete(response);
    } else PRINT("✗ Error  : %s - No response from server\n", command.path);

    PRINT("\n");
}

static void delete_dir(const struct Command command, cJSON *sha_map) {

    cJSON *files_to_delete = cJSON_CreateArray();
    if (!files_to_delete) return;

    size_t dir_path_len = strlen(command.path);

    cJSON *item;
    cJSON_ArrayForEach(item, sha_map) {
        if (item && item->string) {
            if (strncmp(item->string, command.path, dir_path_len) == 0 &&
                (item->string[dir_path_len] == '/' || item->string[dir_path_len] == '\0')) {
                cJSON_AddItemToArray(files_to_delete, cJSON_CreateString(item->string));
            }
        }
    }

    int total = cJSON_GetArraySize(files_to_delete);
    if (total == 0) {
        PRINT("No files found in remote directory: %s\n", command.path);
        cJSON_Delete(files_to_delete);
        PRINT("\n");
        return;
    }

    PRINT("Deleting %d file(s) from %s ...\n\n", total, command.path);

    for (int i = 0; i < total; i++) {
        cJSON *path_item = cJSON_GetArrayItem(files_to_delete, i);
        if (path_item && cJSON_IsString(path_item)) {
            struct Command file_command = command;
            file_command.path = path_item->valuestring;
            delete_file(file_command, sha_map);
        }
    }

    cJSON_Delete(files_to_delete);
}

void delete(const struct Command command) {

    PRINT("Username:   %s\n", command.username);
    PRINT("Repository: %s\n", command.repository);
    PRINT("path:       %s\n", command.path);
    PRINT("Branch:     %s\n", command.branch);
    PRINT("Token:      %s\n", command.token ? "(hidden)" : NULL);
    PRINT("\n");

    if (!command.token) {
        printf("Warning: %s command required github personal access token to reach '%s' repository\n", command.command_name, command.repository);
        return;
    }

    cJSON *sha_map = build_sha_map(command.username, command.repository, command.branch, command.token);
    if (!sha_map) {
        PRINT_ERROR("Failed to fetch SHA map, continuing without change detection\n");
        sha_map = cJSON_CreateObject();
    }

    if (!command.path || *command.path == '\0') {
        PRINT_ERROR("Remote path is required for delete operation\n");
        cJSON_Delete(sha_map);
        return;
    }

    cJSON *remote_info = github_request("GET", command.username, command.repository, command.path, command.branch, command.token, NULL);

    if (remote_info) {
        if (cJSON_IsArray(remote_info)) {
            PRINT_VERBOSE("Remote path is directory\n");
            PRINT("Remote directory: %s/%s\n\n", command.repository, command.path);
            cJSON_Delete(remote_info);
            delete_dir(command, sha_map);
        } else {
            PRINT_VERBOSE("Remote path is file\n");
            PRINT("Remote file: %s\n\n", command.path);
            cJSON_Delete(remote_info);
            delete_file(command, sha_map);
        }
    } else {
        PRINT_ERROR("Cannot determine remote path type, trying as file\n");
        delete_file(command, sha_map);
    }

    cJSON_Delete(sha_map);
}
