include_directories("${PROJECT_SOURCE_DIR}/rawmedia")
add_executable(transcode transcode.c)
target_link_libraries(transcode rawmedia)
