cmake_minimum_required(VERSION 3.20)

project(SaiyanInTheOldWest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

add_library(dbr_core STATIC
    src/core/ki_meter.cpp
)
target_include_directories(dbr_core PUBLIC src)

if(BUILD_TESTING)
    add_executable(dbr_core_tests
        tests/ki_meter_tests.cpp
    )
    target_link_libraries(dbr_core_tests PRIVATE dbr_core)
    add_test(NAME dbr_core_tests COMMAND dbr_core_tests)
endif()

if(WIN32)
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "SaiyanInTheOldWest.asi requires a 64-bit build.")
    endif()

    add_library(saiyan_in_the_old_west SHARED
        src/plugin/asi_entry.cpp
        src/plugin/script_hook_api.cpp
    )
    target_include_directories(saiyan_in_the_old_west PRIVATE src)
    target_link_libraries(saiyan_in_the_old_west PRIVATE dbr_core user32)
    if(MINGW)
        target_link_options(saiyan_in_the_old_west PRIVATE -static-libgcc -static-libstdc++)
    endif()
    set_target_properties(saiyan_in_the_old_west PROPERTIES
        OUTPUT_NAME "SaiyanInTheOldWest"
        PREFIX ""
        SUFFIX ".asi"
    )

    add_custom_command(TARGET saiyan_in_the_old_west POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/config/SaiyanInTheOldWest.ini"
            "$<TARGET_FILE_DIR:saiyan_in_the_old_west>/SaiyanInTheOldWest.ini"
    )
endif()