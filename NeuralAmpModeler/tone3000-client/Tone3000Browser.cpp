#include "Tone3000Browser.h"
#include "Tone3000ApiClient.h"
#include "Tone3000Http.h"

#include <algorithm>
#include <fstream>
#include <thread>

#include <shlobj.h>
#include <windows.h>
#pragma comment(lib, "shell32.lib")

namespace
{

bool IsDownloadableFormat(const std::string& format) { return format == "nam" || format == "ir"; }

std::wstring WidenAscii(const std::string& s) { return std::wstring(s.begin(), s.end()); }

std::string SanitizeForFilename(const std::string& s)
{
  std::string out;
  out.reserve(s.size());
  for (char c : s)
  {
    // Windows-reserved filename characters, plus control chars.
    if (std::string("<>:\"/\\|?*").find(c) != std::string::npos || static_cast<unsigned char>(c) < 0x20)
      out.push_back('_');
    else
      out.push_back(c);
  }
  if (out.empty())
    out = "tone3000-download";
  return out;
}

// %LOCALAPPDATA%\ToneCast\tone3000-cache\ -- created on first use.
// Downloaded files live here indefinitely (no eviction yet; that's the
// deferred file-cache/ module mentioned in docs/decisions-log.md).
std::wstring GetCacheDir()
{
  wchar_t* localAppData = nullptr;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData)) || !localAppData)
    return L"";
  std::wstring dir(localAppData);
  CoTaskMemFree(localAppData);
  dir += L"\\ToneCast\\tone3000-cache\\";

  // Create each path segment; ignore ERROR_ALREADY_EXISTS.
  std::wstring partial;
  for (size_t i = 0; i < dir.size(); i++)
  {
    partial += dir[i];
    if (dir[i] == L'\\')
      CreateDirectoryW(partial.c_str(), nullptr);
  }
  return dir;
}

// Writes to a temp file then renames into place, so a crash or a second
// concurrent write never leaves a half-written model/IR file where the
// plugin might try to load it (per docs/dsp-architecture.md's cache
// write rules).
bool WriteFileAtomic(const std::wstring& finalPath, const std::string& bytes)
{
  const std::wstring tmpPath = finalPath + L".tmp";
  {
    std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
    if (!out)
      return false;
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!out)
      return false;
  }
  return MoveFileExW(tmpPath.c_str(), finalPath.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

} // namespace

namespace tone3000
{

void Browser::Configure(std::function<std::string()> getAccessToken,
                        std::function<void(const std::string&, const std::string&)> onInstall)
{
  mGetAccessToken = std::move(getAccessToken);
  mOnInstall = std::move(onInstall);
}

void Browser::Search(const std::string& query)
{
  const uint64_t requestId = ++mSearchRequestId;
  mSearchState.store(SearchState::Searching);
  std::thread(&Browser::RunSearchThread, this, query, requestId).detach();
}

std::string Browser::GetSearchError() const
{
  std::lock_guard<std::mutex> lock(mMutex);
  return mSearchError;
}

std::vector<ResultItem> Browser::GetResults() const
{
  std::lock_guard<std::mutex> lock(mMutex);
  return mResults;
}

void Browser::RunSearchThread(std::string query, uint64_t requestId)
{
  const std::string accessToken = mGetAccessToken ? mGetAccessToken() : "";
  const SearchResult response = SearchTones(accessToken, query, "", 1);

  // A newer search started while this one was in flight -- drop this
  // result rather than let a slow stale response clobber fresher ones.
  if (requestId != mSearchRequestId.load())
    return;

  std::lock_guard<std::mutex> lock(mMutex);
  if (!response.ok)
  {
    mSearchError = response.error;
    mSearchState.store(SearchState::Error);
    return;
  }

  mResults.clear();
  for (const auto& tone : response.tones)
  {
    ResultItem item;
    item.toneId = tone.id;
    item.title = tone.title;
    item.gear = tone.gear;
    item.format = tone.format;
    item.downloadable = IsDownloadableFormat(tone.format);
    mResults.push_back(std::move(item));
  }
  mSearchError.clear();
  mSearchState.store(SearchState::Ready);
}

void Browser::Download(int toneId)
{
  std::string title, format;
  {
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = std::find_if(mResults.begin(), mResults.end(), [&](const ResultItem& r) { return r.toneId == toneId; });
    if (it == mResults.end() || !it->downloadable || it->downloadState == ResultItem::DownloadState::Downloading
        || it->downloadState == ResultItem::DownloadState::Downloaded)
      return;
    it->downloadState = ResultItem::DownloadState::Downloading;
    title = it->title;
    format = it->format;
  }
  std::thread(&Browser::RunDownloadThread, this, toneId, title, format).detach();
}

void Browser::SetDownloadState(int toneId, ResultItem::DownloadState state)
{
  std::lock_guard<std::mutex> lock(mMutex);
  auto it = std::find_if(mResults.begin(), mResults.end(), [&](const ResultItem& r) { return r.toneId == toneId; });
  if (it != mResults.end())
    it->downloadState = state;
}

void Browser::RunDownloadThread(int toneId, std::string title, std::string format)
{
  const std::string accessToken = mGetAccessToken ? mGetAccessToken() : "";

  const ModelListResult models = ListModelsForTone(accessToken, toneId);
  if (!models.ok || models.models.empty())
  {
    SetDownloadState(toneId, ResultItem::DownloadState::Failed);
    return;
  }

  // A tone can have several model files (different capture sizes); take
  // the first for now -- picking a specific size is future work once
  // there's a UI for it.
  const ModelInfo& model = models.models.front();
  if (model.modelUrl.empty())
  {
    SetDownloadState(toneId, ResultItem::DownloadState::Failed);
    return;
  }

  const HttpResponse fileResponse = HttpGet(model.modelUrl, {{"Authorization", "Bearer " + accessToken}});
  if (!fileResponse.success || fileResponse.statusCode != 200)
  {
    SetDownloadState(toneId, ResultItem::DownloadState::Failed);
    return;
  }

  const std::wstring cacheDir = GetCacheDir();
  if (cacheDir.empty())
  {
    SetDownloadState(toneId, ResultItem::DownloadState::Failed);
    return;
  }

  const char* extension = (format == "ir") ? ".wav" : ".nam";
  const std::wstring fileName =
    WidenAscii(SanitizeForFilename(title)) + L"_" + WidenAscii(std::to_string(model.id)) + WidenAscii(extension);
  const std::wstring finalPath = cacheDir + fileName;

  if (!WriteFileAtomic(finalPath, fileResponse.body))
  {
    SetDownloadState(toneId, ResultItem::DownloadState::Failed);
    return;
  }

  SetDownloadState(toneId, ResultItem::DownloadState::Downloaded);

  if (mOnInstall)
  {
    // Narrow the wide path back for the callback. WidenAscii/this
    // truncation are naive byte-for-byte, not real UTF-16 conversion --
    // round-trips fine for ASCII (the common case) but non-ASCII tone
    // titles or a non-ASCII Windows profile path could mangle. Acceptable
    // for this first pass; see docs/decisions-log.md.
    std::string narrowPath(finalPath.begin(), finalPath.end());
    mOnInstall(narrowPath, format);
  }
}

} // namespace tone3000
