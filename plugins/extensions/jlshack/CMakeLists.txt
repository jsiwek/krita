set(SOURCES 
    plugin.cpp 
)

add_library(kritajlshack MODULE ${SOURCES})

target_link_libraries(
    kritajlshack
    kritaui
    kritalibkis
  )

install(TARGETS kritajlshack DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
