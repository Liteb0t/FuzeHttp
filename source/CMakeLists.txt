cmake_minimum_required(VERSION 3.28 FATAL_ERROR)

target_sources(FuzeHttp
  PRIVATE
    FuzeHttp.cpp
    FuzeHttpState.cpp
    FuzeHttpServer.cpp
    FuzeHttpUtils.cpp
  PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS
      ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
      Migrations.cppm
      PermissionSetting.cppm
      PermissionCollection.cppm
      PermissionObject.cppm
      Group.cppm
    FILE_SET headers
    TYPE HEADERS
    BASE_DIRS
      ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
      FuzeDBI.hpp
      FuzeHttp.hpp
      FuzeHttpState.hpp
      FuzeHttpServer.hpp
      FuzeHttpUtils.hpp
      HttpSession.hpp
      Listener.hpp
      urls.hpp
      WebsocketSession.hpp
)
