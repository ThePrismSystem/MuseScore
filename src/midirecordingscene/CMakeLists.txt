muse_create_module(midirecordingscene)

target_sources(midirecordingscene PRIVATE
    midirecordingscenemodule.cpp
    midirecordingscenemodule.h
    imidirecordingconfiguration.h
    imidirecordingmenu.h

    internal/midirecordingconfiguration.cpp
    internal/midirecordingconfiguration.h
    internal/midirecordingcontroller.cpp
    internal/midirecordingcontroller.h
    internal/midirecordingmenu.cpp
    internal/midirecordingmenu.h
    internal/midirecordinguiactions.cpp
    internal/midirecordinguiactions.h
)

target_link_libraries(midirecordingscene PRIVATE midirecording)
