// Written by Paul Baxter
#include <gtest/gtest.h>
#include <string>
#include <iostream>
#include <filesystem>
#include <vector>
#include <fstream>
#include <atomic>
#include "Options.h"
#include "StereogramGenerator.h"
#include "logger.h"

#pragma warning(disable:4996)

namespace fs = std::filesystem;

// Add this to your test file
struct TestConfig {
    std::string name;
    Options options;
};

static std::string unittestpath = "..\\..\\..\\unittests\\";
static std::vector<TestRunData> globalTestData;
static std::atomic<int> testNum = 0;
static std::vector<std::string> stl_files;
static std::vector<std::string> texture_files;

class GlobalTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override
    {
        fs::path dirPath(unittestpath);
        if (!fs::exists(dirPath)) {
            unittestpath = "..\\..\\unittests\\";
        }

        // Runs before any tests start
        globalTestData.clear();
        std::cout << "Global test setup - starting test run\n";
    }

    void TearDown() override
    {
        // Runs after all tests complete
        std::cout << "Global test teardown - writing test log\n";
        writeTestLog();
    }
    
    static void writeTestLog()
    {
        logger log;

        std::ofstream logfile("test_run_log.html");
        log.log(logfile, globalTestData);
        std::cout << "Logged " << globalTestData.size() << " test runs to test_run_log.xml\n";
    }


    static void addTestData(const TestRunData& data)
    {
        globalTestData.push_back(data);
    }
};

static std::string GetTestName()
{
    return std::to_string(++testNum);
}

// Register the global environment
::testing::Environment* const global_env =
::testing::AddGlobalTestEnvironment(new GlobalTestEnvironment);

static void iterateDirectory(const std::string& path, std::vector<fs::path>& files)
{
    try {
        fs::path dirPath(path);

        if (!fs::exists(dirPath)) {
            std::cerr << "Directory does not exist: " << fs::absolute(dirPath) << std::endl;
            return;
        }

        if (!fs::is_directory(dirPath)) {
            std::cerr << "Path is not a directory: " << fs::absolute(dirPath) << std::endl;
            return;
        }

        for (const auto& entry : fs::directory_iterator(dirPath)) {
            if (entry.is_regular_file()) {
                files.push_back(entry.path());
            }
        }
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error in " << path << ": " << e.what() << std::endl;
    }
}

// Recursive iteration
static void iterateRecursive(const std::string& path, std::vector<fs::path>& files)
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

static void setupBasicOptions(Options& options)
{
    options.sc = { -1.0, -1.0, -1.0 };
    options.rot_deg = { 100, 20, 0 };
    options.fov = 27;
    options.depth_near = .72;
    options.depth_far = .05;
    options.eye_sep = 160;
    options.texture_brightness = 1.0;
    options.texture_contrast = 1.0;
    options.bg_separation = .6;
    options.height = 800;
    options.width = 1200;
    options.shear = { 0, 0, 0 };
    options.depth_gamma = 1.01;
}

std::vector<std::string> generateStlFiles()
{
    if (stl_files.size() == 0) {
        std::vector<fs::path> stl_paths;
        iterateDirectory(unittestpath + "stl\\", stl_paths);
        for (auto& path : stl_paths) {
            stl_files.push_back(path.string());
        }
    }
    return stl_files;
}

std::vector<std::string> generateTextureFiles()
{
    if (texture_files.size() == 0) {
        std::vector<fs::path> texture_paths;
        iterateDirectory(unittestpath + "texture\\", texture_paths);
        for (auto& path : texture_paths) {
            texture_files.push_back(path.string());
        }
    }
    return texture_files;
}

