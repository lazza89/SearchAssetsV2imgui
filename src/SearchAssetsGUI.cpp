#include "SearchAssetsGUI.h"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <thread>
#include <filesystem>
#include <algorithm>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

SearchAssetsGUI::SearchAssetsGUI(GLFWwindow* window) : glfw_window_(window)
{
    search_engine_ = std::make_unique<SearchEngine>();
    // Initialize filtered results as empty
    filtered_result_lines_.clear();

    // Initialize Xbox controller emulator and 4 panels
    controller_emulator_ = std::make_unique<ControllerEmulator>();
    for (int i = 0; i < 4; ++i)
        controller_panels_[i] = std::make_unique<ControllerPanel>(i, *controller_emulator_);

    // Set up peer pointers for mirror group communication
    for (int i = 0; i < 4; ++i)
        controller_panel_ptrs_[i] = controller_panels_[i].get();
    for (int i = 0; i < 4; ++i)
        controller_panels_[i]->SetPeers(controller_panel_ptrs_.data(), 4);
}

SearchAssetsGUI::~SearchAssetsGUI()
{
    if (search_engine_)
    {
        search_engine_->stop_search();
    }
}

void SearchAssetsGUI::render()
{
    // Get the viewport (window area, not screen)
    ImGuiViewport *viewport = ImGui::GetMainViewport();

    // Set window to cover entire client area
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    // Main window - locked but windowed
    static bool open = true;
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoCollapse |
                                    ImGuiWindowFlags_NoTitleBar;

    ImGui::Begin("SearchAssets ImGui", &open, window_flags);

    // Welcome header
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::CalcTextSize("SearchAssets V2 Turbo").x) * 0.5f);
    ImGui::TextColored(ImVec4(0.28f, 0.56f, 1.00f, 1.00f), "SearchAssets V2 Turbo");

    // Always-on-top toggle, allineato a destra sulla stessa riga del titolo
    ImGui::SameLine();
    float cb_w = ImGui::CalcTextSize("Always on top").x + ImGui::GetFrameHeight() +
                 ImGui::GetStyle().ItemInnerSpacing.x;
    ImGui::SetCursorPosX(ImGui::GetWindowSize().x - cb_w - ImGui::GetStyle().WindowPadding.x);
    if (ImGui::Checkbox("Always on top", &always_on_top_) && glfw_window_)
        glfwSetWindowAttrib(glfw_window_, GLFW_FLOATING, always_on_top_ ? GLFW_TRUE : GLFW_FALSE);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Tab bar — Search Assets | Xbox Controller
    if (ImGui::BeginTabBar("##MainTabs"))
    {
        static int prev_tab = -1;
        int cur_tab = -1;

        if (ImGui::BeginTabItem("  Search Assets  "))
        {
            cur_tab = 0;
            ImGui::Spacing();
            render_search_panel();
            ImGui::Separator();
            render_results_panel();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("  Xbox Controller  "))
        {
            cur_tab = 1;
            ImGui::Spacing();
            render_controller_tab();
            ImGui::EndTabItem();
        }

        if (cur_tab != -1 && cur_tab != prev_tab)
        {
            prev_tab = cur_tab;
            resize_to_tab(cur_tab);
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void SearchAssetsGUI::render_search_panel()
{
    // Search Configuration Header
    ImGui::TextColored(ImVec4(0.28f, 0.56f, 1.00f, 1.00f), "Search Configuration");
    ImGui::Separator();
    ImGui::Spacing();

    // Row 1: Search Pattern
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Pattern:");
    ImGui::SameLine();
    if (ImGui::InputTextWithHint("##SearchPattern", "Enter a class or text (press Enter to search)...", search_pattern_, sizeof(search_pattern_), ImGuiInputTextFlags_EnterReturnsTrue))
    {
        // Enter key pressed, start search
        perform_search();
    }

    // Row 2: Path and Options
    ImGui::Text("Path:");
    ImGui::SameLine();
    ImGui::InputTextWithHint("##CustomPath", "Leave empty for Content/Assets", custom_path_, sizeof(custom_path_));

    // Row 3: Size limits and checkboxes
    ImGui::Text("Size (KB):");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60);
    ImGui::InputTextWithHint("##MinSize", "0.1", min_file_size_str_, sizeof(min_file_size_str_));
    ImGui::SameLine();
    ImGui::Text("to");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60);
    ImGui::InputTextWithHint("##MaxSize", "1000", max_file_size_str_, sizeof(max_file_size_str_));

    // Checkboxes - First row
    ImGui::SameLine();
    ImGui::SetCursorPosX(280);
    ImGui::Checkbox("Include Plugins", &search_plugins_);
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Search in plugin Content directories");
        ImGui::Text("(e.g., Plugins/MyPlugin/Content/)");
        ImGui::EndTooltip();
    }

    // Checkboxes - Second row
    ImGui::Checkbox("Remove UE Prefixes", &remove_unreal_prefixes_);
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Auto remove Unreal Engine prefixes from search pattern");
        ImGui::Text("(A, U, F, S, T, E, I)");
        ImGui::Text("Example: 'AWeapon' becomes 'Weapon'");
        ImGui::Text("This must be on if you are trying to search a class name");
        ImGui::EndTooltip();
    }

    ImGui::SameLine();
    ImGui::Checkbox("Match Whole Word", &match_whole_word_);
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Match only complete words");
        ImGui::Text("Example: 'Weapon' will NOT match 'WeaponComponent'");
        ImGui::EndTooltip();
    }

    // Action buttons row
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Buttons with proper spacing
    ImVec2 button_size(80, 32);
    if (!is_searching_)
    {
        // Green search button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.7f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.8f, 0.0f, 1.0f));
        if (ImGui::Button("Start Search") || (ImGui::IsKeyPressed(ImGuiKey_F5) && strlen(search_pattern_) > 0))
        {
            perform_search();
        }
        ImGui::PopStyleColor(2);
    }
    else
    {
        // Red stop button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button("Stop Search") || ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            if (search_engine_)
                search_engine_->stop_search();
        }
        ImGui::PopStyleColor(2);
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear Results"))
    {
        reset_search();
    }

    // Status text with dynamic positioning
    ImGui::SameLine();
    float status_x = ImGui::GetCursorPosX() + 10; // Small gap after buttons
    if (status_x < 600)                           // Only show status if there's room
    {
        if (!is_searching_ && strlen(search_pattern_) == 0)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "Enter pattern");
        }
        else if (!is_searching_)
        {
            ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "Ready (F5)");
        }
        else
        {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Searching... (ESC)");
        }
    }

    // Progress section
    if (is_searching_)
    {
        ImGui::Spacing();
        auto progress = progress_current_.load();
        auto total = progress_total_.load();

        // Progress bar
        ImGui::Text("Progress:");
        ImGui::SameLine();

        // Dynamic progress bar width
        float available_width = ImGui::GetContentRegionAvail().x - 10;
        if (available_width > 400)
            available_width = 400; // Max width cap

        if (total > 0)
        {
            float ratio = static_cast<float>(progress) / total;
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.0f, 0.8f, 0.2f, 1.0f));
            ImGui::ProgressBar(ratio, ImVec2(available_width, 24),
                               (std::to_string(progress) + "/" + std::to_string(total) + " files (" + std::to_string((int)(ratio * 100)) + "%)").c_str());
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.28f, 0.56f, 1.00f, 1.0f));
            ImGui::ProgressBar(-1.0f, ImVec2(available_width, 24), "Initializing search...");
            ImGui::PopStyleColor();
        }

        // Current directory with truncation
        if (!progress_message_.empty())
        {
            std::string current_msg = progress_message_;
            if (current_msg.length() > 60) // Truncate long paths
            {
                current_msg = "..." + current_msg.substr(current_msg.length() - 57);
            }
            ImGui::Text("Current: %s", current_msg.c_str());
        }
    }
}

