#include "gtest/gtest.h"

#include <log4c.h>

int main(int argc, char **argv) {
    log4c_init();
    auto logger = log4c_category_get("libkrossbar.test");
    log4c_category_set_priority(logger, LOG4C_PRIORITY_TRACE);
    log4c_category_set_appender(logger, log4c_appender_get("stdout"));

    ::testing::InitGoogleTest(&argc, argv);
    auto result = RUN_ALL_TESTS();

    log4c_fini();
    return result;
}