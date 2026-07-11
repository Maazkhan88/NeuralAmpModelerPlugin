#include "Tone3000OAuth.h"
#include "Tone3000Config.h"
#include "Tone3000Http.h"
#include "Tone3000Pkce.h"
#include "Tone3000TokenStore.h"

#include <sstream>
#include <thread>

// windows.h pulls in the old Winsock 1.1 header unless WIN32_LEAN_AND_MEAN
// is defined first, which then conflicts with winsock2.h's redefinitions
// (sockaddr, etc.) -- define it and include winsock2.h before windows.h.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>
#pragma comment(lib, "ws2_32.lib")

#include "json.hpp" // nlohmann::json -- already on this project's include path (see common-win.props JSON_PATH)

namespace
{

// Runs a tiny, single-request local HTTP listener on 127.0.0.1:port,
// waiting up to timeoutSeconds for the OAuth redirect. Returns the query
// string of the first request received (e.g. "code=...&state=..."), or
// empty on timeout/error. This exists only to catch the browser's
// redirect after the user approves the TONE3000 login -- it never
// listens beyond one request or beyond the timeout.
std::string WaitForRedirectQuery(int port, int timeoutSeconds)
{
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    return "";

  SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listenSocket == INVALID_SOCKET)
  {
    WSACleanup();
    return "";
  }

  sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1 only -- never bind 0.0.0.0
  addr.sin_port = htons(static_cast<u_short>(port));

  std::string result;
  if (bind(listenSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0 && listen(listenSocket, 1) == 0)
  {
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(listenSocket, &readSet);
    timeval tv = {};
    tv.tv_sec = timeoutSeconds;

    if (select(0, &readSet, nullptr, nullptr, &tv) > 0)
    {
      SOCKET client = accept(listenSocket, nullptr, nullptr);
      if (client != INVALID_SOCKET)
      {
        char buffer[8192] = {};
        const int received = recv(client, buffer, sizeof(buffer) - 1, 0);
        if (received > 0)
        {
          // First line looks like: "GET /tonecast-oauth-callback?code=...&state=... HTTP/1.1"
          const std::string request(buffer, received);
          const size_t pathStart = request.find(' ');
          const size_t pathEnd = request.find(' ', pathStart == std::string::npos ? 0 : pathStart + 1);
          if (pathStart != std::string::npos && pathEnd != std::string::npos)
          {
            const std::string requestLine = request.substr(pathStart + 1, pathEnd - pathStart - 1);
            const size_t queryStart = requestLine.find('?');
            if (queryStart != std::string::npos)
              result = requestLine.substr(queryStart + 1);
          }
        }

        static const char* kResponseBody =
          "<html><body style=\"font-family:sans-serif;background:#171310;color:#f0e8de;"
          "text-align:center;padding-top:4em;\">"
          "<h2>ToneCast</h2><p>You can close this window and return to the app.</p></body></html>";
        std::ostringstream response;
        response << "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\nContent-Length: "
                 << strlen(kResponseBody) << "\r\n\r\n"
                 << kResponseBody;
        const std::string responseStr = response.str();
        send(client, responseStr.c_str(), static_cast<int>(responseStr.size()), 0);
        closesocket(client);
      }
    }
  }

  closesocket(listenSocket);
  WSACleanup();
  return result;
}

std::string ParseQueryParam(const std::string& query, const std::string& key)
{
  const std::string needle = key + "=";
  size_t pos = query.find(needle);
  if (pos == std::string::npos)
    return "";
  pos += needle.size();
  const size_t end = query.find('&', pos);
  return query.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
}

std::string RedirectUri()
{
  char buf[256];
  snprintf(buf, sizeof(buf), tone3000::kRedirectUriTemplate, tone3000::kRedirectPort);
  return buf;
}

} // namespace

