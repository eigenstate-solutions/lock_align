# Install rules for the lock_align library: its headers, and a CMake package
# so that find_package(lock_align) gives lock_align::lock_align with the
# configuration this build was given.

include(CMakePackageConfigHelpers)

# Header-only, so everything goes under the architecture-independent data
# directory, which find_package also searches.
set(lock_align_package_dir ${CMAKE_INSTALL_DATADIR}/cmake/lock_align)

install(TARGETS lock_align EXPORT lock_align_targets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT lock_align_targets
        FILE lock_alignTargets.cmake
        NAMESPACE lock_align::
        DESTINATION ${lock_align_package_dir})

configure_package_config_file(cmake/lock_alignConfig.cmake.in
                              ${CMAKE_CURRENT_BINARY_DIR}/lock_alignConfig.cmake
                              INSTALL_DESTINATION ${lock_align_package_dir})
# Before 1.0, a minor release may break compatibility.
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/lock_alignConfigVersion.cmake
                                 COMPATIBILITY SameMinorVersion
                                 ARCH_INDEPENDENT)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/lock_alignConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/lock_alignConfigVersion.cmake
        DESTINATION ${lock_align_package_dir})
