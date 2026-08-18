; Fast Viewer 1.0 RC1 — Inno Setup installer script.
; Per-user install: no administrator rights required.
; Registers Fast Viewer for "Open with" and as an available default app for
; supported image types (jpg/jpeg/png/bmp/tif/tiff). It never forces a
; default-app change; Windows 10 default-app selection remains user-confirmed.
;
; Build (release-only tool, not part of the viewer runtime):
;   ISCC.exe FastViewer.iss
;
; Version is kept in sync with CMakeLists.txt / src/version.h.in (1.0.0-rc1).

#define MyAppName "Fast Viewer"
#define MyAppVersion "1.0.0-rc1"
#define MyAppExe "fast_viewer.exe"
#define MyAppDir "C:\DSWorkspace\fast-viewer\build"

[Setup]
AppId={{C3F7A8B2-6D41-4E7A-9C5E-2B8A1F0D4E33}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher=
DefaultDirName={localappdata}\Programs\Fast Viewer
DefaultGroupName=Fast Viewer
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=commandline
OutputDir=..\release
OutputBaseFilename=FastViewer-1.0.0-rc1-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
UninstallDisplayIcon={app}\{#MyAppExe}
UninstallDisplayName={#MyAppName}
SetupIconFile=C:\DSWorkspace\fast-viewer\src\resources\fast_viewer.ico
VersionInfoVersion=1.0.0.0
VersionInfoProductVersion=1.0.0.0
VersionInfoDescription=Fast Viewer
VersionInfoProductName=Fast Viewer
MinVersion=10.0.17763

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#MyAppDir}\{#MyAppExe}"; DestDir: "{app}"; Flags: ignoreversion

[Registry]
; --- Application identity / progid (per-user) ---
Root: HKCU; Subkey: "Software\Classes\FastViewer"; ValueType: string; ValueName: ""; ValueData: "Fast Viewer Image"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\FastViewer\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExe},0"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\FastViewer\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExe}"" ""%1"""; Flags: uninsdeletekey

; --- "Open with" per-extension hooks (additive; never replaces defaults) ---
Root: HKCU; Subkey: "Software\Classes\.jpg\OpenWithProgids"; ValueType: string; ValueName: "FastViewer"; ValueData: ""; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\.jpeg\OpenWithProgids"; ValueType: string; ValueName: "FastViewer"; ValueData: ""; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\.png\OpenWithProgids"; ValueType: string; ValueName: "FastViewer"; ValueData: ""; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\.bmp\OpenWithProgids"; ValueType: string; ValueName: "FastViewer"; ValueData: ""; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\.tif\OpenWithProgids"; ValueType: string; ValueName: "FastViewer"; ValueData: ""; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\.tiff\OpenWithProgids"; ValueType: string; ValueName: "FastViewer"; ValueData: ""; Flags: uninsdeletevalue

; --- Application registration for Explorer "Open with" list ---
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExe}"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExe}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExe}"" ""%1"""; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExe}\SupportedTypes"; ValueType: string; ValueName: ".jpg"; ValueData: ""; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExe}\SupportedTypes"; ValueType: string; ValueName: ".jpeg"; ValueData: ""; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExe}\SupportedTypes"; ValueType: string; ValueName: ".png"; ValueData: ""; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExe}\SupportedTypes"; ValueType: string; ValueName: ".bmp"; ValueData: ""; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExe}\SupportedTypes"; ValueType: string; ValueName: ".tif"; ValueData: ""; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExe}\SupportedTypes"; ValueType: string; ValueName: ".tiff"; ValueData: ""; Flags: uninsdeletekey

; --- Capabilities (lets the user pick Fast Viewer in Settings/Default apps) ---
Root: HKCU; Subkey: "Software\FastViewer"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\FastViewer\Capabilities"; ValueType: string; ValueName: "ApplicationName"; ValueData: "Fast Viewer"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\FastViewer\Capabilities"; ValueType: string; ValueName: "ApplicationDescription"; ValueData: "Fast Viewer - tiny fast native Windows image viewer"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\FastViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".jpg"; ValueData: "FastViewer"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\FastViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".jpeg"; ValueData: "FastViewer"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\FastViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".png"; ValueData: "FastViewer"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\FastViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".bmp"; ValueData: "FastViewer"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\FastViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".tif"; ValueData: "FastViewer"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\FastViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".tiff"; ValueData: "FastViewer"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\RegisteredApplications"; ValueType: string; ValueName: "Fast Viewer"; ValueData: "Software\FastViewer\Capabilities"; Flags: uninsdeletevalue

[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Run]
Filename: "{app}\{#MyAppExe}"; Description: "Run Fast Viewer"; Flags: nowait postinstall skipifsilent
