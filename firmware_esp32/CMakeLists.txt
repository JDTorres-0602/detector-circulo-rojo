# Proyecto ESP-IDF: carro que sigue un balón rojo
# Compilar:  idf.py set-target esp32s3   (o esp32 para el módulo WROVER viejo)
#            idf.py build
#            idf.py -p COM10 flash monitor
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(carro_balon)
