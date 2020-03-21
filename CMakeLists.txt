cmake_minimum_required(VERSION 3.9)

project(libpspproxy VERSION 0.1.0 DESCRIPTION "Userspace library to interface with a real PSP from the x86 userspace")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DIN_PSP_EMULATOR")
add_library(pspproxy SHARED
    psp-proxy.c
    psp-proxy-provider-sev.c
    psp-proxy-provider-serial.c
    psp-proxy-provider-tcp.c
    psp-stub-pdu.c
)

set_target_properties(pspproxy PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(pspproxy PROPERTIES SOVERSION 0)
set_target_properties(pspproxy PROPERTIES PUBLIC_HEADER libpspproxy.h)
target_include_directories(pspproxy PRIVATE .)
target_include_directories(pspproxy PRIVATE include)
target_include_directories(pspproxy PRIVATE psp-includes)

add_library(pspproxystatic STATIC
    psp-proxy.c
    psp-proxy-provider-sev.c
    psp-proxy-provider-serial.c
    psp-proxy-provider-tcp.c
    psp-stub-pdu.c
)
set_target_properties(pspproxystatic PROPERTIES OUTPUT_NAME pspproxy)
set_target_properties(pspproxystatic PROPERTIES PUBLIC_HEADER libpspproxy.h)
set_target_properties(pspproxystatic PROPERTIES POSITION_INDEPENDENT_CODE True)
target_include_directories(pspproxystatic PRIVATE .)
target_include_directories(pspproxystatic PRIVATE include)
target_include_directories(pspproxystatic PRIVATE psp-includes)

include(GNUInstallDirs)
install(TARGETS pspproxy
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(TARGETS pspproxystatic
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_file(libpspproxy.pc.in libpspproxy.pc @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/libpspproxy.pc DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)
