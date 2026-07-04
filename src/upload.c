#include "command.h"
#include "logger.h"
#include "api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <cjson/cJSON.h>

static char *base64_encode(const unsigned char *data, size_t input_length) {
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO *mem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, mem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, data, input_length);
    BIO_flush(b64);

    BUF_MEM *buffer;
    BIO_get_mem_ptr(b64, &buffer);
    char *result = strndup(buffer->data, buffer->length);
    BIO_free_all(b64);
    return result;
}

static char *read_file(const char *filepath, size_t *file_size) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) return NULL;

    fseek(fp, 0, SEEK_END);
    *file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *content = malloc(*file_size);
    if (!content) {
        fclose(fp);
        return NULL;
    }

    fread(content, 1, *file_size, fp);
    fclose(fp);

    return content;
}

char* git_blob_sha(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        perror("gitools error: Cannot open file for SHA calculation");
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *sha_str = calloc(41, sizeof(char));
    if (!sha_str) {
        fclose(file);
        return NULL;
    }

    char header[64];
    int header_len = snprintf(header, sizeof(header), "blob %ld", size) + 1;

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(mdctx, EVP_sha1(), NULL);
    EVP_DigestUpdate(mdctx, header, header_len);

    char buffer[4096];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        EVP_DigestUpdate(mdctx, buffer, bytes_read);
    }
    fclose(file);

    unsigned char hash[SHA_DIGEST_LENGTH];
    unsigned int hash_len;
    EVP_DigestFinal_ex(mdctx, hash, &hash_len);
    EVP_MD_CTX_free(mdctx);

    for (unsigned int i = 0; i < hash_len; i++) {
        snprintf(sha_str + (i * 2), 3, "%02x", hash[i]);
    }

    return sha_str;
}

static void upload_file(const struct Command command, cJSON *sha_map) {

    size_t file_size;
    char *file_content = read_file(command.local_path, &file_size);
    if (!file_content) {
        PRINT("✗ Error: Failed to read local file\n");
        return;
    }

    char *encoded_content = base64_encode((unsigned char *)file_content, file_size);
    if (!encoded_content) {
        PRINT("✗ Error: Failed to encode file\n");
        free(file_content);
        return;
    }

    char *sha = NULL;
    cJSON *sha_item = cJSON_GetObjectItem(sha_map, command.path);
    if (sha_item && cJSON_IsString(sha_item)) {
        sha = strdup(sha_item->valuestring);
    }

    char *local_sha = git_blob_sha(command.local_path);

    if (!sha && cJSON_GetArraySize(sha_map) == 0) {
        cJSON *existing_file = github_request("GET", command.username, command.repository, command.path, command.branch, command.token, NULL);
        if (existing_file) {
            cJSON *sha_item_existing = cJSON_GetObjectItem(existing_file, "sha");
            if (sha_item_existing && cJSON_IsString(sha_item_existing)) sha = strdup(sha_item_existing->valuestring);
            cJSON_Delete(existing_file);
        }
    }

    if (sha && local_sha && strcmp(sha, local_sha) == 0) {
        PRINT("Skipping file: %s (unchanged)\n", command.path);
        free(sha);
        free(local_sha);
        free(encoded_content);
        free(file_content);
        return;
    }

    free(local_sha);

    if (sha) PRINT("Updating %s ...\n", command.path);
    else PRINT("Creating %s ...\n", command.path);

    cJSON *json_data = cJSON_CreateObject();
    cJSON_AddStringToObject(json_data, "message", sha ? "Update file via gitool" : "Create file via gitool");
    cJSON_AddStringToObject(json_data, "content", encoded_content);
    if (sha) cJSON_AddStringToObject(json_data, "sha", sha);

    char *json_str = cJSON_PrintUnformatted(json_data);
    cJSON_Delete(json_data);
    free(encoded_content);
    free(sha);

    cJSON *response = github_request("PUT", command.username, command.repository, command.path, command.branch, command.token, json_str);
    free(json_str);

    if (response) {
        cJSON *content = cJSON_GetObjectItem(response, "content");
        if (content) {
            cJSON *commit = cJSON_GetObjectItem(response, "commit");

            cJSON *new_sha = cJSON_GetObjectItem(content, "sha");
            if (new_sha && cJSON_IsString(new_sha)) {
                cJSON_AddStringToObject(sha_map, command.path, new_sha->valuestring);
            }

            if (commit) {
                cJSON *commit_sha = cJSON_GetObjectItem(commit, "sha");
                PRINT("✓ Success : %s\n", command.path);
                if (commit_sha && cJSON_IsString(commit_sha)) {
                    PRINT("  Commit  : %s\n", commit_sha->valuestring);
                }
            } else {
                PRINT("✓ Success: %s\n", command.path);
            }
        } else {
            cJSON *message = cJSON_GetObjectItem(response, "message");
            if (message && cJSON_IsString(message)) {
                PRINT("✗ Error  : %s - %s\n", command.path, message->valuestring);
            }
        }
        cJSON_Delete(response);
    } else {
        PRINT("✗ Error  : %s - No response from server\n", command.path);
    }

    PRINT("\n");
    free(file_content);
}

