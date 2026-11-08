// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file qualify_temporal_resources.cpp
 * @brief Verify and install independently published clock/calendar CI fixtures.
 *
 * Keep this release gate in native C++ and treat every network response as
 * untrusted until its identity, length, and SHA-256 are independently checked.
 * It must never silently skip a failed provider qualification.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include <curl/curl.h>
#include <json-c/json.h>
#include <openssl/sha.h>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {
constexpr size_t maximum_bytes = 128U * 1024U * 1024U;
constexpr int attempts = 4;
using Bytes = std::vector<unsigned char>;

struct HttpResult {
    Bytes body;
    long status = 0;
    CURLcode error = CURLE_OK;
};

size_t receive(char *data, size_t unit, size_t count, void *context)
{
    if (unit != 0U && count > SIZE_MAX / unit) return 0U;
    const size_t length = unit * count;
    auto *buffer = static_cast<Bytes *>(context);
    if (length > maximum_bytes - buffer->size()) return 0U;
    const auto *first = reinterpret_cast<unsigned char *>(data);
    buffer->insert(buffer->end(), first, first + length);
    return length;
}

bool transient(long status, CURLcode error)
{
    if (error != CURLE_OK) return true;
    return status == 408L || status == 429L || status == 500L ||
           status == 502L || status == 503L || status == 504L;
}

Bytes fetch_https(const std::string &url, const std::string &accept)
{
    const char *token = std::getenv("GITHUB_TOKEN");
    for (int attempt = 0; attempt < attempts; ++attempt) {
        CURL *handle = curl_easy_init();
        if (!handle) throw std::runtime_error("Cannot initialise HTTPS client");
        curl_slist *headers = nullptr;
        headers = curl_slist_append(headers, "User-Agent: System-Monitor-provider-check");
        headers = curl_slist_append(headers, ("Accept: " + accept).c_str());
        if (token && *token)
            headers = curl_slist_append(headers, ("Authorization: Bearer " + std::string(token)).c_str());
        HttpResult result;
        curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
        curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(handle, CURLOPT_PROTOCOLS_STR, "https");
        curl_easy_setopt(handle, CURLOPT_REDIR_PROTOCOLS_STR, "https");
        curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 20L);
        curl_easy_setopt(handle, CURLOPT_TIMEOUT, 90L);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, receive);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &result.body);
        result.error = curl_easy_perform(handle);
        curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &result.status);
        curl_slist_free_all(headers);
        curl_easy_cleanup(handle);
        if (result.error == CURLE_OK && result.status == 200L)
            return std::move(result.body);
        if (!transient(result.status, result.error) || attempt + 1 == attempts)
            throw std::runtime_error("HTTPS provider request failed after verified retries: " +
                                     url + " (HTTP " + std::to_string(result.status) + ")");
        std::fprintf(stderr, "Transient provider download fault (HTTP %ld), retry %d/%d\n",
                     result.status, attempt + 2, attempts);
        std::this_thread::sleep_for(std::chrono::seconds(1 << attempt));
    }
    throw std::runtime_error("Provider HTTPS retry limit exceeded");
}

json_object *required(json_object *object, const char *key)
{
    json_object *value = nullptr;
    if (!object || !json_object_object_get_ex(object, key, &value) || !value)
        throw std::runtime_error(std::string("Missing release metadata: ") + key);
    return value;
}

std::string string_field(json_object *object, const char *key)
{
    json_object *value = required(object, key);
    if (!json_object_is_type(value, json_type_string))
        throw std::runtime_error(std::string("Invalid release string: ") + key);
    return json_object_get_string(value);
}

int64_t number_field(json_object *object, const char *key)
{
    json_object *value = required(object, key);
    if (!json_object_is_type(value, json_type_int))
        throw std::runtime_error(std::string("Invalid release integer: ") + key);
    return json_object_get_int64(value);
}

std::string sha256_hex(const Bytes &body)
{
    std::array<unsigned char, SHA256_DIGEST_LENGTH> digest{};
    SHA256(body.data(), body.size(), digest.data());
    const char *hex = "0123456789abcdef";
    std::string output;
    output.reserve(digest.size() * 2U);
    for (const unsigned char byte : digest) {
        output.push_back(hex[byte >> 4U]);
        output.push_back(hex[byte & 15U]);
    }
    return output;
}

