cmake_minimum_required(VERSION 3.21)
project(libevent_demo C)

set(CMAKE_C_STANDARD 11)

include_directories(/usr/local/include/)
link_directories(/usr/local/lib)

add_executable(main main.c)
target_link_libraries(main event)

add_executable(server server.c)
target_link_libraries(server event)

add_executable(client client.c)
target_link_libraries(client event)


# buffer在server的基础上添加了buffer, 使用bev替代了ev
add_executable(server_buffer server_buffer.c)
target_link_libraries(server_buffer event)

add_executable(client_buffer client_buffer.c)
target_link_libraries(server_buffer event)

# server2在buffer的基础上简化了listener的构造，不用自己构造socket
add_executable(server2 server2.c)
target_link_libraries(server2 event)

add_executable(client2 client2.c)
target_link_libraries(client2 event)

add_executable(http_server http_server.c)
target_link_libraries(http_server event)

add_executable(http_client http_client.c)
target_link_libraries(http_client event)