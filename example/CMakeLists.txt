cmake_minimum_required(VERSION 3.28 FATAL_ERROR)

add_executable(example)
target_link_libraries(example
  PRIVATE
    FuzeHttp
    sodium
)
target_sources(example
  PRIVATE
    main.cpp
    shared_state.cpp
    urls.cpp
    views.cpp
    views_registration.cpp
  # PUBLIC FILE_SET CXX_MODULES
  # FILES
  #   migrations.cppm
)
set(PROJECT_FOLDER "FuzeHttp_Example")
target_compile_definitions(example PUBLIC PROJECT_FOLDER="${PROJECT_FOLDER}")
configure_file(config.ini ${CMAKE_BINARY_DIR}/share/${PROJECT_FOLDER}/config.ini COPYONLY)
configure_file(database_template.sql ${CMAKE_BINARY_DIR}/share/${PROJECT_FOLDER}/database_template.sql COPYONLY)

set(frontend_list "") ## list which will be used as DEPENDS on add_custom_target
file(GLOB_RECURSE frontend_files CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/frontend ${CMAKE_CURRENT_SOURCE_DIR}/frontend/*)
foreach(file ${frontend_files})
  set(destination ${CMAKE_BINARY_DIR}/share/${PROJECT_FOLDER}/frontend/${file})
  add_custom_command(
    OUTPUT ${destination}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/frontend/${file} ${destination}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/frontend/${file}
    COMMENT "Copying ${CMAKE_CURRENT_SOURCE_DIR}/frontend/${file}"
  )
  list(APPEND frontend_list ${destination})
endforeach()

add_custom_target(frontend ALL
  DEPENDS
    ${frontend_list}
)
# install(
#   DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/frontend
#   DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_FOLDER}/
# )
add_dependencies(example
  frontend
)
