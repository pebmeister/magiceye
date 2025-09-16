#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <ranges>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include <iostream>

#include "openfile.h"

/// <summary>
/// Iterate a directory
/// </summary>
/// <param name="path">start path</param>
/// <param name="files">vector</param>
/// <param name="directories">vector</param>
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
                    for (auto& filter : filters) {
                        std::filesystem::path p(entry);
                        if (p.extension() != filter)
                            continue;
                        files.push_back(fullpath);
                        break;
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
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "OpenFile::iterateDirectory Filesystem error in " << path << ": " << e.what() << std::endl;
    }
}

/// <summary>
/// constructor for the clss
/// </summary>
/// <param name="title">title of window</param>
/// <param name="startdir">start directory</param>
/// <param name="filefilters">file filters</param>
openfile::openfile(std::string title, std::string startdir, std::vector<std::string> filefilters = {}) : title(title), filefilters(filefilters)
{
    auto dir = std::filesystem::path(startdir);
    currentdir = std::filesystem::absolute(dir);
}
 

openfile::Result openfile::show(bool& show)
{
    Result result = Closed;

    if (ImGui::Begin(title.c_str(), &show)) {
        auto avail = ImGui::GetContentRegionAvail();
        result = None;
        
        if (openfile_items.size() == 0) {
            BuildOpenFiles();
        }

        auto dirs = BuildDirs();

        // build the directory buttons
        for (auto& dir : dirs | std::views::reverse) {
            if (ImGui::Button(dir.first.c_str()))  {
                currentdir = dir.second;
                openfile_items.clear();
            }
            ImGui::SameLine();
        }

        ImGui::Spacing();
        // full path of the current directory
        ImGui::Text("%s", currentdir.string().c_str());
       
        // Create the listbox
        if (ImGui::BeginListBox(" ", ImVec2{ -25 , -75})) {
            for (auto i = 0; i < openfile_items.size(); i++) {
                auto item = openfile_items[i].c_str();
                const bool is_selected = (item_selected_idx == i);

                if (ImGui::Selectable(item, is_selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                    auto doubleclick = 
                        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

                    item_selected_idx = i;
                    if (item_selected_idx > 0 && item_selected_idx  <= files.size()) {
                        selecteditem = files[item_selected_idx -1];
                    }
                    if (doubleclick) {
                        HandleOpen(result, show);
                    }
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

        ImGui::SetNextItemWidth(avail.x - 175);
        if (item_selected_idx > 0 && item_selected_idx <= files.size()) {
            ImGui::LabelText("", "File: %s", (selecteditem.filename().string()).c_str());
        }
        else {
            ImGui::LabelText("", "File:");
        }

        ImGui::SameLine(); if (ImGui::Button("Open"))   { HandleOpen(result, show); }
        ImGui::SameLine(); if (ImGui::Button("Cancel")) { show = false; }
    }

    ImGui::End();
    return result;
}
