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

void openfile::iterateDirectory(const std::string& path, std::vector<std::filesystem::path>& files, std::vector<std::filesystem::path>& directories,std::vector<std::string> filters)
{
    try {
        files.clear();
        directories.clear();

        std::filesystem::path dirPath(path);
        auto dir = std::filesystem::absolute(dirPath);

        if (!std::filesystem::exists(dir)) {
            std::cerr << "Directory does not exist: " << dir << std::endl;
            return;
        }

        if (!std::filesystem::is_directory(dir)) {
            std::cerr << "Path is not a directory: " << dir << std::endl;
            return;
        }

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
        std::cerr << "Filesystem error in " << path << ": " << e.what() << std::endl;
    }
}

openfile::openfile(std::string title, std::string startdir, std::vector<std::string> filefilters = {}) : title(title), filefilters(filefilters)
{
    auto dir = std::filesystem::path(startdir);
    currentdir = std::filesystem::absolute(dir);
}

openfile::Result openfile::show(bool& show)
{
    Result result = Closed;

    if (ImGui::Begin(title.c_str(), &show)) {
        result = None;
        
        if (openfile_items.size() == 0) {
            selecteditem = std::filesystem::path("");
            iterateDirectory(currentdir.string(), files, directories, filefilters);

            item_selected_idx = 0;
            item_highlighted_idx = 0;

            openfile_items.push_back("..");
            for (auto& dir : directories)
                openfile_items.push_back(dir.filename().string());
            for (auto& file : files)
                openfile_items.push_back(file.filename().string());
        }

        std::filesystem::path curpath = currentdir;
        std::string buttonname;

        std::vector<std::pair<std::string, std::filesystem::path>> dirs;
        do {

            auto curname = std::filesystem::absolute(curpath).filename();
            buttonname = curname.string();
            if (buttonname.empty())
                continue;

            dirs.push_back({ buttonname, std::filesystem::absolute(curpath) });
            curpath = std::filesystem::absolute(curpath.parent_path());

        } while (!buttonname.empty());

        for (auto& dir : dirs | std::views::reverse) {
            if (ImGui::Button(dir.first.c_str()))  {
                currentdir = dir.second;
                openfile_items.clear();
            }
            ImGui::SameLine();
        }
        ImGui::Spacing();
        ImGui::Text("%s", currentdir.string().c_str());
       
        if (ImGui::BeginListBox(" ")) {
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
                        // the userer selected an item. // for now just exit but what we really beed to do is navigate if its a directory
                        if (item_selected_idx == 0) {
                            currentdir = std::filesystem::absolute(currentdir.parent_path());
                            openfile_items.clear();
                        }
                        else if (item_selected_idx <= files.size()) {
                            result = FileSelected;
                            show = false;
                        }
                        else {
                            auto dirindex = item_selected_idx - files.size() - 1;
                            currentdir = std::filesystem::absolute(directories[dirindex]);
                            openfile_items.clear();
                        }
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
        ImGui::SameLine();
        ImGui::BeginGroup();

        if (ImGui::Button("Open")) {
            // the userer selected an item. // for now just exit but what we really beed to do is navigate if its a directory
            if (item_selected_idx == 0) {
                currentdir = std::filesystem::absolute(currentdir.parent_path());
                openfile_items.clear();
            }
            else if (item_selected_idx <= files.size()) {
                result = FileSelected;
                show = false;
            }
            else {
                auto dirindex = item_selected_idx - files.size() -1;
                currentdir = std::filesystem::absolute(directories[dirindex]);
                openfile_items.clear();
            }
        }
        if (ImGui::Button("Cancel")) {
            show = false;
        }
        ImGui::EndGroup();
         
        ImGui::Separator();
        if (item_selected_idx > 0 && item_selected_idx <= files.size()) {
            ImGui::Spacing();
            ImGui::Text("File: %s", selecteditem.filename().string().c_str());
        }
    }

    ImGui::End();
    return result;
}
