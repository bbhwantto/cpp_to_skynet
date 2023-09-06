# 项目名称
project(sunnet)
# CMake最低版本号要求
cmake_minimum_required(VERSION 3.22)
#头文件目录
include_directories(include)
#查找./src目录下的所以源文件，保存到DIR_SRCS变量中
aux_source_directory(./src DIR_SRCS)
#添加库文件路径
link_directories(${cmake_CURRENT_SOURCE_DIR}/3rd/lua-5.3.5/src/)
# 用C++11
add_definitions(-std=c++11)
#指定生成目标文件
add_executable(sunnet ${DIR_SRCS})
# 库文件
find_package(Theads)
target_link_libraries(sunnet ${cmake_THREAD_LIBS_INIT})
#Lua头文件、库
include_directories(./3rd/lua-5.3.5/src)
target_link_libraries(sunnet liblua.a)
#target_link_libraries(sunnet dl)
