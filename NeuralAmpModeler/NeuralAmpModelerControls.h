#pragma once

#include <algorithm> // std::transform, std::min
#include <cmath> // std::round, std::ceil
#include <cstdio> // FILE, fclose
#include <cstdint> // uint64_t
#include <functional> // std::function
#include <sstream> // std::stringstream
#include <string> // std::string
#include <unordered_map> // std::unordered_map
#include <unordered_set> // std::unordered_set
#include <utility> // std::pair
#include <vector> // std::vector
#include "IControls.h"
#include "IPlugPaths.h"

#ifdef OS_WIN
  #include <Windows.h>
  #include <Shellapi.h>
#endif

#define PLUG() static_cast<PLUG_CLASS_NAME*>(GetDelegate())
#define NAM_KNOB_HEIGHT 120.0f
#define NAM_SWTICH_HEIGHT 50.0f

using namespace iplug;
using namespace igraphics;

enum class NAMBrowserState
{
  Empty, // when no file loaded, show "Get" button
  Loaded // when file loaded, show "Clear" button
};

// Where the corner button on the plugin (settings, close settings) goes
// :param rect: Rect for the whole plugin's UI
IRECT CornerButtonArea(const IRECT& rect)
{
  const auto mainArea = rect.GetPadded(-20);
  return mainArea.GetFromTRHC(50, 50).GetCentredInside(20, 20);
};

class NAMSquareButtonControl : public ISVGButtonControl
{
public:
  NAMSquareButtonControl(const IRECT& bounds, IActionFunction af, const ISVG& svg)
  : ISVGButtonControl(bounds, af, svg, svg)
  {
  }

  void Draw(IGraphics& g) override
  {
    if (mMouseIsOver)
      g.FillRoundRect(PluginColors::MOUSEOVER, mRECT, 2.f);

    ISVGButtonControl::Draw(g);
  }
};

class NAMCircleButtonControl : public ISVGButtonControl
{
public:
  NAMCircleButtonControl(const IRECT& bounds, IActionFunction af, const ISVG& svg)
  : ISVGButtonControl(bounds, af, svg, svg)
  {
  }

  void Draw(IGraphics& g) override
  {
    if (mMouseIsOver)
      g.FillEllipse(PluginColors::MOUSEOVER, mRECT);

    ISVGButtonControl::Draw(g);
  }
};

// ToneCast: toggles NAMLibraryPanelControl. Drawn as three bars (no
// dedicated icon asset exists for this) rather than an SVG button.
class NAMLibraryToggleButtonControl : public IControl
{
public:
  NAMLibraryToggleButtonControl(const IRECT& bounds, IActionFunction af)
  : IControl(bounds, af)
  {
  }

  // ToneCast: plain IControl has no single-click wiring by default -- the
  // base class's only path to SetDirty(true) (which fires mActionFunc) is
  // OnMouseDblClick -> SetValueToDefault -> SetDirty(true). Sibling button
  // classes here (NAMSquareButtonControl/NAMCircleButtonControl) don't hit
  // this because they extend ISVGButtonControl, which wires up single-click
  // itself; this one extends IControl directly and was missing that,
  // which is why it needed two clicks (i.e. a double-click) to open.
  void OnMouseDown(float x, float y, const IMouseMod& mod) override { SetDirty(true); }

  void Draw(IGraphics& g) override
  {
    if (mMouseIsOver)
      g.FillRoundRect(PluginColors::MOUSEOVER, mRECT, 3.f);

    const IColor c = ToneCastColors::FG_TEXT;
    const IRECT bars = mRECT.GetPadded(-6.f);
    const float barH = 2.f;
    for (int i = 0; i < 3; i++)
    {
      const float y = bars.T + static_cast<float>(i) * (bars.H() - barH) / 2.f;
      g.FillRect(c, IRECT(bars.L, y, bars.R, y + barH));
    }
  }
};

/// Full-window dim layer; click dismisses (used for Slim overlay).
class NAMSlimOverlayBackdropControl : public IControl
{
public:
  NAMSlimOverlayBackdropControl(const IRECT& bounds, IActionFunction dismiss)
  : IControl(bounds, dismiss)
  , mDismiss(dismiss)
  {
  }

  void Draw(IGraphics& g) override { g.FillRect(COLOR_BLACK.WithOpacity(0.45f), mRECT); }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (mDismiss)
      mDismiss(this);
  }

private:
  IActionFunction mDismiss;
};

class NAMFittedBitmapControl : public IControl, public IBitmapBase
{
public:
  NAMFittedBitmapControl(const IRECT& bounds, IBitmap bitmap)
  : IControl(bounds)
  , IBitmapBase(bitmap)
  {
    mIgnoreMouse = true;
  }

  void OnRescale() override { mBitmap = GetUI()->GetScaledBitmap(mBitmap); }
  void Draw(IGraphics& g) override { g.DrawFittedBitmap(mBitmap, mRECT); }
};

class NAMKnobControl : public IVKnobControl, public IBitmapBase
{
public:
  NAMKnobControl(const IRECT& bounds, int paramIdx, const char* label, const IVStyle& style, IBitmap bitmap)
  : IVKnobControl(bounds, paramIdx, label, style, true)
  , IBitmapBase(bitmap)
  {
    mInnerPointerFrac = 0.55;
    // ToneCast: default gearing (4.0) needs ~4x the knob's drag-bounds
    // height in mouse travel to sweep the full range, which reads as
    // "moves very slowly" for knobs this size. Lower = more responsive;
    // holding the fine-control modifier (shift/ctrl) still multiplies
    // this by 10x for precise adjustment.
    SetGearing(1.5);
  }

  void OnRescale() override { mBitmap = GetUI()->GetScaledBitmap(mBitmap); }

  void DrawWidget(IGraphics& g) override
  {
    auto knobRect = mWidgetBounds.GetCentredInside(mWidgetBounds.W(), mWidgetBounds.W());
    const float cx = knobRect.MW(), cy = knobRect.MH();
    const float angle = mAngle1 + (static_cast<float>(GetValue()) * (mAngle2 - mAngle1));
    g.DrawFittedBitmap(mBitmap, knobRect);

    // Draw one pointer after the bitmap. The previous implementation mixed
    // the bitmap with IVKnob's outer indicator track and fed its radii into a
    // second helper, which made the orange mark appear detached/misaligned.
    const float knobRadius = knobRect.W() * 0.5f;
    float data[2][2];
    RadialPoints(angle, cx, cy, 0.12f * knobRadius, 0.52f * knobRadius, 2, data);
    g.DrawLine(GetColor(kX3), data[0][0], data[0][1], data[1][0], data[1][1], &mBlend, 2.5f);
    g.FillCircle(GetColor(kX3), data[1][0], data[1][1], 2.25f, &mBlend);
  }
};

class NAMAnalogMeterControl : public IVPeakAvgMeterControl<1>, public IBitmapBase
{
public:
  NAMAnalogMeterControl(const IRECT& bounds, const IVStyle& style, IBitmap bitmap, float lowRangeDB, float highRangeDB)
  : IVPeakAvgMeterControl<1>(bounds, "", style, EDirection::Vertical, {}, 0, lowRangeDB, highRangeDB, {})
  , IBitmapBase(bitmap)
  {
    mIgnoreMouse = true;
  }

  void OnRescale() override { mBitmap = GetUI()->GetScaledBitmap(mBitmap); }

  void Draw(IGraphics& g) override
  {
    g.DrawFittedBitmap(mBitmap, mRECT);

    const float normalized = Clip(static_cast<float>(GetValue()), 0.f, 1.f);
    const float angle = -52.f + normalized * 104.f;
    const float pivotX = mRECT.MW();
    const float pivotY = mRECT.T + mRECT.H() * 0.79f;
    const float needleLength = mRECT.W() * 0.34f;
    float data[2][2];
    RadialPoints(angle, pivotX, pivotY, 4.f, needleLength, 2, data);
    g.DrawLine(COLOR_BLACK.WithOpacity(0.45f), data[0][0] + 1.f, data[0][1] + 1.f, data[1][0] + 1.f,
               data[1][1] + 1.f, &mBlend, 2.5f);
    g.DrawLine(IColor(255, 35, 27, 20), data[0][0], data[0][1], data[1][0], data[1][1], &mBlend, 1.8f);
    g.FillCircle(IColor(255, 167, 101, 43), pivotX, pivotY, 4.2f, &mBlend);
    g.DrawCircle(IColor(255, 39, 28, 19), pivotX, pivotY, 4.2f, &mBlend, 1.f);
  }
};

class NAMSwitchControl : public IVSlideSwitchControl, public IBitmapBase
{
public:
  NAMSwitchControl(const IRECT& bounds, int paramIdx, const char* label, const IVStyle& style, IBitmap bitmap)
  : IVSlideSwitchControl(bounds, paramIdx, label,
                         style.WithRoundness(0.666f)
                           .WithShowValue(false)
                           .WithEmboss(true)
                           .WithShadowOffset(1.5f)
                           .WithDrawShadows(false)
                           .WithColor(kFR, COLOR_BLACK)
                           .WithFrameThickness(0.5f)
                           .WithWidgetFrac(0.5f)
                           .WithLabelOrientation(EOrientation::South))
  , IBitmapBase(bitmap)
  {
  }

  void DrawWidget(IGraphics& g) override
  {
    DrawTrack(g, mWidgetBounds);
    DrawHandle(g, mHandleBounds);
  }

  void DrawTrack(IGraphics& g, const IRECT& bounds) override
  {
    IRECT handleBounds = GetAdjustedHandleBounds(bounds);
    handleBounds = IRECT(handleBounds.L, handleBounds.T, handleBounds.R, handleBounds.T + mBitmap.H());
    IRECT centreBounds = handleBounds.GetPadded(-mStyle.shadowOffset);
    IRECT shadowBounds = handleBounds.GetTranslated(mStyle.shadowOffset, mStyle.shadowOffset);
    //    const float contrast = mDisabled ? -GRAYED_ALPHA : 0.f;
    float cR = 7.f;
    const float tlr = cR;
    const float trr = cR;
    const float blr = cR;
    const float brr = cR;

    // outer shadow
    if (mStyle.drawShadows)
      g.FillRoundRect(GetColor(kSH), shadowBounds, tlr, trr, blr, brr, &mBlend);

    // Embossed style unpressed
    if (mStyle.emboss)
    {
      // Positive light
      g.FillRoundRect(GetColor(kPR), handleBounds, tlr, trr, blr, brr /*, &blend*/);

      // Negative light
      g.FillRoundRect(GetColor(kSH), shadowBounds, tlr, trr, blr, brr /*, &blend*/);

      // Fill in foreground
      g.FillRoundRect(GetValue() > 0.5 ? GetColor(kX1) : COLOR_BLACK, centreBounds, tlr, trr, blr, brr, &mBlend);

      // Shade when hovered
      if (mMouseIsOver)
        g.FillRoundRect(GetColor(kHL), centreBounds, tlr, trr, blr, brr, &mBlend);
    }
    else
    {
      g.FillRoundRect(GetValue() > 0.5 ? GetColor(kX1) : COLOR_BLACK, handleBounds, tlr, trr, blr, brr /*, &blend*/);

      // Shade when hovered
      if (mMouseIsOver)
        g.FillRoundRect(GetColor(kHL), handleBounds, tlr, trr, blr, brr, &mBlend);
    }

    if (mStyle.drawFrame)
      g.DrawRoundRect(GetColor(kFR), handleBounds, tlr, trr, blr, brr, &mBlend, mStyle.frameThickness);
  }

