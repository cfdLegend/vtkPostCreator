SET(SRC
    TicTok.cxx
    VariableManager.cxx
    Variable.cxx
    VtkLoader.cxx
)

add_library(moviemaker-common STATIC ${SRC})
target_link_libraries(moviemaker-common PUBLIC ${VTK_LIBRARIES})
target_link_libraries(moviemaker-common PUBLIC nlohmann_json::nlohmann_json)
target_link_libraries(moviemaker-common PUBLIC stdc++fs)
target_link_libraries(moviemaker-common PUBLIC ${OMPLIB})
