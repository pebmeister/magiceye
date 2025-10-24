// dear imgui: standalone application for Android + OpenGL ES 3

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "imgui.h"
#include "imgui_impl_android.h"
#include "imgui_impl_opengl3.h"
#include <algorithm>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <android/asset_manager.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <string>
#include <atomic>
#include <future>
#include <chrono>
#include <filesystem>
#include <cstring>
#include <cmath>
#include <memory>
#include <iostream>
#include <vector>

// MagicEye includes
#include "vec3.h"
#include "customwidgets.h"
#include "Options.h"
#include "openfile.h"
#include "StereogramGenerator.h"

// File system
namespace fs = std::filesystem;

// Globals
static int g_img_w = 0, g_img_h = 0;
static bool g_has_result = false;
static GLuint g_tex_sirds = 0;
static GLuint g_tex_depth = 0;
static std::string g_render_error_msg;
static std::atomic<bool> g_render_error_pending{ false };

static std::future<bool> g_render_future;
static std::atomic<bool> g_is_rendering{ false };
static std::string g_rendered_image_path;
static std::string g_rendered_depth_path;
static bool g_prev_render = false;

/////////////////////////////

// Data
static EGLDisplay           g_EglDisplay = EGL_NO_DISPLAY;
static EGLSurface           g_EglSurface = EGL_NO_SURFACE;
static EGLContext           g_EglContext = EGL_NO_CONTEXT;
static struct android_app* g_App = nullptr;
static bool                 g_Initialized = false;
static char                 g_LogTag[] = "MagicEye";
static std::string          g_IniFilename = "";

#if defined(__ANDROID__)
extern "C" JavaVM* ME_GetJavaVM() { return g_App ? g_App->activity->vm : nullptr; }
extern "C" jobject ME_GetActivity() { return g_App ? g_App->activity->clazz : nullptr; }
#endif

// Forward declarations of helper functions
static void Init(struct android_app* app);
static void Shutdown();
static void MainLoopStep();
static int ShowSoftKeyboardInput();
static int PollUnicodeChars();
static int GetAssetData(const char* filename, void** out_data);

// Forward declarations
static bool LoadTextureFromFile(const char* filename, GLuint* out_texture, int* out_width, int* out_height);
static fs::path resolve_path(const fs::path& input_path);
static void RequestExit();

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
static fs::path GetWritableBaseDir();
static void Android_ShareCacheFilePath(const std::string& path, const std::string& mime, const std::string& subject);
static void Android_ShareCacheFilePaths(const std::vector<std::string>& paths, const std::string& mime, const std::string& subject);

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

// Main code
static void handleAppCmd(struct android_app* app, int32_t appCmd)
{
    switch (appCmd) {
        case APP_CMD_SAVE_STATE:
            break;
        case APP_CMD_INIT_WINDOW:
            Init(app);
            break;
        case APP_CMD_TERM_WINDOW:
            Shutdown();
            break;
        case APP_CMD_GAINED_FOCUS:
        case APP_CMD_LOST_FOCUS:
            break;
    }
}

static int32_t handleInputEvent(struct android_app* app, AInputEvent* inputEvent)
{
    return ImGui_ImplAndroid_HandleInputEvent(inputEvent);
}

void android_main(struct android_app* app)
{
    app->onAppCmd = handleAppCmd;
    app->onInputEvent = handleInputEvent;

    while (true) {
        int out_events;
        struct android_poll_source* out_data;

        // Poll all events. If the app is not visible, this loop blocks until g_Initialized == true.
        while (ALooper_pollOnce(g_Initialized ? 0 : -1, nullptr, &out_events, (void**)&out_data) >= 0) {
            // Process one event
            if (out_data != nullptr)
                out_data->process(app, out_data);

            // Exit the app by returning from within the infinite loop
            if (app->destroyRequested != 0) {
                // shutdown() should have been called already while processing the
                // app command APP_CMD_TERM_WINDOW. But we play save here
                if (!g_Initialized)
                    Shutdown();

                return;
            }
        }

        // Initiate a new frame
        MainLoopStep();
    }
}

