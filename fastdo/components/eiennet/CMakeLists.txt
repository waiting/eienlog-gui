project(eiennet)

# Headers
file(
    GLOB eiennet_HEADERS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "include/*.*"
)

# Sources
aux_source_directory(src eiennet_SRCS)

# Targets
add_library(eiennet SHARED ${eiennet_HEADERS} ${eiennet_SRCS})
target_include_directories(eiennet PUBLIC include)
target_compile_definitions(eiennet PUBLIC EIENNET_DLL_USE PRIVATE EIENNET_DLL_EXPORTS CURL_STATICLIB)
target_link_libraries(eiennet PRIVATE winux)

add_library(eiennet_a STATIC ${eiennet_HEADERS} ${eiennet_SRCS})
target_include_directories(eiennet_a PUBLIC include)
target_link_libraries(eiennet_a PRIVATE winux_a)

# Public headers
set_target_properties(eiennet eiennet_a PROPERTIES PUBLIC_HEADER "${eiennet_HEADERS}")

# Search libraries
if(UNIX)
    message(STATUS "Project(${PROJECT_NAME}) search libraries ...")

    set(CURL_ROOT "${CMAKE_INSTALL_PREFIX}" CACHE PATH "libcurl path.")
    find_library(LIBCURL NAMES curl HINTS "${CURL_ROOT}/${CMAKE_INSTALL_LIBDIR}")
    find_path(LIBCURL_INCLUDE NAMES "curl/curl.h" HINTS "${CURL_ROOT}/${CMAKE_INSTALL_INCLUDEDIR}")
    if(LIBCURL AND LIBCURL_INCLUDE)
        message(STATUS "curl: ${LIBCURL}")
        message(STATUS "curl_include: ${LIBCURL_INCLUDE}")
        target_compile_definitions(eiennet PRIVATE HAVE_CURL)
        target_compile_definitions(eiennet_a PRIVATE HAVE_CURL)
        target_include_directories(eiennet PRIVATE ${LIBCURL_INCLUDE})
        target_include_directories(eiennet_a PRIVATE ${LIBCURL_INCLUDE})
        target_link_libraries(eiennet PRIVATE ${LIBCURL})
        target_link_libraries(eiennet_a PRIVATE ${LIBCURL})
    else()
        message(FATAL_ERROR "libcurl is not found.")
    endif()
else()
    if(MSVC)
        set(FASTDO_THIRDPARTIES_ROOT $ENV{FASTDO_THIRDPARTIES_ROOT})

        if(FASTDO_THIRDPARTIES_ROOT)
            if(CMAKE_SIZEOF_VOID_P EQUAL 8)
                set(LIB_FASTDO_THIRDPARTIES_PATH "${FASTDO_THIRDPARTIES_ROOT}/x64-Release")
            elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
                set(LIB_FASTDO_THIRDPARTIES_PATH "${FASTDO_THIRDPARTIES_ROOT}/x86-Release")
            endif()

            target_include_directories(eiennet PRIVATE "${FASTDO_THIRDPARTIES_ROOT}/include")
            target_include_directories(eiennet_a PRIVATE "${FASTDO_THIRDPARTIES_ROOT}/include")
        else()
            message(FATAL_ERROR "\$ENV{FASTDO_THIRDPARTIES_ROOT} is not found.")
        endif()

        message(STATUS "Project(${PROJECT_NAME}) search libraries in ${LIB_FASTDO_THIRDPARTIES_PATH} ...")

        target_link_libraries(eiennet PRIVATE ws2_32 crypt32 wldap32 normaliz)
        target_link_libraries(eiennet_a PRIVATE ws2_32 crypt32 wldap32 normaliz)

        find_library(LIBCURL NAMES libcurl_a HINTS "${LIB_FASTDO_THIRDPARTIES_PATH}/lib")
        find_path(LIBCURL_INCLUDE NAMES "curl/curl.h" HINTS "${FASTDO_THIRDPARTIES_ROOT}/${CMAKE_INSTALL_INCLUDEDIR}")
        if(LIBCURL AND LIBCURL_INCLUDE)
            message(STATUS "curl: ${LIBCURL}")
            message(STATUS "curl_include: ${LIBCURL_INCLUDE}")
            target_compile_definitions(eiennet PRIVATE HAVE_CURL)
            target_compile_definitions(eiennet_a PRIVATE HAVE_CURL)
            target_link_libraries(eiennet PRIVATE ${LIBCURL})
            target_link_libraries(eiennet_a PRIVATE ${LIBCURL})
        endif()

        find_library(LIBZ NAMES zlibstat HINTS "${LIB_FASTDO_THIRDPARTIES_PATH}/lib")
        if(LIBZ)
            message(STATUS "zlib: ${LIBZ}")
            target_compile_definitions(eiennet PRIVATE HAVE_Z)
            target_compile_definitions(eiennet_a PRIVATE HAVE_Z)
            target_link_libraries(eiennet PRIVATE ${LIBZ})
            target_link_libraries(eiennet_a PRIVATE ${LIBZ})
        endif()
    endif()
endif()

# Install
install(
    TARGETS eiennet eiennet_a
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}${CONFIG_SUB_PATH}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}${CONFIG_SUB_PATH}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}${CONFIG_SUB_PATH}"
    PUBLIC_HEADER DESTINATION include
)
