cmake_minimum_required(VERSION 3.5)

include(MPack.cmake)
include(UTHash.cmake)

# Set the project name
project(krossbar)

# Find jemalloc package
find_package(PkgConfig REQUIRED)

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

pkg_search_module(URING REQUIRED liburing)
pkg_search_module(LOG4C REQUIRED log4c)

if(NOT CMAKE_USE_PTHREADS_INIT)
    message(FATAL_ERROR "Pthreads not found")
endif()

set(SOURCES
    src/message_writer.c
    src/message.c
    src/peer.c
    src/rpc.c
    src/uds/transport_uds.c
    src/uds/message_uds.c
    src/uds/message_writer_uds.c
    src/uds/event_manager_uds.c
)

set(IO_URING_FUTEXES YES CACHE BOOL "If target platform supports futexes for io_uring")

if(IO_URING_FUTEXES)
    list(APPEND SOURCES
        src/shmem/allocator.c
        src/shmem/transport_shm.c
        src/shmem/message_shm.c
        src/shmem/message_writer_shm.c
        src/shmem/event_manager_shm.c
    )

    add_definitions(-DIO_URING_FUTEXES)
endif()

# Add the executable
add_library(${PROJECT_NAME} ${SOURCES})

set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 11)

target_compile_definitions(${PROJECT_NAME} PRIVATE _GNU_SOURCE)

if(MESSAGE_BUFFER_SIZE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE MESSAGE_BUFFER_SIZE=${MESSAGE_BUFFER_SIZE})
endif()

if(MAX_MESSAGE_SIZE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE MAX_MESSAGE_SIZE=${MAX_MESSAGE_SIZE})
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC
    mpack
    Threads::Threads
    ${URING_LIBRARIES}
    ${LOG4C_LIBRARIES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    "include/"
    ${URING_INCLUDE_DIRS}
    ${UTHASH_INCLUDE_DIRS}
    ${LOG4C_INCLUDE_DIRS}
)

# Add a custom target to run the executable
add_custom_target(run
    COMMAND ${PROJECT_NAME}
    DEPENDS ${PROJECT_NAME}
    WORKING_DIRECTORY ${CMAKE_PROJECT_DIR}
)

if(ENABLE_TESTING)
    include(CTest)
    add_subdirectory(test)
endif()

if(ENABLE_PERFORMANCE_TESTING)
    add_subdirectory(performance)
endif()