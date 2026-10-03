muse_create_module(midirecordingscene)

target_sources(midirecordingscene PRIVATE
    midirecordingscenemodule.cpp
    midirecordingscenemodule.h
    imidirecordingconfiguration.h

    internal/midirecordingconfiguration.cpp
    internal/midirecordingconfiguration.h
)

target_link_libraries(midirecordingscene PRIVATE midirecording)
