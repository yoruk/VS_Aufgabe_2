# Try to find libcaf headers and library.
#
# Use this module as follows:
#
#     find_package(Libcaf)
#
# Variables used by this module (they can change the default behaviour and need
# to be set before calling find_package):
#
#  LIBCAF_ROOT_DIR  Set this variable to the root installation of
#                   libcaf if the module has problems finding 
#                   the proper installation path.
#
# Variables defined by this module:
#
#  LIBCAF_FOUND              System has libcaf headers and library
#  LIBCAF_LIBRARIES          List of library files  for all components
#  LIBCAF_INCLUDE_DIRS       List of include paths for all components
#  LIBCAF_LIBRARY_$C         Library file for component $C
#  LIBCAF_INCLUDE_DIR_$C     Include path for component $C

# iterate over user-defined components
foreach (comp ${Libcaf_FIND_COMPONENTS})
  # we use uppercase letters only for variable names
  string(TOUPPER "${comp}" UPPERCOMP)
  if ("${comp}" STREQUAL "core")
    set(HDRNAME "caf/all.hpp")
  else ()
    set(HDRNAME "caf/${comp}/all.hpp")
  endif ()
  # look for headers: give CMake hints where to find non-installed CAF versions
  # note that we look for the headers of each component individually: this is
  # necessary to support non-installed versions of CAF, i.e., accessing the
  # checked out "actor-framework" directory structure directly
  set(HDRHINT "actor-framework/libcaf_${comp}")
  find_path(LIBCAF_INCLUDE_DIR_${UPPERCOMP}
            NAMES
              ${HDRNAME}
            HINTS
              ${LIBCAF_ROOT_DIR}/include
              /usr/include
              /usr/local/include
              /opt/local/include
              /sw/include
              ${CMAKE_INSTALL_PREFIX}/include
              ../${HDRHINT}
              ../../${HDRHINT}
              ../../../${HDRHINT})
  mark_as_advanced(LIBCAF_INCLUDE_DIR_${UPPERCOMP})
  if (NOT "${LIBCAF_INCLUDE_DIR_${UPPERCOMP}}" STREQUAL "LIBCAF_INCLUDE_DIR_${UPPERCOMP}-NOTFOUND")
    # mark as found (set back to false in case library cannot be found)
    set(Libcaf_${comp}_FOUND true)
    # add to LIBCAF_INCLUDE_DIRS only if path isn't already set
    set(duplicate false)
    foreach (p ${LIBCAF_INCLUDE_DIRS})
      if (${p} STREQUAL ${LIBCAF_INCLUDE_DIR_${UPPERCOMP}})
        set(duplicate true)
      endif ()
    endforeach ()
    if (NOT duplicate)
      set(LIBCAF_INCLUDE_DIRS ${LIBCAF_INCLUDE_DIRS} ${LIBCAF_INCLUDE_DIR_${UPPERCOMP}})
    endif()
    # look for (.dll|.so|.dylib) file, again giving hints for non-installed CAFs
    # skip probe_event as it is header only
    if (NOT ${comp} STREQUAL "probe_event")
      find_library(LIBCAF_LIBRARY_${UPPERCOMP}
                   NAMES
                     "caf_${comp}"
                   HINTS
                     ${LIBCAF_ROOT_DIR}/lib
                     /usr/lib
                     /usr/local/lib
                     /opt/local/lib
                     /sw/lib
                     ${CMAKE_INSTALL_PREFIX}/lib
                     ../actor-framework/build/lib
                     ../../actor-framework/build/lib
                     ../../../actor-framework/build/lib)
      mark_as_advanced(LIBCAF_LIBRARY_${UPPERCOMP})
      if ("${LIBCAF_LIBRARY_${UPPERCOMP}}" STREQUAL "LIBCAF_LIBRARY-NOTFOUND")
        set(Libcaf_${comp}_FOUND false)
      else ()
        set(LIBCAF_LIBRARIES ${LIBCAF_LIBRARIES} ${LIBCAF_LIBRARY_${UPPERCOMP}})
      endif ()
    endif ()
  endif ()
endforeach ()

# let CMake check whether all requested components have been found
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libcaf
                                  FOUND_VAR LIBCAF_FOUND
                                  REQUIRED_VARS LIBCAF_LIBRARIES LIBCAF_INCLUDE_DIRS
                                  HANDLE_COMPONENTS)
# final step to tell CMake we're done
mark_as_advanced(LIBCAF_ROOT_DIR
                 LIBCAF_LIBRARIES
                 LIBCAF_INCLUDE_DIRS)
