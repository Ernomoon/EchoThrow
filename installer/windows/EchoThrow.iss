; Inno Setup script. Values AppVersion and BundleDir are passed by the CI (ISCC /D...).
#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef BundleDir
  #define BundleDir "..\..\build\EchoThrow_artefacts\Release\VST3\EchoThrow.vst3"
#endif

[Setup]
AppId={{6B1F3C52-8E0A-4D0B-9C6E-3F2A7D15B9E4}
AppName=EchoThrow
AppVersion={#AppVersion}
AppPublisher=EchoThrow Audio
DefaultDirName={commoncf64}\VST3\EchoThrow.vst3
DisableDirPage=yes
DisableProgramGroupPage=yes
DefaultGroupName=EchoThrow
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=Output
OutputBaseFilename=EchoThrow-Setup-Windows
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=EchoThrow (VST3)

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"

[Files]
Source: "{#BundleDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{app}"
