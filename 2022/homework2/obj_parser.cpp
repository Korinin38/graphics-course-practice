#include "obj_parser.hpp"

#include <string>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <map>

namespace
{

    template <typename ... Args>
    std::string to_string(Args const & ... args)
    {
        std::ostringstream os;
        (os << ... << args);
        return os.str();
    }

}
namespace obj_parser {
    void parse_mtl(std::filesystem::path const &path, mtllib &lib) {
        std::ifstream is(path);

        std::string line;
        std::size_t line_count = 0;

//        auto fail = [&](auto const &... args) {
//            throw std::runtime_error(to_string("Error parsing OBJ data, line ", line_count, ": ", args...));
//        };

        std::string material_name = "";
        mtl material;

        while (std::getline(is >> std::ws, line)) {
            ++line_count;

            if (line.empty()) continue;

            if (line[0] == '#') continue;

            std::istringstream ls(std::move(line));

            std::string tag;
            ls >> tag;

            if (tag == "newmtl") {
                if (material_name != "")
                    lib[material_name] = material;
                ls >> material_name;
            } else if (tag == "Ks") {
                auto &n = material.glossiness;
                ls >> n[0] >> n[1] >> n[2];
            } else if (tag == "Ns") {
                ls >> material.roughness;
            } else if (tag == "map_Ka") {
                std::string tex_name;
                ls >> tex_name;
                material.albedo = path.parent_path();
                material.albedo += "/" + tex_name;
            } else if (tag == "map_d") {
                std::string tex_name;
                ls >> tex_name;
                material.transparency = path.parent_path();
                material.transparency += "/" + tex_name;
            }
        }

        lib[material_name] = material;
    }

    obj_data parse_obj(std::filesystem::path const &path) {
        std::ifstream is(path);

        std::vector<std::array<float, 3>> positions;
        std::vector<std::array<float, 3>> normals;
        std::vector<std::array<float, 2>> texcoords;

        std::map<std::array<std::uint32_t, 3>, std::uint32_t> index_map;

        obj_data result;

        std::string line;
        std::size_t line_count = 0;

        mtllib mtllib;
        std::string mtl_name;

        std::string group_name;
        obj_data::group cur_group;

        auto fail = [&](auto const &... args) {
            throw std::runtime_error(to_string("Error parsing OBJ data, line ", line_count, ": ", args...));
        };

        while (std::getline(is >> std::ws, line)) {
            ++line_count;

            if (line.empty()) continue;

            if (line[0] == '#') continue;

            std::istringstream ls(std::move(line));

            std::string tag;
            ls >> tag;

            if (tag == "mtllib") {
                std::string name;
                ls >> name;
                std::filesystem::path mtlpath = path.parent_path();
                mtlpath += "/" + name;
                parse_mtl(mtlpath, mtllib);
            } else if (tag == "usemtl") {
                ls >> mtl_name;
//                if (mtllib.find(mtl_name) == mtllib.end())
//                    fail("unknown material name \"", mtl_name, "\"");
                cur_group.material = mtllib[mtl_name];
            } else if (tag == "g") {
                if (!group_name.empty())
                    result.groups[group_name] = cur_group;
                ls >> group_name;
            } else if (tag == "v") {
                auto &p = positions.emplace_back();
                ls >> p[0] >> p[1] >> p[2];
            } else if (tag == "vn") {
                auto &n = normals.emplace_back();
                ls >> n[0] >> n[1] >> n[2];
            } else if (tag == "vt") {
                auto &t = texcoords.emplace_back();
                ls >> t[0] >> t[1];
            } else if (tag == "f") {
                std::vector<std::uint32_t> vertices;

                while (ls) {
                    std::array<std::int32_t, 3> input_index{0, 0, 0};

                    ls >> input_index[0];
                    if (ls.eof()) break;
                    if (!ls)
                        fail("expected position index");

                    if (!std::isspace(ls.peek()) && !ls.eof()) {
                        if (ls.get() != '/')
                            fail("expected '/'");

                        if (ls.peek() != '/') {
                            ls >> input_index[1];
                            if (!ls)
                                fail("expected texcoord index");

                            if (!std::isspace(ls.peek()) && !ls.eof()) {
                                if (ls.get() != '/')
                                    fail("expected '/'");

                                ls >> input_index[2];
                                if (!ls)
                                    fail("expected normal index");
                            }
                        } else {
                            ls.get();

                            ls >> input_index[2];
                            if (!ls)
                                fail("expected normal index");
                        }
                    }

                    std::array<std::uint32_t, 3> index{0, 0, 0};
                    if (input_index[0] < 0)
                        index[0] = positions.size() + input_index[0];
                    else
                        index[0] = input_index[0] - 1;
                    if (input_index[1] < 0)
                        index[1] = texcoords.size() + input_index[1];
                    else
                        index[1] = input_index[1] - 1;
                    if (input_index[2] < 0)
                        index[2] = normals.size() + input_index[2];
                    else
                        index[2] = input_index[2] - 1;

                    if (index[0] >= positions.size())
                        fail("bad position index (", index[0], ")");

                    if (index[1] != -1 && index[1] >= texcoords.size())
                        fail("bad texcoord index (", index[1], ")");

                    if (index[2] != -1 && index[2] >= normals.size())
                        fail("bad normal index (", index[2], ")");

                    auto it = index_map.find(index);
                    if (it == index_map.end()) {
                        it = index_map.insert({index, result.vertices.size()}).first;

                        auto &v = result.vertices.emplace_back();

                        v.position = positions[index[0]];

                        if (index[1] != -1)
                            v.texcoord = texcoords[index[1]];
                        else
                            v.texcoord = {0.f, 0.f};

                        if (index[2] != -1)
                            v.normal = normals[index[2]];
                        else
                            v.normal = {0.f, 0.f, 0.f};
                    }

                    vertices.push_back(it->second);
                }

                for (std::size_t i = 1; i + 1 < vertices.size(); ++i) {
                    cur_group.indices.push_back(vertices[0]);
                    cur_group.indices.push_back(vertices[i]);
                    cur_group.indices.push_back(vertices[i + 1]);
                }
            }
        }

        result.groups[group_name] = cur_group;
        return result;
    }
}