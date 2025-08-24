// Written by Paul Baxter
#include <gtest/gtest.h>
#include <string>
#include <iostream>
#include <filesystem>
#include <vector>

#include "Options.h"
#include "StereogramGenerator.h"

#pragma warning(disable:4996)

namespace fs = std::filesystem;
void iterateDirectory(const std::string& path, std::vector<fs::path>& files)
{
    try {
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.is_regular_file()) {
                const auto& path = entry.path();
                files.push_back(path);
            }
        }
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

// Recursive iteration
void iterateRecursive(const std::string& path, std::vector<fs::path>& files)
{
    try {
        for (const auto& entry : fs::recursive_directory_iterator(path)) {
            std::cout << entry.path() << std::endl;
        }
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

// Add this to your test file
struct TestConfig {
    std::string name;
    Options options;
};

void setupBasicOptions(Options& options)
{
    options.sc = { -2.0, -1.0, -1.5 };
    options.rot_deg = { 100, 20, 0 };
    options.fov = 27;
    options.depth_near = .72;
    options.depth_far = .05;
    options.eye_sep = 160;
    options.texture_brightness = .8;
    options.texture_contrast = 1.0;
    options.bg_separation = .4;
    options.height = 800;
    options.width = 1200;
    options.shear = { .12, .12, .1 };
    options.depth_gamma = 1.01;
}

std::vector<TestConfig> generateTestConfigs()
{
    std::vector<TestConfig> configs;

    // Base configuration
    Options base;
    base.sc = { -2.0, -1.0, -1.5 };
    base.rot_deg = { 100, 20, 0 };
    base.fov = 27;
    base.depth_near = .72;
    base.depth_far = .05;
    base.eye_sep = 160;
    base.texture_brightness = .8;
    base.texture_contrast = 1.0;
    base.bg_separation = .4;
    base.height = 800;
    base.width = 1200;
    base.shear = { .12, .12, .1 };
    base.depth_gamma = 1.01;

    // Critical parameter variations
    std::vector<float> fov_values = { 20, 27, 35 };
    std::vector<float> depth_values = { 0.5, 0.72, 0.9 };
    std::vector<float> eye_sep_values = { 120, 160, 200 };

    for (float fov : fov_values) {
        for (float depth : depth_values) {
            for (float eye_sep : eye_sep_values) {
                TestConfig config;
                config.options = base;
                config.options.fov = fov;
                config.options.depth_near = depth;
                config.options.eye_sep = eye_sep;
                config.name = "fov_" + std::to_string((int)fov) +
                    "_depth_" + std::to_string((int)(depth * 100)) +
                    "_sep_" + std::to_string((int)eye_sep);
                configs.push_back(config);
            }
        }
    }

    return configs;
}

std::vector<fs::path> selectRepresentativeFiles(const std::vector<fs::path>& files, size_t count)
{
    if (files.size() <= count) return files;

    std::vector<fs::path> selected;
    // Select files that represent different categories
    for (const auto& file : files) {
        std::string ext = file.extension().string();
        std::string stem = file.stem().string();

        // Simple heuristic: pick files with different characteristics
        if (stem.find("simple") != std::string::npos && selected.size() < count / 3) {
            selected.push_back(file);
        }
        else if (stem.find("complex") != std::string::npos && selected.size() < 2 * count / 3) {
            selected.push_back(file);
        }
        else if (selected.size() < count) {
            selected.push_back(file);
        }
    }

    return selected;
}

void testFovVariations()
{
    std::vector<fs::path> stl_files;
    iterateDirectory("..\\..\\..\\unittests\\stl\\", stl_files);

    std::vector<fs::path> texture_files;
    iterateDirectory("..\\..\\..\\unittests\\texture\\", texture_files);

    std::filesystem::create_directory("..\\..\\..\\unittests\\out\\fov\\");

    // Select representative samples
    auto representative_stl = selectRepresentativeFiles(stl_files, 2); // Pick 2 diverse STLs
    auto representative_textures = selectRepresentativeFiles(texture_files, 1); // Pick 1 texture

    std::vector<TestConfig> test_configs = generateTestConfigs();

    auto n = 1;
    for (auto& stlpath : representative_stl) {
        for (auto& texturepath : representative_textures) {
            for (auto& config : test_configs) {

                for (float fov = 10; fov < 45; fov += 5) {
                    // Apply configuration and test
                    config.options.fov = fov;
                    config.options.stlpath = stlpath.string();
                    config.options.texpath = texturepath.string();
                    config.options.depth_gamma = 1.0;
                    config.options.outprefix = "..\\..\\..\\unittests\\out\\fov\\" + std::to_string(n++);
                    auto result = StereogramGenerator::create(std::make_shared<Options>(config.options));
                    EXPECT_EQ(result, 0);
                }
            }
        }
    }
}

void testDepthVariations()
{
    std::vector<fs::path> stl_files;
    iterateDirectory("..\\..\\..\\unittests\\stl\\", stl_files);

    std::vector<fs::path> texture_files;
    iterateDirectory("..\\..\\..\\unittests\\texture\\", texture_files);

    std::filesystem::create_directory("..\\..\\..\\unittests\\out\\depth\\");

    // Select representative samples
    auto representative_stl = selectRepresentativeFiles(stl_files, 2); // Pick 2 diverse STLs
    auto representative_textures = selectRepresentativeFiles(texture_files, 1); // Pick 1 texture

    std::vector<TestConfig> test_configs = generateTestConfigs();

    auto n = 1;
    for (auto& stlpath : representative_stl) {
        for (auto& texturepath : representative_textures) {
            for (auto& config : test_configs) {

                for (float near = 0.65; near < 1.0; near += 0.2) {
                    for (float far = 0; far < .3; far += 0.2) {


                        // Apply configuration and test

                        config.options.depth_near = near;
                        config.options.depth_far = far;
                        config.options.depth_gamma = 1.0;

                        config.options.stlpath = stlpath.string();
                        config.options.texpath = texturepath.string();
                        config.options.outprefix = "..\\..\\..\\unittests\\out\\depth\\" + std::to_string(n++);
                        auto result = StereogramGenerator::create(std::make_shared<Options>(config.options));
                        EXPECT_EQ(result, 0);
                    }
                }
            }
        }
    }
}

void testEyeSeparationVariations()
{
    std::vector<fs::path> stl_files;
    iterateDirectory("..\\..\\..\\unittests\\stl\\", stl_files);

    std::vector<fs::path> texture_files;
    iterateDirectory("..\\..\\..\\unittests\\texture\\", texture_files);

    std::filesystem::create_directory("..\\..\\..\\unittests\\out\\eyesep\\");

    // Select representative samples
    auto representative_stl = selectRepresentativeFiles(stl_files, 2); // Pick 2 diverse STLs
    auto representative_textures = selectRepresentativeFiles(texture_files, 1); // Pick 1 texture

    std::vector<TestConfig> test_configs = generateTestConfigs();

    auto n = 1;
    for (auto& stlpath : representative_stl) {
        for (auto& texturepath : representative_textures) {
            for (auto& config : test_configs) {

                for (float eyesep = 30; eyesep < 190; eyesep += 5) {
                    // Apply configuration and test

                    config.options.eye_sep = eyesep;
                    config.options.depth_gamma = 1.0;

                    config.options.stlpath = stlpath.string();
                    config.options.texpath = texturepath.string();
                    config.options.outprefix = "..\\..\\..\\unittests\\out\\eyesep\\" + std::to_string(n++);
                    auto result = StereogramGenerator::create(std::make_shared<Options>(config.options));
                    EXPECT_EQ(result, 0);
                }
            }
        }
    }
}

class StereogramTest : public ::testing::TestWithParam<std::tuple<std::string, std::string, TestConfig>> {};

TEST_P(StereogramTest, GenerateImage)
{
    auto [stl_file, texture_file, config] = GetParam();
    static int n = 0;

    config.options.stlpath = stl_file;
    config.options.texpath = texture_file;
    config.options.outprefix = "..\\..\\..\\unittests\\out\\generateImage\\" + std::to_string(n++);
    auto result = StereogramGenerator::create(std::make_shared<Options>(config.options));
    EXPECT_EQ(result, 0);
}

INSTANTIATE_TEST_SUITE_P(
    MagicEyeTests,
    StereogramTest,
    ::testing::Combine(
        ::testing::Values("..\\..\\..\\unittests\\stl\\cat.stl", "..\\..\\..\\unittests\\stl\\enterprise.stl"), // Key STLs
        ::testing::Values("..\\..\\..\\unittests\\texture\\brick.jpg", "..\\..\\..\\unittests\\texture\\01.jpg"),         // Key textures
        ::testing::ValuesIn(generateTestConfigs())                 // Parameter variations
    )
);

TEST(stl_test, representative_tests)
{
    std::vector<fs::path> stl_files;
    iterateDirectory("..\\..\\..\\unittests\\stl\\", stl_files);

    std::vector<fs::path> texture_files;
    iterateDirectory("..\\..\\..\\unittests\\texture\\", texture_files);

    std::filesystem::create_directory("..\\..\\..\\unittests\\out\\representative\\");

    // Select representative samples
    auto representative_stl = selectRepresentativeFiles(stl_files, 3); // Pick 3 diverse STLs
    auto representative_textures = selectRepresentativeFiles(texture_files, 2); // Pick 2 textures

    auto test_configs = generateTestConfigs();

    auto n = 1;
    for (auto& stlpath : representative_stl) {
        for (auto& texturepath : representative_textures) {
            for (auto& config : test_configs) {
                // Apply configuration and test
                config.options.stlpath = stlpath.string();
                config.options.texpath = texturepath.string();
                config.options.outprefix = "..\\..\\..\\unittests\\out\\representative\\" + std::to_string(n++);
                auto result = StereogramGenerator::create(std::make_shared<Options>(config.options));
                EXPECT_EQ(result, 0);
            }
        }
    }
}

TEST(stl_test, smoke_test)
{
    // Quick test to ensure basic functionality
    
    Options options;
    setupBasicOptions(options);

    options.stlpath = "..\\..\\..\\unittests\\stl\\Spinosaurus.stl";
    options.texpath = "..\\..\\..\\unittests\\texture\\brick.jpg";
    options.outprefix = "..\\..\\..\\unittests\\out\\smoke\\Spinosaurus_brick";
    std::filesystem::create_directory("..\\..\\..\\unittests\\out\\smoke\\");
    auto result = StereogramGenerator::create(std::make_shared<Options>(options));
    EXPECT_EQ(result, 0);
}

TEST(stl_test, focused_parameter_studies)
{
    // Test one parameter at a time while keeping others constant
    testFovVariations();
    testDepthVariations();
    testEyeSeparationVariations();
}

namespace magiceye_unit_test
{
    TEST(stl_test, magiceye_unit_test)
    {
        auto options = std::make_shared<Options>();

        std::vector<fs::path> stl_files;
        iterateDirectory("..\\..\\..\\unittests\\stl\\", stl_files);

        std::vector<fs::path> texture_files;
        iterateDirectory("..\\..\\..\\unittests\\texture\\", texture_files);

        for (auto& stlpath : stl_files) {
            std::string stl = stlpath.filename().string();

            auto dir = std::string("..\\..\\..\\unittests\\out\\") + stl.substr(0, stl.size() - 4);
            std::filesystem::create_directory(dir);

            for (auto& texturepath : texture_files) {
                std::string texture = texturepath.filename().string();
                options->stlpath = std::string("..\\..\\..\\unittests\\stl\\") + stl;
                options->texpath = std::string("..\\..\\..\\unittests\\texture\\") + texture;
                options->outprefix = dir + "\\" + stl.substr(0, stl.size() - 4) + "_" + texture.substr(0, texture.size() - 4);
                options->sc = { -2.0, -1.0, -1.5 };
                options->rot_deg = { 100, 20, 0 };
                options->fov = 27;
                options->depth_near = .72;
                options->depth_far = .05;
                options->eye_sep = 160;
                options->texture_brightness = .8;
                options->texture_contrast = 1.0;
                options->bg_separation = .4;
                options->height = 800;
                options->width = 1200;
                options->shear = { .12, .12, .1 };
                options->depth_gamma = 1.01;

                auto result = StereogramGenerator::create(options);
                EXPECT_EQ(result, 0);
            }
        }
    }
}
