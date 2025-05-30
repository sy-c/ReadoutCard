# Copyright 2019-2020 CERN and copyright holders of ALICE O2.
# See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
# All rights not expressly granted are reserved.
#
# This software is distributed under the terms of the GNU General Public
# License v3 (GPL Version 3), copied verbatim in the file "COPYING".
#
# In applying this license CERN does not waive the privileges and immunities
# granted to it by virtue of its status as an Intergovernmental Organization
# or submit itself to any jurisdiction.

####################################
# General project definition
####################################

CMAKE_MINIMUM_REQUIRED(VERSION 3.5.2 FATAL_ERROR)

# Set cmake policy by version: https://cmake.org/cmake/help/latest/manual/cmake-policies.7.html
if(${CMAKE_VERSION} VERSION_LESS 3.12)
  cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
else()
  cmake_policy(VERSION 3.12)
endif()

# Define project
project(ReadoutCard
  VERSION 0.46.1
  DESCRIPTION "O2 ReadoutCard library"
  LANGUAGES C CXX
)

# Documentation dir
add_subdirectory(doc)

# simplified build mode for doc only
if(ONLYDOC)
  return()
endif()

# Add compiler flags for warnings and debug symbols
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -Wextra -Werror")

# Increase boost MPL maximum length to accommodate for a longer Parameters variant
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBOOST_MPL_CFG_NO_PREPROCESSED_HEADERS -DBOOST_MPL_LIMIT_LIST_SIZE=30")

# Set fPIC for all targets
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Set CMAKE_INSTALL_LIBDIR explicitly to lib (to avoid lib64 on CC7)
set(CMAKE_INSTALL_LIBDIR lib)

# Set the default build type to "RelWithDebInfo"
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo"
    CACHE
    STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel Coverage."
    FORCE
  )
endif()

####################################
# Populate the Cru/Constants.h file with the register addresses contained in CRU/cru_table.py
####################################
#execute_process(COMMAND /usr/bin/env python3 ${CMAKE_CURRENT_SOURCE_DIR}/src/Cru/cru_constants_populate.py
#  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/Cru
#  RESULT_VARIABLE status)
#IF(NOT "${status}" EQUAL "0")
#  MESSAGE(WARNING "Couldn't update CRU Register Addresses")
#ENDIF()

####################################
# Dependencies
####################################

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

if(NOT APPLE)
  find_package(Python3 3.6 COMPONENTS Interpreter Development)
  if(Python3_FOUND)
    set(boost_python_component "python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}")
  endif()
endif()

find_package(Boost 1.56
  COMPONENTS
  unit_test_framework
  filesystem
  system
  program_options
  ${boost_python_component}
  REQUIRED
)

find_package(Git QUIET)
find_package(Common REQUIRED)
find_package(Configuration REQUIRED)
find_package(Monitoring REQUIRED)
find_package(InfoLogger REQUIRED)
find_package(PDA REQUIRED)

include(GNUInstallDirs)

####################################
# RPATH
####################################

# https://gitlab.kitware.com/cmake/community/wikis/doc/cmake/RPATH-handling
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

####################################
# Module, library and executable definition
####################################

