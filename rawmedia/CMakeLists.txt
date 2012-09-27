find_package(PkgConfig)
pkg_check_modules(FFMPEG REQUIRED
  libavcodec
  libavformat
  libavutil
  libavfilter
)
include_directories(${FFMPEG_INCLUDE_DIRS})
link_directories(${FFMPEG_LIBRARY_DIRS})

add_library(rawmedia SHARED
  decoder.c
  encoder.c
  packet_queue.c
  rawmedia.c
)

target_link_libraries(rawmedia ${FFMPEG_LIBRARIES})
target_link_libraries(rawmedia LINK_INTERFACE_LIBRARIES "")

install(FILES rawmedia.h DESTINATION include COMPONENT headers)
install(TARGETS rawmedia
  RUNTIME DESTINATION bin COMPONENT libraries
  LIBRARY DESTINATION lib COMPONENT libraries
  ARCHIVE DESTINATION lib COMPONENT libraries
)
