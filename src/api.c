#include "api.h"
#include "logger.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

#define API "https://api.github.com"
#define MAX_URL 1024
#define LOG_TRUNCATE_LEN 200

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t newsize = size * nmemb;
    struct MemoryBuffer *mem = (struct MemoryBuffer *)userp;

    PRINT_VERBOSE("write_callback: received %zu bytes, total so far: %zu\n", newsize, mem->size);

    char *ptr = realloc(mem->memory, mem->size + newsize + 1);
    if (!ptr) {
        PRINT_ERROR("write_callback: realloc failed (%zu bytes)\n", mem->size + newsize + 1);
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, newsize);
    mem->size += newsize;
    mem->memory[mem->size] = '\0';

    PRINT_VERBOSE("write_callback: appended data, new total: %zu bytes\n", mem->size);

    return newsize;
}

static bool build_url(char *url, size_t size, const char *method, const char *username, const char *repo, const char *remote_path, const char *branch) {
    PRINT_VERBOSE("build_url: method=%s user=%s repo=%s path=%s branch=%s\n", method?method:"NULL", username?username:"NULL", repo?repo:"NULL", remote_path?remote_path:"NULL", branch?branch:"NULL");

    if (remote_path && (strncmp(remote_path, "http://", 7) == 0 || strncmp(remote_path, "https://", 8) == 0)) {
        snprintf(url, size, "%s", remote_path);
        PRINT_VERBOSE("build_url: full URL: %s\n", url);
        return true;
    }

    if (method && strcmp(method, "GET_TREE") == 0) {
        snprintf(url, size, "%s/repos/%s/%s/git/trees/%s?recursive=1", API, username, repo, branch);
        PRINT_VERBOSE("build_url: tree URL: %s\n", url);
        return true;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        PRINT_ERROR("build_url: curl init failed\n");
        return false;
    }
    char *encoded = curl_easy_escape(curl, remote_path ? remote_path : "", 0);

    if (encoded) {
        if (branch && *branch) snprintf(url, size, "%s/repos/%s/%s/contents/%s?ref=%s", API, username, repo, encoded, branch);
        else snprintf(url, size, "%s/repos/%s/%s/contents/%s", API, username, repo, encoded);

        PRINT_VERBOSE("build_url: content URL: %s\n", url);

        curl_free(encoded);
    } else {
        PRINT_ERROR("build_url: escape failed for: %s\n", remote_path?remote_path:"(empty)");
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_cleanup(curl);

    if (url[0] == '\0') {
        PRINT_ERROR("build_url: empty URL\n");
        return false;
    }

    return true;
}

static struct curl_slist *build_headers(const char *token) {
    PRINT_VERBOSE("build_headers: token %s\n", token ? "provided" : "not provided");

    struct curl_slist *headers = NULL;

    if (token != NULL && strlen(token) > 0) {
        char auth_header[512];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", token);
        headers = curl_slist_append(headers, auth_header);
        PRINT_VERBOSE("build_headers: added auth header\n");
    }

    headers = curl_slist_append(headers, "Accept: application/vnd.github+json");
    headers = curl_slist_append(headers, "User-Agent: gitool");

    return headers;
}

cJSON *github_request(const char *method, const char *username, const char *repo, const char *remote_path, const char *branch, const char *token, const char *json_data) {

    PRINT_VERBOSE("github_request: method=%s user=%s repo=%s path=%s branch=%s token=%s json=%s\n", method?method:"NULL", username?username:"NULL", repo?repo:"NULL", remote_path?remote_path:"NULL", branch?branch:"NULL", token?"yes":"no", json_data?"yes":"no");

    char url[MAX_URL];

    if (!build_url(url, sizeof(url), method, username, repo, remote_path, branch)) {
        PRINT_ERROR("github_request: build_url failed\n");
        return NULL;
    }

    PRINT_VERBOSE("github_request: URL: %s\n", url);

    CURL *curl = curl_easy_init();

    if (!curl) {
        PRINT_ERROR("github_request: curl init failed\n");
        return NULL;
    }

    struct MemoryBuffer *response = malloc(sizeof(struct MemoryBuffer));

    if (!response) {
        PRINT_ERROR("github_request: response alloc failed\n");
        curl_easy_cleanup(curl);
        return NULL;
    }
    response->memory = malloc(1);

    if (!response->memory) { PRINT_ERROR("github_request: memory alloc failed\n"); free(response); curl_easy_cleanup(curl); return NULL; }

    response->memory[0] = '\0';
    response->size = 0;

    struct curl_slist *headers = build_headers(token);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    if (method && strcmp(method, "GET") != 0 && strcmp(method, "GET_TREE") != 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
        PRINT_VERBOSE("github_request: custom method: %s\n", method);
    }
    if (json_data) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
        PRINT_VERBOSE("github_request: post data len: %zu\n", strlen(json_data));
    }

    PRINT_VERBOSE("github_request: executing %s\n", method?method:"GET");

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        PRINT_ERROR("github_request: %s failed: %s (code %d)\n", method?method:"GET", curl_easy_strerror(res), res);
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (http_code) PRINT_ERROR("github_request: HTTP %ld\n", http_code);

        free(response->memory);
        free(response);

        response = NULL;
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        PRINT_VERBOSE("github_request: HTTP %ld, size %zu\n", http_code, response->size);

        if (http_code < 200 || http_code >= 300) {
            PRINT_ERROR("github_request: HTTP error %ld\n", http_code);
            if (response && response->size > 0) {
                char *trunc = response->memory;
                if (strlen(trunc) > LOG_TRUNCATE_LEN) {
                    char buf[LOG_TRUNCATE_LEN+4];
                    snprintf(buf, sizeof(buf), "%.*s...", LOG_TRUNCATE_LEN, trunc);
                    PRINT_ERROR("github_request: body: %s\n", buf);
                }
                else PRINT_ERROR("github_request: body: %s\n", trunc);
            }
        }
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    cJSON *json = NULL;
    if (response) {
        PRINT_VERBOSE("github_request: parsing JSON (size %zu)\n", response->size);
        if (response->size > 0) {
            char *trunc = response->memory;
            if (strlen(trunc) > LOG_TRUNCATE_LEN) {
                char buf[LOG_TRUNCATE_LEN+4];
                snprintf(buf, sizeof(buf), "%.*s...", LOG_TRUNCATE_LEN, trunc);
                PRINT_VERBOSE("github_request: preview: %s\n", buf);
            }
            else PRINT_VERBOSE("github_request: preview: %s\n", trunc);
        } else PRINT_VERBOSE("github_request: empty response\n");
        json = cJSON_Parse(response->memory);

        if (!json) {
            const char *err = cJSON_GetErrorPtr();
            PRINT_ERROR("github_request: JSON parse failed: %s\n", err?err:"unknown");

            char *trunc = response->memory;

            if (strlen(trunc) > LOG_TRUNCATE_LEN) {
                char buf[LOG_TRUNCATE_LEN+4];
                snprintf(buf, sizeof(buf), "%.*s...", LOG_TRUNCATE_LEN, trunc);
                PRINT_ERROR("github_request: invalid JSON: %s\n", buf);
            }
            else PRINT_ERROR("github_request: invalid JSON: %s\n", trunc);
        } else PRINT_VERBOSE("github_request: JSON parsed OK\n");
        free(response->memory);
        free(response);
    }
    return json;
}

