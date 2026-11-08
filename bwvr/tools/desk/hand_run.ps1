# Waits for an idle PC (10 minutes, no game), then one desktop run of the
# given DLL through desk_run.ps1. Gives up after -Minutes.
#
#   hand_run.ps1 -Name hs1 -Dll <dll> -Kind one -Seconds 163
#   hand_run.ps1 -Name hs2 -Dll <dll> -Kind old -Seconds 163
#
# -Kind chooses the script:
#   one       One target: the air, the scroll and the crown, each with
#             near_rule 1 and then 2 (near_test_flips), at the store of Land
#             1. With near_rule 2 the grip at the scroll takes the rock beside
#             it: the Tug state, the Holding state and a set down.
#   old       The start "Keep your old Creature". Four times: the grip on the
#             Creature (the Creature state), four strokes and two slaps.
#   fixed     The places from -Points (cm from the head), with no seek, so a
#             DLL of an older build runs the same test. -Steps gives the
#             script after the start dialog. With no -Points and no -Steps
#             the run only closes the start dialog. A DLL of a build that
#             has no test save needs -NewGame.
#   near      The hand 20, 10, 7 and 5 cm in front of the head, with an eye
#             picture at each place (near_plane_cm). Give it
#             -Set "backend=dxwrapper\ddraw.dll" for eye pictures.
#   holdland  The grip of the other hand holds the land for 250 frames with
#             both hands still, and then for 250 frames with one step of
#             10 cm of the other hand. The lines "grab" of each report
#             window count the frames in which the camera moved.
#   left      left_hand=1: the left controller is the Hand. The points are
#             the mirror of the points of a right hand. The Hand goes 3 cm
#             over open land and the grip closes there. Then the menu button
#             of the right controller and of the left controller are pressed
#             (only the left one sends Esc).
#   glow      hand_glow=1 and hand_glow_idle=1: the Hand's point 3 cm over
#             open land, 17 cm in the land with hand_land_stop=0 (the land
#             hides the glow), back over the land, and at the nearest rock,
#             with an eye picture at each place.
#   forest    forest_take=1: the fetch of one tree from a forest at the store
#             of Land 1, then the aim away from the forest with the grip
#             still closed.
#   zone      The walk with the stick out of the camera zone of the
#             Creature select, and back.
#   mousepan  The test mouse of the mod drags the land, with no controller.
#             Give it -Save temple.
#   hints     The controller in the hand (hint_style=2, controller-hints.md,
#             9.14): both hands 40 cm ahead of the head, 25 cm down and
#             18 cm to each side, the grips turned 60 degrees up and 10
#             degrees in, as a player holds them. The head looks 25 degrees down, so
#             both controllers are in the view over the land. The land
#             prompt gives the cards of both hands (with prompts=2). Eye
#             pictures before and after the cards come, and with the grip
#             of the left hand held (the lit grip moves). Give it
#             -Set "backend=dxwrapper\ddraw.dll;test_frame_rate=60" for eye
#             pictures, or "video_view=1;test_frame_rate=60" with Vulkan for
#             pictures of the view for videos with the cards in them.
#
# THE START. The run starts in a test save (README.md, "A fast start"): the
# save "pen" for the kinds one and fixed (the Creature pens of Land 1 after
# the first story), and the save "temple" for the kind old (no story,
# the Creature of the profile at the door of the Temple). When the save is not
# there, one run makes it. In a test save the steps count from the frame at
# which the controller first places the Hand: that frame is frame 4600 of
# the steps (frame 700 for the kind old), and near_test_flips counts the same
# way. So each frame in the log is less than the frame of the step. -NewGame
# gives a new game: the row "Skip straight to the Creature select" with the
# first story, or "Keep your old Creature" for the kind old. The steps then
# have their own frames. -Save gives a different test save with the state.
#
# -Seconds: the length of the run. With no -Seconds, a new game runs 163 s,
# and a run in a test save ends 4 s after its last step.
param(
    [Parameter(Mandatory = $true)][string]$Name,
    [Parameter(Mandatory = $true)][string]$Dll,
    [string]$Kind = 'one',
    [string]$Set = '',
    [string]$Steps = '',
    [string]$Points = '',
    [string]$Box = '',
    [int]$Seconds = 0,
    [int]$Minutes = 600,
    [string]$Save = '',
    [switch]$NewGame
)
$sp = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $sp 'game.ps1')
if ($Save -eq '') { $Save = if ($Kind -eq 'old') { 'temple' } else { 'pen' } }
$start = ''
if (-not $NewGame) { $start = Confirm-BwvrTestSave $Save $Dll $Minutes }
# The frame of the steps that is the frame at which the controller first
# places the Hand in a test save.
$play = if ($Kind -eq 'old') { 700 } else { 4600 }
# The steps that close the start dialog.
$dialog = 'point@400+1 trigger@440+8 pose2@500+1 point@520+1 trigger@560+8'
# The two dialog points depend on the render size. dialog.ps1 measures a
# new size one time and gives them. For -Kind old it also gives the point of
# the fourth box. -Box gives a different point for that box.
. (Join-Path $sp 'dialog.ps1')
# A test save has no start dialog. The game opens its Main Menu over the
# loaded game, and Get-BwvrMenuStart gives the words, the one point and the
# keys that get the run through it. Only a click on "Continue Game" needs the
# measure of the render size.
$menu = $null
if ($start) { $menu = Get-BwvrMenuStart $Dll }
$d1 = ''; $d2 = ''
if (-not $menu -or $menu.Measure) {
    if (-not (Confirm-DialogCalibration -Dll $Dll)) {
        'NOT RUN: the start dialog is not measured for this render size'
        exit 9
    }
    $dp = Get-DialogPoints 'select'
    $d1 = $dp.Row; $d2 = $dp.Ok
}
# The points that the dialog steps use: the row and OK of the start dialog,
# or the one point of the Main Menu.
$first = "$d1 $d2"
if ($start) {
    if ($menu.Measure) { $menu.Point = Get-MenuContinuePoint }
    $dialog = $menu.Words
    $first = $menu.Point
}
$extra = @{
    play_mode = 4; fake_controller = 2; controller_hand = 1; intro_vr = 0
    fake_controller_yaw = 8; fake_controller_pitch = 1
    fake_controller_sweep_yaw = 8; fake_controller_sweep_pitch = -6
    test_keys = 1; far_test_influence = 1; far_grab = 0; grab_hand = 1
    throw_log = 1; throw_law = 2; depth24 = 1; help_test = 1; fake_controller_front = 1
    # The zoom steps of these recipes keep the land under the head in its
    # place.
    god_zoom_about = 0
}
if ($Kind -eq 'one') {
    # The first story ends before frame 4600, so the script starts there.
    # First to the store of Land 1: four turns
    # right and six zoom steps out.
    $s = '-16,-50,55'
    $extra.fake_controller_points = "$first $s $s $s $s $s $s $s"
    $extra.seek_plan = 'air,air,scroll,scroll,crown,crown'
    $extra.near_test_flips = '5055,5190,5315,5440,5575'
    # The head looks 25 degrees down. The places of the seeks are on the land
    # low in a level view, and the view of a render of 3600 by 2880 ends 48.8
    # degrees below its middle (55 degrees at 2208 by 2208). The controller
    # code drops a press that starts off view, and the draw does not offer an
    # object that the render does not show.
    $extra.fake_head_pitch = -25
    $store = 'stick_right@4600+8 stick_right@4630+8 stick_right@4660+8 stick_right@4690+8 ' +
             'stick_down@4720+10 stick_down@4750+10 stick_down@4780+10 stick_down@4810+10 stick_down@4840+10 stick_down@4870+10'
    $Steps = $store + ' point@4900+1 ' +
             'seek@4930+1 grip@4980+40 shot@5005+1 point@5050+1 ' +
             'seek@5065+1 grip@5115+40 shot@5140+1 point@5185+1 ' +
             'seek@5200+1 shot@5240+1 grip@5250+30 point@5310+1 ' +
             'seek@5325+1 shot@5365+1 grip@5375+30 point@5435+1 ' +
             'seek@5450+1 grip@5500+40 shot@5525+1 point@5570+1 ' +
             'seek@5585+1 grip@5635+40 shot@5660+1 point@5710+1'
} elseif ($Kind -eq 'old') {
    # A new game with "keep your old Creature" (the fourth box of the start
    # dialog, -Box gives its point). The head looks down, one zoom step in,
    # and seek_plan=creature puts the hand on the body bone of the nearest
    # Creature in view. Four times: the grip held with four strokes (nudge),
    # a slap of the wrist (rot out and in) and a slap of the arm.
    $extra.fake_controller_rots = '-27,1,0 8,1,0'
    $extra.seek_plan = 'creature'
    if ($start) {
        $extra.fake_controller_points = "$first 0,-30,60"
    } else {
        if ($Box -eq '') { $Box = (Get-DialogPoints 'keep').Row }
        $extra.fake_controller_points = "$Box $d2 0,-30,60"
        $dialog = 'shot@300+1 point@400+1 trigger@440+8 shot@480+1 pose2@500+1 point@520+1 trigger@560+8'
    }
    $Steps = 'look@700+30000 stick_up@720+10 point@1100+1'
    foreach ($b in 1200, 2400, 3600, 5000) {
        $Steps += " seek@$($b)+1 grip@$($b + 50)+700 nudge@$($b + 150)+1 nudge@$($b + 190)+1" +
                  " nudge@$($b + 230)+1 nudge@$($b + 270)+1 seek@$($b + 300)+1" +
                  " rot@$($b + 340)+1 rot@$($b + 490)+1 slap@$($b + 560)+1 slap@$($b + 700)+1"
    }
} elseif ($Kind -eq 'near') {
    # The hand in front of the eyes, level, 20 to 5 cm from the middle of the
    # head. First six zoom steps in, to about 8.8 units for each meter: there
    # the near plane of the game (about 1 unit) is about 11 cm of the room,
    # so near_plane_cm acts. At 100 units for each meter the game's plane is
    # 1 cm, and the key does not change it. The log line "near plane" has
    # the plane that the mod used. Compare with -Set "near_plane_cm=0".
    $extra.fake_controller_points = "$first 0,-4,20 0,-3,10 0,-3,7 0,-2,5"
    $Steps = 'stick_up@4600+10 stick_up@4630+10 stick_up@4660+10 stick_up@4690+10 stick_up@4720+10 stick_up@4750+10 ' +
             'point@4800+1 shot@4830+1 point@4860+1 shot@4890+1 point@4920+1 shot@4950+1 ' +
             'point@4980+1 shot@5010+1'
} elseif ($Kind -eq 'holdland') {
    # The other hand holds the land. The hands do not move from frame 4650 to
    # 4900, so the camera must not move. Then a new grip, and one lpoint
    # step of 10 cm up in it, so the camera moves one time.
    $extra.fake_controller_points = "$first 16,-30,50"
    $extra.fake_controller_left_points = '-16,-30,50 -16,-20,50'
    $Steps = 'point@4600+1 lpoint@4600+1 lgrip@4650+250 shot@4700+1 shot@4890+1 ' +
             'lgrip@5000+250 lpoint@5100+1 shot@5200+1'
} elseif ($Kind -eq 'left') {
    # The words of the right hand drive the left controller (controllers.md,
    # 5.6). The word "menu" presses the menu button of the left controller,
    # and "lmenu" the menu button of the right controller. The first Esc
    # opens the game menu, and the second "menu" closes it.
    $extra.left_hand = 1
    $extra.hand_follow_game = 0
    $extra.fake_head_pitch = -25
    $extra.seek_plan = 'open'
    $extra.fake_controller_points = "$first -16,-30,45"
    $Steps = 'point@4600+1 shot@4630+1 seek@4660+1 shot@4690+1 grip@4720+60 shot@4760+1 ' +
             'point@4850+1 lmenu@4900+5 shot@4960+1 menu@5000+5 shot@5060+1 menu@5100+5 shot@5160+1'
} elseif ($Kind -eq 'hints') {
    # The hands as a player holds the controllers: 40 cm ahead, 25 cm down,
    # 18 cm to each side. The -z axis of a grip goes through the closed
    # fingers from the little finger to the thumb, so in a natural hold it
    # points up and ahead, and the forearm goes back from the hand. A grip at
    # a pitch of 0 has the forearm straight up. So the grips turn 60 degrees
    # up and 10 degrees in. The rots are yaw,pitch,roll (a positive yaw turns
    # left, a positive pitch turns up).
    # The land prompt needs 1.5 s of the land under the Hand, so the cards
    # show from about frame 4700 at 60 frames a second.
    $extra.fake_head_pitch = -25
    $extra.fake_controller_points = "$first 18,-25,40"
    $extra.fake_controller_left_points = '-18,-25,40'
    $extra.fake_controller_rots = '10,60,0'
    $extra.fake_controller_left_rots = '-10,60,0'
    $Steps = 'point@4600+1 lpoint@4600+1 rot@4600+1 lrot@4600+1 shot@4640+1 ' +
             'shot@4760+1 shot@4840+1 lgrip@4880+80 shot@4920+1 shot@5000+1'
} elseif ($Kind -eq 'glow') {
    # The glow at the Hand's point (hand-model.md, "The glow at the Hand's
    # point"). With hand_land_stop=0 the hand goes into the land, and the
    # land in front of the point hides the glow.
    $extra.hand_glow = 1
    $extra.hand_glow_idle = 1
    $extra.hand_land_stop = 0
    $extra.seek_step_cm = 20
    $extra.fake_head_pitch = -25
    $extra.seek_plan = 'open,down,up,hold'
    $extra.seek_class = 'Rock'
    $extra.fake_controller_points = "$first 0,-30,55"
    $Steps = 'point@4600+1 seek@4630+1 shot@4660+1 seek@4700+1 shot@4730+1 seek@4770+1 shot@4800+1 ' +
             'seek@4840+1 shot@4870+1'
} elseif ($Kind -eq 'forest') {
    # The take from a forest after a fetch of one tree (grab.md, "The take
    # from a forest"), with forest_take=1. First to the store of Land 1, as
    # for the kind one, where the trees are in forests. The hand goes up to
    # the height of the head, and the word "aimfar" aims at the nearest tree
    # (far_test_tree=1). The grip closes, and a pull fetches the tree. Then
    # the word "aimturn" turns the aim 70 degrees away from the forest, and
    # the grip stays closed: no second tree is in the cone for 300 ms, so the
    # tree stays one tree and no bundle starts. The lines "forest take" say
    # so. Compare with -Set "forest_take=0".
    $s = '-16,-50,55'
    $extra.fake_controller_points = "$first $s 0,-5,40"
    $extra.fake_head_pitch = -25
    $extra.far_grab = 1
    $extra.far_test_tree = 1
    $extra.far_test_no_trees = 0
    $extra.far_grab_max_m = 0
    $extra.forest_take = 1
    $extra.fake_controller_turns = '70,0,0'
    $store = 'stick_right@4600+8 stick_right@4630+8 stick_right@4660+8 stick_right@4690+8 ' +
             'stick_down@4720+10 stick_down@4750+10 stick_down@4780+10 stick_down@4810+10 stick_down@4840+10 stick_down@4870+10'
    # The pull fetches the tree about 3 frames after it starts. The turn
    # comes 17 frames (280 ms) after the fetch, before the wait of 600 ms of
    # the take is over. A turn before the fetch ends the lock.
    $Steps = $store + ' point@4900+1 point@4920+1 aimfar@4950+1 shot@4960+1 grip@4990+200 pull@5020+3 ' +
             'aimturn@5040+1 shot@5050+1 shot@5100+1 shot@5180+1'
} elseif ($Kind -eq 'zone') {
    # The camera zone of the Creature select (god-view.md, section 17). The
    # stick of the other hand pushes forward for 8 s: the walk goes out of
    # the zone of the script, stops at god_zone_margin past it, and the push
    # past the limit asks the game for its sound of the block. Then a push
    # back for 3 s. The lines "camera zone" have the limit and the bumps.
    $extra.fake_controller_points = "$first 16,-30,50"
    $Steps = 'point@4600+1 shot@4640+1 lstick_up@4660+480 shot@5100+1 lstick_down@5200+180 shot@5420+1'
} elseif ($Kind -eq 'mousepan') {
    # The mouse pan (god-view.md, section 5.3) with the test mouse of the mod
    # and no controller. Give it -Save temple: the land at the door of the
    # Temple allows a drag. The pointer goes to 50 and 70 percent of the
    # render, the left button is down for 150 frames, and the pointer moves
    # 4 render pixels on each of 50 frames to the right. The lines "god view"
    # have the land that the mouse takes, the frames that the pan moved, the
    # worst distance of the grabbed land from the mouse ray, and no move after
    # the release. The words count the frames with a drawn scene from 0.
    # With a fake controller the controller owns the pointer, and the test
    # mouse does not act. So there is no controller, and the test mouse
    # clicks "Continue Game" of the Main Menu of the test save first (50 and
    # 45 percent of the render).
    $extra.fake_controller = 0
    $extra.controller_hand = 0
    $extra.fake_mouse_step_px = 4
    $extra.fake_mouse_x_pct = 50
    $extra.fake_mouse_y_pct = 45
    $extra.fake_mouse_script = 'home@150+1 button@170+4 home2@400+1 button@410+150 move_right@430+50'
    $Steps = ''
    if ($Seconds -le 0) { $Seconds = 20 }
} else {
    $extra.fake_controller_points = ("$first $Points").Trim()
}
$extra.fake_controller_script = ("$dialog $Steps").Trim()
$from = @{}
if ($start) {
    $extra.fake_controller_play_frame = $play
    foreach ($k in $menu.Ini.Keys) { $extra[$k] = $menu.Ini[$k] }
    $from.Save = $start
    if ($Seconds -le 0) { $Seconds = Get-BwvrSaveSeconds $Steps $play }
} elseif ($Seconds -le 0) {
    $Seconds = 163
}
foreach ($pair in $Set.Split(';')) {
    if ($pair -match '^\s*([^=\s]+)\s*=\s*(.*)$') { $extra[$Matches[1]] = $Matches[2] }
}
"script: $($extra.fake_controller_script.Length) characters, $(if ($start) { "the test save '$start'" } else { 'a new game' }), $Seconds s"
$deadline = (Get-Date).AddMinutes($Minutes)
while ((Get-Date) -lt $deadline) {
    $idle = [int](& (Join-Path $sp 'idle_seconds.ps1'))
    $running = [bool](Get-Process runblack -ErrorAction SilentlyContinue)
    if ($idle -ge 620 -and -not $running) {
        $o = & (Join-Path $sp 'desk_run.ps1') -Name $Name -Dll $Dll -Extra $extra -Seconds $Seconds @from
        $o
        if (-not (Test-BwvrVerdict $o 'NOT RUN')) { "RUN DONE $(Get-Date -Format s)"; exit 0 }
    }
    Start-Sleep -Seconds 30
}
"GAVE UP: the PC was not idle for 10 minutes in $Minutes minutes"
exit 2
