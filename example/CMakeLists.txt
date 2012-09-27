include_directories("${PROJECT_SOURCE_DIR}/rawmedia")
link_directories(${FFMPEG_LIBRARY_DIRS})
add_executable(transcode transcode.c)
target_link_libraries(transcode rawmedia)
