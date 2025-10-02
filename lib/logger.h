#pragma once

#include <iostream>
#include <filesystem>
#include <vector>
#include <fstream>
#include <string>
#include <iomanip>
#include <sstream>
#include <cctype>

#include "Options.h"

struct TestRunData {
    std::string imagePath;
    std::string depthPath;
    Options options;
};

class logger {
private:
    inline static std::string htmlhead = R"(<!DOCTYPE html>
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <meta name="description" content="Magic eye configureations.">
        <title>Magic Eye</title>
        <style>
            table {
                border-collapse: collapse;
                width: 100%;
                margin: 20px 0;
            }
            th, td {
                border: 1px solid #ddd;
                padding: 8px;
                text-align: left;
            }
            th {
                background-color: #f2f2f2;
                font-weight: bold;
            }
            tr:nth-child(even) {
                background-color: #f9f9f9;
            }
            .number {
                text-align: left;
            }
            .path {
                text-align: left;
            }
        </style>
    </head>
)";

public:
    void log(std::ofstream& file, const std::vector<TestRunData>& dataset)
    {
        file << htmlhead;
        file << "<body>\n";

        std::string im_load_name = "imageload_";
        std::string im_fallback_name = "imagefallback_";
        std::string im_depth_name = "imagedepth_";
        std::string im_depth_fallback_name = "imagefallbackdepth_";
        std::string im_tex_name = "imagetexture_";
        std::string im_tex_fallback_name = "imagetexturefallback_";

        int image_num = 0;
        for (const auto& data : dataset) {
            std::string im_suffix = std::to_string(++image_num);

            file << R"(
            <table>
                <tr>
                    <th>stl</th>
                    <th>depth</th>
                    <th>img</th>
                    <th>texture</th>
                </tr>)";

            file << "<tr>\n";
            file << "<td>" << data.options.stlpath << "</td>";
            file << "<td>" << data.imagePath << "</td>";
            file << "<td>" << data.depthPath << "</td>";
            file << "<td>" << data.options.texpath << "</td>";
            file << "</tr>\n";
            file << "<tr>\n<td></td>\n";
            file << R"(
                <td>
                    <img id=')"
                << im_depth_name << im_suffix << R"(' width="100" style="display:none; " alt="image"
                            src=')" << data.depthPath << R"('>
                    <div id=')" << im_depth_fallback_name << im_suffix << R"(' style="border: 1px solid #ccc; padding: 10px;">
                        <em>Loading image...</em>
                    </div>
                </td>
                <td>
                    <img id=')"
                << im_load_name << im_suffix << R"(' width="100" style="display:none; " alt="image"
                            src=')" << data.imagePath << R"('>
                    <div id=')" << im_fallback_name << im_suffix << R"(' style="border: 1px solid #ccc; padding: 10px;">
                        <em>Loading image...</em>
                    </div>
                </td>
                <td>
                    <img id=')"
                << im_tex_name << im_suffix << R"(' width="100" style="display:none; " alt="image"
                        src=')" << data.options.texpath << R"('>
                    <div id=')" << im_tex_fallback_name << im_suffix << R"(' style="border: 1px solid #ccc; padding: 10px;">
                        <em>Loading image...</em>
                    </div>
                </td>
)";
            file << "\n</tr>\n";
            file <<
                "<tr>\n" <<
                "<table>\n" <<
                "<tr>\n" <<
                "<th>width</th>\n" <<
                "<th>height</th>\n" <<
                "<th>eye_sep</th>\n" <<
                "<th>per</th>\n" <<
                R"(<th colspan="3">custom_cam_pos</th>)" << "\n" <<
                R"(<th colspan="3">custom_look_at</th>)" << "\n" <<
                R"(<th colspan="3">rot_deg</th>)" << "\n" <<
                R"(<th colspan="3">trans</th>)" << "\n" <<
                R"(<th colspan="3">sc</th>)" << "\n" <<
                R"(<th colspan="3">shear</th>)" << "\n" <<
                "<th>c pos</th>\n" <<
                "<th>lookt</th>\n" <<
                "<th>use oscale</th>\n" <<
                "<th>lp</th>\n" <<
                "<th>lp layers</th>\n" <<
                "</tr>\n";
            file << "<tr>\n" <<
                R"(    <td class="number"> )" << data.options.width << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.height << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.eye_sep << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.perspective << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.custom_cam_pos[0] << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.custom_cam_pos[1] << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.custom_cam_pos[2] << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.custom_look_at[0] << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.custom_look_at[1] << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.custom_look_at[2] << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.rot_deg[0] << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.rot_deg[1] << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.rot_deg[2] << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.trans[0] << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.trans[1] << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.trans[2] << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.sc[0] << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.sc[1] << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.sc[2] << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.shear[0] << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.shear[1] << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.shear[2] << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.custom_cam_provided << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.custom_lookat_provided << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.custom_orth_scale_provided << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.laplace_smoothing << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.laplace_smooth_layers << "</td>\n" <<
                "</tr>\n" <<
                "</table>\n";
            file <<
                "<tr>\n" <<
                "<table>\n" <<
                "<tr>\n" <<
                "<th>or scale</th>\n" <<
                "<th>fov</th>\n" <<
                "<th>depth_near</th>\n" <<
                "<th>depth_far</th>\n" <<
                "<th>brightness</th>\n" <<
                "<th>contrast</th>\n" <<
                "<th>bg sep</th>\n" <<
                "<th>depth gama</th>\n" <<
                "<th>orthTuneLow</th>\n" <<
                "<th>orthTuneHi</th>\n" <<
                "<th>for threshold</th>\n" <<
                "<th>smooth threshold</th>\n" <<
                "<th>smooth weight</th>\n" <<
                "</tr>\n";
            file << "<tr>\n" <<
                R"(    <td class="number"> )" << data.options.custom_orth_scale << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.fov << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.depth_near << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.depth_far << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.texture_brightness << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.texture_contrast << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.bg_separation << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.depth_gamma << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.orthTuneLow << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.orthTuneHi << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.foreground_threshold << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.smoothThreshold << "</td>\n" <<
                R"(    <td class="number"> )" << data.options.smoothWeight << "</td>\n" <<
                "</tr>";

            file << "</tr>\n";
        }

        file << R"(   </table>
    <script>
    function setupImage(imgId, fallbackId) {
        var img = document.getElementById(imgId);
        var fallback = document.getElementById(fallbackId);
        if (img && fallback) {
            img.onload = function() {
                img.style.display = 'block';
                fallback.style.display = 'none';
            };
            img.onerror = function() {
                fallback.innerHTML = "<strong>Error:</strong> Can't load image from " + img.src;
            };
            img.onclick = function() {
                window.open(img.src);
            };
        }
    }
    for (let i = 1; i < )" << (int)dataset.size() + 1 << R"(; i++) {
        let load = 'imageload_' + i.toString();
        let fall = 'imagefallback_' + i.toString();
        let depth = 'imagedepth_' + i.toString();
        let depthfall = 'imagefallbackdepth_' + i.toString();
        let texture = 'imagetexture_' + i.toString();
        let textfall = 'imagetexturefallback_' + i.toString();
        setupImage(load, fall);
        setupImage(depth, depthfall);
        setupImage(texture, textfall);
    }
    </script>
</body>
)";
    }
};
