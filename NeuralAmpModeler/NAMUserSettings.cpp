#include "NAMUserSettings.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <system_error>

#include <shlobj.h>
#include <windows.h>
#pragma comment(lib, "shell32.lib")

namespace
{

std::filesystem::path GetToneCastDataRoot()
{
  wchar_t* localAppData = nullptr;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData)) || !localAppData)
    return {};
  std::filesystem::path root(localAppData);
  CoTaskMemFree(localAppData);
  return root / L"ToneCast";
}

// %LOCALAPPDATA%\ToneCast\settings.txt -- created on first use.
std::wstring GetSettingsPath()
{
  const std::filesystem::path root = GetToneCastDataRoot();
  if (root.empty())
    return L"";
  std::error_code ec;
  std::filesystem::create_directories(root, ec);
  return (root / L"settings.txt").wstring();
}

std::string TrimCR(std::string s)
{
  while (!s.empty() && (s.back() == '\r' || s.back() == '\n'))
    s.pop_back();
  return s;
}

std::string PathToUTF8(const std::filesystem::path& path)
{
  const auto u8 = path.u8string();
  return std::string(u8.begin(), u8.end());
}

bool FilesEqual(const std::filesystem::path& a, const std::filesystem::path& b)
{
  std::error_code ec;
  if (std::filesystem::file_size(a, ec) != std::filesystem::file_size(b, ec) || ec)
    return false;

  std::ifstream left(a, std::ios::binary);
  std::ifstream right(b, std::ios::binary);
  if (!left || !right)
    return false;

  constexpr size_t kBufferSize = 64 * 1024;
  char leftBuffer[kBufferSize];
  char rightBuffer[kBufferSize];
  while (left && right)
  {
    left.read(leftBuffer, kBufferSize);
    right.read(rightBuffer, kBufferSize);
    const auto leftCount = left.gcount();
    if (leftCount != right.gcount())
      return false;
    if (!std::equal(leftBuffer, leftBuffer + leftCount, rightBuffer))
      return false;
  }
  return true;
}

void MigrateLegacyTone3000Cache()
{
  const auto root = GetToneCastDataRoot();
  if (root.empty())
    return;
  const auto legacy = root / L"tone3000-cache";
  std::error_code ec;
  if (!std::filesystem::is_directory(legacy, ec))
    return;

  for (std::filesystem::directory_iterator it(legacy, ec), end; !ec && it != end; it.increment(ec))
  {
    if (!it->is_regular_file(ec))
      continue;
    std::wstring extension = it->path().extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);
    NAMLibraryFileKind kind;
    if (extension == L".nam")
      kind = NAMLibraryFileKind::Model;
    else if (extension == L".wav")
      kind = NAMLibraryFileKind::IR;
    else
      continue;
    InstallNAMFileInLibrary(PathToUTF8(it->path()), kind);
  }
}

} // namespace

std::filesystem::path GetNAMLibraryRoot()
{
  const auto root = GetToneCastDataRoot();
  return root.empty() ? std::filesystem::path() : root / L"Library";
}

std::filesystem::path GetNAMLibraryDirectory(NAMLibraryFileKind kind)
{
  const auto root = GetNAMLibraryRoot();
  if (root.empty())
    return {};
  switch (kind)
  {
    case NAMLibraryFileKind::Model: return root / L"Models";
    case NAMLibraryFileKind::IR: return root / L"IRs";
    case NAMLibraryFileKind::Preset: return root / L"Presets";
  }
  return {};
}

bool InitializeNAMLibrary()
{
  std::error_code ec;
  for (const auto kind : {NAMLibraryFileKind::Model, NAMLibraryFileKind::IR, NAMLibraryFileKind::Preset})
  {
    std::filesystem::create_directories(GetNAMLibraryDirectory(kind), ec);
    if (ec)
      return false;
  }
  MigrateLegacyTone3000Cache();
  return true;
}

