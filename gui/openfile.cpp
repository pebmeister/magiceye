// written by Paul Baxter + Android SAF backend glue

#include "imgui.h"
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
#include <cstring>

#if defined(__ANDROID__)
#include <jni.h>
#include <android/log.h>
static const char* ME_TAG = "MagicEye";

// Provided by main.cpp (see snippet below)
extern "C" JavaVM* ME_GetJavaVM();
extern "C" jobject ME_GetActivity();

// Small RAII helper to attach JNI per thread
struct JniEnvScope {
    JNIEnv* env{ nullptr };
    bool attached{ false };
    JniEnvScope()
    {
        JavaVM* vm = ME_GetJavaVM();
        if (!vm) return;
        if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
            if (vm->AttachCurrentThread(&env, nullptr) == JNI_OK) attached = true;
        }
    }
    ~JniEnvScope()
    {
        if (attached) {
            ME_GetJavaVM()->DetachCurrentThread();
        }
    }
    bool ok() const { return env != nullptr; }
};

static jclass GetActivityClass(JNIEnv* env)
{
    jobject activity = ME_GetActivity();
    return env->GetObjectClass(activity);
}

bool openfile::Android_HasDownloadsAccess()
{
    JniEnvScope s; if (!s.ok()) return false;
    jobject activity = ME_GetActivity();
    jclass cls = GetActivityClass(s.env);
    jmethodID mid = s.env->GetMethodID(cls, "hasDownloadsAccess", "()Z");
    if (!mid) return false;
    jboolean r = s.env->CallBooleanMethod(activity, mid);
    return r == JNI_TRUE;
}

void openfile::Android_RequestDownloadsAccess()
{
    JniEnvScope s; if (!s.ok()) return;
    jobject activity = ME_GetActivity();
    jclass cls = GetActivityClass(s.env);
    jmethodID mid = s.env->GetMethodID(cls, "requestDownloadsAccess", "()V");
    if (!mid) return;
    s.env->CallVoidMethod(activity, mid);
}

std::string openfile::Android_GetDownloadsTreeUri()
{
    JniEnvScope s; if (!s.ok()) return {};
    jobject activity = ME_GetActivity();
    jclass cls = GetActivityClass(s.env);
    jmethodID mid = s.env->GetMethodID(cls, "getDownloadsTreeUri", "()Ljava/lang/String;");
    if (!mid) return {};
    jstring jres = (jstring)s.env->CallObjectMethod(activity, mid);
    if (!jres) return {};
    const char* cstr = s.env->GetStringUTFChars(jres, nullptr);
    std::string out = cstr ? cstr : "";
    s.env->ReleaseStringUTFChars(jres, cstr);
    s.env->DeleteLocalRef(jres);
    return out;
}

std::vector<openfile::SafEntry> openfile::Android_ListChildren(const std::string& tree_uri, const std::vector<std::string>& filters)
{
    std::vector<SafEntry> out;
    JniEnvScope s; if (!s.ok()) return out;
    jobject activity = ME_GetActivity();
    jclass cls = GetActivityClass(s.env);

    jmethodID mid = s.env->GetMethodID(cls, "listChildren", "(Ljava/lang/String;[Ljava/lang/String;)[Ljava/lang/String;");
    if (!mid) return out;

    jstring juri = s.env->NewStringUTF(tree_uri.c_str());
    jclass strCls = s.env->FindClass("java/lang/String");
    jobjectArray jfilters = s.env->NewObjectArray((jsize)filters.size(), strCls, nullptr);
    for (jsize i = 0; i < (jsize)filters.size(); ++i) {
        jstring jf = s.env->NewStringUTF(filters[i].c_str());
        s.env->SetObjectArrayElement(jfilters, i, jf);
        s.env->DeleteLocalRef(jf);
    }

    jobjectArray jarr = (jobjectArray)s.env->CallObjectMethod(activity, mid, juri, jfilters);
    if (!jarr) {
        s.env->DeleteLocalRef(juri);
        s.env->DeleteLocalRef(jfilters);
        return out;
    }

    jsize n = s.env->GetArrayLength(jarr);
    out.reserve(n);
    for (jsize i = 0; i < n; ++i) {
        jstring jentry = (jstring)s.env->GetObjectArrayElement(jarr, i);
        if (!jentry) continue;
        const char* cstr = s.env->GetStringUTFChars(jentry, nullptr);
        if (cstr) {
            // Expect "D|name|uri" or "F|name|uri"
            std::string line(cstr);
            s.env->ReleaseStringUTFChars(jentry, cstr);
            size_t p1 = line.find('|');
            size_t p2 = (p1 == std::string::npos) ? std::string::npos : line.find('|', p1 + 1);
            if (p1 != std::string::npos && p2 != std::string::npos) {
                char kind = line[0];
                std::string name = line.substr(p1 + 1, p2 - (p1 + 1));
                std::string uri = line.substr(p2 + 1);
                out.push_back({ kind == 'D', name, uri });
            }
        }
        s.env->DeleteLocalRef(jentry);
    }

    s.env->DeleteLocalRef(jarr);
    s.env->DeleteLocalRef(jfilters);
    s.env->DeleteLocalRef(juri);
    return out;
}

