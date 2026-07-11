#pragma once

#include <string>

// PKCE (Proof Key for Code Exchange, RFC 7636) helpers for the TONE3000
// OAuth flow. Standalone-only -- see Tone3000Config.h.

namespace tone3000
{

// A cryptographically random 43-128 character string per RFC 7636 (we use
// 64 bytes of randomness, base64url-encoded -> 86 chars).
std::string GenerateCodeVerifier();

// BASE64URL-ENCODE(SHA256(ASCII(code_verifier))) per RFC 7636 S256 method.
std::string DeriveCodeChallenge(const std::string& codeVerifier);

// A random string used as OAuth `state` to guard against CSRF on the
// redirect callback.
std::string GenerateState();

} // namespace tone3000
