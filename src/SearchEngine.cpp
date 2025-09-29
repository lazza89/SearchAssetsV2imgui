#include "SearchEngine.h"
#include <thread>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <queue>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

SearchEngine::SearchEngine() : thread_count_(std::thread::hardware_concurrency()) {
    if (thread_count_ == 0) thread_count_ = 4;
}

SearchEngine::~SearchEngine() {
    stop_search();
}

void SearchEngine::search(const std::string& search_pattern,
                         const std::vector<std::filesystem::path>& search_paths,
                         const ProgressCallback& progress_cb,
                         const ResultCallback& result_cb) {
    if (searching_) {
        return;
    }

    searching_ = true;
    stop_requested_ = false;
    clear_results();

    try {
        // Use case insensitive regex
        compiled_pattern_ = std::regex(search_pattern, std::regex_constants::icase);
        std::vector<std::future<void>> futures;

        for (const auto& path : search_paths) {
            if (stop_requested_) break;

            if (std::filesystem::exists(path)) {
                futures.emplace_back(
                    std::async(std::launch::async,
                              &SearchEngine::search_directory_worker,
                              this, path, progress_cb, result_cb)
                );
            } else {
                std::string error_msg = "Directory not found: " + path.string();
                if (progress_cb) {
                    progress_cb(error_msg, 0, 0);
                }
            }
        }

        for (const auto& future : futures) {
            future.wait();
        }
    } catch (const std::regex_error& e) {
        std::string error_msg = "Invalid regex pattern: " + std::string(e.what());
        if (progress_cb) {
            progress_cb(error_msg, 0, 0);
        }
    }

    searching_ = false;
}

void SearchEngine::stop_search() {
    stop_requested_ = true;
}

void SearchEngine::clear_results() {
    std::scoped_lock<std::mutex> lock(results_mutex_);
    results_.clear();
}

void SearchEngine::search_file(const std::filesystem::path& file_path,
                              const ResultCallback& result_cb) {
    if (stop_requested_) {
        return;
    }

    // Check file size before processing
    std::error_code ec;
    auto file_size_check = std::filesystem::file_size(file_path, ec);
    if (ec || file_size_check < min_file_size_ || file_size_check > max_file_size_) {
        return;  // Skip files outside size limits or with errors
    }

    try {
        // Use memory-mapped file for better performance
        const char* file_data = nullptr;
        size_t file_size = 0;

#ifdef _WIN32
        HANDLE hFile = CreateFileA(file_path.string().c_str(), GENERIC_READ, FILE_SHARE_READ,
                                   nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) {
            return;
        }

        HANDLE hMapping = CreateFileMappingA(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (hMapping == nullptr) {
            CloseHandle(hFile);
            return;
        }

        file_data = static_cast<const char*>(MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0));
        if (file_data == nullptr) {
            CloseHandle(hMapping);
            CloseHandle(hFile);
            return;
        }

        LARGE_INTEGER fileSize;
        if (!GetFileSizeEx(hFile, &fileSize)) {
            UnmapViewOfFile(file_data);
            CloseHandle(hMapping);
            CloseHandle(hFile);
            return;
        }
        file_size = static_cast<size_t>(fileSize.QuadPart);

#else
        int fd = open(file_path.c_str(), O_RDONLY);
        if (fd == -1) {
            return;
        }

        struct stat sb;
        if (fstat(fd, &sb) == -1) {
            close(fd);
            return;
        }
        file_size = static_cast<size_t>(sb.st_size);

        file_data = static_cast<const char*>(mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0));
        if (file_data == MAP_FAILED) {
            close(fd);
            return;
        }
#endif

        if (file_size == 0 || file_data == nullptr) {
#ifdef _WIN32
            UnmapViewOfFile(file_data);
            CloseHandle(hMapping);
            CloseHandle(hFile);
#else
            munmap(const_cast<char*>(file_data), file_size);
            close(fd);
#endif
            return;
        }

        // Search in memory-mapped data (much faster than loading into string)
        std::string_view file_content(file_data, file_size);

        if (std::regex_search(file_content.begin(), file_content.end(), compiled_pattern_)) {
            // For binary files, we'll just report "binary content" as the line
            std::string content_preview = "Binary content match";

            // If it looks like text (no null bytes in first 1000 chars), show preview
            size_t preview_size = std::min<size_t>(1000, file_content.length());
            std::string_view preview = file_content.substr(0, preview_size);

            bool is_likely_text = std::find(preview.begin(), preview.end(), '\0') == preview.end();
            if (is_likely_text) {
                std::string preview_str(preview);
                // Replace newlines with spaces for single-line display
                std::replace(preview_str.begin(), preview_str.end(), '\n', ' ');
                std::replace(preview_str.begin(), preview_str.end(), '\r', ' ');
                content_preview = std::move(preview_str);
            }

            SearchResult result(file_path, content_preview, 1);

            {
                std::scoped_lock<std::mutex> lock(results_mutex_);
                results_.push_back(result);
            }

            if (result_cb) {
                result_cb(result);
            }
        }

        // Clean up memory mapping
#ifdef _WIN32
        UnmapViewOfFile(file_data);
        CloseHandle(hMapping);
        CloseHandle(hFile);
#else
        munmap(const_cast<char*>(file_data), file_size);
        close(fd);
#endif
    } catch (const std::exception&) {
        // Silently handle file read errors
    }
}

void SearchEngine::search_directory_worker(const std::filesystem::path& dir_path,
                                         const ProgressCallback& progress_cb,
                                         const ResultCallback& result_cb) {
    if (stop_requested_) {
        return;
    }
    try {
        auto files = collect_files(dir_path);
        size_t total_files = files.size();
        size_t processed_files = 0;


        if (progress_cb) {
            progress_cb("Searching in: " + dir_path.string(), processed_files, total_files);
        }

        // Process files in batches for better load balancing
        const size_t batch_size = std::max(static_cast<size_t>(1), files.size() / thread_count_);
        std::vector<std::future<void>> file_futures;

        for (size_t i = 0; i < files.size() && !stop_requested_; i += batch_size) {
            size_t end = std::min(i + batch_size, files.size());

            file_futures.emplace_back(
                std::async(std::launch::async, [this, &files, i, end, result_cb, progress_cb, &processed_files, total_files]() {
                    for (size_t j = i; j < end && !stop_requested_; ++j) {
                        search_file(files[j], result_cb);

                        ++processed_files;
                        if (progress_cb && (processed_files % 10 == 0 || processed_files == total_files)) {
                            progress_cb("Processing files...", processed_files, total_files);
                        }
                    }
                })
            );
        }

        for (auto const& future : file_futures) {
            future.wait();
        }

    } catch (const std::filesystem::filesystem_error& e) {
        std::string error_msg = "Error accessing: " + dir_path.string() + " - " + e.what();
        if (progress_cb) {
            progress_cb(error_msg, 0, 0);
        }
    }
}

std::vector<std::filesystem::path> SearchEngine::collect_files(const std::filesystem::path& directory) const {
    std::vector<std::filesystem::path> files;
    size_t directory_count = 0;
    size_t file_count = 0;

    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (stop_requested_) {
                break;
            }

            if (entry.is_directory()) {
                directory_count++;
            } else if (entry.is_regular_file()) {
                files.push_back(entry.path());
                file_count++;
            }
        }
    } catch (const std::filesystem::filesystem_error&) {
        // Silently handle filesystem errors during collection
    }

    return files;
}