muse_create_module(midirecording)

target_sources(midirecording PRIVATE
    internal/recordingtypes.h
    internal/clockmap.cpp
    internal/clockmap.h
    internal/notepairing.cpp
    internal/notepairing.h
    internal/prepare.cpp
    internal/prepare.h
    internal/gridselection.cpp
    internal/gridselection.h
)

if (MUE_BUILD_MIDIRECORDING_TESTS)
    add_subdirectory(tests)
endif()
