include(CMakeResources.cmake)

add_resources(source_files apple2roms
  IDR_DISK2_13SECTOR_FW               "DISK2-13sector.rom"
  IDR_DISK2_16SECTOR_FW               "DISK2.rom"
  IDR_SSC_FW                          "SSC.rom"
  IDR_HDDRVR_FW                       "Hddrvr.bin"
  IDR_HDDRVR_V2_FW                    "Hddrvr-v2.bin"
  IDR_HDC_SMARTPORT_FW                "HDC-SmartPort.bin"
  IDR_PRINTDRVR_FW                    "Parallel.rom"
  IDR_MOCKINGBOARD_D_FW               "Mockingboard-D.rom"
  IDR_MOUSEINTERFACE_FW               "MouseInterface.rom"
  IDR_THUNDERCLOCKPLUS_FW             "ThunderClockPlus.rom"
  IDR_TKCLOCK_FW                      "TKClock.rom"

  IDR_APPLE2_ROM                      "Apple2.rom"
  IDR_APPLE2_PLUS_ROM                 "Apple2_Plus.rom"
  IDR_APPLE2_JPLUS_ROM                "Apple2_JPlus.rom"
  IDR_APPLE2E_ROM                     "Apple2e.rom"
  IDR_APPLE2E_ENHANCED_ROM            "Apple2e_Enhanced.rom"
  IDR_PRAVETS_82_ROM                  "PRAVETS82.ROM"
  IDR_PRAVETS_8M_ROM                  "PRAVETS8M.ROM"
  IDR_PRAVETS_8C_ROM                  "PRAVETS8C.ROM"
  IDR_TK3000_2E_ROM                   "TK3000e.rom"
  IDR_BASE_64A_ROM                    "Base64A.rom"
  IDR_FREEZES_F8_ROM                  "Freezes_Non-autostart_F8_Rom.rom"

  IDR_APPLE2_VIDEO_ROM                "Apple2_Video.rom"
  IDR_APPLE2_JPLUS_VIDEO_ROM          "Apple2_JPlus_Video.rom"
  IDR_APPLE2E_ENHANCED_VIDEO_ROM      "Apple2e_Enhanced_Video.rom"
  IDR_BASE64A_VIDEO_ROM               "Base64A_German_Video.rom"

  IDB_CHARSET82                       "CHARSET82.bmp"
  IDB_CHARSET8M                       "CHARSET8M.bmp"
  IDB_CHARSET8C                       "CHARSET8C.bmp"

  IDB_APPLEWIN                        "ApplewinLogo.bmp"
  IDC_APPLEWIN_ICON                   "APPLEWIN.ICO"

  IDB_DEBUG_FONT_7_by_8               "debug6502.ttf"
  IDB_IMGUI_FONT_COUSINE              "Cousine-Regular.ttf"
  )

add_library(apple2roms STATIC
  ${source_files}
)

target_include_directories(apple2roms PUBLIC
  ${CMAKE_CURRENT_BINARY_DIR}
)
