#pragma once

#include "SearchEngine.h"
#include "ControllerEmulator.h"
#include "ControllerPanel.h"
#include <imgui.h>
#include <array>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <unordered_set>

// Forward declaration — evita di includere GLFW nell'header
struct GLFWwindow;

class SearchAssetsGUI
{
public:
    explicit SearchAssetsGUI(GLFWwindow* window);
    ~SearchAssetsGUI();

    void render();

private:
    void render_search_panel();
    void render_results_panel();
    void render_controller_tab();
    void resize_to_tab(int tab);   // ridimensiona e ricentra la finestra
    void update_progress(const std::string &message, size_t current, size_t total);
    void add_result(const SearchResult &result);
    void perform_search();
    void reset_search();
    void update_filtered_results();
    void copy_selected_result();
    void copy_all_results();
    std::string remove_unreal_prefix(const std::string& filename);
    void set_clipboard(const std::string& text);
    std::string get_clipboard();

    // UI State
    char search_pattern_[256] = "";
    char custom_path_[512] = "";
    char result_filter_[256] = "";
    bool search_plugins_ = false;
    bool remove_unreal_prefixes_ = true;
    bool match_whole_word_ = false;

    // File size limits (in KB for easier UI)
    char min_file_size_str_[16] = "0.1"; // 100 bytes = 0.1 KB
    char max_file_size_str_[16] = "2000";

    // Search state
    std::atomic<bool> is_searching_{false};
    std::string progress_message_;
    std::atomic<size_t> progress_current_{0};
    std::atomic<size_t> progress_total_{0};

    // Results
    mutable std::mutex results_mutex_;
    std::vector<std::string> result_lines_;
    std::vector<std::string> filtered_result_lines_;
    std::unordered_set<std::string> seen_filenames_; // dedup O(1) durante la ricerca
    int selected_result_ = 0;
    std::string last_copied_item_;
    std::string last_single_copied_item_; // For tooltip feedback

    // Search engine
    std::unique_ptr<SearchEngine> search_engine_;

    // Xbox Controller
    std::unique_ptr<ControllerEmulator>              controller_emulator_;
    std::array<std::unique_ptr<ControllerPanel>, 4>  controller_panels_;
    std::array<ControllerPanel*, 4>                  controller_panel_ptrs_ = {};
    std::array<int, 4>                               mirror_groups_ = {}; // 0=none

    // UI state
    ImVec4 clear_color_ = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    bool always_on_top_ = false;   // mantiene la finestra sopra le altre

    // GLFW window handle (for dynamic resizing on tab switch)
    GLFWwindow* glfw_window_ = nullptr;
};