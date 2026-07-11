#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

// ToneCast: remembers the last-used NAM/IR file and its containing
// folder across app restarts, so the standalone app doesn't start from
// a blank library every launch. Plain local file I/O, no networking --
// unlike tone3000-client/, this is safe to compile into both the app and
// VST3 targets. (Whether it's actually *used* to auto-restore a model at
// startup is a separate, APP_API-gated decision made in
// NeuralAmpModeler.cpp -- see the comment there. A VST3 instance saving
// these on every load is harmless and even useful for the next time the
// standalone app is opened, but a VST3 instance restoring from these
// unconditionally would clobber a DAW project's own saved model.)

struct NAMUserSettings
{
  std::string modelDir;
  std::string irDir;
  std::string modelPath;
  std::string irPath;
};

NAMUserSettings LoadNAMUserSettings();
void SaveNAMUserSettings(const NAMUserSettings& settings);

// Read-modify-write helpers so callers don't need to hand-roll the
// load/mutate/save sequence (and risk clobbering the other fields).
void UpdateNAMModelSettings(const std::string& modelPath);
void UpdateNAMIRSettings(const std::string& irPath);

// ToneCast's single durable user library. Both local imports and TONE3000
// downloads are installed below %LOCALAPPDATA%\ToneCast\Library so the
// standalone app can reconstruct its complete library after a restart.
enum class NAMLibraryFileKind
{
  Model,
  IR,
  Preset
};

std::filesystem::path GetNAMLibraryRoot();
std::filesystem::path GetNAMLibraryDirectory(NAMLibraryFileKind kind);
bool InitializeNAMLibrary();

// Copies an existing file into the matching library directory. Returns the
// canonical library path as UTF-8, or an empty string if installation fails.
// Re-importing identical content reuses the existing file; a different file
// with the same name receives a numeric suffix instead of being overwritten.
std::string InstallNAMFileInLibrary(const std::string& sourcePath, NAMLibraryFileKind kind);

// Imports all supported files in one directory (non-recursive, matching the
// existing file browser). This preserves the complete local library shown
// beside a selected file, rather than remembering only that one file.
size_t ImportNAMDirectoryToLibrary(const std::string& sourceDirectory, NAMLibraryFileKind kind);
