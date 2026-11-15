# kuprell-core: shared code for all Kuprell plugins.
# Sources are compiled into each plugin target (INTERFACE library), so they see
# the same JUCE modules and settings as the plugin that uses them.

juce_add_binary_data(KuprellBrandAssets
    HEADER_NAME KuprellBrandAssets.h
    NAMESPACE   KuprellBrand
    SOURCES
        assets/brand/kuprell-logo-cream.svg
        assets/brand/kuprell-logo-ink.svg)

add_library(kuprell_core INTERFACE)

target_sources(kuprell_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/source/KuprellLookAndFeel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/KuprellTopBar.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/source/KuprellKnob.cpp)

target_include_directories(kuprell_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/source)

target_link_libraries(kuprell_core INTERFACE KuprellBrandAssets)

# Standalone app icon shared by all Kuprell plugins
set(KUPRELL_APP_ICON ${CMAKE_CURRENT_SOURCE_DIR}/assets/brand/kuprell-icon-dark-1024.png CACHE INTERNAL "")