  void DrawHandle(IGraphics& g, const IRECT& filledArea) override
  {
    IRECT r;
    if (GetSelectedIdx() == 0)
    {
      r = filledArea.GetFromLeft(mBitmap.W());
    }
    else
    {
      r = filledArea.GetFromRight(mBitmap.W());
    }

    g.DrawBitmap(mBitmap, r, 0, 0, nullptr);
  }
};

class NAMFileNameControl : public IVButtonControl
{
public:
  NAMFileNameControl(const IRECT& bounds, const char* label, const IVStyle& style)
  : IVButtonControl(bounds, DefaultClickActionFunc, label, style)
  {
  }

  void SetLabelAndTooltip(const char* str)
  {
    SetLabelStr(str);
    SetTooltip(str);
  }

  void SetLabelAndTooltipEllipsizing(const WDL_String& fileName)
  {
    auto EllipsizeFilePath = [](const char* filePath, size_t prefixLength, size_t suffixLength, size_t maxLength) {
      const std::string ellipses = "...";
      assert(maxLength <= (prefixLength + suffixLength + ellipses.size()));
      std::string str{filePath};

      if (str.length() <= maxLength)
      {
        return str;
      }
      else
      {
        return str.substr(0, prefixLength) + ellipses + str.substr(str.length() - suffixLength);
      }
    };

    auto ellipsizedFileName = EllipsizeFilePath(fileName.get_filepart(), 22, 22, 45);
    SetLabelStr(ellipsizedFileName.c_str());
    SetTooltip(fileName.get_filepart());
  }
};

// URL control for the "Get" models/irs links
class NAMGetButtonControl : public NAMSquareButtonControl
{
public:
  NAMGetButtonControl(const IRECT& bounds, const char* label, const char* url, const ISVG& globeSVG)
  : NAMSquareButtonControl(
      bounds,
      [url](IControl* pCaller) {
        WDL_String fullURL(url);
        pCaller->GetUI()->OpenURL(fullURL.Get());
      },
      globeSVG)
  {
    SetTooltip(label);
  }
};

enum class NAMLibraryFilter
{
  All,
  Favorites,
  RecentlyUsed,
  MostUsed,
  RecentlyAdded,
  Groups,
  Tone3000
};

// ToneCast: generic (no tone3000-client dependency, see
// NAMLibraryPanelControl::SetRemoteBrowseHandler) view of one TONE3000
// search result, for the panel's "TONE3000" tab.
enum class NAMRemoteItemState
{
  NotDownloaded,
  Downloading,
  Downloaded,
  Failed
};

struct NAMRemoteItem
{
  std::string id; // tone id, as a string so this struct stays untyped
  std::string title;
  std::string subtitle; // e.g. "amp · nam"
  bool downloadable = false;
  NAMRemoteItemState state = NAMRemoteItemState::NotDownloaded;
};

// ToneCast: shared library-browsing state and helpers, used by both
// NAMFileSearchOverlayControl (the per-browser modal opened from the
// "Select model/IR..." rows) and NAMLibraryPanelControl (the combined,
// toggleable drawer covering both models and IRs). Favoriting/usage
// tracking must be the same set regardless of which UI touched it, so
// this lives at namespace scope rather than as a static member of either
// class. Session-only (resets on app restart) -- real persistence belongs
// to the future file-cache/preset system, not invented ad hoc here.
static inline std::unordered_set<std::string> sFavorites;
static inline std::unordered_map<std::string, int> sUsageCounts;
static inline std::unordered_map<std::string, uint64_t> sFirstSeen;
static inline std::vector<std::string> sRecent;
static inline uint64_t sSeenCounter = 0;

inline const char* FilterLabel(NAMLibraryFilter filter)
{
  switch (filter)
  {
    case NAMLibraryFilter::All: return "All models";
    case NAMLibraryFilter::Favorites: return "Favorites";
    case NAMLibraryFilter::RecentlyUsed: return "Recently used";
    case NAMLibraryFilter::MostUsed: return "Most used";
    case NAMLibraryFilter::RecentlyAdded: return "Recently added";
    case NAMLibraryFilter::Groups: return "Groups";
    case NAMLibraryFilter::Tone3000: return "TONE3000";
  }
  return "All models";
}

inline std::string GroupKey(const std::string& name)
{
  const size_t bracketEnd = (!name.empty() && name.front() == '[') ? name.find(']') : std::string::npos;
  if (bracketEnd != std::string::npos)
    return name.substr(0, bracketEnd + 1);
  const size_t split = name.find_first_of(" -_");
  return name.substr(0, split == std::string::npos ? name.size() : split);
}

inline void DrawFavoriteStar(IGraphics& g, const IRECT& r, bool filled, const IColor& color)
{
  constexpr int kPoints = 10;
  const float outer = std::min(r.W(), r.H()) * 0.34f;
  const float inner = outer * 0.44f;
  for (int i = 0; i < kPoints; i++)
  {
    const float radius = (i % 2 == 0) ? outer : inner;
    const float angle = DegToRad(-90.f + static_cast<float>(i) * 36.f);
    const float x = r.MW() + std::cos(angle) * radius;
    const float y = r.MH() + std::sin(angle) * radius;
    if (i == 0)
      g.PathMoveTo(x, y);
    else
      g.PathLineTo(x, y);
  }
  g.PathClose();
  if (filled)
    g.PathFill(color);
  else
    g.PathStroke(color, 1.25f);
}

inline std::string TruncateToFit(IGraphics& g, const IText& text, const std::string& str, float maxWidth)
{
  IRECT measured;
  if (g.MeasureText(text, str.c_str(), measured) <= maxWidth || str.size() <= 1)
    return str;

  const std::string ellipsis = "...";
  std::string candidate = str;
  while (candidate.size() > 1)
  {
    candidate.pop_back();
    const std::string withEllipsis = candidate + ellipsis;
    if (g.MeasureText(text, withEllipsis.c_str(), measured) <= maxWidth)
      return withEllipsis;
  }
  return ellipsis;
}

inline void RecordUse(const std::string& name)
{
  ++sUsageCounts[name];
  sRecent.erase(std::remove(sRecent.begin(), sRecent.end(), name), sRecent.end());
  sRecent.insert(sRecent.begin(), name);
  if (sRecent.size() > 100)
    sRecent.resize(100);
}

inline void ToggleLibraryFavorite(const std::string& name)
{
  if (sFavorites.count(name) > 0)
    sFavorites.erase(name);
  else
    sFavorites.insert(name);
}

// ToneCast (Task 3.1 follow-up): a searchable, multi-column replacement for
// the plain native/single-column popup menu that NAMFileBrowserControl used
// for its model/IR list. Self-contained: draws its own backdrop, search
// box, and item grid rather than spawning child IControls, similar in
// spirit to how iPlug2's own IPopupMenuControl hit-tests manually.
// Filtering commits when the search box loses focus/Enter is pressed
// (iPlug2's CreateTextEntry doesn't offer live per-keystroke callbacks),
// which is still a large improvement over scrolling a single long list.
class NAMFileSearchOverlayControl : public IControl
{
public:
  NAMFileSearchOverlayControl(const IRECT& bounds, const IVStyle& style, const WDL_PtrList<IPopupMenu::Item>& items,
                              std::function<void(int)> onSelectIndex)
  : IControl(bounds)
  , mStyle(style)
  , mOnSelectIndex(onSelectIndex)
  {
    mIgnoreMouse = false;
    for (int i = 0; i < items.GetSize(); i++)
    {
      const std::string name = items.Get(i)->GetText();
      mAllItems.push_back({name, i});
      if (sFirstSeen.count(name) == 0)
        sFirstSeen[name] = ++sSeenCounter;
    }
    mFiltered = mAllItems;
  }

  void OnAttached() override
  {
    Recalculate();
    RecomputeColumns();
    Recalculate();
  }

  void Draw(IGraphics& g) override
  {
    g.FillRect(COLOR_BLACK.WithOpacity(0.65f), mRECT);
    g.FillRoundRect(mStyle.colorSpec.GetColor(kBG), mPanelRect, 8.f);
    g.DrawRoundRect(mStyle.colorSpec.GetColor(kFR), mPanelRect, 8.f, nullptr, 1.f);

    g.FillRoundRect(mStyle.colorSpec.GetColor(kFG), mSidebarRect, 6.f);
    g.DrawText(mStyle.valueText.WithAlign(EAlign::Near).WithSize(18.f), "LIBRARY",
               mLibraryTitleRect.GetPadded(-12.f, 0.f, 0.f, 0.f));
    for (size_t i = 0; i < mFilterRects.size(); i++)
    {
      const bool active = static_cast<int>(mActiveFilter) == static_cast<int>(i);
      if (active)
      {
        g.FillRoundRect(mStyle.colorSpec.GetColor(kPR), mFilterRects[i], 4.f);
        g.FillRect(ToneCastColors::ACCENT, mFilterRects[i].GetFromLeft(3.f));
      }
      g.DrawText(mStyle.valueText.WithAlign(EAlign::Near), FilterLabel(static_cast<NAMLibraryFilter>(i)),
                 mFilterRects[i].GetPadded(-14.f, 0.f, -6.f, 0.f));
    }

    g.FillRoundRect(mStyle.colorSpec.GetColor(kFG), mSearchRect, 4.f);
    g.DrawRoundRect(mStyle.colorSpec.GetColor(kFR), mSearchRect, 4.f, nullptr, 1.f);
    const std::string display = mFilter.empty() ? std::string("Type to search...") : mFilter;
    g.DrawText(mStyle.valueText.WithAlign(EAlign::Near), display.c_str(), mSearchRect.GetPadded(-10.f, 0.f, -10.f, 0.f));

    const std::string countStr = std::to_string(mFiltered.size()) + " / " + std::to_string(mAllItems.size());
    g.DrawText(mStyle.labelText.WithAlign(EAlign::Far), countStr.c_str(), mSearchRect.GetPadded(-10.f, 0.f, -10.f, 0.f));

    const IText cellText = mStyle.valueText.WithAlign(EAlign::Near);
    g.PathClipRegion(mGridRect);
    for (size_t i = 0; i < mCellRects.size(); i++)
    {
      const auto& r = mCellRects[i];
      if (!r.Intersects(mGridRect))
        continue;
      const bool hovered = mHoveredIdx == static_cast<int>(i);
      g.FillRoundRect(hovered ? mStyle.colorSpec.GetColor(kPR) : mStyle.colorSpec.GetColor(kFG), r, 3.f);
      // Clip per-cell too: a truncated string can still measure slightly
      // wider than the cell once drawn (font hinting/kerning), and without
      // this a long label can bleed into the next column.
      g.PathClipRegion(r);

      const bool isFavorite = sFavorites.count(mFiltered[i].first) > 0;
      const IColor starColor = isFavorite ? ToneCastColors::ACCENT : mStyle.colorSpec.GetColor(kX1).WithOpacity(0.45f);
      DrawFavoriteStar(g, mFavoriteRects[i], isFavorite, starColor);

      const IRECT textRect = r.GetReducedFromLeft(mFavoriteRects[i].W() + 4.f).GetPadded(-4.f, 0.f, -6.f, 0.f);
      g.DrawText(cellText, TruncateToFit(g, cellText, mFiltered[i].first, textRect.W()).c_str(), textRect);
      g.PathClipRegion(mGridRect);
    }
    g.PathClipRegion();

    if (mFiltered.empty())
      g.DrawText(mStyle.labelText, "No matches", mGridRect);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    for (size_t i = 0; i < mFilterRects.size(); i++)
    {
      if (mFilterRects[i].Contains(x, y))
      {
        mActiveFilter = static_cast<NAMLibraryFilter>(i);
        ApplyFilter();
        SetDirty(false);
        return;
      }
    }

    if (mSearchRect.Contains(x, y))
    {
      OpenSearchBox();
      return;
    }

    for (size_t i = 0; i < mFavoriteRects.size(); i++)
    {
      if (mFavoriteRects[i].Contains(x, y))
      {
        ToggleFavorite(mFiltered[i].first);
        SetDirty(false);
        return;
      }
    }

    const int idx = HitTestCell(x, y);
    if (idx >= 0)
    {
      RecordUse(mFiltered[idx].first);
      mOnSelectIndex(mFiltered[idx].second);
      Dismiss();
      return;
    }

    if (!mPanelRect.Contains(x, y))
      Dismiss();
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    mHoveredIdx = HitTestCell(x, y);
    SetDirty(false);
  }

