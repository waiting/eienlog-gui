project(winux)

# Headers
file(
    GLOB winux_HEADERS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "include/*.*"
)
file(
    GLOB winux_INTERNAL_HEADERS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "src/*.h"
)
file(
    GLOB eienexpr_HEADERS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "../eienexpr/include/*.*"
)

# Sources
aux_source_directory(src winux_SRCS)
aux_source_directory(../eienexpr/src eienexpr_SRCS)

# Targets
add_library(winux SHARED ${winux_SRCS} ${eienexpr_SRCS} ${winux_HEADERS} ${winux_INTERNAL_HEADERS} ${eienexpr_HEADERS})
target_include_directories(winux PUBLIC include PRIVATE ../eienexpr/include)
target_compile_definitions(winux PUBLIC WINUX_DLL_USE PRIVATE WINUX_DLL_EXPORTS)

add_library(winux_a STATIC ${winux_SRCS} ${eienexpr_SRCS} ${winux_HEADERS} ${winux_INTERNAL_HEADERS} ${eienexpr_HEADERS})
target_include_directories(winux_a PUBLIC include PRIVATE ../eienexpr/include)

# Public headers
set_target_properties(winux winux_a PROPERTIES PUBLIC_HEADER "${winux_HEADERS}")

# Compile options
option(MIXED_REF_NO_EXCEPTION "The Mixed::ref*() is no exception." OFF)
if(MIXED_REF_NO_EXCEPTION)
    target_compile_definitions(winux PUBLIC MIXED_REF_NO_EXCEPTION)
    target_compile_definitions(winux_a PUBLIC MIXED_REF_NO_EXCEPTION)
endif()


# Search libraries
if(UNIX)
    message(STATUS "Project(${PROJECT_NAME}) search libraries ...")

    find_library(LIBDL NAMES dl)
    if(LIBDL)
        message(STATUS "dl: ${LIBDL}")
        target_link_libraries(winux PRIVATE ${LIBDL})
        target_link_libraries(winux_a PRIVATE ${LIBDL})
    endif()

    find_library(LIBRT NAMES rt)
    if(LIBRT)
        message(STATUS "rt: ${LIBRT}")
        target_link_libraries(winux PRIVATE ${LIBRT})
        target_link_libraries(winux_a PRIVATE ${LIBRT})
    endif()

    find_file(ICONV_H NAMES iconv.h)
    if(ICONV_H)
        message(STATUS "iconv: ${ICONV_H}")
        target_compile_definitions(winux PRIVATE HAVE_ICONV)
        target_compile_definitions(winux_a PRIVATE HAVE_ICONV)
    endif()

    find_library(LIBPTHREAD NAMES pthread)
    if(LIBPTHREAD)
        message(STATUS "pthread: ${LIBPTHREAD}")
        target_compile_definitions(winux PRIVATE HAVE_PTHREAD)
        target_compile_definitions(winux_a PRIVATE HAVE_PTHREAD)
        target_link_libraries(winux PRIVATE ${LIBPTHREAD})
        target_link_libraries(winux_a PRIVATE ${LIBPTHREAD})
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

            target_include_directories(winux PRIVATE "${FASTDO_THIRDPARTIES_ROOT}/include")
            target_include_directories(winux_a PRIVATE "${FASTDO_THIRDPARTIES_ROOT}/include")
        else()
            message(FATAL_ERROR "\$ENV{FASTDO_THIRDPARTIES_ROOT} is not found.")
        endif()

        message(STATUS "Project(${PROJECT_NAME}) search libraries in ${LIB_FASTDO_THIRDPARTIES_PATH} ...")

        find_library(LIBICONV NAMES iconv HINTS "${LIB_FASTDO_THIRDPARTIES_PATH}/lib")
        if(LIBICONV)
            message(STATUS "iconv: ${LIBICONV}")
            target_compile_definitions(winux PRIVATE HAVE_ICONV)
            target_compile_definitions(winux_a PRIVATE HAVE_ICONV)
            target_link_libraries(winux PRIVATE ${LIBICONV})
            target_link_libraries(winux_a PRIVATE ${LIBICONV})
        endif()

        find_library(LIBPTHREAD NAMES pthreadVC2 HINTS "${LIB_FASTDO_THIRDPARTIES_PATH}/dll")
        if(LIBPTHREAD)
            message(STATUS "pthread: ${LIBPTHREAD}")
            target_compile_definitions(winux PRIVATE HAVE_PTHREAD)
            target_compile_definitions(winux_a PRIVATE HAVE_PTHREAD)
            target_link_libraries(winux PRIVATE ${LIBPTHREAD})
            target_link_libraries(winux_a PRIVATE ${LIBPTHREAD})

            find_file(PTHREAD_DLL NAMES pthreadVC2.dll HINTS "${LIB_FASTDO_THIRDPARTIES_PATH}/dll")
            if(PTHREAD_DLL)
                message(STATUS "Install pthread_dll: ${PTHREAD_DLL}")
                install(FILES ${PTHREAD_DLL} DESTINATION "${CMAKE_INSTALL_BINDIR}${CONFIG_SUB_PATH}")
            endif()
        endif()
    endif()

endif()

# Install
install(
    TARGETS winux winux_a
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}${CONFIG_SUB_PATH}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}${CONFIG_SUB_PATH}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}${CONFIG_SUB_PATH}"
    PUBLIC_HEADER DESTINATION include
)
