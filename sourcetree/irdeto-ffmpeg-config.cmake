get_filename_component(SUBPROJECT_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
get_filename_component(SUBPROJECT_DIR "${SUBPROJECT_CMAKE_DIR}" PATH)
get_filename_component(SUBPROJECT_NAME "${SUBPROJECT_DIR}" NAME)

# Cache setting that allows choosing between local source build and using RPM
set(${SUBPROJECT_NAME}_BUILD_MODE "rpm" CACHE
        STRING "Use RPM package or build locally from source."
)
set_property(CACHE ${SUBPROJECT_NAME}_BUILD_MODE PROPERTY STRINGS rpm local)

# Local configuration global property for the package
get_property(localconfig_defined
        GLOBAL PROPERTY ${SUBPROJECT_NAME}_LOCALCONFIG DEFINED
)
if(NOT localconfig_defined)
    define_property(GLOBAL PROPERTY ${SUBPROJECT_NAME}_LOCALCONFIG
        BRIEF_DOCS "${SUBPROJECT_NAME}" FULL_DOCS "${SUBPROJECT_NAME}"
    )
endif()

# Parse and sanitize existing configuration, update it based on user input
set(build_mode "")
set(config_path "")

get_property(localconfig GLOBAL PROPERTY ${SUBPROJECT_NAME}_LOCALCONFIG)
list(LENGTH localconfig localconfig_len)
if( ${localconfig_len} EQUAL 2)
    list(GET localconfig 0 build_mode)
    list(GET localconfig 1 config_path)
endif()

if(NOT ("${build_mode}" STREQUAL "${${SUBPROJECT_NAME}_BUILD_MODE}") )
    set(build_mode "${${SUBPROJECT_NAME}_BUILD_MODE}")
    set(config_path "")
endif()

# Build based on current configuration
function(build_local)
    # Guard against introducing duplicate build targets
    if( NOT "${config_path}" MATCHES "^$" )
        return()
    endif()

    # Build locally (include package in current build tree)
    add_subdirectory("${SUBPROJECT_DIR}" "${SUBPROJECT_NAME}")
    set(config_path "${CMAKE_CURRENT_LIST_FILE}" PARENT_SCOPE)
    set(${SUBPROJECT_NAME}_FOUND "TRUE" PARENT_SCOPE)
endfunction()

function(build_rpm)
    # Note: this package doesn't use target importing/exporting,
    # so we need to guard against duplicate targets in a special way
    if( NOT "${config_path}" MATCHES "^$" )
        set(${SUBPROJECT_NAME}_FOUND TRUE PARENT_SCOPE)
        return()
    endif()

    # Try to locate a corresponding RPM installed in system, on fail carry on silently
    unset(${SUBPROJECT_NAME}_DIR CACHE)
    find_package("${SUBPROJECT_NAME}" CONFIG QUIET)
    set(config_path "${${SUBPROJECT_NAME}_CONFIG}" PARENT_SCOPE)
    set(${SUBPROJECT_NAME}_FOUND "${${SUBPROJECT_NAME}_FOUND}" PARENT_SCOPE)
endfunction()

if("${build_mode}" STREQUAL "local" )
    build_local()
elseif( "${build_mode}" STREQUAL "rpm" )
    build_rpm()
    if(NOT ${SUBPROJECT_NAME}_FOUND)
        build_local()
        set(build_mode "local")
    endif()
endif()

# Update configuration
set_property(GLOBAL PROPERTY ${SUBPROJECT_NAME}_LOCALCONFIG "${build_mode};${config_path}")
set_property(CACHE ${SUBPROJECT_NAME}_BUILD_MODE PROPERTY VALUE "${build_mode}")

# Make sure this configuration file is executed each time a dependency is tested
set(${SUBPROJECT_NAME}_DIR "${SUBPROJECT_CMAKE_DIR}" CACHE
        PATH "The directory containing a CMake configuration file for ${SUBPROJECT_NAME}." FORCE)
