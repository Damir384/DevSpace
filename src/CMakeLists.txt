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

add_custom_command(TARGET DevSpace POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/devspace"
    
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/src/templates"
        "${CMAKE_SOURCE_DIR}/devspace/templates"
    
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/src/static"
        "${CMAKE_SOURCE_DIR}/devspace/static"
        
    COMMENT "Moving binary and assets to root devspace directory..."
)