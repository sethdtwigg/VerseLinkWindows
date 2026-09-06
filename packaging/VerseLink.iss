; VerseLink installer (Inno Setup 6)
;
; Compile through packaging\build-installer.ps1, which supplies AppVersion from
; VerseLinkWindows\Version.h so the installer and the exe can never disagree.
;
; Update behaviour, by design:
;   - The install location is chosen by the user (C:\VerseLink, Program Files,
;     anywhere). AppId lets Setup remember and default to where a previous
;     version went, but the directory page is always offered.
;   - An existing version is NOT uninstalled or deleted. The old executable is
;     moved into {app}\previous-versions\VerseLinkWindows-<version>.exe before
;     the new one is copied over, so rolling back is a matter of copying a file.
;   - Settings are never touched. They live in %APPDATA%\VerseLink, which this
;     installer neither writes nor removes, and no config.json is shipped.
;   - If VerseLink is running it is closed before the update and relaunched
;     afterwards, so an update does not leave the user without their hotkey.

#define AppName        "VerseLink"
#define AppExeName     "VerseLinkWindows.exe"
#define AppPublisher   "sethdtwigg"
#define AppURL         "https://github.com/sethdtwigg/VerseLinkWindows"
#define AppMutexName   "VerseLinkWindows.SingleInstance"
; Must match the class registered in VerseLinkWindows.cpp.
#define AppWindowClass "VerseLinkHiddenWindow"
#define BackupDirName  "previous-versions"

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\"
#endif

