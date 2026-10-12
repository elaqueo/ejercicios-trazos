# audio: el audio de referencia de la toma (HU-106, Mesa de animación). Media Foundation, sin Qt.
add_library(audio STATIC
    include/audio/Audio.h
    src/Audio.cpp
    include/audio/Player.h
    src/Player.cpp
)
target_include_directories(audio PUBLIC include)
target_link_libraries(audio PUBLIC ole32 mfplat mfreadwrite mfuuid)
et_target_defaults(audio)
set_target_properties(audio PROPERTIES AUTOMOC OFF)

add_subdirectory(tests)
