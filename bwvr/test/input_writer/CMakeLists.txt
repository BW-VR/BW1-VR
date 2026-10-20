# The test writer of the input channel on the PC (docs/android.md, "The
# input channel"). It writes the input of a record into the channel of the
# mod in a desktop run, as the headset app does, and it compares two
# records. No game code.
add_executable(bwvr_input_writer main.cpp)
if(MSVC)
  target_compile_options(bwvr_input_writer PRIVATE /W4 /permissive-)
endif()
