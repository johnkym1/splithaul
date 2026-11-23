{ Only GT_Telemetry.dll is changed. Save the original once, preserve it across
  upgrades, and restore it on uninstall only if our installed hash still matches. }
function MoveFileEx(ExistingFile, NewFile: String; Flags: Cardinal): Boolean;
  external 'MoveFileExW@kernel32.dll stdcall';

function PluginStateFile: String;
begin
  Result := ExpandConstant('{app}\plugin-install-state.ini');
end;

procedure CheckedIni(Section, Key, Value: String);
begin
  if not SetIniString(Section, Key, Value, PluginStateFile) then
    RaiseException('Could not save telemetry backup information.');
end;

procedure AtomicCopy(Source, Target: String);
var Stage: String;
begin
  Stage := Target + '.grumpyman-update.tmp';
  if FileExists(Stage) then RaiseException('A previous telemetry update is unfinished: ' + Stage);
  if not FileCopy(Source, Stage, True) then RaiseException('Could not stage telemetry file: ' + Target);
  try
    if GetSHA256OfFile(Source) <> GetSHA256OfFile(Stage) then RaiseException('Telemetry copy verification failed.');
    if not MoveFileEx(Stage, Target, 9) then RaiseException('Could not replace telemetry file. Close the game and try again.');
  finally
    if FileExists(Stage) then DeleteFile(Stage);
  end;
end;

procedure InstallPlugin(Game, Source, GameFolder: String);
var Target, CurrentHash, OldTarget, OldHash, Backup, OriginalHash, NewHash: String;
begin
  Target := AddBackslash(GameFolder) + 'bin\win_x64\plugins\GT_Telemetry.dll';
  if not ForceDirectories(ExtractFilePath(Target)) then RaiseException('Could not create the game plugins folder.');
  NewHash := GetSHA256OfFile(Source);
  OldTarget := GetIniString(Game, 'Target', '', PluginStateFile);
  OldHash := GetIniString(Game, 'InstalledHash', '', PluginStateFile);
  if (OldTarget <> '') and (CompareText(OldTarget, Target) <> 0) then
    RaiseException('A different ' + Game + ' folder was used previously. Uninstall that plugin before changing its location.');
  CurrentHash := '';
  if FileExists(Target) then CurrentHash := GetSHA256OfFile(Target);
  if (OldTarget <> '') and (CurrentHash <> '') and (CurrentHash <> OldHash) then
    RaiseException('The ' + Game + ' telemetry file changed outside this installer. It has been left untouched.');
  ForceDirectories(ExpandConstant('{app}\plugin-backups'));
  if (OldTarget = '') then begin
    Backup := ''; OriginalHash := '';
    if CurrentHash <> '' then begin
      Backup := ExpandConstant('{app}\plugin-backups\') + Game + '-original-' + CurrentHash + '.dll';
      if not FileExists(Backup) then
        if not FileCopy(Target, Backup, True) then RaiseException('Could not back up the existing telemetry DLL.');
      if GetSHA256OfFile(Backup) <> CurrentHash then RaiseException('Original telemetry backup verification failed.');
      OriginalHash := CurrentHash;
    end;
    CheckedIni(Game, 'OriginalBackup', Backup);
    CheckedIni(Game, 'OriginalHash', OriginalHash);
  end;
  if (CurrentHash <> '') and (CurrentHash <> NewHash) then begin
    Backup := ExpandConstant('{app}\plugin-backups\') + Game + '-previous-' + CurrentHash + '.dll';
    if not FileExists(Backup) then
      if not FileCopy(Target, Backup, True) then RaiseException('Could not save the previous telemetry version.');
    if GetSHA256OfFile(Backup) <> CurrentHash then RaiseException('Previous telemetry backup verification failed.');
  end;
  CheckedIni(Game, 'Target', Target);
  CheckedIni(Game, 'InstalledHash', NewHash);
  if CurrentHash <> NewHash then AtomicCopy(Source, Target);
  Log('Verified ' + Game + ' telemetry installed: ' + Target);
end;

procedure RestorePlugin(Game: String);
var Target, InstalledHash, Backup, OriginalHash: String;
begin
  Target := GetIniString(Game, 'Target', '', PluginStateFile);
  if Target = '' then Exit;
  InstalledHash := GetIniString(Game, 'InstalledHash', '', PluginStateFile);
  if FileExists(Target) then
    if GetSHA256OfFile(Target) <> InstalledHash then begin
      Log('Preserved externally changed telemetry: ' + Target);
      Exit;
    end;
  Backup := GetIniString(Game, 'OriginalBackup', '', PluginStateFile);
  OriginalHash := GetIniString(Game, 'OriginalHash', '', PluginStateFile);
  if Backup <> '' then begin
    if not FileExists(Backup) then RaiseException('Original telemetry backup is missing. Existing telemetry was preserved.');
    if GetSHA256OfFile(Backup) <> OriginalHash then RaiseException('Original telemetry backup hash changed. Existing telemetry was preserved.');
    AtomicCopy(Backup, Target);
  end else if FileExists(Target) then begin
    if not DeleteFile(Target) then RaiseException('Could not remove telemetry. Close the game and retry.');
  end;
end;