  void OnMouseOut() override
  {
    mHoveredIdx = -1;
    SetDirty(false);
  }

  void OnMouseWheel(float x, float y, const IMouseMod& mod, float d) override
  {
    mScrollOffset -= d * mRowHeight;
    ClampScroll();
    Recalculate();
    SetDirty(false);
  }

  bool OnKeyDown(float x, float y, const IKeyPress& key) override
  {
    if (key.VK == kVK_ESCAPE)
    {
      Dismiss();
      return true;
    }
    return false;
  }

  void OnTextEntryCompletion(const char* str, int valIdx) override
  {
    mFilter = str ? str : "";
    ApplyFilter();
    SetDirty(false);
  }

private:
  // FilterLabel/GroupKey/DrawFavoriteStar/TruncateToFit now live at
  // namespace scope above (shared with NAMLibraryPanelControl).

  int HitTestCell(float x, float y) const
  {
    if (!mGridRect.Contains(x, y))
      return -1;
    for (size_t i = 0; i < mCellRects.size(); i++)
    {
      if (mCellRects[i].Contains(x, y))
        return static_cast<int>(i);
    }
    return -1;
  }

  void OpenSearchBox() { GetUI()->CreateTextEntry(*this, mStyle.valueText, mSearchRect, mFilter.c_str()); }

  void ApplyFilter()
  {
    mFiltered.clear();
    std::string lowerFilter = mFilter;
    std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
    for (const auto& item : mAllItems)
    {
      std::string lowerName = item.first;
      std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
      const bool searchMatch = lowerFilter.empty() || lowerName.find(lowerFilter) != std::string::npos;
      bool categoryMatch = true;
      if (mActiveFilter == NAMLibraryFilter::Favorites)
        categoryMatch = sFavorites.count(item.first) > 0;
      else if (mActiveFilter == NAMLibraryFilter::RecentlyUsed || mActiveFilter == NAMLibraryFilter::MostUsed)
        categoryMatch = sUsageCounts[item.first] > 0;

      if (searchMatch && categoryMatch)
        mFiltered.push_back(item);
    }

    if (mActiveFilter == NAMLibraryFilter::RecentlyUsed)
    {
      std::stable_sort(mFiltered.begin(), mFiltered.end(), [](const auto& a, const auto& b) {
        return std::find(sRecent.begin(), sRecent.end(), a.first) < std::find(sRecent.begin(), sRecent.end(), b.first);
      });
    }
    else if (mActiveFilter == NAMLibraryFilter::MostUsed)
    {
      std::stable_sort(mFiltered.begin(), mFiltered.end(), [](const auto& a, const auto& b) {
        return sUsageCounts[a.first] == sUsageCounts[b.first] ? a.first < b.first
                                                              : sUsageCounts[a.first] > sUsageCounts[b.first];
      });
    }
    else if (mActiveFilter == NAMLibraryFilter::RecentlyAdded)
    {
      std::stable_sort(mFiltered.begin(), mFiltered.end(),
                       [](const auto& a, const auto& b) { return sFirstSeen[a.first] > sFirstSeen[b.first]; });
    }
    else if (mActiveFilter == NAMLibraryFilter::Groups)
    {
      std::stable_sort(mFiltered.begin(), mFiltered.end(), [](const auto& a, const auto& b) {
        const auto groupA = GroupKey(a.first);
        const auto groupB = GroupKey(b.first);
        return groupA == groupB ? a.first < b.first : groupA < groupB;
      });
    }
    else
    {
      std::stable_sort(mFiltered.begin(), mFiltered.end(),
                       [](const auto& a, const auto& b) { return a.first < b.first; });
    }
    mScrollOffset = 0.f;
    RecomputeColumns();
    Recalculate();
  }

  void ToggleFavorite(const std::string& name)
  {
    ToggleLibraryFavorite(name);
    ApplyFilter();
  }

  // How many columns fit if every column is wide enough to show the
  // longest name in the current list without truncating it. Only called
  // when the filtered set changes (not on every scroll tick) since
  // MeasureText isn't free over hundreds of items.
  void RecomputeColumns()
  {
    const float favoriteW = 24.f;
    const float cellPad = favoriteW + 20.f;
    float widestText = 90.f; // floor, so a short/empty list doesn't get one giant column
    if (IGraphics* ui = GetUI())
    {
      IRECT measured;
      for (const auto& item : mFiltered)
        widestText = std::max(widestText, ui->MeasureText(mStyle.valueText, item.first.c_str(), measured));
    }
    const float availableW = mGridRect.W() > 0.f ? mGridRect.W() : mRECT.W();
    const float idealCellW = widestText + cellPad;
    mNumCols = std::max(1, static_cast<int>(availableW / idealCellW));
  }

  void Recalculate()
  {
    // As big as the window will allow — user asked for "way bigger than
    // the actual app size"; this is the biggest this overlay can be
    // without literally resizing the platform window.
    mPanelRect = mRECT.GetPadded(-8.f);
    mSidebarRect = mPanelRect.GetFromLeft(182.f).GetPadded(-8.f);
    mLibraryTitleRect = mSidebarRect.GetFromTop(54.f);
    mFilterRects.clear();
    float filterTop = mLibraryTitleRect.B + 8.f;
    for (int i = 0; i < 6; i++)
    {
      mFilterRects.emplace_back(mSidebarRect.L + 6.f, filterTop, mSidebarRect.R - 6.f, filterTop + 38.f);
      filterTop += 43.f;
    }

    const auto contentRect = mPanelRect.GetReducedFromLeft(190.f).GetPadded(-10.f);
    mSearchRect = contentRect.GetFromTop(40.f);
    mGridRect = contentRect.GetReducedFromTop(50.f);

    const float cellW = mGridRect.W() / static_cast<float>(mNumCols);
    const float cellH = 32.f;
    const float favoriteW = 24.f;
    mRowHeight = cellH + 6.f;

    mCellRects.clear();
    mFavoriteRects.clear();
    for (size_t i = 0; i < mFiltered.size(); i++)
    {
      const int col = static_cast<int>(i) % mNumCols;
      const int row = static_cast<int>(i) / mNumCols;
      const float top = mGridRect.T + static_cast<float>(row) * mRowHeight - mScrollOffset;
      const IRECT cell(mGridRect.L + col * cellW + 2.f, top, mGridRect.L + (col + 1) * cellW - 2.f, top + cellH);
      mCellRects.push_back(cell);
      mFavoriteRects.push_back(cell.GetFromLeft(favoriteW).GetPadded(-2.f));
    }
  }

  void ClampScroll()
  {
    const int numRows = static_cast<int>(std::ceil(static_cast<float>(mFiltered.size()) / mNumCols));
    const float contentHeight = numRows * mRowHeight;
    const float maxScroll = std::max(0.f, contentHeight - mGridRect.H());
    mScrollOffset = Clip(mScrollOffset, 0.f, maxScroll);
  }

  void Dismiss() { GetUI()->RemoveControl(this); }

  int mNumCols = 1; // recomputed by RecomputeColumns() so columns stay wide enough for full names

  IVStyle mStyle;
  std::function<void(int)> mOnSelectIndex;
  std::vector<std::pair<std::string, int>> mAllItems; // display text, original IPopupMenu::Item index
  std::vector<std::pair<std::string, int>> mFiltered;
  std::vector<IRECT> mCellRects;
  std::vector<IRECT> mFavoriteRects;
  std::vector<IRECT> mFilterRects;
  std::string mFilter;
  IRECT mPanelRect, mSidebarRect, mLibraryTitleRect, mSearchRect, mGridRect;
  float mScrollOffset = 0.f;
  float mRowHeight = 36.f;
  int mHoveredIdx = -1;
  NAMLibraryFilter mActiveFilter = NAMLibraryFilter::All;
  // sFavorites/sUsageCounts/sFirstSeen/sRecent/sSeenCounter now live at
  // namespace scope above (shared with NAMLibraryPanelControl).
};

class NAMFileBrowserControl : public IDirBrowseControlBase
{
public:
  NAMFileBrowserControl(const IRECT& bounds, int clearMsgTag, const char* labelStr, const char* fileExtension,
                        IFileDialogCompletionHandlerFunc ch, const IVStyle& style, const ISVG& loadSVG,
                        const ISVG& clearSVG, const ISVG& leftSVG, const ISVG& rightSVG, const IBitmap& bitmap,
                        const ISVG& globeSVG, const char* getButtonLabel, const char* getButtonURL)
  : IDirBrowseControlBase(bounds, fileExtension, false, false)
  , mClearMsgTag(clearMsgTag)
  , mDefaultLabelStr(labelStr)
  , mCompletionHandlerFunc(ch)
  , mStyle(style.WithColor(kFG, COLOR_TRANSPARENT).WithDrawFrame(false))
  , mBitmap(bitmap)
  , mLoadSVG(loadSVG)
  , mClearSVG(clearSVG)
  , mLeftSVG(leftSVG)
  , mRightSVG(rightSVG)
  , mGlobeSVG(globeSVG)
  , mGetButtonLabel(getButtonLabel)
  , mGetButtonURL(getButtonURL)
  , mBrowserState(NAMBrowserState::Empty)
  {
    mIgnoreMouse = true;
  }

