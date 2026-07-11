#include "Tone3000TokenStore.h"
#include "Tone3000Config.h"

#include <windows.h>
#include <wincred.h>
#pragma comment(lib, "advapi32.lib")

namespace tone3000
{

void SaveRefreshToken(const std::string& refreshToken)
{
  CREDENTIALW cred = {};
  cred.Type = CRED_TYPE_GENERIC;
  cred.TargetName = const_cast<LPWSTR>(kCredentialTargetName);
  cred.CredentialBlobSize = static_cast<DWORD>(refreshToken.size());
  cred.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(refreshToken.data()));
  cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

  // Best-effort: if this fails, the user will just be prompted to
  // reconnect next launch rather than staying signed in. Not fatal.
  CredWriteW(&cred, 0);
}

std::optional<std::string> LoadRefreshToken()
{
  PCREDENTIALW cred = nullptr;
  if (!CredReadW(kCredentialTargetName, CRED_TYPE_GENERIC, 0, &cred) || cred == nullptr)
    return std::nullopt;

  std::string token(reinterpret_cast<const char*>(cred->CredentialBlob), cred->CredentialBlobSize);
  CredFree(cred);
  return token;
}

void ClearRefreshToken() { CredDeleteW(kCredentialTargetName, CRED_TYPE_GENERIC, 0); }

} // namespace tone3000
