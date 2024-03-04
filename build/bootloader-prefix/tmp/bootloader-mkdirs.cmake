# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/Users/jppalacios/esp-adf/esp-idf/components/bootloader/subproject"
  "/Users/jppalacios/esp/EGR536-VoiceAssistantProject/build/bootloader"
  "/Users/jppalacios/esp/EGR536-VoiceAssistantProject/build/bootloader-prefix"
  "/Users/jppalacios/esp/EGR536-VoiceAssistantProject/build/bootloader-prefix/tmp"
  "/Users/jppalacios/esp/EGR536-VoiceAssistantProject/build/bootloader-prefix/src/bootloader-stamp"
  "/Users/jppalacios/esp/EGR536-VoiceAssistantProject/build/bootloader-prefix/src"
  "/Users/jppalacios/esp/EGR536-VoiceAssistantProject/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/jppalacios/esp/EGR536-VoiceAssistantProject/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/jppalacios/esp/EGR536-VoiceAssistantProject/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