  // ToneCast (Task 3.1): vector-drawn box instead of a bitmap texture
  // (mBitmap is still stored/passed in for now, unused here — see
  // docs/decisions-log.md).
  void Draw(IGraphics& g) override
  {
    // The generated rack slot already supplies the background, border and
    // inset shadow. Drawing a second full rectangle here made the selector
    // look like it was floating above (and vertically outside) the slot.
  }

  void OnPopupMenuSelection(IPopupMenu* pSelectedMenu, int valIdx) override
  {
    if (pSelectedMenu)
    {
      IPopupMenu::Item* pItem = pSelectedMenu->GetChosenItem();

      if (pItem)
      {
        mSelectedItemIndex = mItems.Find(pItem);
        LoadFileAtCurrentIndex();
      }
    }
  }

  void OnAttached() override
  {
    auto prevFileFunc = [&](IControl* pCaller) {
      const auto nItems = NItems();
      if (nItems == 0)
        return;
      mSelectedItemIndex--;

      if (mSelectedItemIndex < 0)
        mSelectedItemIndex = nItems - 1;

      LoadFileAtCurrentIndex();
    };

    auto nextFileFunc = [&](IControl* pCaller) {
      const auto nItems = NItems();
      if (nItems == 0)
        return;
      mSelectedItemIndex++;

      if (mSelectedItemIndex >= nItems)
        mSelectedItemIndex = 0;

      LoadFileAtCurrentIndex();
    };

    auto loadFileFunc = [&](IControl* pCaller) {
      WDL_String fileName;
      WDL_String path;
      GetSelectedFileDirectory(path);
#ifdef NAM_PICK_DIRECTORY
      pCaller->GetUI()->PromptForDirectory(path, [&](const WDL_String& fileName, const WDL_String& path) {
        if (path.GetLength())
        {
          ClearPathList();
          AddPath(path.Get(), "");
          SetupMenu();
          SelectFirstFile();
          LoadFileAtCurrentIndex();
        }
      });
#else
      pCaller->GetUI()->PromptForFile(
        fileName, path, EFileAction::Open, mExtension.Get(), [&](const WDL_String& fileName, const WDL_String& path) {
          if (fileName.GetLength())
          {
            ClearPathList();
            AddPath(path.Get(), "");
            SetupMenu();
            SetSelectedFile(fileName.Get());
            LoadFileAtCurrentIndex();
          }
        });
#endif
    };

    auto clearFileFunc = [&](IControl* pCaller) {
      pCaller->GetDelegate()->SendArbitraryMsgFromUI(mClearMsgTag);
      mFileNameControl->SetLabelAndTooltip(mDefaultLabelStr.Get());
      SetBrowserState(NAMBrowserState::Empty);
      // FIXME disabling output mode...
      //      pCaller->GetUI()->GetControlWithTag(kCtrlTagOutputMode)->SetDisabled(false);
    };

    auto chooseFileFunc = [&, loadFileFunc](IControl* pCaller) {
      if (std::string_view(pCaller->As<IVButtonControl>()->GetLabelStr()) == mDefaultLabelStr.Get())
      {
        loadFileFunc(pCaller);
      }
      else
      {
        CheckSelectedItem();

        if (!mMainMenu.HasSubMenus())
        {
          mMainMenu.SetChosenItemIdx(mSelectedItemIndex);
        }
        // ToneCast (Task 3.1 follow-up): searchable, multi-column overlay
        // instead of the platform-native single-column scrolling menu.
        pCaller->GetUI()->AttachControl(new NAMFileSearchOverlayControl(
          pCaller->GetUI()->GetBounds(), mStyle, mItems, [this](int idx) {
            mSelectedItemIndex = idx;
            LoadFileAtCurrentIndex();
          }));
      }
    };

    IRECT padded = mRECT.GetPadded(-6.f).GetHPadded(-2.f);
    const auto buttonWidth = padded.H();
    const auto loadFileButtonBounds = padded.ReduceFromLeft(buttonWidth);
    const auto clearAndGetButtonBounds = padded.ReduceFromRight(buttonWidth);
    const auto leftButtonBounds = padded.ReduceFromLeft(buttonWidth);
    const auto rightButtonBounds = padded.ReduceFromLeft(buttonWidth);
    const auto fileNameButtonBounds = padded;

    AddChildControl(new NAMSquareButtonControl(loadFileButtonBounds, DefaultClickActionFunc, mLoadSVG))
      ->SetAnimationEndActionFunction(loadFileFunc);
    AddChildControl(new NAMSquareButtonControl(leftButtonBounds, DefaultClickActionFunc, mLeftSVG))
      ->SetAnimationEndActionFunction(prevFileFunc);
    AddChildControl(new NAMSquareButtonControl(rightButtonBounds, DefaultClickActionFunc, mRightSVG))
      ->SetAnimationEndActionFunction(nextFileFunc);
    AddChildControl(mFileNameControl = new NAMFileNameControl(fileNameButtonBounds, mDefaultLabelStr.Get(), mStyle))
      ->SetAnimationEndActionFunction(chooseFileFunc);

    // creates both right-side controls but only show one based on state
    mClearButton = new NAMSquareButtonControl(clearAndGetButtonBounds, DefaultClickActionFunc, mClearSVG);
    mClearButton->SetAnimationEndActionFunction(clearFileFunc);
    AddChildControl(mClearButton);

    mGetButton = new NAMGetButtonControl(clearAndGetButtonBounds, mGetButtonLabel, mGetButtonURL, mGlobeSVG);
    AddChildControl(mGetButton);

    // initialize control visibility
    SetBrowserState(NAMBrowserState::Empty);
  }

  void LoadFileAtCurrentIndex()
  {
    if (mSelectedItemIndex > -1 && mSelectedItemIndex < NItems())
    {
      WDL_String fileName, path;
      GetSelectedFile(fileName);
      mFileNameControl->SetLabelAndTooltipEllipsizing(fileName);
      mCompletionHandlerFunc(fileName, path);
    }
  }

  // ToneCast: public accessors so NAMLibraryPanelControl (a persistent,
  // toggleable panel covering both models and IRs) can read this browser's
  // currently-known items and trigger a load, without duplicating the
  // directory-scanning logic in IDirBrowseControlBase.
  const WDL_PtrList<IPopupMenu::Item>& GetItems() const { return mItems; }

  void SelectAndLoadIndex(int index)
  {
    mSelectedItemIndex = index;
    LoadFileAtCurrentIndex();
  }

  // ToneCast: populates this browser's item list from `dir` without the
  // user having to browse for it interactively -- used to restore the
  // last-used folder on startup (see NAMUserSettings.h and the
  // #ifdef APP_API block in NeuralAmpModeler.cpp's layout function).
  // Deliberately doesn't call LoadFileAtCurrentIndex() -- the caller
  // stages the remembered file directly (see that same block) so a
  // missing/moved file fails silently instead of popping an error
  // dialog on every launch.
  void ScanDirectory(const char* dir)
  {
    ClearPathList();
    AddPath(dir, "");
    SetupMenu();
    SelectFirstFile();
  }

  void OnMsgFromDelegate(int msgTag, int dataSize, const void* pData) override
  {
    switch (msgTag)
    {
      case kMsgTagLoadFailed:
        // Honestly, not sure why I made a big stink of it before. Why not just say it failed and move on? :)
        {
          std::string label(std::string("(FAILED) ") + std::string(mFileNameControl->GetLabelStr()));
          mFileNameControl->SetLabelAndTooltip(label.c_str());
          SetBrowserState(NAMBrowserState::Empty);
        }
        break;
      case kMsgTagLoadedModel:
      case kMsgTagLoadedIR:
      {
        WDL_String fileName, directory;
        fileName.Set(reinterpret_cast<const char*>(pData));
        directory.Set(reinterpret_cast<const char*>(pData));
        directory.remove_filepart(true);

        ClearPathList();
        AddPath(directory.Get(), "");
        SetupMenu();
        SetSelectedFile(fileName.Get());
        mFileNameControl->SetLabelAndTooltipEllipsizing(fileName);
        SetBrowserState(NAMBrowserState::Loaded);
      }
      break;
      default: break;
    }
  }

private:
  void SelectFirstFile() { mSelectedItemIndex = mFiles.GetSize() ? 0 : -1; }

  void GetSelectedFileDirectory(WDL_String& path)
  {
    GetSelectedFile(path);
    path.remove_filepart();
    return;
  }

  // set the state of the browser and the visibility of the "Get" vs. "Clear" buttons
  void SetBrowserState(NAMBrowserState newState)
  {
    mBrowserState = newState;

    switch (mBrowserState)
    {
      case NAMBrowserState::Empty:
        mClearButton->Hide(true);
        mGetButton->Hide(false);
        break;
      case NAMBrowserState::Loaded:
        mClearButton->Hide(false);
        mGetButton->Hide(true);
        break;
    }
  }

  WDL_String mDefaultLabelStr;
  IFileDialogCompletionHandlerFunc mCompletionHandlerFunc;
  NAMFileNameControl* mFileNameControl = nullptr;
  IVStyle mStyle;
  IBitmap mBitmap;
  ISVG mLoadSVG, mClearSVG, mLeftSVG, mRightSVG, mGlobeSVG;
  int mClearMsgTag;

  // new members for the "Get" button
  const char* mGetButtonLabel;
  const char* mGetButtonURL;
  NAMBrowserState mBrowserState;
  NAMSquareButtonControl* mClearButton = nullptr;
  NAMGetButtonControl* mGetButton = nullptr;
};

// ToneCast (Task 3.1): a persistent, toggleable drawer docked to the left
// that combines both the model and IR libraries into one organized list —
// unlike NAMFileSearchOverlayControl above (a modal triggered per-browser
// from the "Select model/IR..." rows, one file type at a time), this is
// reachable via a dedicated toggle button and shows both together. Shares
// the same filter categories, favorites, usage tracking, and drawing
// helpers (namespace scope, above) so favoriting/recents stay consistent
// between the two UIs.
class NAMLibraryPanelControl : public IControl
{
public:
  NAMLibraryPanelControl(const IRECT& bounds, const IVStyle& style)
  : IControl(bounds)
  , mStyle(style)
  {
    mIgnoreMouse = false;
  }

  void SetBrowsers(NAMFileBrowserControl* modelBrowser, NAMFileBrowserControl* irBrowser)
  {
    mModelBrowser = modelBrowser;
    mIRBrowser = irBrowser;
  }

  // ToneCast: generic hook for a TONE3000 connect affordance, deliberately
  // untyped so this shared-UI class (compiled into both app and VST3) has
  // no reference to tone3000-client/ (standalone-only, see
  // Tone3000Config.h). Unset by default -- NeuralAmpModeler.cpp wires this
  // up only inside an #ifdef APP_API block, so the VST3 build never shows
  // or references it.
  void SetConnectHandler(std::function<void()> onClick, std::function<std::string()> statusText)
  {
    mOnConnectClick = std::move(onClick);
    mConnectStatusText = std::move(statusText);
  }

