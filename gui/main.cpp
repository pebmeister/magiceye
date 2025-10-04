// written by Paul Baxter (refactored UI)
// Dear ImGui + GLFW + OpenGL3
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <locale>
#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h>
#include <filesystem>
#include <thread>
#include <atomic>
#include <future>
#include <sstream>
#include <iostream>
#include <cmath>
#include <cstring>

// stb_image for texture loading
// (only include header for prototypes; implementation should be compiled elsewhere in your project)
#include "stb_image.h"

#ifdef __linux__
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

#include "customwidgets.h"
#include "Options.h"
#include "openfile.h"
#include "StereogramGenerator.h"

namespace fs = std::filesystem;

// Globals
static int g_img_w = 0, g_img_h = 0;
static bool g_has_result = false;
static GLuint g_tex_sirds = 0;
static GLuint g_tex_depth = 0;

static std::future<bool> g_render_future;
static std::atomic<bool> g_is_rendering{ false };
static std::string g_rendered_image_path;
static std::string g_rendered_depth_path;

// Forward declarations
static void glfw_error_callback(int error, const char* description);
static bool LoadTextureFromFile(const char* filename, GLuint* out_texture, int* out_width, int* out_height);
static fs::path resolve_path(const fs::path& input_path);

// UI helpers
static void SetupSexyStyle(float scale);
static bool KnobID(const char* id, const char* visible_label, float* v, float v_min, float v_max, float size = 62.0f, int bar_segments = 64, float bar_height = 6.0f, float bar_gap = 2.0f);
static bool KnobIntID(const char* id, const char* visible_label, int* v, int v_min, int v_max, float size = 62.0f);
static bool Knob(const char* label, float* v, float v_min, float v_max, float size = 62.0f, int bar_segments = 64, float bar_height = 6.0f, float bar_gap = 2.0f);

static bool KnobInt(const char* label, int* v, int v_min, int v_max, float size = 62.0f);
static ImVec2 FitInto(const ImVec2& avail, int image_w, int image_h);
static void DrawViewport(bool* open, bool has_result, GLuint tex_sirds, GLuint tex_depth, int img_w, int img_h, int* tab_idx);
static void DrawInspector(Options* opt, bool& show_stl_openfile, openfile& stl_openfile_dialog, bool& show_texture_openfile, openfile& texture_openfile_dialog);
static void HandleRenderCompletion(Options* opt);

// Small wrapper to use std::string with InputTextWithHint without imgui_stdlib
static bool InputTextWithHintStr(const char* label, const char* hint, std::string& str, ImGuiInputTextFlags flags = 0)
{
    char buf[1024];
    size_t len = str.size();
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    std::memcpy(buf, str.c_str(), len);
    buf[len] = '\0';

    bool changed = ImGui::InputTextWithHint(label, hint, buf, sizeof(buf), flags);
    if (changed)
        str = buf;
    return changed;
}

// Main
int main(int, char**)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

#if defined(IMGUI_IMPL_OPENGL_ES2)
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(IMGUI_IMPL_OPENGL_ES3)
    const char* glsl_version = "#version 300 es";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
    GLFWwindow* window = glfwCreateWindow((int)(1280 * main_scale), (int)(800 * main_scale), "Magic Eye", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    // No docking: your ImGui version doesn't have docking branch APIs.

    SetupSexyStyle(main_scale);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);

    // App state
    bool openMain = true;
    bool show_stl_openfile = false;
    bool show_texture_openfile = false;

    // Root folder detection
    std::string sep(1, std::filesystem::path::preferred_separator);
    std::string root;
#ifdef _WIN32
#pragma comment(lib, "shell32.lib")
    {
        CHAR my_documents[MAX_PATH];
        HRESULT result = SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, my_documents);
        (void)result;
        root = std::string(my_documents) + sep;
    }
#endif
#ifdef __linux__
    {
        struct passwd* pw = getpwuid(getuid());
        const char* homedir = pw ? pw->pw_dir : nullptr;
        if (homedir && *homedir)
            root = std::string(homedir) + sep + "Documents" + sep;
        else
            root = std::string("./");
    }
#endif
#ifdef __APPLE__
    {
        const char* home = getenv("HOME");
        if (home && *home)
            root = std::string(home) + sep + "Documents" + sep;
        else
            root = std::string("./");
    }
