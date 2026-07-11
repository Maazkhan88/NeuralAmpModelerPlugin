#pragma once

#include <string>
#include <vector>

// Minimal HTTPS client for the TONE3000 API, built on WinHTTP (native to
// Windows -- no new third-party dependency). Standalone-only; see
// Tone3000Config.h. Deliberately small: this project only needs a handful
// of GET/POST calls, not a general-purpose HTTP library.

namespace tone3000
{

struct HttpResponse
{
  bool success = false;
  int statusCode = 0;
  std::string body;
  std::string error; // set when success == false and it's a transport-level failure
};

struct HttpHeader
{
  std::string name;
  std::string value;
};

// GET https://host/path with optional extra headers (e.g. Authorization).
HttpResponse HttpGet(const std::string& url, const std::vector<HttpHeader>& headers = {});

// POST with an application/x-www-form-urlencoded body (what the OAuth
// token endpoint expects) or a raw body with an explicit content type.
HttpResponse HttpPostForm(const std::string& url, const std::string& formBody,
                          const std::vector<HttpHeader>& headers = {});

// application/x-www-form-urlencoded percent-encoding of a single value.
std::string UrlEncode(const std::string& value);

} // namespace tone3000
