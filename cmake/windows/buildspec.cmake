include_guard(GLOBAL)

include(buildspec_common)

function(_check_dependencies_windows)
  set(arch ${CMAKE_VS_PLATFORM_NAME})
  set(platform windows-${arch})

  # Edit OBS_DEPS_DIR (or pass -DOBS_DEPS_DIR=<folder>) to change where OBS sources and dependencies download.
  # If several plugins point at the same folder, they share one copy instead of 3 GB each.
  set(OBS_DEPS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.deps" CACHE PATH "Folder for downloaded OBS dependencies")
  cmake_path(ABSOLUTE_PATH OBS_DEPS_DIR NORMALIZE OUTPUT_VARIABLE dependencies_dir)
  set(prebuilt_filename "windows-deps-VERSION-ARCH-REVISION.zip")
  set(prebuilt_destination "obs-deps-VERSION-ARCH")
  set(qt6_filename "windows-deps-qt6-VERSION-ARCH-REVISION.zip")
  set(qt6_destination "obs-deps-qt6-VERSION-ARCH")
  set(obs-studio_filename "VERSION.zip")
  set(obs-studio_destination "obs-studio-VERSION")
  set(dependencies_list prebuilt qt6 obs-studio)

  _check_dependencies()
endfunction()

_check_dependencies_windows()
