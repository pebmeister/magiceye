// written by Paul Baxter

#define GLM_ENABLE_EXPERIMENTAL

#include <iostream>
#include <exception>

#define STL_CLI 1

#include "Options.h"
#include "ParseArgs.h"
#include "StereogramGenerator.h"

int main(int argc, char** argv)
{
    try {
        auto options = std::make_shared<Options>();
        if (!ParseArgs::parseArgs(argc, argv, options))
            return 1;

        StereogramGenerator st(options);
        return st.create();
    }
    catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
}
