<#
.SYNOPSIS
    Desktop runs of the ring, its sheets and the aim of a new bookmark. See
    bwvr/docs/controllers.md, section 10.

.DESCRIPTION
    ring_run.ps1 -Name rg1 -Dll <dll> -Kind pages
    ring_run.ps1 -Name rg2 -Dll <dll> -Kind places
    ring_run.ps1 -Name rg3 -Dll <dll> -Kind leash

    Each run is a run of temple_run.ps1 -NoEnter in the test save "temple":
    the God view at the door of the Temple. Y holds the ring open, and the
    left hand steps to the items and to the tiles of a sheet. The place of
    a tile is in the plane of the ring, from the middle of the sheet, as the
    log line "the ring goes to the sheet" gives it.

    pages   page 1: the hand goes over four items and back to the middle
            (no choice), More, then "names" two times.
    places  the sheet of the places: the plus and a grip on the land, the
            plus and "aimmark" with a grip on a living thing, the seal of
            bookmark 1, the bin of bookmark 2, and the plus with B. After
            each grip the lines "bookmark N holds" give the record of the
            bookmark 2, 120 and 300 frames later. Give -More "far_grab=0" or
            "far_grab=1" to fix the far grab of the run.
    leash   the sheet of the leashes: the hand over each tile, then a
            choice of Compassion.

    The steps assume the test save has no bookmark set at the start. The
    log line "the ring goes to the sheet" lists the tiles of each sheet.
#>
param(
    [Parameter(Mandatory = $true)][string]$Name,
    [Parameter(Mandatory = $true)][string]$Dll,
    [ValidateSet('pages', 'places', 'leash')][string]$Kind = 'pages',
    [string]$More = ''
)
$desk = $PSScriptRoot
. (Join-Path $desk 'game.ps1')

# The rest of the left hand, in cm (x right, y up, z forward), as
# Get-BwvrRingChoice has it.
$rest = @(-20, -30, 35)

# A point of the plane of the ring that opens at the rest: x cm to the right
# and y cm up, as the player sees the ring.
function PlanePoint([double]$X, [double]$Y) {
    $h = @([double]$rest[0], [double]$rest[1], -[double]$rest[2])
    $len = [Math]::Sqrt($h[0] * $h[0] + $h[1] * $h[1] + $h[2] * $h[2])
    $n = @((-$h[0] / $len), (-$h[1] / $len), (-$h[2] / $len))
    $flat = [Math]::Sqrt($n[0] * $n[0] + $n[2] * $n[2])
    $right = @(($n[2] / $flat), 0.0, (-$n[0] / $flat))
    $up = @(($n[1] * $right[2] - $n[2] * $right[1]), ($n[2] * $right[0] - $n[0] * $right[2]), ($n[0] * $right[1] - $n[1] * $right[0]))
    $p = @(0..2 | ForEach-Object { $h[$_] + $X * $right[$_] + $Y * $up[$_] })
    return '{0},{1},{2}' -f [int][Math]::Round($p[0]), [int][Math]::Round($p[1]), [int][Math]::Round(-$p[2])
}

# The point of slot s of the ring, 10 cm from the middle.
function SlotXY([int]$Slot) {
    $t = $Slot * [Math]::PI / 4
    return @((10.0 * [Math]::Sin($t)), (10.0 * [Math]::Cos($t)))
}

$restPoint = '{0},{1},{2}' -f $rest[0], $rest[1], $rest[2]
$script:steps = @()
$script:points = @()

# One open of the ring at frame F: Y down for H frames, and the hand to each
# point of the list, one each `Gap` frames from F + 6. Then the hand rests.
function Open([int]$F, [int]$H, [string[]]$To, [int]$Gap = 30) {
    $script:steps += "y@$F+$H"
    $at = $F + 6
    foreach ($p in $To) {
        $script:steps += "lpoint@$at+1"
        $script:points += $p
        $at += $Gap
    }
    $script:steps += "lpoint@$($F + $H + 6)+1"
    $script:points += $restPoint
}

