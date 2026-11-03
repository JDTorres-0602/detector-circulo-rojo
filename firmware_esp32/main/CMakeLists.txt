idf_component_register(
    SRCS "main.c" "wifi.c" "web.c" "uno.c"
         "camara.c" "redimensionar.c"
         "vision_color.c" "detector.cc" "modelo_balon.cc"
         "postproceso.c" "seguidor.c"
    INCLUDE_DIRS "."
    # La página web se mete dentro del firmware como un archivo de texto
    EMBED_TXTFILES "pagina.html"
    # Sin REQUIRES: "main" puede usar todos los componentes de ESP-IDF y los de idf_component.yml
)
# TFLite Micro tiene avisos que no son nuestros
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-variable)
