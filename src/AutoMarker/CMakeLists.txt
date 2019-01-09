set(LLVM_LINK_COMPONENTS
    ${LLVM_TARGETS_TO_BUILD}
    Option
    Support
)

add_clang_executable(clang-automarker
    src/main.cpp
    src/Logger.cpp

    src/ast/ASTMapper.cpp
    src/ast/ASTPruner.cpp
    src/ast/SimpleStatement.cpp
    src/ast/Solution.cpp

        src/ast/helpers/IsModifiedChecker.cpp
        src/ast/helpers/PostCreateTree.cpp

    src/markers/Marker.cpp
    src/markers/ECE459_A1_parallel.cpp
    src/markers/ECE459_A1_nbio.cpp

    src/costmodels/SimpleStatementCostModel.cpp
    src/costmodels/KeyFnCostModel.cpp
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../capted/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(clang-automarker
    clangAST
    clangBasic
    clangDriver
    clangFrontend
    clangRewriteFrontend
    clangStaticAnalyzerFrontend
    clangTooling
)

install(TARGETS clang-automarker
    RUNTIME
    DESTINATION
    bin
)