void install_verified(const std::string &filename, const Bytes &body)
{
    char path[] = "/tmp/system-monitor-provider-XXXXXX";
    char *folder = mkdtemp(path);
    if (!folder) throw std::runtime_error("Cannot create private provider staging");
    const std::filesystem::path directory(folder);
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code error; std::filesystem::remove_all(path, error); }
    } cleanup{directory};
    if (chmod(folder, 0755) != 0)
        throw std::runtime_error("Cannot permit system package manager to access staging");
    const std::filesystem::path destination = directory / filename;
    int file = open(destination.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0644);
    if (file < 0) throw std::runtime_error("Cannot open verified package staging");
    size_t offset = 0U;
    while (offset < body.size()) {
        const ssize_t written = write(file, body.data() + offset, body.size() - offset);
        if (written <= 0) {
            close(file);
            throw std::runtime_error("Cannot write verified package");
        }
        offset += static_cast<size_t>(written);
    }
    if (close(file) != 0) throw std::runtime_error("Cannot close verified package");
    const pid_t pid = fork();
    if (pid < 0) throw std::runtime_error("Cannot launch package verification");
    if (pid == 0) {
        execl("/usr/bin/sudo", "sudo", "/usr/bin/dpkg", "--install",
              destination.c_str(), static_cast<char *>(nullptr));
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
        throw std::runtime_error("Verified independent provider failed to install");
}

void qualify(const std::string &repository)
{
    const std::string api = "https://api.github.com/repos/Infiltrator-Projects/" +
                            repository + "/releases/latest";
    const Bytes metadata = fetch_https(api, "application/vnd.github+json");
    json_object *parsed = json_tokener_parse(
        std::string(metadata.begin(), metadata.end()).c_str());
    if (!parsed) throw std::runtime_error("Release metadata is not JSON");
    const std::unique_ptr<json_object, decltype(&json_object_put)> release(parsed, &json_object_put);
    const std::string tag = string_field(parsed, "tag_name");
    if (!std::regex_match(tag, std::regex(R"(^v[0-9]+\.[0-9]+\.[0-9]+$)")))
        throw std::runtime_error("Unqualified provider version tag");
    const std::string filename = "infiltrator-" + [&repository] {
        std::string lowered = repository;
        for (char &value : lowered)
            if (value >= 'A' && value <= 'Z') value = static_cast<char>(value - 'A' + 'a');
        return lowered;
    }() + "_" + tag.substr(1) + "_amd64.deb";
    json_object *assets = required(parsed, "assets");
    if (!json_object_is_type(assets, json_type_array))
        throw std::runtime_error("Release assets are not an array");
    json_object *asset = nullptr;
    for (int i = 0; i < json_object_array_length(assets); ++i) {
        json_object *candidate = json_object_array_get_idx(assets, i);
        if (string_field(candidate, "name") == filename) {
            if (asset) throw std::runtime_error("Duplicate published provider package");
            asset = candidate;
        }
    }
    if (!asset) throw std::runtime_error("Published provider package is absent");
    const std::string expected = "https://github.com/Infiltrator-Projects/" +
                                 repository + "/releases/download/" + tag + "/" + filename;
    if (string_field(asset, "browser_download_url") != expected)
        throw std::runtime_error("Provider asset has unexpected source URL");
    const std::string digest = string_field(asset, "digest");
    if (!std::regex_match(digest, std::regex(R"(^sha256:[0-9a-f]{64}$)")))
        throw std::runtime_error("Provider asset has no trusted SHA-256");
    const int64_t length = number_field(asset, "size");
    if (length <= 0 || static_cast<uint64_t>(length) > maximum_bytes)
        throw std::runtime_error("Published provider asset size is invalid");
    Bytes bytes;
    try {
        bytes = fetch_https(expected, "application/octet-stream");
    } catch (const std::runtime_error &) {
        const int64_t id = number_field(asset, "id");
        if (id <= 0) throw std::runtime_error("Provider asset has invalid REST ID");
        std::fprintf(stderr, "Using verified REST fallback for %s\n", filename.c_str());
        bytes = fetch_https("https://api.github.com/repos/Infiltrator-Projects/" +
                            repository + "/releases/assets/" + std::to_string(id),
                            "application/octet-stream");
    }
    if (bytes.size() != static_cast<size_t>(length) || sha256_hex(bytes) != digest.substr(7))
        throw std::runtime_error("Provider package SHA-256 or length mismatch");
    std::printf("Installing SHA-256-verified %s\n", filename.c_str());
    install_verified(filename, bytes);
}
} // namespace

int main()
{
    try {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
            throw std::runtime_error("Unable to initialise HTTPS transport");
        qualify("Clock-Decimal");
        qualify("Calendar-Hebrew");
        curl_global_cleanup();
        std::puts("Independent clock and calendar release qualification passed.");
        return EXIT_SUCCESS;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "Independent provider qualification failed: %s\n", error.what());
        curl_global_cleanup();
        return EXIT_FAILURE;
    }
}