void SearchAssetsGUI::render_results_panel()
{
    // Results header with modern styling
    auto total_results = result_lines_.size();
    auto filtered_results = filtered_result_lines_.size();

    ImGui::TextColored(ImVec4(0.28f, 0.56f, 1.00f, 1.00f), "Search Results");
    ImGui::SameLine();
    ImGui::SetCursorPosX(150);

    // Results count with colored status
    if (filtered_results > 0)
    {
        ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), " : %zu assets found", filtered_results);
    }
    else if (total_results > 0)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), " : %zu total (filtered to 0)", total_results);
    }
    else if (!is_searching_)
    {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), " : No results");
    }

    // Show status message to the right (copy all success or search warning)
    std::string status_message = "";
    ImVec4 status_color = ImVec4(0.0f, 0.9f, 0.0f, 1.0f); // Default green

    if (is_searching_)
    {
        status_message = "Cannot copy while searching";
        status_color = ImVec4(1.0f, 0.6f, 0.0f, 1.0f); // Orange warning
    }
    else if (!last_copied_item_.empty() && last_copied_item_.find("results copied") != std::string::npos)
    {
        status_message = last_copied_item_;
        status_color = ImVec4(0.0f, 0.9f, 0.0f, 1.0f); // Green success
    }

    if (!status_message.empty())
    {
        ImGui::SameLine();

        // Calculate text width dynamically
        float text_width = ImGui::CalcTextSize(status_message.c_str()).x;
        float remaining_width = ImGui::GetContentRegionAvail().x;

        if (text_width < remaining_width) // Only show if it fits
        {
            // Position from the right edge, accounting for text width
            float window_width = ImGui::GetWindowSize().x;
            float padding = 20; // Small padding from window edge
            ImGui::SetCursorPosX(window_width - text_width - padding);
            ImGui::TextColored(status_color, "%s", status_message.c_str());
        }
    }

    ImGui::Separator();
    ImGui::Spacing();

    // Filter and copy controls row
    ImGui::Text("Filter:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180);
    if (ImGui::InputTextWithHint("##FilterResults", "Filter results...", result_filter_, sizeof(result_filter_)))
    {
        update_filtered_results();
    }

    // Copy buttons with dynamic positioning
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
    if (ImGui::Button("Copy Selected"))
    {
        copy_selected_result();
    }
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.4f, 1.0f));
    if (ImGui::Button("Copy All"))
    {
        copy_all_results();
    }
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Modern results table with enhanced styling
    if (ImGui::BeginTable("ResultsTable", 1,
                          ImGuiTableFlags_Borders |
                              ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_ScrollY |
                              ImGuiTableFlags_Sortable |
                              ImGuiTableFlags_Resizable))
    {
        // Table headers
        ImGui::TableSetupColumn("Asset Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        std::lock_guard<std::mutex> lock(results_mutex_);

        // Enhanced table rows with better visual feedback
        for (size_t i = 0; i < filtered_result_lines_.size(); i++)
        {
            ImGui::TableNextRow();
            const auto &result = filtered_result_lines_[i];
            bool is_selected = (static_cast<int>(i) == selected_result_);

            // First column - Asset name
            ImGui::TableNextColumn();

            // Highlight selected row
            if (is_selected)
            {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::ColorConvertFloat4ToU32(ImVec4(0.28f, 0.56f, 1.00f, 0.3f)));
            }

            if (ImGui::Selectable(("##row" + std::to_string(i)).c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns))
            {
                selected_result_ = static_cast<int>(i);
            }

            // Double-click to copy with visual feedback
            if (ImGui::IsItemHovered())
            {
                if (ImGui::IsMouseDoubleClicked(0))
                {
                    selected_result_ = static_cast<int>(i);
                    copy_selected_result();
                }

                // Show different tooltip based on copy status
                if (!last_single_copied_item_.empty() && last_single_copied_item_ == result.substr(0, result.find_last_of('.')))
                {
                    // Show success tooltip if this item was just copied
                    ImGui::BeginTooltip();
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Copied to clipboard!");
                    ImGui::Text("Asset: %s", result.c_str());
                    ImGui::EndTooltip();
                }
                else
                {
                    // Show default tooltip
                    ImGui::SetTooltip("Double-click to copy to clipboard\nAsset: %s", result.c_str());
                }
            }

            // Show the filename
            ImGui::SameLine(0, 0);
            ImGui::Text("%s", result.c_str());
        }

        ImGui::EndTable();
    }
}

