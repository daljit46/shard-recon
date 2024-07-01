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

add_custom_target(PythonExternalCommands ALL)
foreach(cmd_file ${PYTHON_COMMANDS})
    get_filename_component(CMDNAME ${cmd_file} NAME_WE)
    # Read the file to check if it contains an "import mrtrix3" statement
    file(READ ${cmd_file} cmd_content)
    string(FIND "${cmd_content}" "import mrtrix3" has_import)
    if(has_import GREATER -1)
        add_custom_command(
            TARGET PythonExternalCommands
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${cmd_file} ${PROJECT_BINARY_DIR}/mrtrix3/lib/mrtrix3/commands/${CMDNAME}.py
        )
        add_custom_command(
            TARGET PythonExternalCommands
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/bin
            COMMAND ${CMAKE_COMMAND} 
                -DCMDNAME=${CMDNAME}
                -DOUTPUT_DIR="${PROJECT_BINARY_DIR}/bin"
                -DEXTERNAL_PROJECT_COMMAND=TRUE
                -P ${PROJECT_SOURCE_DIR}/mrtrix3/cmake/MakePythonExecutable.cmake

        )
    else()
        file(COPY ${cmd_file} DESTINATION ${PROJECT_BINARY_DIR}/bin/${CMDNAME})
    endif()
endforeach()
