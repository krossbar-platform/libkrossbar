#include <iostream>

#include <log4c.h>

#include "transport_perf_test.h"

int main()
{
    log4c_init();
    auto logger = log4c_category_get("libkrossbar.test");
    log4c_category_set_priority(logger, LOG4C_PRIORITY_WARN);
    log4c_category_set_appender(logger, log4c_appender_get("stdout"));

    auto shmem_test_cases = std::vector<std::pair<size_t, size_t>>{
        {10, 100000},
        {1000, 100000},
        {100000, 10000},
        {1000000, 2000}};

    for (auto &test_case : shmem_test_cases)
    {
        TransportPerfTestRunner runner{test_case.first, test_case.second, TransportPerfTestRunner::TransportType::SHMEM, logger};
        auto duration = runner.run();

        std::cout << "Shared memory transport: " << test_case.first << " bytes, " << test_case.second << " messages: " << duration.count() << "us" << std::endl;
        std::cout << "~" << int(1000000.f / duration.count() * test_case.second) << " messages/s" << std::endl;
    }

    return 0;
}