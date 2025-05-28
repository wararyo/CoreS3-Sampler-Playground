set(COMPONENT_NAME capsulesampler)

set(COMPONENT_ADD_INCLUDEDIRS "include")
file(GLOB SRCS
     src/*.cpp
     src/*.S
     )
set(COMPONENT_SRCS ${SRCS})
set(COMPONENT_REQUIRES esp_timer)

register_component()
