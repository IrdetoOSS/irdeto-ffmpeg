get_filename_component(SUBPROJECT_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
get_filename_component(SUBPROJECT_DIR "${SUBPROJECT_CMAKE_DIR}" PATH)
set(SUBPROJECT_NAME irdeto-xps-export)

# Expose only include directories, do not try define any build targets
include_directories("${SUBPROJECT_DIR}/${CMAKE_INSTALL_INCLUDEDIR}")
