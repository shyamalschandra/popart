include(GNUInstallDirs)

set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL
    "Default value for POSITION_INDEPENDENT_CODE of targets.")

set(POPLAR_INSTALL_DIR "" CACHE STRING "The Poplar install directory")
list(APPEND WILLOW_CMAKE_ARGS -DPOPLAR_INSTALL_DIR=${POPLAR_INSTALL_DIR})

