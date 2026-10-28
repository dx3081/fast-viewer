; Fast Viewer 1.0 RC1 - Inno Setup installer script.
; Per-user install: no administrator rights required.
; Registers Fast Viewer for "Open with" / default-app availability for the
; image formats the user selects on the File Associations page (default: all).
; It never forces a default-app change; Windows default-app selection remains
; user-confirmed.
;
; Build (release-only tool, not part of the viewer runtime):
;   ISCC.exe FastViewer.iss
;
; Version is kept in sync with CMakeLists.txt / src/version.h.in (1.1.0-beta1).

#define MyAppName "Fast Viewer"
#define MyAppVersion "1.1.0-beta1"
#define MyAppExe "fast_viewer.exe"
; All machine-specific inputs are overridable ISPP defines:
;   /DMyAppBuildDir=<path>  (default: repo\build relative to this script)
;   /DMyIcon=<path>         (default: repo\src\resources\fast_viewer.ico)
;   /DMyOutputDir=<path>    (default: repo\release)
;   /DMyLicense=<path>      (default: repo\LICENSE, GPL-3.0-only)
#ifndef MyAppBuildDir
#define MyAppBuildDir "..\build"
#endif
#ifndef MyIcon
#define MyIcon "..\src\resources\fast_viewer.ico"
#endif
#ifndef MyOutputDir
#define MyOutputDir "..\release"
#endif
#ifndef MyLicense
#define MyLicense "..\LICENSE"
#endif

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
OutputDir={#MyOutputDir}
OutputBaseFilename=FastViewer-1.1.0-beta1-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
UninstallDisplayIcon={app}\{#MyAppExe}
UninstallDisplayName={#MyAppName}
SetupIconFile={#MyIcon}
LicenseFile={#MyLicense}
VersionInfoVersion=1.1.0.0
VersionInfoProductVersion=1.1.0.0
VersionInfoDescription=Fast Viewer
VersionInfoProductName=Fast Viewer
MinVersion=10.0.17763

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#MyAppBuildDir}\{#MyAppExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyLicense}"; DestDir: "{app}"; Flags: ignoreversion

[Registry]
; --- Application identity / progid (per-user) ---
Root: HKCU; Subkey: "Software\Classes\FastViewer"; ValueType: string; ValueName: ""; ValueData: "Fast Viewer Image"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\FastViewer\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExe}"",0"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\FastViewer\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExe}"" ""%1"""; Flags: uninsdeletekey

; --- "Open with" per-extension hooks (additive; never replaces defaults) ---
; Each extension is only registered when the user keeps it checked.
Root: HKCU; Subkey: "Software\Classes\.jpg\OpenWithProgids"; ValueType: string; ValueName: "FastViewer"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.jpg')
Root: HKCU; Subkey: "Software\Classes\.jpeg\OpenWithProgids"; ValueType: string; ValueName: "FastViewer"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.jpeg')
Root: HKCU; Subkey: "Software\Classes\.png\OpenWithProgids"; ValueType: string; ValueName: "FastViewer"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.png')
Root: HKCU; Subkey: "Software\Classes\.bmp\OpenWithProgids"; ValueType: string; ValueName: "FastViewer"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.bmp')
Root: HKCU; Subkey: "Software\Classes\.tif\OpenWithProgids"; ValueType: string; ValueName: "FastViewer"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.tif')
Root: HKCU; Subkey: "Software\Classes\.tiff\OpenWithProgids"; ValueType: string; ValueName: "FastViewer"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.tiff')
Root: HKCU; Subkey: "Software\Classes\.webp\OpenWithProgids"; ValueType: string; ValueName: "FastViewer"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.webp')
Root: HKCU; Subkey: "Software\Classes\.gif\OpenWithProgids"; ValueType: string; ValueName: "FastViewer"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.gif')
Root: HKCU; Subkey: "Software\Classes\.heic\OpenWithProgids"; ValueType: string; ValueName: "FastViewer"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.heic')
Root: HKCU; Subkey: "Software\Classes\.heif\OpenWithProgids"; ValueType: string; ValueName: "FastViewer"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.heif')
Root: HKCU; Subkey: "Software\Classes\.avif\OpenWithProgids"; ValueType: string; ValueName: "FastViewer"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.avif')
Root: HKCU; Subkey: "Software\Classes\.dng\OpenWithProgids"; ValueType: string; ValueName: "FastViewer"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.dng')
Root: HKCU; Subkey: "Software\Classes\.cr2\OpenWithProgids"; ValueType: string; ValueName: "FastViewer"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.cr2')
Root: HKCU; Subkey: "Software\Classes\.cr3\OpenWithProgids"; ValueType: string; ValueName: "FastViewer"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.cr3')
Root: HKCU; Subkey: "Software\Classes\.nef\OpenWithProgids"; ValueType: string; ValueName: "FastViewer"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.nef')
Root: HKCU; Subkey: "Software\Classes\.arw\OpenWithProgids"; ValueType: string; ValueName: "FastViewer"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.arw')
Root: HKCU; Subkey: "Software\Classes\.orf\OpenWithProgids"; ValueType: string; ValueName: "FastViewer"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.orf')
Root: HKCU; Subkey: "Software\Classes\.rw2\OpenWithProgids"; ValueType: string; ValueName: "FastViewer"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.rw2')
Root: HKCU; Subkey: "Software\Classes\.raf\OpenWithProgids"; ValueType: string; ValueName: "FastViewer"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.raf')

; --- Application registration for Explorer "Open with" list ---
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExe}"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExe}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExe}"" ""%1"""; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExe}\SupportedTypes"; ValueType: string; ValueName: ".jpg"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.jpg')
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExe}\SupportedTypes"; ValueType: string; ValueName: ".jpeg"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.jpeg')
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExe}\SupportedTypes"; ValueType: string; ValueName: ".png"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.png')
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExe}\SupportedTypes"; ValueType: string; ValueName: ".bmp"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.bmp')
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExe}\SupportedTypes"; ValueType: string; ValueName: ".tif"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.tif')
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExe}\SupportedTypes"; ValueType: string; ValueName: ".tiff"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.tiff')
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExe}\SupportedTypes"; ValueType: string; ValueName: ".webp"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.webp')
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExe}\SupportedTypes"; ValueType: string; ValueName: ".gif"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.gif')
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExe}\SupportedTypes"; ValueType: string; ValueName: ".heic"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.heic')
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExe}\SupportedTypes"; ValueType: string; ValueName: ".heif"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.heif')
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExe}\SupportedTypes"; ValueType: string; ValueName: ".avif"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.avif')
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExe}\SupportedTypes"; ValueType: string; ValueName: ".dng"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.dng')
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExe}\SupportedTypes"; ValueType: string; ValueName: ".cr2"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.cr2')
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExe}\SupportedTypes"; ValueType: string; ValueName: ".cr3"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.cr3')
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExe}\SupportedTypes"; ValueType: string; ValueName: ".nef"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.nef')
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExe}\SupportedTypes"; ValueType: string; ValueName: ".arw"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.arw')
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExe}\SupportedTypes"; ValueType: string; ValueName: ".orf"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.orf')
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExe}\SupportedTypes"; ValueType: string; ValueName: ".rw2"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.rw2')
Root: HKCU; Subkey: "Software\Classes\Applications\{#MyAppExe}\SupportedTypes"; ValueType: string; ValueName: ".raf"; ValueData: ""; Flags: uninsdeletevalue; Check: IsFormatChecked('.raf')

