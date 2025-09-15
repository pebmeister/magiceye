#pragma once
#include <filesystem>
#include <vector>

class openfile {
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

public:
    enum Result {
        None,
        Closed,
        Cancel,
        FileSelected
    };
    std::filesystem::path selecteditem;
    openfile(std::string title, std::string startdir, std::vector<std::string> filefilters);
    Result show(bool& show);
};

