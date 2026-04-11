cmake_minimum_required(VERSION 3.16)
project(FightGameOpenWorld LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(FightGameOpenWorld main.cpp)

if(WIN32)
    set(ALLEGRO_ROOT "C:/c++/drive-download-20250522T121114Z-1-001/AllegroLib_5.2.8" CACHE PATH "Root folder of the Allegro 5 SDK")
    set(ALLEGRO_INCLUDE_DIR "${ALLEGRO_ROOT}/include")
    set(ALLEGRO_LIBRARY_DIR "${ALLEGRO_ROOT}/lib")
    set(ALLEGRO_BINARY_DIR "${ALLEGRO_ROOT}/bin")

    foreach(required_dir IN ITEMS ALLEGRO_INCLUDE_DIR ALLEGRO_LIBRARY_DIR ALLEGRO_BINARY_DIR)
        if(NOT EXISTS "${${required_dir}}")
            message(FATAL_ERROR "Allegro path not found: ${${required_dir}}. Set ALLEGRO_ROOT to your Allegro 5 SDK folder.")
        endif()
    endforeach()

    target_include_directories(FightGameOpenWorld PRIVATE "${ALLEGRO_INCLUDE_DIR}")
    target_link_directories(FightGameOpenWorld PRIVATE "${ALLEGRO_LIBRARY_DIR}")
    target_link_libraries(FightGameOpenWorld PRIVATE
        allegro
        allegro_main
        allegro_audio
        allegro_acodec
        allegro_primitives
        allegro_font
        allegro_ttf
    )

    foreach(dll_name IN ITEMS
        allegro-5.2.dll
        allegro_font-5.2.dll
        allegro_main-5.2.dll
        allegro_audio-5.2.dll
        allegro_acodec-5.2.dll
        allegro_primitives-5.2.dll
        allegro_ttf-5.2.dll
    )
        add_custom_command(TARGET FightGameOpenWorld POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ALLEGRO_BINARY_DIR}/${dll_name}"
            "$<TARGET_FILE_DIR:FightGameOpenWorld>/${dll_name}"
        )
    endforeach()
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ALLEGRO REQUIRED IMPORTED_TARGET
        allegro-5
        allegro_main-5
        allegro_audio-5
        allegro_acodec-5
        allegro_primitives-5
        allegro_font-5
        allegro_ttf-5
    )
    target_link_libraries(FightGameOpenWorld PRIVATE PkgConfig::ALLEGRO)
endif()



