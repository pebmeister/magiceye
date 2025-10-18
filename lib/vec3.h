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
    #define GLM_ENABLE_EXPERIMENTAL
    #include <glm/vec3.hpp>
    #include <glm/vec4.hpp>
    #include <glm/mat4x4.hpp>
    #include <glm/gtc/matrix_transform.hpp>
    #include <glm/gtx/quaternion.hpp>
    #include <glm/common.hpp> // for glm::radians
#else
   #include <cmath>

    namespace glm {

        // radians
        inline float radians(float deg) { return deg * 0.017453292519943295769f; }

        // vec3
        class vec3 {
        public:
            float x, y, z;
            constexpr vec3() : x(0), y(0), z(0) {}
            constexpr vec3(float s) : x(s), y(s), z(s) {};
            constexpr vec3(float X, float Y, float Z) : x(X), y(Y), z(Z) {}
            float& operator[](int i) { return (&x)[i]; }
            const float& operator[](int i) const { return (&x)[i]; }

        };

        inline vec3 operator+(const vec3& a, const vec3& b) { return vec3(a.x + b.x, a.y + b.y, a.z + b.z); }
        inline vec3 operator-(const vec3& a, const vec3& b) { return vec3(a.x - b.x, a.y - b.y, a.z - b.z); }
        inline vec3 operator-(const vec3& a) { return vec3(-a.x, -a.y, -a.z); }
        inline vec3 operator*(const vec3& a, float s) { return vec3(a.x * s, a.y * s, a.z * s); }
        inline vec3 operator*(float s, const vec3& a) { return vec3(a.x * s, a.y * s, a.z * s); }
        inline vec3 operator/(const vec3& a, float s) { return vec3(a.x / s, a.y / s, a.z / s); }
        inline float dot(const vec3& a, const vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
        inline vec3 cross(const vec3& a, const vec3& b) { return vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
        inline float length(const vec3& a) { return std::sqrt(dot(a, a)); }
        inline vec3 normalize(const vec3& a) { float l = length(a); return (l > 0.0f) ? (a / l) : vec3(0, 0, 0); }

        // vec4
        struct vec4 {
            float x, y, z, w;
            constexpr vec4() : x(0), y(0), z(0), w(0) {}
            constexpr vec4(float X, float Y, float Z, float W) : x(X), y(Y), z(Z), w(W) {}
            constexpr vec4(const vec3& v, float W) : x(v.x), y(v.y), z(v.z), w(W) {}
            float& operator[](int i) { return (&x)[i]; }
            const float& operator[](int i) const { return (&x)[i]; }
        };

        inline vec4 operator+(const vec4& a, const vec4& b) { return vec4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w); }
        inline vec4 operator-(const vec4& a, const vec4& b) { return vec4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w); }
        inline vec4 operator*(const vec4& a, float s) { return vec4(a.x * s, a.y * s, a.z * s, a.w * s); }
        inline vec4 operator*(float s, const vec4& a) { return a * s; }
        inline vec4 operator/(const vec4& a, float s) { return vec4(a.x / s, a.y / s, a.z / s, a.w / s); }
        inline float dot(const vec4& a, const vec4& b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
        inline float length(const vec4& a) { return std::sqrt(dot(a, a)); }
        inline vec4 normalize(const vec4& a) { float l = length(a); return (l > 0.0f) ? (a / l) : vec4(0, 0, 0, 0); }

        // mat4 (column-major)
        struct mat4 {
            vec4 c[4];
            constexpr mat4()
                : c{ vec4(1,0,0,0), vec4(0,1,0,0), vec4(0,0,1,0), vec4(0,0,0,1) } {}
            explicit constexpr mat4(float diag)
                : c{ vec4(diag,0,0,0), vec4(0,diag,0,0), vec4(0,0,diag,0), vec4(0,0,0,diag) } {}
            vec4& operator[](int i) { return c[i]; }
            const vec4& operator[](int i) const { return c[i]; }
        };

        inline vec4 operator*(const mat4& m, const vec4& v) {
            return vec4(
                m[0].x * v.x + m[1].x * v.y + m[2].x * v.z + m[3].x * v.w,
                m[0].y * v.x + m[1].y * v.y + m[2].y * v.z + m[3].y * v.w,
                m[0].z * v.x + m[1].z * v.y + m[2].z * v.z + m[3].z * v.w,
                m[0].w * v.x + m[1].w * v.y + m[2].w * v.z + m[3].w * v.w
            );
        }

        inline mat4 operator*(const mat4& a, const mat4& b) {
            mat4 r(0.0f);
            r[0] = a * b[0];
            r[1] = a * b[1];
            r[2] = a * b[2];
            r[3] = a * b[3];
            return r;
        }

        inline mat4 transpose(const mat4& m) {
            mat4 r(0.0f);
            r[0].x = m[0].x; r[0].y = m[1].x; r[0].z = m[2].x; r[0].w = m[3].x;
            r[1].x = m[0].y; r[1].y = m[1].y; r[1].z = m[2].y; r[1].w = m[3].y;
            r[2].x = m[0].z; r[2].y = m[1].z; r[2].z = m[2].z; r[2].w = m[3].z;
            r[3].x = m[0].w; r[3].y = m[1].w; r[3].z = m[2].w; r[3].w = m[3].w;
            return r;
        }

        inline mat4 translate(const mat4& m, const vec3& v) {
            mat4 r = m;
            r[3] = m[0] * v.x + m[1] * v.y + m[2] * v.z + m[3];
            return r;
        }

        inline mat4 perspective(float fovy, float aspect, float zNear, float zFar) {
            float f = 1.0f / std::tan(fovy * 0.5f);
            mat4 r(0.0f);
            r[0].x = f / aspect;
            r[1].y = f;
            r[2].z = (zFar + zNear) / (zNear - zFar);
            r[2].w = -1.0f;
            r[3].z = (2.0f * zFar * zNear) / (zNear - zFar);
            return r;
        }

        inline mat4 ortho(float left, float right, float bottom, float top, float zNear, float zFar) {
            mat4 r(1.0f);
            r[0].x = 2.0f / (right - left);
            r[1].y = 2.0f / (top - bottom);
            r[2].z = -2.0f / (zFar - zNear);
            r[3].x = -(right + left) / (right - left);
            r[3].y = -(top + bottom) / (top - bottom);
            r[3].z = -(zFar + zNear) / (zFar - zNear);
            return r;
        }

        // quat
        struct quat {
            float w, x, y, z;
            quat() : w(1), x(0), y(0), z(0) {}
            quat(float W, float X, float Y, float Z) : w(W), x(X), y(Y), z(Z) {}
            explicit quat(const vec3& euler); // defined after helpers
        };

        inline quat operator*(const quat& a, const quat& b) {
            return quat(
                a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z,
                a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
                a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
                a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w
            );
        }

        inline quat normalize(const quat& q) {
            float s = std::sqrt(q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z);
            if (s <= 0.0f) return quat();
            float inv = 1.0f / s;
            return quat(q.w*inv, q.x*inv, q.y*inv, q.z*inv);
        }

        inline quat angleAxis(float angle, const vec3& axis) {
            vec3 a = normalize(axis);
            float s = std::sin(angle * 0.5f);
            float c = std::cos(angle * 0.5f);
            return quat(c, a.x * s, a.y * s, a.z * s);
        }

        inline mat4 toMat4(const quat& q_) {
            quat q = normalize(q_);
            float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
            float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
            float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;

            mat4 r(1.0f);
            // Row/column mapping for column-major storage:
            r[0].x = 1.0f - 2.0f * (yy + zz);
            r[0].y = 2.0f * (xy + wz);
            r[0].z = 2.0f * (xz - wy);

            r[1].x = 2.0f * (xy - wz);
            r[1].y = 1.0f - 2.0f * (xx + zz);
            r[1].z = 2.0f * (yz + wx);

            r[2].x = 2.0f * (xz + wy);
            r[2].y = 2.0f * (yz - wx);
            r[2].z = 1.0f - 2.0f * (xx + yy);

            // r[3] is (0,0,0,1) from mat4(1.0f)
            return r;
        }

        inline mat4 rotate(const mat4& m, float angle, const vec3& axis) {
            // matches GLM semantics: result = m * R(angle, axis)
            return m * toMat4(angleAxis(angle, axis));
        }

        inline quat::quat(const vec3& euler) {
            // GLM’s quat(vec3) uses XYZ intrinsic rotations in radians.
            quat qx = angleAxis(euler.x, vec3(1,0,0));
            quat qy = angleAxis(euler.y, vec3(0,1,0));
            quat qz = angleAxis(euler.z, vec3(0,0,1));
            *this = qz * qy * qx;
        }

    } // namespace glm
#endif