  // ToneCast: generic hook for the "TONE3000" tab's search/download, same
  // untyped pattern as SetConnectHandler above -- no tone3000-client
  // reference in this shared-UI class. `getResults` is polled fresh every
  // Draw() call rather than pushed via callback, since results live on a
  // background thread (see tone3000-client/Tone3000Browser.h) and this
  // control already redraws continuously.
  void SetRemoteBrowseHandler(std::function<bool()> isSignedIn, std::function<void(const std::string&)> onSearch,
                              std::function<std::vector<NAMRemoteItem>()> getResults,
                              std::function<std::string()> statusText,
                              std::function<void(const std::string&)> onDownloadClick)
  {
    mIsSignedIn = std::move(isSignedIn);
    mOnRemoteSearch = std::move(onSearch);
    mGetRemoteResults = std::move(getResults);
    mRemoteStatusText = std::move(statusText);
    mOnRemoteDownloadClick = std::move(onDownloadClick);
  }

  void OnAttached() override { Recalculate(); }

  void Hide(bool hide) override
  {
    IControl::Hide(hide);
    if (!hide)
      RefreshItems();
  }

  void Draw(IGraphics& g) override
  {
    // Bounds are the FULL canvas (see attachment in NeuralAmpModeler.cpp) so
    // that clicking anywhere outside the opaque sidebar reliably closes the
    // panel -- a narrower hit area made it possible for the panel to end up
    // effectively unclosable depending on exactly where the toggle button
    // sat relative to it. The dim backdrop covers everything; the actual
    // sidebar content only occupies mSidebarRect on the left.
    g.FillRect(COLOR_BLACK.WithOpacity(0.35f), mRECT);
    g.FillRect(ToneCastColors::BACKGROUND.WithOpacity(0.99f), mSidebarRect);
    g.DrawLine(mStyle.colorSpec.GetColor(kFR), mSidebarRect.R, mSidebarRect.T, mSidebarRect.R, mSidebarRect.B, nullptr,
               1.f);

    g.DrawText(mStyle.valueText.WithAlign(EAlign::Near).WithSize(18.f), "LIBRARY",
               mTitleRect.GetPadded(-12.f, 0.f, 0.f, 0.f));
    DrawCloseGlyph(g, mCloseRect);

    if (mOnConnectClick)
    {
      const std::string status = mConnectStatusText ? mConnectStatusText() : std::string("Connect TONE3000");
      g.FillRoundRect(mStyle.colorSpec.GetColor(kFG), mConnectRect, 4.f);
      g.DrawRoundRect(ToneCastColors::ACCENT.WithOpacity(0.6f), mConnectRect, 4.f, nullptr, 1.f);
      g.DrawText(mStyle.valueText.WithAlign(EAlign::Near).WithSize(12.f).WithFGColor(ToneCastColors::ACCENT),
                 status.c_str(), mConnectRect.GetPadded(-8.f, 0.f, -8.f, 0.f));
    }

    for (size_t i = 0; i < mFilterRects.size(); i++)
    {
      const bool active = static_cast<int>(mActiveFilter) == static_cast<int>(i);
      if (active)
      {
        g.FillRoundRect(mStyle.colorSpec.GetColor(kPR), mFilterRects[i], 4.f);
        g.FillRect(ToneCastColors::ACCENT, mFilterRects[i].GetFromLeft(3.f));
      }
      g.DrawText(mStyle.valueText.WithAlign(EAlign::Near).WithSize(13.f),
                 FilterLabel(static_cast<NAMLibraryFilter>(i)), mFilterRects[i].GetPadded(-12.f, 0.f, -6.f, 0.f));
    }

    const bool remoteTab = mActiveFilter == NAMLibraryFilter::Tone3000;
    if (remoteTab && mGetRemoteResults)
      mRemoteItemsCache = mGetRemoteResults();

    g.FillRoundRect(mStyle.colorSpec.GetColor(kFG), mSearchRect, 4.f);
    g.DrawRoundRect(mStyle.colorSpec.GetColor(kFR), mSearchRect, 4.f, nullptr, 1.f);
    const std::string placeholder = remoteTab ? std::string("Search TONE3000...") : std::string("Search library...");
    const std::string display = mFilter.empty() ? placeholder : mFilter;
    g.DrawText(mStyle.valueText.WithAlign(EAlign::Near), display.c_str(),
               mSearchRect.GetPadded(-10.f, 0.f, -10.f, 0.f));

    const std::string statusStr = (remoteTab && mRemoteStatusText)
                                    ? mRemoteStatusText()
                                    : std::to_string(mFiltered.size()) + " / " + std::to_string(mAllItems.size());
    g.DrawText(mStyle.labelText.WithAlign(EAlign::Near).WithSize(11.f), statusStr.c_str(), mCountRect);

    const IText rowText = mStyle.valueText.WithAlign(EAlign::Near);
    g.PathClipRegion(mListRect);
    if (remoteTab)
    {
      for (size_t i = 0; i < mRowRects.size() && i < mRemoteItemsCache.size(); i++)
      {
        const auto& r = mRowRects[i];
        if (!r.Intersects(mListRect))
          continue;
        const bool hovered = mHoveredIdx == static_cast<int>(i);
        if (hovered)
          g.FillRoundRect(mStyle.colorSpec.GetColor(kPR), r, 3.f);

        const auto& item = mRemoteItemsCache[i];

        IColor dotColor = mStyle.colorSpec.GetColor(kX1).WithOpacity(item.downloadable ? 0.4f : 0.15f);
        if (item.state == NAMRemoteItemState::Downloading)
          dotColor = ToneCastColors::ACCENT;
        else if (item.state == NAMRemoteItemState::Downloaded)
          dotColor = IColor(255, 90, 200, 110);
        else if (item.state == NAMRemoteItemState::Failed)
          dotColor = IColor(255, 210, 80, 80);
        g.FillCircle(dotColor, mStarRects[i].MW(), mStarRects[i].MH(), 4.f);

        const IColor badgeColor = item.downloadable ? ToneCastColors::ACCENT.WithOpacity(0.75f)
                                                     : mStyle.colorSpec.GetColor(kX1).WithOpacity(0.5f);
        g.DrawText(rowText.WithSize(10.f).WithFGColor(badgeColor), item.subtitle.c_str(), mTypeRects[i]);

        const IRECT actionRect = r.GetFromRight(70.f).GetPadded(-4.f, 0.f, -4.f, 0.f);
        const IRECT titleRect = r.GetReducedFromLeft(mStarRects[i].W() + mTypeRects[i].W() + 8.f)
                                  .GetReducedFromRight(70.f)
                                  .GetPadded(-4.f, 0.f, -2.f, 0.f);
        g.DrawText(rowText, TruncateToFit(g, rowText, item.title, titleRect.W()).c_str(), titleRect);

        const char* actionLabel = "Download";
        IColor actionColor = ToneCastColors::ACCENT;
        if (item.state == NAMRemoteItemState::Downloading)
        {
          actionLabel = "...";
          actionColor = mStyle.colorSpec.GetColor(kX1);
        }
        else if (item.state == NAMRemoteItemState::Downloaded)
        {
          actionLabel = "Installed";
          actionColor = IColor(255, 90, 200, 110);
        }
        else if (item.state == NAMRemoteItemState::Failed)
        {
          actionLabel = "Retry";
          actionColor = IColor(255, 210, 80, 80);
        }
        else if (!item.downloadable)
        {
          actionLabel = "N/A";
          actionColor = mStyle.colorSpec.GetColor(kX1).WithOpacity(0.4f);
        }
        g.DrawText(rowText.WithSize(10.f).WithFGColor(actionColor).WithAlign(EAlign::Far), actionLabel, actionRect);
      }
    }
    else
    {
      for (size_t i = 0; i < mRowRects.size(); i++)
      {
        const auto& r = mRowRects[i];
        if (!r.Intersects(mListRect))
          continue;
        const bool hovered = mHoveredIdx == static_cast<int>(i);
        if (hovered)
          g.FillRoundRect(mStyle.colorSpec.GetColor(kPR), r, 3.f);

        const auto& item = mFiltered[i];
        const bool isFavorite = sFavorites.count(item.name) > 0;
        const IColor starColor =
          isFavorite ? ToneCastColors::ACCENT : mStyle.colorSpec.GetColor(kX1).WithOpacity(0.45f);
        DrawFavoriteStar(g, mStarRects[i], isFavorite, starColor);

        const IColor typeColor = item.isIR ? ToneCastColors::ACCENT.WithOpacity(0.75f) : mStyle.colorSpec.GetColor(kX1);
        g.DrawText(rowText.WithSize(10.f).WithFGColor(typeColor), item.isIR ? "IR" : "AMP", mTypeRects[i]);

        const IRECT textRect =
          r.GetReducedFromLeft(mStarRects[i].W() + mTypeRects[i].W() + 8.f).GetPadded(-4.f, 0.f, -6.f, 0.f);
        g.DrawText(rowText, TruncateToFit(g, rowText, item.name, textRect.W()).c_str(), textRect);
      }
    }
    g.PathClipRegion();

    if (remoteTab)
    {
      if (!mIsSignedIn || !mIsSignedIn())
        g.DrawText(mStyle.labelText, "Connect TONE3000 above to browse", mListRect);
      else if (mRemoteItemsCache.empty())
        g.DrawText(mStyle.labelText, mFilter.empty() ? "Type a search and press Enter" : "No results", mListRect);
    }
    else if (mFiltered.empty())
    {
      const char* msg = mAllItems.empty() ? "No models or IRs loaded yet" : "No matches";
      g.DrawText(mStyle.labelText, msg, mListRect);
    }
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (mCloseRect.Contains(x, y) || !mSidebarRect.Contains(x, y))
    {
      Hide(true);
      return;
    }

    if (mOnConnectClick && mConnectRect.Contains(x, y))
    {
      mOnConnectClick();
      SetDirty(false);
      return;
    }

    for (size_t i = 0; i < mFilterRects.size(); i++)
    {
      if (mFilterRects[i].Contains(x, y))
      {
        mActiveFilter = static_cast<NAMLibraryFilter>(i);
        ApplyFilter();
        SetDirty(false);
        return;
      }
    }

    if (mSearchRect.Contains(x, y))
    {
      OpenSearchBox();
      return;
    }

    if (mActiveFilter == NAMLibraryFilter::Tone3000)
    {
      const int idx = HitTestRow(x, y);
      if (idx >= 0 && static_cast<size_t>(idx) < mRemoteItemsCache.size())
      {
        const auto& item = mRemoteItemsCache[idx];
        if (item.downloadable
            && (item.state == NAMRemoteItemState::NotDownloaded || item.state == NAMRemoteItemState::Failed)
            && mOnRemoteDownloadClick)
          mOnRemoteDownloadClick(item.id);
        SetDirty(false);
      }
      return;
    }

    for (size_t i = 0; i < mStarRects.size(); i++)
    {
      if (mStarRects[i].Contains(x, y))
      {
        ToggleLibraryFavorite(mFiltered[i].name);
        ApplyFilter();
        SetDirty(false);
        return;
      }
    }

    const int idx = HitTestRow(x, y);
    if (idx >= 0)
    {
      const auto& item = mFiltered[idx];
      RecordUse(item.name);
      NAMFileBrowserControl* target = item.isIR ? mIRBrowser : mModelBrowser;
      if (target)
        target->SelectAndLoadIndex(item.sourceIndex);
      SetDirty(false);
      return;
    }
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    mHoveredIdx = HitTestRow(x, y);
    SetDirty(false);
  }

