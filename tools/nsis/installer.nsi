Unicode true
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "WinVer.nsh"
!include "x64.nsh"
!define PRODUCT "Tappyweb"
!define VERSION "0.3.1"
!define KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\TappywebCLI"
Name "${PRODUCT} ${VERSION}"
OutFile "${OUTPUT}"
InstallDir "$LOCALAPPDATA\Programs\Tappyweb"
InstallDirRegKey HKCU "${KEY}" "InstallLocation"
VIProductVersion "0.3.1.0"
VIAddVersionKey /LANG=1033 "ProductName" "Tappyweb"
VIAddVersionKey /LANG=1033 "CompanyName" "TappyWeb"
VIAddVersionKey /LANG=1033 "LegalCopyright" "Copyright (c) 2026 TappyWeb"
VIAddVersionKey /LANG=1033 "FileDescription" "Tappyweb Installer"
VIAddVersionKey /LANG=1033 "FileVersion" "0.3.1"
VIAddVersionKey /LANG=1033 "ProductVersion" "0.3.1"
RequestExecutionLevel user
SetCompressor /SOLID lzma
ShowInstDetails show
ShowUninstDetails show
BrandingText "Tappyweb"
Icon "${ICON}"
UninstallIcon "${ICON}"
!define MUI_ICON "${ICON}"
!define MUI_UNICON "${ICON}"
!define MUI_ABORTWARNING
!define MUI_WELCOMEFINISHPAGE_BITMAP "${ART}"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "${ART}"
!define MUI_WELCOMEPAGE_TEXT "Install the Tappyweb language and its Terminal command on this account."
!define MUI_FINISHPAGE_TEXT "Tappyweb is installed.$\r$\n$\r$\nOpen a new terminal and run:$\r$\ntappy file.tappy"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH
!insertmacro MUI_LANGUAGE "English"
Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "This installer requires 64-bit Windows."
    Abort
  ${EndIf}
  ${IfNot} ${AtLeastWin10}
    MessageBox MB_ICONSTOP "This installer requires Windows 10 or newer."
    Abort
  ${EndIf}
  SetRegView 64
FunctionEnd
Section "Tappyweb language" MainSection
  SectionIn RO
  SetOutPath "$INSTDIR"
  File /r "${PAYLOAD_FILES}"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "${KEY}" "DisplayName" "Tappyweb"
  WriteRegStr HKCU "${KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "${KEY}" "Publisher" "TappyWeb"
  WriteRegStr HKCU "${KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${KEY}" "DisplayIcon" "$INSTDIR\AppIcon.ico"
  WriteRegStr HKCU "${KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKCU "${KEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegDWORD HKCU "${KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${KEY}" "NoRepair" 1
  ReadRegStr $0 HKCU "Software\Classes\.tappy" ""
  ${If} $0 != "Tappyweb.Source"
    WriteRegStr HKCU "${KEY}" "PreviousAssociation" "$0"
  ${EndIf}
  WriteRegStr HKCU "Software\Classes\.tappy" "" "Tappyweb.Source"
  WriteRegStr HKCU "Software\Classes\.tappy\OpenWithProgids" "Tappyweb.Source" ""
  WriteRegStr HKCU "Software\Classes\Tappyweb.Source" "" "Tappyweb source"
  WriteRegStr HKCU "Software\Classes\Tappyweb.Source\DefaultIcon" "" "$INSTDIR\AppIcon.ico"
  WriteRegStr HKCU "Software\Classes\Tappyweb.Source\shell\open\command" "" '"$SYSDIR\cmd.exe" /d /s /k ""$INSTDIR\tappy.exe" "%1""'
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'

SectionEnd
Section "Terminal command (user PATH)" PathSection
  ClearErrors
  ExecWait '"$INSTDIR\EnvPath.exe" add "$INSTDIR"' $0
  ${If} ${Errors}
    MessageBox MB_ICONSTOP "Could not start the Terminal PATH helper."
    SetErrorLevel 1
    Abort
  ${EndIf}
  ${If} $0 == 0
    WriteRegDWORD HKCU "${KEY}" "PathAdded" 1
  ${ElseIf} $0 != 10
    MessageBox MB_ICONSTOP "Could not add tappy to your user PATH (error $0)."
    SetErrorLevel 1
    Abort
  ${EndIf}
SectionEnd
Function un.onInit
  SetRegView 64
FunctionEnd
Section "Uninstall"
  ReadRegStr $0 HKCU "Software\Classes\.tappy" ""
  ${If} $0 == "Tappyweb.Source"
    ReadRegStr $0 HKCU "${KEY}" "PreviousAssociation"
    ${If} $0 == ""
      DeleteRegValue HKCU "Software\Classes\.tappy" ""
    ${Else}
      WriteRegStr HKCU "Software\Classes\.tappy" "" "$0"
    ${EndIf}
  ${EndIf}
  DeleteRegValue HKCU "Software\Classes\.tappy\OpenWithProgids" "Tappyweb.Source"
  DeleteRegKey HKCU "Software\Classes\Tappyweb.Source"
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
  ReadRegDWORD $0 HKCU "${KEY}" "PathAdded"
  ${If} $0 == 1
    ClearErrors
    ExecWait '"$INSTDIR\EnvPath.exe" remove "$INSTDIR"' $0
    ${If} ${Errors}
      MessageBox MB_ICONSTOP "Could not start the PATH removal helper."
      SetErrorLevel 1
      Abort
    ${EndIf}
    ${If} $0 != 0
      MessageBox MB_ICONSTOP "Could not remove the Terminal PATH entry (error $0)."
      SetErrorLevel 1
      Abort
    ${EndIf}
  ${EndIf}
  !include "${REMOVE_FILES}"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  DeleteRegKey HKCU "${KEY}"
SectionEnd
