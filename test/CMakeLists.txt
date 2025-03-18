set(BINARY "${PROJECT_NAME}_test")
set(CMAKE_CXX_STANDARD 23)

pkg_check_modules(GTEST REQUIRED gtest)
pkg_check_modules(GTEST REQUIRED gtest_main)

file(GLOB_RECURSE TEST_SOURCES LIST_DIRECTORIES false *.h *.cpp)
add_executable(${BINARY}
    ${TEST_SOURCES}
    "mocks/transport_mock.cpp")

add_dependencies(${BINARY} ${PROJECT_NAME})

target_link_libraries(${BINARY} PRIVATE
    ${PROJECT_NAME}
    ${GTEST_LIBRARIES}
)

target_link_directories(${BINARY} PRIVATE
    ${CMAKE_BINARY_DIR}
    ${GTEST_LIBRARY_DIRS})

target_include_directories(${BINARY} PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/src"
    ${GTEST_INCLUDE_DIRS}
    ${UTHASH_INCLUDE_DIRS}
)

include(GoogleTest)
gtest_discover_tests(${BINARY})

add_custom_target(memcheck
    COMMAND valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes $<TARGET_FILE:${BINARY}>
    DEPENDS ${BINARY}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
