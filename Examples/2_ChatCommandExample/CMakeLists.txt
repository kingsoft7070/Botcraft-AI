project(2_ChatCommandExample)

set(${PROJECT_NAME}_SOURCE_FILES
    ${PROJECT_SOURCE_DIR}/include/ChatCommandClient.hpp

    ${PROJECT_SOURCE_DIR}/src/ChatCommandClient.cpp
    ${PROJECT_SOURCE_DIR}/src/main.cpp
)
set(${PROJECT_NAME}_INCLUDE_FOLDERS
    ${PROJECT_SOURCE_DIR}/include
)

add_example("${${PROJECT_NAME}_INCLUDE_FOLDERS}" "${${PROJECT_NAME}_SOURCE_FILES}")

# 原有内容
add_executable(2_ChatCommandExample
    ChatCommandClient.cpp
    getopt.h  # 新增：添加getopt.h
    ${CMAKE_SOURCE_DIR}/botcraft/src/AI/OpenAIApi.cpp  # 新增：OpenAI源码
)

# 原有链接逻辑不变，确保链接vcpkg依赖
target_link_libraries(2_ChatCommandExample PRIVATE botcraft)
target_include_directories(2_ChatCommandExample PRIVATE
    ${CMAKE_SOURCE_DIR}/botcraft/include
    ${CMAKE_SOURCE_DIR}/examples/2_ChatCommandExample  # 新增：包含getopt.h路径
)
