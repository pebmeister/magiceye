// written by Paul Baxter

// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <locale>
#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include <filesystem>

#include <thread>
#include <atomic>
#include <future>

#ifdef __linux__
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <iostream>
#include <shlobj.h>
#endif
// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

#include "customwidgets.h"
#include "Options.h"
#include "openfile.h"
#include "StereogramGenerator.h"
int my_width, my_height;

bool render_image = false;
GLuint my_sirds_texture = 0;
GLuint my_depth_texture = 0;

std::future<bool> render_future;
std::atomic<bool> is_rendering{ false };
std::string rendered_image_path;
std::string rendered_depth_path;

namespace fs = std::filesystem;

static std::string ToNarrow(const wchar_t s, char dfault = '?',
    const std::locale& loc = std::locale())
{
    std::ostringstream stm;

    stm << std::use_facet< std::ctype<wchar_t> >(loc).narrow(s, dfault);
    return stm.str();
}

// Helper function to resolve paths with ".." components
static fs::path resolve_path(const fs::path& input_path)
{
    try {
        // First, convert to absolute path
        fs::path absolute_path = fs::absolute(input_path);

        // Then, canonicalize it (which resolves symlinks and normalizes path)
        // Note: canonical requires the path to exist
        if (fs::exists(absolute_path)) {
            return fs::canonical(absolute_path);
        }

        // If the path doesn't exist, we need to manually resolve ".." components
        fs::path resolved_path;
        for (const auto& part : absolute_path) {
            if (part == "..") {
                if (!resolved_path.empty() && resolved_path != resolved_path.root_path()) {
                    resolved_path = resolved_path.parent_path();
                }
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

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Helper function to load an image into an OpenGL texture
static bool LoadTextureFromFile(const char* filename, GLuint* out_texture, int* out_width, int* out_height)
{
    // Load image data
    int image_width = 0;
    int image_height = 0;
    unsigned char* image_data = stbi_load(filename, &image_width, &image_height, NULL, 4);
    if (image_data == NULL)
        return false;

    // Create OpenGL texture
    glGenTextures(1, out_texture);
    glBindTexture(GL_TEXTURE_2D, *out_texture);

    // Setup filtering parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    // Upload pixels to GPU
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    stbi_image_free(image_data);

    *out_width = image_width;
    *out_height = image_height;

    return true;
}

// Main code
int main(int, char**)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100 (WebGL 1.0)
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(IMGUI_IMPL_OPENGL_ES3)
    // GL ES 3.0 + GLSL 300 es (WebGL 2.0)
    const char* glsl_version = "#version 300 es";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif
    
    // Create window with graphics context
    float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor()); // Valid on GLFW 3.3+ only
    GLFWwindow* window = glfwCreateWindow((int)(1280 * main_scale), (int)(800 * main_scale), "Magic Eye", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();
    // ImGui::StyleColorsClassic();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale;        // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details. If you like the default font but want it to scale better, consider using the 'ProggyVector' from the same author!
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
    //style.FontSizeBase = 20.0f;
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
    //IM_ASSERT(font != nullptr);


    // Our state
    bool show_stl_openfile = false;
    bool show_texture_openfile = false;
    ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.80f, 1.00f);

    std::string sep = ToNarrow(std::filesystem::path::preferred_separator);
    std::string root;
    
    // Find the Documents folder on various platforms
#ifdef _WIN32
#pragma comment(lib, "shell32.lib")
    {
        CHAR my_documents[MAX_PATH];
        HRESULT result = SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, my_documents);
        root = std::string(my_documents) + sep;
    }
#endif
#ifdef __linux__
    {
        struct passwd* pw = getpwuid(getuid());
        const char* homedir = pw->pw_dir;
        root = std::string(homedir) + sep + "Documents" + sep;
    }
#endif
#ifdef __APPLE__
    root = std::string("~") + sep + "Documents" + "sep";
#endif


    auto stlpath = resolve_path(std::filesystem::absolute(root + "stl"));
    auto texturepath = resolve_path(std::filesystem::absolute(root + "texture"));
    openfile stl_openfile_dialog("open STL/OBJ file", stlpath.string(), {".stl", ".obj"});
    openfile texture_openfile_dialog("open Texture file", texturepath.string(), { ".png", ".jpg" });

    auto image_rendured = false;
    auto stereogram_options = std::make_shared<Options>();
    
    // Main loop
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!glfwWindowShouldClose(window))
#endif
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x + 50, main_viewport->WorkPos.y + 50), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(1000, 600), ImGuiCond_FirstUseEver);

        ImGui::Begin("Magic Eye");

        // STL/OBJ
        ImGui::BeginDisabled(is_rendering);

        if (ImGui::Button("Select STL/OBJ")) {
            show_stl_openfile = true;
        }
        ImGui::SameLine();
        auto stldisabled = stereogram_options->stlpath.empty();

        ImGui::BeginDisabled(stldisabled);
        ImGui::LabelText("", "%s", stereogram_options->stlpath.c_str());
        ImGui::EndDisabled();

        // TEXTURE
        if (ImGui::Button("Select TEXTURE")) {
            show_texture_openfile = true;
        }
        ImGui::SameLine();
        auto texturedisabled = stereogram_options->stlpath.empty();
        ImGui::BeginDisabled(texturedisabled);
        ImGui::LabelText("", "%s", stereogram_options->texpath.c_str());
        ImGui::EndDisabled();

        auto paramsdiabled = stereogram_options->stlpath.empty() || stereogram_options->texpath.empty() || is_rendering;
        ImGui::BeginDisabled(paramsdiabled);

        // Check if rendering is complete (call this in your update loop)
        if (is_rendering) {
            // Check if the future is ready (non-blocking)
            if (render_future.valid() &&
                render_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {

                bool success = render_future.get();
                if (success) {
                    // Load the texture on the main thread
                    bool ret = LoadTextureFromFile(
                        rendered_image_path.c_str(),
                        &my_sirds_texture,
                        &my_width,
                        &my_height
                    );
                    if (ret) {
                        bool ret2 = LoadTextureFromFile(
                            rendered_depth_path.c_str(),
                            &my_depth_texture,
                            &my_width,
                            &my_height
                        );

                        render_image = true;
                    }
                }
                is_rendering = false;
            }
        }

        // Display the image when ready
        if (render_image) {
            std::filesystem::path depth(stereogram_options->outprefix + "_depth.png");
            auto path = std::filesystem::absolute(depth);
            if (ImGui::Begin(path.string().c_str(), &render_image)) {
                ImGui::Image((void*)(intptr_t)my_depth_texture, ImVec2(my_width, my_height));
            }
            ImGui::End();

            std::filesystem::path sird(stereogram_options->outprefix + "_sirds.png");
            path = std::filesystem::absolute(sird);
            if (ImGui::Begin(path.string().c_str(), &render_image)) {
                ImGui::Image((void*)(intptr_t)my_sirds_texture, ImVec2(my_width, my_height));
            }
            ImGui::End();
        }

        ImGui::Separator();

        //////////////////////////////////////////////////
        // Width - Height
        //////////////////////////////////////////////////
        ImGui::SetNextItemWidth(264);
        if (CustomWidgets::InputInt2("Dimentions", &stereogram_options->width)) {}

        //////////////////////////////////////////////////
        // Eye Seperation
        //////////////////////////////////////////////////
        ImGui::SetNextItemWidth(130);
        if (CustomWidgets::SliderInt("Eye separation", &stereogram_options->eye_sep, 0, 250)) {}

        //////////////////////////////////////////////////
        // Camera Pos
        //////////////////////////////////////////////////
        ImGui::BeginDisabled(!stereogram_options->custom_cam_provided);
        ImGui::SetNextItemWidth(400);
        if (CustomWidgets::InputFloat3("Camera pos", &stereogram_options->custom_cam_pos[0])) {}
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Checkbox("Use Custom camera pos", &stereogram_options->custom_cam_provided)) {}

        //////////////////////////////////////////////////
        // Look At
        //////////////////////////////////////////////////
        ImGui::BeginDisabled(!stereogram_options->custom_lookat_provided);
        ImGui::SetNextItemWidth(400);
        if (CustomWidgets::InputFloat3("Look at", &stereogram_options->custom_look_at[0])) {}
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Checkbox("Use Custom look at", &stereogram_options->custom_lookat_provided)) {}

        //////////////////////////////////////////////////
        // Orthtagonal scale
        //////////////////////////////////////////////////
        ImGui::BeginDisabled(!stereogram_options->custom_orth_scale_provided);
        ImGui::SetNextItemWidth(130);
        if (CustomWidgets::InputFloat("ortho scale", &stereogram_options->custom_orth_scale)) {}
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(261);
        ImGui::LabelText("", "%s", "");
        ImGui::SameLine();
        if (ImGui::Checkbox("Use Custom orthogonal scale", &stereogram_options->custom_orth_scale_provided)) {}
        //////////////////////////////////////////////////
        // Laplace smoothing
        //////////////////////////////////////////////////
        ImGui::BeginDisabled(!stereogram_options->laplace_smoothing);
        ImGui::SetNextItemWidth(130);
        if (CustomWidgets::InputInt("Laplace layers", &stereogram_options->laplace_smooth_layers)) {}
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(261);
        ImGui::LabelText("", "%s", "");
        ImGui::SameLine();
        if (ImGui::Checkbox("Use Laplace smoothing", &stereogram_options->laplace_smoothing)) {}

        //////////////////////////////////////////////////
        // Field of view
        //////////////////////////////////////////////////
        ImGui::SetNextItemWidth(130);
        if (CustomWidgets::InputFloat("Field of View", &stereogram_options->fov)) {}

        //////////////////////////////////////////////////
        // Rotation
        //////////////////////////////////////////////////
        ImGui::SetNextItemWidth(400);
        if (CustomWidgets::InputFloat3("Rotation", &stereogram_options->rot_deg[0])) {}

        //////////////////////////////////////////////////
        // Translation
        //////////////////////////////////////////////////
        ImGui::SetNextItemWidth(400);
        if (CustomWidgets::InputFloat3("Translation", &stereogram_options->trans[0])) {}

        //////////////////////////////////////////////////
        // Scale
        //////////////////////////////////////////////////
        ImGui::SetNextItemWidth(400);
        if (CustomWidgets::InputFloat3("Scale", &stereogram_options->sc[0])) {}

        //////////////////////////////////////////////////
        // Shear
        //////////////////////////////////////////////////
        ImGui::SetNextItemWidth(400);
        if (CustomWidgets::InputFloat3("Shear", &stereogram_options->shear[0])) {}

        //////////////////////////////////////////////////
        // Depth near far
        //////////////////////////////////////////////////
        ImGui::SetNextItemWidth(263);
        if (CustomWidgets::InputFloat2("Depth", &stereogram_options->depth_near)) {}

        //////////////////////////////////////////////////
        // Brightness
        //////////////////////////////////////////////////
        ImGui::SetNextItemWidth(130);
        if (CustomWidgets::InputFloat("Brightness", &stereogram_options->texture_brightness)) {}

        //////////////////////////////////////////////////
        // Contrast
        //////////////////////////////////////////////////
        ImGui::SetNextItemWidth(130);
        if (CustomWidgets::InputFloat("Contrast", &stereogram_options->texture_contrast)) {}

        //////////////////////////////////////////////////
        // Eye Seperation
        //////////////////////////////////////////////////
        ImGui::SetNextItemWidth(130);
        if (CustomWidgets::InputFloat("Separation", &stereogram_options->bg_separation)) {}

        //////////////////////////////////////////////////
        // Depth Gama
        //////////////////////////////////////////////////
        ImGui::SetNextItemWidth(130);
        if (CustomWidgets::InputFloat("Depth gama", &stereogram_options->depth_gamma)) {}

        //////////////////////////////////////////////////
        // Orthtagonal Tuning hi lo
        //////////////////////////////////////////////////
        ImGui::SetNextItemWidth(263);
        if (CustomWidgets::InputFloat2("Orth tune", &stereogram_options->orthTuneLow)) {}

        //////////////////////////////////////////////////
        // Foreground Threshhold
        //////////////////////////////////////////////////
        ImGui::SetNextItemWidth(130);
        if (CustomWidgets::InputFloat("Foreground", &stereogram_options->foreground_threshold)) {}

        //////////////////////////////////////////////////
        // Smooth Threshhold
        //////////////////////////////////////////////////
        static float smooththresh = stereogram_options->smoothThreshold;
        ImGui::SetNextItemWidth(130);
        if (CustomWidgets::InputFloat("Smooth thresh", &smooththresh))
            stereogram_options->smoothThreshold = smooththresh;

        //////////////////////////////////////////////////
        // Smooth Weight
        //////////////////////////////////////////////////
        ImGui::SetNextItemWidth(130);
        if (CustomWidgets::InputFloat("Smooth weight", &stereogram_options->smoothWeight)) {}

        //////////////////////////////////////////////////
        // Ramp width height
        //////////////////////////////////////////////////
        ImGui::SetNextItemWidth(263);
        if (CustomWidgets::InputFloat2("Ramp", &stereogram_options->rampWidth)) {}

        //////////////////////////////////////////////////
        // Perspective
        //////////////////////////////////////////////////
        ImGui::SetNextItemWidth(130);
        if (ImGui::Checkbox("Perspective", &stereogram_options->perspective)) {}

        //////////////////////////////////////////////////
        // Reset
        //////////////////////////////////////////////////
        if (ImGui::Button("Reset Options")) {
            auto& stl = stereogram_options->stlpath;
            auto& texture = stereogram_options->texpath;
            auto options = std::make_shared<Options>();
            options->stlpath = stl;
            options->texpath = texture;
            stereogram_options = options;
        }
        //////////////////////////////////////////////////
        // Render
        //////////////////////////////////////////////////
        // 
        if (ImGui::Button("Render")) {
            is_rendering = true;
            render_image = false;

            auto path = std::filesystem::path(stereogram_options->stlpath);
            auto outp = std::filesystem::absolute(path.replace_extension(""));

            stereogram_options->outprefix =  outp.string();
            // Launch the rendering in a separate thread
            render_future = std::async(std::launch::async, [&]()
                {
                    StereogramGenerator st(stereogram_options);
                    auto error = st.create();

                    if (!error) {
                        rendered_image_path = stereogram_options->outprefix + "_sirds.png";
                        rendered_depth_path = stereogram_options->outprefix + "_depth.png";
                        return true;
                    }
                    return false;
                });
        }


        ImGui::EndDisabled();
        ImGui::EndDisabled();
        if (is_rendering) {
            ImGui::SameLine(); CustomWidgets::LoadingSpinner("##spinner", 12.0f, 4);
        }
        ImGui::End();

        if (show_stl_openfile) {
            const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x + 50, main_viewport->WorkPos.y + 20), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(550, 680), ImGuiCond_FirstUseEver);

            ImGui::SetNextWindowFocus();
            auto result = stl_openfile_dialog.show(show_stl_openfile);
            if (result == openfile::FileSelected) {
                stereogram_options->stlpath = stl_openfile_dialog.selecteditem.string();
            }
        }

        if (show_texture_openfile) {
            const ImGuiViewport* main_viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x + 50, main_viewport->WorkPos.y + 20), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(550, 680), ImGuiCond_FirstUseEver);

            ImGui::SetNextWindowFocus();
            auto result = texture_openfile_dialog.show(show_texture_openfile);
            if (result == openfile::FileSelected) {
                stereogram_options->texpath = texture_openfile_dialog.selecteditem.string();
            }
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
