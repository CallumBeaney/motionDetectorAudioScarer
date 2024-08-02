idf_component_register(
  SRCS 
    "main.c"
    "amplifier/MAX98357.c"
    "motion_detector/ADA4871.c"

  INCLUDE_DIRS     
    "."
    "amplifier"
    "motion_detector"

  REQUIRES 
    "freertos"
    "driver"

  EMBED_FILES
    "o.pcm" # the audio file used by the MAX98357
)
