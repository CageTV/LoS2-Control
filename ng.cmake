# LoS2Control.dll on alandtse's CommonLibSSE-NG (Skyrim 1.5.97 to 1.6.1170 and 1.7.x, SE and AE); included by E:/WorkSpace/ng-build/CMakeLists.txt (see ng_plugin there).
find_path(LOS2C_SIMPLEINI_INCLUDE_DIRS "SimpleIni.h")
ng_plugin(TARGET LoS2Control NAME LoS2Control VERSION 0.1.0 AUTHOR "CageTV" ROOT "${CMAKE_CURRENT_LIST_DIR}"
    SOURCES src/main.cpp src/Settings.cpp src/Schedule.cpp src/Menu.cpp src/Editor.cpp
    INCLUDES "${CMAKE_CURRENT_LIST_DIR}/src" "${LOS2C_SIMPLEINI_INCLUDE_DIRS}"
    PCH "${CMAKE_CURRENT_LIST_DIR}/src/PCH.h"
    LIBS nlohmann_json::nlohmann_json)
