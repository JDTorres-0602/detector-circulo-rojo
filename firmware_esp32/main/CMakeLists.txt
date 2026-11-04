idf_component_register(
    SRCS "main.c" "wifi.c" "web.c" "uno.c"
         "camara.c" "redimensionar.c"
         "vision_color.c" "detector.cc" "modelo_balon.cc"
         "postproceso.c" "seguidor.c" "autonomo.c" "usb_fotos.c"
    INCLUDE_DIRS "."
    # La página web se mete dentro del firmware como un archivo de texto
    # pagina.html = página de depuración (/debug); hmi.html.gz = el HMI en React (/), ya
    # compilado y comprimido por hmi_react/build.mjs
    EMBED_TXTFILES "pagina.html"
    EMBED_FILES "hmi.html.gz"
    # Sin REQUIRES: "main" puede usar todos los componentes de ESP-IDF y los de idf_component.yml
)
# TFLite Micro tiene avisos que no son nuestros
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-variable)