void SearchAssetsGUI::perform_search()
{
    if (is_searching_ || strlen(search_pattern_) == 0)
    {
        return;
    }

    // Update file size limits from UI
    try
    {
        double min_kb = std::stod(min_file_size_str_);
        double max_kb = std::stod(max_file_size_str_);
        size_t min_bytes = static_cast<size_t>(min_kb * 1024);
        size_t max_bytes = static_cast<size_t>(max_kb * 1024);
        search_engine_->set_file_size_limits(min_bytes, max_bytes);
    }
    catch (const std::exception &)
    {
        // Use default values if parsing fails
    }

    // Sanitize search pattern if Unreal prefix removal is enabled
    std::string actual_search_pattern = search_pattern_;
    if (remove_unreal_prefixes_)
    {
        actual_search_pattern = remove_unreal_prefix(search_pattern_);
    }

    // NON aggiungere \b qui, passa il flag invece al search engine
    search_engine_->set_match_whole_word(match_whole_word_);

    reset_search();
    is_searching_ = true;

    std::vector<std::filesystem::path> search_paths;

    // Determine search paths
    if (strlen(custom_path_) > 0)
    {
        search_paths.push_back(std::filesystem::path(custom_path_));
    }
    else
    {
        // Default Content/Assets path
        if (std::filesystem::exists("Content/Assets"))
        {
            search_paths.push_back(std::filesystem::path("Content/Assets"));
        }

        // Add plugin paths if enabled
        if (search_plugins_)
        {
            try
            {
                if (std::filesystem::exists("Plugins"))
                {
                    for (const auto &plugin_dir : std::filesystem::directory_iterator("Plugins"))
                    {
                        if (plugin_dir.is_directory())
                        {
                            auto content_path = plugin_dir.path() / "Content";
                            if (std::filesystem::exists(content_path))
                            {
                                search_paths.push_back(content_path);
                            }
                        }
                    }
                }
            }
            catch (const std::filesystem::filesystem_error &)
            {
                // Skip if can't access Plugins directory
            }
        }
    }

    if (search_paths.empty())
    {
        is_searching_ = false;
        update_progress("No search paths available", 0, 0);
        return;
    }

    // Start search in separate thread
    std::thread search_thread([this, search_paths, actual_search_pattern]()
                              {
        search_engine_->search(
            actual_search_pattern,
            search_paths,
            [this](const std::string& message, size_t current, size_t total) {
                update_progress(message, current, total);
            },
            [this](const SearchResult& result) {
                add_result(result);
            }
        );
        is_searching_ = false; });
    search_thread.detach();
}

