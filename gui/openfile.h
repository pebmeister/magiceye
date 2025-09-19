#pragma once
#include <filesystem>
#include <vector>
#include <stack>

class openfile {
public:
    enum Result {
        None,
        Closed,
        Cancel,
        FileSelected
    };

private:
    std::vector<std::filesystem::path> files;
    std::vector<std::filesystem::path> directories;
    std::vector<std::string> openfile_items;
    std::vector<std::string> filefilters;
    std::stack<std::filesystem::path> directoryHistory;
    std::stack<std::filesystem::path> backHistory;
    std::vector<std::pair<std::string, std::filesystem::path>> dirs;
    std::filesystem::path currentdir;
    std::filesystem::path startdir;

    int item_selected_idx = 0;
    int item_highlighted_idx = 0;
    bool item_highlight = true;
    std::string title;

    // Helper function for case-insensitive string comparison
    static bool caseInsensitiveCompare(const std::string& a, const std::string& b)
    {
        return std::lexicographical_compare(
            a.begin(), a.end(),
            b.begin(), b.end(),
            [](char c1, char c2)
            {
                return std::tolower(static_cast<unsigned char>(c1)) <
                    std::tolower(static_cast<unsigned char>(c2));
            }
        );
    }

    void iterateDirectory(const std::string& path, std::vector<std::filesystem::path>& files, std::vector<std::filesystem::path>& directories, std::vector<std::string> fileters);

    void BuildOpenFiles()
    {
        iterateDirectory(currentdir.string(), files, directories, filefilters);

        item_selected_idx = 0;
        item_highlighted_idx = 0;

        // Note: This logic assumes directories come first in your openfile_items list
        if (item_selected_idx < directories.size()) {
            // This is a directory
            selecteditem = directories[item_selected_idx];
        }
        else {
            // This is a file
            if (files.size() > 0)
                selecteditem = files[item_selected_idx - directories.size()];
        }

        for (auto& dir : directories)
            openfile_items.push_back(dir.filename().string());
        for (auto& file : files)
            openfile_items.push_back(file.filename().string());
    }

    std::vector<std::pair<std::string, std::filesystem::path>> BuildDirs() const
    {
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

        } while (!buttonname.empty() && dirs.size() < 5);
        return dirs;
    }

private:
    void HandleOpen(Result& result, bool& show)
    {
        while (!backHistory.empty())
            backHistory.pop();

        if (item_selected_idx >= directories.size()) {
            result = FileSelected;
            show = false;
        }
        else {
            directoryHistory.push(currentdir);
            auto dirindex = item_selected_idx;
            currentdir = std::filesystem::absolute(directories[dirindex]);
            openfile_items.clear();
        }
    }

public:
    std::filesystem::path selecteditem;
    openfile(std::string title, std::string startdir, std::vector<std::string> filefilters);
    Result show(bool& show);
};