; --- Capabilities (lets the user pick Fast Viewer in Settings/Default apps) ---
Root: HKCU; Subkey: "Software\FastViewer"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\FastViewer\Capabilities"; ValueType: string; ValueName: "ApplicationName"; ValueData: "Fast Viewer"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\FastViewer\Capabilities"; ValueType: string; ValueName: "ApplicationDescription"; ValueData: "Fast Viewer - tiny fast native Windows image viewer"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\FastViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".jpg"; ValueData: "FastViewer"; Flags: uninsdeletevalue; Check: IsFormatChecked('.jpg')
Root: HKCU; Subkey: "Software\FastViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".jpeg"; ValueData: "FastViewer"; Flags: uninsdeletevalue; Check: IsFormatChecked('.jpeg')
Root: HKCU; Subkey: "Software\FastViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".png"; ValueData: "FastViewer"; Flags: uninsdeletevalue; Check: IsFormatChecked('.png')
Root: HKCU; Subkey: "Software\FastViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".bmp"; ValueData: "FastViewer"; Flags: uninsdeletevalue; Check: IsFormatChecked('.bmp')
Root: HKCU; Subkey: "Software\FastViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".tif"; ValueData: "FastViewer"; Flags: uninsdeletevalue; Check: IsFormatChecked('.tif')
Root: HKCU; Subkey: "Software\FastViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".tiff"; ValueData: "FastViewer"; Flags: uninsdeletevalue; Check: IsFormatChecked('.tiff')
Root: HKCU; Subkey: "Software\FastViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".webp"; ValueData: "FastViewer"; Flags: uninsdeletevalue; Check: IsFormatChecked('.webp')
Root: HKCU; Subkey: "Software\FastViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".gif"; ValueData: "FastViewer"; Flags: uninsdeletevalue; Check: IsFormatChecked('.gif')
Root: HKCU; Subkey: "Software\FastViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".heic"; ValueData: "FastViewer"; Flags: uninsdeletevalue; Check: IsFormatChecked('.heic')
Root: HKCU; Subkey: "Software\FastViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".heif"; ValueData: "FastViewer"; Flags: uninsdeletevalue; Check: IsFormatChecked('.heif')
Root: HKCU; Subkey: "Software\FastViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".avif"; ValueData: "FastViewer"; Flags: uninsdeletevalue; Check: IsFormatChecked('.avif')
Root: HKCU; Subkey: "Software\FastViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".dng"; ValueData: "FastViewer"; Flags: uninsdeletevalue; Check: IsFormatChecked('.dng')
Root: HKCU; Subkey: "Software\FastViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".cr2"; ValueData: "FastViewer"; Flags: uninsdeletevalue; Check: IsFormatChecked('.cr2')
Root: HKCU; Subkey: "Software\FastViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".cr3"; ValueData: "FastViewer"; Flags: uninsdeletevalue; Check: IsFormatChecked('.cr3')
Root: HKCU; Subkey: "Software\FastViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".nef"; ValueData: "FastViewer"; Flags: uninsdeletevalue; Check: IsFormatChecked('.nef')
Root: HKCU; Subkey: "Software\FastViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".arw"; ValueData: "FastViewer"; Flags: uninsdeletevalue; Check: IsFormatChecked('.arw')
Root: HKCU; Subkey: "Software\FastViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".orf"; ValueData: "FastViewer"; Flags: uninsdeletevalue; Check: IsFormatChecked('.orf')
Root: HKCU; Subkey: "Software\FastViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".rw2"; ValueData: "FastViewer"; Flags: uninsdeletevalue; Check: IsFormatChecked('.rw2')
Root: HKCU; Subkey: "Software\FastViewer\Capabilities\FileAssociations"; ValueType: string; ValueName: ".raf"; ValueData: "FastViewer"; Flags: uninsdeletevalue; Check: IsFormatChecked('.raf')
Root: HKCU; Subkey: "Software\RegisteredApplications"; ValueType: string; ValueName: "Fast Viewer"; ValueData: "Software\FastViewer\Capabilities"; Flags: uninsdeletevalue

