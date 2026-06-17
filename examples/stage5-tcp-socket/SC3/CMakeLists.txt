cmake_minimum_required(VERSION 3.20.1)
project(SC1)

add_executable(client echo_client.c ../Utils/SC_Utils.c)
add_executable(server echo_server.c ../Utils/SC_Utils.c)
