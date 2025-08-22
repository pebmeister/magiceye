// Written by Paul Baxter
#include <gtest/gtest.h>
#include <string>

#include "Options.h"
#include "StereogramGenerator.h"

#pragma warning(disable:4996)

namespace magiceye_unit_test
{
    TEST(magiceye_unit_test, cat_test)
    {
        auto options = std::make_shared<Options>();

        options->stlpath = "..\\..\\..\\unittests\\stl\\cat.stl";
        options->texpath = "..\\..\\..\\unittests\\texture\\brick.jpg";
        options->outprefix = "..\\..\\..\\unittests\\out\\cat";
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
