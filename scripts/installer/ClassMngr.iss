#define AppName "ClassMngr"
#define AppPublisher "PaperCloud"

#define AppVersion GetEnv("CLASSMNGR_APP_VERSION")
#if AppVersion == ""
  #error CLASSMNGR_APP_VERSION must be set by the release build.
#endif

#define AppArch GetEnv("CLASSMNGR_INSTALLER_ARCH")
#if AppArch == ""
  #define AppArch "x64"
#endif

#if AppArch == "x64"
  #define AllowedArchitecture "x64compatible"
  #define VCRedistFileName "vc_redist.x64.exe"
  #define VCRedistRegistryArch "x64"
#elif AppArch == "arm64"
  #define AllowedArchitecture "arm64"
  #define VCRedistFileName "vc_redist.arm64.exe"
  #define VCRedistRegistryArch "arm64"
#else
  #error CLASSMNGR_INSTALLER_ARCH must be either x64 or arm64.
#endif

#define SourceDir GetEnv("CLASSMNGR_INSTALLER_SOURCE_DIR")
#if SourceDir == ""
  #define SourceDir "..\..\dist\ClassMngr-windows-" + AppArch
#endif

#define OutputDir GetEnv("CLASSMNGR_INSTALLER_OUTPUT_DIR")
#if OutputDir == ""
  #define OutputDir "..\..\dist"
#endif

#define SignToolName GetEnv("CLASSMNGR_INSTALLER_SIGN_TOOL")

[Setup]
AppId={{73790AE0-7C8F-4BA7-B34D-E9A3787D321A}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={localappdata}\Programs\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=ClassMngr-{#AppVersion}-win-{#AppArch}
SetupIconFile=..\..\resources\assets\icons\app_icon.ico
UninstallDisplayIcon={app}\ClassMngr.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
MinVersion=10.0.17763
ArchitecturesAllowed={#AllowedArchitecture}
ArchitecturesInstallIn64BitMode={#AllowedArchitecture}
CloseApplications=yes
CloseApplicationsFilter=ClassMngr.exe
RestartApplications=no
ChangesAssociations=yes
VersionInfoCompany={#AppPublisher}
VersionInfoDescription={#AppName} Setup
VersionInfoProductName={#AppName}
VersionInfoProductVersion={#AppVersion}
VersionInfoVersion={#AppVersion}
#if SignToolName != ""
SignTool={#SignToolName}
SignedUninstaller=yes
#endif

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "korean"; MessagesFile: "compiler:Languages\Korean.isl"

[CustomMessages]
english.AssociateTps=Associate .tps files with ClassMngr
korean.AssociateTps=.tps 파일을 ClassMngr와 연결
english.FileAssociations=File associations:
korean.FileAssociations=파일 연결:
english.CreateStartMenuEntry=Create a Start Menu entry
korean.CreateStartMenuEntry=시작 메뉴 항목 만들기

[Tasks]
Name: "startmenuentry"; Description: "{cm:CreateStartMenuEntry}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "associatetps"; Description: "{cm:AssociateTps}"; GroupDescription: "{cm:FileAssociations}"

[InstallDelete]
Type: filesandordirs; Name: "{app}\plugins"
Type: filesandordirs; Name: "{app}\qml"
Type: filesandordirs; Name: "{app}\translations"
Type: files; Name: "{app}\Qt6*.dll"
Type: files; Name: "{app}\D3Dcompiler_47.dll"
Type: files; Name: "{app}\dxcompiler.dll"
Type: files; Name: "{app}\dxil.dll"
Type: files; Name: "{app}\opengl32sw.dll"
Type: files; Name: "{app}\qt.conf"
Type: files; Name: "{app}\vc_redist.*.exe"

[Files]
#if SignToolName != ""
Source: "{#SourceDir}\ClassMngr.exe"; DestDir: "{app}"; Flags: ignoreversion signonce
#else
Source: "{#SourceDir}\ClassMngr.exe"; DestDir: "{app}"; Flags: ignoreversion
#endif
Source: "{#SourceDir}\*"; DestDir: "{app}"; Excludes: "ClassMngr.exe,{#VCRedistFileName}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\{#VCRedistFileName}"; DestDir: "{tmp}"; Flags: deleteafterinstall

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\ClassMngr.exe"; Tasks: startmenuentry
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\ClassMngr.exe"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\Classes\.tps"; ValueType: string; ValueName: ""; ValueData: "ClassMngr.Database"; Tasks: associatetps
Root: HKCU; Subkey: "Software\Classes\ClassMngr.Database"; ValueType: string; ValueName: ""; ValueData: "ClassMngr Database"; Tasks: associatetps
Root: HKCU; Subkey: "Software\Classes\ClassMngr.Database\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: """{app}\ClassMngr.exe"",0"; Tasks: associatetps
Root: HKCU; Subkey: "Software\Classes\ClassMngr.Database\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\ClassMngr.exe"" ""%1"""; Tasks: associatetps

[Run]
Filename: "{tmp}\{#VCRedistFileName}"; Parameters: "/install /quiet /norestart"; StatusMsg: "Installing the Microsoft Visual C++ Runtime..."; Flags: waituntilterminated; Check: VCRedistNeedsInstall
Filename: "{app}\ClassMngr.exe"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

[Code]
procedure RemoveTpsAssociation;
var
  CurrentProgId: String;
begin
  if RegQueryStringValue(
    HKCU,
    'Software\Classes\.tps',
    '',
    CurrentProgId) and
    (CompareText(CurrentProgId, 'ClassMngr.Database') = 0) then
  begin
    RegDeleteValue(HKCU, 'Software\Classes\.tps', '');
  end;

  RegDeleteKeyIncludingSubkeys(
    HKCU,
    'Software\Classes\ClassMngr.Database');
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssInstall) and
    not WizardIsTaskSelected('associatetps') then
  begin
    RemoveTpsAssociation;
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
  begin
    RemoveTpsAssociation;
  end;
end;

function VCRedistNeedsInstall: Boolean;
var
  Installed: Cardinal;
  Major: Cardinal;
  Minor: Cardinal;
  Build: Cardinal;
  Revision: Cardinal;
  InstalledMS: Cardinal;
  InstalledLS: Cardinal;
  RequiredMS: Cardinal;
  RequiredLS: Cardinal;
  RuntimeKey: String;
begin
  Result := True;
  RuntimeKey := 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\{#VCRedistRegistryArch}';

  if not RegQueryDWordValue(HKLM64, RuntimeKey, 'Installed', Installed) or
     (Installed <> 1) or
     not RegQueryDWordValue(HKLM64, RuntimeKey, 'Major', Major) or
     not RegQueryDWordValue(HKLM64, RuntimeKey, 'Minor', Minor) or
     not RegQueryDWordValue(HKLM64, RuntimeKey, 'Bld', Build) or
     not RegQueryDWordValue(HKLM64, RuntimeKey, 'Rbld', Revision) then
  begin
    Exit;
  end;

  if not GetVersionNumbers(
    ExpandConstant('{tmp}\{#VCRedistFileName}'),
    RequiredMS,
    RequiredLS) then
  begin
    Exit;
  end;

  InstalledMS := (Major * 65536) + Minor;
  InstalledLS := (Build * 65536) + Revision;
  Result := (InstalledMS < RequiredMS) or
    ((InstalledMS = RequiredMS) and (InstalledLS < RequiredLS));
end;
