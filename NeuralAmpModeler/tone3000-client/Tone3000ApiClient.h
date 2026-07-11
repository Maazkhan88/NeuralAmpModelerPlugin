#pragma once

#include <string>
#include <vector>

// Thin wrapper over the TONE3000 REST endpoints actually needed so far.
// Standalone-only -- see Tone3000Config.h. Extend as the library-browsing
// UI grows; deliberately minimal for now (this is the first pass, not a
// full client).

namespace tone3000
{

struct UserInfo
{
  bool ok = false;
  std::string username;
  std::string error;
};

struct ToneSummary
{
  std::string id;
  std::string name;
  std::string gearType;
  std::string format;
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

// GET /user -- used as a lightweight "is this access token actually
// valid" check right after signing in.
UserInfo GetCurrentUser(const std::string& accessToken);

// GET /tones/search -- query is passed straight through as free text;
// gearType/format may be empty to mean "any".
SearchResult SearchTones(const std::string& accessToken, const std::string& query, const std::string& gearType,
                         const std::string& format, int page);

} // namespace tone3000
