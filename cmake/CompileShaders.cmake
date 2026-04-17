# compile_shaders(<target> <source-list> <output-dir>)
#
# For each GLSL source, runs glslangValidator -V to produce <output-dir>/<name>.spv,
# attaches the outputs to <target> as a build dependency, and re-runs on source change.
function(compile_shaders TARGET SOURCES OUTPUT_DIR)
  if(NOT Vulkan_GLSLANG_VALIDATOR_EXECUTABLE)
    message(FATAL_ERROR "glslangValidator not found. Install the Vulkan SDK.")
  endif()

  file(MAKE_DIRECTORY ${OUTPUT_DIR})

  set(SPV_OUTPUTS "")
  foreach(SHADER ${SOURCES})
    get_filename_component(NAME ${SHADER} NAME)
    set(SPV ${OUTPUT_DIR}/${NAME}.spv)
    add_custom_command(
      OUTPUT ${SPV}
      COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} -V ${SHADER} -o ${SPV}
      DEPENDS ${SHADER}
      COMMENT "Compiling shader ${NAME}"
      VERBATIM
    )
    list(APPEND SPV_OUTPUTS ${SPV})
  endforeach()

  add_custom_target(${TARGET}_shaders DEPENDS ${SPV_OUTPUTS})
  add_dependencies(${TARGET} ${TARGET}_shaders)
endfunction()
