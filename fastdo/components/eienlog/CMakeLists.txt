project(eienlog)

# Headers
file(
    GLOB eienlog_HEADERS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "include/*.*"
)

# Sources
aux_source_directory(src eienlog_SRCS)

add_library(eienlog SHARED ${eienlog_SRCS} ${eienlog_HEADERS})
target_include_directories(eienlog PUBLIC include)
target_compile_definitions(eienlog PUBLIC EIENLOG_DLL_USE PRIVATE EIENLOG_DLL_EXPORTS)
target_link_libraries(eienlog PRIVATE winux eiennet)

add_library(eienlog_a STATIC ${eienlog_SRCS} ${eienlog_HEADERS})
target_include_directories(eienlog_a PUBLIC include)
target_link_libraries(eienlog_a PRIVATE winux_a eiennet_a)

# Public headers
set_target_properties(eienlog eienlog_a PROPERTIES PUBLIC_HEADER "${eienlog_HEADERS}")

# Install
install(
    TARGETS eienlog eienlog_a
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}${CONFIG_SUB_PATH}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}${CONFIG_SUB_PATH}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}${CONFIG_SUB_PATH}"
    PUBLIC_HEADER DESTINATION include
)
