// written by Paul Baxter

// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "imgui_internal.h"
#include "customwidgets.h"

void CustomWidgets::LoadingSpinner(const char* label, float radius, int thickness)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size(radius * 2.0f, (radius + style.FramePadding.y) * 2.0f);

    ImU32 color = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
    float time = ImGui::GetTime();
    float angle = time * 360.0f; // Rotate at 360 degrees per second

    window->DrawList->PathClear();
    int num_segments = 15;
    for (int i = 0; i < num_segments; i++) {
        float segment_angle = angle + (i * 360.0f / num_segments);
        window->DrawList->PathLineTo(ImVec2(pos.x + radius + cosf(segment_angle * IM_PI / 180.0f) * radius,
            pos.y + radius + sinf(segment_angle * IM_PI / 180.0f) * radius));
    }
    window->DrawList->PathStroke(color, false, thickness);

    ImGui::Dummy(size);
}

bool CustomWidgets::SliderInt(const char* label, int* v, int v_min, int v_max, const char* format, ImGuiSliderFlags flags)
{
    return SliderScalarN(label, ImGuiDataType_S32, v, 1, &v_min, &v_max, format, flags);
}

bool CustomWidgets::SliderInt2(const char* label, int v[2], int v_min, int v_max, const char* format, ImGuiSliderFlags flags)
{
    return SliderScalarN(label, ImGuiDataType_S32, v, 2, &v_min, &v_max, format, flags);
}

bool CustomWidgets::SliderInt3(const char* label, int v[3], int v_min, int v_max, const char* format, ImGuiSliderFlags flags)
{
    return SliderScalarN(label, ImGuiDataType_S32, v, 3, &v_min, &v_max, format, flags);
}

bool CustomWidgets::SliderInt4(const char* label, int v[4], int v_min, int v_max, const char* format, ImGuiSliderFlags flags)
{
    return SliderScalarN(label, ImGuiDataType_S32, v, 4, &v_min, &v_max, format, flags);
}

bool CustomWidgets::SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
    return SliderScalarN(label, ImGuiDataType_Float, v, 1, &v_min, &v_max, format, flags);
}

bool CustomWidgets::SliderFloat2(const char* label, float v[2], float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
    return SliderScalarN(label, ImGuiDataType_Float, v, 2, &v_min, &v_max, format, flags);
}

bool CustomWidgets::SliderFloat3(const char* label, float v[3], float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
    return SliderScalarN(label, ImGuiDataType_Float, v, 3, &v_min, &v_max, format, flags);
}

bool CustomWidgets::SliderFloat4(const char* label, float v[4], float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
    return SliderScalarN(label, ImGuiDataType_Float, v, 4, &v_min, &v_max, format, flags);
}

bool CustomWidgets::InputInt(const char* label, int* v, int step, int step_fast, ImGuiInputTextFlags flags)
{
    return InputScalarN(label, ImGuiDataType_S32, v, 1, NULL, NULL, "%d", flags);
}

bool CustomWidgets::InputInt2(const char* label, int v[2], ImGuiInputTextFlags flags)
{
    return InputScalarN(label, ImGuiDataType_S32, v, 2, NULL, NULL, "%d", flags);
}

bool CustomWidgets::InputInt3(const char* label, int v[3], ImGuiInputTextFlags flags)
{
    return InputScalarN(label, ImGuiDataType_S32, v, 3, NULL, NULL, "%d", flags);
}

bool CustomWidgets::InputInt4(const char* label, int v[4], ImGuiInputTextFlags flags)
{
    return InputScalarN(label, ImGuiDataType_S32, v, 4, NULL, NULL, "%d", flags);
}

bool CustomWidgets::InputFloat(const char* label, float* v, float step, float step_fast, const char* format, ImGuiInputTextFlags flags)
{
    return InputScalarN(label, ImGuiDataType_Float, v, 1, NULL, NULL, "%.3f", flags);
}

bool CustomWidgets::InputFloat2(const char* label, float v[2], const char* format, ImGuiInputTextFlags flags)
{
    return InputScalarN(label, ImGuiDataType_Float, v, 2, NULL, NULL, format, flags);
}

bool CustomWidgets::InputFloat3(const char* label, float v[3], const char* format, ImGuiInputTextFlags flags)
{
    return InputScalarN(label, ImGuiDataType_Float, v, 3, NULL, NULL, format, flags);
}

bool CustomWidgets::InputFloat4(const char* label, float v[4], const char* format, ImGuiInputTextFlags flags)
{
    return InputScalarN(label, ImGuiDataType_Float, v, 4, NULL, NULL, format, flags);
}

// Add multiple sliders on 1 line for compact edition of multiple components
bool CustomWidgets::SliderScalarN(const char* label, ImGuiDataType data_type, void* v, int components, const void* v_min, const void* v_max, const char* format, ImGuiSliderFlags flags)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    bool value_changed = false;
    ImGui::BeginGroup();
    ImGui::PushID(label);

    ImGui::PushMultiItemsWidths(components, ImGui::CalcItemWidth());

    ImGui::LabelText("", label);
    ImGui::SameLine();
    
    auto type_size = ImGui::DataTypeGetInfo(data_type);
    for (int i = 0; i < components; i++) {
        ImGui::PushID(i);
        if (i > 0)
            ImGui::SameLine(0, g.Style.ItemInnerSpacing.x);
        value_changed |= ImGui::SliderScalar("", data_type, v, v_min, v_max, format, flags);
        ImGui::PopID();
        ImGui::PopItemWidth();
        v = (void*)((char*)v + type_size->Size);
    }
    ImGui::PopID();

    ImGui::EndGroup();
    return value_changed;
}

bool CustomWidgets::InputScalarN(const char* label, ImGuiDataType data_type, void* p_data, int components, const void* p_step, const void* p_step_fast, const char* format, ImGuiInputTextFlags flags)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    bool value_changed = false;
    ImGui::BeginGroup();
    ImGui::PushID(label);

    ImGui::PushMultiItemsWidths(components, ImGui::CalcItemWidth());

    ImGui::LabelText("", label);
    ImGui::SameLine();

    auto type_size = ImGui::DataTypeGetInfo(data_type);
    for (int i = 0; i < components; i++) {
        ImGui::PushID(i);
        if (i > 0)
            ImGui::SameLine(0, g.Style.ItemInnerSpacing.x);
        value_changed |= ImGui::InputScalar("", data_type, p_data, p_step, p_step_fast, format, flags);
        ImGui::PopID();
        ImGui::PopItemWidth();
        p_data = (void*)((char*)p_data + type_size->Size);
    }
    ImGui::PopID();
    ImGui::EndGroup();
    return value_changed;
}