void Init(struct android_app* app)
{
    if (g_Initialized)
        return;

    g_App = app;
    ANativeWindow_acquire(g_App->window);

    // Initialize EGL
    // This is mostly boilerplate code for EGL...
    {
        g_EglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (g_EglDisplay == EGL_NO_DISPLAY)
            __android_log_print(ANDROID_LOG_ERROR, g_LogTag, "%s", "eglGetDisplay(EGL_DEFAULT_DISPLAY) returned EGL_NO_DISPLAY");

        if (eglInitialize(g_EglDisplay, 0, 0) != EGL_TRUE)
            __android_log_print(ANDROID_LOG_ERROR, g_LogTag, "%s", "eglInitialize() returned with an error");

        const EGLint egl_attributes[] = { EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_NONE };
        EGLint num_configs = 0;
        if (eglChooseConfig(g_EglDisplay, egl_attributes, nullptr, 0, &num_configs) != EGL_TRUE)
            __android_log_print(ANDROID_LOG_ERROR, g_LogTag, "%s", "eglChooseConfig() returned with an error");
        if (num_configs == 0)
            __android_log_print(ANDROID_LOG_ERROR, g_LogTag, "%s", "eglChooseConfig() returned 0 matching config");

        // Get the first matching config
        EGLConfig egl_config;
        eglChooseConfig(g_EglDisplay, egl_attributes, &egl_config, 1, &num_configs);
        EGLint egl_format;
        eglGetConfigAttrib(g_EglDisplay, egl_config, EGL_NATIVE_VISUAL_ID, &egl_format);
        ANativeWindow_setBuffersGeometry(g_App->window, 0, 0, egl_format);

        const EGLint egl_context_attributes[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
        g_EglContext = eglCreateContext(g_EglDisplay, egl_config, EGL_NO_CONTEXT, egl_context_attributes);

        if (g_EglContext == EGL_NO_CONTEXT)
            __android_log_print(ANDROID_LOG_ERROR, g_LogTag, "%s", "eglCreateContext() returned EGL_NO_CONTEXT");

        g_EglSurface = eglCreateWindowSurface(g_EglDisplay, egl_config, g_App->window, nullptr);
        eglMakeCurrent(g_EglDisplay, g_EglSurface, g_EglSurface, g_EglContext);
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    // Redirect loading/saving of .ini file to our location.
    // Make sure 'g_IniFilename' persists while we use Dear ImGui.
    g_IniFilename = std::string(app->activity->internalDataPath) + "/imgui.ini";
    io.IniFilename = g_IniFilename.c_str();

    auto main_scale = 1.0;

    SetupSexyStyle(main_scale);

    // Setup Platform/Renderer backends
    ImGui_ImplAndroid_Init(g_App->window);
    ImGui_ImplOpenGL3_Init("#version 300 es");

    // Load Fonts
    ImFontConfig font_cfg;
    font_cfg.SizePixels = 22.0f;
    io.Fonts->AddFontDefault(&font_cfg);

    // Arbitrary scale-up
    // FIXME: Put some effort into DPI awareness
    ImGui::GetStyle().ScaleAllSizes(3.0f);

    g_Initialized = true;
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
        KnobIntID("eye_sep", "-    Eye separation", &opt->eye_sep, 0, 250, 62.0f);
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
            ImGui::EndDisabled();
        }
        else {

            ImGui::BeginDisabled(opt->perspective);
            ImGui::Checkbox("Use Custom Ortho scale", &opt->custom_orth_scale_provided);
            ImGui::BeginDisabled(!opt->custom_orth_scale_provided);
            KnobID("orth", "Ortho scale", &opt->custom_orth_scale, 1.0f, 300.0f, 62.0f);
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
        ImGui::TextUnformatted("Depth range (Far/Near)");
        float n = opt->depth_near;
        float f = opt->depth_far;
        if (ImGui::DragFloatRange2("##clip", &f, &n, 0.01f, 0.0f, 2.0f, "F: %.2f", "N: %.2f")) {
            opt->depth_near = n;
            opt->depth_far = f;
        }
    }
    ImGui::EndChild();
    ImGui::Dummy(ImVec2(0, 6));

    // CARD: Transform (explicit height)
    ImGui::BeginChild("card_transform", ImVec2(0, line * 9.0f), true, ImGuiWindowFlags_None);
    {
        ImGui::Text("Transform");
        ImGui::Separator();

        CustomWidgets::InputFloat3("Rotation", &opt->rot_deg[0]);
        CustomWidgets::InputFloat3("Translation", &opt->trans[0]);
        CustomWidgets::InputFloat3("Scale", &opt->sc[0]);
        CustomWidgets::InputFloat3("Shear", &opt->shear[0]);
    }
    ImGui::EndChild();
    ImGui::Dummy(ImVec2(0, 6));

    // CARD: Stereo & Texture (explicit height)
    ImGui::BeginChild("card_stereo_tex", ImVec2(0, line * 21.0f), true, ImGuiWindowFlags_None);
    {
        ImGui::Text("Stereo & Texture");
        ImGui::Separator();

        // Row 1: Brightness / Contrast / Separation
        {
            KnobID("bright", "  Brightness", &opt->texture_brightness, 0.2f, 3.0f, 56.0f);
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

        ImGui::Dummy(ImVec2(0, 6));
        ImGui::Separator();

        // Texture tiling (repeat/clamp)
        ImGui::SetNextItemWidth(400);
        ImGui::Checkbox("Tile texture", &opt->tile_texture);
        ImGui::SameLine();
        // RNG seed (-1 = random_device)
        ImGui::SetNextItemWidth(160);
        CustomWidgets::InputInt("RNG seed", &opt->rng_seed);

        // Occlusion gate
        ImGui::Checkbox("Occlusion gate", &opt->occlusion);
        ImGui::BeginDisabled(!opt->occlusion);
        ImGui::SetNextItemWidth(160);
        CustomWidgets::InputFloat("Occlusion epsilon", &opt->occlusion_epsilon);
        ImGui::EndDisabled();

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
            KnobID("sth", "-    Smooth thresh", &opt->smoothThreshold, 0.0f, 1.0f, 56.0f);

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
        ImGui::Checkbox("Add floor ramp", &opt->add_floor);
        ImGui::BeginDisabled(!opt->add_floor);
        {
            ImGui::TextUnformatted("Ramp (Width / Height)");
            {
                KnobID("rw", "Width", &opt->rampWidth, 0.0f, 20.0f, 56.0f);
                ImGui::SameLine(0, 24);
                KnobID("ra", "Angle", &opt->rampAngle, 0.0f, 360.0f, 56.0f);
                ImGui::SameLine(0, 24);
                KnobID("rs", "RampSep", &opt->rampSep, 0.0f, 1.0f, 56.0f);
            }
        }
        ImGui::EndDisabled();
    }
    ImGui::EndChild();
    ImGui::Dummy(ImVec2(0, 10));

    // Footer: Reset & Render
    bool disabled_render = (opt->stlpath.empty() || opt->texpath.empty());
    ImGui::BeginDisabled(disabled_render || g_is_rendering);
    if (ImGui::Button("Render", ImVec2(160, 0))) {
        g_is_rendering = true;
        g_has_result = false;
        if (g_tex_sirds) { glDeleteTextures(1, &g_tex_sirds); g_tex_sirds = 0; }
        if (g_tex_depth) { glDeleteTextures(1, &g_tex_depth); g_tex_depth = 0; }

        fs::path out = fs::absolute(fs::path(opt->stlpath).replace_extension(""));
        opt->outprefix = out.string();

        g_render_future = std::async(std::launch::async, [o = std::make_shared<Options>(*opt)]() mutable
            {
                try {
                    StereogramGenerator st(o);
                    bool ok = !st.create();  // 0 == success
                    if (ok) {
                        g_rendered_image_path = o->outprefix + "_sirds.png";
                        g_rendered_depth_path = o->outprefix + "_depth.png";
                        return true;
                    }
                    return false;
                }
                catch (const std::exception& e) {
                    g_render_error_msg = e.what();
                    g_render_error_pending = true;
                    return false;
                }
                catch (...) {
                    g_render_error_msg = "Unknown exception during render.";
                    g_render_error_pending = true;
                    return false;
                }
            });
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Reset", ImVec2(160, 0))) {
        std::string stl = opt->stlpath;
        std::string tex = opt->texpath;
        Options reset;
        reset.stlpath = stl;
        reset.texpath = tex;
        *opt = reset;
    }
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

// Theme setup
static void SetupSexyStyle(float scale)
{
    ImGui::StyleColorsDark();
#ifdef __USE_CUSTOM_THEME__
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
#endif
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
        if (g_is_rendering) {
            ImGui::SameLine();
            CustomWidgets::LoadingSpinner("##spinner", 14.0f, 4);
            ImGui::SameLine();
            ImGui::TextUnformatted("Rendering...");
        }
        else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 0.6f));
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8);
            ImGui::TextWrapped("Load a mesh and texture, adjust settings in Inspector, then click Render.");
            ImGui::PopStyleColor();
        }
    }
    else {
        // Center-fit image
        ImVec2 sz = FitInto(avail, img_w, img_h);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail.x - sz.x) * 0.5f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (avail.y - sz.y) * 0.5f);
        GLuint t = (*tab_idx == 0 ? tex_sirds : tex_depth);
        if (t)
            ImGui::Image((ImTextureID)t, sz);

