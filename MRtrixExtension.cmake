set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(MRTRIX_SUBPROJECT_INSTALL_PREFIX "mrtrix3"
    CACHE STRING "Install prefix for mrtrix3 subproject"
)
set(MRTRIX_BUILD_GUI OFF CACHE BOOL "Build MRtrix3 GUI")

add_subdirectory(mrtrix3)
add_library(mrtrix3 INTERFACE)
target_link_libraries(mrtrix3 INTERFACE
    mrtrix::core
    mrtrix::headless
    mrtrix::exec-version-lib
)

if(APPLE)
    set(rpath_base @loader_path)
else()
    set(rpath_base $ORIGIN)
endif()

file(GLOB_RECURSE CPP_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/*.cpp" )
file(GLOB_RECURSE CPP_COMMANDS CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/cmd/*.cpp")
file(GLOB_RECURSE PYTHON_COMMANDS CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/bin/*")

if(CPP_SOURCES)
    add_library(${PROJECT_NAME}_lib STATIC ${CPP_SOURCES})
    target_link_libraries(${PROJECT_NAME}_lib PRIVATE mrtrix3)
    target_include_directories(${PROJECT_NAME}_lib PRIVATE ${PROJECT_SOURCE_DIR}/src)
endif()

foreach(cmd_file ${CPP_COMMANDS})
    get_filename_component(cmd_name ${cmd_file} NAME_WE)
    add_executable(${cmd_name} ${cmd_file})
    target_link_libraries(${cmd_name} PRIVATE mrtrix3)
    if(${PROJECT_NAME}_lib)
        target_link_libraries(${cmd_name} PRIVATE ${PROJECT_NAME}_lib)
    endif()
    target_include_directories(${cmd_name} PRIVATE ${PROJECT_SOURCE_DIR}/src)
    set_target_properties(${cmd_name} PROPERTIES
        INSTALL_RPATH "${rpath_base}/../mrtrix3/lib"
        RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
    )
    install(TARGETS ${cmd_name}
        RUNTIME DESTINATION bin
    )
endforeach()

foreach(cmd_file ${PYTHON_COMMANDS})
    file(COPY ${cmd_file} DESTINATION ${PROJECT_BINARY_DIR}/bin)
    install(FILES ${cmd_file} DESTINATION bin)
    # Check if mrtrix3.py is already in bin
    if(NOT EXISTS ${PROJECT_BINARY_DIR}/bin/mrtrix3.py)
        file(COPY ${PROJECT_SOURCE_DIR}/mrtrix3/python/bin/mrtrix3.py DESTINATION ${PROJECT_BINARY_DIR}/bin)
    endif()
endforeach()
