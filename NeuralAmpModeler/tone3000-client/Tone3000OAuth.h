#pragma once

#include <atomic>
#include <mutex>
#include <string>

// Orchestrates the TONE3000 OAuth 2.0 + PKCE login flow: opens the
// system browser to the authorize endpoint, listens on a loopback
// redirect URI for the callback, exchanges the authorization code for
// tokens, and persists the refresh token via Tone3000TokenStore.
// Standalone-only -- see Tone3000Config.h. Never call into this from
// VST3 code.
//
// Threading: BeginLogin()/TryResume() do their work on a detached
// background thread (network I/O + a blocking local socket accept must
// not run on iPlug2's UI thread). Poll GetStatus() from OnIdle() rather
// than expecting a callback on the UI thread.

namespace tone3000
{

enum class AuthStatus
{
  SignedOut,
  Connecting, // system browser opened; waiting for the redirect callback
  ExchangingToken,
  SignedIn,
  Failed
};

class OAuthFlow
{
public:
  // Opens the system browser and runs the full PKCE flow. No-op if a
  // flow is already in progress.
  void BeginLogin();

  // Tries to sign back in using a previously stored refresh token,
  // without opening a browser. No-op if there's no stored token.
  void TryResume();

  void SignOut();

  AuthStatus GetStatus() const { return mStatus.load(); }
  std::string GetLastError() const;
  std::string GetAccessToken() const;

private:
  void RunLoginThread();
  void RunResumeThread(std::string refreshToken);
  bool ExchangeAuthorizationCode(const std::string& code, const std::string& codeVerifier, std::string& outError);
  bool ExchangeRefreshToken(const std::string& refreshToken, std::string& outError);
  bool ApplyTokenResponse(const std::string& jsonBody, std::string& outError);

  std::atomic<AuthStatus> mStatus{AuthStatus::SignedOut};
  std::atomic<bool> mFlowInProgress{false};
  mutable std::mutex mMutex;
  std::string mLastError;
  std::string mAccessToken;
};

} // namespace tone3000
