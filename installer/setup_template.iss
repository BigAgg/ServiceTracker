#define Name "@PROJECT_NAME@"
#define Version "@PROJECT_VERSION@"
#define InstallDir "{commonpf}\@PROJECT_NAME@"


[Setup]
; Setup options
AppName={#Name}
AppVersion={#Version}
DefaultDirName={#InstallDir}
DisableDirPage=no
DefaultGroupName={#Name}
OutputDir=..\output
OutputBaseFilename=install_{#Name}
Compression=lzma
SolidCompression=yes
LicenseFile=..\LICENSE
SetupIconFile=..\res\icon.ico

[FILES]
; Programm (Build mit: cmake --preset windows && cmake --build --preset windows-release)
Source: "..\build\windows\app\Release\{#Name}.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\windows\app\Release\updater.exe"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
; Ressourcen (Schrift, Icons, Anleitung)
Source: "..\res\*"; DestDir: "{app}\"; Flags: ignoreversion recursesubdirs createallsubdirs
; Standard-Fensterlayout: nur bei Erstinstallation, eigene Layouts der Benutzer bleiben erhalten
Source: "..\deploy\imgui.ini"; DestDir: "{app}"; Flags: onlyifdoesntexist
Source: "..\deploy\windowcontrol.ini"; DestDir: "{app}"; Flags: onlyifdoesntexist
; Optionale Konfiguration (Pfade). Wird nie überschrieben.
Source: "..\deploy\ServiceTracker.json"; DestDir: "{app}"; Flags: onlyifdoesntexist skipifsourcedoesntexist

[Dirs]
; Setting folder permissions to be editable
Name: "{app}"; Permissions: users-modify; Flags: uninsalwaysuninstall

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional icons:";

[Icons]
Name: "{group}\{#Name}"; Filename: "{app}\{#Name}.exe"; IconFilename: "{app}\icon.ico";
Name: "{autodesktop}\{#Name}"; Filename: "{app}\{#Name}.exe"; Tasks: desktopicon; IconFilename: "{app}\icon.ico";

[Registry]
; Optional registry entries (e.g., add app to start Menu or uninstall registry)
; Root: HKCU; Subkey: "Software\{#Name}"; ValueType: string; ValueName: "InstallDir"; ValueData: "{app}"

[Run]
; Optional: Auto-run the app after installation
Filename: "{app}\{#Name}.exe"; Description: "{cm:LaunchProgram,{#Name}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Uninstall all files and dirs
Type: filesandordirs; Name: "{app}"
; Remove registry keys

[Code]

function FindUninstallString(AppName: string; var UninstallString: string): Boolean;
var
  Names: TArrayOfString;
  I: Integer;
  Key: string;
  DisplayName: string;
begin
  Result := False;

  // Check 64-bit + 32-bit uninstall locations
  if RegGetSubkeyNames(HKLM, 'Software\Microsoft\Windows\CurrentVersion\Uninstall', Names) or
     RegGetSubkeyNames(HKLM, 'Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall', Names) then
  begin
    for I := 0 to GetArrayLength(Names) - 1 do
    begin
      Key := 'Software\Microsoft\Windows\CurrentVersion\Uninstall\' + Names[I];
      if not RegQueryStringValue(HKLM, Key, 'DisplayName', DisplayName) then
        Key := 'Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\' + Names[I];

      if RegQueryStringValue(HKLM, Key, 'DisplayName', DisplayName) then
      begin
        if Pos(AppName, DisplayName) > 0 then
        begin
          if RegQueryStringValue(HKLM, Key, 'UninstallString', UninstallString) then
          begin
            Result := True;
            exit;
          end;
        end;
      end;
    end;
  end;
end;


function UninstallOldServiceCreator(): Boolean;
var
  UninstallString: string;
  ResultCode: Integer;
begin
  Result := True;

  if not FindUninstallString('ServiceCreator', UninstallString) then
    exit;

  // IMPORTANT: handle quoted exe properly
  if Exec(RemoveQuotes(UninstallString), '',
          '', SW_SHOW, ewWaitUntilTerminated, ResultCode) then
  begin
    if ResultCode <> 0 then
      MsgBox('Uninstall returned code ' + IntToStr(ResultCode), mbError, MB_OK);
  end
  else
  begin
    MsgBox('Failed to start uninstaller', mbError, MB_OK);
  end;
end;


function InitializeSetup(): Boolean;
begin
  UninstallOldServiceCreator();
  Result := True;
end;