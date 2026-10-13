# cast (HU-145): mostrar una imagen en un TV de la red por DLNA/UPnP (el protocolo de pdfcast).
# Solo Qt Core y Network; la usa Bocetos y puede usarla otra app de la suite.
qt_add_library(cast STATIC
    include/cast/AvTransport.h
    include/cast/Discovery.h
    include/cast/Dlna.h
    include/cast/MediaServer.h
    src/AvTransport.cpp
    src/Discovery.cpp
    src/Dlna.cpp
    src/MediaServer.cpp
)
target_include_directories(cast PUBLIC include)
target_link_libraries(cast PUBLIC Qt6::Network)
et_target_defaults(cast)

add_subdirectory(tests)
