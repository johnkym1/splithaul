{ Mod files are deliberately not registered for uninstall. Owned trailers in
  saves can depend on them even after the companion app has been removed. }
function ValidateModHome(Game, Folder: String): String;
begin
  Result := '';
  if (Folder = '') or not FileExists(AddBackslash(Folder) + 'config.cfg') then
    Result := 'Choose the ' + Game + ' data folder containing config.cfg (usually in Documents), not its Steam installation or mod folder. Start the game once first if this folder does not exist.';
end;

procedure InstallTrailerMod(Game, Source, GameHome: String);
var Target, CurrentHash, Backup, BackupFolder: String;
begin
  if ValidateModHome(Game, GameHome) <> '' then RaiseException(ValidateModHome(Game, GameHome));
  Target := AddBackslash(GameHome) + 'mod\' + ExtractFileName(Source);
  if not ForceDirectories(ExtractFilePath(Target)) then RaiseException('Could not create the ' + Game + ' mod folder.');
  if FileExists(Target) then begin
    CurrentHash := GetSHA256OfFile(Target);
    if CurrentHash = GetSHA256OfFile(Source) then begin
      Log('Trailer mod already matches this package: ' + Target);
      Exit;
    end;
    { Keep backups outside mod so they cannot appear as duplicate active mods. }
    BackupFolder := AddBackslash(GameHome) + 'mod-backups\DispatchCompanion\';
    if not ForceDirectories(BackupFolder) then RaiseException('Could not create the trailer mod backup folder.');
    Backup := BackupFolder + ExtractFileName(Source) + '.' + CurrentHash + '.bak';
    if not FileExists(Backup) then
      if not FileCopy(Target, Backup, True) then RaiseException('Could not back up the existing trailer mod.');
    if GetSHA256OfFile(Backup) <> CurrentHash then RaiseException('Trailer mod backup verification failed. Existing mod was preserved.');
  end;
  AtomicCopy(Source, Target);
  Log('Verified ' + Game + ' trailer mod installed: ' + Target);
end;
