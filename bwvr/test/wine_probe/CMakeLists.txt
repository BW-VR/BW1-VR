# The test program of Wine in the headset app (docs/android.md, "Wine in
# the app"). A 32 bit console program with no game code. The emulator test
# of Wine puts it into the game folder of the app. Only Windows calls, so
# it builds with the static CRT and needs nothing beside it.
add_executable(bwvr_wine_probe main.cpp)
target_link_libraries(bwvr_wine_probe PRIVATE gdi32 advapi32)
if(MSVC)
  target_compile_options(bwvr_wine_probe PRIVATE /W4 /permissive-)
endif()
