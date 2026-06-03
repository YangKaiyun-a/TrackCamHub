cmake_minimum_required(VERSION 3.20)

project(TrackCamHub VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TRACKCAMHUB_ENABLE_THRIFT "Build Apache Thrift integration" ON)
set(TRACKCAMHUB_THRIFT_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/thrift" CACHE PATH "Vendored Apache Thrift include directory")
set(TRACKCAMHUB_RUNTIME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/native-runtime" CACHE PATH "Vendored native runtime libraries directory")

if (NOT EXISTS "${TRACKCAMHUB_RUNTIME_DIR}" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/native-runtime")
    set(TRACKCAMHUB_RUNTIME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/native-runtime" CACHE PATH "Vendored native runtime libraries directory" FORCE)
endif()

if (TRACKCAMHUB_ENABLE_THRIFT AND WIN32 AND NOT MSVC)
    message(FATAL_ERROR
        "The vendored Thrift runtime libraries are MSVC binaries. "
        "Configure TrackCamHub with an MSVC toolchain, or set TRACKCAMHUB_ENABLE_THRIFT=OFF for non-Thrift compilation.")
endif()

add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

file(GLOB_RECURSE TRACKCAMHUB_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

if (NOT TRACKCAMHUB_ENABLE_THRIFT)
    list(FILTER TRACKCAMHUB_SOURCES EXCLUDE REGEX ".*/src/thrift/.*")
endif()

add_executable(TrackCamHub
    main.cpp
    ${TRACKCAMHUB_SOURCES}
)

target_include_directories(TrackCamHub PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/thrift/gen-cpp
)

if (WIN32)
    target_compile_definitions(TrackCamHub PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(TrackCamHub PRIVATE ws2_32)
endif()

if (TRACKCAMHUB_ENABLE_THRIFT)
    if (NOT EXISTS "${TRACKCAMHUB_THRIFT_INCLUDE_DIR}/thrift/Thrift.h")
        message(FATAL_ERROR "Cannot find vendored Thrift headers: ${TRACKCAMHUB_THRIFT_INCLUDE_DIR}")
    endif()

    if (NOT EXISTS "${TRACKCAMHUB_RUNTIME_DIR}/lib/x64/Debug/thriftmdd.lib")
        message(FATAL_ERROR "Cannot find vendored Debug runtime libraries: ${TRACKCAMHUB_RUNTIME_DIR}/lib/x64/Debug")
    endif()

    target_compile_definitions(TrackCamHub PRIVATE TRACKCAMHUB_ENABLE_THRIFT=1)
    target_include_directories(TrackCamHub PRIVATE
        ${TRACKCAMHUB_THRIFT_INCLUDE_DIR}
    )

    target_link_directories(TrackCamHub PRIVATE
        $<$<CONFIG:Debug>:${TRACKCAMHUB_RUNTIME_DIR}/lib/x64/Debug>
        $<$<CONFIG:Release>:${TRACKCAMHUB_RUNTIME_DIR}/lib/x64/Release>
        $<$<CONFIG:RelWithDebInfo>:${TRACKCAMHUB_RUNTIME_DIR}/lib/x64/Release>
        $<$<CONFIG:MinSizeRel>:${TRACKCAMHUB_RUNTIME_DIR}/lib/x64/Release>
    )

    target_link_libraries(TrackCamHub PRIVATE
        $<$<CONFIG:Debug>:event_cored event_extrad eventd thriftmdd thriftnbmdd thriftzmdd zlibd>
        $<$<NOT:$<CONFIG:Debug>>:event_core event_extra event thriftmd thriftnbmd thriftzmd zlib>
    )

    add_custom_command(TARGET TrackCamHub POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "$<IF:$<CONFIG:Debug>,${TRACKCAMHUB_RUNTIME_DIR}/bin/x64/Debug,${TRACKCAMHUB_RUNTIME_DIR}/bin/x64/Release>"
            "$<TARGET_FILE_DIR:TrackCamHub>"
        VERBATIM
    )
endif()
