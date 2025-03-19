#include <iostream>

#include "transport_perf_test.h"

int main()
{
    auto test_cases = std::vector<std::pair<size_t, size_t>>
    {
        {10, 1000},
        {1000, 1000},
        {100000, 100},
        {1000000, 10}
    };

    for (auto &test_case : test_cases)
    {
        TransportPerfTestRunner runner {test_case.first, test_case.second, TransportPerfTestRunner::TransportType::SHMEM};
        auto duration = runner.run();

        std::cout << "UDS: " << test_case.first << " bytes, " << test_case.second << " messages: " << duration.count() << "us" << std::endl;
        std::cout << "~" << 1000000 / duration.count() * test_case.second << " messages/s" << std::endl;
    }

    return 0;
}