void SearchAssetsGUI::reset_search()
{
    if (search_engine_)
    {
        search_engine_->stop_search();
        search_engine_->clear_results();
    }

    {
        std::lock_guard<std::mutex> lock(results_mutex_);
        result_lines_.clear();
        filtered_result_lines_.clear();
        seen_filenames_.clear();
        selected_result_ = 0;
    }

    memset(result_filter_, 0, sizeof(result_filter_));
    last_copied_item_.clear();

    progress_message_ = "";
    progress_current_ = 0;
    progress_total_ = 0;
    is_searching_ = false;
}

void SearchAssetsGUI::update_progress(const std::string &message, size_t current, size_t total)
{
    progress_message_ = message;
    progress_current_ = current;
    progress_total_ = total;
}

void SearchAssetsGUI::add_result(const SearchResult &result)
{
    std::lock_guard<std::mutex> lock(results_mutex_);

    // Show only the filename (without path)
    std::string filename = result.file_path.filename().string();

    // Check if filename already exists in results to avoid duplicates (O(1))
    if (seen_filenames_.insert(filename).second)
    {
        result_lines_.push_back(filename);
        // Update filtered results inline to avoid double locking.
        // Append solo il nuovo elemento: copiare tutto il vettore ad ogni
        // risultato renderebbe add_result O(N^2) sul totale dei match.
        if (strlen(result_filter_) == 0)
        {
            filtered_result_lines_.push_back(filename);
        }
        else
        {
            // Re-filter all results
            std::string filter = result_filter_;
            std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

            std::string filename_lower = filename;
            std::transform(filename_lower.begin(), filename_lower.end(), filename_lower.begin(), ::tolower);

            if (filename_lower.find(filter) != std::string::npos)
            {
                filtered_result_lines_.push_back(filename);
            }
        }
    }
}

