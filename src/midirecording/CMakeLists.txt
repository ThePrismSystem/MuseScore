muse_create_module(midirecording)

target_sources(midirecording PRIVATE
    internal/recordingtypes.h
    internal/clockmap.cpp
    internal/clockmap.h
    internal/rawevents.cpp
    internal/rawevents.h
    internal/timemap.cpp
    internal/timemap.h
    internal/notatedtext.cpp
    internal/notatedtext.h
    internal/notepairing.cpp
    internal/notepairing.h
    internal/prepare.cpp
    internal/prepare.h
    internal/gridselection.cpp
    internal/gridselection.h
    internal/snap.cpp
    internal/snap.h
    internal/voicebuilder.cpp
    internal/voicebuilder.h
    internal/quantizer.cpp
    internal/quantizer.h
    internal/takefile.cpp
    internal/takefile.h
    internal/takepipeline.cpp
    internal/takepipeline.h
    internal/takesetup.cpp
    internal/takesetup.h
    internal/takerecorder.cpp
    internal/takerecorder.h
    internal/takewriter.cpp
    internal/takewriter.h
    internal/calibration.cpp
    internal/calibration.h
)

target_link_libraries(midirecording PRIVATE engraving)

if (MUE_BUILD_MIDIRECORDING_TESTS)
    add_subdirectory(tests)
endif()
