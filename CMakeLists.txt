cmake_minimum_required(VERSION 3.14)
project(shellext LANGUAGES CXX)

add_library(shellext SHARED)

target_sources(shellext
    PRIVATE
        common.h
        shellext.cpp
        shellext.rc
        sidecar.cpp
        sidecar.def
)

target_compile_features(shellext
    PRIVATE
        cxx_std_17
)

target_compile_definitions(shellext
    PRIVATE
        WIN32
        _WINDOWS
        _USRDLL
        UNICODE
        _UNICODE
        "$<$<CONFIG:Debug>:_DEBUG>"
        "$<$<CONFIG:Release>:NDEBUG>"
)

target_link_libraries(shellext
    PRIVATE
        Gdiplus
)

target_compile_options(shellext
    PRIVATE
        /W3
)

