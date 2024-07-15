cmake_minimum_required(VERSION 3.27)
project(FlatOut2DBMaker)

SET(CMAKE_SYSTEM_NAME Windows)

SET(CMAKE_C_COMPILER i686-w64-mingw32-gcc)
SET(CMAKE_CXX_COMPILER i686-w64-mingw32-g++)
SET(CMAKE_RC_COMPILER i686-w64-mingw32-windres)
SET(CMAKE_RANLIB i686-w64-mingw32-ranlib)

SET(CMAKE_CXX_STANDARD 20)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static -s")

add_executable(FlatOut2DBMaker main.cpp)
set_target_properties(FlatOut2DBMaker PROPERTIES SUFFIX "_gcp.exe")