# One choice on a sheet: the hand to the item of the sheet (slot s of page
# 1), a dwell of 0.45 s, then the hand to the tiles x y (cm from the middle
# of the sheet). The release chooses the last one.
function Sheet([int]$F, [int]$Slot, [double[]]$Tiles, [int]$Gap = 30) {
    $s = SlotXY $Slot
    $to = @(PlanePoint $s[0] $s[1])
    for ($i = 0; $i -lt $Tiles.Count; $i += 2) {
        $to += PlanePoint ($s[0] + $Tiles[$i]) ($s[1] + $Tiles[$i + 1])
    }
    # The first step to a tile waits for the dwell: 50 frames after the open.
    $script:steps += "y@$F+$(50 + $Gap * ($to.Count - 1))"
    $script:steps += "lpoint@$($F + 6)+1"
    $script:points += $to[0]
    $at = $F + 50
    for ($i = 1; $i -lt $to.Count; ++$i) {
        $script:steps += "lpoint@$at+1"
        $script:points += $to[$i]
        $at += $Gap
    }
    $script:steps += "lpoint@$($at + 6)+1"
    $script:points += $restPoint
    return $at
}

# THE LIMIT: the left hand of the scripted controller keeps 16 points, so a
# run has at most 16 steps of the hand. Quick save is off on page 1 of each
# run, so a step that misses never saves the game.
$base = 'backend=dxwrapper\ddraw.dll;test_frame_rate=60;aim_preset=0;radial_test_save=60;' +
        'radial_menu_items=leash,places,recenter,none,more,settings,names,pause'
switch ($Kind) {
    'pages' {
        # Round page 1 with no choice, then More to page 2.
        $round = @(0, 2, 5, 7 | ForEach-Object { $s = SlotXY $_; PlanePoint $s[0] $s[1] })
        Open 900 200 ($round + @((PlanePoint 0 1))) 20
        $script:steps += 'shot@960+1'
        $s = SlotXY 4
        Open 1250 120 @((PlanePoint $s[0] $s[1]), (PlanePoint 0 1)) 60
        # Names on and off.
        foreach ($f in 1500, 1650) {
            $s = SlotXY 6
            Open $f 30 @((PlanePoint $s[0] $s[1]))
        }
        $set = $base
        $seconds = 45
    }
    'places' {
        # 1. The plus, then a grip on the land in front.
        $end = Sheet 900 1 @(0, -5.6) 40
        $script:steps += "shot@$($end - 10)+1 shot@$($end + 40)+1 grip@$($end + 70)+10"
        # 2. Bookmark 1 is set: the plus is at 1.9 -5.6. aimmark turns the
        #    ray to a living thing, and the grip sets bookmark 2 on it.
        $end = Sheet 1200 1 @(1.9, -5.6) 40
        $script:steps += "aimmark@$($end + 30)+1 shot@$($end + 50)+1 grip@$($end + 70)+10"
        # 4. The seal of bookmark 1 (bookmarks 1 and 2 set: -3.9 -5.6).
        $end = Sheet 1750 1 @(-3.9, -5.6) 40
        $script:steps += "shot@$($end + 60)+1"
        # 5. The bin of bookmark 2 (0.0 -9.2).
        $end = Sheet 2000 1 @(0.0, -9.2) 40
        # 6. The plus (bookmark 1 is set: 1.9 -5.6), then B stops the aim.
        $end = Sheet 2250 1 @(1.9, -5.6) 40
        $script:steps += "b@$($end + 40)+6"
        $set = "$base;bookmark_test_aim=living"
        $seconds = 60
    }
    'leash' {
        # Over each tile, then a release on Compassion.
        $end = Sheet 900 0 @(9.4, 7.8, 0.0, 7.8, -9.4, 7.8) 40
        $script:steps += "shot@$($end - 20)+1"
        $set = $base
        $seconds = 35
    }
}
if ($More -ne '') { $set = "$set;$More" }
$set = "$set;fake_controller_left_points=$($script:points -join ' ')"
"steps: $($script:steps -join ' ')"
"set: $set"
& (Join-Path $desk 'temple_run.ps1') -Name $Name -Dll $Dll -NoEnter -Steps ($script:steps -join ' ') -Set $set -Seconds $seconds `
    -Pattern '^(ring  |bookmark aim|far grab    : (far_grab=0|on\.)|hint card|eye capture|god view|controllers  : the (fake (hand aims|trigger|y|grip)|word aim)|game start|hooks        : comfort)'
