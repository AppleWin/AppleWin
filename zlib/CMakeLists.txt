set(ZLIB_SOURCES
  adler32.c
  crc32.c
  compress.c
  uncompr.c
  gzclose.c
  gzlib.c
  gzread.c
  gzwrite.c
  deflate.c
  infback.c
  inftrees.c
  inffast.c
  inflate.c
  trees.c
  zutil.c
)

add_library(zlib2 STATIC
  ${ZLIB_SOURCES}
  )

target_include_directories(zlib2 INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

target_compile_options(zlib2 PRIVATE
  -Wno-implicit-function-declaration
  )
