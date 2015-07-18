# Project definition
# 
project(knntool)

# 
# Dependencies
# 

if(STATIC_BINARIES)
  set(Boost_USE_STATIC_LIBS ON)
endif()

find_package(Boost COMPONENTS program_options)

# 
# Executable
# 

# all required source files
set(SOURCES Main.cc
            Parser.cc
            Trainer.cc)

# we can only build the KNN tool if we have boost on the system
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})

  add_executable(knntool ${SOURCES})

  # Linking
  if(STATIC_BINARIES)
    target_link_libraries(knntool ${Boost_LIBRARIES} libpca-static)
  else()
    target_link_libraries(knntool ${Boost_LIBRARIES} libpca-shared)
  endif()

else()
  message("Boost component 'program_options' was not found. Cannot build 'knntool'")
endif()
