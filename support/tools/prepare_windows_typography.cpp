// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file prepare_windows_typography.cpp
 * @brief Prepare Common-verified MB Corpo assets for the Windows PE build.
 *
 * The helper reads canonical asset provenance from the pinned Infiltratr
 * Common metadata, downloads that exact archive, verifies every published
 * SHA-256 value, then writes stable local filenames consumed by windres.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

static std::string read_text(const fs::path &path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("unable to read " + path.string());
    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

static std::string cmake_value(const std::string &text,
                               const std::string &key)
{
    const std::string marker = "set(" + key;
    const std::size_t marker_pos = text.find(marker);
    if (marker_pos == std::string::npos)
        throw std::runtime_error("missing Common typography key " + key);

    const std::size_t first_quote = text.find('"', marker_pos + marker.size());
    if (first_quote == std::string::npos)
        throw std::runtime_error("missing value for Common typography key " + key);
    const std::size_t second_quote = text.find('"', first_quote + 1U);
    if (second_quote == std::string::npos)
        throw std::runtime_error("unterminated value for Common typography key " + key);
    return text.substr(first_quote + 1U, second_quote - first_quote - 1U);
}

static std::string shell_quote(const std::string &value)
{
    std::string result = "'";
    for (const char ch : value) {
        if (ch == '\'')
            result += "'\\''";
        else
            result += ch;
    }
    result += "'";
    return result;
}

static void run(const std::string &command)
{
    const int result = std::system(command.c_str());
    if (result != 0)
        throw std::runtime_error("command failed: " + command);
}

static void verify_sha256(const fs::path &file, const std::string &expected,
                          const fs::path &workspace)
{
    const fs::path manifest = workspace / ".sha256-check";
    {
        std::ofstream output(manifest, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("unable to create SHA-256 manifest");
        output << expected << "  " << file.string() << "\n";
    }
    run("sha256sum -c " + shell_quote(manifest.string()));
    fs::remove(manifest);
}

static fs::path find_named(const fs::path &root, const std::string &filename)
{
    for (const fs::directory_entry &entry :
         fs::recursive_directory_iterator(root)) {
        if (entry.is_regular_file() &&
            entry.path().filename().string() == filename)
            return entry.path();
    }
    throw std::runtime_error("archive does not contain " + filename);
}

static void copy_verified(const fs::path &root,
                          const std::string &source_name,
                          const std::string &expected_sha,
                          const fs::path &target)
{
    const fs::path source = find_named(root, source_name);
    verify_sha256(source, expected_sha, root);
    fs::copy_file(
        source, target, fs::copy_options::overwrite_existing);
}

int main(int argc, char **argv)
{
    try {
        if (argc != 3) {
            std::cerr
                << "usage: prepare-windows-typography <Common metadata> <output dir>\n";
            return EXIT_FAILURE;
        }

        const fs::path metadata_path = argv[1];
        const fs::path output_dir = argv[2];
        const std::string metadata = read_text(metadata_path);

        const std::string source_commit =
            cmake_value(metadata, "INFILTRATR_MB_CORPO_SOURCE_COMMIT");
        std::string archive_url =
            cmake_value(metadata, "INFILTRATR_MB_CORPO_ARCHIVE_URL");
        const std::string archive_sha =
            cmake_value(metadata, "INFILTRATR_MB_CORPO_ARCHIVE_SHA256");
        const std::string ui_regular_file =
            cmake_value(metadata, "INFILTRATR_MB_CORPO_UI_REGULAR_FILE");
        const std::string ui_regular_sha =
            cmake_value(metadata, "INFILTRATR_MB_CORPO_UI_REGULAR_SHA256");
        const std::string ui_bold_file =
            cmake_value(metadata, "INFILTRATR_MB_CORPO_UI_BOLD_FILE");
        const std::string ui_bold_sha =
            cmake_value(metadata, "INFILTRATR_MB_CORPO_UI_BOLD_SHA256");
        const std::string brand_file =
            cmake_value(metadata, "INFILTRATR_MB_CORPO_BRAND_REGULAR_FILE");
        const std::string brand_sha =
            cmake_value(metadata, "INFILTRATR_MB_CORPO_BRAND_REGULAR_SHA256");

        const std::string token =
            "${INFILTRATR_MB_CORPO_SOURCE_COMMIT}";
        const std::size_t token_pos = archive_url.find(token);
        if (token_pos == std::string::npos)
            throw std::runtime_error(
                "Common typography archive URL lacks source-commit placeholder");
        archive_url.replace(token_pos, token.size(), source_commit);

        fs::remove_all(output_dir);
        fs::create_directories(output_dir);
        const fs::path archive = output_dir / "mb-corpo-fonts.tar.xz";

        run("curl -fsSL --retry 5 " + shell_quote(archive_url) +
            " -o " + shell_quote(archive.string()));
        verify_sha256(archive, archive_sha, output_dir);
        // Compiler inputs need file contents, not the archive creator's UID/GID.
        run("tar --no-same-owner -xJf " + shell_quote(archive.string()) +
            " -C " + shell_quote(output_dir.string()));

        copy_verified(
            output_dir, ui_regular_file, ui_regular_sha,
            output_dir / "ui_regular.ttf");
        copy_verified(
            output_dir, ui_bold_file, ui_bold_sha,
            output_dir / "ui_bold.ttf");
        copy_verified(
            output_dir, brand_file, brand_sha,
            output_dir / "brand_regular.ttf");

        return EXIT_SUCCESS;
    } catch (const std::exception &error) {
        std::cerr << "prepare-windows-typography: "
                  << error.what() << "\n";
        return EXIT_FAILURE;
    }
}
