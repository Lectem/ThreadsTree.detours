project(ThreadsTree.detours)

set(ThreadsTree_detours_SOURCES
    # .def file for the dll exports
    src/ThreadsTree.detours.def
    # The patches
    src/ThreadsTree.detours.cpp
)

set(ThreadsTree_detours_HEADERS
    include/Log.h
)

add_library(ThreadsTree.detours 
    SHARED # We want to create a detours dll
    ${ThreadsTree_detours_SOURCES} ${ThreadsTree_detours_HEADERS}
)
target_include_directories(ThreadsTree.detours PRIVATE include)

target_link_libraries(ThreadsTree.detours
    PRIVATE
        fmt::fmt
        Shlwapi.lib
		DbgHelp.lib
        Detours::Detours
)

target_compile_definitions(ThreadsTree.detours
    PRIVATE
        # For faster build
        -DWIN32_LEAN_AND_MEAN
        -DNOCOMM
        # To remove annoying stuff
        -DNOMINMAX
		-D_CRT_SECURE_NO_WARNINGS
)