std::string openfile::Android_CopyDocumentToCache(const std::string& doc_uri)
{
    JniEnvScope s; if (!s.ok()) return {};
    jobject activity = ME_GetActivity();
    jclass cls = GetActivityClass(s.env);
    jmethodID mid = s.env->GetMethodID(cls, "copyDocumentToCache", "(Ljava/lang/String;)Ljava/lang/String;");
    if (!mid) return {};
    jstring juri = s.env->NewStringUTF(doc_uri.c_str());
    jstring jres = (jstring)s.env->CallObjectMethod(activity, mid, juri);
    s.env->DeleteLocalRef(juri);
    if (!jres) return {};
    const char* cstr = s.env->GetStringUTFChars(jres, nullptr);
    std::string out = cstr ? cstr : "";
    s.env->ReleaseStringUTFChars(jres, cstr);
    s.env->DeleteLocalRef(jres);
    return out;
}
#endif // __ANDROID__

/// Iterate a directory
void openfile::iterateDirectory(const std::string& path, std::vector<std::filesystem::path>& files, std::vector<std::filesystem::path>& directories, std::vector<std::string> filters)
{
#if defined(__ANDROID__)
    // SAF-backed pseudo path?
    auto abs_key = std::filesystem::absolute(std::filesystem::path(path)).string();
    auto itroot = saf_map_.find(abs_key);
    if (itroot != saf_map_.end()) {
        files.clear();
        directories.clear();

        auto entries = Android_ListChildren(itroot->second, filters);
        std::filesystem::path base(abs_key);

        for (auto& e : entries) {
            std::filesystem::path p = base / e.name;
            if (e.is_dir) {
                directories.push_back(std::filesystem::absolute(p));
            }
            else {
                files.push_back(std::filesystem::absolute(p));
            }
            // Map this child pseudo path -> its URI
            saf_map_[std::filesystem::absolute(p).string()] = e.uri;
        }

        std::sort(directories.begin(), directories.end(), [](const auto& a, const auto& b)
            {
                return caseInsensitiveCompare(a.filename().string(), b.filename().string());
            });
        std::sort(files.begin(), files.end(), [](const auto& a, const auto& b)
            {
                return caseInsensitiveCompare(a.filename().string(), b.filename().string());
            });
        return;
    }
#endif

    try {
        files.clear();
        directories.clear();

        std::filesystem::path dirPath(path);
        auto dir = std::filesystem::absolute(dirPath);

        if (!std::filesystem::exists(dir)) {
            std::cerr << "OpenFile::iterateDirectory Directory does not exist: " << dir << std::endl;
            return;
        }

        if (!std::filesystem::is_directory(dir)) {
            std::cerr << "OpenFile::iterateDirectory Path is not a directory: " << dir << std::endl;
            return;
        }

        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            auto fullpath = std::filesystem::absolute(entry);
            if (entry.is_regular_file()) {
                if (!filters.empty()) {
                    std::string ext = fullpath.extension().string();
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

        std::sort(directories.begin(), directories.end(), [](const auto& a, const auto& b)
            {
                return caseInsensitiveCompare(a.filename().string(), b.filename().string());
            });
        std::sort(files.begin(), files.end(), [](const auto& a, const auto& b)
            {
                return caseInsensitiveCompare(a.filename().string(), b.filename().string());
            });
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "OpenFile::iterateDirectory Filesystem error in " << path << ": " << e.what() << std::endl;
    }
}

openfile::openfile(std::string title, std::string startdir, std::vector<std::string> filefilters /* = {} */)
    : title(std::move(title)), startdir(std::move(startdir)), filefilters(std::move(filefilters))
{
    auto dir = std::filesystem::path(this->startdir);

    if (!std::filesystem::exists(dir)) {
        std::cerr << "OpenFile: Directory does not exist: " << dir << std::endl;
        try {
            std::filesystem::create_directories(dir);
            std::cout << "OpenFile: Created directory: " << dir << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "OpenFile: Failed to create directory: " << e.what() << std::endl;
            dir = std::filesystem::current_path();
        }
    }

    currentdir = std::filesystem::absolute(dir);
}

openfile::Result openfile::show(bool& show)
{
    Result result = Closed;

    if (ImGui::Begin(title.c_str(), &show)) {
        auto avail = ImGui::GetContentRegionAvail();
        result = None;

        if (openfile_items.empty()) {
            BuildOpenFiles();
            dirs.clear();
        }
        if (dirs.empty()) {
            dirs = BuildDirs();
        }

        if (ImGui::Button("Up")) {
            directoryHistory.push(currentdir);
#if defined(__ANDROID__)
            auto parent = std::filesystem::absolute(currentdir.parent_path());
            // If in SAF root (/saf/Downloads), going up would leave SAF; bounce to startdir instead.
            if (isSafPath(currentdir) && !isSafPath(parent)) {
                currentdir = std::filesystem::absolute(std::filesystem::path(startdir));
            }
            else {
                currentdir = parent;
            }
#else
            currentdir = std::filesystem::absolute(currentdir.parent_path());
#endif
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
            while (!backHistory.empty()) backHistory.pop();
            while (!directoryHistory.empty()) directoryHistory.pop();
            currentdir = std::filesystem::path(startdir);
            openfile_items.clear();
        }
        ImGui::SameLine();

#if defined(__ANDROID__)
        if (ImGui::Button("Downloads")) {
            // If we already have a persisted Downloads tree, jump there.
            std::string uri = Android_GetDownloadsTreeUri();
            if (uri.empty()) {
                Android_RequestDownloadsAccess();
            }
            else {
                std::filesystem::path safRoot("/saf/Downloads");
                saf_map_[std::filesystem::absolute(safRoot).string()] = uri;
                while (!backHistory.empty()) backHistory.pop();
                while (!directoryHistory.empty()) directoryHistory.pop();
                currentdir = std::filesystem::absolute(safRoot);
                openfile_items.clear();
            }
        }
#endif

        // Path crumbs
        for (auto& dir : dirs | std::views::reverse) {
#if defined(__ANDROID__)
            if (dir.first == "Downloads")
                continue;
#endif
            if (ImGui::Button(dir.first.c_str())) {
                while (!backHistory.empty()) backHistory.pop();
                directoryHistory.push(currentdir);
                currentdir = dir.second;
                openfile_items.clear();
            }
            ImGui::SameLine();
        }

        ImGui::Spacing();
        ImGui::Text("%s", currentdir.string().c_str());

        ImVec4 background_color = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
        float luminance = 0.2126f * background_color.x + 0.7152f * background_color.y + 0.0722f * background_color.z;
        bool is_dark_theme = luminance < 0.5f;

        if (ImGui::BeginListBox("##listbox", ImVec2{ -25 , -50 })) {
            for (int i = 0; i < (int)openfile_items.size(); i++) {
                auto item = openfile_items[i].c_str();
                const bool is_selected = (item_selected_idx == i);

                if (i < (int)directories.size()) {
                    if (is_dark_theme) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.39f, 0.71f, 1.0f, 1.0f));
                    }
                    else {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.5f, 1.0f, 1.0f));
                    }
                }
                if (ImGui::Selectable(item, is_selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                    bool doubleclick = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
                    item_selected_idx = i;

                    if (item_selected_idx < (int)directories.size()) {
                        selecteditem = directories[item_selected_idx];
                    }
                    else {
                        selecteditem = files[item_selected_idx - (int)directories.size()];
                    }

                    if (doubleclick) {
                        HandleOpen(result, show);
                    }
                }
                if (i < (int)directories.size()) {
                    ImGui::PopStyleColor();
                }

                if (item_highlight && ImGui::IsItemHovered())
                    item_highlighted_idx = i;

                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
        }
        ImGui::EndListBox();
        ImGui::Separator();

        std::string file;
        if (item_selected_idx >= 0 && item_selected_idx >= (int)directories.size() && item_selected_idx < (int)openfile_items.size()) {
            file = (selecteditem.filename().string());
        }
        ImGui::SetNextItemWidth(avail.x - 175);
        ImGui::LabelText("##File", "File: %s", file.c_str());
        ImGui::SameLine(); if (ImGui::Button("Open")) { HandleOpen(result, show); }
        ImGui::SameLine(); if (ImGui::Button("Cancel")) { show = false; }
    }

    ImGui::End();
    return result;
}
