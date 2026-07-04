#include "command.h"
#include "logger.h"
#include "api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <cjson/cJSON.h>

static char *base64_decode(const char *data, size_t *output_length) {
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO *mem = BIO_new_mem_buf(data, -1);
    b64 = BIO_push(b64, mem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

    size_t max_len = strlen(data) * 3 / 4 + 1;
    char *decoded = malloc(max_len);
    if (!decoded) {
        BIO_free_all(b64);
        return NULL;
    }

    *output_length = BIO_read(b64, decoded, max_len);
    BIO_free_all(b64);

    if (*output_length <= 0) {
        free(decoded);
        return NULL;
    }

    return decoded;
}

static void download_file(const struct Command command) {

    PRINT("Downloading %s ...\n", command.path);

    cJSON *file_info = github_request("GET", command.username, command.repository, command.path, command.branch, command.token, NULL);

    if (!file_info) {
        PRINT("✗ Error: Failed to get file info: %s\n", command.path);
        return;
    }

    cJSON *message = cJSON_GetObjectItem(file_info, "message");
    if (message && cJSON_IsString(message)) {
        PRINT("✗ Error: %s - %s\n", command.path, message->valuestring);
        cJSON_Delete(file_info);
        return;
    }

    cJSON *content = cJSON_GetObjectItem(file_info, "content");
    cJSON *encoding = cJSON_GetObjectItem(file_info, "encoding");
    cJSON *name = cJSON_GetObjectItem(file_info, "name");

    if (!content || !cJSON_IsString(content)) {
        PRINT("✗ Error: No content found for %s\n", command.path);
        cJSON_Delete(file_info);
        return;
    }

    if (!encoding || !cJSON_IsString(encoding) || strcmp(encoding->valuestring, "base64") != 0) {
        PRINT("✗ Error: Unsupported encoding for %s\n", command.path);
        cJSON_Delete(file_info);
        return;
    }

    size_t decoded_size;
    char *decoded_content = base64_decode(content->valuestring, &decoded_size);

    if (!decoded_content) {
        PRINT("✗ Error: Failed to decode file content\n");
        cJSON_Delete(file_info);
        return;
    }

    const char *filename;
    bool needs_free = false;

    if (command.local_path && *command.local_path) {
        struct stat path_stat;
        if (stat(command.local_path, &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
            const char *remote_name = name && cJSON_IsString(name) ? name->valuestring : "download";
            char *full_path = malloc(strlen(command.local_path) + strlen(remote_name) + 2);
            sprintf(full_path, "%s/%s", command.local_path, remote_name);
            filename = full_path;
            needs_free = true;
        } else {
            filename = command.local_path;
        }
    } else {
        filename = name && cJSON_IsString(name) ? name->valuestring : "download";
    }

    char *dir_path = strdup(filename);
    char *last_slash = strrchr(dir_path, '/');
    if (last_slash) {
        *last_slash = '\0';
        struct stat dir_stat;
        if (stat(dir_path, &dir_stat) != 0) {
            char *p = dir_path;
            while (*p) {
                if (*p == '/') {
                    *p = '\0';
                    mkdir(dir_path, 0755);
                    *p = '/';
                }
                p++;
            }
            mkdir(dir_path, 0755);
        }
    }
    free(dir_path);

    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        PRINT("✗ Error: Failed to create file: %s\n", filename);
        free(decoded_content);
        cJSON_Delete(file_info);
        if (needs_free) free((void *)filename);
        return;
    }

    size_t written = fwrite(decoded_content, 1, decoded_size, fp);
    fclose(fp);

    if (written != decoded_size) {
        PRINT("✗ Error: Failed to write file: %s\n", filename);
        remove(filename);
    } else {
        cJSON *size = cJSON_GetObjectItem(file_info, "size");

        PRINT("✓ Success : %s\n", filename);

        if (size && cJSON_IsNumber(size)) {
            char size_display[16];
            if (size->valueint < 1024) snprintf(size_display, sizeof(size_display), "%d B", size->valueint);
            else if (size->valueint < 1024 * 1024) snprintf(size_display, sizeof(size_display), "%.1f KB", size->valueint / 1024.0);
            else snprintf(size_display, sizeof(size_display), "%.1f MB", size->valueint / (1024.0 * 1024.0));
            PRINT("  Size     : %s\n", size_display);
        }
    }

    if (needs_free) free((void *)filename);
    free(decoded_content);
    cJSON_Delete(file_info);

    PRINT("\n");
}

static void download_dir(const struct Command command, cJSON *dir_content) {

    PRINT_VERBOSE("Downloading directory: %s\n", command.path);

    if (!dir_content) {
        PRINT("✗ Error: Failed to list directory: %s\n", command.path);
        return;
    }

    if (!cJSON_IsArray(dir_content)) {
        cJSON *message = cJSON_GetObjectItem(dir_content, "message");
        if (message && cJSON_IsString(message))
            PRINT("✗ Error: %s - %s\n", command.path, message->valuestring);
        cJSON_Delete(dir_content);
        return;
    }

    const char *local_dir;
    if (command.local_path && *command.local_path) {
        local_dir = command.local_path;
    } else {
        if (command.path && *command.path) {
            const char *last_slash = strrchr(command.path, '/');
            local_dir = last_slash ? last_slash + 1 : command.path;
        } else {
            local_dir = command.repository;
        }
    }

    struct stat path_stat;
    if (stat(local_dir, &path_stat) != 0) {
        if (mkdir(local_dir, 0755) != 0) {
            PRINT("✗ Error: Failed to create directory: %s\n", local_dir);
            cJSON_Delete(dir_content);
            return;
        }
        PRINT_VERBOSE("Created directory: %s\n", local_dir);
    }

    int file_count = 0;
    cJSON *item;
    cJSON_ArrayForEach(item, dir_content) {
        cJSON *type = cJSON_GetObjectItem(item, "type");
        cJSON *name = cJSON_GetObjectItem(item, "name");

        if (!type || !cJSON_IsString(type) || !name || !cJSON_IsString(name))
            continue;

        const char *remote_name = name->valuestring;

        char remote_full_path[1024];
        if (command.path && *command.path)
            snprintf(remote_full_path, sizeof(remote_full_path), "%s/%s", command.path, remote_name);
        else
            snprintf(remote_full_path, sizeof(remote_full_path), "%s", remote_name);

        char local_full_path[1024];
        snprintf(local_full_path, sizeof(local_full_path), "%s/%s", local_dir, remote_name);

        struct Command file_command = command;
        file_command.path = remote_full_path;
        file_command.local_path = local_full_path;

        if (strcmp(type->valuestring, "dir") == 0) {
            cJSON *sub_content = github_request("GET", command.username, command.repository, remote_full_path, command.branch, command.token, NULL);
            download_dir(file_command, sub_content);
        } else if (strcmp(type->valuestring, "file") == 0) {
            download_file(file_command);
            file_count++;
        }
    }

    if (file_count > 0) {
        PRINT("Downloaded %d file(s) to %s\n\n", file_count, local_dir);
    }

    cJSON_Delete(dir_content);
}

static void download_repo(const struct Command command) {

    PRINT("Downloading entire repository: %s/%s\n\n", command.username, command.repository);
    const char *local_dir;
    if (command.local_path && *command.local_path) {
        local_dir = command.local_path;
    } else {
        local_dir = command.repository;
    }

    struct stat path_stat;
    if (stat(local_dir, &path_stat) != 0) {
        if (mkdir(local_dir, 0755) != 0) {
            PRINT("✗ Error: Failed to create directory: %s\n", local_dir);
            return;
        }
        PRINT_VERBOSE("Created directory: %s\n", local_dir);
    }

    cJSON *sha_map = build_sha_map(command.username, command.repository, command.branch, command.token);
    if (!sha_map) {
        PRINT_ERROR("Failed to fetch repository tree\n");
        return;
    }

    int file_count = 0;
    int total_files = cJSON_GetArraySize(sha_map);

    PRINT("Found %d file(s) in repository\n\n", total_files);

    cJSON *item;
    cJSON_ArrayForEach(item, sha_map) {
        if (item && item->string) {
            char local_full_path[1024];
            snprintf(local_full_path, sizeof(local_full_path), "%s/%s", local_dir, item->string);

            struct Command file_command = command;
            file_command.path = item->string;
            file_command.local_path = local_full_path;

            download_file(file_command);
            file_count++;
        }
    }

    PRINT("Downloaded %d file(s) to %s\n", file_count, local_dir);
    cJSON_Delete(sha_map);
}

void download(const struct Command command) {

    PRINT("Username:   %s\n", command.username);
    PRINT("Repository: %s\n", command.repository);
    PRINT("path:       %s\n", command.path);
    PRINT("Branch:     %s\n", command.branch);
    PRINT("Token:      %s\n", command.token ? "(hidden)" : NULL);
    PRINT("\n");

    if (!command.token) {
        printf("Warning: %s command maybe required github personal access token to reach private repository\n", command.command_name);
    }

    if (!command.path || *command.path == '\0') {
        PRINT_VERBOSE("Remote path is empty, downloading entire repository\n");
        download_repo(command);
        return;
    }

    cJSON *remote_info = github_request("GET", command.username, command.repository, command.path, command.branch, command.token, NULL);

    if (remote_info) {
        if (cJSON_IsArray(remote_info)) {
            PRINT_VERBOSE("Remote path is directory\n");
            PRINT("Remote directory: %s/%s\n\n", command.repository, command.path);
            download_dir(command, remote_info);
        } else {
            cJSON *message = cJSON_GetObjectItem(remote_info, "message");
            if (message && cJSON_IsString(message)) {
                PRINT("✗ Error: %s - %s\n", command.path, message->valuestring);
            } else {
                PRINT_VERBOSE("Remote path is file\n");
                PRINT("Remote file: %s\n\n", command.path);
                download_file(command);
            }
            cJSON_Delete(remote_info);
        }
    } else {
        PRINT_ERROR("Failed to access remote path: %s\n", command.path);
    }
}
