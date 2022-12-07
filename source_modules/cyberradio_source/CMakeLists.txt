cmake_minimum_required(VERSION 3.13)
project(cyberradio_source)

file(GLOB SRC "src/*.cpp")

add_library(cyberradio_source SHARED ${SRC})
target_link_libraries(cyberradio_source PRIVATE sdrpp_core)
set_target_properties(cyberradio_source PROPERTIES PREFIX "")

target_include_directories(cyberradio_source PRIVATE "src/")

if (MSVC)
    message(WARNING "Libcyberradio Not availible for Windows")
else (MSVC)
    find_package(PkgConfig)
    pkg_check_modules(LIBCYBERRADIO REQUIRED CyberRadio)
    target_compile_options(cyberradio_source PRIVATE -std=c++17)
    #find_package(LibCyberRadio REQUIRED)
    message(STATUS "LIBCYBERRADIO_FOUND=${LIBCYBERRADIO_FOUND}")
    message(STATUS "LIBCYBERRADIO_INCLUDE_DIR=${LIBCYBERRADIO_INCLUDEDIR}")
    message(STATUS "LIBCYBERRADIO_LIB_DIR=${LIBCYBERRADIO_LIBDIR}")
    message(STATUS "LIBCYBERRADIO_LIB=${LIBCYBERRADIO_LIBRARIES}")

    target_include_directories(cyberradio_source PRIVATE ${LIBCYBERRADIO_INCLUDEDIR})
    target_link_directories(cyberradio_source PRIVATE ${LIBCYBERRADIO_LIBDIR})
    target_link_libraries(cyberradio_source PRIVATE ${LIBCYBERRADIO_LIBRARIES})
endif ()

# Install directives
install(TARGETS cyberradio_source DESTINATION lib/sdrpp/plugins)