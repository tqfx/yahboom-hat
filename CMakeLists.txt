if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel ..." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release MinSizeRel RelWithDebInfo)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS 1 CACHE BOOL "Enable/Disable output of compile commands during generation.")

cmake_minimum_required(VERSION 2.8.12...4.0)

project(yahboom-hat C)

add_executable(${PROJECT_NAME}
  minIni/minGlue.h
  minIni/minIni.h
  minIni/minIni.c
  ssd1306_i2c.h
  ssd1306_i2c.c
  pool_str.h
  pool_str.c
  i2c.h
  i2c.c
  rgb.h
  rgb.c
  main.c
)
target_compile_options(${PROJECT_NAME} PRIVATE -pedantic -Wall -Wextra)

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