std::vector<TestConfig> generateTestConfigs()
{
    std::vector<TestConfig> configs;

    // Base configuration
    Options base;
    base.sc = { -1.0, -1.0, -1.0 };
    base.rot_deg = { 100, 20, 0 };
    base.fov = 27;
    base.depth_near = .72;
    base.depth_far = .05;
    base.eye_sep = 180;
    base.texture_brightness = .8;
    base.texture_contrast = 1.0;
    base.bg_separation = .6;
    base.height = 800;
    base.width = 1200;
    base.shear = { 0, 0, 0 };
    base.depth_gamma = 1.0;

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

std::vector<std::string> selectRepresentativeFiles(const std::vector<std::string>& files, size_t count)
{
    if (files.size() <= count) return files;

    std::vector<std::string> selected;
    // Select files that represent different categories
    for (const auto& file : files) {
        if (selected.size() < count) {
            selected.push_back(file);
        }
    }
    return selected;
}

static void testFovVariations()
{
    auto stl_files = generateStlFiles();
    auto texture_files = generateTextureFiles();

    std::filesystem::create_directory(unittestpath + "out\\");
    std::filesystem::create_directory(unittestpath + "out\\fov\\");

    // Select representative samples
    auto representative_stl = selectRepresentativeFiles(stl_files, 1);          // Pick 1 STLs
    auto representative_textures = selectRepresentativeFiles(texture_files, 1); // Pick 1 texture

    std::vector<TestConfig> test_configs = generateTestConfigs();

    auto n = 1;
    for (auto& stlpath : representative_stl) {
        for (auto& texturepath : representative_textures) {
            for (auto& config : test_configs) {

                for (float fov = 10; fov < 45; fov += 5) {
                    // Apply configuration and test
                    config.options.fov = fov;
                    config.options.stlpath = stlpath;
                    config.options.texpath = texturepath;
                    config.options.outprefix = unittestpath + "out\\fov\\" + GetTestName();

                    auto options = std::make_shared<Options>(config.options);
                    StereogramGenerator st(options);
                    auto result = st.create();

                    // Log the test run data
                    TestRunData data;
                    data.imagePath = config.options.outprefix + "_sirds.png";
                    data.depthPath = config.options.outprefix + "_depth.png";
                    data.options = config.options;
                    GlobalTestEnvironment::addTestData(data);

                    EXPECT_EQ(result, 0);
                }
            }
        }
    }
}

static void testDepthVariations()
{
    auto stl_files = generateStlFiles();
    auto texture_files = generateTextureFiles();

    std::filesystem::create_directory(unittestpath + "out\\");
    std::filesystem::create_directory(unittestpath + "out\\depth\\");

    // Select representative samples
    auto representative_stl = selectRepresentativeFiles(stl_files, 1);          // Pick 1 STLs
    auto representative_textures = selectRepresentativeFiles(texture_files, 1); // Pick 1 texture

    std::vector<TestConfig> test_configs = generateTestConfigs();

    auto n = 1;
    for (auto& stlpath : representative_stl) {
        for (auto& texturepath : representative_textures) {
            for (auto& config : test_configs) {

                for (float nearv = 0.65; nearv < 1.0; nearv += 0.2) {
                    for (float farv = 0; farv < .3; farv += 0.2) {

                        // Apply configuration and test

                        config.options.depth_near = nearv;
                        config.options.depth_far = farv;

                        config.options.stlpath = stlpath;
                        config.options.texpath = texturepath;
                        config.options.outprefix = unittestpath + "out\\depth\\" + GetTestName();
                        
                        auto options = std::make_shared<Options>(config.options);
                        StereogramGenerator st(options);
                        auto result = st.create();

                        // Log the test run data
                        TestRunData data;
                        data.imagePath = config.options.outprefix + "_sirds.png";
                        data.depthPath = config.options.outprefix + "_depth.png";
                        data.options = config.options;
                        GlobalTestEnvironment::addTestData(data);

                        EXPECT_EQ(result, 0);
                    }
                }
            }
        }
    }
}

static void testEyeSeparationVariations()
{
    auto stl_files = generateStlFiles();
    auto texture_files = generateTextureFiles();

    std::filesystem::create_directory(unittestpath + "out\\");
    std::filesystem::create_directory(unittestpath + "out\\eyesep\\");

    // Select representative samples
    auto representative_stl = selectRepresentativeFiles(stl_files, 1);          // Pick 1 STLs
    auto representative_textures = selectRepresentativeFiles(texture_files, 1); // Pick 1 texture

    std::vector<TestConfig> test_configs = generateTestConfigs();

    auto n = 1;
    for (auto& stlpath : representative_stl) {
        for (auto& texturepath : representative_textures) {
            for (auto& config : test_configs) {

                for (float eyesep = 30; eyesep < 200; eyesep += 5) {
                    // Apply configuration and test

                    config.options.eye_sep = eyesep;
                    config.options.stlpath = stlpath;
                    config.options.texpath = texturepath;
                    config.options.outprefix = unittestpath + "out\\eyesep\\" + GetTestName();


                    auto options = std::make_shared<Options>(config.options);
                    StereogramGenerator st(options);
                    auto result = st.create();

                    // Log the test run data
                    TestRunData data;
                    data.imagePath = config.options.outprefix + "_sirds.png";
                    data.depthPath = config.options.outprefix + "_depth.png";
                    data.options = config.options;
                    GlobalTestEnvironment::addTestData(data);
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

    std::filesystem::create_directory(unittestpath + "out\\");
    std::filesystem::create_directory(unittestpath + "out\\generateImage\\");

    config.options.stlpath = stl_file;
    config.options.texpath = texture_file;
    config.options.outprefix = unittestpath + "out\\generateImage\\" + GetTestName();

    auto options = std::make_shared<Options>(config.options);
    StereogramGenerator st(options);
    auto result = st.create();

    // Log the test run data
    TestRunData data;
    data.imagePath = config.options.outprefix + "_sirds.png";
    data.depthPath = config.options.outprefix + "_depth.png";
    data.options = config.options;
    GlobalTestEnvironment::addTestData(data);

    EXPECT_EQ(result, 0);
}

INSTANTIATE_TEST_SUITE_P(
    MagicEyeTests,
    StereogramTest,
    ::testing::Combine(
        ::testing::ValuesIn(generateStlFiles()),
        ::testing::ValuesIn(generateTextureFiles()),         // Key textures
        ::testing::ValuesIn(generateTestConfigs())          // Parameter variations
    )
);

TEST(stl_test, representative_tests)
{
    auto stl_files =  generateStlFiles();
    auto texture_files = generateTextureFiles();

    // Select representative samples
    auto representative_stl = selectRepresentativeFiles(stl_files, 3); // Pick 3 diverse STLs
    auto representative_textures = selectRepresentativeFiles(texture_files, 3); // Pick 3 textures
    auto test_configs = generateTestConfigs();

    std::filesystem::create_directory(unittestpath + "out\\");
    std::filesystem::create_directory(unittestpath + "out\\representative\\");

    for (auto& stlpath : representative_stl) {
        for (auto& texturepath : representative_textures) {
            for (auto& config : test_configs) {
                // Apply configuration and test
                config.options.stlpath = stlpath;
                config.options.texpath = texturepath;
                config.options.outprefix = unittestpath + "out\\representative\\" + GetTestName();

                auto options = std::make_shared<Options>(config.options);
                StereogramGenerator st(options);
                auto result = st.create();

                // Log the test run data
                TestRunData data;
                data.imagePath = config.options.outprefix + "_sirds.png";
                data.depthPath = config.options.outprefix + "_depth.png";
                data.options = config.options;
                GlobalTestEnvironment::addTestData(data);

                EXPECT_EQ(result, 0);
            }
        }
    }
}

TEST(stl_test, smoke_test)
{
    // Quick test to ensure basic functionality
    
    Options config;
    setupBasicOptions(config);

    std::filesystem::create_directory(unittestpath + "out\\");
    std::filesystem::create_directory(unittestpath + "out\\smoke\\");

    auto stl_files = generateStlFiles();
    auto texture_files = generateTextureFiles();

    auto representative_stl = selectRepresentativeFiles(stl_files, 2);          // Pick diverse STLs
    auto representative_textures = selectRepresentativeFiles(texture_files, 2); // Pick textures

    for (auto& stlpath : representative_stl) {
        for (auto& texturepath : representative_textures) {
            config.outprefix = unittestpath + "out\\smoke\\" + GetTestName();
            auto options = std::make_shared<Options>(config);
            options->stlpath = stlpath;
            options->texpath = texturepath;
            StereogramGenerator st(options);
            auto result = st.create();

            // Log the test run data
            TestRunData data;
            data.imagePath = options->outprefix + "_sirds.png";
            data.depthPath = options->outprefix + "_depth.png";
            data.options = config;
            GlobalTestEnvironment::addTestData(data);

            EXPECT_EQ(result, 0);
        }
    }

}

TEST(stl_test, focused_parameter_studies)
{
    // Test one parameter at a time while keeping others constant
    testFovVariations();
    testDepthVariations();
    testEyeSeparationVariations();
}
