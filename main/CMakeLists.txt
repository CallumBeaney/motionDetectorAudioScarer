idf_component_register(
  SRCS 
    "main.c"
    "amplifier/MAX98357.c"
    "motion_detector/ADA4871.c"

  INCLUDE_DIRS     
    "."
    "amplifier"
    "motion_detector"

  EMBED_FILES
    "o.pcm" 
    # used by MAX98357
)
