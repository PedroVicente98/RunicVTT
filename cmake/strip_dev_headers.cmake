# Remove development headers from the staging area before CPack zips it
message(STATUS "[CPack] Stripping development headers from staging...")
file(REMOVE_RECURSE "${CMAKE_INSTALL_PREFIX}/include")