#if defined(__ANDROID__)
        // Share controls (Android only)
        ImGui::Dummy(ImVec2(0, 10));
        ImGui::Separator();
        ImGui::Text("Share");
        ImGui::BeginDisabled(g_is_rendering || !has_result);
        {
            if (ImGui::Button("Share SIRDS")) {
                Android_ShareCacheFilePath(g_rendered_image_path, "image/png", "MagicEye SIRDS");
            }
            ImGui::SameLine();
            if (ImGui::Button("Share Depth")) {
                Android_ShareCacheFilePath(g_rendered_depth_path, "image/png", "MagicEye Depth");
            }
            ImGui::SameLine();
            if (ImGui::Button("Share both")) {
                std::vector<std::string> paths{ g_rendered_image_path, g_rendered_depth_path };
                Android_ShareCacheFilePaths(paths, "image/*", "MagicEye images");
            }
        }
        ImGui::EndDisabled();
#endif
    }
    ImGui::EndChild();

    ImGui::End();
}

void MainLoopStep()
{
    static auto options = std::make_shared<Options>();
    static int viewport_tab = 0; // 0=SIRDS, 1=Depth
    static bool viewport_open = false;
    static bool show_stl_openfile = false;
    static bool show_texture_openfile = false;
    static std::string root = GetWritableBaseDir().string();

    // Layout management: apply a sensible first layout and when display size changes.
    static bool layout_dirty = true;
    static ImVec2 last_display_size = ImVec2(0, 0);

    static auto stlpath = resolve_path(std::filesystem::absolute(root));
    static auto texturepath = resolve_path(std::filesystem::absolute(root));
    static openfile stl_openfile_dialog("Open Mesh", stlpath.string(), { ".stl", ".obj" });
    static openfile texture_openfile_dialog("Open Texture", texturepath.string(), { ".png", ".jpg", ".jpeg", ".bmp" });

    ImGuiIO& io = ImGui::GetIO();
    if (g_EglDisplay == EGL_NO_DISPLAY)
        return;

    static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Poll Unicode characters via JNI
    // FIXME: do not call this every frame because of JNI overhead
    PollUnicodeChars();

    // Open on-screen (soft) input if requested by Dear ImGui
    static bool WantTextInputLast = false;
    if (io.WantTextInput && !WantTextInputLast)
        ShowSoftKeyboardInput();
    WantTextInputLast = io.WantTextInput;

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();

    // Detect display size change (orientation, resize, etc.) and mark layout dirty
    if (last_display_size.x != io.DisplaySize.x || last_display_size.y != io.DisplaySize.y) {
        layout_dirty = true;
        last_display_size = io.DisplaySize;
    }

    // Main menu bar at top
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open Mesh...", "M")) show_stl_openfile = true;
            if (ImGui::MenuItem("Open Texture...", "T")) show_texture_openfile = true;
            if (ImGui::MenuItem("Quit", "Alt+F4")) RequestExit();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Viewport", nullptr, &viewport_open);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // Compute dynamic layout for Android:
    // - Inspector takes ~1/4 of the width
    // - Viewport takes the rest
    // - Both extend to the bottom with a small margin
    const float margin = 12.0f;
    const float menu_h = ImGui::GetFrameHeight();
    const float top = margin + menu_h;
    const float left = margin;
    const float right = margin;
    const float bottom = margin;
    const float gap = margin;

    const float full_w = io.DisplaySize.x;
    const float full_h = io.DisplaySize.y;
    const float content_h = std::max(1.0f, full_h - top - bottom);

    const float usable_w = std::max(1.0f, full_w - left - right - gap);
    const float inspector_w = usable_w;
    const float viewport_w = usable_w;

    ImGuiCond layout_cond = layout_dirty ? ImGuiCond_Always : ImGuiCond_FirstUseEver;

    // Inspector panel
    ImGui::SetNextWindowPos(ImVec2(left, top), layout_cond);
    ImGui::SetNextWindowSize(ImVec2(inspector_w, content_h), layout_cond);
    ImGui::Begin("Inspector - Magic Eye");
    DrawInspector(options.get(), show_stl_openfile, stl_openfile_dialog, show_texture_openfile, texture_openfile_dialog);
    ImGui::End();
    if (g_prev_render != g_is_rendering) {
        g_prev_render = g_is_rendering;
        if (g_is_rendering) {
            viewport_open = true;
        }
    }

    // Viewport panel
    if (viewport_open) {
        ImGui::SetNextWindowPos(ImVec2(left, top), layout_cond);
        ImGui::SetNextWindowSize(ImVec2(viewport_w, content_h), layout_cond);
        DrawViewport(&viewport_open, g_has_result, g_tex_sirds, g_tex_depth, g_img_w, g_img_h, &viewport_tab);

        if (g_render_error_pending.exchange(false)) {
            ImGui::OpenPopup("Render error");
        }
        if (ImGui::BeginPopupModal("Render error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("%s", g_render_error_msg.c_str());
            ImGui::Dummy(ImVec2(0, 6));
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    // After applying the layout once (on first run or after resize), stop forcing it
    if (layout_dirty) layout_dirty = false;

    // Async render completion
    HandleRenderCompletion(options.get());

    // File dialogs (position near top-left)
    if (show_stl_openfile) {
        ImGui::SetNextWindowPos(ImVec2(left, top), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(std::min(usable_w, 560.0f), std::min(content_h, 700.0f)), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowFocus();
        auto result = stl_openfile_dialog.show(show_stl_openfile);
        if (result == openfile::FileSelected)
            options->stlpath = stl_openfile_dialog.selecteditem.string();
    }
    if (show_texture_openfile) {
        ImGui::SetNextWindowPos(ImVec2(left, top), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(std::min(usable_w, 560.0f), std::min(content_h, 700.0f)), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowFocus();
        auto result = texture_openfile_dialog.show(show_texture_openfile);
        if (result == openfile::FileSelected)
            options->texpath = texture_openfile_dialog.selecteditem.string();
    }

    // Rendering
    ImGui::Render();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    eglSwapBuffers(g_EglDisplay, g_EglSurface);
}

void Shutdown()
{
    if (!g_Initialized)
        return;

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplAndroid_Shutdown();
    ImGui::DestroyContext();

    if (g_EglDisplay != EGL_NO_DISPLAY) {
        eglMakeCurrent(g_EglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

        if (g_EglContext != EGL_NO_CONTEXT)
            eglDestroyContext(g_EglDisplay, g_EglContext);

        if (g_EglSurface != EGL_NO_SURFACE)
            eglDestroySurface(g_EglDisplay, g_EglSurface);

        eglTerminate(g_EglDisplay);
    }

    g_EglDisplay = EGL_NO_DISPLAY;
    g_EglContext = EGL_NO_CONTEXT;
    g_EglSurface = EGL_NO_SURFACE;
    ANativeWindow_release(g_App->window);

    g_Initialized = false;
}

// Helper functions

// Unfortunately, there is no way to show the on-screen input from native code.
// Therefore, we call ShowSoftKeyboardInput() of the main activity implemented in MainActivity.kt via JNI.
static int ShowSoftKeyboardInput()
{
    JavaVM* java_vm = g_App->activity->vm;
    JNIEnv* java_env = nullptr;

    jint jni_return = java_vm->GetEnv((void**)&java_env, JNI_VERSION_1_6);
    if (jni_return == JNI_ERR)
        return -1;

    jni_return = java_vm->AttachCurrentThread(&java_env, nullptr);
    if (jni_return != JNI_OK)
        return -2;

    jclass native_activity_clazz = java_env->GetObjectClass(g_App->activity->clazz);
    if (native_activity_clazz == nullptr)
        return -3;

    jmethodID method_id = java_env->GetMethodID(native_activity_clazz, "showSoftInput", "()V");
    if (method_id == nullptr)
        return -4;

    java_env->CallVoidMethod(g_App->activity->clazz, method_id);

    jni_return = java_vm->DetachCurrentThread();
    if (jni_return != JNI_OK)
        return -5;

    return 0;
}

// Unfortunately, the native KeyEvent implementation has no getUnicodeChar() function.
// Therefore, we implement the processing of KeyEvents in MainActivity.kt and poll
// the resulting Unicode characters here via JNI and send them to Dear ImGui.
static int PollUnicodeChars()
{
    JavaVM* java_vm = g_App->activity->vm;
    JNIEnv* java_env = nullptr;

    jint jni_return = java_vm->GetEnv((void**)&java_env, JNI_VERSION_1_6);
    if (jni_return == JNI_ERR)
        return -1;

    jni_return = java_vm->AttachCurrentThread(&java_env, nullptr);
    if (jni_return != JNI_OK)
        return -2;

    jclass native_activity_clazz = java_env->GetObjectClass(g_App->activity->clazz);
    if (native_activity_clazz == nullptr)
        return -3;

    jmethodID method_id = java_env->GetMethodID(native_activity_clazz, "pollUnicodeChar", "()I");
    if (method_id == nullptr)
        return -4;

    // Send the actual characters to Dear ImGui
    ImGuiIO& io = ImGui::GetIO();
    jint unicode_character;
    while ((unicode_character = java_env->CallIntMethod(g_App->activity->clazz, method_id)) != 0)
        io.AddInputCharacter(unicode_character);

    jni_return = java_vm->DetachCurrentThread();
    if (jni_return != JNI_OK)
        return -5;

    return 0;
}

// Helper to retrieve data placed into the assets/ directory (android/app/src/main/assets)
static int GetAssetData(const char* filename, void** out_data)
{
    int num_bytes = 0;
    AAsset* asset_descriptor = AAssetManager_open(g_App->activity->assetManager, filename, AASSET_MODE_BUFFER);
    if (asset_descriptor) {
        num_bytes = AAsset_getLength(asset_descriptor);
        *out_data = IM_ALLOC(num_bytes);
        int64_t num_bytes_read = AAsset_read(asset_descriptor, *out_data, num_bytes);
        AAsset_close(asset_descriptor);
        IM_ASSERT(num_bytes_read == num_bytes);
    }
    return num_bytes;
}

static void RequestExit()
{
    if (g_App && g_App->activity)
        ANativeActivity_finish(g_App->activity);
}

static fs::path GetWritableBaseDir()
{
    const char* ext = g_App->activity->externalDataPath;
    const char* in = g_App->activity->internalDataPath;
    return fs::path((ext && *ext) ? ext : in);
}

#if defined(__ANDROID__)
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

// Share a single cache file path using MainActivity.shareCacheFilePath(...)
static void Android_ShareCacheFilePath(const std::string& path, const std::string& mime, const std::string& subject)
{
    JniEnvScope s; if (!s.ok()) return;
    jobject activity = ME_GetActivity();
    jclass cls = GetActivityClass(s.env);
    jmethodID mid = s.env->GetMethodID(cls, "shareCacheFilePath", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    if (!mid) return;
    jstring jpath = s.env->NewStringUTF(path.c_str());
    jstring jmime = s.env->NewStringUTF(mime.c_str());
    jstring jsub = s.env->NewStringUTF(subject.c_str());
    s.env->CallVoidMethod(activity, mid, jpath, jmime, jsub);
    s.env->DeleteLocalRef(jpath);
    s.env->DeleteLocalRef(jmime);
    s.env->DeleteLocalRef(jsub);
}

// Share multiple cache file paths using MainActivity.shareCacheFilePaths(...)
static void Android_ShareCacheFilePaths(const std::vector<std::string>& paths, const std::string& mime, const std::string& subject)
{
    JniEnvScope s; if (!s.ok()) return;
    jobject activity = ME_GetActivity();
    jclass cls = GetActivityClass(s.env);
    jmethodID mid = s.env->GetMethodID(cls, "shareCacheFilePaths", "([Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    if (!mid) return;

    jclass strCls = s.env->FindClass("java/lang/String");
    jobjectArray jarr = s.env->NewObjectArray((jsize)paths.size(), strCls, nullptr);
    for (jsize i = 0; i < (jsize)paths.size(); ++i) {
        jstring jp = s.env->NewStringUTF(paths[i].c_str());
        s.env->SetObjectArrayElement(jarr, i, jp);
        s.env->DeleteLocalRef(jp);
    }

    jstring jmime = s.env->NewStringUTF(mime.c_str());
    jstring jsub = s.env->NewStringUTF(subject.c_str());
    s.env->CallVoidMethod(activity, mid, jarr, jmime, jsub);

    s.env->DeleteLocalRef(jarr);
    s.env->DeleteLocalRef(jmime);
    s.env->DeleteLocalRef(jsub);
}
#endif
