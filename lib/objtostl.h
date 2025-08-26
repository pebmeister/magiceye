#pragma once

#include <rapidobj/rapidobj.hpp>
#include <stl.h>
#include <array>

class OBJToSTL {
public:
    static bool convert(const std::string& objfile, stl& mesh)
    {
        auto result = rapidobj::ParseFile(objfile);
        if (result.error) {
            return false;
        }
        rapidobj::Triangulate(result);

        // Clear STL
        mesh.m_vectors.clear();
        mesh.m_num_triangles = 0;

        for (auto& shape : result.shapes) {
            auto& meshobj = shape.mesh;
            for (size_t i = 0; i < meshobj.indices.size(); i += 3) {
                auto i1 = meshobj.indices[i + 0].position_index;
                auto i2 = meshobj.indices[i + 1].position_index;
                auto i3 = meshobj.indices[i + 2].position_index;

                std::array<float, 9> tri = {
                    result.attributes.positions[i1 * 3 + 0],
                    result.attributes.positions[i1 * 3 + 1],
                    result.attributes.positions[i1 * 3 + 2],

                    result.attributes.positions[i2 * 3 + 0],
                    result.attributes.positions[i2 * 3 + 1],
                    result.attributes.positions[i2 * 3 + 2],

                    result.attributes.positions[i3 * 3 + 0],
                    result.attributes.positions[i3 * 3 + 1],
                    result.attributes.positions[i3 * 3 + 2]
                };

                mesh.m_vectors.insert(mesh.m_vectors.end(), tri.begin(), tri.end());
                mesh.m_num_triangles++;
            }
        }

        return true;
    }
};
