
#pragma once

// If you want to force the mini glm (even if GLM is present), define CUSTOMWIDGETS_FORCE_MINI_GLM.
#if defined(CUSTOMWIDGETS_FORCE_MINI_GLM)
    #define CUSTOMWIDGETS_USE_MINI_GLM 1
#elif defined(__has_include)
    #if __has_include(<glm/vec3.hpp>)
        #define CUSTOMWIDGETS_USE_MINI_GLM 0
    #else
        #define CUSTOMWIDGETS_USE_MINI_GLM 1
    #endif
#else
    #define CUSTOMWIDGETS_USE_MINI_GLM 1
#endif

#if !CUSTOMWIDGETS_USE_MINI_GLM
    #include <glm/vec3.hpp>
    #include <glm/geometric.hpp>
#else
    #include <cmath>
    namespace glm {
        struct vec3 {
            float x, y, z;
            constexpr vec3() : x(0), y(0), z(0) {}
            constexpr vec3(float X, float Y, float Z) : x(X), y(Y), z(Z) {}
        };
        inline vec3 operator+(const vec3& a, const vec3& b) { return vec3(a.x + b.x, a.y + b.y, a.z + b.z); }
        inline vec3 operator-(const vec3& a, const vec3& b) { return vec3(a.x - b.x, a.y - b.y, a.z - b.z); }
        inline vec3 operator*(const vec3& a, float s) { return vec3(a.x * s, a.y * s, a.z * s); }
        inline vec3 operator*(float s, const vec3& a) { return vec3(a.x * s, a.y * s, a.z * s); }
        inline vec3 operator/(const vec3& a, float s) { return vec3(a.x / s, a.y / s, a.z / s); }
        inline float dot(const vec3& a, const vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
        inline vec3 cross(const vec3& a, const vec3& b) { return vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
        inline float length(const vec3& a) { return std::sqrt(dot(a, a)); }
        inline vec3 normalize(const vec3& a) { float l = length(a); return (l > 0.0f) ? (a / l) : vec3(0, 0, 0); }
    }
#endif
