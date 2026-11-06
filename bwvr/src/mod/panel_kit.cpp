// The parts of a panel in the room. See panel_kit.h.

#include "panel_kit.h"

#include <math.h>
#include <string.h>

#include <new>

#include "../vr/xr_screen.h"
#include "card_scroll.h"
#include "controllers.h"
#include "menu_room.h"
#include "menu_room_math.h"

extern void ModLog(const char* format, ...);

void ControllersGesturePulse(float amplitude, float seconds);
uint64_t HeadPoseOriginSerial();
// hint_card.cpp: hint_card_style now, or below 0 while the cards are off.
int HintCardStyleValue();

namespace panel_kit {

namespace {

const ULONGLONG kFlashMs = 120;

bool Place(Room* room) {
  float head[3], orientation[4];
  if (!MenuRoomHeadPose(head, orientation)) return false;
  panel_kit_math::PlaceAt(head, menu_room_math::YawOf(orientation),
                          room->distance, room->pitch, room->width,
                          room->height, &room->panel);
  room->serial = HeadPoseOriginSerial();
  return true;
}

Font Measure(Canvas* canvas, HFONT font, int cap) {
  Font f = {font, cap, 0, 0};
  if (font == nullptr) return f;
  TEXTMETRICW metrics;
  SelectObject(canvas->dc, font);
  GetTextMetricsW(canvas->dc, &metrics);
  f.ascent = metrics.tmAscent;
  f.height = metrics.tmHeight;
  return f;
}

// White letters on black in the GDI picture, in the box, with the capital
// letter in the middle of the box. The clip keeps them in the box.
void Letters(Canvas* c, const Font& f, const wchar_t* words, const int b[4]) {
  const size_t row = static_cast<size_t>(b[2] - b[0]) * 4;
  for (int y = b[1]; y < b[3]; ++y) {
    memset(c->bits + static_cast<size_t>(y) * c->width + b[0], 0, row);
  }
  const int baseline = b[1] + (b[3] - b[1] + f.cap) / 2;
  SelectObject(c->dc, f.font);
  SetTextColor(c->dc, RGB(255, 255, 255));
  RECT rect = {b[0], baseline - f.ascent, b[2],
               baseline - f.ascent + f.height};
  HRGN clip = CreateRectRgn(b[0], b[1], b[2], b[3]);
  SelectClipRgn(c->dc, clip);
  DrawTextW(c->dc, words, -1, &rect,
            DT_SINGLELINE | DT_NOPREFIX | DT_CENTER | DT_TOP);
  SelectClipRgn(c->dc, nullptr);
  DeleteObject(clip);
  GdiFlush();
}

card_scroll::Picture PictureOf(Canvas* c, bool page) {
  const card_scroll::Picture p = {page ? c->page : c->card, c->width,
                                  c->width, c->height};
  return p;
}

// The box clears in the GDI picture, and words go in it with `format` of
// DrawTextW. The clip keeps them in the box.
void LettersBox(Canvas* c, const Font& f, const wchar_t* words, const int b[4],
                const RECT& rect, UINT format) {
  const size_t row = static_cast<size_t>(b[2] - b[0]) * 4;
  for (int y = b[1]; y < b[3]; ++y) {
    memset(c->bits + static_cast<size_t>(y) * c->width + b[0], 0, row);
  }
  SelectObject(c->dc, f.font);
  SetTextColor(c->dc, RGB(255, 255, 255));
  HRGN clip = CreateRectRgn(b[0], b[1], b[2], b[3]);
  SelectClipRgn(c->dc, clip);
  RECT r = rect;
  DrawTextW(c->dc, words, -1, &r, format);
  SelectClipRgn(c->dc, nullptr);
  DeleteObject(clip);
  GdiFlush();
}

int KeyLook(int look) {
  return look == kHover ? card_scroll::kKeyHover
         : (look == kFlash || look == kOn) ? card_scroll::kKeyFlash
                                           : card_scroll::kKeyPlain;
}

// A box of `to` that is in the GDI picture too.
bool Fits(const Canvas* c, const card_scroll::Picture& to, const int b[4]) {
  return to.pitch == c->width && to.height <= c->height && b[0] >= 0 &&
         b[1] >= 0 && b[2] <= to.width && b[3] <= to.height && b[0] < b[2] &&
         b[1] < b[3];
}

}  // namespace

// --- the room --------------------------------------------------------------------

bool Open(Room* room, float distance, float pitch, float width,
          float height) {
  room->distance = distance;
  room->pitch = pitch;
  room->width = width;
  room->height = height;
  if (!Place(room)) return false;
  room->open = true;
  room->on_panel = false;
  room->meters = 0.0f;
  room->hover = -1;
  room->flash = -1;
  room->flash_until = 0;
  panel_kit_math::PressOpen(&room->press);
  return true;
}

void Close(Room* room) {
  room->open = false;
  room->on_panel = false;
  room->meters = 0.0f;
  room->hover = -1;
  room->flash = -1;
}

bool FollowRecenter(Room* room) {
  if (!room->open || HeadPoseOriginSerial() == room->serial) return false;
  if (!Place(room)) {
    room->serial = HeadPoseOriginSerial();
    return false;
  }
  return true;
}

int Step(Room* room, HitFn hit, void* context, float* trigger) {
  float p[3], q[4], t = 0.0f;
  const bool aim = ControllersPointingAim(p, q, &t);
  *trigger = aim ? t : 0.0f;
  int part = -1;
  room->on_panel = false;
  room->meters = 0.0f;
  if (aim) {
    float direction[3];
    menu_room_math::AimForward(q, direction);
    float u = 0.0f, v = 0.0f, along = 0.0f;
    if (menu_room_math::Hit(p, direction, room->panel, &u, &v, &along) &&
        menu_room_math::Inside(u, v)) {
      room->on_panel = true;
      room->u = u;
      room->v = v;
      room->meters = along;
      part = hit(u, v, context);
    }
  }
  if (part != room->hover && part >= 0) ControllersGesturePulse(0.1f, 0.008f);
  room->hover = part;
  if (room->flash >= 0 && GetTickCount64() >= room->flash_until) {
    room->flash = -1;
  }
  return panel_kit_math::PressStep(&room->press, *trigger, part);
}

void Pressed(Room* room, int part) {
  ControllersGesturePulse(0.3f, 0.02f);
  Flash(room, part);
}

void Flash(Room* room, int part) {
  room->flash = part;
  room->flash_until = GetTickCount64() + kFlashMs;
}

int Flashing(Room* room) {
  if (room->flash >= 0 && GetTickCount64() >= room->flash_until) {
    room->flash = -1;
  }
  return room->flash;
}

void PointOf(const Room& room, float u, float v, float out[3]) {
  menu_room_math::PointAt(room.panel, u, v, out);
}

bool StageLayer(const Room& room, const uint8_t* pixels, int width,
                int height) {
  if (bwvr::XrScreen::CardFailed(bwvr::kCardRing)) return false;
  bwvr::CardLayer layer = {};
  layer.visible = true;
  layer.pixels = pixels;
  layer.texture_width = static_cast<uint32_t>(width);
  layer.texture_height = static_cast<uint32_t>(height);
  layer.rect[0] = 0;
  layer.rect[1] = 0;
  layer.rect[2] = width;
  layer.rect[3] = height;
  for (int i = 0; i < 3; ++i) layer.position[i] = room.panel.position[i];
  for (int i = 0; i < 4; ++i) layer.orientation[i] = room.panel.orientation[i];
  layer.width = room.panel.width;
  layer.height = room.panel.height;
  bwvr::XrScreen::StageCard(bwvr::kCardRing, layer);
  return true;
}

// --- the picture -----------------------------------------------------------------

bool Start(Canvas* c, int width, int height, const char* tag) {
  if (c->dc != nullptr) return true;
  if (c->failed) return false;
  c->width = width;
  c->height = height;
  c->tag = tag;
  c->dc = CreateCompatibleDC(nullptr);
  BITMAPINFO info;
  memset(&info, 0, sizeof(info));
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = width;
  info.bmiHeader.biHeight = -height;  // top first
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  if (c->dc != nullptr) {
    c->dib = CreateDIBSection(c->dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
  }
  if (c->dc == nullptr || c->dib == nullptr || bits == nullptr) {
    c->failed = true;
    ModLog("%s: GDI gave no picture for the panel, so the panel does not "
           "show", tag);
    return false;
  }
  c->bits = static_cast<uint32_t*>(bits);
  SelectObject(c->dc, c->dib);
  SetBkMode(c->dc, TRANSPARENT);
  return true;
}

bool StartParchment(Canvas* c, PageFn draw_page, void* context) {
  if (c->parchment_ready) return true;
  if (c->parchment_failed || c->dc == nullptr) return false;
  c->parchment_failed = true;
  if (!card_scroll::Start(c->dc)) {
    ModLog("%s: GDI gave no font for the parchment, so the panel is dark",
           c->tag);
    return false;
  }
  const size_t bytes = static_cast<size_t>(c->width) * c->height * 4;
  c->page = new (std::nothrow) uint8_t[bytes];
  c->card = new (std::nothrow) uint8_t[bytes];
  if (c->page == nullptr || c->card == nullptr) {
    delete[] c->page;
    delete[] c->card;
    c->page = c->card = nullptr;
    ModLog("%s: no memory for the parchment, so the panel is dark", c->tag);
    return false;
  }
  LARGE_INTEGER f, t0, t1;
  QueryPerformanceFrequency(&f);
  QueryPerformanceCounter(&t0);
  card_scroll::Page(PictureOf(c, true), 4, 4, c->width - 4, c->height - 4,
                    17);
  if (draw_page != nullptr) draw_page(c, context);
  memcpy(c->card, c->page, bytes);
  QueryPerformanceCounter(&t1);
  c->parchment_failed = false;
  c->parchment_ready = true;
  ModLog("%s: the page of the parchment panel took %.2f ms (%s)", c->tag,
         static_cast<double>(t1.QuadPart - t0.QuadPart) * 1000.0 /
             static_cast<double>(f.QuadPart),
         card_scroll::FontNote());
  return true;
}

Font WordsFont(Canvas* c, int cap) {
  return Measure(c, card_scroll::WordsFont(c->dc, cap), cap);
}

Font SealFont(Canvas* c, int cap) {
  return Measure(c, card_scroll::SealFont(c->dc, cap), cap);
}

Font DigitsFont(Canvas* c, int cap, const char** face) {
  return Measure(c, card_scroll::DigitsFont(c->dc, cap, face), cap);
}

void InkWords(Canvas* c, const Font& font, const wchar_t* words,
              const int box[4], int seed, bool to_page) {
  if (font.font == nullptr || words == nullptr || words[0] == 0) return;
  Letters(c, font, words, box);
  card_scroll::Ink(PictureOf(c, to_page), c->bits, box[0], box[1], box[2],
                   box[3], seed);
}

void PageToCard(Canvas* c, const int box[4]) {
  const size_t row = static_cast<size_t>(box[2] - box[0]) * 4;
  for (int y = box[1]; y < box[3]; ++y) {
    const size_t at = (static_cast<size_t>(y) * c->width + box[0]) * 4;
    memcpy(c->card + at, c->page + at, row);
  }
}

void Fade(Canvas* c, const int box[4]) {
  for (int y = box[1]; y < box[3]; ++y) {
    const size_t at = (static_cast<size_t>(y) * c->width + box[0]) * 4;
    uint8_t* card = c->card + at;
    const uint8_t* page = c->page + at;
    for (int i = 0; i < (box[2] - box[0]) * 4; ++i) {
      card[i] = static_cast<uint8_t>(page[i] + ((card[i] - page[i]) * 90) / 256);
    }
  }
}

void ClearCard(Canvas* c) {
  memcpy(c->card, c->page, static_cast<size_t>(c->width) * c->height * 4);
}

// --- the parts -------------------------------------------------------------------

void Button(Canvas* c, const int rect[4], int look, const wchar_t* word,
            const Font& font, int seed) {
  int box[4];
  panel_kit_math::RectBox(rect, kRim, c->width, c->height, box);
  PageToCard(c, box);
  const int r[4] = {rect[0], rect[1], rect[0] + rect[2], rect[1] + rect[3]};
  card_scroll::Key(PictureOf(c, false), r[0], r[1], r[2], r[3], KeyLook(look),
                   seed);
  InkWords(c, font, word, r, seed, false);
  if (look == kDim) Fade(c, box);
}

bool MakeTiles(Tiles* t, int parts, int looks) {
  if (t->tiles != nullptr) return true;
  t->tiles = new (std::nothrow) uint8_t*[static_cast<size_t>(parts) * looks];
  if (t->tiles == nullptr) return false;
  memset(t->tiles, 0, sizeof(uint8_t*) * parts * looks);
  t->parts = parts;
  t->looks = looks;
  return true;
}

bool ShowPart(Canvas* c, Tiles* t, int part, int look, const int box[4],
              DrawFn draw, void* context) {
  const size_t row = static_cast<size_t>(box[2] - box[0]) * 4;
  uint8_t** slot = nullptr;
  if (t != nullptr && t->tiles != nullptr && part >= 0 && part < t->parts &&
      look >= 0 && look < t->looks) {
    slot = &t->tiles[part * t->looks + look];
  }
  if (slot != nullptr && *slot != nullptr) {
    for (int y = box[1]; y < box[3]; ++y) {
      memcpy(c->card + (static_cast<size_t>(y) * c->width + box[0]) * 4,
             *slot + static_cast<size_t>(y - box[1]) * row, row);
    }
    return true;
  }
  draw(c, part, look, context);
  if (slot != nullptr) {
    *slot = new (std::nothrow) uint8_t[row * (box[3] - box[1])];
    if (*slot != nullptr) {
      for (int y = box[1]; y < box[3]; ++y) {
        memcpy(*slot + static_cast<size_t>(y - box[1]) * row,
               c->card + (static_cast<size_t>(y) * c->width + box[0]) * 4,
               row);
      }
    }
  }
  return false;
}

// --- the widgets -----------------------------------------------------------------

card_scroll::Picture CardPicture(Canvas* c) { return PictureOf(c, false); }
card_scroll::Picture PagePicture(Canvas* c) { return PictureOf(c, true); }

void InkWordsIn(Canvas* c, const card_scroll::Picture& to, const Font& font,
                const wchar_t* words, const int box[4], int seed, int align) {
  if (font.font == nullptr || words == nullptr || words[0] == 0 ||
      !Fits(c, to, box)) {
    return;
  }
  const int baseline = box[1] + (box[3] - box[1] + font.cap) / 2;
  const RECT rect = {box[0], baseline - font.ascent, box[2],
                     baseline - font.ascent + font.height};
  const UINT side = align == kLeftAlign    ? DT_LEFT
                    : align == kRightAlign ? DT_RIGHT
                                           : DT_CENTER;
  LettersBox(c, font, words, box, rect,
             DT_SINGLELINE | DT_NOPREFIX | DT_TOP | side);
  card_scroll::Ink(to, c->bits, box[0], box[1], box[2], box[3], seed);
}

int InkLines(Canvas* c, const card_scroll::Picture& to, const Font& font,
             const wchar_t* words, const int box[4], int seed, bool draw) {
  if (font.font == nullptr || words == nullptr || words[0] == 0) return 0;
  const UINT format = DT_WORDBREAK | DT_NOPREFIX | DT_LEFT | DT_TOP;
  RECT measure = {box[0], box[1], box[2], box[1] + 1};
  SelectObject(c->dc, font.font);
  const int height =
      DrawTextW(c->dc, words, -1, &measure, format | DT_CALCRECT);
  if (!draw) return height;
  int b[4] = {box[0], box[1], box[2], box[1] + height + 4};
  if (b[3] > box[3]) b[3] = box[3];
  if (!Fits(c, to, b)) return height;
  const RECT rect = {box[0], box[1], box[2], box[3]};
  LettersBox(c, font, words, b, rect, format);
  card_scroll::Ink(to, c->bits, b[0], b[1], b[2], b[3], seed);
  return height;
}

void ArrowKey(Canvas* c, const card_scroll::Picture& to, const int rect[4],
              int look, bool left, int seed) {
  const int r[4] = {rect[0], rect[1], rect[0] + rect[2], rect[1] + rect[3]};
  if (!Fits(c, to, r)) return;
  card_scroll::Key(to, r[0], r[1], r[2], r[3], KeyLook(look), seed);
  // The triangle: 36 percent of the key across and 46 percent high. Each
  // pixel has a cover of 4 by 4 samples, so the edges are smooth.
  const float w = rect[2] * 0.36f, h = rect[3] * 0.46f;
  const float cx = rect[0] + rect[2] * 0.5f, cy = rect[1] + rect[3] * 0.5f;
  const float tip = left ? cx - w * 0.5f : cx + w * 0.5f;
  const float base = left ? cx + w * 0.5f : cx - w * 0.5f;
  const int b[4] = {static_cast<int>(cx - w * 0.5f) - 1,
                    static_cast<int>(cy - h * 0.5f) - 1,
                    static_cast<int>(cx + w * 0.5f) + 2,
                    static_cast<int>(cy + h * 0.5f) + 2};
  for (int y = b[1]; y < b[3]; ++y) {
    for (int x = b[0]; x < b[2]; ++x) {
      int in = 0;
      for (int j = 0; j < 4; ++j) {
        for (int i = 0; i < 4; ++i) {
          const float px = x + (i + 0.5f) * 0.25f;
          const float py = y + (j + 0.5f) * 0.25f;
          // The share of the way from the base to the tip.
          const float t = (px - base) / (tip - base);
          if (t < 0.0f || t > 1.0f) continue;
          if (fabsf(py - cy) <= (1.0f - t) * h * 0.5f) ++in;
        }
      }
      const uint32_t cover = static_cast<uint32_t>(in * 255 / 16);
      c->bits[static_cast<size_t>(y) * c->width + x] =
          cover | (cover << 8) | (cover << 16);
    }
  }
  card_scroll::Ink(to, c->bits, b[0], b[1], b[2], b[3], seed);
}

void Toggle(Canvas* c, const card_scroll::Picture& to, const int rect[4],
            int look, bool on, int seed) {
  const int r[4] = {rect[0], rect[1], rect[0] + rect[2], rect[1] + rect[3]};
  if (!Fits(c, to, r)) return;
  card_scroll::Key(to, r[0], r[1], r[2], r[3], KeyLook(look), seed);
  const int pad = 6;
  const int knob = (rect[2] - 2 * pad) / 2 + 4;
  const int x0 = on ? r[2] - pad - knob : r[0] + pad;
  card_scroll::Seal(to, x0, r[1] + pad, x0 + knob, r[3] - pad - 2, on ? 1 : 0,
                    seed + (on ? 3 : 0));
}

void ScrollBar(Canvas* c, const card_scroll::Picture& to, const int track[4],
               const int thumb[4], int look, int seed) {
  if (!Fits(c, to, track)) return;
  card_scroll::Key(to, track[0], track[1], track[2], track[3],
                   card_scroll::kKeyPlain, seed);
  const int inset = 5;
  card_scroll::Key(to, thumb[0] + inset, thumb[1] + inset, thumb[2] - inset,
                   thumb[3] - inset, KeyLook(look), seed + 1);
}

void FadeBox(const card_scroll::Picture& to, const card_scroll::Picture& under,
             const int box[4]) {
  for (int y = box[1]; y < box[3]; ++y) {
    uint8_t* a = to.bgra + (static_cast<size_t>(y) * to.pitch + box[0]) * 4;
    const uint8_t* b =
        under.bgra + (static_cast<size_t>(y) * under.pitch + box[0]) * 4;
    for (int i = 0; i < (box[2] - box[0]) * 4; ++i) {
      a[i] = static_cast<uint8_t>(b[i] + ((a[i] - b[i]) * 90) / 256);
    }
  }
}

namespace {

const int kNotePad = 10;  // from the edge of the paper to the words
const int kNoteGap = 6;   // between two paragraphs

void NoteWordsBox(int width, int out[4]) {
  out[0] = card_scroll::kWordsLeft;
  out[1] = card_scroll::kPaperTop + kNotePad;
  out[2] = width - card_scroll::kWordsRight;
  out[3] = 0;
}

}  // namespace

int NoteHeight(Canvas* c, int width, const wchar_t* const* lines, int count,
               const Font& title, const Font& font) {
  int box[4];
  NoteWordsBox(width, box);
  int h = 0;
  const card_scroll::Picture none = {nullptr, c->width, width, c->height};
  for (int i = 0; i < count; ++i) {
    if (lines[i] == nullptr || lines[i][0] == 0) continue;
    if (h > 0) h += kNoteGap;
    h += InkLines(c, none, i == 0 ? title : font, lines[i], box, 0, false);
  }
  return box[1] + h + kNotePad + card_scroll::kPaperBottom;
}

bool Note(Canvas* c, const card_scroll::Picture& note,
          const wchar_t* const* lines, int count, const Font& title,
          const Font& font, int seed) {
  if (note.pitch != c->width || note.height > c->height ||
      note.width > c->width) {
    return false;
  }
  card_scroll::Paper(note, card_scroll::kEdgeLeft, card_scroll::kPaperTop,
                     note.width - card_scroll::kEdgeRight,
                     note.height - card_scroll::kPaperBottom, seed);
  int box[4];
  NoteWordsBox(note.width, box);
  box[3] = note.height - card_scroll::kPaperBottom;
  for (int i = 0; i < count; ++i) {
    if (lines[i] == nullptr || lines[i][0] == 0) continue;
    const int h =
        InkLines(c, note, i == 0 ? title : font, lines[i], box, seed + i, true);
    box[1] += h + kNoteGap;
    if (box[1] >= box[3]) break;
  }
  return true;
}

void Over(Canvas* c, const card_scroll::Picture& note, int x, int y) {
  for (int j = 0; j < note.height; ++j) {
    const int cy = y + j;
    if (cy < 0 || cy >= c->height) continue;
    for (int i = 0; i < note.width; ++i) {
      const int cx = x + i;
      if (cx < 0 || cx >= c->width) continue;
      const uint8_t* n =
          note.bgra + (static_cast<size_t>(j) * note.pitch + i) * 4;
      if (n[3] == 0) continue;
      uint8_t* d = c->card + (static_cast<size_t>(cy) * c->width + cx) * 4;
      const int keep = 255 - n[3];
      for (int k = 0; k < 4; ++k) {
        d[k] = static_cast<uint8_t>(n[k] + (d[k] * keep + 127) / 255);
      }
    }
  }
}

void PlainFill(Canvas* c, const int box[4], COLORREF color) {
  HBRUSH brush = CreateSolidBrush(color);
  RECT rect = {box[0], box[1], box[2], box[3]};
  FillRect(c->dc, &rect, brush);
  DeleteObject(brush);
}

void PlainText(Canvas* c, HFONT font, COLORREF color, const wchar_t* words,
               const int box[4]) {
  SelectObject(c->dc, font);
  SetTextColor(c->dc, color);
  RECT rect = {box[0], box[1], box[2], box[3]};
  DrawTextW(c->dc, words, -1, &rect,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

void PlainButton(Canvas* c, const int rect[4], int look, const wchar_t* word,
                 HFONT font) {
  COLORREF face = RGB(62, 66, 76), ink = RGB(235, 235, 235);
  switch (look) {
    case kHover: face = RGB(205, 172, 96); ink = RGB(20, 20, 20); break;
    case kFlash:
    case kOn: face = RGB(250, 225, 150); ink = RGB(20, 20, 20); break;
    case kDim: face = RGB(40, 42, 48); ink = RGB(110, 110, 110); break;
    default: break;
  }
  const int box[4] = {rect[0], rect[1], rect[0] + rect[2], rect[1] + rect[3]};
  PlainFill(c, box, face);
  PlainText(c, font, ink, word, box);
}

void PlainDone(Canvas* c) {
  GdiFlush();
  const size_t count = static_cast<size_t>(c->width) * c->height;
  for (size_t i = 0; i < count; ++i) c->bits[i] |= 0xFF000000u;
}

int CardStyle(int ini) {
  const int live = HintCardStyleValue();
  if (live >= 0) return live != 0 ? 1 : 0;
  return ini != 0 ? 1 : 0;
}

}  // namespace panel_kit
