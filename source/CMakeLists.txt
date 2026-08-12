cmake_minimum_required(VERSION 3.28 FATAL_ERROR)

target_sources(FuzeHttp
  # PRIVATE
    # FuzeHttpServer.cpp
    # WebsocketSession.cpp
  PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS
      ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
      FuzeDBI.cppm
      FuzeHttp.cppm
      FuzeHttpServer.cppm
      FuzeHttpState.cppm
      FuzeHttpUtils.cppm
      Group.cppm
      HttpSession.cppm
      Listener.cppm
      Migrations.cppm
      PermissionSetting.cppm
      PermissionCollection.cppm
      PermissionObject.cppm
    FILE_SET headers
    TYPE HEADERS
    BASE_DIRS
      ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
      Controller.hpp
      Request.hpp
      urls.hpp
      ViewPath.hpp
)