  void OnMouseOut() override
  {
    mHoveredIdx = -1;
    SetDirty(false);
  }

  void OnMouseWheel(float x, float y, const IMouseMod& mod, float d) override
  {
    mScrollOffset -= d * mRowHeight;
    ClampScroll();
    Recalculate();
    SetDirty(false);
  }

  void OnTextEntryCompletion(const char* str, int valIdx) override
  {
    mFilter = str ? str : "";
    if (mActiveFilter == NAMLibraryFilter::Tone3000)
    {
      if (mOnRemoteSearch)
        mOnRemoteSearch(mFilter);
    }
    else
    {
      ApplyFilter();
    }
    SetDirty(false);
  }

private:
  struct LibraryItem
  {
    std::string name;
    int sourceIndex; // index into the owning browser's own item list
    bool isIR;
  };

  void DrawCloseGlyph(IGraphics& g, const IRECT& r) const
  {
    // Filled circular backdrop so this reads as an obvious button, not
    // just two thin lines that are easy to miss against a dark panel.
    g.FillEllipse(mStyle.colorSpec.GetColor(kFG), r);
    g.DrawEllipse(mStyle.colorSpec.GetColor(kFR), r, nullptr, 1.f);
    const IRECT glyph = r.GetPadded(-6.f);
    const IColor c = ToneCastColors::FG_TEXT;
    g.DrawLine(c, glyph.L, glyph.T, glyph.R, glyph.B, nullptr, 1.75f);
    g.DrawLine(c, glyph.L, glyph.B, glyph.R, glyph.T, nullptr, 1.75f);
  }

  void RefreshItems()
  {
    mAllItems.clear();
    auto pullFrom = [&](NAMFileBrowserControl* browser, bool isIR) {
      if (!browser)
        return;
      const auto& items = browser->GetItems();
      for (int i = 0; i < items.GetSize(); i++)
      {
        const std::string name = items.Get(i)->GetText();
        mAllItems.push_back({name, i, isIR});
        if (sFirstSeen.count(name) == 0)
          sFirstSeen[name] = ++sSeenCounter;
      }
    };
    pullFrom(mModelBrowser, false);
    pullFrom(mIRBrowser, true);
    ApplyFilter();
  }

  void OpenSearchBox() { GetUI()->CreateTextEntry(*this, mStyle.valueText, mSearchRect, mFilter.c_str()); }

  void ApplyFilter()
  {
    mFiltered.clear();
    std::string lowerFilter = mFilter;
    std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
    for (const auto& item : mAllItems)
    {
      std::string lowerName = item.name;
      std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
      const bool searchMatch = lowerFilter.empty() || lowerName.find(lowerFilter) != std::string::npos;
      bool categoryMatch = true;
      if (mActiveFilter == NAMLibraryFilter::Favorites)
        categoryMatch = sFavorites.count(item.name) > 0;
      else if (mActiveFilter == NAMLibraryFilter::RecentlyUsed || mActiveFilter == NAMLibraryFilter::MostUsed)
        categoryMatch = sUsageCounts[item.name] > 0;

      if (searchMatch && categoryMatch)
        mFiltered.push_back(item);
    }

    if (mActiveFilter == NAMLibraryFilter::RecentlyUsed)
    {
      std::stable_sort(mFiltered.begin(), mFiltered.end(), [](const auto& a, const auto& b) {
        return std::find(sRecent.begin(), sRecent.end(), a.name) < std::find(sRecent.begin(), sRecent.end(), b.name);
      });
    }
    else if (mActiveFilter == NAMLibraryFilter::MostUsed)
    {
      std::stable_sort(mFiltered.begin(), mFiltered.end(), [](const auto& a, const auto& b) {
        return sUsageCounts[a.name] == sUsageCounts[b.name] ? a.name < b.name : sUsageCounts[a.name] > sUsageCounts[b.name];
      });
    }
    else if (mActiveFilter == NAMLibraryFilter::RecentlyAdded)
    {
      std::stable_sort(mFiltered.begin(), mFiltered.end(),
                       [](const auto& a, const auto& b) { return sFirstSeen[a.name] > sFirstSeen[b.name]; });
    }
    else if (mActiveFilter == NAMLibraryFilter::Groups)
    {
      std::stable_sort(mFiltered.begin(), mFiltered.end(), [](const auto& a, const auto& b) {
        const auto groupA = GroupKey(a.name);
        const auto groupB = GroupKey(b.name);
        return groupA == groupB ? a.name < b.name : groupA < groupB;
      });
    }
    else
    {
      std::stable_sort(mFiltered.begin(), mFiltered.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
    }
    mScrollOffset = 0.f;
    Recalculate();
  }

  int HitTestRow(float x, float y) const
  {
    if (!mListRect.Contains(x, y))
      return -1;
    for (size_t i = 0; i < mRowRects.size(); i++)
    {
      if (mRowRects[i].Contains(x, y))
        return static_cast<int>(i);
    }
    return -1;
  }

  void Recalculate()
  {
    // Opaque sidebar is only the left portion of the full-canvas mRECT; the
    // remainder is the click-to-dismiss backdrop (see Draw()/OnMouseDown()).
    mSidebarRect = mRECT.GetFromLeft(300.f);

    mTitleRect = mSidebarRect.GetFromTop(40.f);
    mCloseRect = mTitleRect.GetFromRight(40.f).GetPadded(-8.f);

    mConnectRect = IRECT(mSidebarRect.L + 8.f, mTitleRect.B + 2.f, mSidebarRect.R - 8.f, mTitleRect.B + 28.f);
    const float belowConnect = mOnConnectClick ? mConnectRect.B + 6.f : mTitleRect.B + 4.f;

    mFilterRects.clear();
    float filterTop = belowConnect;
    for (int i = 0; i < 7; i++)
    {
      mFilterRects.emplace_back(mSidebarRect.L + 8.f, filterTop, mSidebarRect.R - 8.f, filterTop + 30.f);
      filterTop += 33.f;
    }

    mSearchRect = IRECT(mSidebarRect.L + 8.f, filterTop + 6.f, mSidebarRect.R - 8.f, filterTop + 40.f);
    mCountRect = IRECT(mSidebarRect.L + 8.f, mSearchRect.B + 2.f, mSidebarRect.R - 8.f, mSearchRect.B + 18.f);
    mListRect = IRECT(mSidebarRect.L + 8.f, mCountRect.B + 4.f, mSidebarRect.R - 8.f, mSidebarRect.B - 8.f);

    const float rowH = 30.f;
    const float starW = 20.f;
    const float typeW = 28.f;
    mRowHeight = rowH + 4.f;

    mRowRects.clear();
    mStarRects.clear();
    mTypeRects.clear();
    for (size_t i = 0; i < ActiveItemCount(); i++)
    {
      const float top = mListRect.T + static_cast<float>(i) * mRowHeight - mScrollOffset;
      const IRECT row(mListRect.L, top, mListRect.R, top + rowH);
      mRowRects.push_back(row);
      mStarRects.push_back(row.GetFromLeft(starW).GetPadded(-3.f));
      mTypeRects.push_back(row.GetFromLeft(starW + typeW).GetReducedFromLeft(starW));
    }
  }

  size_t ActiveItemCount() const
  {
    return mActiveFilter == NAMLibraryFilter::Tone3000 ? mRemoteItemsCache.size() : mFiltered.size();
  }

  void ClampScroll()
  {
    const float contentHeight = static_cast<float>(ActiveItemCount()) * mRowHeight;
    const float maxScroll = std::max(0.f, contentHeight - mListRect.H());
    mScrollOffset = Clip(mScrollOffset, 0.f, maxScroll);
  }

  IVStyle mStyle;
  NAMFileBrowserControl* mModelBrowser = nullptr;
  NAMFileBrowserControl* mIRBrowser = nullptr;
  std::vector<LibraryItem> mAllItems;
  std::vector<LibraryItem> mFiltered;
  std::vector<IRECT> mRowRects, mStarRects, mTypeRects, mFilterRects;
  std::string mFilter;
  IRECT mSidebarRect, mTitleRect, mCloseRect, mConnectRect, mSearchRect, mCountRect, mListRect;
  std::function<void()> mOnConnectClick;
  std::function<std::string()> mConnectStatusText;
  std::function<bool()> mIsSignedIn;
  std::function<void(const std::string&)> mOnRemoteSearch;
  std::function<std::vector<NAMRemoteItem>()> mGetRemoteResults;
  std::function<std::string()> mRemoteStatusText;
  std::function<void(const std::string&)> mOnRemoteDownloadClick;
  std::vector<NAMRemoteItem> mRemoteItemsCache;
  float mScrollOffset = 0.f;
  float mRowHeight = 34.f;
  int mHoveredIdx = -1;
  NAMLibraryFilter mActiveFilter = NAMLibraryFilter::All;
};

class NAMMeterControl : public IVPeakAvgMeterControl<>, public IBitmapBase
{
  static constexpr float KMeterMin = -70.0f;
  static constexpr float KMeterMax = -0.01f;

public:
  NAMMeterControl(const IRECT& bounds, const IBitmap& bitmap, const IVStyle& style)
  : IVPeakAvgMeterControl<>(bounds, "", style.WithShowValue(false).WithDrawFrame(false).WithWidgetFrac(0.8),
                            EDirection::Vertical, {}, 0, KMeterMin, KMeterMax, {})
  , IBitmapBase(bitmap)
  {
    SetPeakSize(1.0f);
  }

  void OnRescale() override { mBitmap = GetUI()->GetScaledBitmap(mBitmap); }

  virtual void OnResize() override
  {
    SetTargetRECT(MakeRects(mRECT));
    mWidgetBounds = mWidgetBounds.GetMidHPadded(5).GetVPadded(10);
    MakeTrackRects(mWidgetBounds);
    MakeStepRects(mWidgetBounds, mNSteps);
    SetDirty(false);
  }

  void DrawBackground(IGraphics& g, const IRECT& r) override { g.DrawFittedBitmap(mBitmap, r); }

  void DrawTrackHandle(IGraphics& g, const IRECT& r, int chIdx, bool aboveBaseValue) override
  {
    if (r.H() > 2)
      g.FillRect(GetColor(kX1), r, &mBlend);
  }