add_library(ReadoutCard SHARED
  src/BarInterfaceBase.cxx
  src/CardConfigurator.cxx
  src/CardFinder.cxx
  src/CardType.cxx
  src/Crorc/Crorc.cxx
  src/Crorc/CrorcDmaChannel.cxx
  src/Crorc/CrorcBar.cxx
  src/Cru/Common.cxx
  src/Cru/CruDmaChannel.cxx
  src/Cru/CruBar.cxx
  src/Cru/DatapathWrapper.cxx
  src/Cru/Eeprom.cxx
  src/Cru/Gbt.cxx
  src/Cru/I2c.cxx
  src/Cru/PatternPlayer.cxx
  src/Cru/Ttc.cxx
  src/DmaChannelBase.cxx
  src/DmaChannelPdaBase.cxx
  src/ChannelPaths.cxx
  src/ExceptionInternal.cxx
  src/Factory/ChannelFactory.cxx
  src/FirmwareChecker.cxx
  src/Logger.cxx
  src/MemoryMappedFile.cxx
  src/Parameters.cxx
  src/ParameterTypes/Clock.cxx
  src/ParameterTypes/DatapathMode.cxx
  src/ParameterTypes/DownstreamData.cxx
  src/ParameterTypes/GbtCounterType.cxx
  src/ParameterTypes/GbtMode.cxx
  src/ParameterTypes/GbtMux.cxx
  src/ParameterTypes/GbtPatternMode.cxx
  src/ParameterTypes/GbtStatsMode.cxx
  src/ParameterTypes/Hex.cxx
  src/ParameterTypes/DataSource.cxx
  src/ParameterTypes/PciAddress.cxx
  src/ParameterTypes/PciSequenceNumber.cxx
  src/ParameterTypes/ResetLevel.cxx
  src/ParameterTypes/SerialId.cxx
  src/Pda/PdaBar.cxx
  src/Pda/PdaDevice.cxx
  src/Pda/PdaDmaBuffer.cxx
  src/ReadoutCardVersion.cxx
  src/RocPciDevice.cxx
  src/Utilities/Hugetlbfs.cxx
  src/Utilities/MemoryMaps.cxx
  src/Utilities/Numa.cxx
  $<$<BOOL:${Python2_FOUND}>:src/PythonInterface.cxx>
  $<$<BOOL:${Python3_FOUND}>:src/PythonInterface.cxx>
)

target_include_directories(ReadoutCard
  PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link targets
target_link_libraries(ReadoutCard
  PUBLIC
    AliceO2::InfoLogger
    AliceO2::Common
    AliceO2::Configuration
    AliceO2::Monitoring
    $<$<BOOL:${Python2_FOUND}>:Boost::python27>
    $<$<BOOL:${Python2_FOUND}>:Python2::Python>
    $<$<BOOL:${Python3_FOUND}>:Boost::python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}>
    $<$<BOOL:${Python3_FOUND}>:Python3::Python>
  PRIVATE
    pda::pda
    Boost::filesystem
    Boost::system
    pthread
)

# Use C++17
target_compile_features(ReadoutCard PUBLIC cxx_std_17)


####################################
# Executables
####################################

set(EXE_SRCS
  ProgramBarStress.cxx
  ProgramConfig.cxx
  ProgramCtpEmulator.cxx
  ProgramCleanup.cxx
  ProgramDmaBench.cxx
  ../Example.cxx
  ProgramFirmwareCheck.cxx
  ProgramFlash.cxx
  ProgramFlashRead.cxx
  ProgramListCards.cxx
  ProgramLoopback.cxx
  ProgramMetrics.cxx
  ProgramPacketMonitor.cxx
  ProgramPatternPlayer.cxx
  ProgramReset.cxx
  ProgramRegisterModify.cxx
  ProgramRegisterRead.cxx
  ProgramRegisterReadRange
  ProgramRegisterWrite.cxx
  ProgramSiuStatus.cxx
  ProgramStatus.cxx
  ProgramTriggerMonitor.cxx
  ProgramUserLogic.cxx
)

set(EXE_NAMES
  o2-roc-bar-stress
  o2-roc-config
  o2-roc-ctp-emulator
  o2-roc-cleanup
  o2-roc-bench-dma
  o2-roc-example
  o2-roc-fw-check
  o2-roc-flash
  o2-roc-flash-read
  o2-roc-list-cards
  o2-roc-loopback
  o2-roc-metrics
  o2-roc-pkt-monitor
  o2-roc-pat-player
  o2-roc-reset
  o2-roc-reg-modify
  o2-roc-reg-read
  o2-roc-reg-read-range
  o2-roc-reg-write
  o2-roc-siu-status
  o2-roc-status
  o2-roc-trig-monitor
  o2-roc-ul
)

