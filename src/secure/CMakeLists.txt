add_library(secure
    secure.cpp
    secure.hpp
)

target_include_directories(secure
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

find_library(PAM_LIB pam)
if(PAM_LIB)
    target_link_libraries(secure
        PUBLIC
            Crow::Crow
            ${PAM_LIB}
    )
else()
    message(FATAL_ERROR "PAM library not found")
endif()