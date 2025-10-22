#pragma once
#include <filesystem>
#include <vector>
#include <stack>
#include <string>
#include <algorithm>

#if defined(__ANDROID__)
#include <unordered_map>
#endif

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

#if defined(__ANDROID__)
    // Map pseudo SAF paths (e.g. /saf/Downloads/Folder) -> content Uri string
    std::unordered_map<std::string, std::string> saf_map_;
    bool isSafPath(const std::filesystem::path& p) const
    {
        auto s = std::filesystem::absolute(p).string();
        return s.rfind("/saf/", 0) == 0;
    }
    static bool Android_HasDownloadsAccess();
    static void Android_RequestDownloadsAccess();
    static bool Android_HasPicturesAccess();
    static void Android_RequestPicturesAccess();

    static std::string Android_GetDownloadsTreeUri();
    static std::string Android_GetPicturesTreeUri();

    struct SafEntry { bool is_dir; std::string name; std::string uri; };
    static std::vector<SafEntry> Android_ListChildren(const std::string& tree_uri, const std::vector<std::string>& filters);
    static std::string Android_CopyDocumentToCache(const std::string& doc_uri);
    static std::string Android_CopyCachePathToPictures(const std::string& cache_path, const std::string& display_name, const std::string& mime_type);

    static void Android_ShareDocumentUri(const std::string& doc_uri, const std::string& mime_type, const std::string& subject);

#endif

    int item_selected_idx = 0;
    int item_highlighted_idx = 0;
    bool item_highlight = true;
    std::string title;

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

        if (item_selected_idx < directories.size()) {
            selecteditem = directories[item_selected_idx];
        }
        else {
            if (!files.empty())
                selecteditem = files[item_selected_idx - directories.size()];
        }

        openfile_items.clear();
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
            // File selected
#if defined(__ANDROID__)
            auto abs_sel = std::filesystem::absolute(selecteditem).string();
            // If it's a SAF pseudo path, copy to cache first
            auto it = saf_map_.find(abs_sel);
            if (it != saf_map_.end()) {
                std::string local = Android_CopyDocumentToCache(it->second);
                if (!local.empty()) {
                    selecteditem = std::filesystem::absolute(std::filesystem::path(local));
                    result = FileSelected;
                    show = false;
                }
                else {
                    // copy failed; leave dialog open
                    result = None;
                }
                return;
            }
#endif
            result = FileSelected;
            show = false;
        }
        else {
            // Directory selected, navigate into it
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
