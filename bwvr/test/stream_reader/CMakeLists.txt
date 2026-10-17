# The test reader of the live stream of the walks on the PC
# (docs/android.md, "The live stream"). It reads the ring of the mod in a
# desktop run, and it compares the frames of the ring with the frames of a
# record. No game code.
add_executable(bwvr_stream_reader main.cpp)
if(MSVC)
  target_compile_options(bwvr_stream_reader PRIVATE /W4 /permissive-)
endif()
