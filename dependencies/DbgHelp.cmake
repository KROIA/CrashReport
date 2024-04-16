## description: DbgHelp

# Add this library to the specific profiles of this project
list(APPEND DEPENDENCIES_FOR_SHARED_LIB DbgHelp.lib)
list(APPEND DEPENDENCIES_FOR_STATIC_LIB DbgHelp.lib)
list(APPEND DEPENDENCIES_FOR_STATIC_PROFILE_LIB DbgHelp.lib) # only use for static profiling profile
