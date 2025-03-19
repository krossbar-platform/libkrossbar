set(BINARY "${PROJECT_NAME}_performance_test")
set(CMAKE_CXX_STANDARD 23)

add_executable(${BINARY}
    "main.cpp"
    "transport_perf_test.cpp"
    "rpc_perf_test.cpp")

add_dependencies(${BINARY} ${PROJECT_NAME})

target_link_libraries(${BINARY} PRIVATE
    ${PROJECT_NAME}
)

target_link_directories(${BINARY} PRIVATE
    ${CMAKE_BINARY_DIR})

target_include_directories(${BINARY} PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/src"
    ${UTHASH_INCLUDE_DIRS}
)

set(PERF_TEST_TARGET "perf_test")

add_custom_target(${PERF_TEST_TARGET}
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${BINARY}
    COMMAND ${CMAKE_BINARY_DIR}/performance/${BINARY}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/performance/
    COMMENT "Building and running performance tests..."
)

add_dependencies(${PERF_TEST_TARGET} ${BINARY})
