// written by Paul Baxter

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <ranges>

#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif

#include <iostream>

#include "openfile.h"
#include "stl.h"

/// <summary>
/// Iterate a directory
/// </summary>
/// <param name="path">start path</param>
/// <param name="files">vector of file paths</param>
/// <param name="directories">vector of directory paths</param>
/// <param name="filters">file extension filters</param>
void openfile::iterateDirectory(const std::string& path, std::vector<std::filesystem::path>& files, std::vector<std::filesystem::path>& directories,std::vector<std::string> filters)
{
    try {
        files.clear();
        directories.clear();

        // get the absolute path
        std::filesystem::path dirPath(path);
        auto dir = std::filesystem::absolute(dirPath);

        // check for exist
        if (!std::filesystem::exists(dir)) {
            std::cerr << "OpenFile::iterateDirectory Directory does not exist: " << dir << std::endl;
            return;
        }

        // make sure its a directory
        if (!std::filesystem::is_directory(dir)) {
            std::cerr << "OpenFile::iterateDirectory Path is not a directory: " << dir << std::endl;
            return;
        }

        // iterate
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            auto fullpath = std::filesystem::absolute(entry);
            if (entry.is_regular_file()) {
                if (!filters.empty()) {
                    std::string ext = fullpath.extension().string();
                    // Convert to lower case
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    for (auto& filter : filters) {
                        std::string filter_lower = filter;
                        std::transform(filter_lower.begin(), filter_lower.end(), filter_lower.begin(), ::tolower);
                        if (ext == filter_lower) {
                            files.push_back(fullpath);
                            break;
                        }
                    }
                }
                else {
                    files.push_back(fullpath);
                }
            }
            else if (std::filesystem::is_directory(entry)) {
                directories.push_back(fullpath);
            }
        }

        // Sort directories case-insensitively
        std::sort(directories.begin(), directories.end(), [](const auto& a, const auto& b)
            {
                return caseInsensitiveCompare(a.filename().string(), b.filename().string());
            });

        // Sort files case-insensitively
        std::sort(files.begin(), files.end(), [](const auto& a, const auto& b)
            {
                return caseInsensitiveCompare(a.filename().string(), b.filename().string());
            });
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "OpenFile::iterateDirectory Filesystem error in " << path << ": " << e.what() << std::endl;
    }
}

/// <summary>
/// constructor for openfile class
/// </summary>
/// <param name="title">title of window</param>
/// <param name="startdir">start directory</param>
/// <param name="filefilters">file filters. A vector of extensions</param>
// In the constructor, ensure the directory exists or fall back to a reasonable default
openfile::openfile(std::string title, std::string startdir, std::vector<std::string> filefilters = {})
    : title(title), startdir(startdir), filefilters(filefilters)
{
    auto dir = std::filesystem::path(startdir);

    // Check if directory exists, if not try to create it or use current directory
    if (!std::filesystem::exists(dir)) {
        std::cerr << "OpenFile: Directory does not exist: " << dir << std::endl;

        // Try to create the directory
        try {
            std::filesystem::create_directories(dir);
            std::cout << "OpenFile: Created directory: " << dir << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "OpenFile: Failed to create directory: " << e.what() << std::endl;
            // Fall back to current directory
            dir = std::filesystem::current_path();
        }
    }

    currentdir = std::filesystem::absolute(dir);
}

// run the dialog
openfile::Result openfile::show(bool& show)
{
    Result result = Closed;

    if (ImGui::Begin(title.c_str(), &show)) {
        auto avail = ImGui::GetContentRegionAvail();
        result = None;
        
        if (openfile_items.size() == 0) {
            BuildOpenFiles();
            dirs.clear();
        }

        if (dirs.empty()) {
            dirs = BuildDirs();
        }

        // Add navigation buttons in the UI

        if (ImGui::Button("Up")) {
            directoryHistory.push(currentdir);
            currentdir = std::filesystem::absolute(currentdir.parent_path());
            openfile_items.clear();
        }
        ImGui::SameLine();
        bool disablefoward = backHistory.size() == 0;
        ImGui::BeginDisabled(disablefoward);
        if (ImGui::Button("Forward")) {
            directoryHistory.push(currentdir);
            currentdir = backHistory.top();
            backHistory.pop();
            openfile_items.clear();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();

        bool disableback = directoryHistory.size() == 0;
        ImGui::BeginDisabled(disableback);
        if (ImGui::Button("Back")) {
            backHistory.push(currentdir);
            currentdir = directoryHistory.top();
            directoryHistory.pop();
            openfile_items.clear();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();

        if (ImGui::Button("Home")) {
            while (!backHistory.empty())
                backHistory.pop();
            currentdir = std::filesystem::path(startdir);
            openfile_items.clear();
        }

        // build the directory buttons
        for (auto& dir : dirs | std::views::reverse) {
            if (ImGui::Button(dir.first.c_str()))  {
                while (!backHistory.empty())
                    backHistory.pop();
                directoryHistory.push(currentdir);
                currentdir = dir.second;
                openfile_items.clear();
            }
            ImGui::SameLine();
        }

        ImGui::Spacing();
        // full path of the current directory
        ImGui::Text("%s", currentdir.string().c_str());
       
        ImVec4 background_color = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
        float luminance = 0.2126f * background_color.x + 0.7152f * background_color.y + 0.0722f * background_color.z;
        // Detect if we're using a dark theme
        bool is_dark_theme = luminance < 0.5f;

        // Create the listbox
        if (ImGui::BeginListBox("##listbox", ImVec2{ -25 , -50 })) {

            for (auto i = 0; i < openfile_items.size(); i++) {
                auto item = openfile_items[i].c_str();
                const bool is_selected = (item_selected_idx == i);

                // Set color based on whether it's a directory or file
                if (i < directories.size()) {
                    // This is a directory - make it blue
                    if (is_dark_theme) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.39f, 0.71f, 1.0f, 1.0f));
                    }
                    else {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.5f, 1.0f, 1.0f)); // Blue color
                    }
                }
                if (ImGui::Selectable(item, is_selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                    auto doubleclick = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
                    item_selected_idx = i;

                    // Note: This logic assumes directories come first in your openfile_items list
                    if (item_selected_idx < directories.size()) {
                        // This is a directory
                        selecteditem = directories[item_selected_idx];
                    }
                    else {
                        // This is a file
                        selecteditem = files[item_selected_idx - directories.size()];
                    }

                    if (doubleclick) {
                        HandleOpen(result, show);
                    }
                }
                if (i < directories.size()) {
                    ImGui::PopStyleColor();
                }

                // Highlight hovered item (optional)
                if (item_highlight && ImGui::IsItemHovered())
                    item_highlighted_idx = i;

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
        }
        ImGui::EndListBox();
        ImGui::Separator();

        std::string file;
        if (item_selected_idx >= 0 && item_selected_idx >= directories.size()) {
            file = (selecteditem.filename().string());
        }
        ImGui::SetNextItemWidth(avail.x - 175);
        ImGui::LabelText("##File", "File: %s", file.c_str());
        ImGui::SameLine(); if (ImGui::Button("Open"))   {  HandleOpen(result, show); }
        ImGui::SameLine(); if (ImGui::Button("Cancel")) { show = false; }
    }

    ImGui::End();
    return result;
}