std::string InstallNAMFileInLibrary(const std::string& sourcePath, NAMLibraryFileKind kind)
{
  std::error_code ec;
  const auto source = std::filesystem::u8path(sourcePath);
  if (!std::filesystem::is_regular_file(source, ec) || ec)
    return {};

  const auto targetDir = GetNAMLibraryDirectory(kind);
  std::filesystem::create_directories(targetDir, ec);
  if (ec)
    return {};

  if (std::filesystem::equivalent(source.parent_path(), targetDir, ec) && !ec)
    return PathToUTF8(source);
  ec.clear();

  auto destination = targetDir / source.filename();
  if (std::filesystem::exists(destination, ec))
  {
    if (!ec && FilesEqual(source, destination))
      return PathToUTF8(destination);
    const auto stem = source.stem().wstring();
    const auto extension = source.extension().wstring();
    for (unsigned int suffix = 2; suffix < 10000; ++suffix)
    {
      destination = targetDir / (stem + L" (" + std::to_wstring(suffix) + L")" + extension);
      if (!std::filesystem::exists(destination, ec))
        break;
      if (!ec && FilesEqual(source, destination))
        return PathToUTF8(destination);
      ec.clear();
    }
  }

  const auto temporary = destination.wstring() + L".importing";
  std::filesystem::copy_file(source, temporary, std::filesystem::copy_options::overwrite_existing, ec);
  if (ec)
    return {};
  std::filesystem::rename(temporary, destination, ec);
  if (ec)
  {
    std::filesystem::remove(temporary);
    return {};
  }
  return PathToUTF8(destination);
}

size_t ImportNAMDirectoryToLibrary(const std::string& sourceDirectory, NAMLibraryFileKind kind)
{
  const auto directory = std::filesystem::u8path(sourceDirectory);
  std::error_code ec;
  if (!std::filesystem::is_directory(directory, ec) || ec)
    return 0;

  size_t installed = 0;
  for (std::filesystem::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec))
  {
    if (!it->is_regular_file(ec))
      continue;
    std::wstring extension = it->path().extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);
    const bool supported = (kind == NAMLibraryFileKind::Model && extension == L".nam")
                           || (kind == NAMLibraryFileKind::IR && extension == L".wav");
    if (!supported)
      continue;
    if (!InstallNAMFileInLibrary(PathToUTF8(it->path()), kind).empty())
      ++installed;
  }
  return installed;
}

NAMUserSettings LoadNAMUserSettings()
{
  NAMUserSettings settings;
  const std::wstring path = GetSettingsPath();
  if (path.empty())
    return settings;

  std::ifstream in(path);
  if (!in)
    return settings;

  std::string line;
  while (std::getline(in, line))
  {
    line = TrimCR(line);
    const size_t eq = line.find('=');
    if (eq == std::string::npos)
      continue;
    const std::string key = line.substr(0, eq);
    const std::string value = line.substr(eq + 1);
    if (key == "model_dir")
      settings.modelDir = value;
    else if (key == "ir_dir")
      settings.irDir = value;
    else if (key == "model_path")
      settings.modelPath = value;
    else if (key == "ir_path")
      settings.irPath = value;
  }
  return settings;
}

void SaveNAMUserSettings(const NAMUserSettings& settings)
{
  const std::wstring path = GetSettingsPath();
  if (path.empty())
    return;

  std::ofstream out(path, std::ios::trunc);
  if (!out)
    return;
  out << "model_dir=" << settings.modelDir << "\n";
  out << "ir_dir=" << settings.irDir << "\n";
  out << "model_path=" << settings.modelPath << "\n";
  out << "ir_path=" << settings.irPath << "\n";
}

void UpdateNAMModelSettings(const std::string& modelPath)
{
  NAMUserSettings settings = LoadNAMUserSettings();
  settings.modelPath = modelPath;
  try
  {
    // parent_path().u8string() returns std::u8string (char8_t) in C++20;
    // this is a byte-for-byte reinterpretation back to std::string, not a
    // re-encoding -- both are already UTF-8.
    const auto u8dir = std::filesystem::u8path(modelPath).parent_path().u8string();
    settings.modelDir.assign(u8dir.begin(), u8dir.end());
  }
  catch (const std::exception&)
  {
    // Leave modelDir as whatever was previously saved.
  }
  SaveNAMUserSettings(settings);
}

void UpdateNAMIRSettings(const std::string& irPath)
{
  NAMUserSettings settings = LoadNAMUserSettings();
  settings.irPath = irPath;
  try
  {
    const auto u8dir = std::filesystem::u8path(irPath).parent_path().u8string();
    settings.irDir.assign(u8dir.begin(), u8dir.end());
  }
  catch (const std::exception&)
  {
    // Leave irDir as whatever was previously saved.
  }
  SaveNAMUserSettings(settings);
}
