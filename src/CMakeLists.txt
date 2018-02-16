add_custom_target(tlochat_server_headers SOURCES client_handlers.h)

add_executable(tlochat_server client_handlers.c tlochat_server.c)
target_include_directories(tlochat_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tlochat_server PRIVATE tloc tlocposix)
