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



#include "imgui_internal.h"

void LoadingSpinner(const char* label, float radius, int thickness)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size(radius * 2.0f, (radius + style.FramePadding.y) * 2.0f);

    ImU32 color = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
    float time = ImGui::GetTime();
    float angle = time * 360.0f; // Rotate at 360 degrees per second

    window->DrawList->PathClear();
    int num_segments = 30;
    for (int i = 0; i < num_segments; i++) {
        float segment_angle = angle + (i * 360.0f / num_segments);
        window->DrawList->PathLineTo(ImVec2(pos.x + radius + cosf(segment_angle * IM_PI / 180.0f) * radius,
            pos.y + radius + sinf(segment_angle * IM_PI / 180.0f) * radius));
    }
    window->DrawList->PathStroke(color, false, thickness);

    ImGui::Dummy(size);
}
