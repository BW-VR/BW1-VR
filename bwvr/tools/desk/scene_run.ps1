# Desktop runs of the camera of a miracle, the hands in the cut scene window
# and the influence at the hand. See bwvr/docs/miracles.md and
# bwvr/docs/cutscene-window.md.
#
#   scene_run.ps1 -Name sf1 -Dll <dll> -Kind forest
#   scene_run.ps1 -Name sf0 -Dll <dll> -Kind forest -Set "miracle_camera_still=0"
#   scene_run.ps1 -Name sw1 -Dll <dll> -Kind window -Set "backend=dxwrapper\ddraw.dll"
#   scene_run.ps1 -Name se1 -Dll <dll> -Kind edge -Set "backend=dxwrapper\ddraw.dll"
#
# THE START. The run starts in the test save "temple" (README.md, "A fast
# start"): no story, and the Creature of the profile at the door of the
# Temple. When the save is not there, one run makes it. The steps count from
# frame 800 as the frame at which the controller first places the Hand in the
# loaded game (fake_controller_play_frame=800). -NewGame gives a new game with
# the choice "Keep your old Creature" (spawn_run.ps1). The head does not turn
# and the view does not zoom in these recipes, and the hand points are in cm
# from the head, so no step depends on the render size.
#
# -Kind forest: the head looks 30 degrees to the left and 20 degrees down (the
#   word "look", as in miracle_run.ps1), a seed of the Forest miracle (type 1,
#   the test key miracle_test_seeds) on the land under the Hand, a take and a
#   press that casts it, then 25 s with no input while the forest grows. The log has the
#   lines "script camera" (the owner of the camera: CameraModePath is the
#   camera path of the miracle) and "god view" (the God view stands aside or
#   stays). With miracle_camera_still=0 the path plays as the game makes it.
# -Kind window: the test key cutscene_window_test_bars puts the cut scene
#   window up at frame 300 of the game. Then the hand steps to three points
#   in front of the head, with an eye picture and a press of the trigger at
#   each. The log has the lines "overlay" (the pixels of the hands and of the
#   end of the ray in each eye picture) and "controllers" (the pixel of the
#   ray, and the pointer of the game at each press). Give -Set
#   "cutscene_hands=1" for the hands: with the default 0 the window has no
#   hands and no ray, and a press goes to the pointer.
# -Kind edge: the test keys influence_test_circle_cm=25 and
#   influence_test_circle_frame=230 make the influence of the player a circle
#   of 25 cm of the room around the Hand's point at its first step. The hand
#   steps forward in steps of 5 cm to 40 cm past that point, with an eye
#   picture at each step, and back. So the Hand's point is 25, 20, 15, 10 and
#   5 cm inside the edge, at the edge, and 5, 10 and 15 cm past it, and then
#   5 cm inside again. The log has the lines "influence": the hand goes out
#   of the influence and comes back, and the glow at the edge shows, with its
#   distance and its strength each 60 frames, and goes. The report has the
#   state of the stream of the game. With influence_stream_hand=1 a stream of
#   the game ends at the Hand's point. influence_wall_test_light_pct=300
#   makes the glow three times as bright for the eye pictures.
#
# Give -Set "backend=dxwrapper\ddraw.dll" for eye pictures (the eye pictures
# of a run with the Vulkan renderer are black). With the Vulkan renderer give
# -Set "test_frame_rate=60" too, because the recipes count frames. -Steps
# gives other steps.
param(
    [Parameter(Mandatory = $true)][string]$Name,
    [Parameter(Mandatory = $true)][string]$Dll,
    [string]$Kind = 'forest',
    [string]$Set = '',
    [string]$Steps = '',
    [int]$Seconds = 0,
    [string]$Save = 'temple',
    [switch]$NewGame
)
$sp = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $sp 'game.ps1')
$start = ''
if (-not $NewGame) { $start = Confirm-BwvrTestSave $Save $Dll }
$keys = 'god_zoom_about=0;grab_hand=1;far_grab_max_m=0'
$points = '0,-30,60 14,-32,56 -12,-31,58'
$more = @()
if ($Kind -eq 'forest') {
    # The steps of miracle_run.ps1 -Kind place: two zoom steps in (44.4 units
    # for each meter), the look, the take of the seed and the press. With no
    # zoom steps the game refuses the cast at this place of the hand.
    $keys = "$keys;far_test_creature=1;far_test_influence=1;miracle_test_seeds=1@930"
    $points = '0,-30,60 14,-32,56 -12,-31,58 10,-30,62 -6,-32,54 4,-31,64 0,-30,60 12,-31,58'
    if ($Steps -eq '') {
        $Steps = 'stick_up@800+10 stick_up@830+10 look@860+5000 point@900+1 ' +
                 'seek@960+1 grip@1000+3 point@1080+1 grip@1120+10 ' +
                 'shot@1300+1 shot@1700+1 b@2600+5'
    }
    $pattern = '^script camera|^miracle      : .*(Forest|casts)|^god view     : .*(story|stands aside|your camera|goes to the game camera)|' +
               '^comfort      : the game (moved|put) its'
} elseif ($Kind -eq 'window') {
    # The window comes at frame 300 of the game, which is before the first
    # step: the play frame 800 is about frame 70 of the game.
    $keys = "$keys;cutscene_window_test_bars=300;cutscene_window_capture=1"
    $points = '0,-30,55 25,-15,50 -20,-5,45'
    if ($Steps -eq '') {
        $Steps = 'point@1100+1 shot@1200+1 trigger@1240+6 ' +
                 'point@1400+1 shot@1500+1 trigger@1540+6 ' +
                 'point@1700+1 shot@1800+1 trigger@1840+6'
    }
    $pattern = '^overlay      : the hands in the cut scene window|^controllers  : the (ray of the pointing hand|left button (presses|is down))|' +
               '^cut scene    : (frame \d+: (the first|the eye pictures|the cinema)|cutscene_hands)|^hand model   : the first'
} elseif ($Kind -eq 'edge') {
    # The circle starts at frame 230 of the controllers, about 60 frames
    # after the first step: its middle is the Hand's point at 20 cm in front
    # of the head. Each next step is 5 cm (5 units at the start scale), so
    # the Hand's point is 0 to 40 units from the middle, and then 20 again.
    # The grip is behind the Hand's point, so the fade (at the grip) comes
    # some steps after the edge.
    $keys = "$keys;influence_test_circle_cm=25;influence_test_circle_frame=230"
    $points = '0,-30,20 0,-30,25 0,-30,30 0,-30,35 0,-30,40 0,-30,45 0,-30,50 ' +
              '0,-30,55 0,-30,60 0,-30,40'
    if ($Steps -eq '') {
        $w = @()
        for ($k = 0; $k -lt 10; $k++) {
            $f = 900 + 120 * $k
            $w += "point@$f+1"
            $w += "shot@$($f + 60)+1"
        }
        $Steps = $w -join ' '
    }
    $pattern = '^influence    :|^hooks        : .*(AddManaPath|IsInPlayerRaw)'
} else {
    "no kind named $Kind"
    exit 7
}
if ($start) {
    $keys = "fake_controller_play_frame=800;$keys"
    if ($Seconds -le 0) { $Seconds = Get-BwvrSaveSeconds $Steps 800 0 $more }
    if ($Kind -eq 'forest' -and $Seconds -lt 70) { $Seconds = 70 }
} elseif ($Seconds -le 0) {
    $Seconds = 100
}
if ($Set -ne '') { $keys = "$keys;$Set" }
& (Join-Path $sp 'spawn_run.ps1') -Name $Name -Dll $Dll -Choice keep -Steps $Steps `
    -Points $points -Set $keys -Seconds $Seconds -Save $start
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$log = Join-Path $sp "desk-$Name\bwvr.log"
if (-not (Test-Path $log)) { "no log at $log"; exit 1 }
Get-Content $log | Where-Object { $_ -match $pattern } | ForEach-Object {
    $_.Substring(0, [Math]::Min(420, $_.Length))
} | Select-Object -First 200
