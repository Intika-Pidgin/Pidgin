;;
;; Windows Pidgin NSIS installer plugin helper utilities
;; Copyright 2005, Daniel Atallah <daniel_atallah@yahoo.com>
;;
;; Include in plugin installer scripts using:
;;   !addincludedir "${PATH_TO_PIDGIN_SRC}\pidgin\win32\nsis"
;;   !include "pidgin-plugin.nsh"
;;

!define PIDGIN_REG_KEY              "SOFTWARE\pidgin"

!define PIDGIN_VERSION_OK           0
!define PIDGIN_VERSION_INCOMPATIBLE 1
!define PIDGIN_VERSION_UNDEFINED    2

; Extract the Pidgin Version from the registry
; This will set the Error flag if unable to determine the value
; Pop the value of the stack after calling this to get the value (unless Error Flag is set)
Function GetPidginVersion
  Push $R0

  ; Read the pidgin version
  ClearErrors
  ReadRegStr $R0 HKLM ${PIDGIN_REG_KEY} "Version"
  IfErrors +1 GetPidginVersion_found
  ; fall back to the HKCU registry key
  ReadRegStr $R0 HKCU ${PIDGIN_REG_KEY} "Version"
  IfErrors GetPidginVersion_done ; Keep the error flag set

  GetPidginVersion_found:
  Push $R0 ; Push the value onto the stack
  Exch

  GetPidginVersion_done:
  ; restore $R0
  Pop $R0
FunctionEnd

; Check that the currently installed Pidgin version is compatible
; with the plugin version we are installing
; Push the Plugin's Pidgin Version onto the Stack before calling
; After calling, the top of the Stack will contain the result of the check:
;   PIDGIN_VERSION_OK - If the installed Pidgin version is compatible w/ the version specified
;   PIDGIN_VERSION_INCOMPATIBLE - If the installed Pidgin version isn't compatible w/ the version specified
;   PIDGIN_VERSION_UNDEFINED - If the installed Pidgin version can't be determined 
Function CheckPidginVersion
  ; Save the Variable values that we will use in the stack
  Push $R4
  Exch
  Pop $R4 ; Get the plugin's Pidgin Version
  Push $R0
  Push $R1
  Push $R2

  ; Read the pidgin version
  Call GetPidginVersion
  IfErrors checkPidginVersion_noPidginInstallFound
  Pop $R0

  ;If they are exactly the same, we don't need to look at anything else
  StrCmp $R0 $R4 checkPidginVersion_VersionOK 

  ; Versions are in the form of X.Y.Z
  ; If X is different or plugin's Y > pidgin's Y, then we shouldn't install

  ;Check the Major Version
  Push $R0
  Push 0
  Call GetVersionComponent
  IfErrors checkPidginVersion_noPidginInstallFound ;We couldn't extract 'X' from the installed pidgin version
  Pop $R2
  Push $R4
  Push 0
  Call GetVersionComponent
  IfErrors checkPidginVersion_BadVersion ; this isn't a valid version, so don't bother even checking
  Pop $R1
  ;Check that both versions' X is the same
  StrCmp $R1 $R2 +1 checkPidginVersion_BadVersion

  ;Check the Minor Version
  Push $R0
  Push 1
  Call GetVersionComponent
  IfErrors checkPidginVersion_noPidginInstallFound ;We couldn't extract 'Y' from the installed pidgin version
  Pop $R2
  Push $R4
  Push 1
  Call GetVersionComponent
  IfErrors checkPidginVersion_BadVersion ; this isn't a valid version, so don't bother even checking
  Pop $R1
  ;Check that plugin's Y <= pidgin's Y
  IntCmp $R1 $R2 checkPidginVersion_VersionOK checkPidginVersion_VersionOK checkPidginVersion_BadVersion

  checkPidginVersion_BadVersion:
    Push ${PIDGIN_VERSION_INCOMPATIBLE}
    goto checkPidginVersion_done
  checkPidginVersion_noPidginInstallFound:
    Push ${PIDGIN_VERSION_UNDEFINED}
    goto checkPidginVersion_done
  checkPidginVersion_VersionOK:
    Push ${PIDGIN_VERSION_OK}

  checkPidginVersion_done:
  ; Restore the Variables that we used
  Exch
  Pop $R2
  Exch
  Pop $R1
  Exch
  Pop $R0
  Exch
  Pop $R4
FunctionEnd

; Extract the part of a string prior to "." (or the whole string if there is no ".")
; If no "." was found, the ErrorFlag will be set
; Before this is called, Push ${VERSION_STRING} must be called, and then Push 0 for Major, 1 for Minor, etc
; Pop should be called after to retrieve the new value
Function GetVersionComponent
  ClearErrors

  ; Save the Variable values that we will use in the stack
  Push $1
  Exch
  Pop $1 ;The version component which we want to extract (0, 1, 2)
  Exch
  Push $0
  Exch
  Pop $0 ;The string from which to extract the version component

  Push $2
  Push $3
  Push $4
  Push $5
  Push $6
  Push $7

  StrCpy $2 "0" ;Initialize our string index counter
  StrCpy $7 "0" ;Index of last "."
  StrCpy $3 "0" ;Initialize our version index counter

  startGetVersionComponentLoop:
    ;avoid infinite loop (if we have gotten the whole initial string, exit the loop and set the error flag)
    StrCmp $6 $0 GetVersionComponentSetErrorFlag
    IntOp $2 $2 + 1
    StrCpy $6 $0 $2 ;Update the infinite loop preventing string
    ;Determine the correct substring (only the current index component)
    IntOp $5 $2 - $7
    StrCpy $4 $0 $5 $7 ;Append the current character in $0 to $4
    StrCpy $5 $0 1 $2 ;store the next character in $5

    ;if the next character is ".", $4 will contain the version component prior to "."
    StrCmp $5 "." +1 startGetVersionComponentLoop
    StrCmp $3 $1 doneGetVersionComponent ;If it is the version component we're looking for, stop
    IntOp $3 $3 + 1 ;Increment the version index counter
    IntOp $2 $2 + 1 ;Increment the version string index to "." (so it will be skipped)
    StrCpy $7 $2 ;Keep track of the index of the last "."
    StrCpy $6 $0 $2 ;Update the infinite loop preventing string
    goto startGetVersionComponentLoop

  GetVersionComponentSetErrorFlag:
    SetErrors

  doneGetVersionComponent:
  ; Restore the Variables that we used
  Pop $7
  Pop $6
  Pop $5
  Push $4 ;This is the value we're returning
  Exch
  Pop $4
  Exch
  Pop $3
  Exch
  Pop $2
  Exch
  Pop $0
  Exch
  Pop $1
FunctionEnd

