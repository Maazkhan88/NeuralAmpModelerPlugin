#pragma once

#include <string>
#include <vector>

// Thin wrapper over the TONE3000 REST endpoints actually needed so far.
// Standalone-only -- see Tone3000Config.h. Field names/shapes here match
// https://www.tone3000.com/api (fetched directly, not guessed).

namespace tone3000
{

struct UserInfo
{
  bool ok = false;
  int id = 0;
  std::string username;
  std::string error;
};

struct ToneSummary
{
  int id = 0;
  std::string title;
  std::string gear; // e.g. "amp", "amp-cab", "pedal"
  std::string format; // "nam", "ir", "aida-x", "aa-snapshot", "proteus"
  std::string license;
};

struct SearchResult
{
  bool ok = false;
  std::vector<ToneSummary> tones;
  int page = 1;
  int totalPages = 1;
  std::string error;
};

struct ModelInfo
{
  int id = 0;
  std::string name;
  std::string size;
  std::string modelUrl; // download with Authorization: Bearer <accessToken>
};

struct ModelListResult
{
  bool ok = false;
  std::vector<ModelInfo> models;
  std::string error;
};

// GET /user -- used as a lightweight "is this access token actually
// valid" check right after signing in.
UserInfo GetCurrentUser(const std::string& accessToken);

// GET /tones/search. `format` may be empty to mean "any" (nam/ir/etc, see
// TONE3000's Format enum); results are still filtered client-side to
// nam/ir since those are the only formats this plugin can load.
SearchResult SearchTones(const std::string& accessToken, const std::string& query, const std::string& format,
                         int page);

// GET /models?tone_id=<id> -- needed before downloading, since a tone can
// have multiple model files (e.g. different capture sizes).
ModelListResult ListModelsForTone(const std::string& accessToken, int toneId);

} // namespace tone3000
