#define AppName "ClassMngr"
#define AppVersion GetEnv("CLASSMNGR_APP_VERSION")
#if AppVersion == ""
  #define AppVersion "0.5.0"
#endif
#define SourceDir GetEnv("CLASSMNGR_INSTALLER_SOURCE_DIR")
#if SourceDir == ""
  #define SourceDir "..\..\dist\ClassMngr-windows-x64"
#endif
#define OutputDir GetEnv("CLASSMNGR_INSTALLER_OUTPUT_DIR")
#if OutputDir == ""
  #define OutputDir "..\..\dist"
#endif

[Setup]
AppId={{73790AE0-7C8F-4BA7-B34D-E9A3787D321A}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=ClassMngr
DefaultDirName={localappdata}\Programs\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=ClassMngrSetup-{#AppVersion}-x64
SetupIconFile=..\..\resources\assets\icons\app_icon.ico
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\ClassMngr.exe"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\ClassMngr.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\ClassMngr.exe"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent
