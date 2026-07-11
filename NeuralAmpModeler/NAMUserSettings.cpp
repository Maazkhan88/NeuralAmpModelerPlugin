#include "NAMUserSettings.h"

#include <filesystem>
#include <fstream>

#include <shlobj.h>
#include <windows.h>
#pragma comment(lib, "shell32.lib")

namespace
{

// %LOCALAPPDATA%\ToneCast\settings.txt -- created on first use.
std::wstring GetSettingsPath()
{
  wchar_t* localAppData = nullptr;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData)) || !localAppData)
    return L"";
  std::wstring dir(localAppData);
  CoTaskMemFree(localAppData);
  dir += L"\\ToneCast\\";
  CreateDirectoryW(dir.c_str(), nullptr);
  return dir + L"settings.txt";
}

std::string TrimCR(std::string s)
{
  while (!s.empty() && (s.back() == '\r' || s.back() == '\n'))
    s.pop_back();
  return s;
}

} // namespace

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
