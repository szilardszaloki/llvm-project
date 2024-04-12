set(LLVM_LINK_COMPONENTS support)

add_clang_executable(state-machine-visualizer
  StateMachineVisualizer.cpp
)

target_link_libraries(state-machine-visualizer
  PRIVATE
  clangAST
  clangASTMatchers
  clangBasic
  clangFrontend
  clangSerialization
  clangTooling
)
