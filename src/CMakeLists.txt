add_executable(my_project
    main.cpp
    main.hpp
)

add_subdirectory(logic)

target_link_libraries(my_project
    PRIVATE
        logic
)
