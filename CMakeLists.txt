cmake_minimum_required(VERSION 3.9)

project(libpspproxy VERSION 0.1.0 DESCRIPTION "Userspace library to interface with a real PSP from the x86 userspace")

add_library(pspproxy SHARED
    psp-proxy.c
)

set_target_properties(pspproxy PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(pspproxy PROPERTIES SOVERSION 0)
set_target_properties(pspproxy PROPERTIES PUBLIC_HEADER libpspproxy.h)
target_include_directories(pspproxy PRIVATE .)
target_include_directories(pspproxy PRIVATE include)

add_library(pspproxystatic STATIC
    psp-proxy.c
)
set_target_properties(pspproxystatic PROPERTIES OUTPUT_NAME pspproxy)
set_target_properties(pspproxystatic PROPERTIES PUBLIC_HEADER libpspproxy.h)
target_include_directories(pspproxystatic PRIVATE .)
target_include_directories(pspproxystatic PRIVATE include)

include(GNUInstallDirs)
install(TARGETS pspproxy
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(TARGETS pspproxystatic
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_file(libpspproxy.pc.in libpspproxy.pc @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/libpspproxy.pc DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)
