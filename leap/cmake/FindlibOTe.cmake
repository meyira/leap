# https://raw.githubusercontent.com/stevedekorte/io/master/modules/FindlibOTe.cmake

# Try to find the libOTe librairies
# libOTe_FOUND - system has libOTe lib
# libOTe_INCLUDE_DIR - the libOTe include directory
# libOTe_LIBRARIES - Libraries needed to use libOTe
# 
if (libOTe_INCLUDE_DIR AND libOTe_LIBRARIES)
 		# Already in cache, be silent
 		set(libOTe_FIND_QUIETLY TRUE)
 endif (libOTe_INCLUDE_DIR AND libOTe_LIBRARIES)
# 
 find_path(libOTe_INCLUDE_DIR NAMES libOTe.h )
 find_library(libOTe_LIBRARIES NAMES libOTe )
 MESSAGE(STATUS "libOTe libs: " ${libOTe_LIBRARIES} )
 
 include(FindPackageHandleStandardArgs)
 FIND_PACKAGE_HANDLE_STANDARD_ARGS(libOTe DEFAULT_MSG libOTe_INCLUDE_DIR libOTe_LIBRARIES)
 
 mark_as_advanced(libOTe_INCLUDE_DIR libOTe_LIBRARIES)