[Setup]
AppId={{B60AB966-8EA7-43D5-AF1F-4DD311F75EA9}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}/issues
AppUpdatesURL={#AppURL}/releases
VersionInfoVersion={#AppVersion}

; Default to a per-user location so the common case needs no UAC prompt, but
; allow elevating from the first dialog so somewhere like C:\VerseLink or
; Program Files can be chosen. The app itself does not need a writable install
; directory - its settings live in %APPDATA%.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes

; The directory page is the point of this installer - never skip it, and show
; the chosen path again on the confirmation page.
DisableDirPage=no
AlwaysShowDirOnReadyPage=yes
UsePreviousAppDir=yes

; A tray app keeps its own exe locked, so a running VerseLink has to be closed
; before its executable can be replaced.
;
; AppMutex is deliberately NOT used. It only *blocks*: it tells the user to
; close the app and aborts if they do not, which fails outright under /SILENT
; ("Defaulting to Cancel for suppressed message box... Got EAbort exception").
; Instead PrepareToInstall closes VerseLink itself by posting WM_CLOSE to its
; window, which the app handles as a clean shutdown. CloseApplications stays on
; as a Restart Manager backstop for any other locked file, and restarting is
; done in [Code] rather than by RestartApplications so it happens exactly once.
CloseApplications=yes
RestartApplications=no

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

OutputDir={#SourceDir}\dist
OutputBaseFilename=VerseLink-{#AppVersion}-Setup
SetupIconFile={#SourceDir}\VerseLinkWindows\VerseLinkIcon.ico
UninstallDisplayIcon={app}\{#AppExeName}
UninstallDisplayName={#AppName} {#AppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern

; Shown before installing rather than as a licence: it explains that only the
; KJV is bundled and where settings live. No acceptance checkbox.
InfoBeforeFile={#SourceDir}\packaging\DISTRIBUTION-NOTES.txt

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
; A hotkey utility that is not running does nothing, so this is on by default.
Name: "startup"; Description: "Start {#AppName} automatically when I sign in"; GroupDescription: "Additional options:"
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional options:"; Flags: unchecked

[Files]
Source: "{#SourceDir}\VerseLinkWindows\x64\Release\{#AppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\VerseLinkWindows\VerseLinkIcon.ico";          DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\README.md";                                   DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\packaging\DISTRIBUTION-NOTES.txt";            DestDir: "{app}"; Flags: ignoreversion

; Only the KJV is redistributable. NASB, ESV and other modern translations are
; copyrighted and must be supplied by the user into this folder.
Source: "{#SourceDir}\Bibles\KJV.xml"; DestDir: "{app}\Bibles"; Flags: ignoreversion

; config.json is deliberately not installed. Settings live in
; %APPDATA%\VerseLink\config.json, created with defaults on first run; shipping
; one here would be picked up by the app's legacy-config migration and could
; overwrite what an updating user actually had.

[Icons]
Name: "{group}\{#AppName}";           Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
Name: "{userstartup}\{#AppName}";     Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"; Tasks: startup
Name: "{autodesktop}\{#AppName}";     Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
; Only offered when VerseLink was not already running - if it was, [Code]
; relaunches it automatically and this would just start a second copy that
; immediately exits on the mutex.
Filename: "{app}\{#AppExeName}"; Description: "Launch {#AppName}"; WorkingDir: "{app}"; \
    Flags: nowait postinstall skipifsilent; Check: NotRunningBeforeInstall

[UninstallDelete]
; Backups of previous executables are created at runtime, so they are not
; tracked as installed files and would otherwise keep {app} from being removed.
Type: filesandordirs; Name: "{app}\{#BackupDirName}"
; The log is generated at runtime.
Type: files; Name: "{userappdata}\{#AppName}\verselink.log"

[Code]
var
  WasRunning: Boolean;

function InitializeSetup(): Boolean;
begin
  // Recorded before Setup asks the user to close the app, so the decision to
  // relaunch afterwards reflects whether it was actually running to begin with.
  WasRunning := CheckForMutexes('{#AppMutexName}');
  Result := True;
end;

function NotRunningBeforeInstall(): Boolean;
begin
  Result := not WasRunning;
end;

// Closes a running VerseLink the way its own tray Quit does: WM_CLOSE to the
// hidden window, which the app handles by shutting down cleanly (removing its
// tray icon, releasing the hotkey and joining its worker thread). Waits for the
// single-instance mutex to clear so the exe is genuinely unlocked before Setup
// tries to replace it.
function CloseRunningInstance(): Boolean;
var
  Wnd: HWND;
  Waited: Integer;
begin
  Result := True;
  if not CheckForMutexes('{#AppMutexName}') then
    exit;

  Wnd := FindWindowByClassName('{#AppWindowClass}');
  if Wnd <> 0 then
  begin
    Log('Asking the running VerseLink to close');
    PostMessage(Wnd, $0010 { WM_CLOSE }, 0, 0);
  end
  else
    Log('VerseLink appears to be running but its window was not found');

  Waited := 0;
  while (Waited < 15000) and CheckForMutexes('{#AppMutexName}') do
  begin
    Sleep(250);
    Waited := Waited + 250;
  end;

  Result := not CheckForMutexes('{#AppMutexName}');
  if Result then
    Log('Running VerseLink closed after ' + IntToStr(Waited) + ' ms');
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  if not CloseRunningInstance() then
    Result := 'VerseLink is still running and could not be closed automatically.' + #13#10 +
              'Close it from its system tray icon, then run Setup again.';
end;

// Keeps the outgoing executable instead of letting it be overwritten. Named by
// the version it reports, so several updates do not collide and it is obvious
// which build each backup is.
procedure BackupExistingBinary();
var
  ExistingExe, BackupDir, OldVersion, Target: String;
begin
  ExistingExe := ExpandConstant('{app}\{#AppExeName}');
  if not FileExists(ExistingExe) then
    exit;

  BackupDir := ExpandConstant('{app}\{#BackupDirName}');
  if not DirExists(BackupDir) then
  begin
    if not ForceDirectories(BackupDir) then
    begin
      Log('Could not create ' + BackupDir + '; leaving the existing binary in place');
      exit;
    end;
  end;

  if not GetVersionNumbersString(ExistingExe, OldVersion) then
    OldVersion := GetDateTimeString('yyyymmdd-hhnnss', '-', '');

  Target := BackupDir + '\VerseLinkWindows-' + OldVersion + '.exe';

  // Same version installed twice: keep the newer backup rather than failing.
  if FileExists(Target) then
    DeleteFile(Target);

  if RenameFile(ExistingExe, Target) then
    Log('Kept the previous executable as ' + Target)
  else
    Log('Could not move ' + ExistingExe + ' to ' + Target + '; it will be overwritten');
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
begin
  if CurStep = ssInstall then
  begin
    BackupExistingBinary();
  end
  else if CurStep = ssPostInstall then
  begin
    // Put the user back where they were: if VerseLink was running when Setup
    // started, it is running when Setup finishes.
    if WasRunning then
    begin
      if not Exec(ExpandConstant('{app}\{#AppExeName}'), '', ExpandConstant('{app}'),
                  SW_SHOW, ewNoWait, ResultCode) then
        Log('Could not relaunch VerseLink after the update');
    end;
  end;
end;

// Settings are user data, so they are kept by default and only removed if the
// user asks. Silent uninstalls always keep them.
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  SettingsDir: String;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    SettingsDir := ExpandConstant('{userappdata}\{#AppName}');
    if DirExists(SettingsDir) then
    begin
      if not UninstallSilent then
      begin
        if MsgBox('Also remove your VerseLink settings?' + #13#10 + #13#10 +
                  SettingsDir + #13#10 + #13#10 +
                  'Choose No to keep them for a future install.',
                  mbConfirmation, MB_YESNO) = IDYES then
          DelTree(SettingsDir, True, True, True);
      end;
    end;
  end;
end;
