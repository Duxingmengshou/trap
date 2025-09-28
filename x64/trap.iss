[Setup]
AppName=Trap
AppVersion=1.3
DefaultDirName={pf}\Trap
DefaultGroupName=Trap
OutputDir=.
OutputBaseFilename=TrapSetup
Compression=lzma
SolidCompression=yes

[Files]
Source: "Release\trap.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "Release\code.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "Release\trap.ico"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Trap"; Filename: "{app}\Trap.exe"
Name: "{userdesktop}\Trap"; Filename: "{app}\Trap.exe"

[Run]
Filename: "{app}\Trap.exe"; Description: "启动 Trap"; Flags: nowait postinstall skipifsilent

[Tasks]
Name: "autostart"; Description: "开机自动启动"; GroupDescription: "其他选项"

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; \
    ValueType: string; ValueName: "Trap"; \
    ValueData: """{app}\Trap.exe"""; Tasks: autostart

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\Chinese.isl"

