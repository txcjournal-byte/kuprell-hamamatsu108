cmake_minimum_required(VERSION 3.22)

project(KuprellHamamatsu108 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# -----------------------------------------------------------------------------
#  JUCE: use a local checkout (-DJUCE_DIR=/path/to/JUCE) or download it
# -----------------------------------------------------------------------------
set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")

if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# Shared Kuprell modules (frame, look and feel, brand assets) used by every plugin
add_subdirectory(kuprell-core)

# Plugins
add_subdirectory(plugins/hamamatsu108)
