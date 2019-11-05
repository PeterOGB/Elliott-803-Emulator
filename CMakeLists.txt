cmake_minimum_required(VERSION 2.8.0)

PROJECT("803 Emulator" C)

ADD_EXECUTABLE(803 Main.c Keyboard.c Hands.c Contactor.c Charger.c PowerCabinet.c Sound.c 
  Cpu.c HoursRun.c Emulate.c PTS.c Wiring.c Common.c E803ops.c Logging.c fsm.c ) 

SET(CMAKE_C_FLAGS "-std=gnu99  -g  -Wall -Wextra -Wunused -Wconversion"
"-Wundef -Wcast-qual -Wmissing-prototypes "
"-Wredundant-decls -Wunreachable-code -Wwrite-strings -Warray-bounds"
"-Wstrict-aliasing=3 -Wstrict-overflow=1 -Wstrict-prototypes -Winline"
"-Wshadow -Wswitch -Wmissing-include-dirs -Woverlength-strings -Wpacked"
"-Wdisabled-optimization  -Wformat=2 -Winit-self"
"-Wunused-parameter -Wlogical-op -Wuninitialized"
"-Wnested-externs -Wpointer-arith -Wdouble-promotion -Wunused-macros"
"-Wunsafe-loop-optimizations -rdynamic -fdiagnostics-color")



STRING(REGEX REPLACE ";" " " CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")


find_package( PkgConfig REQUIRED)


pkg_check_modules ( GLIB REQUIRED glib-2.0 )
include_directories(${GLIB_INCLUDE_DIRS})
set(LIBS ${LIBS} ${GLIB_LIBRARIES})

pkg_check_modules ( GDK REQUIRED gdk-3.0 )
include_directories(${GDK_INCLUDE_DIRS})
set(LIBS ${LIBS} ${GDK_LIBRARIES})

#pkg_check_modules ( XMU REQUIRED xmu )
#include_directories(${XMU_INCLUDE_DIRS})
#set(LIBS ${LIBS} ${XMU_LIBRARIES})

pkg_check_modules ( GTK REQUIRED gtk+-3.0 )
include_directories(${GTK_INCLUDE_DIRS})
set(LIBS ${LIBS} ${GTK_LIBRARIES})

pkg_check_modules ( ALSA REQUIRED alsa )
include_directories(${ALSA_INCLUDE_DIRS})
set(LIBS ${LIBS} ${ALSA_LIBRARIES})

#pkg_check_modules ( FT2  REQUIRED freetype2 )
#include_directories(${FT2_INCLUDE_DIRS})
#set(LIBS ${LIBS}  ${FT2_LIBRARIES})

target_link_libraries(803 ${LIBS} -liberty -lm )

