; Instalador de Ejercicios de trazos y Cartuchera (HU-70). Lo genera scripts/package.ps1, que
; pasa AppVersion, SourceDir (build/package) y OutputDir (dist).
; Por usuario y sin administrador. Los datos (%LOCALAPPDATA%\trazos: calibración, medios.json,
; config.json) no son del instalador: quedan al desinstalar y al actualizar.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

[Setup]
AppId={{6C1F2E8A-5B7D-4E39-9A0C-3D8B71F4A2E6}
AppName=Trazos
AppVersion={#AppVersion}
AppVerName=Trazos {#AppVersion}
AppPublisher=Pablo Daniel Britos
AppPublisherURL=https://github.com/elaqueo/ejercicios-trazos
DefaultDirName={localappdata}\Programs\Trazos
DefaultGroupName=Trazos
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutputDir}
OutputBaseFilename=TrazosSetup-{#AppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
SetupIconFile=..\assets\icons\instalador.ico
LicenseFile={#SourceDir}\LICENSE.txt
UninstallDisplayName=Trazos {#AppVersion}
UninstallDisplayIcon={app}\ejercicios.exe
CloseApplications=yes

[Languages]
Name: "es"; MessagesFile: "compiler:Languages\Spanish.isl"

[Tasks]
Name: "desktopicon"; Description: "Crear accesos en el escritorio"; GroupDescription: "Accesos directos:"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Ejercicios de trazos"; Filename: "{app}\ejercicios.exe"; WorkingDir: "{app}"
Name: "{group}\Cartuchera"; Filename: "{app}\cartuchera.exe"; WorkingDir: "{app}"
Name: "{group}\Desinstalar Trazos"; Filename: "{uninstallexe}"
Name: "{userdesktop}\Ejercicios de trazos"; Filename: "{app}\ejercicios.exe"; WorkingDir: "{app}"; Tasks: desktopicon
Name: "{userdesktop}\Cartuchera"; Filename: "{app}\cartuchera.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\ejercicios.exe"; Description: "Abrir Ejercicios de trazos"; Flags: nowait postinstall skipifsilent unchecked