namespace tone3000
{

void OAuthFlow::BeginLogin()
{
  bool expected = false;
  if (!mFlowInProgress.compare_exchange_strong(expected, true))
    return; // already running

  mStatus.store(AuthStatus::Connecting);
  std::thread(&OAuthFlow::RunLoginThread, this).detach();
}

void OAuthFlow::TryResume()
{
  auto stored = LoadRefreshToken();
  if (!stored.has_value())
    return;

  bool expected = false;
  if (!mFlowInProgress.compare_exchange_strong(expected, true))
    return;

  mStatus.store(AuthStatus::ExchangingToken);
  std::thread(&OAuthFlow::RunResumeThread, this, *stored).detach();
}

void OAuthFlow::SignOut()
{
  ClearRefreshToken();
  {
    std::lock_guard<std::mutex> lock(mMutex);
    mAccessToken.clear();
  }
  mStatus.store(AuthStatus::SignedOut);
}

std::string OAuthFlow::GetLastError() const
{
  std::lock_guard<std::mutex> lock(mMutex);
  return mLastError;
}

std::string OAuthFlow::GetAccessToken() const
{
  std::lock_guard<std::mutex> lock(mMutex);
  return mAccessToken;
}

void OAuthFlow::RunLoginThread()
{
  const std::string codeVerifier = GenerateCodeVerifier();
  const std::string codeChallenge = DeriveCodeChallenge(codeVerifier);
  const std::string state = GenerateState();
  const std::string redirectUri = RedirectUri();

  std::ostringstream authorizeUrl;
  authorizeUrl << kAuthorizeEndpoint << "?response_type=code" << "&client_id=" << UrlEncode(kClientId)
               << "&redirect_uri=" << UrlEncode(redirectUri) << "&code_challenge=" << UrlEncode(codeChallenge)
               << "&code_challenge_method=S256" << "&state=" << UrlEncode(state);

  // System-browser OAuth per docs/decisions-log.md -- never an embedded
  // webview, so the user enters credentials only on TONE3000's own page.
  ShellExecuteA(nullptr, "open", authorizeUrl.str().c_str(), nullptr, nullptr, SW_SHOWNORMAL);

  const std::string query = WaitForRedirectQuery(kRedirectPort, 180);
  std::string error;
  bool ok = false;

  if (query.empty())
  {
    error = "Timed out waiting for the TONE3000 login to complete.";
  }
  else if (!ParseQueryParam(query, "error").empty())
  {
    error = "TONE3000 returned an error: " + ParseQueryParam(query, "error");
  }
  else
  {
    const std::string returnedState = ParseQueryParam(query, "state");
    const std::string code = ParseQueryParam(query, "code");
    if (returnedState != state)
      error = "OAuth state mismatch -- discarding this response (possible CSRF).";
    else if (code.empty())
      error = "No authorization code in the TONE3000 redirect.";
    else
    {
      mStatus.store(AuthStatus::ExchangingToken);
      ok = ExchangeAuthorizationCode(code, codeVerifier, error);
    }
  }

  {
    std::lock_guard<std::mutex> lock(mMutex);
    mLastError = error;
  }
  mStatus.store(ok ? AuthStatus::SignedIn : AuthStatus::Failed);
  mFlowInProgress.store(false);
}

void OAuthFlow::RunResumeThread(std::string refreshToken)
{
  std::string error;
  const bool ok = ExchangeRefreshToken(refreshToken, error);
  {
    std::lock_guard<std::mutex> lock(mMutex);
    mLastError = error;
  }
  mStatus.store(ok ? AuthStatus::SignedIn : AuthStatus::Failed);
  mFlowInProgress.store(false);
}

bool OAuthFlow::ExchangeAuthorizationCode(const std::string& code, const std::string& codeVerifier,
                                          std::string& outError)
{
  std::ostringstream body;
  body << "grant_type=authorization_code" << "&client_id=" << UrlEncode(kClientId) << "&code=" << UrlEncode(code)
       << "&redirect_uri=" << UrlEncode(RedirectUri()) << "&code_verifier=" << UrlEncode(codeVerifier);

  const HttpResponse response = HttpPostForm(kTokenEndpoint, body.str());
  if (!response.success)
  {
    outError = "Token exchange request failed: " + response.error;
    return false;
  }
  if (response.statusCode != 200)
  {
    outError = "Token exchange returned HTTP " + std::to_string(response.statusCode) + ": " + response.body;
    return false;
  }
  return ApplyTokenResponse(response.body, outError);
}

bool OAuthFlow::ExchangeRefreshToken(const std::string& refreshToken, std::string& outError)
{
  std::ostringstream body;
  body << "grant_type=refresh_token" << "&client_id=" << UrlEncode(kClientId)
       << "&refresh_token=" << UrlEncode(refreshToken);

  const HttpResponse response = HttpPostForm(kTokenEndpoint, body.str());
  if (!response.success)
  {
    outError = "Refresh-token request failed: " + response.error;
    return false;
  }
  if (response.statusCode != 200)
  {
    // The stored refresh token may have expired/been revoked -- clear it
    // so we don't keep retrying with a dead token every launch.
    ClearRefreshToken();
    outError = "Refresh-token exchange returned HTTP " + std::to_string(response.statusCode) + ": " + response.body;
    return false;
  }
  return ApplyTokenResponse(response.body, outError);
}

bool OAuthFlow::ApplyTokenResponse(const std::string& jsonBody, std::string& outError)
{
  try
  {
    const auto json = nlohmann::json::parse(jsonBody);
    const std::string accessToken = json.value("access_token", "");
    const std::string refreshToken = json.value("refresh_token", "");
    if (accessToken.empty())
    {
      outError = "Token response had no access_token.";
      return false;
    }
    {
      std::lock_guard<std::mutex> lock(mMutex);
      mAccessToken = accessToken;
    }
    if (!refreshToken.empty())
      SaveRefreshToken(refreshToken);
    return true;
  }
  catch (const std::exception& e)
  {
    outError = std::string("Failed to parse token response: ") + e.what();
    return false;
  }
}

} // namespace tone3000
