# WebAssembly module for the web simulator. Output: gart.js + gart.wasm,
# copied to web/wasm/ after every build.
add_executable(gart_wasm bindings.cpp)
set_target_properties(gart_wasm PROPERTIES OUTPUT_NAME gart SUFFIX ".js")
target_link_libraries(gart_wasm PRIVATE gart::core embind)
target_compile_options(gart_wasm PRIVATE -O3 -msimd128)
target_link_options(gart_wasm PRIVATE
  -O3
  -msimd128
  -sMODULARIZE=1
  -sEXPORT_ES6=1
  -sEXPORT_NAME=createGart
  -sENVIRONMENT=web,worker,node
  -sALLOW_MEMORY_GROWTH=1
  -sINITIAL_MEMORY=33554432
  -sFILESYSTEM=0)
target_compile_options(gart_core PRIVATE -msimd128)

set(GART_WEB_WASM_DIR ${PROJECT_SOURCE_DIR}/web/wasm)
add_custom_command(TARGET gart_wasm POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${GART_WEB_WASM_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE_DIR:gart_wasm>/gart.js
          $<TARGET_FILE_DIR:gart_wasm>/gart.wasm
          ${GART_WEB_WASM_DIR}
  COMMENT "Copying gart.js / gart.wasm to web/wasm")
