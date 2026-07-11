#pragma once

// ToneCast (Phase 4, pulled forward): configuration for the TONE3000 API
// client. This whole tone3000-client/ directory is standalone-only per
// docs/dsp-architecture.md and CLAUDE.md's architecture rules -- it must
// never be compiled into the VST3 target. That's enforced at the build
// level: these files are only added to NeuralAmpModeler-app.vcxproj, not
// NeuralAmpModeler-vst3.vcxproj (see the .vcxproj diff for this change).
//
// API reference: https://www.tone3000.com/api

namespace tone3000
{

// TODO(user): replace with the real client_id once registered at
// tone3000.com as a developer application. A placeholder here does not
// leak anything sensitive -- client_id/"Publishable Key" is meant to be
// public per TONE3000's own docs (it identifies the app in the OAuth
// flow, it is not a secret). The corresponding Secret Key (t3k_cs_...),
// if TONE3000 ever issues one for this app, must NEVER be committed here
// or anywhere in source -- see docs/decisions-log.md.
constexpr const char* kClientId = "TONECAST_CLIENT_ID_PLACEHOLDER";

constexpr const char* kApiBaseUrl = "https://www.tone3000.com/api/v1";
constexpr const char* kAuthorizeEndpoint = "https://www.tone3000.com/api/v1/oauth/authorize";
constexpr const char* kTokenEndpoint = "https://www.tone3000.com/api/v1/oauth/token";

// Loopback redirect URI for the system-browser PKCE flow. The exact port
// must be registered with TONE3000 alongside the client_id -- if they
// require a fixed port, update kRedirectPort to match rather than relying
// on OS-assigned ephemeral ports.
constexpr int kRedirectPort = 17390;
constexpr const char* kRedirectUriTemplate = "http://127.0.0.1:%d/tonecast-oauth-callback";

// Windows Credential Manager target name for the stored refresh token.
// See Tone3000TokenStore.h -- tokens are never written to a plain file or
// to source; the OS credential store is used instead.
constexpr const wchar_t* kCredentialTargetName = L"ToneCast/TONE3000/RefreshToken";

} // namespace tone3000