static void upload_dir(const struct Command command, cJSON *sha_map) {
    DIR *dir = opendir(command.local_path);

    if (!dir) {
        PRINT_ERROR("Failed to open directory: %s\n", command.local_path);
        return;
    }

    PRINT_VERBOSE("Uploading directory: %s -> %s\n", command.local_path, *command.path ? command.path : "(root)");

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char local_fullpath[1024];
        char remote_fullpath[1024];

        snprintf(local_fullpath, sizeof(local_fullpath), "%s/%s", command.local_path, entry->d_name);

        if (command.path == NULL || *command.path == '\0' ||
            strcmp(command.path, "/") == 0) {
            snprintf(remote_fullpath, sizeof(remote_fullpath), "%s", entry->d_name);
        } else {
            snprintf(remote_fullpath, sizeof(remote_fullpath), "%s/%s", command.path, entry->d_name);
        }

        struct stat st;
        if (stat(local_fullpath, &st) != 0) {
            PRINT_ERROR("Failed to stat: %s\n", local_fullpath);
            continue;
        }

        struct Command file_command = command;
        file_command.local_path = local_fullpath;
        file_command.path = remote_fullpath;

        if (S_ISDIR(st.st_mode)) {
            upload_dir(file_command, sha_map);
        } else if (S_ISREG(st.st_mode)) {
            upload_file(file_command, sha_map);
        } else {
            PRINT_VERBOSE("Skipping special file: %s\n", local_fullpath);
        }
    }

    closedir(dir);
}

void upload(const struct Command command){

    PRINT("Username:   %s\n", command.username);
    PRINT("Repository: %s\n", command.repository);
    PRINT("path:       %s\n", command.path);
    PRINT("Branch:     %s\n", command.branch);
    PRINT("Token:      %s\n", command.token ? "(hidden)" : NULL);
    PRINT("\n");

    if (!command.token) {
        printf("Warning: %s command required github personal access token to reach '%s' repository\n", command.command_name, command.repository);
        return ;
    }

    cJSON *sha_map = build_sha_map(command.username, command.repository, command.branch,command.token);

    if (!sha_map) {
        PRINT_ERROR("Failed to fetch SHA map, continuing without change detection\n");
        sha_map = cJSON_CreateObject();
    }

    struct stat path_stat;

    if (stat(command.local_path, &path_stat) != 0) {
        log_error("Failed to access path: %s\n", command.local_path);
        return;
    }

    if (S_ISREG(path_stat.st_mode)) {
        PRINT_VERBOSE("Local path is file\n");

        PRINT("Local file:  %s\n", command.local_path);
        PRINT("Remote file: %s\n", command.path);

        PRINT("\n");
        upload_file(command, sha_map);
    } else if (S_ISDIR(path_stat.st_mode)) {
        PRINT_VERBOSE("Local path is Directory\n");

        PRINT("Local directory: %s\n", command.local_path);
        PRINT("Remote directory: %s/%s\n", command.repository, *command.path ? command.path : "root");

        PRINT("\n");
        upload_dir(command, sha_map);
    } else {
        PRINT_ERROR("Unknown file type\n");
        exit(EXIT_FAILURE);
    }

}
