#pragma once

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
