#include "gtest/gtest.h"

#include <log4c.h>

int main(int argc, char **argv) {
    log4c_init();

    ::testing::InitGoogleTest(&argc, argv);
    auto result = RUN_ALL_TESTS();

    log4c_fini();
    return result;
}