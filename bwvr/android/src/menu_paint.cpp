#include "menu_paint.h"

#include <vector>

#include "../../src/common/version.h"
#include "menu_layout.h"

namespace {

namespace ml = menu_layout;
using menu_model::Item;
using menu_model::ItemKind;

// THE LOOK OF THE MENU OF THE GAME (the Main Menu over a loaded game): a
// light panel that shows the world behind it, a thin light frame, tabs at
// its top left, a line of white text, and dark buttons with a light frame and
// white text in the middle. The text of the game has a dark edge, so it
// stays clear over the land and the sky.
constexpr uint32_t kPanel = 0x5CFFFFFFu;        // white, 36 percent
constexpr uint32_t kPanelEdge = 0xD8F4F0EAu;
constexpr uint32_t kPanelShade = 0x70000000u;
constexpr uint32_t kTab = 0x5CFFFFFFu;
constexpr uint32_t kTabBack = 0x30FFFFFFu;
constexpr uint32_t kButton = 0xE02A2321u;       // the dark brown of a button
constexpr uint32_t kButtonLit = 0xF0463A33u;
constexpr uint32_t kButtonEdge = 0xE8E8E2DAu;
constexpr uint32_t kButtonEdgeLit = 0xFFFFFFFFu;
constexpr uint32_t kButtonShade = 0xC0000000u;
constexpr uint32_t kText = 0xFFF4F1EDu;
constexpr uint32_t kTextDim = 0xFFD8D2CAu;
constexpr uint32_t kRowLit = 0x38FFFFFFu;

constexpr float kTabTop = 22;
constexpr float kPanelTop = 74;
constexpr float kButtonLeft = 330;
constexpr float kButtonRight = ml::kWidth - 330;

void Button(Canvas2D* c, float l, float t, float r, float b, bool lit) {
  c->FillRect(l - 2, t - 2, r + 2, b + 2, kButtonShade);
  c->FillRect(l, t, r, b, lit ? kButtonLit : kButton);
  c->StrokeRect(l + 1, t + 1, r - 1, b - 1, lit ? 3 : 2,
                lit ? kButtonEdgeLit : kButtonEdge);
}

// A tab at the top of the panel. The tab of the page is lighter and joins
// the panel.
float Tab(Canvas2D* c, float left, const std::string& text, bool on) {
  const float size = 30;
  const float w = c->Measure(text, size, false) + 70;
  const float bottom = kPanelTop + (on ? 2 : 0);
  c->FillRect(left, kTabTop, left + w, bottom, on ? kTab : kTabBack);
  c->StrokeRect(left + 1, kTabTop + 1, left + w - 1, bottom + 2, 2,
                on ? kPanelEdge : 0x90F4F0EAu);
  c->Text(text, left + w * 0.5f, kTabTop + 38, size, on ? kText : kTextDim,
          false, Canvas2D::kCenterAlign, true);
  return left + w + 10;
}

void Panel(Canvas2D* c, const menu_model::Menu& menu) {
  const float w = static_cast<float>(ml::kWidth);
  const float h = static_cast<float>(ml::kHeight);
  float x = 40;
  x = Tab(c, x, "Main Menu", menu.OnMain());
  if (!menu.OnMain()) Tab(c, x, menu.Title(), true);
  c->FillRect(14, kPanelTop, w - 14, h - 14, kPanel);
  c->StrokeRect(14, kPanelTop, w - 14, h - 14, 6, kPanelShade);
  c->StrokeRect(17, kPanelTop + 3, w - 17, h - 17, 3, kPanelEdge);
}

// The words of `text` in lines of at most `width` pixels.
std::vector<std::string> Wrap(Canvas2D* c, const std::string& text,
                              float size, float width) {
  std::vector<std::string> lines;
  std::string line;
  size_t i = 0;
  while (i < text.size()) {
    size_t j = text.find(' ', i);
    if (j == std::string::npos) j = text.size();
    const std::string word = text.substr(i, j - i);
    const std::string next = line.empty() ? word : line + " " + word;
    if (!line.empty() && c->Measure(next, size, false) > width) {
      lines.push_back(line);
      line = word;
    } else {
      line = next;
    }
    i = j + 1;
  }
  if (!line.empty()) lines.push_back(line);
  return lines;
}

void Slider(Canvas2D* c, float mid, double fraction, bool lit) {
  const float l = ml::kBarLeft, r = ml::kBarRight;
  c->FillRect(l - 2, mid - 10, r + 2, mid + 10, kButtonShade);
  c->FillRect(l, mid - 8, r, mid + 8, kButton);
  const float x = static_cast<float>(l + fraction * (r - l));
  c->FillRect(l + 3, mid - 5, x, mid + 5, 0xC0D8D2CAu);
  c->StrokeRect(l + 1, mid - 7, r - 1, mid + 7, 2, lit ? kButtonEdgeLit : kButtonEdge);
  c->FillRect(x - 9, mid - 22, x + 9, mid + 22, kButtonShade);
  c->FillRect(x - 7, mid - 20, x + 7, mid + 20, lit ? kButtonEdgeLit : kButtonEdge);
}

void CheckBox(Canvas2D* c, float mid, bool on, bool lit) {
  const float r = ml::kRight, l = r - ml::kCheckSize;
  const float t = mid - ml::kCheckSize * 0.5f, b = mid + ml::kCheckSize * 0.5f;
  Button(c, l, t, r, b, lit);
  if (on) {
    c->Line(l + 10, mid + 1, l + 19, b - 11, 6, kText);
    c->Line(l + 19, b - 11, r - 9, t + 10, 6, kText);
  }
}

}  // namespace

