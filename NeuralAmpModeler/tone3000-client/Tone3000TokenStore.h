#pragma once

#include <optional>
#include <string>

// Persists the TONE3000 refresh token in Windows Credential Manager --
// never in a plain file, never in source. Standalone-only; see
// Tone3000Config.h.

namespace tone3000
{

void SaveRefreshToken(const std::string& refreshToken);
std::optional<std::string> LoadRefreshToken();
void ClearRefreshToken();

} // namespace tone3000
