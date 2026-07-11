#include "Tone3000Pkce.h"

#include <stdexcept>
#include <vector>

#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

namespace
{

std::vector<unsigned char> RandomBytes(size_t count)
{
  std::vector<unsigned char> buffer(count);
  const NTSTATUS status =
    BCryptGenRandom(nullptr, buffer.data(), static_cast<ULONG>(buffer.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
  if (status < 0)
    throw std::runtime_error("BCryptGenRandom failed");
  return buffer;
}

std::vector<unsigned char> Sha256(const std::string& input)
{
  BCRYPT_ALG_HANDLE alg = nullptr;
  NTSTATUS status = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
  if (status < 0)
    throw std::runtime_error("BCryptOpenAlgorithmProvider(SHA256) failed");

  std::vector<unsigned char> digest(32); // SHA-256 is always 32 bytes
  status = BCryptHash(alg, nullptr, 0, reinterpret_cast<PUCHAR>(const_cast<char*>(input.data())),
                      static_cast<ULONG>(input.size()), digest.data(), static_cast<ULONG>(digest.size()));
  BCryptCloseAlgorithmProvider(alg, 0);
  if (status < 0)
    throw std::runtime_error("BCryptHash(SHA256) failed");
  return digest;
}

// RFC 4648 base64url, no padding -- what RFC 7636 requires for both the
// code_verifier's own encoding and the code_challenge derivation.
std::string Base64UrlEncode(const std::vector<unsigned char>& data)
{
  static const char* kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  std::string out;
  out.reserve((data.size() + 2) / 3 * 4);

  size_t i = 0;
  while (i + 3 <= data.size())
  {
    const unsigned int chunk = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
    out.push_back(kAlphabet[(chunk >> 18) & 0x3F]);
    out.push_back(kAlphabet[(chunk >> 12) & 0x3F]);
    out.push_back(kAlphabet[(chunk >> 6) & 0x3F]);
    out.push_back(kAlphabet[chunk & 0x3F]);
    i += 3;
  }

  const size_t remaining = data.size() - i;
  if (remaining == 1)
  {
    const unsigned int chunk = data[i] << 16;
    out.push_back(kAlphabet[(chunk >> 18) & 0x3F]);
    out.push_back(kAlphabet[(chunk >> 12) & 0x3F]);
  }
  else if (remaining == 2)
  {
    const unsigned int chunk = (data[i] << 16) | (data[i + 1] << 8);
    out.push_back(kAlphabet[(chunk >> 18) & 0x3F]);
    out.push_back(kAlphabet[(chunk >> 12) & 0x3F]);
    out.push_back(kAlphabet[(chunk >> 6) & 0x3F]);
  }
  // No '=' padding -- deliberately omitted per RFC 7636.
  return out;
}

} // namespace

namespace tone3000
{

std::string GenerateCodeVerifier()
{
  // 64 random bytes -> 86-character base64url string, within RFC 7636's
  // required 43-128 character range.
  return Base64UrlEncode(RandomBytes(64));
}

std::string DeriveCodeChallenge(const std::string& codeVerifier)
{
  return Base64UrlEncode(Sha256(codeVerifier));
}

std::string GenerateState()
{
  return Base64UrlEncode(RandomBytes(24));
}

} // namespace tone3000
