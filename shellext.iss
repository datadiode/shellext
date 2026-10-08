[Deco]
(*//

[Setup]
AppName=shellext
AppVersion=1.0.0.8
AppId={{8545BEA4-83DF-4167-B67D-5FF71162F6D4}
ShowLanguageDialog=no
LanguageDetectionMethod=none
AppPublisher=datadiode
AppPublisherURL=https://github.com/datadiode/shellext
RestartApplications=True
CloseApplicationsFilter=explorer.exe
VersionInfoVersion=1.0.0.8
VersionInfoCompany=datadiode
OutputBaseFilename=shellext-setup
ArchitecturesInstallIn64BitMode=x64
DefaultDirName={commonpf}\datadiode\shellext

[Files]
Source: "shellext.png"; DestDir: "{app}"; Flags: ignoreversion
Source: "shellext.ini"; DestDir: "{app}"; Flags: ignoreversion
Source: "bld\MinSizeRel\shellext.dll"; DestDir: "{app}"; Flags: ignoreversion 64bit restartreplace regserver

[Code]
//*)
function PrepareToInstall(var NeedsRestart: Boolean): string;
begin
  RenameFile(ExpandConstant('{app}\shellext.dll'), ExpandConstant('{tmp}\shellext.dll.old'))
end;
