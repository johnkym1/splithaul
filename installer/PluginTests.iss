[Setup]
AppId=Grumpyman-Installer-Isolated-Tests
AppName=Dispatch Companion Installer Tests
AppVersion=1.0
DefaultDirName={localappdata}\Grumpyman\InstallerTests
PrivilegesRequired=lowest
OutputDir=build
OutputBaseFilename=PluginTests
Uninstallable=yes
CreateAppDir=yes
DisableProgramGroupPage=yes
SetupLogging=yes
CloseApplications=no

[Code]
var Checks: Integer;
#include "plugin_files.iss"

procedure Check(Value: Boolean; MessageText: String);
begin
  Checks := Checks + 1;
  if not Value then RaiseException(MessageText);
end;

procedure CurStepChanged(CurStep: TSetupStep);
var GameDir, Target, Source, OtherFile, OriginalHash, VersionOneHash, OtherHash, Backup: String;
begin
  if CurStep <> ssPostInstall then Exit;
  GameDir := ExpandConstant('{app}\fixtures\game');
  ForceDirectories(GameDir + '\bin\win_x64\plugins');
  Target := GameDir + '\bin\win_x64\plugins\GT_Telemetry.dll';
  OtherFile := GameDir + '\bin\win_x64\plugins\other-plugin.dll';
  Source := ExpandConstant('{app}\fixtures\new.dll');
  SaveStringToFile(Target, 'original plugin fixture', False);
  SaveStringToFile(OtherFile, 'preserve other plugin', False);
  SaveStringToFile(Source, 'new plugin version one', False);
  OriginalHash := GetSHA256OfFile(Target); OtherHash := GetSHA256OfFile(OtherFile);
  VersionOneHash := GetSHA256OfFile(Source);
  InstallPlugin('ATS', Source, GameDir);
  Check(GetSHA256OfFile(Target) = VersionOneHash, 'Initial telemetry install failed');
  Check(GetSHA256OfFile(OtherFile) = OtherHash, 'Another plugin was modified');
  Backup := GetIniString('ATS', 'OriginalBackup', '', PluginStateFile);
  Check(GetSHA256OfFile(Backup) = OriginalHash, 'Original plugin backup was not preserved');
  SaveStringToFile(Source, 'new plugin version two', False);
  InstallPlugin('ATS', Source, GameDir);
  Check(GetSHA256OfFile(Target) = GetSHA256OfFile(Source), 'Telemetry update failed');
  Check(GetIniString('ATS', 'OriginalHash', '', PluginStateFile) = OriginalHash, 'Update lost the pre-install original');
  RestorePlugin('ATS');
  Check(GetSHA256OfFile(Target) = OriginalHash, 'Uninstall did not restore original telemetry');
  GameDir := ExpandConstant('{app}\fixtures\no-original');
  Target := GameDir + '\bin\win_x64\plugins\GT_Telemetry.dll';
  InstallPlugin('ETS2', Source, GameDir);
  RestorePlugin('ETS2');
  Check(not FileExists(Target), 'Uninstall did not remove a newly installed telemetry DLL');
  InstallPlugin('ETS2', Source, GameDir);
  SaveStringToFile(Target, 'externally changed plugin', False);
  OtherHash := GetSHA256OfFile(Target);
  RestorePlugin('ETS2');
  Check(GetSHA256OfFile(Target) = OtherHash, 'Uninstall overwrote externally changed telemetry');
  SaveStringToFile(ExpandConstant('{app}\test-results.txt'), 'PASS: ' + IntToStr(Checks) + ' actual installer plugin backup/update/restore checks.', False);
end;
