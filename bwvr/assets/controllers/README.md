# The controller models of the hints

The mod shows these models in the hand of the player when a hint is active
(`hint_style=2`, `bwvr/docs/controller-hints.md`, "The controller in the
hand"). The installer and `dev.ps1 install` put this folder in
`bwvr-controllers` of the game folder. This README stays in the repo.

## Where the files come from

- Project: WebXR Input Profiles,
  https://github.com/immersive-web/webxr-input-profiles, folder
  `packages/assets`.
- Version: the npm package `@webxr-input-profiles/assets` 1.0.20. Its glb
  files are the same, byte for byte, as the glb files of the git commit
  `f4992299601614adbfefd398dc8e281556bb7444`. The git tree has only a stub of
  each `profile.json`, so the `profile.json` files and `profilesList.json` come
  from the npm package (`dist/profiles`).
- License: MIT, Copyright (c) 2019 Amazon. The text is in
  [`LICENSE.md`](LICENSE.md), a copy of `packages/assets/LICENSE.md`.
- The files are as the project ships them. The mod does not change them.

## Trademarks

The note of the assets README of the project, word for word:

> Please note: Some assets portray registered trademarks of the device
> manufacturers in the interest of accurately depicting the corresponding
> physical device. The license _does not_ grant permission to use these
> trademarks in derivative works (such as alternate controller skins), and
> they may not be used to endorse or promote products derived from this
> software without specific prior written permission.

So the mod shows a model only for the device that it shows, and never as art
of the mod. The mod reads the meshes and the nodes of each file and draws them
in its own colors. It does not read the textures, so no logo of a texture
shows.

## The profiles

| Folder | Controllers | Files |
| --- | --- | --- |
| `meta-quest-touch-plus` | Touch Plus (Quest 3, Quest 3S) | `left.glb`, `right.glb` |
| `meta-quest-touch-pro` | Touch Pro (Quest Pro) | `left.glb`, `right.glb` |
| `oculus-touch-v3` | Touch of Quest 2 | `left.glb`, `right.glb` |
| `oculus-touch-v2` | Touch of Quest and Rift S | `left.glb`, `right.glb` |
| `valve-index` | Index | `left.glb`, `right.glb` |
| `htc-vive` | Vive wand | `none.glb` for both hands |
| `htc-vive-cosmos` | Vive Cosmos | `left.glb`, `right.glb` |
| `htc-vive-focus-3` | Vive Focus 3 | `left.glb`, `right.glb` |
| `hp-mixed-reality` | HP Reverb G2 | `left.glb`, `right.glb` |
| `pico-4` | Pico 4 | `left.glb`, `right.glb` |
| `generic-trigger` | A controller with only a trigger (`khr/simple_controller`) | `left.glb`, `right.glb`, `none.glb` |

`hint_model_math.h` (`ModelOfProfile`) gives the folder for the interaction
profile and the name of the headset. An other profile has no model (also a Mixed Reality controller that is not
from HP), and the
cards keep the look of `hint_style=1`. `profilesList.json` lists all profiles
of the project, also the ones that are not in this folder.

The node names of each control follow the `profile.json` of each folder:
`<component>_pressed_value`, `_min` and `_max`, for example
`xr_standard_trigger_pressed_value`. The mod uses this rule and does not read
the `profile.json` files. They stay here as the source of the names.
