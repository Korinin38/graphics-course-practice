#pragma once

#include <vector>
#include <map>
#include <filesystem>

namespace obj_parser {
    struct mtl {
        std::array<float, 3> glossiness;    // Ks
        float roughness;                    // Ns
        std::filesystem::path albedo;       // map_Ka
        std::filesystem::path transparency; // map_d
    };

    typedef std::map<std::string, mtl> mtllib;

    struct obj_data {
        struct vertex {
            std::array<float, 3> position;  // v
            std::array<float, 3> normal;    // vn
            std::array<float, 2> texcoord;  // vt
        };
        struct group {
            mtl material;
            std::vector<std::uint32_t> indices;
        };
        std::vector<vertex> vertices;
        std::map<std::string, group> groups; // g / f
    };


    // expands already existing mtl library
    void parse_mtl(std::filesystem::path const &path, mtllib& add);
    obj_data parse_obj(std::filesystem::path const &path);
}