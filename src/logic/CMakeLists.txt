add_library(logic
    logic.cpp
    logic.hpp
)

target_include_directories(logic
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(logic
    PUBLIC
        Crow::Crow
)
