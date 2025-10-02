
// Written by Paul Baxter (revised)
#pragma once
#include <cstdint>
#include <glm/fwd.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

class vectorutils {
public:
    // apply 3d rotation of array using Quaternion
    static void rotateQuaternion(float* array, uint32_t vcount, float xrot_deg, float yrot_deg, float zrot_deg, const glm::vec3& origin = { 0.0f, 0.0f, 0.0f })
    {
        if (!array || vcount == 0) return;

        glm::quat rotation = glm::quat(glm::vec3(glm::radians(xrot_deg), glm::radians(yrot_deg), glm::radians(zrot_deg)));
        glm::mat4 toOrigin = glm::translate(glm::mat4(1.0f), -origin);
        glm::mat4 fromOrigin = glm::translate(glm::mat4(1.0f), origin);
        glm::mat4 rotMatrix = glm::toMat4(rotation);
        glm::mat4 transform = fromOrigin * rotMatrix * toOrigin;

        for (size_t i = 0; i < vcount; ++i) {
            glm::vec4 v(array[i * 3 + 0], array[i * 3 + 1], array[i * 3 + 2], 1.0f);
            v = transform * v;
            array[i * 3 + 0] = v.x;
            array[i * 3 + 1] = v.y;
            array[i * 3 + 2] = v.z;
        }
    }

    static void rotate(float* array, uint32_t vcount, float xrot_deg, float yrot_deg, float zrot_deg, const glm::vec3& origin = { 0, 0, 0 })
    {
        glm::mat4 rx = glm::rotate(glm::mat4(1.0f), glm::radians(xrot_deg), glm::vec3(1, 0, 0));
        glm::mat4 ry = glm::rotate(glm::mat4(1.0f), glm::radians(yrot_deg), glm::vec3(0, 1, 0));
        glm::mat4 rz = glm::rotate(glm::mat4(1.0f), glm::radians(zrot_deg), glm::vec3(0, 0, 1));
        glm::mat4 rot = rz * ry * rx;

        glm::mat4 toOrigin = glm::translate(glm::mat4(1.0f), -origin);
        glm::mat4 fromOrigin = glm::translate(glm::mat4(1.0f), origin);

        glm::mat4 transform = fromOrigin * rot * toOrigin;

        for (size_t i = 0; i < vcount; ++i) {
            glm::vec4 v(array[i * 3 + 0], array[i * 3 + 1], array[i * 3 + 2], 1.0f);
            v = transform * v;
            array[i * 3 + 0] = v.x;
            array[i * 3 + 1] = v.y;
            array[i * 3 + 2] = v.z;
        }
    }

    static void translate(float* array, uint32_t vcount, float xoffset, float yoffset, float zoffset)
    {
        for (size_t i = 0; i < vcount; ++i) {
            array[i * 3 + 0] += xoffset;
            array[i * 3 + 1] += yoffset;
            array[i * 3 + 2] += zoffset;
        }
    }

    static void scale(float* array, uint32_t vcount, float xscale, float yscale, float zscale)
    {
        for (size_t i = 0; i < vcount; ++i) {
            array[i * 3 + 0] *= xscale;
            array[i * 3 + 1] *= yscale;
            array[i * 3 + 2] *= zscale;
        }
    }

    static void shear_mesh(float* array, uint32_t vcount, float sh_xy, float sh_xz, float sh_yz)
    {
        glm::mat4 shearMat(1.0f);
        shearMat[1][0] = sh_xy; // y += sh_xy * x
        shearMat[2][0] = sh_xz; // z += sh_xz * x
        shearMat[2][1] = sh_yz; // z += sh_yz * y

        for (size_t i = 0; i < vcount; ++i) {
            glm::vec4 v(array[i * 3 + 0], array[i * 3 + 1], array[i * 3 + 2], 1.0f);
            v = shearMat * v;
            array[i * 3 + 0] = v.x;
            array[i * 3 + 1] = v.y;
            array[i * 3 + 2] = v.z;
        }
    }

    static void min_max(const float* array, uint32_t vcount,
        float& minx, float& maxx,
        float& miny, float& maxy,
        float& minz, float& maxz)
    {
        for (size_t i = 0; i < vcount; ++i) {
            float x = array[i * 3 + 0];
            float y = array[i * 3 + 1];
            float z = array[i * 3 + 2];
            minx = std::min(minx, x); maxx = std::max(maxx, x);
            miny = std::min(miny, y); maxy = std::max(maxy, y);
            minz = std::min(minz, z); maxz = std::max(maxz, z);
        }
    }
};
