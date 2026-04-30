# compile_shaders(<target> <source-list> <output-dir>)
#
# For each Slang source, runs slangc to produce <output-dir>/<basename>.spv,
# attaches the outputs to <target> as a build dependency, and re-runs on source change.
# Each source is expected to mark its entry point with a [shader("...")] attribute
# so slangc can discover the stage automatically.
function(compile_shaders TARGET SOURCES OUTPUT_DIR)
  if(NOT SLANGC_EXECUTABLE)
    message(FATAL_ERROR "slangc not found. Install Slang (e.g. brew install shader-slang/tap/slang).")
  endif()

  file(MAKE_DIRECTORY ${OUTPUT_DIR})

  set(SPV_OUTPUTS "")
  foreach(SHADER ${SOURCES})
    get_filename_component(NAME ${SHADER} NAME_WE)
    set(SPV ${OUTPUT_DIR}/${NAME}.spv)
    add_custom_command(
      OUTPUT ${SPV}
      COMMAND ${SLANGC_EXECUTABLE} ${SHADER}
              -target spirv
              -profile glsl_460
              -emit-spirv-directly
              -o ${SPV}
      DEPENDS ${SHADER}
      COMMENT "Compiling shader ${NAME}.slang"
      VERBATIM
    )
    list(APPEND SPV_OUTPUTS ${SPV})
  endforeach()

  add_custom_target(${TARGET}_shaders DEPENDS ${SPV_OUTPUTS})
  add_dependencies(${TARGET} ${TARGET}_shaders)
endfunction()
