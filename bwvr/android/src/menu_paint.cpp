#include "menu_paint.h"

#include <vector>

#include "menu_layout.h"

namespace {

namespace ml = menu_layout;
using menu_model::Item;
using menu_model::ItemKind;

// THE COLORS. A dark panel with a frame of bronze and text of gold and of
// parchment, as the dialog boxes of the game.
constexpr uint32_t kPanelTop = 0xF22A1C10u;
constexpr uint32_t kPanelBottom = 0xF2100A05u;
constexpr uint32_t kBronze = 0xFFA87A3Au;
constexpr uint32_t kBronzeDark = 0xFF5A3D18u;
constexpr uint32_t kGold = 0xFFF0CF7Au;
constexpr uint32_t kGoldDim = 0xFFC9A55Au;
constexpr uint32_t kParchment = 0xFFEFE2C2u;
constexpr uint32_t kParchmentDim = 0xFFB9AC8Cu;
constexpr uint32_t kHighlight = 0xD08A5A22u;
constexpr uint32_t kHelpBack = 0x50000000u;

void Diamond(Canvas2D* c, float x, float y, float r, uint32_t argb) {
  // Two lines across give a small mark with four points.
  c->Line(x - r, y, x + r, y, 3.0f, argb);
  c->Line(x, y - r * 0.6f, x, y + r * 0.6f, 3.0f, argb);
  c->FillCircle(x, y, r * 0.35f, argb);
}

void Frame(Canvas2D* c) {
  const float w = static_cast<float>(ml::kWidth);
  const float h = static_cast<float>(ml::kHeight);
  c->GradientRect(8, 8, w - 8, h - 8, 0, 0, 0, h, kPanelTop, kPanelBottom, 28);
  c->StrokeRoundRect(8, 8, w - 8, h - 8, 28, 8, kBronzeDark);
  c->StrokeRoundRect(12, 12, w - 12, h - 12, 24, 3, kBronze);
  c->StrokeRoundRect(30, 30, w - 30, h - 30, 14, 2, kGoldDim);
  const float inset = 30;
  Diamond(c, inset, inset, 14, kGold);
  Diamond(c, w - inset, inset, 14, kGold);
  Diamond(c, inset, h - inset, 14, kGold);
  Diamond(c, w - inset, h - inset, 14, kGold);
}

void Title(Canvas2D* c, const std::string& title) {
  const float mid = ml::kWidth * 0.5f;
  c->Text(title, mid, ml::kTitleBaseline, 66, kGold, true,
          Canvas2D::kCenterAlign, true);
  const float y = ml::kTitleBaseline + 34;
  c->Line(ml::kLeft + 40, y, mid - 40, y, 2, kBronze);
  c->Line(mid + 40, y, ml::kRight - 40, y, 2, kBronze);
  Diamond(c, mid, y, 18, kGold);
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

void Slider(Canvas2D* c, float mid, double fraction) {
  const float l = ml::kBarLeft, r = ml::kBarRight;
  c->FillRoundRect(l, mid - 7, r, mid + 7, 7, 0xFF241608u);
  const float x = static_cast<float>(l + fraction * (r - l));
  c->GradientRect(l, mid - 7, x, mid + 7, l, 0, r, 0, kBronze, kGold, 7);
  c->StrokeRoundRect(l, mid - 7, r, mid + 7, 7, 2, kBronzeDark);
  c->FillCircle(x, mid, 17, kBronzeDark);
  c->FillCircle(x, mid, 13, kGold);
}

void CheckBox(Canvas2D* c, float mid, bool on) {
  const float r = ml::kRight, l = r - ml::kCheckSize;
  const float t = mid - ml::kCheckSize * 0.5f, b = mid + ml::kCheckSize * 0.5f;
  c->FillRoundRect(l, t, r, b, 6, 0xFF241608u);
  c->StrokeRoundRect(l, t, r, b, 6, 3, kBronze);
  if (on) {
    c->Line(l + 9, mid + 1, l + 19, b - 10, 6, kGold);
    c->Line(l + 19, b - 10, r - 8, t + 9, 6, kGold);
  }
}

}  // namespace

void PaintMenu(Canvas2D* c, const menu_model::Menu& menu,
               const std::string& status) {
  c->Clear();
  Frame(c);
  Title(c, menu.Title());

  const std::vector<Item> items = menu.Items();
  const bool main = menu.OnMain();
  const int cursor = menu.Cursor();
  for (int r = 0; r < static_cast<int>(items.size()) && r < ml::kMaxRows; ++r) {
    const Item& item = items[r];
    const float top = static_cast<float>(ml::RowTop(r));
    const float mid = top + ml::kRowHeight * 0.5f;
    const float baseline = mid + 14;
    const bool lit = r == cursor;
    if (lit) {
      c->GradientRect(ml::kLeft - 40, top + 4, ml::kRight + 40,
                      top + ml::kRowHeight - 4, ml::kLeft - 40, 0,
                      ml::kRight + 40, 0, kHighlight, 0x308A5A22u, 10);
      c->FillRect(ml::kLeft - 40, top + 8, ml::kLeft - 32,
                  top + ml::kRowHeight - 8, kGold);
    }
    const std::string label = menu.Label(item);
    const uint32_t color = lit ? 0xFFFFF2C8u : kParchment;
    const bool command = item.kind != ItemKind::kSetting;
    if (main || item.kind == ItemKind::kBack) {
      // The main page of the game has its entries in the middle.
      const bool group = item.kind == ItemKind::kGroup;
      c->Text(label, ml::kWidth * 0.5f, baseline, group ? 40 : 44,
              group ? color : (lit ? 0xFFFFE9A8u : kGold), !group,
              Canvas2D::kCenterAlign, true);
      continue;
    }
    c->Text(label, ml::kLeft, baseline, 38, color, command,
            Canvas2D::kLeftAlign, true);
    const double fraction = menu.SliderFraction(item);
    const int check = menu.CheckValue(item);
    const std::string value = menu.ValueText(item);
    if (fraction >= 0.0) {
      Slider(c, mid, fraction);
      c->Text(value, ml::kBarLeft - 30, baseline, 34, kGold, false,
              Canvas2D::kRightAlign, true);
    } else if (check >= 0) {
      CheckBox(c, mid, check != 0);
    } else if (!value.empty()) {
      c->Text(value, ml::kRight, baseline, 36, kGold, false,
              Canvas2D::kRightAlign, true);
    }
  }

  // THE HELP of the row with the highlight, as the tooltips of the launcher.
  c->FillRoundRect(ml::kLeft - 40, ml::kHelpTop, ml::kRight + 40,
                   ml::kHelpBottom, 12, kHelpBack);
  c->StrokeRoundRect(ml::kLeft - 40, ml::kHelpTop, ml::kRight + 40,
                     ml::kHelpBottom, 12, 2, kBronzeDark);
  if (cursor >= 0 && cursor < static_cast<int>(items.size())) {
    const std::string help = menu.Help(items[cursor]);
    if (!help.empty()) {
      const float size = 27;
      const std::vector<std::string> lines =
          Wrap(c, help, size, ml::kRight - ml::kLeft);
      float y = ml::kHelpTop + 38;
      for (size_t i = 0; i < lines.size() && i < 4; ++i) {
        c->Text(lines[i], ml::kLeft, y, size, kParchmentDim, false,
                Canvas2D::kLeftAlign, false);
        y += size * 1.32f;
      }
    }
  }
  c->Text(status, ml::kWidth * 0.5f, ml::kStatusBaseline, 28, kGoldDim, false,
          Canvas2D::kCenterAlign, false);
}

void PaintCursor(Canvas2D* c) {
  c->Clear();
  const float m = c->Width() * 0.5f;
  c->FillCircle(m, m, m * 0.95f, 0x40F0CF7Au);
  c->FillCircle(m, m, m * 0.55f, 0xA0F0CF7Au);
  c->FillCircle(m, m, m * 0.28f, 0xFFFFF2C8u);
}