void SearchAssetsGUI::update_filtered_results()
{
    std::lock_guard<std::mutex> lock(results_mutex_);
    filtered_result_lines_.clear();

    std::string filter = result_filter_;
    if (filter.empty())
    {
        filtered_result_lines_ = result_lines_;
    }
    else
    {
        std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

        for (const auto &result : result_lines_)
        {
            std::string result_lower = result;
            std::transform(result_lower.begin(), result_lower.end(), result_lower.begin(), ::tolower);

            if (result_lower.find(filter) != std::string::npos)
            {
                filtered_result_lines_.push_back(result);
            }
        }
    }

    if (selected_result_ >= static_cast<int>(filtered_result_lines_.size()))
    {
        selected_result_ = 0;
    }
}

std::string SearchAssetsGUI::remove_unreal_prefix(const std::string &filename)
{
    if (filename.length() < 2)
        return filename;

    // Get the base name without extension first
    std::string basename = filename;
    size_t dot_pos = basename.find_last_of('.');
    std::string extension = "";
    if (dot_pos != std::string::npos)
    {
        extension = basename.substr(dot_pos);
        basename = basename.substr(0, dot_pos);
    }

    // Check if it starts with Unreal prefixes
    char first_char = basename[0];
    if (basename.length() > 1 &&
        (first_char == 'A' || first_char == 'U' || first_char == 'F' ||
         first_char == 'S' || first_char == 'T' || first_char == 'E' ||
         first_char == 'I') &&
        std::isupper(basename[1]))
    { // Second char should be uppercase for valid Unreal class
        return basename.substr(1) + extension;
    }

    return filename;
}

void SearchAssetsGUI::copy_selected_result()
{
    try
    {
        // Don't copy while searching
        if (is_searching_)
        {
            last_copied_item_ = "Cannot copy while searching";
            return;
        }

        if (filtered_result_lines_.empty() ||
            selected_result_ < 0 ||
            selected_result_ >= static_cast<int>(filtered_result_lines_.size()))
        {
            last_copied_item_ = "No result selected";
            return;
        }

        std::string selected_item = filtered_result_lines_[selected_result_];

        // Remove file extension
        size_t dot_pos = selected_item.find_last_of('.');
        if (dot_pos != std::string::npos)
        {
            selected_item = selected_item.substr(0, dot_pos);
        }

        set_clipboard(selected_item);
        // Set tooltip feedback variable but not main status
        last_single_copied_item_ = selected_item;
    }
    catch (const std::exception &e)
    {
        last_copied_item_ = "Copy failed: exception";
    }
    catch (...)
    {
        last_copied_item_ = "Copy failed: unknown error";
    }
}

void SearchAssetsGUI::copy_all_results()
{
    // Don't copy while searching
    if (is_searching_)
    {
        last_copied_item_ = "Cannot copy while searching";
        return;
    }

    std::lock_guard<std::mutex> lock(results_mutex_);

    if (filtered_result_lines_.empty())
    {
        last_copied_item_ = "No results to copy";
        return;
    }

    // Create a formatted string with all results
    std::stringstream ss;
    ss << "Search Results (" << filtered_result_lines_.size() << " items):\n";
    ss << "====================================\n";

    for (size_t i = 0; i < filtered_result_lines_.size(); ++i)
    {
        ss << (i + 1) << ". " << filtered_result_lines_[i] << "\n";
    }

    std::string all_results = ss.str();
    set_clipboard(all_results);

    last_copied_item_ = std::to_string(filtered_result_lines_.size()) + " results copied to clipboard";
}

