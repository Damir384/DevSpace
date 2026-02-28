add_executable(DevSpace
    main.cpp
    main.hpp
)

add_custom_command(TARGET DevSpace PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/devspace"
)

set_target_properties(DevSpace PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/devspace"
)

add_subdirectory(logic)

target_link_libraries(DevSpace
    PRIVATE
        logic
)

file(GLOB_RECURSE TEMPLATE_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/templates/*"
)

file(GLOB_RECURSE STATIC_FILES
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/static/*"
)

add_custom_target(assets ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_SOURCE_DIR}/devspace/templates"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_SOURCE_DIR}/devspace/static"

    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/src/templates"
        "${CMAKE_SOURCE_DIR}/devspace/templates"

    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/src/static"
        "${CMAKE_SOURCE_DIR}/devspace/static"

    DEPENDS
        ${TEMPLATE_FILES}
        ${STATIC_FILES}

    COMMENT "Copying templates and static assets to devspace..."
)

add_dependencies(DevSpace assets)