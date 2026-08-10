cmake_minimum_required(VERSION 3.28 FATAL_ERROR)

target_sources(FuzeHttp
  PRIVATE
    FuzeHttp.cpp
    FuzeHttpState.cpp
    FuzeHttpServer.cpp
    FuzeHttpUtils.cpp
    PermissionObject.cpp
  PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS
      ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
      Migrations.cppm
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
      Group.hpp
      HttpSession.hpp
      Listener.hpp
      PermissionCollection.hpp
      PermissionObject.hpp
      PermissionSetting.hpp
      urls.hpp
      WebsocketSession.hpp
)
