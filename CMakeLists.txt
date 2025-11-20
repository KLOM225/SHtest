# ============================================================================
# 现代化CMake配置 - SplitPanel
# ============================================================================

cmake_minimum_required(VERSION 3.21)

# 项目定义
project(SplitPanel 
    VERSION 1.0.0
    DESCRIPTION "Qt6 QML Docking System Standalone Application"
    LANGUAGES CXX
)

# ============================================================================
# 编译器和语言标准设置
# ============================================================================

# 使用C++17标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 导出编译命令用于IDE和工具
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================================
# Qt6配置
# ============================================================================

# Qt自动化工具
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)

# 查找Qt6包
find_package(Qt6 6.2 REQUIRED COMPONENTS
    Core
    Quick
    Qml
)

qt_policy(SET QTP0001 NEW) 

# 使Qt CMake命令可用
qt_standard_project_setup()

# ============================================================================
# 源文件组织
# ============================================================================

set(PROJECT_SOURCES 
    src/main.cpp
    src/utils/Logger.cpp
    src/utils/Logger.hpp
    src/models/SplitPanelNode.cpp
    src/models/SplitPanelNode.hpp
    src/models/SplitManager.cpp
    src/models/SplitManager.hpp
    src/models/SplitTreeModel.cpp
    src/models/SplitTreeModel.hpp
)

qt_add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

qt_add_qml_module(${PROJECT_NAME}
    URI SplitPanel
    VERSION 1.0
    QML_FILES
        SplitPanel/Main.qml
        SplitPanel/SplitContainerView.qml
        SplitPanel/SplitPanelContent.qml
        SplitPanel/SplitSystemView.qml
        SplitPanel/SplitNodeRenderer.qml
        SplitPanel/SplitPanelView.qml
   )

# ============================================================================
# 目标属性配置
# ============================================================================

# 设置目标属性
set_target_properties(${PROJECT_NAME} PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
    
    # 输出目录
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
    
    # 版本信息
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# ============================================================================
# 编译选项
# ============================================================================

# 目标编译特性
target_compile_features(${PROJECT_NAME} PRIVATE
    cxx_std_17
)

# 编译器警告选项
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4                 # 警告级别4
        /utf-8              # UTF-8源文件编码
    )
else()
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall               # 所有警告
        -Wextra             # 额外警告
        -pedantic           # 严格标准遵循
    )
endif()

# ============================================================================
# 包含目录
# ============================================================================

# 使用现代CMake的target_include_directories
target_include_directories(${PROJECT_NAME} PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/utils>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/models>
)

# ============================================================================
# 链接库
# ============================================================================

# 链接Qt6库（使用PRIVATE避免传递依赖）
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::Qml
)


# ============================================================================
# 安装规则
# ============================================================================

# include(GNUInstallDirs)

# install(TARGETS ${PROJECT_NAME}
#     BUNDLE DESTINATION .
#     RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
#         COMPONENT Runtime
# )

# ============================================================================
# 构建信息输出
# ============================================================================

message(STATUS "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
message(STATUS "  ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
message(STATUS "  CMake Version:    ${CMAKE_VERSION}")
message(STATUS "  Qt Version:       ${Qt6_VERSION}")
message(STATUS "  C++ Standard:     C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler:         ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Build Type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install Prefix:   ${CMAKE_INSTALL_PREFIX}")
message(STATUS "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