// Clipboard functions
void SearchAssetsGUI::set_clipboard(const std::string &text)
{
    if (!OpenClipboard(nullptr))
        return;

    EmptyClipboard();

    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if (hGlobal)
    {
        char *pGlobal = static_cast<char *>(GlobalLock(hGlobal));
        if (pGlobal)
        {
            strcpy_s(pGlobal, text.size() + 1, text.c_str());
            GlobalUnlock(hGlobal);
            SetClipboardData(CF_TEXT, hGlobal);
        }
    }

    CloseClipboard();
}

std::string SearchAssetsGUI::get_clipboard()
{
    if (!OpenClipboard(nullptr))
    {
        return "";
    }

    HANDLE hData = GetClipboardData(CF_TEXT);
    if (hData == nullptr)
    {
        CloseClipboard();
        return "";
    }

    char *pszText = static_cast<char *>(GlobalLock(hData));
    if (pszText == nullptr)
    {
        CloseClipboard();
        return "";
    }

    std::string text(pszText);
    GlobalUnlock(hData);
    CloseClipboard();
    return text;
}

void SearchAssetsGUI::resize_to_tab(int tab)
{
    if (!glfw_window_) return;

    int w = (tab == 0) ? 800 : 1000;
    int h = (tab == 0) ? 800 : 1000;

    glfwSetWindowSize(glfw_window_, w, h);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (monitor)
    {
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        if (mode)
            glfwSetWindowPos(glfw_window_, (mode->width - w) / 2, (mode->height - h) / 2);
    }
}

void SearchAssetsGUI::render_controller_tab()
{
    // Show a warning banner if the ViGEmBus driver is not installed
    if (!controller_emulator_->IsDriverAvailable())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.45f, 0.45f, 1.0f));
        ImGui::TextWrapped("ViGEmBus driver not found!");
        ImGui::PopStyleColor();
        ImGui::TextWrapped("Install it from: github.com/nefarius/ViGEmBus/releases");
        ImGui::Separator();
        ImGui::Spacing();
    }

    // --- Mirror Group selector ---
    ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "Mirror Groups");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(same group = shared inputs)");

    static const char* groupLabels[] = { "None", "Group 1", "Group 2", "Group 3" };
    for (int i = 0; i < 4; ++i) {
        ImGui::PushID(i + 100);
        ImGui::Text("P%d:", i + 1);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        if (ImGui::Combo("##mg", &mirror_groups_[i], groupLabels, IM_ARRAYSIZE(groupLabels)))
            controller_panels_[i]->SetMirrorGroup(mirror_groups_[i]);
        if (i < 3) ImGui::SameLine();
        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Lay out 4 panels in a 2x2 grid using child windows so each panel
    // can compute its own scale from its available width.
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float availW  = ImGui::GetContentRegionAvail().x;
    float availH  = ImGui::GetContentRegionAvail().y;
    float colW    = (availW - spacing) * 0.5f;
    float rowH    = (availH - spacing) * 0.5f;

    constexpr ImGuiWindowFlags kNoScroll = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    // Row 1 — Player 1 | Player 2
    ImGui::BeginChild("##ctrl_p1", {colW, rowH}, false, kNoScroll);
    controller_panels_[0]->Render();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##ctrl_p2", {colW, rowH}, false, kNoScroll);
    controller_panels_[1]->Render();
    ImGui::EndChild();

    // Row 2 — Player 3 | Player 4
    ImGui::BeginChild("##ctrl_p3", {colW, rowH}, false, kNoScroll);
    controller_panels_[2]->Render();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##ctrl_p4", {colW, rowH}, false, kNoScroll);
    controller_panels_[3]->Render();
    ImGui::EndChild();
}