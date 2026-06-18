; ============================================================================
;  AtiNAMatiC-Gate -- Inno Setup script (VST3 + CLAP only)
;  Atij 666 Studio
;
;  Workflow: run build.ps1 (Release) -> populates Installer\Bundle\
;            then: ISCC.exe AtiNAMatiC-Gate.iss
; ============================================================================

#define MyAppName        "AtiNAMatiC-Gate"
#define MyAppVersion     "0.1.0"
#define MyAppPublisher   "Atij 666 Studio"
#define MyAppURL         "https://atij666studio.example"
#define MyClapName       "AtiNAMatiC-Gate.clap"
#define MyManualName     "AtiNAMatiC-Gate-Manual.md"
#define ProjectRoot      ".."
#define Bundle           ProjectRoot + "\Installer\Bundle"

[Setup]
AppId={{B1BEF6FC-399E-4CA7-A7E0-450D32048688}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
VersionInfoVersion={#MyAppVersion}
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription={#MyAppName} Installer
VersionInfoProductName={#MyAppName}

DefaultDirName={autopf}\{#MyAppPublisher}\{#MyAppName}
DefaultGroupName={#MyAppPublisher}\{#MyAppName}
DisableProgramGroupPage=yes
DisableDirPage=auto
DisableReadyPage=no
DisableWelcomePage=no
DisableFinishedPage=no
ShowLanguageDialog=no
AllowNoIcons=yes

PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible

OutputDir={#ProjectRoot}\Installer\Output
OutputBaseFilename={#MyAppName}-Setup-{#MyAppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
LZMAUseSeparateProcess=yes
InternalCompressLevel=ultra
WizardStyle=modern
WizardSizePercent=110
SetupIconFile={#ProjectRoot}\Resources\Gate.ico
UninstallDisplayName={#MyAppName} {#MyAppVersion}
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full";   Description: "Full installation (VST3 + CLAP)"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in (for your DAW)"; Types: full custom; Flags: checkablealone
Name: "clap"; Description: "CLAP plug-in (for your DAW)"; Types: full custom; Flags: checkablealone

[Files]
; ---- VST3 (whole bundle, recurse) -----------------------------------------
Source: "{#Bundle}\AtiNAMatiC-Gate.vst3\*"; \
    DestDir: "{commoncf64}\VST3\AtiNAMatiC-Gate.vst3"; \
    Flags: ignoreversion recursesubdirs createallsubdirs; \
    Components: vst3

; ---- CLAP ------------------------------------------------------------------
Source: "{#Bundle}\{#MyClapName}"; \
    DestDir: "{commoncf64}\CLAP"; \
    Flags: ignoreversion; Components: clap

; ---- Manual (placed in {app}) ---------------------------------------------
Source: "{#ProjectRoot}\docs\{#MyManualName}"; \
    DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{autoprograms}\{#MyAppPublisher}\{#MyAppName}\User Manual"; \
    Filename: "{app}\{#MyManualName}"; \
    Check: FileExists(ExpandConstant('{app}\{#MyManualName}'))

Name: "{autoprograms}\{#MyAppPublisher}\{#MyAppName}\Uninstall {#MyAppName}"; \
    Filename: "{uninstallexe}"

[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Code]
function InitializeSetup(): Boolean;
begin
  Result := True;
  if not IsWin64 then
  begin
    MsgBox('{#MyAppName} requires 64-bit Windows.', mbCriticalError, MB_OK);
    Result := False;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  msg: String;
begin
  if CurStep = ssDone then
  begin
    msg := '{#MyAppName} has been installed.' + #13#10 + #13#10 +
           'Please rescan plug-ins in your DAW so it appears in your library:' + #13#10 +
           '  Reaper:      Options > Preferences > VST > Re-scan' + #13#10 +
           '  Cubase:      Studio > VST Plug-in Manager > Update' + #13#10 +
           '  Studio One:  Options > Locations > Plug-ins > Rescan' + #13#10 +
           '  Ableton:     Preferences > Plug-Ins > Rescan' + #13#10 +
           '  Bitwig:      Settings > Plug-ins > Rescan';
    MsgBox(msg, mbInformation, MB_OK);
  end;
end;
