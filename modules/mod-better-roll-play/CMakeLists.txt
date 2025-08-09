cmake_minimum_required(VERSION 3.5)
project(mod_better_roll_play)

add_library(mod_better_roll_play SHARED
    mod_better_roll_play.cpp
)

target_link_libraries(mod_better_roll_play
    PRIVATE
        azerothcore
)

set_target_properties(mod_better_roll_play PROPERTIES
    PREFIX ""
)

install(TARGETS mod_better_roll_play DESTINATION lib)
