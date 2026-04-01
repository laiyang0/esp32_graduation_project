cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_graduation_project)

# 在项目的CMakeLists.txt中添加
set(ENV{DASHSCOPE_API_KEY} "sk")
set(ENV{DASHSCOPE_MODEL} "qwen3-omni-flash-realtime")
# 2. 添加main组件
set(COMPONENTS main)