  void DrawPeak(IGraphics& g, const IRECT& r, int chIdx, bool aboveBaseValue) override
  {
    g.DrawGrid(COLOR_BLACK, mTrackBounds.Get()[chIdx], 10, 2);
    g.FillRect(GetColor(kX3), r, &mBlend);
  }
};

// Container where we can refer to children by names instead of indices
class IContainerBaseWithNamedChildren : public IContainerBase
{
public:
  IContainerBaseWithNamedChildren(const IRECT& bounds)
  : IContainerBase(bounds) {};
  ~IContainerBaseWithNamedChildren() = default;

protected:
  IControl* AddNamedChildControl(IControl* control, std::string name, int ctrlTag = kNoTag, const char* group = "")
  {
    // Make sure we haven't already used this name
    assert(mChildNameIndexMap.find(name) == mChildNameIndexMap.end());
    mChildNameIndexMap[name] = NChildren();
    return AddChildControl(control, ctrlTag, group);
  };

  IControl* GetNamedChild(std::string name)
  {
    const int index = mChildNameIndexMap[name];
    return GetChild(index);
  };


private:
  std::unordered_map<std::string, int> mChildNameIndexMap;
}; // class IContainerBaseWithNamedChildren


struct PossiblyKnownParameter
{
  bool known = false;
  double value = 0.0;
};

struct ModelInfo
{
  PossiblyKnownParameter sampleRate;
  PossiblyKnownParameter inputCalibrationLevel;
  PossiblyKnownParameter outputCalibrationLevel;
};

class ModelInfoControl : public IContainerBaseWithNamedChildren
{
public:
  ModelInfoControl(const IRECT& bounds, const IVStyle& style)
  : IContainerBaseWithNamedChildren(bounds)
  , mStyle(style) {};

  void ClearModelInfo()
  {
    static_cast<IVLabelControl*>(GetNamedChild(mControlNames.sampleRate))->SetStr("");
    mHasInfo = false;
  };

  void Hide(bool hide) override
  {
    // Don't show me unless I have info to show!
    IContainerBase::Hide(hide || (!mHasInfo));
  };

  void OnAttached() override
  {
    AddChildControl(new IVLabelControl(GetRECT().SubRectVertical(4, 0), "Model information:", mStyle));
    AddNamedChildControl(new IVLabelControl(GetRECT().SubRectVertical(4, 1), "", mStyle), mControlNames.sampleRate);
    // AddNamedChildControl(
    //   new IVLabelControl(GetRECT().SubRectVertical(4, 2), "", mStyle), mControlNames.inputCalibrationLevel);
    // AddNamedChildControl(
    //   new IVLabelControl(GetRECT().SubRectVertical(4, 3), "", mStyle), mControlNames.outputCalibrationLevel);
  };

  void SetModelInfo(const ModelInfo& modelInfo)
  {
    auto SetControlStr = [&](const std::string& name, const PossiblyKnownParameter& p, const std::string& units,
                             const std::string& childName) {
      std::stringstream ss;
      ss << name << ": ";
      if (p.known)
      {
        ss << p.value << " " << units;
      }
      else
      {
        ss << "(Unknown)";
      }
      static_cast<IVLabelControl*>(GetNamedChild(childName))->SetStr(ss.str().c_str());
    };

    SetControlStr("Sample rate", modelInfo.sampleRate, "Hz", mControlNames.sampleRate);
    // SetControlStr(
    //   "Input calibration level", modelInfo.inputCalibrationLevel, "dBu", mControlNames.inputCalibrationLevel);
    // SetControlStr(
    //   "Output calibration level", modelInfo.outputCalibrationLevel, "dBu", mControlNames.outputCalibrationLevel);

    mHasInfo = true;
  };

private:
  const IVStyle mStyle;
  struct
  {
    const std::string sampleRate = "sampleRate";
    // const std::string inputCalibrationLevel = "inputCalibrationLevel";
    // const std::string outputCalibrationLevel = "outputCalibrationLevel";
  } mControlNames;
  // Do I have info?
  bool mHasInfo = false;
};

class OutputModeControl : public IVRadioButtonControl
{
public:
  OutputModeControl(const IRECT& bounds, int paramIdx, const IVStyle& style, float buttonSize)
  : IVRadioButtonControl(
      bounds, paramIdx, {}, "Output Mode", style, EVShape::Ellipse, EDirection::Vertical, buttonSize) {};

  void SetNormalizedDisable(const bool disable)
  {
    // HACK non-DRY string and hard-coded indices
    std::stringstream ss;
    ss << "Normalized";
    if (disable)
    {
      ss << " [Not supported by model]";
    }
    mTabLabels.Get(1)->Set(ss.str().c_str());
  };
  void SetCalibratedDisable(const bool disable)
  {
    // HACK non-DRY string and hard-coded indices
    std::stringstream ss;
    ss << "Calibrated";
    if (disable)
    {
      ss << " [Not supported by model]";
    }
    mTabLabels.Get(2)->Set(ss.str().c_str());
  };
};

class NAMSettingsPageControl : public IContainerBaseWithNamedChildren
{
public:
  NAMSettingsPageControl(const IRECT& bounds, const IBitmap& bitmap, const IBitmap& inputLevelBackgroundBitmap,
                         const IBitmap& switchBitmap, ISVG closeSVG, const IVStyle& style,
                         const IVStyle& radioButtonStyle)
  : IContainerBaseWithNamedChildren(bounds)
  , mAnimationTime(0)
  , mBitmap(bitmap)
  , mInputLevelBackgroundBitmap(inputLevelBackgroundBitmap)
  , mSwitchBitmap(switchBitmap)
  , mStyle(style)
  , mRadioButtonStyle(radioButtonStyle)
  , mCloseSVG(closeSVG)
  {
    mIgnoreMouse = false;
  }

  void ClearModelInfo()
  {
    auto* modelInfoControl = static_cast<ModelInfoControl*>(GetNamedChild(mControlNames.modelInfo));
    assert(modelInfoControl != nullptr);
    modelInfoControl->ClearModelInfo();
  }

  bool OnKeyDown(float x, float y, const IKeyPress& key) override
  {
    if (key.VK == kVK_ESCAPE)
    {
      HideAnimated(true);
      return true;
    }

    return false;
  }

  void HideAnimated(bool hide)
  {
    mWillHide = hide;

    if (hide == false)
    {
      mHide = false;
    }
    else // hide subcontrols immediately
    {
      ForAllChildrenFunc([hide](int childIdx, IControl* pChild) { pChild->Hide(hide); });
    }

    SetAnimation(
      [&](IControl* pCaller) {
        auto progress = static_cast<float>(pCaller->GetAnimationProgress());

        if (mWillHide)
          SetBlend(IBlend(EBlend::Default, 1.0f - progress));
        else
          SetBlend(IBlend(EBlend::Default, progress));

        if (progress > 1.0f)
        {
          pCaller->OnEndAnimation();
          IContainerBase::Hide(mWillHide);
          GetUI()->SetAllControlsDirty();
          return;
        }
      },
      mAnimationTime);

    SetDirty(true);
  }

  void OnAttached() override
  {
    const float pad = 20.0f;
    const IVStyle titleStyle = DEFAULT_STYLE.WithValueText(IText(30, COLOR_WHITE, "Michroma-Regular"))
                                 .WithDrawFrame(false)
                                 .WithShadowOffset(2.f);
    const auto text = IText(DEFAULT_TEXT_SIZE, EAlign::Center, PluginColors::HELP_TEXT);
    const auto leftText = text.WithAlign(EAlign::Near);
    const auto style = mStyle.WithDrawFrame(false).WithValueText(text);
    const IVStyle leftStyle = style.WithValueText(leftText);

    AddNamedChildControl(new IBitmapControl(GetRECT(), mBitmap), mControlNames.bitmap)->SetIgnoreMouse(true);
    // ToneCast: everything below was laid out relative to GetRECT() (the
    // full 900x650 canvas), on the assumption that mBitmap fills it. It
    // doesn't -- IBitmapControl's Draw() calls IBitmapBase::DrawBitmap(),
    // which draws the bitmap at its NATIVE size, centered in GetRECT()
    // (see IBitmapBase::DrawBitmap in IControl.h: "GetCentredInside(IRECT
    // (0, 0, mBitmap))"), not stretched to fill it the way
    // DrawFittedBitmap does elsewhere in this codebase. Background.jpg is
    // 600x400 -- in the 900x650 canvas that's a card with ~150px of
    // margin on each side and ~125px top/bottom, not the full window.
    // Everything positioned relative to GetRECT() (the old titleArea, the
    // input/output calibration block, Output Mode, the bottom info
    // panels) was floating above/beside/past the actual visible card
    // instead of inside it. cardRect below reproduces the exact same
    // centering DrawBitmap uses, so it always matches wherever the
    // bitmap actually renders.
    const IRECT cardRect = GetRECT().GetCentredInside(IRECT(0, 0, mBitmap));
    const auto titleArea = cardRect.GetPadded(-(pad + 10.0f)).GetFromTop(50.0f);
    AddNamedChildControl(new IVLabelControl(titleArea, "SETTINGS", titleStyle), mControlNames.title);

    // Attach input/output calibration controls
    {
      const float height = NAM_KNOB_HEIGHT + NAM_SWTICH_HEIGHT + 10.0f;
      const float width = titleArea.W();
      const auto inputOutputArea = titleArea.GetFromBottom(height).GetTranslated(0.0f, height);
      const auto inputArea = inputOutputArea.GetFromLeft(0.5f * width);
      const auto outputArea = inputOutputArea.GetFromRight(0.5f * width);

      const float knobWidth = 87.0f; // HACK based on looking at the main page knobs.
      const auto inputLevelArea =
        inputArea.GetFromTop(NAM_KNOB_HEIGHT).GetFromBottom(25.0f).GetMidHPadded(0.5f * knobWidth);
      const auto inputSwitchArea = inputArea.GetFromBottom(NAM_SWTICH_HEIGHT).GetMidHPadded(0.5f * knobWidth);

      auto* inputLevelControl = AddNamedChildControl(
        new InputLevelControl(inputLevelArea, kInputCalibrationLevel, mInputLevelBackgroundBitmap, text),
        mControlNames.inputCalibrationLevel, kCtrlTagInputCalibrationLevel);
      inputLevelControl->SetTooltip(
        "The analog level, in dBu RMS, that corresponds to digital level of 0 dBFS peak in the host as its signal "
        "enters this plugin.");
      AddNamedChildControl(
        new NAMSwitchControl(inputSwitchArea, kCalibrateInput, "Calibrate Input", mStyle, mSwitchBitmap),
        mControlNames.calibrateInput, kCtrlTagCalibrateInput);

      // Same-ish height & width as input controls
      const auto outputRadioArea = outputArea.GetFromBottom(
        1.1f * (inputLevelArea.H() + inputSwitchArea.H())); // .GetMidHPadded(0.55f * knobWidth);
      const float buttonSize = 10.0f;
      auto* outputModeControl =
        AddNamedChildControl(new OutputModeControl(outputRadioArea, kOutputMode, mRadioButtonStyle, buttonSize),
                             mControlNames.outputMode, kCtrlTagOutputMode);
      outputModeControl->SetTooltip(
        "How to adjust the level of the output.\nRaw=No adjustment.\nNormalized=Adjust the level so that all models "
        "are about the same loudness.\nCalibrated=Match the input's digital-analog calibration.");
    }

    // ToneCast: was PLUG_WIDTH/2 (full-canvas half-width, 430px) and
    // GetRECT() -- see the cardRect comment above. Both overflowed past
    // the card's actual right/bottom edges into the main view behind it.
    const float halfWidth = cardRect.W() / 2.0f - pad;
    const auto bottomArea = cardRect.GetPadded(-pad).GetFromBottom(78.0f);
    const float lineHeight = 15.0f;
    const auto modelInfoArea = bottomArea.GetFromLeft(halfWidth).GetFromTop(4 * lineHeight);
    const auto aboutArea = bottomArea.GetFromRight(halfWidth).GetFromTop(5 * lineHeight);
    AddNamedChildControl(new ModelInfoControl(modelInfoArea, leftStyle), mControlNames.modelInfo);
    AddNamedChildControl(new AboutControl(aboutArea, leftStyle, leftText), mControlNames.about);

    auto closeAction = [&](IControl* pCaller) {
      static_cast<NAMSettingsPageControl*>(pCaller->GetParent())->HideAnimated(true);
    };
    AddNamedChildControl(
      new NAMSquareButtonControl(CornerButtonArea(GetRECT()), closeAction, mCloseSVG), mControlNames.close);

    OnResize();
  }

