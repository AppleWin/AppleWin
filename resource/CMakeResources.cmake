function(add_resources out_var id)
  set(result)

  string(CONCAT content_h
    "#include <map>\n"
    "#include <cstdint>\n"
    "\n"
    "namespace ${id}\n"
    "{\n"
    "    extern const std::map<uint32_t, std::pair<const unsigned char *, unsigned int>> data\;\n"
    "}\n"
  )

  string(CONCAT content_cpp_top
    "#include \"${id}_data.h\"\n"
    "#include \"../resource/resource.h\"\n"
  )

  string(CONCAT content_cpp_public
    "namespace ${id}\n"
    "{\n"
    "\n"
    "    const std::map<uint32_t, std::pair<const unsigned char *, unsigned int>> data = {\n"
  )

  set(options ${ARGN})

  while(options)
    list(POP_FRONT options resource_id in_f_bin)
    get_filename_component(in_f_name "${in_f_bin}" NAME)

    set(out_f_cpp "${CMAKE_CURRENT_BINARY_DIR}/${in_f_name}.cpp")
    add_custom_command(
      OUTPUT ${out_f_cpp}
      COMMAND xxd -i -n ${in_f_name} ${in_f_bin} > ${out_f_cpp}
      COMMENT "Adding resource: ${in_f_bin} -> ${out_f_cpp}"
      DEPENDS ${binary_file}
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # this is the same as what xxd does re. filename
    string(REGEX REPLACE "[ ./-]" "_" symbol ${in_f_name})

    string(APPEND content_cpp_private
      "// ${in_f_bin}\n"
      "extern unsigned char ${symbol}[]\;\n"
      "extern unsigned int ${symbol}_len\;\n"
      "\n")
    string(APPEND content_cpp_public
      "        {${resource_id}, {${symbol}, ${symbol}_len}},\n")

    list(APPEND result ${out_f_cpp})
  endwhile()

  string(APPEND content_cpp_public
    "    }\;\n"
    "\n"
    "}\n"
  )

  set(out_h "${CMAKE_CURRENT_BINARY_DIR}/${id}_data.h")
  file(WRITE ${out_h} ${content_h})
  message("Generating header for '${id}': ${out_h}")
  list(APPEND result ${out_h})
  set(out_cpp "${CMAKE_CURRENT_BINARY_DIR}/${id}_data.cpp")
  file(WRITE ${out_cpp}
    ${content_cpp_top}
    "\n"
    ${content_cpp_private}
    "\n"
    ${content_cpp_public})
  message("Generating cpp for '${id}': ${out_cpp}")
  list(APPEND result ${out_cpp})
  set(${out_var} "${result}" PARENT_SCOPE)

endfunction()
