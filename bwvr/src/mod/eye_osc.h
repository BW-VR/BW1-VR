// The gaze of an eye tracker from OSC messages over UDP (docs/backend.md,
// "Foveated rendering"). Steam Link sends the gaze of the Steam Frame this
// way, and the senders of other eye trackers do too.
#pragma once

// Starts the thread that receives the messages on the port eye_osc_port, on
// 127.0.0.1 or, with eye_osc_any=1, on all addresses of the PC. A second
// call does nothing. Port 0 starts nothing.
void EyeOscStart(int port, bool any_address);

// Stops the thread and closes the socket.
void EyeOscStop();

// The last gaze that came, in the space of the head (x right, y up, -z
// forward), when it is at most osc_gaze_math::kFreshMs old.
bool EyeOscGaze(float direction[3]);

// One line in the log with the packets, the messages and the gazes since
// the last line.
void EyeOscReport();
