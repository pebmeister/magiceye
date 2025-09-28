
#pragma once
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>

namespace CustomWidgets
{

    bool KnobWithIndicator(const char* label,
        float* v,
        float v_min,
        float v_max,
        float size = 64.0f,
        int bar_segments = 16,
        float bar_height = 10.0f,
        float bar_gap = 2.0f);

    void LoadingSpinner(const char* label, float radius, int thickness);
    bool OrbitCamControl(const char* label, glm::vec3* cam_pos, glm::vec3* look_at);


    bool SliderInt(const char* label, int* v, int v_min, int v_max, const char* format = "%d", ImGuiSliderFlags flags = 0);     // adjust format to decorate the value with a prefix or a suffix for in-slider labels or unit display.
    bool SliderInt2(const char* label, int v[2], int v_min, int v_max, const char* format = "%d", ImGuiSliderFlags flags = 0);
    bool SliderInt3(const char* label, int v[3], int v_min, int v_max, const char* format = "%d", ImGuiSliderFlags flags = 0);
    bool SliderInt4(const char* label, int v[4], int v_min, int v_max, const char* format = "%d", ImGuiSliderFlags flags = 0);

    bool SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0);     // adjust format to decorate the value with a prefix or a suffix for in-slider labels or unit display.
    bool SliderFloat2(const char* label, float v[2], float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
    bool SliderFloat3(const char* label, float v[3], float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
    bool SliderFloat4(const char* label, float v[4], float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0);

    bool SliderScalarN(const char* label, ImGuiDataType data_type, void* v, int components, const void* v_min, const void* v_max, const char* format, ImGuiSliderFlags flags);
    
    bool InputInt(const char* label, int* v, int step = 1, int step_fast = 100, ImGuiInputTextFlags flags = 0);
    bool InputInt2(const char* label, int v[2], ImGuiInputTextFlags flags = 0);
    bool InputInt3(const char* label, int v[3], ImGuiInputTextFlags flags = 0);
    bool InputInt4(const char* label, int v[4], ImGuiInputTextFlags flags = 0);
    
    bool InputFloat(const char* label, float* v, float step = 0.0f, float step_fast = 0.0f, const char* format = "%.3f", ImGuiInputTextFlags flags = 0);
    bool InputFloat2(const char* label, float v[2], const char* format = "%.3f", ImGuiInputTextFlags flags = 0);
    bool InputFloat3(const char* label, float v[3], const char* format = "%.3f", ImGuiInputTextFlags flags = 0);
    bool InputFloat4(const char* label, float v[4], const char* format = "%.3f", ImGuiInputTextFlags flags = 0);

    bool InputScalarN(const char* label, ImGuiDataType data_type, void* p_data, int components, const void* p_step = NULL, const void* p_step_fast = NULL, const char* format = NULL, ImGuiInputTextFlags flags = 0);

};