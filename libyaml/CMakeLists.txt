set(SOURCE_FILES
  src/api.c
  src/dumper.c
  src/emitter.c
  src/loader.c
  src/parser.c
  src/reader.c
  src/scanner.c
  src/writer.c
  )

add_library(yaml STATIC
  ${SOURCE_FILES}
  )

target_include_directories(yaml PUBLIC
  include
  )

target_include_directories(yaml PRIVATE
  win32
  )

target_compile_definitions(yaml PRIVATE
  HAVE_CONFIG_H
  )
