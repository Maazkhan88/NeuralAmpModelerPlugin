#include "Tone3000ApiClient.h"
#include "Tone3000Config.h"
#include "Tone3000Http.h"

#include <sstream>

#include "json.hpp"

namespace
{
std::vector<tone3000::HttpHeader> BearerHeader(const std::string& accessToken)
{
  return {{"Authorization", "Bearer " + accessToken}};
}
} // namespace

namespace tone3000
{

UserInfo GetCurrentUser(const std::string& accessToken)
{
  UserInfo result;
  const HttpResponse response = HttpGet(std::string(kApiBaseUrl) + "/user", BearerHeader(accessToken));
  if (!response.success)
  {
    result.error = response.error;
    return result;
  }
  if (response.statusCode != 200)
  {
    result.error = "GET /user returned HTTP " + std::to_string(response.statusCode);
    return result;
  }
  try
  {
    const auto json = nlohmann::json::parse(response.body);
    result.username = json.value("username", json.value("name", ""));
    result.ok = true;
  }
  catch (const std::exception& e)
  {
    result.error = std::string("Failed to parse /user response: ") + e.what();
  }
  return result;
}

SearchResult SearchTones(const std::string& accessToken, const std::string& query, const std::string& gearType,
                         const std::string& format, int page)
{
  SearchResult result;

  std::ostringstream url;
  url << kApiBaseUrl << "/tones/search?page=" << page;
  if (!query.empty())
    url << "&q=" << UrlEncode(query);
  if (!gearType.empty())
    url << "&gear_type=" << UrlEncode(gearType);
  if (!format.empty())
    url << "&format=" << UrlEncode(format);

  const HttpResponse response = HttpGet(url.str(), BearerHeader(accessToken));
  if (!response.success)
  {
    result.error = response.error;
    return result;
  }
  if (response.statusCode != 200)
  {
    result.error = "GET /tones/search returned HTTP " + std::to_string(response.statusCode);
    return result;
  }

  try
  {
    const auto json = nlohmann::json::parse(response.body);
    result.page = json.value("page", 1);
    result.totalPages = json.value("total_pages", 1);
    if (json.contains("data") && json["data"].is_array())
    {
      for (const auto& item : json["data"])
      {
        ToneSummary tone;
        tone.id = item.value("id", "");
        tone.name = item.value("name", "");
        tone.gearType = item.value("gear_type", "");
        tone.format = item.value("format", "");
        tone.license = item.value("license", "");
        result.tones.push_back(std::move(tone));
      }
    }
    result.ok = true;
  }
  catch (const std::exception& e)
  {
    result.error = std::string("Failed to parse /tones/search response: ") + e.what();
  }
  return result;
}

} // namespace tone3000
