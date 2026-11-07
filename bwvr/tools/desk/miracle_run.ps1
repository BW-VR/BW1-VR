# Desktop runs of a miracle in the Hand. See bwvr/docs/miracles.md.
#
#   miracle_run.ps1 -Name x1 -Dll <dll> -Kind creature -Set "miracle_hand=0"
#
#   miracle_run.ps1 -Name x2 -Dll <dll> -Kind throw -NewGame
#
# THE START. The run starts in the test save "temple" (README.md, "A fast
# start"): no story, and the Creature of the profile at the door of the
# Temple. When the save is not there, one run makes it. The steps and the
# frames of miracle_test_seeds count from frame 800 as the frame at which the
# controller first places the Hand in the loaded game
# (fake_controller_play_frame=800), so each frame in the log is about 730
# less than the frame of the step. -NewGame gives a new game with the choice
# "Keep your old Creature" (spawn_run.ps1): the same state. -Save gives a
# different test save with that state. With no -Seconds, a new game runs 75 s,
# and a run in a test save ends 4 s after its last step.
#
# The test key miracle_test_seeds makes one-off seeds on the land under the
# Hand.
#
# -Kind creature: a seed of Creature Strong (16). A short press of the grip
#   makes it active. A press over bare land, a press with the aim on the
#   Creature from a distance, a shake (5 steps of the word "pull"), a second
#   seed, a press with the Hand on the Creature, and a short press of B.
# -Kind throw: a Fireball (2): hold the grip, a step of the hand, open. Then
#   Water (9): hold the grip for 90 frames, and a tap of B. Water stays in
#   the Hand after a pour, and the tap takes it out. Then Heal (7): a press.
#   Then a Creature seed that the Hand carries, and the trigger.
# The other kinds (cancel, place, shield, away, fire, firelong, pshield,
# teleport, storm) have a comment at their steps below. -Seeds gives other
# seed types to the steps of a kind (the types are in bwvr/docs/miracles.md),
# and -Steps gives other steps.
param(
    [Parameter(Mandatory = $true)][string]$Name,
    [Parameter(Mandatory = $true)][string]$Dll,
    [string]$Kind = 'creature',
    [string]$Set = '',
    [string]$Steps = '',
    [string]$Seeds = '',
    [int]$Seconds = 0,
    [string]$Save = 'temple',
    [switch]$NewGame
)
$sp = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $sp 'game.ps1')
$start = ''
if (-not $NewGame) { $start = Confirm-BwvrTestSave $Save $Dll }
# The zoom steps of these recipes keep the land under the head in its place
# (god_zoom_about=0).
$keys = 'god_zoom_about=0;far_test_creature=1;far_test_influence=1;grab_hand=1;far_grab_max_m=0'
$points = '0,-30,60 14,-32,56 -12,-31,58 10,-30,62 -6,-32,54 4,-31,64 0,-30,60 12,-31,58'
# The longest run in a test save, in seconds.
$most = 65
if ($Kind -eq 'away') {
    # Enlarge (14) and then Shrink (13) on the Creature away from the pen of
    # the Temple. At 100 units a meter: the grip from a distance puts the
    # leash on, and a double press of the trigger on a lantern (the word
    # "aimcmd") ties the leash to it. The Creature walks about 40 units to
    # the lantern, out of the 16 m of the game in front of the pen in which
    # the game draws the body at the size 0.22. Then each seed: a tap makes
    # it active, and a press with the aim on the Creature from a distance
    # casts it. The lines "miracle watch" give the metres from the point at
    # the pen and the size of the drawn body.
    if ($Seeds -eq '') { $Seeds = '14@1800 13@2500' }
    $most = 75
    if ($Steps -eq '') {
        $Steps = 'look@800+300 point@860+1 aimfar@890+1 grip@910+6 aimcmd@1000+1 shot@1010+1 ' +
                 'trigger@1020+6 trigger@1037+6 shot@1700+1 ' +
                 'seek@1830+1 grip@1870+3 aimfar@2000+1 grip@2040+10 shot@2060+1 shot@2200+1 shot@2300+1 ' +
                 'point@2480+1 seek@2530+1 grip@2570+3 aimfar@2650+1 grip@2690+10 shot@2710+1 shot@2850+1'
    }
} elseif ($Kind -eq 'fire') {
    # A Fireball (2) on a house, then Water (9) on the fire. At 100 units a
    # meter the God view goes before the Village Store (the place "gostore"
    # of seek_plan), near the round house at 1804.7 2651.1 of the test save
    # "temple". The head looks 25 degrees down, so the seed under the
    # Hand is in the view. The tap makes the seed active, the Hand's point
    # goes 4 cm of the room into the house at 150 percent of its height (over
    # the roof, the place "feel" of seek_feel), and a still grip of 40 frames
    # opens there: the ball falls onto the house. The hand then goes back to
    # a point before the head, so the hand does not hide the house in the eye
    # pictures of the fire. Then the Water seed: two seeks 60 and 80 frames
    # after the seed comes (it is not a candidate at once), a tap, the Hand
    # over the same house, a pour of 150 frames, and a tap of B. -Set
    # "seek_plan=go,hold,feel,hold,hold,feel;seek_go=x,z" gives a different
    # place.
    if ($Seeds -eq '') { $Seeds = '2@960 9@1600' }
    $most = 75
    $keys = "fake_head_pitch=-25;$keys;seek_plan=gostore,hold,feel,hold,hold,feel;" +
            'seek_class=OneOffSpellSeed,OneOffSpellSeed,OneOffSpellSeed;seek_feel=house,150,-4|house,150,-4'
    if ($Steps -eq '') {
        $Steps = 'point@900+1 seek@920+1 shot@950+1 seek@990+1 grip@1000+3 seek@1060+1 shot@1070+1 ' +
                 'grip@1100+40 point@1150+1 shot@1160+1 shot@1210+1 shot@1300+1 shot@1450+1 shot@1580+1 ' +
                 'seek@1660+1 seek@1680+1 grip@1690+3 seek@1740+1 grip@1780+150 shot@1840+1 shot@1920+1 ' +
                 'b@2000+5 point@2010+1 shot@2040+1 shot@2200+1 shot@2400+1 shot@2600+1'
    }
} elseif ($Kind -eq 'firelong') {
    # As -Kind fire, with a pour of 900 frames (15 s) on the burning house,
    # and eye pictures during the pour and after it. The lines "miracle
    # watch" give each drop, the temperature of the house, and its
    # temperature each second for 30 s after the last drop.
    if ($Seeds -eq '') { $Seeds = '2@960 9@1600' }
    $most = 100
    $keys = "fake_head_pitch=-25;$keys;seek_plan=gostore,hold,feel,hold,hold,feel;" +
            'seek_class=OneOffSpellSeed,OneOffSpellSeed,OneOffSpellSeed;seek_feel=house,150,-4|house,150,-4'
    if ($Steps -eq '') {
        $Steps = 'point@900+1 seek@920+1 shot@950+1 seek@990+1 grip@1000+3 seek@1060+1 shot@1070+1 ' +
                 'grip@1100+40 point@1150+1 shot@1160+1 shot@1300+1 shot@1580+1 ' +
                 'seek@1660+1 seek@1680+1 grip@1690+3 seek@1740+1 grip@1780+900 shot@1900+1 shot@2200+1 ' +
                 'shot@2500+1 shot@2660+1 b@2720+5 point@2730+1 shot@2760+1 shot@2900+1 shot@3100+1 shot@3400+1'
    }
} elseif ($Kind -eq 'pshield') {
    # A Physical Shield (5) with the circle (template 5, as -Kind shield),
    # and then an object thrown at the dome. At the door of the Temple no
    # rock is within 2 m of the room of the Hand, so the object is the
    # nearest pig (the place "hold" of seek_plan, seek_class). -Set
    # "seek_class=OneOffSpellSeed,Rock" takes a rock in a save that has one.
    # The Hand goes to the second point, 5 cm before the head, and jumps to
    # the third point in one frame as the grip opens: a throw of about 12 m/s
    # forward and down. At 44 units a meter the dome has its middle about 47
    # cm before the head, and the third point is outside it, so the object
    # flies at the dome. Eye pictures of the flight. The lines "miracle
    # watch" give the place and the radius of the shield, and the lines
    # "hand throw" the flight.
    if ($Seeds -eq '') { $Seeds = '5@930' }
    $keys = "$keys;fake_controller_gestures=5;seek_plan=hold,hold;seek_class=OneOffSpellSeed,Pig;throw_log=1"
    $points = '0,-30,60 0,0,5 0,-15,20'
    if ($Steps -eq '') {
        $Steps = 'stick_up@800+10 stick_up@830+10 look@860+5000 point@900+1 ' +
                 'seek@960+1 grip@1000+3 trace@1100+200 grip@1106+150 shot@1300+1 ' +
                 'seek@1400+1 grip@1440+120 point@1500+1 point@1559+1 shot@1565+1 shot@1580+1 ' +
                 'shot@1600+1 shot@1630+1 shot@1700+1 shot@1850+1'
    }
} elseif ($Kind -eq 'teleport') {
    # Two Teleport casts (28) at two places, and a villager that goes in at
    # one site and comes out at the other. At 100 units a meter the God view
    # goes to 1827 2537, before the door of the Temple (the place "go" of
    # seek_plan, seek_go). The head looks 25 degrees down, so a seed under
    # the Hand is in the view. A tap makes the first seed active, the hand
    # goes back to its point, and a press casts the miracle at the land point
    # under the Hand: site A. Then the hand goes 35 cm nearer to the head,
    # the second seed comes under it, and a tap and a press there make site
    # B, about 35 units from site A. (At the Village Store the game refuses
    # the cast.) Then the God view goes before the Village Store (the place
    # "gostore"), and the Hand takes the nearest villager that the game can
    # give a job (the place "villager"). The God view goes back to 1827 2537
    # with the villager in the Hand, the hand goes 10 cm under its first
    # point, over site A, and the grip opens: the Hand puts the villager on
    # the site (miracle_site_pick=1), and the game sends him to site B. The
    # lines "miracle watch" give the sites, the teleport and the place of the
    # villager after it. Eye pictures of the two sites and of the villager.
    if ($Seeds -eq '') { $Seeds = '28@960 28@1220' }
    $most = 75
    $keys = "fake_head_pitch=-25;$keys;seek_plan=go,hold,hold,gostore,villager,go;" +
            'seek_go=1827,2537|1827,2537;seek_class=OneOffSpellSeed,OneOffSpellSeed'
    $points = '0,-30,60 0,-30,60 0,-30,25 0,-30,25 0,-40,60'
    if ($Steps -eq '') {
        $Steps = 'point@900+1 seek@920+1 seek@1000+1 grip@1010+3 point@1080+1 grip@1100+10 shot@1130+1 ' +
                 'point@1180+1 seek@1260+1 grip@1270+3 point@1340+1 grip@1360+10 shot@1390+1 ' +
                 'seek@1440+1 seek@1490+1 grip@1500+300 seek@1570+1 point@1620+1 shot@1700+1 ' +
                 'shot@1810+1 shot@1840+1 shot@1900+1 shot@2000+1 shot@2150+1'
    }
} elseif ($Kind -eq 'storm') {
    # A Storm (0) with the circle (template 5), for the look of its cloud
    # (miracles.md, "The cloud of a Storm"). The grip stays closed while the
    # hand steps through the circle, and the game casts when the circle
    # closes. The test key sky_test_mist_log gives a line for each mist and
    # each sky cloud, with its place, its size and its flatten. Eye pictures
    # before the cast (frame 1050) and after it. -Set "cloud_round_near=0"
    # gives the flatten of the engine for each sky cloud, and -Set
    # "sky_test_mist_hide=1560;sky_test_mist_hide_all=1640" takes the mists
    # and then the sky clouds out of the picture from those frames. Give it
    # the dxwrapper renderer for the pictures.
    if ($Seeds -eq '') { $Seeds = '0@930' }
    $keys = "$keys;fake_controller_gestures=5;sky_test_mist_log=1"
    if ($Steps -eq '') {
        $Steps = 'stick_up@800+10 stick_up@830+10 look@860+5000 point@900+1 ' +
                 'seek@960+1 grip@1000+3 shot@1050+1 trace@1100+200 grip@1106+150 point@1300+1 ' +
                 'shot@1330+1 shot@1400+1 shot@1480+1 shot@1540+1 shot@1580+1 shot@1620+1 shot@1660+1 shot@1720+1'
    }
} elseif ($Kind -eq 'throw') {
    if ($Seeds -eq '') { $Seeds = '2@930 9@1330 7@1730 16@2030' }
    if ($Steps -eq '') {
        $Steps = 'stick_up@800+10 stick_up@830+10 look@860+5000 point@900+1 ' +
                 'seek@960+1 grip@1000+3 point@1080+1 grip@1120+40 pull@1156+3 ' +
                 'pull@1230+1 point@1300+1 seek@1360+1 grip@1400+3 point@1480+1 grip@1520+90 b@1640+5 ' +
                 'point@1700+1 seek@1760+1 grip@1800+3 point@1880+1 grip@1920+10 ' +
                 'point@2000+1 seek@2060+1 grip@2100+260 trigger@2200+6 shot@2420+1'
    }
} elseif ($Kind -eq 'cancel') {
    # A shake of a new miracle, the button on a new miracle, and a shake while
    # the spell of a cast miracle runs.
    if ($Seeds -eq '') { $Seeds = '16@930 16@1250 16@1560' }
    if ($Steps -eq '') {
        $Steps = 'stick_up@800+10 stick_up@830+10 look@860+5000 point@900+1 ' +
                 'seek@960+1 grip@1000+3 ' +
                 'pull@1100+1 pull@1106+1 pull@1112+1 pull@1118+1 pull@1124+1 pull@1130+1 ' +
                 'seek@1300+1 grip@1340+3 b@1440+5 ' +
                 'seek@1610+1 grip@1650+3 aimfar@1720+1 grip@1760+10 ' +
                 'pull@1800+1 pull@1806+1 pull@1812+1 pull@1818+1 pull@1824+1 pull@1830+1 ' +
                 'shot@1980+1'
    }
} elseif ($Kind -eq 'place') {
    # Heal (7): a press casts it at the place of the Hand. Then a Creature
    # seed that the grip carries: the trigger makes it active, and a press
    # with the Hand on the Creature casts it.
    if ($Seeds -eq '') { $Seeds = '7@930 16@1330' }
    if ($Steps -eq '') {
        $Steps = 'stick_up@800+10 stick_up@830+10 look@860+5000 point@900+1 ' +
                 'seek@960+1 grip@1000+3 point@1080+1 grip@1120+10 ' +
                 'point@1300+1 seek@1360+1 grip@1400+260 trigger@1500+6 ' +
                 'touchc@1700+1 grip@1740+10 shot@1900+1'
    }
} elseif ($Kind -eq 'shield') {
    # A shield (4): the grip stays closed while the hand steps through the
    # circle of Gestures.jty (template 5), and opens. Then a storm (0).
    if ($Seeds -eq '') { $Seeds = '4@930 0@1500' }
    $keys = "$keys;fake_controller_gestures=5,5"
    if ($Steps -eq '') {
        $Steps = 'stick_up@800+10 stick_up@830+10 look@860+5000 point@900+1 ' +
                 'seek@960+1 grip@1000+3 trace@1100+200 grip@1106+150 ' +
                 'point@1470+1 seek@1540+1 grip@1580+3 trace@1680+200 grip@1686+150 ' +
                 'b@1980+5 shot@2060+1'
    }
} else {
    if ($Seeds -eq '') { $Seeds = '16@930 16@1700' }
    if ($Steps -eq '') {
        $Steps = 'stick_up@800+10 stick_up@830+10 look@860+5000 point@900+1 ' +
                 'seek@960+1 grip@1000+3 point@1080+1 grip@1120+10 ' +
                 'aimfar@1200+1 grip@1240+10 ' +
                 'pull@1500+1 pull@1506+1 pull@1512+1 pull@1518+1 pull@1524+1 pull@1530+1 ' +
                 'point@1660+1 seek@1740+1 grip@1780+3 touchc@1860+1 grip@1900+10 ' +
                 'b@2100+5 shot@2200+1'
    }
}
$keys = "$keys;miracle_test_seeds=$Seeds"
if ($start) {
    # In a test save the steps and the frames of the seeds count from the
    # frame at which the controller first places the Hand, as frame 800. The
    # start is 10 s shorter than the start of a new game.
    $keys = "fake_controller_play_frame=800;$keys"
    if ($Seconds -le 0) { $Seconds = Get-BwvrSaveSeconds $Steps 800 $most }
} elseif ($Seconds -le 0) {
    $Seconds = 75
}
if ($Set -ne '') { $keys = "$keys;$Set" }
& (Join-Path $sp 'spawn_run.ps1') -Name $Name -Dll $Dll -Choice keep -Steps $Steps `
    -Points $points -Set $keys -Seconds $Seconds -Save $start
# spawn_run.ps1 gave up: no run, so no log of this run to read.
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$log = Join-Path $sp "desk-$Name\bwvr.log"
if (-not (Test-Path $log)) { "no log at $log"; exit 1 }
$pattern = '^miracle      :|^sky mist     :|^sky cloud    :|^sky          : (sky_test|cloud_round|the test keys|.*near sky cloud)|^hand press   : the grip pressed|^haptics      : (effect (14|18|19|20|21|23|24|25|26|27|28)|cast|refused)|' +
           '^hint card    : frame .*(iracle|ancel|Cast|pour|throw|Use)|^hand         : state|^miracle watch: |' +
           '^hand pick    : seek plan|^creature cmd : (the leash|a double press|test: aimcmd)|' +
           '^far grab    : (the grip closed|the grip opened)|^hand throw   : release'
Get-Content $log | Where-Object { $_ -match $pattern } | ForEach-Object {
    $_.Substring(0, [Math]::Min(420, $_.Length))
} | Select-Object -First 220
