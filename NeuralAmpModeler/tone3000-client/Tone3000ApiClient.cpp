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
    result.id = json.value("id", 0);
    result.username = json.value("username", "");
    result.ok = true;
  }
  catch (const std::exception& e)
  {
    result.error = std::string("Failed to parse /user response: ") + e.what();
  }
  return result;
}

SearchResult SearchTones(const std::string& accessToken, const std::string& query, const std::string& format,
                         int page)
{
  SearchResult result;

  std::ostringstream url;
  url << kApiBaseUrl << "/tones/search?page=" << page << "&page_size=20&sort=best-match";
  if (!query.empty())
    url << "&query=" << UrlEncode(query);
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
        tone.id = item.value("id", 0);
        tone.title = item.value("title", "");
        tone.gear = item.value("gear", "");
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

ModelListResult ListModelsForTone(const std::string& accessToken, int toneId)
{
  ModelListResult result;

  std::ostringstream url;
  url << kApiBaseUrl << "/models?tone_id=" << toneId << "&page_size=100";

  const HttpResponse response = HttpGet(url.str(), BearerHeader(accessToken));
  if (!response.success)
  {
    result.error = response.error;
    return result;
  }
  if (response.statusCode != 200)
  {
    result.error = "GET /models returned HTTP " + std::to_string(response.statusCode);
    return result;
  }

  try
  {
    const auto json = nlohmann::json::parse(response.body);
    if (json.contains("data") && json["data"].is_array())
    {
      for (const auto& item : json["data"])
      {
        ModelInfo model;
        model.id = item.value("id", 0);
        model.name = item.value("name", "");
        model.size = item.value("size", "");
        model.modelUrl = item.value("model_url", "");
        result.models.push_back(std::move(model));
      }
    }
    result.ok = true;
  }
  catch (const std::exception& e)
  {
    result.error = std::string("Failed to parse /models response: ") + e.what();
  }
  return result;
}

} // namespace tone3000