[Run]
; Completion-page action: "set Fast Viewer as the default image viewer".
; Opens Windows Default Apps settings where Windows handles the final user
; confirmation. Never launches Fast Viewer itself and never writes UserChoice.
; Checked by default; the user can uncheck it before pressing Finish.
Filename: "ms-settings:defaultapps?registeredAppUser=Fast%20Viewer"; Description: "Open Fast Viewer's default-apps page (pick it for each image type)"; Flags: nowait postinstall shellexec

[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Code]
var
  AssnPage: TWizardPage;
  ChkJpg:  TNewCheckBox;
  ChkPng:  TNewCheckBox;
  ChkWebp: TNewCheckBox;
  ChkBmp:  TNewCheckBox;
  ChkTiff: TNewCheckBox;
  ChkGif:  TNewCheckBox;
  ChkHeic: TNewCheckBox;
  ChkAvif: TNewCheckBox;
  ChkRaw:  TNewCheckBox;

const
  AssnRoot = 'Software\Classes';
  CapRoot  = 'Software\FastViewer\Capabilities\FileAssociations';
  AppRoot  = 'Software\Classes\Applications\fast_viewer.exe\SupportedTypes';

procedure InitializeWizard;
var
  NoteLbl: TNewStaticText;
begin
  AssnPage := CreateCustomPage(wpReady, 'File Associations',
    'Choose the image formats to register with Fast Viewer.');

  ChkJpg := TNewCheckBox.Create(AssnPage);
  ChkJpg.Parent := AssnPage.Surface;
  ChkJpg.Caption := 'JPEG (.jpg, .jpeg)';
  ChkJpg.Left := 24;
  ChkJpg.Top := 16;
  ChkJpg.Width := AssnPage.SurfaceWidth div 2 - 32;
  ChkJpg.Checked := True;

  ChkPng := TNewCheckBox.Create(AssnPage);
  ChkPng.Parent := AssnPage.Surface;
  ChkPng.Caption := 'PNG (.png)';
  ChkPng.Left := 24;
  ChkPng.Top := 42;
  ChkPng.Width := AssnPage.SurfaceWidth div 2 - 32;
  ChkPng.Checked := True;

  ChkWebp := TNewCheckBox.Create(AssnPage);
  ChkWebp.Parent := AssnPage.Surface;
  ChkWebp.Caption := 'WebP (.webp)';
  ChkWebp.Left := 24;
  ChkWebp.Top := 68;
  ChkWebp.Width := AssnPage.SurfaceWidth div 2 - 32;
  ChkWebp.Checked := True;

  ChkBmp := TNewCheckBox.Create(AssnPage);
  ChkBmp.Parent := AssnPage.Surface;
  ChkBmp.Caption := 'BMP (.bmp)';
  ChkBmp.Left := 24;
  ChkBmp.Top := 94;
  ChkBmp.Width := AssnPage.SurfaceWidth div 2 - 32;
  ChkBmp.Checked := True;

  ChkTiff := TNewCheckBox.Create(AssnPage);
  ChkTiff.Parent := AssnPage.Surface;
  ChkTiff.Caption := 'TIFF (.tif, .tiff)';
  ChkTiff.Left := 24;
  ChkTiff.Top := 120;
  ChkTiff.Width := AssnPage.SurfaceWidth div 2 - 32;
  ChkTiff.Checked := True;

  ChkGif := TNewCheckBox.Create(AssnPage);
  ChkGif.Parent := AssnPage.Surface;
  ChkGif.Caption := 'GIF (.gif, animated)';
  ChkGif.Left := AssnPage.SurfaceWidth div 2;
  ChkGif.Top := 16;
  ChkGif.Width := AssnPage.SurfaceWidth div 2 - 24;
  ChkGif.Checked := True;

  ChkHeic := TNewCheckBox.Create(AssnPage);
  ChkHeic.Parent := AssnPage.Surface;
  ChkHeic.Caption := 'HEIC / HEIF (.heic, .heif)';
  ChkHeic.Left := AssnPage.SurfaceWidth div 2;
  ChkHeic.Top := 42;
  ChkHeic.Width := AssnPage.SurfaceWidth div 2 - 24;
  ChkHeic.Checked := True;

  ChkAvif := TNewCheckBox.Create(AssnPage);
  ChkAvif.Parent := AssnPage.Surface;
  ChkAvif.Caption := 'AVIF (.avif)';
  ChkAvif.Left := AssnPage.SurfaceWidth div 2;
  ChkAvif.Top := 68;
  ChkAvif.Width := AssnPage.SurfaceWidth div 2 - 24;
  ChkAvif.Checked := True;

  ChkRaw := TNewCheckBox.Create(AssnPage);
  ChkRaw.Parent := AssnPage.Surface;
  ChkRaw.Caption := 'Camera RAW (.dng .cr2 .cr3 .nef .arw ...)';
  ChkRaw.Left := AssnPage.SurfaceWidth div 2;
  ChkRaw.Top := 94;
  ChkRaw.Width := AssnPage.SurfaceWidth div 2 - 24;
  ChkRaw.Checked := False;

  NoteLbl := TNewStaticText.Create(AssnPage);
  NoteLbl.Parent := AssnPage.Surface;
  NoteLbl.Caption := 'Windows may require you to confirm Fast Viewer as the default app separately.' + #13#10 +
    'HEIC / AVIF / RAW open only when the matching Windows (Microsoft Store) extension is installed.';
  NoteLbl.AutoSize := False;
  NoteLbl.WordWrap := True;
  NoteLbl.Height := 48;
  NoteLbl.Left := 24;
  NoteLbl.Top := 152;
  NoteLbl.Width := AssnPage.SurfaceWidth - 48;
