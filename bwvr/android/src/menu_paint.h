// The paint of the menu into the picture of the CPU (docs/android.md, "The
// menu"). The layout is menu_layout.h.
#pragma once

#include <string>

#include "canvas.h"
#include "menu_model.h"

// The whole menu: the frame, the title, the rows with the highlight on the
// cursor of the menu, the help of that row and the status line.
void PaintMenu(Canvas2D* canvas, const menu_model::Menu& menu,
               const std::string& status);

// The cursor of a controller ray: a small glow that fills the picture.
void PaintCursor(Canvas2D* canvas);

// The keyboard of the app (keyboard_layout.h) in the look of the menu: a
// panel with a button of the game for each key, and the key `lit` (-1: none)
// lit.
void PaintKeyboard(Canvas2D* canvas, int lit);
