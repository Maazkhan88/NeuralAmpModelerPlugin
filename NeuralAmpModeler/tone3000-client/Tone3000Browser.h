#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

// ToneCast: orchestrates TONE3000 search + download for the library
// panel's "TONE3000" tab. Standalone-only, like the rest of
// tone3000-client/ (see Tone3000Config.h).
//
// All network work runs on background threads (search and each download
// get their own detached std::thread, mirroring OAuthFlow's pattern).
// Results are read via thread-safe snapshots (GetResults()) rather than
// callbacks marshaled onto the UI thread -- the plugin's IGraphics
// instance already redraws continuously (see IGraphicsWin's WM_TIMER-
// driven OnDisplayTimer), so polling a snapshot once per Draw() is
// sufficient and avoids needing a cross-thread event queue.
//
// Actually installing a downloaded file (calling into DSP staging code)
// is NOT done here -- that must happen on the UI thread, since
// NeuralAmpModeler::_StageModel/_StageIR touch unsynchronized state also
// read by the audio thread. This class only writes the file to disk and
// hands the resulting path to the onInstall callback; the caller
// (NeuralAmpModeler.cpp) is responsible for marshaling that onto the UI
// thread via OnIdle(), the same way mNewModelLoadedInDSP already works.

namespace tone3000
{

enum class SearchState
{
  Idle,
  Searching,
  Ready,
  Error
};

struct ResultItem
{
  enum class DownloadState
  {
    NotDownloaded,
    Downloading,
    Downloaded,
    Failed
  };

  int toneId = 0;
  std::string title;
  std::string gear;
  std::string format;
  bool downloadable = false; // true only for format == "nam" or "ir"
  DownloadState downloadState = DownloadState::NotDownloaded;
};

class Browser
{
public:
  // Must be called once before Search()/Download() are used. Split from
  // the constructor so NeuralAmpModeler can default-construct this as a
  // plain member and wire up `this`-capturing callbacks afterward, same
  // pattern as NAMLibraryPanelControl::SetConnectHandler.
  void Configure(std::function<std::string()> getAccessToken,
                 std::function<void(const std::string& localPath, const std::string& format)> onInstall);

  void Search(const std::string& query);
  SearchState GetSearchState() const { return mSearchState.load(); }
  std::string GetSearchError() const;

  // Starts a background download+install for the given tone if it isn't
  // already downloading/downloaded. No-op if toneId isn't in the current
  // result set or isn't downloadable.
  void Download(int toneId);

  std::vector<ResultItem> GetResults() const;

private:
  void RunSearchThread(std::string query, uint64_t requestId);
  void RunDownloadThread(int toneId, std::string title, std::string format);
  void SetDownloadState(int toneId, ResultItem::DownloadState state);

  std::function<std::string()> mGetAccessToken;
  std::function<void(const std::string&, const std::string&)> mOnInstall;

  mutable std::mutex mMutex;
  std::vector<ResultItem> mResults;
  std::string mSearchError;

  std::atomic<SearchState> mSearchState{SearchState::Idle};
  std::atomic<uint64_t> mSearchRequestId{0};
};

} // namespace tone3000