end;

function IsFormatChecked(const Ext: String): Boolean;
begin
  Result := False;
  if (Ext = '.jpg') or (Ext = '.jpeg') then
    Result := ChkJpg.Checked
  else if Ext = '.png' then
    Result := ChkPng.Checked
  else if Ext = '.webp' then
    Result := ChkWebp.Checked
  else if Ext = '.bmp' then
    Result := ChkBmp.Checked
  else if (Ext = '.tif') or (Ext = '.tiff') then
    Result := ChkTiff.Checked
  else if Ext = '.gif' then
    Result := ChkGif.Checked
  else if (Ext = '.heic') or (Ext = '.heif') then
    Result := ChkHeic.Checked
  else if Ext = '.avif' then
    Result := ChkAvif.Checked
  else if (Ext = '.dng') or (Ext = '.cr2') or (Ext = '.cr3') or (Ext = '.nef') or (Ext = '.arw') or (Ext = '.orf') or (Ext = '.rw2') or (Ext = '.raf') then
    Result := ChkRaw.Checked;
end;

// On reinstall, remove Fast Viewer's OWN registrations for formats the user
// unchecks. Only FastViewer values are deleted; unrelated applications and the
// user's default choice are never touched.
procedure DeleteFastViewerReg(const Ext1, Ext2: String);
begin
  RegDeleteValue(HKCU, AssnRoot + '\' + Ext1 + '\OpenWithProgids', 'FastViewer');
  RegDeleteValue(HKCU, AssnRoot + '\' + Ext2 + '\OpenWithProgids', 'FastViewer');
  RegDeleteValue(HKCU, CapRoot, Ext1);
  RegDeleteValue(HKCU, CapRoot, Ext2);
  RegDeleteValue(HKCU, AppRoot, Ext1);
  RegDeleteValue(HKCU, AppRoot, Ext2);
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssInstall then begin
    if not ChkJpg.Checked then DeleteFastViewerReg('.jpg', '.jpeg');
    if not ChkPng.Checked then DeleteFastViewerReg('.png', '.png');
    if not ChkWebp.Checked then DeleteFastViewerReg('.webp', '.webp');
    if not ChkBmp.Checked then DeleteFastViewerReg('.bmp', '.bmp');
    if not ChkTiff.Checked then DeleteFastViewerReg('.tif', '.tiff');
    if not ChkGif.Checked then DeleteFastViewerReg('.gif', '.gif');
    if not ChkHeic.Checked then DeleteFastViewerReg('.heic', '.heif');
    if not ChkAvif.Checked then DeleteFastViewerReg('.avif', '.avif');
    if not ChkRaw.Checked then begin
      DeleteFastViewerReg('.dng', '.cr2');
      DeleteFastViewerReg('.cr3', '.nef');
      DeleteFastViewerReg('.arw', '.orf');
      DeleteFastViewerReg('.rw2', '.raf');
    end;
  end;
end;
