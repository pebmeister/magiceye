#pragma once
#include <filesystem>
#include <vector>

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

    std::filesystem::path currentdir;

    int item_selected_idx = 0;
    int item_highlighted_idx = 0;
    bool item_highlight = true;
    std::string title;

    void iterateDirectory(const std::string& path, std::vector<std::filesystem::path>& files, std::vector<std::filesystem::path>& directories, std::vector<std::string> fileters);

    void BuildOpenFiles()
    {
        iterateDirectory(currentdir.string(), files, directories, filefilters);

        item_selected_idx = 0;
        item_highlighted_idx = 0;

        openfile_items.push_back("..");
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

        } while (!buttonname.empty());
        return dirs;
    }

    void HandleOpen(Result& result, bool& show)
    {
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

public:
    std::filesystem::path selecteditem;
    openfile(std::string title, std::string startdir, std::vector<std::string> filefilters);
    Result show(bool& show);
};

