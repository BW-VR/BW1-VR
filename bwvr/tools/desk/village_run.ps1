# Desktop runs of the village: the totem of the Village Centre, the miracle
# icons of the Worship Site, the Village Store, a villager, scaffolds and the
# Workshop. See bwvr/docs/village.md, "The village with the real hand", and
# bwvr/docs/grab.md.
#
#   village_run.ps1 -Name vilD -Dll <dll> -Kind miracle
#   village_run.ps1 -Name vilE -Dll <dll> -Kind totem -Set "totem_hand=0"
#   village_run.ps1 -Name vilF -Dll <dll> -Kind miracle -Icon 3
#
#   village_run.ps1 -Name vilG -Dll <dll> -Kind store -NewGame
#
# THE START. The run starts in the test save "temple" (README.md, "A fast
# start"): no story, and a town of the player with no Worship Site. When the
# save is not there, one run makes it. The steps and the frames of the test
# keys count from frame 800 as the frame at which the controller first places
# the Hand in the loaded game (fake_controller_play_frame=800), so each frame
# in the log is about 730 less than the frame of the step. -NewGame gives a
# new game with the choice "Keep your old Creature" (spawn_run.ps1): the same
# state. -Save gives a different test save with that state. With no -Seconds,
# a run in a test save ends 4 s after its last step or census.
#
# The test keys of village.cpp make the rest with the game's own functions:
#   village_test_site       the frame at which the town gets its Worship Site
#   village_test_miracles   the magic types that the town knows (22 Water, 23
#                           its power up, 14 Food, 15 its power up, 10 Heal)
#   village_test_chants     amount@frame: the pool of chants of the site
#   village_test_census     frames at which the log gets the towns and icons
#   village_test_icon       the seed type of the icon for the place "icon"
# The test key seek_plan gives the places of the word "seek": gosite, icon,
# gototem, totem, up, down (hand_direct.cpp).
#
# -Kind totem:   the grip closes on the totem, the hand goes 3 steps of 4 cm
#                up and 5 steps down, the grip opens.
# -Kind miracle: a tap on the icon of Water, the pour, the power up with the
#                spiral (B and the word "trace"), a tap of B (the cancel), the
#                choice with the gestures (the spiral and the W shape), and a
#                shake. -Icon gives the seed type of a different icon (3 is
#                Food, which pours as Water does).
# -Kind palm:    the gesture pictures of the game and the palm that turns up
#                (bwvr/docs/miracles.md, "The gesture row over the palm" and
#                "The book on the palm"): the steps of -Kind miracle with
#                gesture_panel=0 and eye pictures of the corner, then
#                gesture_panel=1 and the book on the palm.
# The other kinds (charge, store, neartake, land, job, tree, throw,
# throwlive, throwhouse, throwhouselow, throwpile, probe, workshop, site, build, scaffold,
# turn, fartake, fartotem, choice, look, book, controls, rayover, walk, wall,
# housestop, headhouse, pinch,
# aimsign, aimmiracle, aimpast, tglow, tooltip) have a comment at their steps
# below.
param(
    [Parameter(Mandatory = $true)][string]$Name,
    [Parameter(Mandatory = $true)][string]$Dll,
    [string]$Kind = 'miracle',
    [string]$Set = '',
    [string]$Steps = '',
    [string]$Plan = '',
    [string]$Chants = '20000@950',
    [string]$Census = '',
    [string]$Miracles = '22,23,14,15,10',
    [int]$Icon = 9,
    [string]$Gestures = '64 64 48',
    [int]$Seconds = 0,
    [string]$Go = '1860,2580|1840,2600|1880,2610|1850,2670|1790,2650|1900,2660',
    [int]$Lines = 160,
    [string]$Save = 'temple',
    [string]$Parts = '',
    [switch]$NewGame
)
$sp = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $sp 'game.ps1')
$start = ''
if (-not $NewGame) { $start = Confirm-BwvrTestSave $Save $Dll }
# With no -Seconds, each kind gives the length of its run in a new game.
$given = $Seconds -gt 0
# The zoom steps of these recipes keep the land under the head in its place
# (god_zoom_about=0).
$keys = 'god_zoom_about=0;far_test_influence=1;grab_hand=1;far_grab_max_m=0'
$points = '0,-30,60 14,-32,56 -12,-31,58 10,-30,62 -6,-32,54 4,-31,64 0,-30,60 12,-31,58'
$pre = 'stick_up@800+10 stick_up@830+10 look@860+5000 point@900+1 '
if ($Kind -eq 'totem') {
    if ($Plan -eq '') { $Plan = 'gototem,totem,up,up,up,down,down,down,down,down' }
    if ($Census -eq '') { $Census = '890,1400' }
    if ($Seconds -eq 0) { $Seconds = 60 }
    if ($Steps -eq '') {
        $Steps = $pre + 'seek@920+1 seek@1000+1 grip@1040+280 seek@1090+1 seek@1120+1 seek@1150+1 ' +
                 'seek@1180+1 seek@1210+1 seek@1240+1 seek@1270+1 seek@1300+1 shot@1390+1'
    }
} elseif ($Kind -eq 'fartotem') {
    # The totem from a distance (bwvr/docs/village.md, "The totem from a
    # distance"). The God view goes 60 cm of the room before the totem, the
    # hand goes near the body, and the word "aimfar" aims at the totem, the
    # nearest bulk source (seek_bulk=1). The grip stays closed for 400 frames.
    # The hand goes 3 steps of 4 cm up and 1 step down, and the grip opens.
    # The lines "far grab" and "village" show who has the totem and when the
    # Hand lets it go. With -Set "totem_take=0" the far grab lets the totem go
    # 700 ms after the press, with the grip still closed. The eye pictures
    # show the ghost of the pole while the grip holds it and after the
    # release (totem_ghost), and the height of the pole in the second after
    # the release (totem_hold).
    if ($Plan -eq '') { $Plan = 'gototem,up,up,up,down' }
    if ($Census -eq '') { $Census = '890' }
    if ($Seconds -eq 0) { $Seconds = 70 }
    $points = '0,-30,60 0,-10,20'
    if ($Steps -eq '') {
        $Steps = $pre + 'seek@920+1 point@1000+1 aimfar@1040+1 grip@1080+400 ' +
                 'seek@1140+1 shot@1150+1 seek@1170+1 seek@1200+1 shot@1230+1 seek@1260+1 shot@1300+1 ' +
                 'shot@1500+1 shot@1530+1 shot@1560+1'
    }
    $keys = "$keys;seek_bulk=1;far_test_least_cm=30;far_grab_choice=0;far_touch=0"
} elseif ($Kind -eq 'charge') {
    # An icon that charges for more than one game turn (3000 of 5000 chants),
    # the tap that stops it, a full pool, a tap, and a press with the miracle
    # at its icon (the miracle goes back).
    if ($Plan -eq '') { $Plan = 'gosite,icon' }
    if ($Census -eq '') { $Census = '890,1100,1260,1500' }
    if ($Seconds -eq 0) { $Seconds = 65 }
    if ($Chants -eq '20000@950') { $Chants = '3000@950 20000@1300' }
    if ($Steps -eq '') {
        $Steps = $pre + 'seek@920+1 seek@1000+1 grip@1040+3 grip@1200+3 grip@1340+3 grip@1440+3 shot@1560+1'
    }
    $keys = "$keys;village_test_icon=$Icon"
} elseif ($Kind -eq 'store') {
    # The Village Store: the grip collects from a pile, opens (the bundle
    # stays), the hand goes 8 cm up and then over the point of the store, and
    # the grip closes and opens (the bundle goes into the store).
    if ($Plan -eq '') { $Plan = 'gostore,store,pile,up,up,store' }
    if ($Census -eq '') { $Census = '890' }
    if ($Seconds -eq 0) { $Seconds = 62 }
    if ($Steps -eq '') {
        $Steps = $pre + 'seek@920+1 seek@1000+1 seek@1040+1 grip@1080+120 seek@1230+1 seek@1250+1 ' +
                 'seek@1280+1 grip@1330+40 shot@1450+1'
    }
} elseif ($Kind -eq 'tooltip') {
    # The tooltip at the Hand's point (bwvr/docs/ui-layer.md, 15.3). With
    # hint_card=0 a tooltip of the game draws in the eye pictures and not on
    # the card. The Hand's point goes onto a pile of the Village Store, and
    # the grip collects from it: the game gives the tooltip "Amount". An eye
    # picture before the grip and two in the take. The lines "ui hint" give
    # the box of each tooltip in each eye and its place at the Hand, and the
    # report of "ui depth" counts the tooltips at the Hand of the controller.
    if ($Plan -eq '') { $Plan = 'gostore,store,pile' }
    if ($Census -eq '') { $Census = '890' }
    if ($Seconds -eq 0) { $Seconds = 45 }
    if ($Steps -eq '') {
        $Steps = $pre + 'seek@920+1 seek@1000+1 seek@1040+1 shot@1070+1 grip@1080+150 shot@1150+1 shot@1210+1'
    }
    $keys = "$keys;hint_card=0;help_test=1"
} elseif ($Kind -eq 'neartake') {
    # A take at the Hand that goes on while the hand moves away from the pile
    # (bwvr/docs/grab.md, "The bulk take"). The grip closes on a pile of the
    # Village Store and stays closed for 600 frames. In that time the hand
    # steps 12 cm to the side (slap), 15 cm toward the head (pull) and three
    # times 10 cm up (up, seek_step_cm=10). The lines "hand bulk" have the
    # amount in the Hand, one line at 15, 30, 60 and 120 cm of the room from
    # the pile, and the end of the take with the button of the game, the grip,
    # the interface action and the influence at that time. At step 1400 the
    # squeeze dips to 0.59 for 2 frames (gripdip), a fast fall. With
    # near_take_keeps=1 the line "the grip falls fast during a bulk take and
    # stays closed" comes, and the take goes on to the opening of the grip.
    # With 0 the dip opens the grip and ends the take.
    if ($Plan -eq '') { $Plan = 'gostore,store,pile,up,up,up' }
    if ($Census -eq '') { $Census = '890' }
    if ($Seconds -eq 0) { $Seconds = 75 }
    if ($Steps -eq '') {
        $Steps = $pre + 'seek@920+1 seek@1000+1 seek@1040+1 shot@1070+1 grip@1080+600 slap@1160+1 ' +
                 'pull@1220+10 shot@1260+1 seek@1300+1 seek@1340+1 seek@1380+1 gripdip@1400+2 shot@1420+1 shot@1660+1 shot@1720+1'
    }
    $keys = "$keys;seek_step_cm=10"
} elseif ($Kind -eq 'land') {
    # The Hand stops at the land (bwvr/docs/hand-model.md, "The Hand stops at
    # the land"). Near the Village Store the Hand's point goes 3 cm over open
    # land in the middle of the view (open: no body within 40 cm of the
    # room), then 15 cm of the room
    # down (down, seek_step_cm=15), so the real hand is 12 cm in the land. The
    # hand steps 2 cm to the side (nudge), 12 cm to the other side (slap) and
    # back (slap) in the land, and then 15 cm up from the drawn point (up),
    # out of the land. An eye picture at each place. Give it
    # -Set "backend=dxwrapper\ddraw.dll" for the eye pictures. The lines
    # "hand land" have the touch, the lift and the end of the touch.
    if ($Plan -eq '') { $Plan = 'gostore,open,down,up' }
    if ($Census -eq '') { $Census = '890' }
    if ($Seconds -eq 0) { $Seconds = 55 }
    if ($Steps -eq '') {
        $Steps = $pre + 'seek@920+1 seek@1000+1 shot@1030+1 seek@1060+1 shot@1090+1 nudge@1120+1 shot@1150+1 ' +
                 'slap@1180+1 shot@1210+1 slap@1240+1 shot@1270+1 seek@1300+1 shot@1330+1'
    }
    $keys = "$keys;seek_step_cm=15"
} elseif ($Kind -eq 'job') {
    # A villager near the Village Store: the grip takes him, the hand goes to
    # the point of the store and the grip opens. The log has his job.
    if ($Plan -eq '') { $Plan = 'gostore,store,villager,store' }
    if ($Census -eq '') { $Census = '890' }
    if ($Seconds -eq 0) { $Seconds = 62 }
    if ($Steps -eq '') {
        $Steps = $pre + 'seek@920+1 seek@1000+1 seek@1040+1 grip@1046+260 seek@1180+1 shot@1420+1'
    }
} elseif ($Kind -eq 'book') {
    # The spell book (bwvr/docs/miracles.md, "The spell book"). X opens the
    # book over the left hand, and the word "book" steps the right hand to the
    # middle of the first item and then of the second. The grip closes on the
    # second item, the word "pull" moves the hand 9 cm toward the head, and
    # the grip opens: the bubble springs back into the book. The hand goes
    # back. The grip closes again and the hand moves 15 cm: at 10 cm the
    # bubble snaps into the hand, and the game puts the miracle in the Hand.
    # A tap of B cancels it. Then the pool of the site has no chants, X opens
    # the book again, the right hand goes to an item and the grip takes it:
    # the game refuses it, and the card says why. X closes the book. Each
    # picture of the book and of the bubble is saved (spell_book_test_save).
    # -Set "spell_book_pull_cm=0" gives the take at the grip, -Set
    # "debug_all_miracles=1" gives the book every miracle of the game, and
    # -Set "spell_book=0" gives X no job.
    if ($Plan -eq '') { $Plan = 'gosite' }
    if ($Census -eq '') { $Census = '890,1300' }
    if ($Seconds -eq 0) { $Seconds = 60 }
    if ($Chants -eq '20000@950') { $Chants = '20000@950 0@1400' }
    if ($Steps -eq '') {
        $Steps = $pre + 'seek@920+1 x@1000+4 shot@1012+1 book@1030+1 shot@1042+1 book@1070+1 shot@1082+1 ' +
                 'grip@1110+40 pull@1115+60 shot@1130+1 shot@1145+1 pull@1200+10 grip@1240+60 pull@1245+40 ' +
                 'shot@1260+1 shot@1272+1 shot@1300+1 ' +
                 'pull@1320+10 b@1350+5 x@1420+4 book@1440+1 shot@1452+1 grip@1470+20 shot@1482+1 ' +
                 'x@1540+4 shot@1560+1'
    }
    $keys = "$keys;spell_book_test_save=16"
} elseif ($Kind -eq 'controls') {
    # The page Controls of the VR Settings window (bwvr/docs/ui-layer.md,
    # "The page Controls", and bwvr/docs/controllers.md, "The map of the
    # buttons"). The welcome page opens 3 s after the controllers can act
    # (welcome_done=0), and the trigger presses its button VR Settings. In
    # the window the trigger presses the tab Controls (tab2), then the box
    # "Y button" of the left controller (page1 with the models of
    # bwvr/assets/controllers): its list opens. The trigger picks its item
    # Recenter (item2): Y takes the job of the left stick click (the
    # recenter hold), and the stick click takes the ring. The ray rests on
    # the box "B button" of the right controller (page9), so the model
    # lights B. Close closes the window. Then a hold of the left stick click
    # opens the ring, and a hold of Y recenters the view. Each new picture of
    # the window is saved (debug_window_test_save).
    # -Parts gives other parts of the window for the word aimdebug, and
    # -Steps other steps. For Reset Defaults: -Set "button_map_other=1324"
    # -Parts "tab2 reset close". -Set "hint_profile=valve/index_controller"
    # draws Index controllers, -Set "hint_card_style=0" the dark look, and
    # -Set "controls_page_models=0" the page with the drawing and the rows
    # (then -Parts "tab2 right5 row9 close").
    if ($Plan -eq '') { $Plan = 'gosite' }
    if ($Census -eq '') { $Census = '890' }
    if ($Parts -eq '') { $Parts = 'tab2 page1 item2 page9 close' }
    if ($Steps -eq '') {
        # The first aimdebug aims at the button of the welcome page, then
        # one aimdebug for each word of -Parts. The shots are eye pictures
        # (with the renderer dxwrapper): the tab Controls, the list, the
        # new job, and the ray on B.
        $Steps = 'aimdebug@1400+1 trigger@1410+4 aimdebug@1460+1 trigger@1470+4 shot@1500+1 ' +
                 'aimdebug@1520+1 trigger@1530+4 shot@1560+1 aimdebug@1580+1 trigger@1590+4 shot@1620+1 ' +
                 'aimdebug@1640+1 shot@1680+1 aimdebug@1700+1 trigger@1710+4 lstick_click@1780+60 y@1920+200'
    }
    # test_frame_rate=60: the frames of the steps count for a game at 60
    # frames a second, also with the renderer Vulkan.
    $keys = "$keys;welcome_done=0;welcome_test_parts=settings;debug_test_parts=$Parts;debug_window_test_save=12;test_frame_rate=60"
} elseif ($Kind -eq 'rayover') {
    # The ray over the panels in the room (bwvr/docs/ui-layer.md, "The ray
    # over the panels"). The welcome page opens 3 s after the controllers can
    # act (welcome_done=0). The word point puts the hand where a player holds
    # it: 15 cm right, 25 cm down and 35 cm in front of the head, below the
    # eyes and in front of the panel (the first point of the list is the
    # point of the Main Menu). The word aimdebug aims the ray at the button
    # Settings, and the eye pictures come while the ray rests there. The
    # trigger presses VR Settings, which opens the VR Settings window. The ray
    # rests on the row 2 of the window until its note shows, more eye
    # pictures, and the trigger presses Close. The strip of the ray is saved
    # (ray_card_test_save), and so are the first pictures of the page and the
    # window. Give it
    # -Set 'backend=dxwrapper\ddraw.dll' for eye pictures, and
    # -Set 'video_view=1' with the Vulkan renderer for the pictures of the
    # view for videos, which have the panels and the strip over the eye
    # picture. Compare with -Set 'ray_over_panels=0'.
    if ($Plan -eq '') { $Plan = 'gosite' }
    if ($Census -eq '') { $Census = '890' }
    if ($Parts -eq '') { $Parts = 'row2 close' }
    $points = '15,-25,35'
    if ($Steps -eq '') {
        $Steps = 'point@1380+1 aimdebug@1400+1 shot@1420+1 trigger@1440+4 aimdebug@1490+1 shot@1530+1 ' +
                 'aimdebug@1560+1 trigger@1570+4 shot@1600+1'
    }
    # test_frame_rate=60: the frames of the steps count for a game at 60
    # frames a second, also with the renderer Vulkan.
    $keys = "$keys;welcome_done=0;welcome_test_parts=settings;debug_test_parts=$Parts;welcome_test_save=2;debug_window_test_save=2;ray_card_test_save=3;test_frame_rate=60"
} elseif ($Kind -eq 'look') {
    # A villager that a script turns to the camera (bwvr/docs/village.md, "A
    # villager that a script turns to the player"). In the test save "temple"
    # a script turns the villager of script thing 189, 114 units from the
    # head on the map, to "camera position" at log frame 164 (step frame 894), and
    # again some times later. The log has the place of the villager, of the
    # head and of the game camera, and the heading that the game gave him. The
    # eye pictures come after the turn. The two zoom steps first make the
    # game put its own camera about 400 units from the player
    # (bwvr/docs/god-view.md, section 7). Compare with -Set
    # "villager_look_head=0".
    if ($Plan -eq '') { $Plan = 'gostore' }
    if ($Census -eq '') { $Census = '890' }
    if ($Seconds -eq 0) { $Seconds = 40 }
    if ($Steps -eq '') {
        $Steps = $pre + 'shot@900+1 shot@960+1 shot@1100+1'
    }
} elseif ($Kind -eq 'tree') {
    # A tree near the Village Store: the grip takes it, the hand goes to the
    # point of the store and the grip opens. The store takes the tree as
    # wood.
    if ($Plan -eq '') { $Plan = 'gostore,store,treebase,store' }
    if ($Census -eq '') { $Census = '890' }
    if ($Seconds -eq 0) { $Seconds = 62 }
    if ($Steps -eq '') {
        $Steps = $pre + 'seek@920+1 seek@1000+1 seek@1040+1 grip@1046+260 seek@1180+1 shot@1420+1'
    }
} elseif ($Kind -eq 'throw') {
    # A villager near the Village Store: the grip takes him, the hand steps
    # 15 cm toward the head, and the grip opens in that step: a throw of
    # about 2.6 m/s at 44 units for each meter.
    if ($Plan -eq '') { $Plan = 'gostore,store,villager' }
    if ($Census -eq '') { $Census = '890' }
    if ($Seconds -eq 0) { $Seconds = 62 }
    if ($Steps -eq '') {
        $Steps = $pre + 'seek@920+1 seek@1000+1 seek@1040+1 grip@1046+100 pull@1143+3 shot@1200+1 shot@1420+1'
    }
    $keys = "$keys;throw_log=1"
} elseif ($Kind -eq 'throwlive') {
    # A soft throw and a hard throw of a villager (bwvr/docs/throw.md, "The
    # damage"). The first throw is the throw of -Kind throw: a pull of 15 cm
    # over 3 frames, about 3 m/s. Then the Hand takes the nearest villager
    # again, goes to the second point (head height, 30 cm before the head),
    # and jumps to the third point in one frame as the grip opens: 42 cm
    # forward, down and to the left, a throw of about 12.6 m/s. The census
    # before, after the soft throw and after the hard throw gives the count
    # of the villagers of the town that can worship. A villager that dies
    # goes out of that count.
    if ($Plan -eq '') { $Plan = 'gostore,store,villager,villager' }
    if ($Census -eq '') { $Census = '890,1400,1700,1760,1800,1850,1900,2000' }
    if ($Seconds -eq 0) { $Seconds = 72 }
    $points = '0,-30,60 0,0,30 -25,-30,45'
    if ($Steps -eq '') {
        $Steps = $pre + 'seek@920+1 seek@1000+1 seek@1040+1 grip@1046+100 pull@1143+3 shot@1200+1 shot@1300+1 ' +
                 'seek@1420+1 grip@1426+120 point@1480+1 point@1545+1 shot@1550+1 shot@1580+1 shot@1640+1 ' +
                 'shot@1750+1 shot@1950+1'
    }
    $keys = "$keys;throw_log=1"
} elseif ($Kind -in 'throwhouse', 'throwhousefast', 'throwhouselow') {
    # A rock that falls onto a house (bwvr/docs/throw.md, "The damage",
    # Abode::ReactToPhysicsImpact). The God view goes before the Village
    # Store, the hand goes to the height of the head, and the word "aimfar"
    # aims at the nearest rock in the view (far_test_aim_class=3). The grip
    # and a pull fetch it. With the rock in the Hand the God view goes to
    # 1810.8 2680.1 (the place "go", seek_go), 40 units before the house at
    # 1850.2 2673.1 of the test save "temple". The Hand's point goes over
    # that house at 1100 percent of its height, 19.6 cm of the room past its
    # middle on the far side from the camera (the place "feel"). The rock in
    # the Hand hangs about 7 units toward -x from the Hand's point, so it is
    # then about 37 units (80 cm of the room) over the roof. The grip opens
    # with a still hand, higher than release_fall_cm: the rock falls onto
    # the roof. A throw down from a lower place meets the house on its real
    # arc before the game's physics has the rock (0.1 to 0.2 s after the
    # release). With throw_arc_objects=1 the arc stops at the box of the
    # house, and the line "The real arc meets the box of the Abode" says so.
    # Then the Hand's point goes 2 cm of the room beside the wall of
    # the house (the second "feel"), and the log has the card of the hand
    # there. The head looks 25 degrees down and not to the side, so the
    # house is in the view. Eye pictures of the house before and after the
    # hit. Give it -Set "backend=dxwrapper\ddraw.dll" for eye pictures.
    #
    # -Kind throwhousefast: the same, but the hand steps 20 cm down in one
    # frame as the grip opens (the place "down", seek_step_cm=20), a throw of
    # about 7.5 m/s down and toward the head. That throw moves the rock about
    # 0.75 units toward -x for each unit of the fall, so the Hand's point
    # goes 78 cm of the room past the middle of the house.
    #
    # -Kind throwhouselow: the throw of throwhousefast from a low place, the
    # Hand's point at 300 percent of the height of the house and 44 cm of the
    # room past its middle. The rock is about 5 units over the roof at the
    # release, and the real arc meets the box of the house after about 0.01
    # s, before the game's physics has the rock (bwvr/docs/throw.md, "The
    # objects on the arc"). With throw_arc_objects=1 the log has "meets the
    # box of the Abode ... before the land" and a first contact "(an
    # object)", and the roof breaks. Compare with -Set "throw_arc_objects=0".
    # A fall from a still hand does not give this case: the rock does not
    # meet the roof before the game has it.
    $fast = $Kind -in 'throwhousefast', 'throwhouselow'
    if ($Plan -eq '') { $Plan = $(if ($fast) { 'gostore,go,feel,down,feel' } else { 'gostore,go,feel,feel' }) }
    if ($Census -eq '') { $Census = '890' }
    if ($Seconds -eq 0) { $Seconds = 66 }
    $points = '0,-30,60 0,0,45'
    if ($Steps -eq '') {
        $Steps = 'stick_up@800+10 stick_up@830+10 point@900+1 seek@920+1 point@1000+1 aimfar@1040+1 ' +
                 'grip@1080+300 pull@1120+3 shot@1160+1 seek@1200+1 seek@1240+1 shot@1250+1 ' +
                 $(if ($fast) { 'seek@1379+1 ' } else { '' }) +
                 'shot@1382+1 shot@1400+1 shot@1420+1 shot@1440+1 shot@1500+1 seek@1600+1 shot@1620+1 shot@1700+1'
    }
    $gap = $(if ($Kind -eq 'throwhouselow') { '-44' } elseif ($fast) { '-78' } else { '-19.6' })
    $height = $(if ($Kind -eq 'throwhouselow') { '300' } else { '1100' })
    $keys = "fake_head_pitch=-25;$keys;far_test_aim_class=3;seek_step_cm=20;" +
            "seek_go=1810.8,2680.1;seek_feel=house,$height,$gap|house,50,2;throw_log=1"
} elseif ($Kind -eq 'throwpile') {
    # Wood thrown into the pile of the Village Store. The grip takes wood
    # from the pile and opens (the bundle stays in the Hand), as in -Kind
    # store. The hand goes to the second point, 25 cm before the head and
    # out of the reach of hold_target_cm of the store, and the grip closes.
    # The hand pulls 15 cm toward the head, and then steps back over 3
    # frames as the grip opens: a throw of about 3 m/s forward and down,
    # toward the store about 58 cm before the head. A grip that opens with
    # the bundle 15 cm over the pile is no throw: the store has the halo,
    # and the game puts the wood into the store (packet 0x11). The lines
    # "hand throw" have the flight, and the lines "village" and "hand bulk"
    # what the game does with the bundle. Give it
    # -Set "backend=dxwrapper\ddraw.dll" for eye pictures.
    if ($Plan -eq '') { $Plan = 'gostore,store,pile' }
    if ($Census -eq '') { $Census = '890,1700' }
    if ($Seconds -eq 0) { $Seconds = 64 }
    $points = '0,-30,60 0,-10,25'
    if ($Steps -eq '') {
        $Steps = $pre + 'seek@920+1 seek@1000+1 seek@1040+1 grip@1080+120 point@1230+1 shot@1250+1 ' +
                 'grip@1300+80 pull@1320+3 pull@1377+3 shot@1385+1 shot@1400+1 shot@1430+1 shot@1500+1 shot@1600+1'
    }
    $keys = "$keys;throw_log=1"
} elseif ($Kind -eq 'probe') {
    # One scaffold in the Hand over six places of the test key seek_go: the
    # log says where the game has a place for a building.
    if ($Plan -eq '') { $Plan = 'scaffold,go,go,go,go,go,go' }
    if ($Census -eq '') { $Census = '960,1680' }
    if ($Seconds -eq 0) { $Seconds = 68 }
    if ($Steps -eq '') {
        $Steps = $pre + 'seek@1000+1 grip@1040+580 seek@1100+1 seek@1180+1 seek@1260+1 seek@1340+1 ' +
                 'seek@1420+1 seek@1500+1 shot@1720+1'
    }
    $keys = "$keys;village_test_scaffolds=1@930;seek_go=$Go"
} elseif ($Kind -eq 'workshop') {
    # A Workshop on the land under the Hand, and then wood from the pile of
    # the Village Store that the hand carries to the Workshop.
    if ($Plan -eq '') { $Plan = 'gostore,store,pile,goshop,shop' }
    if ($Census -eq '') { $Census = '915,1600' }
    if ($Seconds -eq 0) { $Seconds = 66 }
    if ($Steps -eq '') {
        $Steps = $pre + 'seek@920+1 seek@1000+1 seek@1040+1 grip@1080+120 seek@1240+1 seek@1320+1 ' +
                 'grip@1400+40 shot@1640+1'
    }
    $keys = "$keys;village_test_workshop=905"
} elseif ($Kind -eq 'site') {
    # One scaffold: the grip takes it, the God view goes to a place where the
    # game builds (seek_go), and the grip opens: a building site. Then wood
    # from the pile of the Village Store goes to the planted scaffold.
    if ($Plan -eq '') { $Plan = 'scaffold,go,gostore,store,pile,go,scaffold' }
    if ($Census -eq '') { $Census = '960,1240,1800' }
    if ($Seconds -eq 0) { $Seconds = 72 }
    if ($Steps -eq '') {
        $Steps = $pre + 'seek@1000+1 grip@1040+160 seek@1100+1 seek@1260+1 seek@1320+1 seek@1360+1 ' +
                 'grip@1400+120 seek@1560+1 seek@1640+1 grip@1700+40 shot@1840+1'
    }
    $keys = "$keys;village_test_scaffolds=1@930;seek_go=1860,2580|1860,2580"
} elseif ($Kind -eq 'turn') {
    # The building of a scaffold and the roll of the wrist (bwvr/docs/village.md,
    # "The building of a scaffold and the wrist"). The grip takes one scaffold,
    # and the God view goes to a place where the game builds (seek_go). The
    # word "rot" turns the right hand to the next pose of fake_controller_rots:
    # a roll of 45, of 90 and of -45 degrees, then a turn of the aim 25
    # degrees to the left and 20 degrees up with the roll of -45 degrees
    # (no roll, so no turn of the building). The grip opens: a building site
    # at the angle of the wrist. An eye picture after each pose. With -Set
    # "scaffold_wrist_turn=0" the building turns by itself, as in the plain
    # game. Give it -Set 'backend=dxwrapper\ddraw.dll' for eye pictures.
    if ($Plan -eq '') { $Plan = 'scaffold,go' }
    if ($Census -eq '') { $Census = '960,1640' }
    if ($Seconds -eq 0) { $Seconds = 70 }
    if ($Steps -eq '') {
        $Steps = $pre + 'seek@1000+1 grip@1040+460 seek@1100+1 shot@1160+1 rot@1180+1 shot@1210+1 ' +
                 'rot@1260+1 shot@1290+1 rot@1340+1 shot@1370+1 rot@1420+1 shot@1450+1 shot@1580+1'
    }
    $keys = "$keys;village_test_scaffolds=1@930;seek_go=1860,2580|1860,2580;fake_controller_rots=0,-30,45 0,-30,90 0,-30,-45 25,-10,-45"
} elseif ($Kind -eq 'build') {
    # One scaffold on the land under the Hand: the grip takes it and opens at
    # the same place.
    if ($Plan -eq '') { $Plan = 'scaffold' }
    if ($Census -eq '') { $Census = '960,1300,1500' }
    if ($Seconds -eq 0) { $Seconds = 60 }
    if ($Steps -eq '') { $Steps = $pre + 'seek@1000+1 grip@1040+160 shot@1420+1' }
    $keys = "$keys;village_test_scaffolds=1@930"
} elseif ($Kind -eq 'scaffold') {
    # Two scaffolds on the land under the Hand. The grip takes the first and
    # opens over the second (they join), a tap takes the result apart, and
    # the grip takes one and opens over the land (the building site).
    if ($Plan -eq '') { $Plan = 'scaffold,scaffold,scaffold,scaffold' }
    if ($Census -eq '') { $Census = '960,1320,1460,1800' }
    if ($Seconds -eq 0) { $Seconds = 70 }
    if ($Steps -eq '') {
        $Steps = $pre + 'seek@1000+1 grip@1040+220 seek@1150+1 seek@1360+1 grip@1400+3 ' +
                 'seek@1500+1 grip@1540+200 point@1620+1 shot@1850+1'
    }
    $keys = "$keys;village_test_scaffolds=2@930"
} elseif ($Kind -eq 'fartake') {
    # A take at a distance from a pile of the Village Store (bwvr/docs/grab.md,
    # "A bulk source at a distance"). The God view goes to the store, and the
    # hand goes to the height of the head: the Hand's point on the store turns
    # the far grab off. The word "aimfar" aims at the nearest bulk source
    # (seek_bulk=1), and the grip stays closed for 1500 frames. The test key
    # far_test_take_gone=30 ends the take at its game turn 30 with the grip
    # closed, and tells the far grab that the source is gone, as for a pile
    # that the game deletes when it is used up. The lines "grip" and "hand
    # pick" then show who has the bundle: the grip carries it to the end of
    # the run, or with -Set "far_take_carry=0" the Hand lets it go at once.
    #
    # A take that the grip ends: -Set "far_test_take_gone=0" with
    #   -Steps "<the steps below with grip@1080+180 and shot@2400+1>"
    # The bundle then stays in the Hand with the grip open.
    #
    # The sound of the take: the lines "audio take" have the start and the
    # stop of the loop (sample 44 of the in game bank for food, 98 for wood),
    # or the refusal of the game with -Set "audio_take_loop=0". With
    # audio_trace=1 the lines "audio play" and "audio chan" have what the DLL
    # sent to QMixer. With audio_capture=1 the mod writes the stream to
    # bwvr-audio.wav in the game folder, and each line "audio take" ends with
    # the frame of that file. Take the file out of the game folder after the
    # run.
    #
    # The stream of the take: the lines "take stream" have its start (the
    # source, its top, the Hand, the length of the way) and its end (the
    # count of the pieces that the mod placed), or with -Set "take_stream=0"
    # that the pieces are on the line of the game. Eye pictures of a take
    # that the grip ends, with pictures while it runs:
    #   -Set "far_test_take_gone=0" -Steps "<the steps below with
    #   grip@1080+400 and shot@1140+1 shot@1180+1 shot@1220+1 shot@1300+1>"
    #
    # far_grab_choice=0 and far_touch=0: a villager or a scroll near the line
    # of the aim does not take the halo from the pile.
    if ($Plan -eq '') { $Plan = 'gostore' }
    if ($Census -eq '') { $Census = '890' }
    if ($Seconds -eq 0) { $Seconds = 80 }
    $points = '0,-30,60 0,0,45'
    if ($Steps -eq '') {
        $Steps = $pre + 'seek@920+1 point@1000+1 aimfar@1040+1 shot@1070+1 grip@1080+1500 ' +
                 'shot@1200+1 shot@2500+1'
    }
    $keys = "$keys;seek_bulk=1;far_test_take_gone=30;far_grab_choice=0;far_touch=0"
} elseif ($Kind -eq 'choice') {
    # The choice of a far target (bwvr/docs/grab.md, "The cone and the
    # choice"). The God view goes to the Village Store. The word "aimfar"
    # takes the villager that has a tree nearest to him (far_test_aim_pair),
    # and each later word turns the aim one step along the line from his body
    # to the base of that tree, past the tree, and back
    # (far_test_aim_blends, in percent of the way). The log has each new
    # target with its reason, and the line of a target that keeps the halo.
    # Compare runs with -Set "far_grab_choice=1;far_grab_stay=0" (the bias),
    # with -Set "far_grab_stay=0" and with no -Set (the default). Add
    # far_test_quest=1 to the first one: the bias then does not know the
    # class of a villager, as it does not know a quest character. Add
    # far_touch=0 to each -Set: the village has scrolls near that line, and a
    # scroll is then no far target. far_test_aim_class=7 takes a quest
    # character in the place of a villager. In the test save "temple" no
    # quest character stands 20 units or less from a tree.
    #
    # The second point of the hand is at the height of the head. The Hand's
    # point on the Village Store turns the far grab off (near objects win).
    if ($Plan -eq '') { $Plan = 'gostore' }
    if ($Census -eq '') { $Census = '890' }
    if ($Seconds -eq 0) { $Seconds = 70 }
    $points = '0,-30,60 0,0,45'
    # A villager walks, so a step of the aim is 20 frames long.
    $blends = '0,30,60,90,120,160,200,240,200,160,120,90,60,30,0'
    if ($Steps -eq '') {
        $Steps = $pre + 'seek@920+1 point@1000+1 '
        $at = 1040
        foreach ($b in $blends.Split(',')) { $Steps += "aimfar@$at+1 "; $at += 20 }
        $Steps += "shot@$at+1"
    }
    $keys = "$keys;far_test_aim_class=1;far_test_aim_toward=1;far_test_aim_pair=1;far_test_aim_blends=$blends"
} elseif ($Kind -eq 'walk') {
    # The walk with the stick to the edge of the map (bwvr/docs/god-view.md,
    # section 5.7). From the door of the Temple the stick of the other hand
    # pushes forward for 40 s, at 100 units for each meter: 6000 units, more
    # than the way to the wall of 3400 units from the middle of the map. The
    # line of the end of the walk has the way, the frames that moved, and how
    # far the land went down for a hill near the head. An eye picture at the
    # start, on the way and at the end. Give it
    # -Set "backend=dxwrapper\ddraw.dll" for eye pictures.
    if ($Plan -eq '') { $Plan = 'up' }
    if ($Census -eq '') { $Census = '890' }
    if ($Seconds -eq 0) { $Seconds = 70 }
    if ($Steps -eq '') {
        $Steps = 'point@900+1 shot@930+1 lstick_up@940+2400 shot@1500+1 shot@2400+1 shot@3330+1 shot@3400+1'
    }
} elseif ($Kind -eq 'wall') {
    # The fingers at the wall of a house and of the Village Store
    # (bwvr/docs/hand-model.md, "Hands that react to the world"). Before the
    # Village Store the Hand's point goes to the side of the nearest house
    # at half its height, with a gap of 0, then 1.5 cm in its body, then to
    # the side of the Village Store. An eye picture at each place. The lines
    # "hand feel" have the bodies and the curls of the fingers, and the
    # lines "haptics" the pulse at the first touch. Five zoom steps in (13.2
    # units for each meter, so a house is about 25 cm high), as in the runs
    # of hand-model.md. Compare with -Set "hand_feel_offer=0".
    if ($Plan -eq '') { $Plan = 'gostore,feel,feel,feel' }
    if ($Census -eq '') { $Census = '890' }
    if ($Seconds -eq 0) { $Seconds = 50 }
    if ($Steps -eq '') {
        $Steps = 'stick_up@800+10 stick_up@830+10 stick_up@860+10 stick_up@890+10 stick_up@920+10 point@950+1 ' +
                 'seek@970+1 seek@1050+1 shot@1080+1 seek@1130+1 shot@1160+1 seek@1210+1 shot@1240+1'
    }
    $keys = "$keys;seek_feel=house,50,0|house,50,-1.5|StoragePit,50,0"
} elseif ($Kind -eq 'housestop') {
    # The Hand stops at the wall of a house (bwvr/docs/hand-model.md, "The
    # Hand stops at the land"). Two zoom steps in (44.4 units for each
    # meter). The God view goes before the Village Store (gostore), then 50
    # cm before the side of the nearest house (gofeel), and one more zoom
    # step in (29.6 units for each meter, so a house is about 11 cm high).
    # The Hand's point then goes to the side of that house across the view
    # (the fourth number 1 of seek_feel), at 70 percent of its height: 6 cm
    # out of the wall, then 2 cm and 5 cm in the box, then 6 cm out again.
    # At 3 cm out the hand of the fake controller already touches the box.
    # An eye picture at each place: with hand_object_stop=1 the drawn hand
    # stays at the wall in the pictures in the box. The lines "hand land"
    # have the touch, the face of the box, each change of the push and the
    # end of the touch. Compare with -Set "hand_object_stop=0". Give it
    # -Set "backend=dxwrapper\ddraw.dll" for the eye pictures.
    if ($Plan -eq '') { $Plan = 'gostore,gofeel,feel,feel,feel,feel' }
    if ($Census -eq '') { $Census = '890' }
    if ($Seconds -eq 0) { $Seconds = 55 }
    if ($Steps -eq '') {
        $Steps = 'stick_up@800+10 stick_up@830+10 point@900+1 seek@920+1 seek@1000+1 stick_up@1040+10 ' +
                 'seek@1090+1 shot@1120+1 seek@1160+1 shot@1190+1 seek@1230+1 shot@1260+1 seek@1300+1 shot@1330+1'
    }
    $keys = "$keys;seek_feel=house,50,0|house,70,6,1|house,70,-2,1|house,70,-5,1|house,70,6,1"
} elseif ($Kind -eq 'headhouse') {
    # The head blinder in a house (bwvr/docs/god-view.md, "The head under the
    # land"). Two zoom steps in (44.4 units for each meter). The God view
    # goes before the Village Store (gostore), then 50 cm before the side of
    # the nearest house (gofeel), one more zoom step in (29.6 units for each
    # meter, so a house is about 11 cm high), and then the head goes over the
    # middle of the nearest house (gointo), about 62 cm over the land. The
    # word "lean" puts the fake head 55 cm lower for 120 frames, into the box
    # of the house, and then the head comes up again. Eye pictures over the
    # house, two in it and one after it. The lines "head blinder" have the
    # start of the overlay, the house, the opaque view and the end. Compare
    # with -Set "head_blinder_objects=0". Give it
    # -Set "backend=dxwrapper\ddraw.dll" for the eye pictures.
    if ($Plan -eq '') { $Plan = 'gostore,gofeel,gointo' }
    if ($Census -eq '') { $Census = '890' }
    if ($Seconds -eq 0) { $Seconds = 50 }
    if ($Steps -eq '') {
        $Steps = 'stick_up@800+10 stick_up@830+10 point@900+1 seek@920+1 seek@1000+1 stick_up@1040+10 ' +
                 'seek@1090+1 shot@1120+1 lean@1160+120 shot@1200+1 shot@1250+1 shot@1330+1'
    }
    $keys = "$keys;seek_feel=house,50,0|house,50,0;fake_head_up_cm=-55"
} elseif ($Kind -eq 'pinch') {
    # A small rock in the pinch at a change of the zoom (bwvr/docs/hand-model.md,
    # "The pinch"). Before the Village Store no rock is within 2 m of the
    # room of the Hand, so the far grab fetches the nearest rock in the view
    # (far_test_aim_class=3), as in -Kind throwhouse, and the grip stays
    # closed. Two zoom steps out (44.4 to 100 units for each meter) make the
    # rock smaller against the hand, and two steps in make it larger again.
    # Eye pictures 3, 9 and 20 frames after each step. The lines "hand feel
    # ... the Hand holds" have the size of the rock against the hand and the
    # pose at each change.
    if ($Plan -eq '') { $Plan = 'gostore' }
    if ($Census -eq '') { $Census = '890' }
    if ($Seconds -eq 0) { $Seconds = 60 }
    $points = '0,-30,60 0,0,45 0,-15,40'
    if ($Steps -eq '') {
        $Steps = 'stick_up@800+10 stick_up@830+10 point@900+1 seek@920+1 point@1000+1 aimfar@1040+1 ' +
                 'grip@1080+560 pull@1120+3 point@1160+1 shot@1190+1'
        foreach ($z in @(@('stick_down', 1220), @('stick_down', 1320), @('stick_up', 1420), @('stick_up', 1520))) {
            $Steps += " $($z[0])@$($z[1])+10 shot@$($z[1])+1 shot@$($z[1] + 6)+1 shot@$($z[1] + 17)+1"
        }
    }
    $keys = "fake_head_pitch=-25;$keys;far_test_aim_class=3"
} elseif ($Kind -eq 'aimsign') {
    # Your Creature and the signpost at the door of the Temple in the aim
    # (bwvr/docs/grab.md, "The cone and the choice"). The hand goes to the
    # height of the head. The word "aimfar" aims at the body of your
    # Creature (far_test_aim_class=4), and each later word turns the aim
    # along the line from the Creature to the base of the nearest signpost
    # (far_test_aim_toward=3), past it, and back. The lines "far grab" have
    # each target and why it won. The word "look" turns the head 30 degrees
    # to the left and 20 degrees down, as in creature_recipe.ps1 -Kind
    # leash: the draw offers the Creature only while the view shows it.
    if ($Plan -eq '') { $Plan = 'up' }
    if ($Census -eq '') { $Census = '890' }
    if ($Seconds -eq 0) { $Seconds = 45 }
    $points = '0,-30,60 0,0,45'
    $blends = '0,50,100,130,100,50,0'
    if ($Steps -eq '') {
        $Steps = 'look@800+1200 point@860+1 point@880+1 '
        $at = 920
        foreach ($b in $blends.Split(',')) { $Steps += "aimfar@$at+1 shot@$($at + 10)+1 "; $at += 30 }
    }
    $keys = "$keys;far_test_aim_class=4;far_test_aim_toward=3;far_test_aim_blends=$blends;far_test_least_cm=20"
} elseif ($Kind -eq 'aimmiracle') {
    # A miracle on the land in the aim. The test key miracle_test_seeds puts
    # a one-off seed of Forest (type 1) on the land under the Hand, as in
    # miracle_run.ps1, and the hand goes back to the height of the head. The
    # word "aimfar" aims at the nearest miracle that the Hand can take
    # (far_test_aim_class=8), and then toward the signpost at the door
    # (far_test_aim_toward=3).
    if ($Plan -eq '') { $Plan = 'up' }
    if ($Census -eq '') { $Census = '890' }
    if ($Seconds -eq 0) { $Seconds = 50 }
    $points = '0,-30,60 0,0,45'
    $blends = '0,50,100,50,0'
    if ($Steps -eq '') {
        $Steps = 'point@900+1 shot@960+1 point@1000+1 '
        $at = 1040
        foreach ($b in $blends.Split(',')) { $Steps += "aimfar@$at+1 shot@$($at + 10)+1 "; $at += 30 }
    }
    $keys = "$keys;miracle_test_seeds=1@930;far_test_aim_class=8;far_test_aim_toward=3;far_test_aim_blends=$blends;far_touch=0;far_test_least_cm=20"
} elseif ($Kind -eq 'aimpast') {
    # A far aim past a near house, tree and pile (bwvr/docs/grab.md, "Near
    # objects win"). Before the Village Store the Hand's point goes 3 cm of
    # the room from the side of the nearest house, then 7 cm from a tree,
    # then 4 cm over a pile of the store. At each place the word "aimfar"
    # aims at the nearest villager (far_test_aim_class=1). The lines
    # "far grab" say if the far grab stays on and which object is the far
    # target.
    if ($Plan -eq '') { $Plan = 'gostore,feel,feel,store,pile,up' }
    if ($Census -eq '') { $Census = '890' }
    if ($Seconds -eq 0) { $Seconds = 55 }
    if ($Steps -eq '') {
        $Steps = $pre + 'seek@920+1 seek@1000+1 aimfar@1030+1 shot@1040+1 seek@1100+1 aimfar@1130+1 shot@1140+1 ' +
                 'seek@1200+1 seek@1230+1 seek@1260+1 aimfar@1290+1 shot@1300+1'
    }
    $keys = "$keys;seek_feel=house,50,3|Tree,50,7;far_test_aim_class=1;far_touch=0"
} elseif ($Kind -eq 'tglow') {
    # The glow on the mesh of a target (bwvr/docs/hand-model.md, "The glow on
    # the mesh of a target") in the light of the day of the test save. The
    # Hand's point goes over the Village Store, onto a pile of the store, to
    # the base of a tree and to a villager, with an eye picture at each.
    # target_glow_log=30 gives the first batches of each target: a batch
    # "after the call of the object" is a mesh of the sorted list. Compare
    # with -Set "target_glow=0" (the halo).
    if ($Plan -eq '') { $Plan = 'gostore,store,pile,treebase,villager' }
    if ($Census -eq '') { $Census = '890' }
    if ($Seconds -eq 0) { $Seconds = 50 }
    if ($Steps -eq '') {
        $Steps = $pre + 'seek@920+1 seek@1000+1 shot@1030+1 seek@1060+1 shot@1090+1 seek@1120+1 shot@1150+1 ' +
                 'seek@1180+1 shot@1210+1'
    }
    $keys = "$keys;target_glow_log=30"
} elseif ($Kind -eq 'palm') {
    # The steps of -Kind miracle with an eye picture at each state of the
    # game's gesture pictures, first with gesture_panel=0: the pictures of an
    # empty Hand, of a Miracle in the Hand, of the power up, of the chain
    # after the spiral, and of the W. The game draws them at the corner of
    # its screen. At step frame 2690 the test key gesture_panel_test_frame
    # turns the row on. Then the word "lrot" turns the left palm up: the
    # palm opens the book (frame 2700) and closes it (2800). With the grip
    # of the left hand on the land the palm opens no book until 0.4 s after
    # the grip ends (2900 to 3140). With B held the palm up shows the row of
    # an empty Hand and opens no book. When B ends, the palm opens the book
    # (3200 to 3480). Give it -Set "backend=dxwrapper\ddraw.dll" for eye
    # pictures.
    if ($Plan -eq '') { $Plan = 'gosite,icon' }
    if ($Census -eq '') { $Census = '890' }
    if ($Seconds -eq 0) { $Seconds = 110 }
    if ($Steps -eq '') {
        $Steps = $pre + 'seek@920+1 shot@960+1 seek@1000+1 grip@1040+3 point@1100+1 shot@1120+1 ' +
                 'trace@1150+320 b@1156+300 shot@1300+1 point@1505+1 grip@1520+60 b@1650+5 shot@1720+1 ' +
                 'trace@1750+320 b@1756+300 shot@1950+1 shot@2075+1 trace@2100+320 b@2106+300 shot@2200+1 ' +
                 'shot@2380+1 pull@2500+1 pull@2506+1 pull@2512+1 pull@2518+1 pull@2524+1 pull@2530+1 ' +
                 'shot@2650+1 lrot@2700+1 shot@2760+1 lrot@2800+1 lrot@2900+1 lgrip@2900+120 shot@2990+1 ' +
                 'shot@3080+1 lrot@3140+1 shot@3160+1 b@3200+150 lrot@3210+1 shot@3300+1 shot@3420+1 lrot@3480+1'
    }
    $keys = "$keys;fake_controller_gestures=$Gestures;village_test_icon=$Icon;gesture_panel=0;" +
            'gesture_panel_test_frame=2690;spell_book_palm=1;' +
            'fake_controller_left_rots=0,0,-90 0,0,0 0,0,-90 0,0,0 0,0,-90 0,0,0'
} else {
    if ($Plan -eq '') { $Plan = 'gosite,icon' }
    if ($Census -eq '') { $Census = '890,1010,1500,1600,1700,2460,2600' }
    if ($Seconds -eq 0) { $Seconds = 100 }
    if ($Steps -eq '') {
        $Steps = $pre + 'seek@920+1 seek@1000+1 grip@1040+3 point@1100+1 ' +
                 'trace@1150+320 b@1156+300 point@1505+1 grip@1520+60 b@1650+5 ' +
                 'trace@1750+320 b@1756+300 trace@2100+320 b@2106+300 ' +
                 'pull@2500+1 pull@2506+1 pull@2512+1 pull@2518+1 pull@2524+1 pull@2530+1 shot@2650+1'
    }
    $keys = "$keys;fake_controller_gestures=$Gestures;village_test_icon=$Icon"
}
$keys = "$keys;seek_plan=$Plan;village_test_chants=$Chants;village_test_census=$Census;village_test_site=870;village_test_miracles=$Miracles"
if ($start) {
    # In a test save the steps and the frames of the test keys count from the
    # frame at which the controller first places the Hand, as frame 800. The
    # start is 10 s shorter than the start of a new game.
    $keys = "fake_controller_play_frame=800;$keys"
    if (-not $given) {
        $after = @($Census.Split(',') | Where-Object { $_ -match '^\d+$' } | ForEach-Object { [int]$_ })
        $Seconds = Get-BwvrSaveSeconds $Steps 800 ($Seconds - 10) $after
    }
}
if ($Set -ne '') { $keys = "$keys;$Set" }
& (Join-Path $sp 'spawn_run.ps1') -Name $Name -Dll $Dll -Choice keep -Steps $Steps `
    -Points $points -Set $keys -Seconds $Seconds -Save $start
# spawn_run.ps1 gave up: no run, so no log of this run to read.
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$log = Join-Path $sp "desk-$Name\bwvr.log"
if (-not (Test-Path $log)) { "no log at $log"; exit 1 }
$pattern = '^village      :|^miracle      : (?!miracle_)|^hand pick    : seek plan|^hand bulk    :|^hand press   : the grip pressed|' +
           '^haptics      : (effect (3|14|15|18|19|20|21|23|24|25|26|27|28)|cast|refused)|' +
           '^hand         : state|^gesture      : (the draw|the game)|^controllers  : the right button press started off|^controllers  : the right button is|^grip         :|^hand (throw|drop|hold|take) *:|^hand land    :|^villager look:'
if ($Kind -eq 'land') {
    $pattern = '^hand land    :|^hand pick    : seek plan|^hand         : the point is|^hand feel    : (a finger|the fingers of)|' +
               '^controllers  : (haptic pulse|the fake)|^haptics      : surface'
}
if ($Kind -eq 'choice') {
    $pattern = '^far grab    : (far_grab_choice|far_grab_stay|the cone of a living|target|no target|the target \S+ \(\w+\) keeps|test: (aimfar|the tree))|^hand pick    : seek plan'
}
if ($Kind -eq 'fartotem') {
    $pattern = '^village      :|^far grab    : (test|target|the grip closed|the grip goes|the game (takes|moves)|the take|the \w+ ends|aimfar|the word aimfar)|' +
               '^hand bulk    : the next pick record|^hand press   : the grip pressed|^grip         : (?!\d)'
}
if ($Kind -eq 'fartake') {
    $pattern = '^far grab    : (far_take_carry|test|target|the grip closed|the grip goes|the game takes|the take|the target \S+ is gone|the \w+ ends|\d+ targets were deleted)|' +
               '^hand bulk    : (the game|the bulk take ends|the amount in the Hand is \d+ after (10|20|30|50|100) game|test key|the take|the test key)|' +
               '^hand pick    : (seek plan|the Hand holds)|^hand press   : the grip pressed|^grip         : (?!\d)|^hand throw   : (release|the object left)|' +
               '^audio play   : .* in game (44|98),|^audio chan   : .* in game (44|98) |^audio take   :|^take stream  :'
}
if ($Kind -eq 'walk') {
    $pattern = '^god view     : (the walk|walk check)|^controllers  : the stick of the other hand'
}
if ($Kind -eq 'wall') {
    $pattern = '^hand pick    : seek plan|^hand feel    : (the fingers|a finger|the \w+ \S+ has the box)|^haptics      : (?!\d)'
}
if ($Kind -eq 'housestop') {
    $pattern = '^hand pick    : seek plan|^hand land    :|^hand feel    : (the fingers|the \w+ \S+ has the box)|^haptics      : surface'
}
if ($Kind -eq 'headhouse') {
    $pattern = '^hand pick    : seek plan|^head blinder :|^hand land    : (hand_object_stop|this window a solid)'
}
if ($Kind -eq 'pinch') {
    $pattern = '^hand pick    : seek plan|^hand feel    : the Hand holds|^god view     : zoomed'
}
if ($Kind -in 'aimsign', 'aimmiracle', 'aimpast') {
    $pattern = '^hand pick    : seek plan|^far grab    : (target|no target|stays on|is off|the target|your Creature|test: (aimfar|the))|^miracle      : test seed'
}
if ($Kind -eq 'tooltip') {
    $pattern = '^hand pick    : seek plan|^ui hint      : .*(tooltip|at the Hand)|^ui depth     : \d+ tooltips'
}
if ($Kind -eq 'tglow') {
    $pattern = '^hand pick    : seek plan|^target glow  : (the mesh|test: a batch|\S+ \S+ has no mesh)'
}
if ($Kind -eq 'palm') {
    $pattern ='^gesture panel:|^spell book   : (the book|the palm)|^controllers  : the fake left hand'
}
if ($Kind -eq 'book') {
    $pattern = '^spell book   :|^village      : (test key|the pool|the miracle icon)|^miracle      : (?!miracle_).*(active in the Hand|left the Hand|takes)|' +
               '^controllers  : (the fake x|the word book|the first face button)|^haptics      : (cast|refused)|^hand         : state'
}
# The hint cards of this hand go in the list: the left hand for the page
# Controls, which moves the jobs of the left controller.
$hand = 'right'
if ($Kind -eq 'controls') {
    $pattern = '^controls     :|^settings     :|^hints        : the pointing hand|' +
               '^controllers  : (the (left|right) controller (is|has)|the new map|button_map|.*recenters the view|the word aimdebug)|' +
               '^ring         : the ring (opens|closes|does not)|^spell book   : the book (opens|closes)'
    $hand = 'left'
}
if ($Kind -eq 'rayover') {
    $pattern = '^ray card     :|^settings     :|^debug window : the window opens|^overlay      : (the ray of the menu|the glow of a ray|ray_glow)|' +
               '^controllers  : the fake hand (aims|steps)|^debug window : (the note|the trigger)|^eye capture  :|^video view   : the (ray|ring)|^vk window    : the picture'
}
$cards = @{}
$card = "^hint card    : frame (\d+), the card of the $hand hand: (.*?)\. \d"
Get-Content $log | Where-Object { $_ -match $pattern -or $_ -match $card } | ForEach-Object {
    if ($_ -match $card) {
        # One line for each new text of the card.
        $text = $Matches[2] -replace '87\d\d Belief Left', 'N Belief Left'
        if (-not $cards.ContainsKey($text)) { $cards[$text] = 1; "card @$($Matches[1]): $text" }
    } else {
        $_.Substring(0, [Math]::Min(440, $_.Length))
    }
} | Select-Object -First $Lines
