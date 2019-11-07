
add_custom_target(
  Tarballs  ALL
  DEPENDS 803-Emulator.tgz
)

add_custom_command ( OUTPUT 803-Emulator.tgz 
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/createTarballs.sh
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/)