void PaintMenu(Canvas2D* c, const menu_model::Menu& menu,
               const std::string& status) {
  c->Clear();
  Panel(c, menu);
  // The line of white text at the top of the panel, as "Welcome. You are
  // playing Black & White as ..." of the game.
  const std::string head =
      menu.OnMain() ? "Welcome to the " BWVR_PRODUCT_NAME " " BWVR_VERSION_TEXT "."
                    : menu.Title();
  c->Text(head, ml::kWidth * 0.5f, ml::kTitleBaseline + 18, 40, kText, false,
          Canvas2D::kCenterAlign, true);

  const std::vector<Item> items = menu.Items();
  const bool main = menu.OnMain();
  const int cursor = menu.Cursor();
  for (int r = 0; r < static_cast<int>(items.size()) && r < ml::kMaxRows; ++r) {
    const Item& item = items[r];
    const float top = static_cast<float>(ml::RowTop(r));
    const float mid = top + ml::kRowHeight * 0.5f;
    const float baseline = mid + 12;
    const bool lit = r == cursor;
    const std::string label = menu.Label(item);
    if (main || item.kind == ItemKind::kBack) {
      // A button of the game: each entry of the main page, and Back.
      Button(c, kButtonLeft, top + 6, kButtonRight, top + ml::kRowHeight - 6, lit);
      c->Text(label, ml::kWidth * 0.5f, baseline, 36, kText, false,
              Canvas2D::kCenterAlign, true);
      continue;
    }
    // A row of settings: the label on the panel, its control at the right.
    if (lit) c->FillRect(ml::kLeft - 40, top + 3, ml::kRight + 40, top + ml::kRowHeight - 3, kRowLit);
    c->Text(label, ml::kLeft, baseline, 34, kText, false, Canvas2D::kLeftAlign, true);
    const double fraction = menu.SliderFraction(item);
    const int check = menu.CheckValue(item);
    const std::string value = menu.ValueText(item);
    if (fraction >= 0.0) {
      Slider(c, mid, fraction, lit);
      c->Text(value, ml::kBarLeft - 30, baseline, 32, kText, false,
              Canvas2D::kRightAlign, true);
    } else if (check >= 0) {
      CheckBox(c, mid, check != 0, lit);
    } else if (!value.empty()) {
      // A list: its item in a dark box, as a button of the game.
      const float w = c->Measure(value, 32, false) + 60;
      Button(c, ml::kRight - w, top + 10, ml::kRight, top + ml::kRowHeight - 10, lit);
      c->Text(value, ml::kRight - w * 0.5f, baseline, 32, kText, false,
              Canvas2D::kCenterAlign, true);
    }
  }

  // THE HELP of the row with the highlight, as the tooltips of the launcher,
  // in a dark box of the game.
  Button(c, ml::kLeft - 40, ml::kHelpTop, ml::kRight + 40, ml::kHelpBottom, false);
  if (cursor >= 0 && cursor < static_cast<int>(items.size())) {
    const std::string help = menu.Help(items[cursor]);
    if (!help.empty()) {
      const float size = 27;
      const std::vector<std::string> lines =
          Wrap(c, help, size, ml::kRight - ml::kLeft);
      float y = ml::kHelpTop + 38;
      for (size_t i = 0; i < lines.size() && i < 4; ++i) {
        c->Text(lines[i], ml::kLeft, y, size, kTextDim, false,
                Canvas2D::kLeftAlign, false);
        y += size * 1.32f;
      }
    }
  }
  c->Text(status, ml::kWidth * 0.5f, ml::kStatusBaseline, 28, kText, false,
          Canvas2D::kCenterAlign, true);
}

void PaintCursor(Canvas2D* c) {
  c->Clear();
  const float m = c->Width() * 0.5f;
  c->FillCircle(m, m, m * 0.95f, 0x40FFFFFFu);
  c->FillCircle(m, m, m * 0.55f, 0xA0FFFFFFu);
  c->FillCircle(m, m, m * 0.28f, 0xFFFFFFFFu);
}
