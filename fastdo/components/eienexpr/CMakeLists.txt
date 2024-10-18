project(eienexpr)

# Headers
file(
    GLOB eienexpr_HEADERS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "include/*.*"
)

# Sources
aux_source_directory(src eienexpr_SRCS)

add_library(eienexpr SHARED ${eienexpr_SRCS} ${eienexpr_HEADERS})
target_include_directories(eienexpr PUBLIC include)
target_compile_definitions(eienexpr PUBLIC EIENEXPR_DLL_USE PRIVATE EIENEXPR_DLL_EXPORTS)
target_link_libraries(eienexpr PRIVATE winux)

add_library(eienexpr_a STATIC ${eienexpr_SRCS} ${eienexpr_HEADERS})
target_include_directories(eienexpr_a PUBLIC include)
target_link_libraries(eienexpr_a PRIVATE winux_a)

# Public headers
set_target_properties(eienexpr eienexpr_a PROPERTIES PUBLIC_HEADER "${eienexpr_HEADERS}")

# Install
install(
    TARGETS eienexpr eienexpr_a
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}${CONFIG_SUB_PATH}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}${CONFIG_SUB_PATH}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}${CONFIG_SUB_PATH}"
    PUBLIC_HEADER DESTINATION include
)
