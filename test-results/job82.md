# Job 82: the grip pose on the frame of the controller model (agent/grip efab675)

`.\dev.ps1 build -Clean`: 6479 checks, 0 failed. Each log has "Black & White VR Mod 1.2.0 (build efab675)". `hand_run.ps1 -Kind hints`, dxwrapper, `test_frame_rate=60`, `card_send_test_save=0`. Each run folder has the eye pictures at frames 00113, 00233, 00313, 00393 and 00473. The desk-state.txt of each run says "the real cursor: at the same place before and after the run".

The picture crops are in C:\BW1-VR\local\shots\job82 (full-left-*, crop-left-*, zoomL-left-00233, zoomR-left-00233, crop-right-00393). Each sheet has grip0, grip1 and grip2 side by side at the same frame.

## 1. grip0 (grip_fit=1, no grip_fit_test): PASS

- "grip fit     : grip_fit=1. On Virtual Desktop, the Oculus runtime and SteamVR, the grip pose of a Meta Touch controller is the frame of its model: the model, the hand and the Hand of the game go on that frame. The trim of the player is 0 mm ahead, 0 mm up and 0 mm out (grip_trim_ahead_mm, grip_trim_up_mm, grip_trim_out_mm)"
- "grip fit     : the right hand, runtime "", model meta-quest-touch-plus: the table has no fit for them. The model and the hand are on the grip pose of the runtime, frame 0"
- "grip fit     : the left hand, runtime "", model meta-quest-touch-plus: the table has no fit for them. The model and the hand are on the grip pose of the runtime, frame 0"
- The run is in the God view. "view check   : frame 42, the left hand. The grip point is 50.2 cm from the middle of the head. ... (0.00 and 0.00 pixels apart). The two eyes see it 0.00 cm farther from the head, and 0.00 cm to the side."
- The two report windows: "818 points of the grip. ... 0.00 cm farther from the head ... at 52.2 cm from the head. The worst gap ... is 0.00 pixels" and "900 points of the grip. ... at 50.5 cm from the head. The worst gap ... is 0.00 pixels".
- This run is the reference for the pictures of grip1 and grip2.

## 2. grip1 (grip_fit_test=1): PASS

- "grip fit     : grip_fit=1. ... The trim of the player is 0 mm ahead, 0 mm up and 0 mm out (grip_trim_ahead_mm, grip_trim_up_mm, grip_trim_out_mm). grip_fit_test=1: the scripted controller gets the fit of Virtual Desktop"
- "grip fit     : the right hand, runtime "VirtualDesktopXR", model meta-quest-touch-plus: the frame of the model is turned 18.0 degrees from the grip pose of the runtime, at 0.3 -1.1 -2.6 cm in the frame of the grip (x right, y up, z back). The middle of the controller moves 2.7 cm, frame 0"
- "grip fit     : the left hand, runtime "VirtualDesktopXR", model meta-quest-touch-plus: the frame of the model is turned 18.0 degrees from the grip pose of the runtime, at -0.3 -1.1 -2.6 cm in the frame of the grip (x right, y up, z back). The middle of the controller moves 2.7 cm, frame 0"
- The view check still gives 0.00 cm: "view check   : frame 42, the left hand. The grip point is 52.7 cm from the middle of the head. ... (0.00 and 0.01 pixels apart). The two eyes see it 0.00 cm nearer to the head, and 0.00 cm to the side."
- The report windows: "818 points ... at 53.3 cm from the head. The worst gap ... is 0.01 pixels" and "900 points ... at 51.5 cm from the head. The worst gap ... is 0.00 pixels".
- The grip distance differs from grip0: 52.7 cm against 50.2 cm at frame 42, and 53.3 / 51.5 cm against 52.2 / 50.5 cm in the report windows. The fit moves the drawn grip point 2.5 cm farther from the head at frame 42.

Pictures (the same frames as grip0):
- Frame 00233 (both controllers and hands), left eye: both controllers and hands are higher in the picture than in grip0, by about 35 px of 2208 at the ring. The head looks down on the land, so higher in the picture is farther ahead, toward the top of the controller. This agrees with the move of 2.6 cm along -z.
- The turn: in grip0 the ring and the face of each controller show almost face-on to the eye. In grip1 the ring shows as a flatter ellipse, and the handle hangs more steeply below it. So the front of the controller (the ring end) turns more down as seen by the player. This agrees with the fit in grip_fit_math.h ("16 to 18 degrees down about x", q x = -0.156).
- The hand stays on the controller as in grip0: the thumb is on the face buttons and the thumbstick, and the index finger and the other fingers wrap round the front of the handle under the ring at the same places. The hand and the model move and turn as one.
- Frame 00113 (the hands only, no controller): both hands are higher and the fingers hang more down than in grip0. The same move and turn.
- Frame 00393, right eye: the same difference. The left controller with its hand and the right open hand are higher and turned more down than in grip0.
- I cannot measure the 18 degrees or the 2.7 cm from the pictures. The size and the direction of the change agree with the log.
- The hint cards are the same pictures in all three runs (the same MD5 for bwvr-hintcard-left-button0-00233.bmp). This is correct, because a card is a texture and the fit does not change it.

## 3. grip2 (grip_fit_test=1, grip_trim_ahead_mm=30, grip_trim_out_mm=-20): PASS

- "grip fit     : grip_fit=1. ... The trim of the player is 30 mm ahead, 0 mm up and -20 mm out (grip_trim_ahead_mm, grip_trim_up_mm, grip_trim_out_mm). grip_fit_test=1: the scripted controller gets the fit of Virtual Desktop"
- The fit lines of the two hands are the same as in grip1 ("turned 18.0 degrees ... at 0.3 -1.1 -2.6 cm" and "at -0.3 -1.1 -2.6 cm ... moves 2.7 cm, frame 0").
- "view check   : frame 42, the left hand. The grip point is 53.8 cm from the middle of the head. ... (0.00 and 0.00 pixels apart). The two eyes see it 0.00 cm farther from the head, and 0.00 cm to the side."
- The report windows: "818 points ... at 52.6 cm from the head. The worst gap ... is 0.01 pixels" and "900 points ... at 50.6 cm from the head. The worst gap ... is 0.01 pixels".
- The grip distance changes from grip1 by +1.1 cm at frame 42 (53.8 against 52.7), and by -0.7 cm and -0.9 cm in the report windows. That is about 1 cm, less than "a few cm". A trim of 3 cm ahead and 2 cm in moves the point mostly across the line of sight from the head, so the distance to the head changes less than the trim. The view check stays at 0.00 cm.

Pictures:
- Frame 00233, left eye: both controllers are farther ahead (higher in the picture by about 25 to 35 px at the ring) than in grip1. They are nearer to each other: the gap of the two rings is about 300 px of the crop against about 340 px in grip1.
- The turn of the controllers is the same as in grip1, and the hand stays on the controller in the same way.
- Frames 00113 and 00393 show the same: the hands and the controller are higher and nearer to the middle than in grip1.

## What only a headset can show

- The desk runs use the scripted controller with the fit of Virtual Desktop (grip_fit_test=1). The real runtime name and the real grip pose of Virtual Desktop come only in a headset session.
- Only a headset can show whether the drawn controller and hand now sit on the real controller in the hand of the player, and which trim, if any, the player needs.
