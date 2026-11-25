#include "build\version.iss"

[Setup]
#ifdef InstallerTestBuild
AppId=Grumpyman-Dispatch-AllInOne-Isolated-Tests
#else
AppId={{04C09966-B3D8-4D9E-BB7B-2759517FDC82}
#endif
AppName=Dispatch Companion
AppVersion={#AppVersion}
AppPublisher=Grumpyman
AppPublisherURL=https://github.com/johnkym1
AppSupportURL=https://github.com/johnkym1
DefaultDirName={autopf}\Grumpyman\Dispatch Companion
DefaultGroupName=Grumpyman\Dispatch Companion
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=commandline
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir=..\dist
OutputBaseFilename=DispatchCompanion-Setup-{#AppVersion}-win64
SetupIconFile=build\grumpyman.ico
UninstallDisplayIcon={app}\grumpyman.ico
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
InfoBeforeFile=DriverGuide.txt
CloseApplications=no
RestartApplications=no
SetupLogging=yes
UninstallDisplayName=Dispatch Companion
UsePreviousTasks=no

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Flags: unchecked
Name: "atsplugin"; Description: "Install GT_Telemetry.dll for American Truck Simulator"
Name: "etsplugin"; Description: "Install GT_Telemetry.dll for Euro Truck Simulator 2"
Name: "atsmod"; Description: "Install GT multi-trailer mod for ATS (doubles and triples)"
Name: "etsmod"; Description: "Install GT multi-trailer mod for ETS2 (doubles)"

[Files]
Source: "build\payload\DispatchPlanner.exe"; DestDir: "{app}"; Flags: ignoreversion; BeforeInstall: BackupApplication
Source: "build\payload\installed-layout.json"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\payload\catalogs\*.json"; DestDir: "{app}\catalogs"; Flags: ignoreversion
Source: "build\payload\plugins\ats\GT_Telemetry.dll"; DestDir: "{app}\plugins\ats"; Flags: ignoreversion
Source: "build\payload\plugins\ets2\GT_Telemetry.dll"; DestDir: "{app}\plugins\ets2"; Flags: ignoreversion
Source: "build\payload\mods\*.scs"; DestDir: "{app}\mods"; Flags: ignoreversion
Source: "build\payload\notices\*"; DestDir: "{app}\notices"; Flags: ignoreversion
Source: "build\payload\package-manifest.json"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\grumpyman.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "DriverGuide.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "TrailerMods.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Dispatch Companion"; Filename: "{app}\DispatchPlanner.exe"; IconFilename: "{app}\grumpyman.ico"
Name: "{group}\Driver guide"; Filename: "{app}\DriverGuide.txt"
Name: "{group}\Trailer mod guide"; Filename: "{app}\TrailerMods.txt"
Name: "{group}\Uninstall Dispatch Companion"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Dispatch Companion"; Filename: "{app}\DispatchPlanner.exe"; IconFilename: "{app}\grumpyman.ico"; Tasks: desktopicon

[Run]
Filename: "{app}\DispatchPlanner.exe"; Description: "Open Dispatch Companion"; Flags: nowait postinstall skipifsilent runasoriginaluser unchecked

[UninstallDelete]
Type: files; Name: "{app}\plugin-install-state.ini"

[Code]
var GameFolders, ModFolders: TInputDirWizardPage;

#include "plugin_files.iss"
#include "mod_files.iss"
#include "build\supported_hosts.iss"

function ProcessRunning(Name: String): Boolean;
var Locator, Services, Processes: Variant;
begin
  Locator := CreateOleObject('WbemScripting.SWbemLocator');
  Services := Locator.ConnectServer('', 'root\CIMV2');
  Processes := Services.ExecQuery('SELECT Name FROM Win32_Process WHERE Name="' + Name + '"');
  Result := Processes.Count > 0;
end;

function FindGame(AppId, FolderName: String): String;
var Steam: String;
begin
  Result := '';
  if RegQueryStringValue(HKLM32, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\Steam App ' + AppId, 'InstallLocation', Result) and DirExists(Result) then Exit;
  Result := '';
  if RegQueryStringValue(HKCU, 'Software\Valve\Steam', 'SteamPath', Steam) then
    if DirExists(AddBackslash(Steam) + 'steamapps\common\' + FolderName) then Result := AddBackslash(Steam) + 'steamapps\common\' + FolderName;
end;

procedure InitializeWizard;
begin
  GameFolders := CreateInputDirPage(wpSelectTasks, 'Game folders', 'Choose the games for telemetry installation',
    'Select each game''s main folder, not its plugins folder. Both games must be closed. Existing GT_Telemetry.dll files are backed up; other plugins are preserved.', False, '');
  GameFolders.Add('American Truck Simulator:');
  GameFolders.Add('Euro Truck Simulator 2:');
  GameFolders.Values[0] := ExpandConstant('{param:ATSDIR|}');
  GameFolders.Values[1] := ExpandConstant('{param:ETSDIR|}');
  if GameFolders.Values[0] = '' then GameFolders.Values[0] := FindGame('270880', 'American Truck Simulator');
  if GameFolders.Values[1] = '' then GameFolders.Values[1] := FindGame('227300', 'Euro Truck Simulator 2');
  ModFolders := CreateInputDirPage(GameFolders.ID, 'Trailer mod folders', 'Choose each game''s data folder in Documents',
    'Select the folder containing config.cfg. Setup copies the mod into its mod subfolder. For OneDrive or a custom -homedir, browse to the data folder the game actually uses. Enable the mod in your profile''s Mod Manager after installation.', False, '');
  ModFolders.Add('American Truck Simulator data folder:');
  ModFolders.Add('Euro Truck Simulator 2 data folder:');
  ModFolders.Values[0] := ExpandConstant('{param:ATSHOME|}');
  ModFolders.Values[1] := ExpandConstant('{param:ETSHOME|}');
  if ModFolders.Values[0] = '' then ModFolders.Values[0] := GetPreviousData('ATSHome', ExpandConstant('{userdocs}\American Truck Simulator'));
  if ModFolders.Values[1] = '' then ModFolders.Values[1] := GetPreviousData('ETSHome', ExpandConstant('{userdocs}\Euro Truck Simulator 2'));
end;

procedure RegisterPreviousData(PreviousDataKey: Integer);
begin
  if WizardIsTaskSelected('atsmod') then SetPreviousData(PreviousDataKey, 'ATSHome', ModFolders.Values[0]);
  if WizardIsTaskSelected('etsmod') then SetPreviousData(PreviousDataKey, 'ETSHome', ModFolders.Values[1]);
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = wpFinished then begin
    WizardForm.FinishedHeadingLabel.Caption := 'IN GAME: PRESS SHIFT + F9 TO OPEN DISPATCH COMPANION';
    if WizardIsTaskSelected('atsmod') or WizardIsTaskSelected('etsmod') then
      WizardForm.FinishedLabel.Caption := 'Dispatch Companion and the selected game files are installed.' + #13#10 + #13#10 +
        'Before loading your profile, open the game''s Mod Manager and enable GT Multi-Trailer Combinations for that game. Already enabled? Keep that same mod entry enabled.' + #13#10 + #13#10 +
        'See the Trailer mod guide in the Start menu for the new combinations. Start Dispatch Companion once before using Shift + F9.';
  end;
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := (PageID = GameFolders.ID) and not (WizardIsTaskSelected('atsplugin') or WizardIsTaskSelected('etsplugin'));
  if PageID = ModFolders.ID then Result := not (WizardIsTaskSelected('atsmod') or WizardIsTaskSelected('etsmod'));
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = wpSelectTasks then begin
    GameFolders.Edits[0].Enabled := WizardIsTaskSelected('atsplugin');
    GameFolders.Buttons[0].Enabled := WizardIsTaskSelected('atsplugin');
    GameFolders.Edits[1].Enabled := WizardIsTaskSelected('etsplugin');
    GameFolders.Buttons[1].Enabled := WizardIsTaskSelected('etsplugin');
    ModFolders.Edits[0].Enabled := WizardIsTaskSelected('atsmod');
    ModFolders.Buttons[0].Enabled := WizardIsTaskSelected('atsmod');
    ModFolders.Edits[1].Enabled := WizardIsTaskSelected('etsmod');
    ModFolders.Buttons[1].Enabled := WizardIsTaskSelected('etsmod');
  end;
  if CurPageID = ModFolders.ID then begin
    if WizardIsTaskSelected('atsmod') then
      if ValidateModHome('ATS', ModFolders.Values[0]) <> '' then begin
        SuppressibleMsgBox(ValidateModHome('ATS', ModFolders.Values[0]), mbError, MB_OK, IDOK); Result := False; Exit;
      end;
    if WizardIsTaskSelected('etsmod') then
      if ValidateModHome('ETS2', ModFolders.Values[1]) <> '' then begin
        SuppressibleMsgBox(ValidateModHome('ETS2', ModFolders.Values[1]), mbError, MB_OK, IDOK); Result := False;
      end;
  end;
end;

function ValidateGame(Game, Folder, ExeName: String): String;
var Exe: String;
begin
  Result := '';
  Exe := AddBackslash(Folder) + 'bin\win_x64\' + ExeName;
  if not FileExists(Exe) then Result := 'Choose the correct ' + Game + ' folder. Missing: ' + Exe
  else if not SupportedHost(Game, GetSHA256OfFile(Exe)) then Result := Game + ' has an unsupported game build. No telemetry DLL was replaced. Use a compatible Dispatch Companion release.';
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  try
    if ProcessRunning('DispatchPlanner.exe') then begin Result := 'Close Dispatch Companion before installing or updating.'; Exit; end;
    if (WizardIsTaskSelected('atsplugin') or WizardIsTaskSelected('etsplugin')) then begin
      if not IsAdminInstallMode then begin Result := 'Game-plugin installation requires administrator mode. Run Setup normally or leave the game-plugin tasks unchecked.'; Exit; end;
      if ProcessRunning('amtrucks.exe') or ProcessRunning('eurotrucks2.exe') then begin Result := 'Close ATS and ETS2 normally before replacing telemetry DLLs. No SDK reload is needed.'; Exit; end;
    end;
    if WizardIsTaskSelected('atsplugin') then begin Result := ValidateGame('ATS', GameFolders.Values[0], 'amtrucks.exe'); if Result <> '' then Exit; end;
    if WizardIsTaskSelected('etsplugin') then begin Result := ValidateGame('ETS2', GameFolders.Values[1], 'eurotrucks2.exe'); if Result <> '' then Exit; end;
    if WizardIsTaskSelected('atsmod') then begin
      if ProcessRunning('amtrucks.exe') then begin Result := 'Close ATS normally before installing its trailer mod.'; Exit; end;
      Result := ValidateModHome('ATS', ModFolders.Values[0]); if Result <> '' then Exit;
    end;
    if WizardIsTaskSelected('etsmod') then begin
      if ProcessRunning('eurotrucks2.exe') then begin Result := 'Close ETS2 normally before installing its trailer mod.'; Exit; end;
      Result := ValidateModHome('ETS2', ModFolders.Values[1]); if Result <> '' then Exit;
    end;
    if WizardIsTaskSelected('atsmod') and WizardIsTaskSelected('etsmod') then
      if CompareText(RemoveBackslashUnlessRoot(ExpandFileName(ModFolders.Values[0])), RemoveBackslashUnlessRoot(ExpandFileName(ModFolders.Values[1]))) = 0 then
        Result := 'Choose separate ATS and ETS2 data folders. The two games must not share a mod folder.';
  except
    Result := 'Could not verify the running applications or game files. Installation was stopped. ' + GetExceptionMessage;
  end;
end;

procedure BackupApplication;
var Existing, Backup: String;
begin
  Existing := ExpandConstant('{app}\DispatchPlanner.exe');
  if not FileExists(Existing) then Exit;
  ForceDirectories(ExpandConstant('{app}\app-backups'));
  Backup := ExpandConstant('{app}\app-backups\DispatchPlanner-') + GetSHA256OfFile(Existing) + '.exe';
  if not FileExists(Backup) then
    if not FileCopy(Existing, Backup, True) then RaiseException('Could not save the previous app version.');
  if GetSHA256OfFile(Backup) <> GetSHA256OfFile(Existing) then RaiseException('App backup verification failed.');
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then begin
    if WizardIsTaskSelected('atsplugin') then InstallPlugin('ATS', ExpandConstant('{app}\plugins\ats\GT_Telemetry.dll'), GameFolders.Values[0]);
    if WizardIsTaskSelected('etsplugin') then InstallPlugin('ETS2', ExpandConstant('{app}\plugins\ets2\GT_Telemetry.dll'), GameFolders.Values[1]);
    if WizardIsTaskSelected('atsmod') then InstallTrailerMod('ATS', ExpandConstant('{app}\mods\GT_Tanker_Combinations_ATS.scs'), ModFolders.Values[0]);
    if WizardIsTaskSelected('etsmod') then InstallTrailerMod('ETS2', ExpandConstant('{app}\mods\GT_Tanker_Combinations_ETS2.scs'), ModFolders.Values[1]);
  end;
end;

function InitializeUninstall: Boolean;
begin
  Result := False;
  try
    if ProcessRunning('DispatchPlanner.exe') then begin SuppressibleMsgBox('Close Dispatch Companion before uninstalling.', mbError, MB_OK, IDOK); Exit; end;
    if FileExists(PluginStateFile) then
      if ProcessRunning('amtrucks.exe') or ProcessRunning('eurotrucks2.exe') then begin SuppressibleMsgBox('Close both games normally before restoring telemetry DLLs.', mbError, MB_OK, IDOK); Exit; end;
    Result := True;
  except
    SuppressibleMsgBox('Could not verify that the game and app are closed. Uninstall was stopped.', mbError, MB_OK, IDOK);
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then begin RestorePlugin('ATS'); RestorePlugin('ETS2'); end;
end;
