cmake_minimum_required(VERSION 3.21)

# LoS2Control.dll for Skyrim VR and 1.6.1130 and older: CharmedBaryon's CommonLibSSE-NG through vcpkg, the same sources as the SE/AE build in ../ng.cmake.
project(LoS2Control VERSION 0.2.0 LANGUAGES CXX)

find_package(CommonLibSSE CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_path(LOS2C_SIMPLEINI_INCLUDE_DIRS "SimpleIni.h")

add_commonlibsse_plugin(${PROJECT_NAME} NAME "LoS2Control" AUTHOR "CageTV" SOURCES
    ../src/main.cpp
    ../src/Settings.cpp
    ../src/Schedule.cpp
    ../src/Menu.cpp
    ../src/Editor.cpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_include_directories(${PROJECT_NAME} PRIVATE ../src "${LOS2C_SIMPLEINI_INCLUDE_DIRS}")
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)
target_precompile_headers(${PROJECT_NAME} PRIVATE ../src/PCH.h)
target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:/Z7>)
target_link_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:/DEBUG /OPT:REF /OPT:ICF>)
