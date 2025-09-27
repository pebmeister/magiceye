#define IMGUI_DEFINE_MATH_OPERATORS

// written by Paul Baxter

// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp
#include "imgui.h"
#include <cmath>
#include <cstdio>

#include "imgui_internal.h"
#include "customwidgets.h"

namespace CustomWidgets
{
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
        int num_segments = 15;
        for (int i = 0; i < num_segments; i++) {
            float segment_angle = angle + (i * 360.0f / num_segments);
            window->DrawList->PathLineTo(ImVec2(pos.x + radius + cosf(segment_angle * IM_PI / 180.0f) * radius,
                pos.y + radius + sinf(segment_angle * IM_PI / 180.0f) * radius));
        }
        window->DrawList->PathStroke(color, false, thickness);

        ImGui::Dummy(size);
    }

    bool SliderInt(const char* label, int* v, int v_min, int v_max, const char* format, ImGuiSliderFlags flags)
    {
        return SliderScalarN(label, ImGuiDataType_S32, v, 1, &v_min, &v_max, format, flags);
    }

    bool SliderInt2(const char* label, int v[2], int v_min, int v_max, const char* format, ImGuiSliderFlags flags)
    {
        return SliderScalarN(label, ImGuiDataType_S32, v, 2, &v_min, &v_max, format, flags);
    }

    bool SliderInt3(const char* label, int v[3], int v_min, int v_max, const char* format, ImGuiSliderFlags flags)
    {
        return SliderScalarN(label, ImGuiDataType_S32, v, 3, &v_min, &v_max, format, flags);
    }

    bool SliderInt4(const char* label, int v[4], int v_min, int v_max, const char* format, ImGuiSliderFlags flags)
    {
        return SliderScalarN(label, ImGuiDataType_S32, v, 4, &v_min, &v_max, format, flags);
    }

    bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
    {
        return SliderScalarN(label, ImGuiDataType_Float, v, 1, &v_min, &v_max, format, flags);
    }

    bool SliderFloat2(const char* label, float v[2], float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
    {
        return SliderScalarN(label, ImGuiDataType_Float, v, 2, &v_min, &v_max, format, flags);
    }

    bool SliderFloat3(const char* label, float v[3], float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
    {
        return SliderScalarN(label, ImGuiDataType_Float, v, 3, &v_min, &v_max, format, flags);
    }

    bool SliderFloat4(const char* label, float v[4], float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
    {
        return SliderScalarN(label, ImGuiDataType_Float, v, 4, &v_min, &v_max, format, flags);
    }

    bool InputInt(const char* label, int* v, int step, int step_fast, ImGuiInputTextFlags flags)
    {
        return InputScalarN(label, ImGuiDataType_S32, v, 1, NULL, NULL, "%d", flags);
    }

    bool InputInt2(const char* label, int v[2], ImGuiInputTextFlags flags)
    {
        return InputScalarN(label, ImGuiDataType_S32, v, 2, NULL, NULL, "%d", flags);
    }

    bool InputInt3(const char* label, int v[3], ImGuiInputTextFlags flags)
    {
        return InputScalarN(label, ImGuiDataType_S32, v, 3, NULL, NULL, "%d", flags);
    }

    bool InputInt4(const char* label, int v[4], ImGuiInputTextFlags flags)
    {
        return InputScalarN(label, ImGuiDataType_S32, v, 4, NULL, NULL, "%d", flags);
    }

    bool InputFloat(const char* label, float* v, float step, float step_fast, const char* format, ImGuiInputTextFlags flags)
    {
        return InputScalarN(label, ImGuiDataType_Float, v, 1, NULL, NULL, "%.3f", flags);
    }

    bool InputFloat2(const char* label, float v[2], const char* format, ImGuiInputTextFlags flags)
    {
        return InputScalarN(label, ImGuiDataType_Float, v, 2, NULL, NULL, format, flags);
    }

    bool InputFloat3(const char* label, float v[3], const char* format, ImGuiInputTextFlags flags)
    {
        return InputScalarN(label, ImGuiDataType_Float, v, 3, NULL, NULL, format, flags);
    }

    bool InputFloat4(const char* label, float v[4], const char* format, ImGuiInputTextFlags flags)
    {
        return InputScalarN(label, ImGuiDataType_Float, v, 4, NULL, NULL, format, flags);
    }

    // Add multiple sliders on 1 line for compact edition of multiple components
    bool SliderScalarN(const char* label, ImGuiDataType data_type, void* v, int components, const void* v_min, const void* v_max, const char* format, ImGuiSliderFlags flags)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        bool value_changed = false;
        ImGui::BeginGroup();
        ImGui::PushID(label);

        ImGui::PushMultiItemsWidths(components, ImGui::CalcItemWidth());

        ImGui::LabelText("", "%s", label);
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

    bool InputScalarN(const char* label, ImGuiDataType data_type, void* p_data, int components, const void* p_step, const void* p_step_fast, const char* format, ImGuiInputTextFlags flags)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        bool value_changed = false;
        ImGui::BeginGroup();
        ImGui::PushID(label);

        ImGui::PushMultiItemsWidths(components, ImGui::CalcItemWidth());

        ImGui::LabelText("", "%s", label);
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

    static float clampf(float v, float mn, float mx) { return v < mn ? mn : (v > mx ? mx : v); }

    bool KnobWithIndicator(const char* label,
        float* v,
        float v_min,
        float v_max,
        float size,
        int bar_segments,
        float bar_height,
        float bar_gap)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems) return false;

        ImGuiContext& g = *ImGui::GetCurrentContext();
        const ImGuiStyle& style = g.Style;
        ImDrawList* dl = window->DrawList;

        // Clamp and normalize
        *v = clampf(*v, v_min, v_max);
        const float range = (v_max - v_min);
        float t = (range > 0.0f) ? ((*v - v_min) / range) : 0.0f;

        // Layout
        const ImVec2 pos = window->DC.CursorPos;

        char val_buf[32];
        std::snprintf(val_buf, sizeof(val_buf), "%.3f", *v);
        const ImVec2 val_text_sz = ImGui::CalcTextSize(val_buf, nullptr, true);
        const ImVec2 label_sz = ImGui::CalcTextSize(label, nullptr, true);

        const float bar_w = size; // indicator bar width equals knob width

        float total_w = size;
        if (bar_segments > 0 && bar_height > 0.0f)
            total_w = ImMax(total_w, bar_w + style.ItemSpacing.x + val_text_sz.x);
        total_w = ImMax(total_w, label_sz.x);

        float total_h = 0.0f;
        if (bar_segments > 0 && bar_height > 0.0f)
            total_h += bar_height + style.ItemSpacing.y;
        total_h += size;
        if (label && label[0] != 0)
            total_h += style.ItemInnerSpacing.y + label_sz.y;

        const ImRect total_bb(pos, pos + ImVec2(total_w, total_h));
        ImGui::ItemSize(total_bb.GetSize());

        // Knob geometry
        const float radius = size * 0.5f;
        const ImVec2 knob_center = ImVec2(pos.x + radius,
            pos.y + ((bar_segments > 0 && bar_height > 0.0f) ? (bar_height + style.ItemSpacing.y) : 0.0f) + radius);
        const ImRect knob_bb(knob_center - ImVec2(radius, radius), knob_center + ImVec2(radius, radius));

        // Bars geometry
        const ImVec2 bar_pos = pos;
        const ImRect bar_bb(bar_pos, bar_pos + ImVec2(bar_w, bar_height));

        // Label geometry
        const ImVec2 label_pos(knob_center.x - label_sz.x * 0.5f,
            knob_bb.Max.y + ((label && label[0] != 0) ? style.ItemInnerSpacing.y : 0.0f));
        const ImRect label_bb(label_pos, label_pos + label_sz);

        // Register the knob as the interactive item (we consume the whole area for layout but only the knob is draggable)
        ImGuiID id = window->GetID(label);
        if (!ImGui::ItemAdd(knob_bb, id)) {
            // Still draw visuals even if not interactive (clipped), but return false for changed
            // Fallthrough to draw
        }

        // Handle input on knob only
        bool hovered_knob = false, held_knob = false;
        ImGui::ButtonBehavior(knob_bb, id, &hovered_knob, &held_knob, ImGuiButtonFlags_MouseButtonLeft);

        bool value_changed = false;

        // Drag interaction: radial mapping (locks at ends; no wrapping)
        if (held_knob && range > 0.0f) {
            const ImVec2 mp = g.IO.MousePos;

            // Arc from 135° to 405° (270° total sweep)
            const float a_min = 3.0f * IM_PI * 0.25f;   // 135°
            const float a_max = 9.0f * IM_PI * 0.25f;   // 405°

            float ang = std::atan2(mp.y - knob_center.y, mp.x - knob_center.x);

            // Map into [a_min, a_min + 2π) and then hard clamp into [a_min, a_max]
            if (ang < a_min) ang += 2.0f * IM_PI;
            ang = clampf(ang, a_min, a_max);

            float new_t = (ang - a_min) / (a_max - a_min);
            float new_v = v_min + new_t * range;
            new_v = clampf(new_v, v_min, v_max); // explicit lock at ends

            if (new_v != *v) {
                *v = new_v;
                value_changed = true;
            }
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        }

        // Mouse wheel to adjust when hovering the knob
        if (hovered_knob && g.IO.MouseWheel != 0.0f && range > 0.0f) {
            float base_step = range * 0.01f; // 1% per notch
            float step = base_step;
            if (g.IO.KeyShift) step *= 0.25f; // fine
            if (g.IO.KeyAlt)   step *= 4.0f;  // coarse

            float new_v = clampf(*v + g.IO.MouseWheel * step, v_min, v_max);
            if (new_v != *v) {
                *v = new_v;
                value_changed = true;
            }
        }

        // Recompute normalized t for drawing
        t = (range > 0.0f) ? ((*v - v_min) / range) : 0.0f;

        // Colors
        const bool active = held_knob;
        const bool hovered = hovered_knob;

        ImU32 col_knob_bg = ImGui::GetColorU32(style.Colors[active ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg]);
        ImU32 col_knob_border = ImGui::GetColorU32(style.Colors[ImGuiCol_Border]);
        ImU32 col_tick = ImGui::GetColorU32(style.Colors[ImGuiCol_SliderGrabActive]);
        ImU32 col_bar_bg = ImGui::GetColorU32(style.Colors[ImGuiCol_FrameBg]);
        ImU32 col_bar_on = ImGui::GetColorU32(style.Colors[ImGuiCol_PlotHistogram]);
        ImU32 col_text = ImGui::GetColorU32(style.Colors[ImGuiCol_Text]);

        // Draw indicator bar (segments) above knob, and numeric value at its right
        if (bar_segments > 0 && bar_height > 0.0f) {
            const int segs = ImMax(1, bar_segments);
            const float seg_count = (float)segs;
            const float total_gap = bar_gap * (segs - 1);
            const float seg_w = (bar_w - total_gap) / seg_count;
            const int on_count = (int)floorf(t * seg_count + 0.0001f);
            const float denom = ImMax(1.0f, seg_count - 1.0f);

            for (int i = 0; i < segs; ++i) {
                float x0 = bar_pos.x + i * (seg_w + bar_gap);
                float x1 = x0 + seg_w;
                ImVec2 p0(x0, bar_pos.y);
                ImVec2 p1(x1, bar_pos.y + bar_height);
                const bool on = (i < on_count);
                ImU32 col = on ? col_bar_on : col_bar_bg;

                if (on) {
                    // Slight hue shift towards yellow as i increases
                    col = ImColor::HSV(0.35f - 0.35f * (i / denom), 0.85f, hovered ? 0.95f : 0.85f);
                }

                dl->AddRectFilled(p0, p1, col, 2.0f);
            }

            // Value text next to the bar
            ImVec2 text_pos = ImVec2(bar_pos.x + bar_w + style.ItemSpacing.x,
                bar_pos.y - (val_text_sz.y - bar_height) * 0.5f);
            dl->AddText(text_pos, col_text, val_buf);
        }

        // Knob sweep angles (for drawing)
        const float a_min = 3.0f * IM_PI * 0.25f;   // 135°
        const float a_max = 9.0f * IM_PI * 0.25f;   // 405°
        const float a = a_min + t * (a_max - a_min);

        // Draw knob body
        dl->AddCircleFilled(knob_center, radius, col_knob_bg, 32);
        dl->AddCircle(knob_center, radius, col_knob_border, 32, 1.5f);

        // Inner ring to add depth
        const float inner_r = radius * 0.7f;
        const ImU32 col_inner = ImGui::GetColorU32(style.Colors[ImGuiCol_FrameBg]);
        dl->AddCircleFilled(knob_center, inner_r, col_inner, 32);

        // Pointer/tick
        const float tick_r0 = inner_r * 0.2f;
        const float tick_r1 = radius * 0.9f;
        const ImVec2 p0(knob_center.x + std::cos(a) * tick_r0, knob_center.y + std::sin(a) * tick_r0);
        const ImVec2 p1(knob_center.x + std::cos(a) * tick_r1, knob_center.y + std::sin(a) * tick_r1);
        dl->AddLine(p0, p1, col_tick, 3.0f);

        // Minor ticks around the arc
        const int minor_ticks = 8;
        for (int i = 0; i <= minor_ticks; ++i) {
            const float aa = a_min + (a_max - a_min) * (i / (float)minor_ticks);
            const ImVec2 q0(knob_center.x + std::cos(aa) * (radius * 0.88f), knob_center.y + std::sin(aa) * (radius * 0.88f));
            const ImVec2 q1(knob_center.x + std::cos(aa) * (radius * 0.78f), knob_center.y + std::sin(aa) * (radius * 0.78f));
            dl->AddLine(q0, q1, ImGui::GetColorU32(style.Colors[ImGuiCol_Border]), 1.0f);
        }

        // Label below
        if (label && label[0] != 0)
            dl->AddText(label_pos, col_text, label);

        return value_changed;
    }
}