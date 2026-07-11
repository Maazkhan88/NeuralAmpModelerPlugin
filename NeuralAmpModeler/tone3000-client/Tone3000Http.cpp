#include "Tone3000Http.h"

#include <sstream>
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

namespace
{

std::wstring WidenAscii(const std::string& s) { return std::wstring(s.begin(), s.end()); }

std::wstring JoinHeaders(const std::vector<tone3000::HttpHeader>& headers)
{
  std::wstring out;
  for (const auto& h : headers)
  {
    out += WidenAscii(h.name) + L": " + WidenAscii(h.value) + L"\r\n";
  }
  return out;
}

// Shared send/receive logic for both GET and the form-POST helper below.
tone3000::HttpResponse DoRequest(const std::string& url, const wchar_t* verb, const std::string& body,
                                 const std::vector<tone3000::HttpHeader>& headers)
{
  tone3000::HttpResponse result;

  const std::wstring wideUrl = WidenAscii(url);

  wchar_t hostName[256] = {};
  wchar_t urlPath[4096] = {};
  wchar_t extraInfo[4096] = {};
  URL_COMPONENTS urlComp = {};
  urlComp.dwStructSize = sizeof(urlComp);
  urlComp.lpszHostName = hostName;
  urlComp.dwHostNameLength = _countof(hostName);
  urlComp.lpszUrlPath = urlPath;
  urlComp.dwUrlPathLength = _countof(urlPath);
  urlComp.lpszExtraInfo = extraInfo;
  urlComp.dwExtraInfoLength = _countof(extraInfo);

  if (!WinHttpCrackUrl(wideUrl.c_str(), static_cast<DWORD>(wideUrl.size()), 0, &urlComp))
  {
    result.error = "WinHttpCrackUrl failed to parse URL";
    return result;
  }
  const std::wstring fullPath = std::wstring(urlPath) + std::wstring(extraInfo);

  HINTERNET hSession = WinHttpOpen(L"ToneCast/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                   WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession)
  {
    result.error = "WinHttpOpen failed";
    return result;
  }

  HINTERNET hConnect = WinHttpConnect(hSession, urlComp.lpszHostName, urlComp.nPort, 0);
  if (!hConnect)
  {
    result.error = "WinHttpConnect failed";
    WinHttpCloseHandle(hSession);
    return result;
  }

  const DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
  HINTERNET hRequest = WinHttpOpenRequest(hConnect, verb, fullPath.c_str(), nullptr, WINHTTP_NO_REFERER,
                                          WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
  if (!hRequest)
  {
    result.error = "WinHttpOpenRequest failed";
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
  }

  const std::wstring headerBlock = JoinHeaders(headers);
  LPVOID bodyPtr = body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data());
  const DWORD bodyLen = static_cast<DWORD>(body.size());

  BOOL sent =
    WinHttpSendRequest(hRequest, headerBlock.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headerBlock.c_str(),
                       static_cast<DWORD>(headerBlock.size()), bodyPtr, bodyLen, bodyLen, 0);
  if (sent)
    sent = WinHttpReceiveResponse(hRequest, nullptr);

  if (sent)
  {
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_FLAG_NUMBER | WINHTTP_QUERY_STATUS_CODE, WINHTTP_HEADER_NAME_BY_INDEX,
                        &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
    result.statusCode = static_cast<int>(statusCode);

    std::string bodyOut;
    for (;;)
    {
      DWORD available = 0;
      if (!WinHttpQueryDataAvailable(hRequest, &available) || available == 0)
        break;
      std::vector<char> chunk(available);
      DWORD read = 0;
      if (!WinHttpReadData(hRequest, chunk.data(), available, &read) || read == 0)
        break;
      bodyOut.append(chunk.data(), read);
    }
    result.body = bodyOut;
    result.success = true;
  }
  else
  {
    result.error = "WinHttpSendRequest/WinHttpReceiveResponse failed (GetLastError=" + std::to_string(GetLastError()) +
      ")";
  }

  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  WinHttpCloseHandle(hSession);
  return result;
}

} // namespace

namespace tone3000
{

HttpResponse HttpGet(const std::string& url, const std::vector<HttpHeader>& headers)
{
  return DoRequest(url, L"GET", "", headers);
}

HttpResponse HttpPostForm(const std::string& url, const std::string& formBody, const std::vector<HttpHeader>& headers)
{
  std::vector<HttpHeader> allHeaders = headers;
  allHeaders.push_back({"Content-Type", "application/x-www-form-urlencoded"});
  return DoRequest(url, L"POST", formBody, allHeaders);
}

std::string UrlEncode(const std::string& value)
{
  static const char* kHex = "0123456789ABCDEF";
  std::string out;
  out.reserve(value.size());
  for (unsigned char c : value)
  {
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
      out.push_back(static_cast<char>(c));
    else
    {
      out.push_back('%');
      out.push_back(kHex[(c >> 4) & 0xF]);
      out.push_back(kHex[c & 0xF]);
    }
  }
  return out;
}

} // namespace tone3000
