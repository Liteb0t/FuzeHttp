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
      Client.cppm
      Controller.cppm
      FuzeDBI.cppm
      FuzeHttp.cppm
      FuzeHttpServer.cppm
      FuzeHttpUtils.cppm
      Group.cppm
      HttpSession.cppm
      Listener.cppm
      Migrations.cppm
      PermissionSetting.cppm
      PermissionCollection.cppm
      PermissionObject.cppm
      Resolver.cppm
      Secret.cppm
      ProgramOptions.cppm
      StateBase.cppm
      ViewPath.cppm
    FILE_SET headers
    TYPE HEADERS
    BASE_DIRS
      ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
      # Controller.hpp
      Request.hpp
      # ViewPath.hpp
)
