[Setup]
AppId=Grumpyman-Installer-Mod-Tests
AppName=SplitHaul Mod Installer Tests
AppVersion=1.0
DefaultDirName={localappdata}\Grumpyman\ModInstallerTests
PrivilegesRequired=lowest
OutputDir=build
OutputBaseFilename=ModTests
Uninstallable=yes
CreateAppDir=yes
DisableProgramGroupPage=yes
SetupLogging=yes
CloseApplications=no

[Code]
var Checks: Integer;
#include "plugin_files.iss"
#include "mod_files.iss"

procedure Check(Value: Boolean; MessageText: String);
begin
  Checks := Checks + 1;
  if not Value then RaiseException(MessageText);
end;

procedure CurStepChanged(CurStep: TSetupStep);
var Home, Target, Source, Other, Backup, OriginalHash, FirstHash, OtherHash: String;
    Failed: Boolean;
begin
  if CurStep <> ssPostInstall then Exit;
  Home := ExpandConstant('{app}\fixtures\game home');
  ForceDirectories(Home + '\mod');
  Check(ValidateModHome('ATS', Home) <> '', 'A non-game folder was accepted');
  SaveStringToFile(Home + '\config.cfg', 'config fixture', False);
  Check(ValidateModHome('ATS', Home) = '', 'A custom game home was rejected');
  Source := ExpandConstant('{app}\fixtures\GT_Tanker_Combinations_ATS.scs');
  Target := Home + '\mod\GT_Tanker_Combinations_ATS.scs';
  Other := Home + '\mod\another-mod.scs';
  SaveStringToFile(Other, 'untouched other mod', False);
  OtherHash := GetSHA256OfFile(Other);
  SaveStringToFile(Source, 'version one fixture', False);
  FirstHash := GetSHA256OfFile(Source);
  InstallTrailerMod('ATS', Source, Home);
  Check(GetSHA256OfFile(Target) = FirstHash, 'Fresh mod installation failed');
  Check(not DirExists(Home + '\mod-backups'), 'Fresh install created an unnecessary backup');
  InstallTrailerMod('ATS', Source, Home);
  Check(not DirExists(Home + '\mod-backups'), 'Unchanged install created duplicate backup');
  SaveStringToFile(Source, 'version two fixture', False);
  InstallTrailerMod('ATS', Source, Home);
  Backup := Home + '\mod-backups\DispatchCompanion\GT_Tanker_Combinations_ATS.scs.' + FirstHash + '.bak';
  Check(GetSHA256OfFile(Backup) = FirstHash, 'Update lost the previous mod');
  Check(GetSHA256OfFile(Target) = GetSHA256OfFile(Source), 'Mod update failed');
  Check(GetSHA256OfFile(Other) = OtherHash, 'Another mod was changed');
  Check(not FileExists(Target + '.grumpyman-update.tmp'), 'Staging file was left behind');
  OriginalHash := GetSHA256OfFile(Target);
  Backup := Home + '\mod-backups\DispatchCompanion\GT_Tanker_Combinations_ATS.scs.' + OriginalHash + '.bak';
  SaveStringToFile(Backup, 'corrupted existing backup', False);
  SaveStringToFile(Source, 'version three fixture', False);
  Failed := False;
  try InstallTrailerMod('ATS', Source, Home); except Failed := True; end;
  Check(Failed, 'A corrupt backup did not stop replacement');
  Check(GetSHA256OfFile(Target) = OriginalHash, 'Corrupt backup failure changed the installed mod');
  DeleteFile(Backup);
  SaveStringToFile(Target + '.grumpyman-update.tmp', 'unfinished update', False);
  Failed := False;
  try InstallTrailerMod('ATS', Source, Home); except Failed := True; end;
  Check(Failed, 'An unfinished update was overwritten');
  Check(GetSHA256OfFile(Target) = OriginalHash, 'Staging conflict changed the installed mod');
  DeleteFile(Target + '.grumpyman-update.tmp');
  SaveStringToFile(ExpandConstant('{app}\test-results.txt'), 'PASS: ' + IntToStr(Checks) + ' actual installer mod install/update/backup/failure checks.', False);
end;
