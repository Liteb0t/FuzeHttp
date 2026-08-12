cmake_minimum_required(VERSION 3.28 FATAL_ERROR)

target_sources(FuzeHttp
  PRIVATE
    FuzeHttpState.cpp
    FuzeHttpServer.cpp
    FuzeHttpUtils.cpp
  PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS
      ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
      FuzeDBI.cppm
      FuzeHttp.cppm
      Group.cppm
      Migrations.cppm
      PermissionSetting.cppm
      PermissionCollection.cppm
      PermissionObject.cppm
    FILE_SET headers
    TYPE HEADERS
    BASE_DIRS
      ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
      # buildResponse.hpp
      Controller.hpp
      FuzeHttpState.hpp
      FuzeHttpServer.hpp
      FuzeHttpUtils.hpp
      HttpSession.hpp
      Listener.hpp
      Request.hpp
      urls.hpp
      ViewPath.hpp
      WebsocketSession.hpp
)
