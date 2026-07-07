cmake_minimum_required(VERSION 3.23 FATAL_ERROR)

target_sources(FuzeHttp
  PRIVATE
    FuzeHttp.cpp
    FuzeHttpState.cpp
    FuzeHttpServer.cpp
    FuzeHttpUtils.cpp
    Migrations.cpp
    PermissionObject.cpp
  PUBLIC
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
      Migrations.hpp
      PermissionCollection.hpp
      PermissionObject.hpp
      PermissionSetting.hpp
      urls.hpp
      WebsocketSession.hpp
)
