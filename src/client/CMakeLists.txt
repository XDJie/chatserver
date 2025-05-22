# 包含目录下所有的源文件
aux_source_directory(. SRC_LIST)

# 执行生成可执行文件
add_executable(ChatClient ${SRC_LIST})

# 添加可执行文件需要的依赖库
target_link_libraries(ChatClient pthread)