#endif

    auto stlpath = resolve_path(std::filesystem::absolute(root + "stl"));
    auto texturepath = resolve_path(std::filesystem::absolute(root + "texture"));
    openfile stl_openfile_dialog("Open Mesh", stlpath.string(), { ".stl", ".obj" });
    openfile texture_openfile_dialog("Open Texture", texturepath.string(), { ".png", ".jpg", ".jpeg", ".bmp" });

    auto options = std::make_shared<Options>();
    int viewport_tab = 0; // 0=SIRDS, 1=Depth
    bool viewport_open = true;

#ifndef __EMSCRIPTEN__
    while (openMain && !glfwWindowShouldClose(window))
#else
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#endif
    {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Main menu bar at top
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open Mesh...", "M")) show_stl_openfile = true;
                if (ImGui::MenuItem("Open Texture...", "T")) show_texture_openfile = true;
                if (ImGui::MenuItem("Quit", "Alt+F4")) openMain = false;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Viewport", nullptr, &viewport_open);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // Inspector panel
        ImGui::SetNextWindowPos(ImVec2(20, 60), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(420, 600), ImGuiCond_FirstUseEver);
        ImGui::Begin("Inspector - Magic Eye");
        DrawInspector(options.get(), show_stl_openfile, stl_openfile_dialog, show_texture_openfile, texture_openfile_dialog);
        ImGui::End();

        // Viewport panel
        if (viewport_open) {
            ImGui::SetNextWindowPos(ImVec2(460, 60), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(800, 800), ImGuiCond_FirstUseEver);
            DrawViewport(&viewport_open, g_has_result, g_tex_sirds, g_tex_depth, g_img_w, g_img_h, &viewport_tab);
        }

        // Async render completion
        HandleRenderCompletion(options.get());

        // File dialogs (position near top-left)
        if (show_stl_openfile) {
            ImGui::SetNextWindowPos(ImVec2(40, 90), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(560, 700), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowFocus();
            auto result = stl_openfile_dialog.show(show_stl_openfile);
            if (result == openfile::FileSelected)
                options->stlpath = stl_openfile_dialog.selecteditem.string();
        }
        if (show_texture_openfile) {
            ImGui::SetNextWindowPos(ImVec2(40, 90), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(560, 700), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowFocus();
            auto result = texture_openfile_dialog.show(show_texture_openfile);
            if (result == openfile::FileSelected)
                options->texpath = texture_openfile_dialog.selecteditem.string();
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        ImVec4 clear_color = ImVec4(0.05f, 0.06f, 0.09f, 1.0f);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // Cleanup
    if (g_tex_sirds) { glDeleteTextures(1, &g_tex_sirds); g_tex_sirds = 0; }
    if (g_tex_depth) { glDeleteTextures(1, &g_tex_depth); g_tex_depth = 0; }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

// -------------- Helpers & UI --------------

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Load image file to OpenGL texture
static bool LoadTextureFromFile(const char* filename, GLuint* out_texture, int* out_width, int* out_height)
{
    int w = 0, h = 0;
    unsigned char* image_data = stbi_load(filename, &w, &h, NULL, 4);
    if (!image_data) return false;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#ifdef GL_UNPACK_ROW_LENGTH
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    stbi_image_free(image_data);

    *out_texture = tex;
    *out_width = w;
    *out_height = h;
    return true;
}

static fs::path resolve_path(const fs::path& input_path)
{
    try {
        fs::path absolute_path = fs::absolute(input_path);
        if (fs::exists(absolute_path)) {
            return fs::canonical(absolute_path);
        }
        fs::path resolved_path;
        for (const auto& part : absolute_path) {
            if (part == "..") {
                if (!resolved_path.empty() && resolved_path != resolved_path.root_path())
                    resolved_path = resolved_path.parent_path();
            }
            else if (part != "." && !part.empty()) {
                resolved_path /= part;
            }
        }
        return resolved_path;
    }
    catch (const std::exception& e) {
        std::cerr << "Error resolving path: " << e.what() << std::endl;
        return input_path;
    }
}

// Theme setup
static void SetupSexyStyle(float scale)
{
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();
    ImGuiStyle& s = ImGui::GetStyle();
    s.ScaleAllSizes(scale);

    s.WindowRounding = 10.0f;
    s.ChildRounding = 8.0f;
    s.FrameRounding = 8.0f;
    s.GrabRounding = 8.0f;
    s.PopupRounding = 8.0f;
    s.ScrollbarRounding = 8.0f;
    s.TabRounding = 8.0f;

    ImVec4 accent(0.19f, 0.69f, 0.92f, 1.0f);
    ImVec4 accent2(0.74f, 0.35f, 0.98f, 1.0f);
    ImVec4 bg(0.10f, 0.11f, 0.14f, 1.0f);
    ImVec4 bg2(0.13f, 0.14f, 0.18f, 1.0f);
    ImVec4 text(0.95f, 0.96f, 0.98f, 1.0f);
    ImVec4 muted(0.70f, 0.72f, 0.78f, 1.0f);

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = bg;
    c[ImGuiCol_ChildBg] = bg2;
    c[ImGuiCol_PopupBg] = ImVec4(0.09f, 0.10f, 0.13f, 1.0f);
    c[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.19f, 0.23f, 1.0f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.24f, 0.28f, 1.0f);
    c[ImGuiCol_FrameBgActive] = accent;
    c[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.09f, 0.12f, 1.0f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.14f, 0.18f, 1.0f);
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.09f, 0.12f, 1.0f);
    c[ImGuiCol_Header] = ImVec4(0.18f, 0.20f, 0.25f, 1.0f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.21f, 0.23f, 0.28f, 1.0f);
    c[ImGuiCol_HeaderActive] = accent;
    c[ImGuiCol_CheckMark] = accent;
    c[ImGuiCol_SliderGrab] = accent;
    c[ImGuiCol_SliderGrabActive] = accent2;
    c[ImGuiCol_Button] = ImVec4(0.18f, 0.20f, 0.25f, 1.0f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.21f, 0.23f, 0.28f, 1.0f);
    c[ImGuiCol_ButtonActive] = accent;
    c[ImGuiCol_Separator] = ImVec4(0.20f, 0.22f, 0.26f, 1.0f);
    c[ImGuiCol_Tab] = ImVec4(0.16f, 0.18f, 0.22f, 1.0f);
    c[ImGuiCol_TabHovered] = ImVec4(0.20f, 0.22f, 0.26f, 1.0f);
    c[ImGuiCol_TabActive] = ImVec4(0.26f, 0.29f, 0.36f, 1.0f);
    c[ImGuiCol_Text] = text;
    c[ImGuiCol_TextDisabled] = ImVec4(muted.x, muted.y, muted.z, 1.0f);
}

// Knob wrappers
static bool KnobID(const char* id, const char* visible_label, float* v, float v_min, float v_max, float size, int bar_segments, float bar_height, float bar_gap)
{
    ImGui::PushID(id);
    bool changed = CustomWidgets::KnobWithIndicator(visible_label, v, v_min, v_max, size, bar_segments, bar_height, bar_gap);
    ImGui::PopID();
    return changed;
}
static bool KnobIntID(const char* id, const char* visible_label, int* v, int v_min, int v_max, float size)
{
    float tmp = (float)*v;
    ImGui::PushID(id);
    bool changed = CustomWidgets::KnobWithIndicator(visible_label, &tmp, (float)v_min, (float)v_max, size);
    ImGui::PopID();
    if (changed) {
        int nv = (int)lroundf(tmp);
        nv = (nv < v_min ? v_min : (nv > v_max ? v_max : nv));
        if (nv != *v) { *v = nv; changed = true; }
    }
    return changed;
}

static bool Knob(const char* label, float* v, float v_min, float v_max, float size, int bar_segments, float bar_height, float bar_gap)
{
    return CustomWidgets::KnobWithIndicator(label, v, v_min, v_max, size, bar_segments, bar_height, bar_gap);
}
static bool KnobInt(const char* label, int* v, int v_min, int v_max, float size)
{
    float tmp = (float)*v;
    bool changed = Knob(label, &tmp, (float)v_min, (float)v_max, size);
    if (changed) {
        int nv = (int)lroundf(tmp);
        nv = (nv < v_min ? v_min : (nv > v_max ? v_max : nv));
        if (nv != *v) { *v = nv; changed = true; }
    }
    return changed;
}

// Fit image into region
static ImVec2 FitInto(const ImVec2& avail, int image_w, int image_h)
{
    if (image_w <= 0 || image_h <= 0) return ImVec2(0, 0);
    float ar = (float)image_w / (float)image_h;
    float target_w = avail.x;
    float target_h = target_w / ar;
    if (target_h > avail.y) {
        target_h = avail.y;
        target_w = target_h * ar;
    }
    return ImVec2(target_w, target_h);
}

// Viewport window
static void DrawViewport(bool* open, bool has_result, GLuint tex_sirds, GLuint tex_depth, int img_w, int img_h, int* tab_idx)
{
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
    if (!ImGui::Begin("Viewport - Magic Eye", open, flags)) { ImGui::End(); return; }

    ImGui::Text("Preview");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 2));

    if (ImGui::BeginTabBar("preview_tabs", ImGuiTabBarFlags_NoCloseWithMiddleMouseButton)) {
        if (ImGui::BeginTabItem("SIRDS")) { *tab_idx = 0; ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Depth")) { *tab_idx = 1; ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }

    ImGui::BeginChild("preview_area", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollWithMouse);
    if (has_result) {
        if (*tab_idx == 0) {
            ImGui::Text("%s", g_rendered_image_path.c_str());
        }
        else {
            ImGui::Text("%s", g_rendered_depth_path.c_str());
        }
    }
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (!has_result) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 0.6f));
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8);
        ImGui::TextWrapped("Load a mesh and texture, adjust settings in Inspector, then click Render.");
        ImGui::PopStyleColor();
    }
    else {
        ImVec2 sz = FitInto(avail, img_w, img_h);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail.x - sz.x) * 0.5f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (avail.y - sz.y) * 0.5f);
        GLuint t = (*tab_idx == 0 ? tex_sirds : tex_depth);
        if (t)
            ImGui::Image((void*)(intptr_t)t, sz);
    }
    ImGui::EndChild();

    ImGui::End();
}

