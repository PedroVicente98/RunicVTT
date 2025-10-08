# Sign all .exe files in the package directory (used to sign NSIS installer)
if(NOT DEFINED PKG_DIR OR NOT EXISTS "${PKG_DIR}")
  message(FATAL_ERROR "PKG_DIR not set or doesn't exist")
endif()
file(GLOB _exes "${PKG_DIR}/*.exe")
foreach(exe ${_exes})
  execute_process(COMMAND "${SIGNTOOL}" sign
    /fd SHA256 /tr http://timestamp.sectigo.com /td SHA256
    /f "${PFX}" /p "${PWD}" "${exe}"
    RESULT_VARIABLE rc
  )
  if (rc EQUAL 0)
    message(STATUS "Signed ${exe}")
  else()
    message(WARNING "signtool failed for ${exe} (code ${rc})")
  endif()
endforeach()