  void SetModelInfo(const ModelInfo& modelInfo)
  {
    auto* modelInfoControl = static_cast<ModelInfoControl*>(GetNamedChild(mControlNames.modelInfo));
    assert(modelInfoControl != nullptr);
    modelInfoControl->SetModelInfo(modelInfo);
  };

private:
  IBitmap mBitmap;
  IBitmap mInputLevelBackgroundBitmap;
  IBitmap mSwitchBitmap;
  IVStyle mStyle;
  IVStyle mRadioButtonStyle;
  ISVG mCloseSVG;
  int mAnimationTime = 200;
  bool mWillHide = false;

  // Names for controls
  // Make sure that these are all unique and that you use them with AddNamedChildControl
  struct ControlNames
  {
    const std::string about = "About";
    const std::string bitmap = "Bitmap";
    const std::string calibrateInput = "CalibrateInput";
    const std::string close = "Close";
    const std::string inputCalibrationLevel = "InputCalibrationLevel";
    const std::string modelInfo = "ModelInfo";
    const std::string outputMode = "OutputMode";
    const std::string title = "Title";
  } mControlNames;

  class InputLevelControl : public IEditableTextControl
  {
  public:
    InputLevelControl(const IRECT& bounds, int paramIdx, const IBitmap& bitmap, const IText& text = DEFAULT_TEXT,
                      const IColor& BGColor = DEFAULT_BGCOLOR)
    : IEditableTextControl(bounds, "", text, BGColor)
    , mBitmap(bitmap)
    {
      SetParamIdx(paramIdx);
    };

    void Draw(IGraphics& g) override
    {
      g.DrawFittedBitmap(mBitmap, mRECT);
      ITextControl::Draw(g);
    };

    void SetValueFromUserInput(double normalizedValue, int valIdx) override
    {
      IControl::SetValueFromUserInput(normalizedValue, valIdx);
      const std::string s = ConvertToString(normalizedValue);
      OnTextEntryCompletion(s.c_str(), valIdx);
    };

    void SetValueFromDelegate(double normalizedValue, int valIdx) override
    {
      IControl::SetValueFromDelegate(normalizedValue, valIdx);
      const std::string s = ConvertToString(normalizedValue);
      SetStr(s.c_str());
      SetDirty(false);
    };

  private:
    std::string ConvertToString(const double normalizedValue)
    {
      const double naturalValue = GetParam()->FromNormalized(normalizedValue);
      // And make the value to display
      std::stringstream ss;
      ss << naturalValue << " dBu";
      std::string s = ss.str();
      return s;
    };

    IBitmap mBitmap;
  };

  class AboutControl : public IContainerBase
  {
  public:
    AboutControl(const IRECT& bounds, const IVStyle& style, const IText& text)
    : IContainerBase(bounds)
    , mStyle(style)
    , mText(text) {};

    void OnAttached() override
    {
      WDL_String verStr, buildInfoStr;
      PLUG()->GetPluginVersionStr(verStr);

      buildInfoStr.SetFormatted(100, "Version %s %s %s", verStr.Get(), PLUG()->GetArchStr(), PLUG()->GetAPIStr());

      AddChildControl(new IURLControl(GetRECT().SubRectVertical(5, 0), "NEURAL AMP MODELER",
                                      "https://www.neuralampmodeler.com", mText, COLOR_TRANSPARENT,
                                      PluginColors::HELP_TEXT_MO, PluginColors::HELP_TEXT_CLICKED));
      AddChildControl(new IVLabelControl(GetRECT().SubRectVertical(5, 1), "By Steven Atkinson", mStyle));
      AddChildControl(new IVLabelControl(GetRECT().SubRectVertical(5, 2), buildInfoStr.Get(), mStyle));
      AddChildControl(new IURLControl(GetRECT().SubRectVertical(5, 3),
                                      "Plug-in development: Steve Atkinson, Oli Larkin, ... ",
                                      "https://github.com/sdatkinson/NeuralAmpModelerPlugin/graphs/contributors", mText,
                                      COLOR_TRANSPARENT, PluginColors::HELP_TEXT_MO, PluginColors::HELP_TEXT_CLICKED));
      AddChildControl(new ThirdPartyNoticesControl(GetRECT().SubRectVertical(5, 4), mText));
    };

  private:
    class ThirdPartyNoticesControl : public IURLControl
    {
    public:
      ThirdPartyNoticesControl(const IRECT& bounds, const IText& text)
      : IURLControl(bounds, "Third party notices", "", text, COLOR_TRANSPARENT, PluginColors::HELP_TEXT_MO,
                    PluginColors::HELP_TEXT_CLICKED)
      {
      }

      void OnMouseDown(float x, float y, const IMouseMod& mod) override
      {
        WDL_String path;
        bool opened = false;

        if (ResolveNoticesPath(GetUI(), path))
          opened = OpenNoticesPath(GetUI(), path);

        if (!opened)
          ShowOpenError(GetUI());

        GetUI()->ReleaseMouseCapture();
        mClicked = true;
        SetDirty(false);
      }

    private:
      static bool FileExists(const WDL_String& path)
      {
        if (!CStringHasContents(path.Get()))
          return false;

        FILE* file = WDL_fopenA(path.Get(), "rb");
        if (file == nullptr)
          return false;

        fclose(file);
        return true;
      }

      static bool TryNoticePathInDirectory(WDL_String& result, const WDL_String& directory)
      {
        if (!CStringHasContents(directory.Get()))
          return false;

        WDL_String candidate(directory);
        const char lastChar = candidate.Get()[candidate.GetLength() - 1];

        if (!WDL_IS_DIRCHAR(lastChar))
          candidate.Append(WDL_DIRCHAR_STR);

        candidate.Append(kNoticesFileName);

        if (!FileExists(candidate))
          return false;

        result.Set(candidate.Get());
        return true;
      }

      // AAX (and similar) load the binary from Contents\x64 or Contents\Win32 while notices live in
      // Contents\Resources. Same layout as a VST3 bundle; this path is not covered by BundleResourcePath
      // when the plug-in is built as AAX_API only (no VST3_API).
      static bool TryNoticePathSiblingResources(WDL_String& result, const WDL_String& moduleDirectory)
      {
        if (!CStringHasContents(moduleDirectory.Get()))
          return false;

        WDL_String candidate(moduleDirectory);
        const char lastChar = candidate.Get()[candidate.GetLength() - 1];
        if (!WDL_IS_DIRCHAR(lastChar))
          candidate.Append(WDL_DIRCHAR_STR);

        candidate.Append("..");
        candidate.Append(WDL_DIRCHAR_STR);
        candidate.Append("Resources");
        candidate.Append(WDL_DIRCHAR_STR);
        candidate.Append(kNoticesFileName);

        if (!FileExists(candidate))
          return false;

        result.Set(candidate.Get());
        return true;
      }

      static bool ResolveNoticesPath(IGraphics* pGraphics, WDL_String& path)
      {
        path.Set("");

        if (pGraphics == nullptr)
          return false;

#ifdef OS_WIN
        WDL_String directory;
        const auto moduleHandle = static_cast<PluginIDType>(pGraphics->GetWinModuleHandle());

        BundleResourcePath(directory, moduleHandle);
        if (TryNoticePathInDirectory(path, directory))
          return true;

        directory.Set("");
        PluginPath(directory, moduleHandle);
        if (TryNoticePathInDirectory(path, directory))
          return true;

        if (TryNoticePathSiblingResources(path, directory))
          return true;
#endif

        const auto resourceLocation =
          LocateResource(kNoticesFileName, "txt", path, pGraphics->GetBundleID(), pGraphics->GetWinModuleHandle(),
                         pGraphics->GetSharedResourcesSubPath());

        return resourceLocation == EResourceLocation::kAbsolutePath && FileExists(path);
      }

      static bool OpenNoticesPath(IGraphics* pGraphics, const WDL_String& path)
      {
        if (pGraphics == nullptr || !CStringHasContents(path.Get()))
          return false;

#ifdef OS_WIN
        WCHAR pathWide[IPLUG_WIN_MAX_WIDE_PATH];
        UTF8ToUTF16(pathWide, path.Get(), IPLUG_WIN_MAX_WIDE_PATH);

        if (pathWide[0] == 0)
          return false;

        WCHAR canon[IPLUG_WIN_MAX_WIDE_PATH];
        const DWORD nCanon = GetFullPathNameW(pathWide, IPLUG_WIN_MAX_WIDE_PATH, canon, nullptr);
        const WCHAR* const launchPath = (nCanon > 0 && nCanon < IPLUG_WIN_MAX_WIDE_PATH) ? canon : pathWide;

        return ShellExecuteW(nullptr, L"open", launchPath, nullptr, nullptr, SW_SHOWNORMAL) > HINSTANCE(32);
#else
        return pGraphics->OpenURL(path.Get());
#endif
      }

      static void ShowOpenError(IGraphics* pGraphics)
      {
        if (pGraphics == nullptr)
          return;

        const char* const title = "Third party notices";
        const char* const message = "Could not open ThirdPartyNotices.txt.";

#ifdef OS_MAC
        pGraphics->ShowMessageBox(title, message, kMB_OK);
#else
        pGraphics->ShowMessageBox(message, title, kMB_OK);
#endif
      }

      static constexpr const char* kNoticesFileName = "ThirdPartyNotices.txt";
    };

    IVStyle mStyle;
    IText mText;
  };
};
