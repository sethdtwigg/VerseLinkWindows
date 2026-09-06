; VerseLink installer (Inno Setup 6)
;
; Compile through packaging\build-installer.ps1, which supplies AppVersion from
; VerseLinkWindows\Version.h so the installer and the exe can never disagree.
;
; Upgrades: AppId below is the identity Windows and Inno use to recognise an
; existing installation. Because it never changes between releases, installing a
; newer build upgrades the existing one in place - old files are replaced, the
; Add/Remove Programs entry is updated rather than duplicated, and the previous
; uninstaller is superseded. Changing AppId would strand every copy already
; installed as a separate product, so do not.

#define AppName        "VerseLink"
#define AppExeName     "VerseLinkWindows.exe"
#define AppPublisher   "sethdtwigg"
#define AppURL         "https://github.com/sethdtwigg/VerseLinkWindows"

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

; Per-user install: no UAC prompt, and - the reason it matters - the install
; directory stays writable. VerseLink runs AsInvoker; under Program Files it
; could not write next to itself at all.
PrivilegesRequired=lowest
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes

; The app holds this mutex while running. A tray app keeps its own exe locked,
; so without this an upgrade started while VerseLink is running would fail part
; way through; instead Setup asks the user to close it first.
AppMutex=VerseLinkWindows.SingleInstance
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
; overwrite what an upgrading user actually had.

[Icons]
Name: "{group}\{#AppName}";           Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
Name: "{userstartup}\{#AppName}";     Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"; Tasks: startup
Name: "{autodesktop}\{#AppName}";     Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "Launch {#AppName}"; WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; The log is generated at runtime, so it is not tracked as an installed file.
Type: files; Name: "{userappdata}\{#AppName}\verselink.log"

[Code]
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