list(LENGTH EXE_SRCS count)
math(EXPR count "${count}-1")
foreach(i RANGE ${count})
  list(GET EXE_SRCS ${i} src)
  list(GET EXE_NAMES ${i} name)
  add_executable(${name} src/CommandLineUtilities/${src} src/CommandLineUtilities/Options.cxx src/CommandLineUtilities/Common.cxx)
  target_include_directories(${name}
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  target_link_libraries(${name}
    PRIVATE
      ReadoutCard
      Boost::program_options
      pda::pda
  )

  # create binary symlinks to preserve backwards compatibility
  STRING(REGEX REPLACE "^o2-roc" "roc" legacy_exe_link ${name})
  add_custom_command(TARGET ${name} POST_BUILD
    BYPRODUCTS ${legacy_exe_link}
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${name} ${legacy_exe_link})
  list(APPEND LEGACY_EXE_NAMES ${legacy_exe_link}) 
endforeach()

####################################
# Tests
####################################

enable_testing()

set(TEST_SRCS
  test/TestChannelFactoryUtils.cxx
  test/TestChannelPaths.cxx
  test/TestCruBar.cxx
  test/TestCruDataFormat.cxx
  test/TestEnums.cxx
  test/TestInterprocessLock.cxx
  test/TestMemoryMappedFile.cxx
  test/TestParameters.cxx
  test/TestPciAddress.cxx
  test/TestProgramOptions.cxx
  test/TestRorcException.cxx
)

foreach (test ${TEST_SRCS})
  get_filename_component(test_name ${test} NAME)
  string(REGEX REPLACE ".cxx" "" test_name ${test_name})

  add_executable(${test_name} ${test} src/CommandLineUtilities/Options.cxx)
   target_include_directories(${test_name}
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  target_link_libraries(${test_name}
    PRIVATE
      ReadoutCard
      Boost::unit_test_framework
      pda::pda
  )
  add_test(NAME ${test_name} COMMAND ${test_name})
  set_tests_properties(${test_name} PROPERTIES TIMEOUT 15)
endforeach()

####################################
# Install
####################################

# Install library and executables
install(TARGETS ReadoutCard ${EXE_NAMES}
  EXPORT ReadoutCardTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

set_target_properties(ReadoutCard
  PROPERTIES
  OUTPUT_NAME
    O2ReadoutCard # Adhere to O2 naming conventions
)

# create a symlink libReadoutCard -> libO2ReadoutCard for backwards compatibility
set(legacy_library_link libReadoutCard.so)
set(legacy_library_target libO2ReadoutCard.so)

add_custom_command(TARGET ReadoutCard POST_BUILD
  BYPRODUCTS ${legacy_library_link}
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${legacy_library_target} ${legacy_library_link})

# Create version file
include(CMakePackageConfigHelpers)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/cmake/ReadoutCardConfigVersion.cmake"
  VERSION ${PACKAGE_VERSION}
  COMPATIBILITY AnyNewerVersion
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ReadoutCard DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Export targets
install(EXPORT ReadoutCardTargets
  FILE
    ReadoutCardTargets.cmake
  NAMESPACE
    AliceO2::
  DESTINATION
    ${CMAKE_INSTALL_LIBDIR}/cmake/ReadoutCard
)

# Configure and install Config files
configure_package_config_file(
  cmake/ReadoutCardConfig.cmake.in cmake/ReadoutCardConfig.cmake
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/ReadoutCard"
  PATH_VARS CMAKE_INSTALL_PREFIX
)

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/ReadoutCardConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/ReadoutCardConfigVersion.cmake"
  DESTINATION
    ${CMAKE_INSTALL_LIBDIR}/cmake/ReadoutCard
)

# Install the roc-config template
install(
  FILES cru_template.cfg
  DESTINATION ${CMAKE_INSTALL_PREFIX}/etc
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

# Install the hugetlbfs script separately, because it's not a compiled target
install(
  FILES src/o2-roc-setup-hugetlbfs.sh
  DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  RENAME o2-roc-setup-hugetlbfs
)

# Create a symlink for backwards compatibility
set(legacy_hugetlbfs_target o2-roc-setup-hugetlbfs)
set(legacy_hugetlbfs_link roc-setup-hugetlbfs)

add_custom_command(TARGET ReadoutCard
  DEPENDS ${legacy_hugetlbfs_target}
  BYPRODUCTS ${legacy_hugetlbfs_link}
  COMMAND ${CMAKE_COMMAND} -E create_symlink ${legacy_hugetlbfs_target} ${legacy_hugetlbfs_link})

# Deploy script and symlink
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/${legacy_hugetlbfs_target} ${CMAKE_CURRENT_BINARY_DIR}/${legacy_hugetlbfs_link}
  DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

# Install the binary symlinks
foreach(legacy_exe ${LEGACY_EXE_NAMES})
  install (
    FILES ${CMAKE_CURRENT_BINARY_DIR}/${legacy_exe}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}
  )
endforeach()

# Install the legacy library symlink
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/${legacy_library_link}
  DESTINATION ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}
)
