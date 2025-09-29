#pragma once

#include <string>
#include <vector>
#include <future>
#include <functional>
#include <filesystem>
#include <regex>
#include <mutex>
#include <atomic>

struct SearchResult
{
    std::filesystem::path file_path;
    std::string line_content;
    size_t line_number;

    SearchResult(const std::filesystem::path &path, const std::string &content, size_t line_num)
        : file_path(path), line_content(content), line_number(line_num) {}
};

class SearchEngine
{
public:
    using ProgressCallback = std::function<void(const std::string &, size_t, size_t)>;
    using ResultCallback = std::function<void(const SearchResult &)>;

    SearchEngine();
    ~SearchEngine();

    void set_file_size_limits(size_t min_size, size_t max_size)
    {
        min_file_size_ = min_size;
        max_file_size_ = max_size;
    }

    void search(const std::string &search_pattern,
                const std::vector<std::filesystem::path> &search_paths,
                const ProgressCallback& progress_cb = nullptr,
                const ResultCallback& result_cb = nullptr);

    void stop_search();
    bool is_searching() const { return searching_; }

    const std::vector<SearchResult> &get_results() const { return results_; }
    void clear_results();

    void set_thread_count(size_t threads) { thread_count_ = threads; }
    size_t get_thread_count() const { return thread_count_; }

private:
    void search_file(const std::filesystem::path &file_path,
                     const ResultCallback& result_cb);

    void search_directory_worker(const std::filesystem::path &dir_path,
                                 const ProgressCallback& progress_cb,
                                 const ResultCallback& result_cb);

    std::vector<std::filesystem::path> collect_files(const std::filesystem::path &directory) const;

    mutable std::mutex results_mutex_;
    std::vector<SearchResult> results_;
    std::atomic<bool> searching_{false};
    std::atomic<bool> stop_requested_{false};
    size_t thread_count_;

    size_t min_file_size_ = 100;         // Skip files smaller than 100 bytes
    size_t max_file_size_ = 1024 * 1024; // Skip files larger than 1MB

    std::regex compiled_pattern_; // Cached compiled regex
};