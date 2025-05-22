# cmake_minimum_required(VERSION 3.0)
# project(main)

# #配置编译选项
# set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -g)

#配置头文件搜索路径
# include_directories()
# 配置库文件搜索路径
# link_directories()

# 设置需要编译的源文件列表
set(SRC_LIST muduo_server.cpp)

# 设置可执行文件存储路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# 生成可执行文件server
add_executable(server ${SRC_LIST})
# add_executable(server muduo_server.cpp)

# 生成库文件
# add_library()

# 为server链接对应的库
target_link_libraries(server muduo_net muduo_base pthread)