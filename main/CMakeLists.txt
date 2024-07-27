idf_component_register(SRCS "main.cpp" "RTC/ds1302z.cpp" "UI/clock.cpp" "Audio/Speaker/spkr.cpp" "RGB/rgb.cpp" "WIFI/wifi.cpp"
                    INCLUDE_DIRS "." "include")
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++11)
set(CMAKE_C_FLAGS "-Wno-misleading-indentation -mlongcalls")
set(CMAKE_CXX_FLAGS "-Wno-misleading-indentation  -mlongcalls")