// Inspector window content
static void DrawInspector(Options* opt, bool& show_stl_openfile, openfile& stl_openfile_dialog, bool& show_texture_openfile, openfile& texture_openfile_dialog)
{
    ImGuiStyle& style = ImGui::GetStyle();
    const float line = ImGui::GetTextLineHeightWithSpacing();

    // CARD: Sources (explicit height to avoid eating all remaining space)
    ImGui::BeginChild("card_sources", ImVec2(0, line * 14.0f), true, ImGuiWindowFlags_None /* ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse*/);
    {
        ImGui::Text("Sources");
        ImGui::Separator();

        // Mesh path
        ImGui::PushID("mesh");
        ImGui::TextUnformatted("Mesh");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 90.0f);
        InputTextWithHintStr("##path", "Select mesh...", opt->stlpath, ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();
        if (ImGui::Button("Browse")) show_stl_openfile = true;
        ImGui::PopID();

        // Texture path
        ImGui::PushID("texture");
        ImGui::TextUnformatted("Texture");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 90.0f);
        InputTextWithHintStr("##path", "Select texture...", opt->texpath, ImGuiInputTextFlags_ReadOnly);
        ImGui::SameLine();
        if (ImGui::Button("Browse")) show_texture_openfile = true;
        ImGui::PopID();

        ImGui::Dummy(ImVec2(0, 4));
        ImGui::Separator();

        // Resolution
        ImGui::TextUnformatted("Resolution");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        CustomWidgets::InputInt2("##res", &opt->width);
        ImGui::SameLine();
        if (ImGui::BeginMenu("Preset")) {
            if (ImGui::MenuItem("1280 x 800")) { opt->width = 1280; opt->height = 800; }
            if (ImGui::MenuItem("1920 x 1080")) { opt->width = 1920; opt->height = 1080; }
            if (ImGui::MenuItem("2560 x 1440")) { opt->width = 2560; opt->height = 1440; }
            if (ImGui::MenuItem("3840 x 2160 (4K)")) { opt->width = 3840; opt->height = 2160; }
            ImGui::EndMenu();
        }

        ImGui::Dummy(ImVec2(0, 4));
        // Eye separation knob (no raw ## label shown)
        KnobIntID("eye_sep", "-- Eye separation", &opt->eye_sep, 0, 250, 62.0f);
        //ImGui::SameLine();
        //ImGui::Text(" %d", opt->eye_sep);
    }
    ImGui::EndChild();
    ImGui::Dummy(ImVec2(0, 6));

    // CARD: Camera (explicit height; removes big gap before camera)
    ImGui::BeginChild("card_camera", ImVec2(0, line * 19.0f), true, ImGuiWindowFlags_None /* ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse */);
    {
        ImGui::Text("Camera");
        ImGui::Separator();

        // Perspective / Ortho
        ImGui::Checkbox("Perspective", &opt->perspective);
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        if (opt->perspective) {
            ImGui::BeginDisabled(!opt->perspective);
            KnobID("fov", "FOV", &opt->fov, 10.0f, 120.0f, 62.0f);
            //ImGui::SameLine();
            //ImGui::Text("%.0f deg", opt->fov);
            ImGui::EndDisabled();
        }
        else {

            ImGui::BeginDisabled(opt->perspective);
            ImGui::Checkbox("Use Custom Ortho scale", &opt->custom_orth_scale_provided);
            ImGui::BeginDisabled(!opt->custom_orth_scale_provided);
            KnobID("orth", "Ortho scale", &opt->custom_orth_scale, 1.0f, 300.0f, 62.0f);
            //ImGui::SameLine();
            //ImGui::Text("%.1f", opt->custom_orth_scale);
            ImGui::EndDisabled();
            ImGui::EndDisabled();
        }

        ImGui::Dummy(ImVec2(0, 5));

        // Custom camera pos (aligned like Rotation)
        ImGui::Checkbox("Use custom camera pos", &opt->custom_cam_provided);
        ImGui::BeginDisabled(!opt->custom_cam_provided);
        CustomWidgets::InputFloat3("Camera pos", &opt->custom_cam_pos[0]);
        ImGui::EndDisabled();

        // Custom look-at (aligned like Rotation)
        ImGui::Checkbox("Use custom look-at", &opt->custom_lookat_provided);
        ImGui::BeginDisabled(!opt->custom_lookat_provided);
        CustomWidgets::InputFloat3("Look at", &opt->custom_look_at[0]);
        ImGui::EndDisabled();

        ImGui::Dummy(ImVec2(0, 4));
        ImGui::TextUnformatted("Depth range (Near/Far)");
        // ImGui::SetNextItemWidth(-1);
        float minv = opt->depth_near;
        float maxv = opt->depth_far;
        if (ImGui::DragFloatRange2("##clip", &minv, &maxv, 0.01f, 0.0f, 1000.0f, "N: %.2f", "F: %.2f")) {
            opt->depth_near = minv;
            opt->depth_far = maxv;
        }
    }
    ImGui::EndChild();
    ImGui::Dummy(ImVec2(0, 6));

    // CARD: Transform (explicit height)
    ImGui::BeginChild("card_transform", ImVec2(0, line * 8.0f), true, ImGuiWindowFlags_None);
    {
        ImGui::Text("Transform");
        ImGui::Separator();

        // ImGui::SetNextItemWidth(-1);
        CustomWidgets::InputFloat3("Rotation", &opt->rot_deg[0]);

        // ImGui::SetNextItemWidth(-1);
        CustomWidgets::InputFloat3("Translation", &opt->trans[0]);

        // ImGui::SetNextItemWidth(-1);
        CustomWidgets::InputFloat3("Scale", &opt->sc[0]);

        // ImGui::SetNextItemWidth(-1);
        CustomWidgets::InputFloat3("Shear", &opt->shear[0]);
    }
    ImGui::EndChild();
    ImGui::Dummy(ImVec2(0, 6));

    // CARD: Stereo & Texture (explicit height)
    ImGui::BeginChild("card_stereo_tex", ImVec2(0, line * 17.0f), true, ImGuiWindowFlags_None);
    {
        ImGui::Text("Stereo & Texture");
        ImGui::Separator();

        // Row 1: Brightness / Contrast / Separation
        {
            KnobID("bright", "Brightness", &opt->texture_brightness, 0.2f, 3.0f, 56.0f);
            ImGui::SameLine(0, 24);
            KnobID("contrast", "Contrast", &opt->texture_contrast, 0.2f, 3.0f, 56.0f);

            ImGui::SameLine(0, 24);
            KnobID("sep", "Separation", &opt->bg_separation, 0.0f, 2.5f, 56.0f);
        }

        ImGui::Dummy(ImVec2(0, 6));
        // Row 2: Depth gamma / Foreground
        {
            KnobID("dg", "-  Depth gamma", &opt->depth_gamma, 0.1f, 5.0f, 56.0f);

            ImGui::SameLine(0, 24);
            KnobID("fg", "Foreground", &opt->foreground_threshold, 0.1f, 1.0f, 56.0f);
        }

        ImGui::Dummy(ImVec2(0, 6));
        ImGui::TextUnformatted("Ortho tune (Low/High)");
        ImGui::SetNextItemWidth(-1);
        float lo = opt->orthTuneLow;
        float hi = opt->orthTuneHi;
        if (ImGui::DragFloatRange2("##orth_tune", &lo, &hi, 0.01f, 0.1f, 5.0f, "L: %.2f", "H: %.2f")) {
            opt->orthTuneLow = lo;
            opt->orthTuneHi = hi;
        }
    }
    ImGui::EndChild();
    ImGui::Dummy(ImVec2(0, 6));

    // CARD: Edges & Smoothing (fills remaining space)
    ImGui::BeginChild("card_edges", ImVec2(0, 0), true);
    {
        ImGui::Text("Edges & Smoothing");
        ImGui::Separator();

        ImGui::Checkbox("Smooth edges", &opt->smoothEdges);
        ImGui::BeginDisabled(!opt->smoothEdges);
        {
            KnobID("sth", "-  Smooth thresh", &opt->smoothThreshold, 0.0f, 1.0f, 56.0f);

            ImGui::SameLine(0, 24);
            KnobID("sw", "Smooth weight", &opt->smoothWeight, 1.0f, 20.0f, 56.0f);
        }
        ImGui::EndDisabled();

        ImGui::Dummy(ImVec2(0, 4));
        ImGui::Checkbox("Use Laplace smoothing", &opt->laplace_smoothing);
        ImGui::BeginDisabled(!opt->laplace_smoothing);
        ImGui::SetNextItemWidth(160);
        CustomWidgets::InputInt("Laplace layers", &opt->laplace_smooth_layers);
        ImGui::EndDisabled();

        ImGui::Dummy(ImVec2(0, 4));
        ImGui::TextUnformatted("Ramp (Width / Height)");
        {
            KnobID("rw", "Width", &opt->rampWidth, 0.0f, 20.0f, 56.0f);
            ImGui::SameLine(0, 24);
            KnobID("rh", "Height", &opt->rampHeight, 0.0f, 200.0f, 56.0f);
        }
    }
    ImGui::EndChild();
    ImGui::Dummy(ImVec2(0, 10));

    // Footer: Reset & Render
    bool disabled_render = (opt->stlpath.empty() || opt->texpath.empty());
    ImGui::BeginDisabled(disabled_render || g_is_rendering);
    if (ImGui::Button("Render", ImVec2(ImGui::GetContentRegionAvail().x * 0.55f, 0))) {
        g_is_rendering = true;
        g_has_result = false;
        if (g_tex_sirds) { glDeleteTextures(1, &g_tex_sirds); g_tex_sirds = 0; }
        if (g_tex_depth) { glDeleteTextures(1, &g_tex_depth); g_tex_depth = 0; }

        fs::path out = fs::absolute(fs::path(opt->stlpath).replace_extension(""));
        opt->outprefix = out.string();

        g_render_future = std::async(std::launch::async, [o = std::make_shared<Options>(*opt)]() mutable
            {
                StereogramGenerator st(o);
                bool ok = !st.create();
                if (ok) {
                    g_rendered_image_path = o->outprefix + "_sirds.png";
                    g_rendered_depth_path = o->outprefix + "_depth.png";
                    return true;
                }
                return false;
            });
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Reset", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        std::string stl = opt->stlpath;
        std::string tex = opt->texpath;
        Options reset;
        reset.stlpath = stl;
        reset.texpath = tex;
        *opt = reset;
    }

    if (g_is_rendering) {
        ImGui::SameLine();
        CustomWidgets::LoadingSpinner("##spinner", 14.0f, 4);
        ImGui::SameLine();
        ImGui::TextUnformatted("Rendering...");
    }
}

// Render future handling
static void HandleRenderCompletion(Options* opt)
{
    if (!g_is_rendering) return;
    if (!g_render_future.valid()) return;

    if (g_render_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        bool success = g_render_future.get();
        if (success) {
            GLuint tex1 = 0, tex2 = 0;
            int w1 = 0, h1 = 0;
            int w2 = 0, h2 = 0;

            bool ok1 = LoadTextureFromFile(g_rendered_image_path.c_str(), &tex1, &w1, &h1);
            bool ok2 = LoadTextureFromFile(g_rendered_depth_path.c_str(), &tex2, &w2, &h2);

            if (ok1 && ok2) {
                g_tex_sirds = tex1;
                g_tex_depth = tex2;
                g_img_w = w1;
                g_img_h = h1;
                g_has_result = true;
            }
            else {
                if (tex1) glDeleteTextures(1, &tex1);
                if (tex2) glDeleteTextures(1, &tex2);
                g_has_result = false;
            }
        }
        g_is_rendering = false;
    }
}