cJSON *build_sha_map(const char *username, const char *repo, const char *branch, const char *token) {
    PRINT_VERBOSE("build_sha_map: user=%s repo=%s branch=%s token=%s\n", username?username:"NULL", repo?repo:"NULL", branch?branch:"NULL", token?"yes":"no");

    if (!username || !repo || !branch) {
        PRINT_ERROR("build_sha_map: missing params\n");
        return NULL;
    }

    cJSON *tree_json = github_request("GET_TREE", username, repo, NULL, branch, token, NULL);

    if (!tree_json) { PRINT_ERROR("build_sha_map: tree request failed\n"); return NULL; }
    cJSON *tree_array = cJSON_GetObjectItem(tree_json, "tree");

    if (!tree_array || !cJSON_IsArray(tree_array)) {
        PRINT_ERROR("build_sha_map: tree missing/invalid\n");
        cJSON_Delete(tree_json);
        return NULL;
    }

    int tree_size = cJSON_GetArraySize(tree_array);
    PRINT_VERBOSE("build_sha_map: tree size: %d\n", tree_size);

    cJSON *sha_map = cJSON_CreateObject();

    if (!sha_map) {
        PRINT_ERROR("build_sha_map: create map failed\n"); cJSON_Delete(tree_json);
        return NULL;
    }

    int blob_count = 0;

    cJSON *item;
    cJSON_ArrayForEach(item, tree_array) {
        cJSON *type = cJSON_GetObjectItem(item, "type");
        cJSON *path = cJSON_GetObjectItem(item, "path");
        cJSON *sha = cJSON_GetObjectItem(item, "sha");

        if (type && cJSON_IsString(type) && strcmp(type->valuestring, "blob") == 0 && path && cJSON_IsString(path) && sha && cJSON_IsString(sha)) {
            cJSON_AddStringToObject(sha_map, path->valuestring, sha->valuestring);
            blob_count++;
            if (blob_count <= 5) PRINT_VERBOSE("build_sha_map: blob: %s -> %s\n", path->valuestring, sha->valuestring);
        }
    }

    PRINT_VERBOSE("build_sha_map: mapped %d blobs\n", blob_count);

    cJSON_Delete(tree_json);
    return sha_map;
}
