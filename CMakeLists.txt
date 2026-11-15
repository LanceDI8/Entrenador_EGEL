cmake_minimum_required(VERSION 3.25)

project(EntrenadorEGEL VERSION 0.1.0 LANGUAGES CXX)

# Usa C++17 como estándar para compilar el proyecto.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Genera las instrucciones de compilacion que puede consultar VS Code.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Localiza las bibliotecas para la interfaz, la API y los datos.
# El lector del banco usa Qt Core para abrir archivos y leer JSON.
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network HttpServer)
find_package(mongocxx 4.6 CONFIG REQUIRED)
find_package(bsoncxx 4.6 CONFIG REQUIRED)

# Prepara las herramientas automaticas de Qt que necesite el proyecto.
qt_standard_project_setup()

# Compila el programa principal junto con el modelo y el repositorio.
qt_add_executable(EntrenadorEGEL
    src/main.cpp
    src/modelo/Tipos.hpp
    src/modelo/CalculoResumen.hpp
    src/datos/RepositorioMongoDB.hpp
    src/datos/RepositorioMongoDB.cpp
)

# Permite incluir encabezados partiendo de la carpeta src.
target_include_directories(EntrenadorEGEL PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# Proporciona las bibliotecas y sus rutas de encabezados.
target_link_libraries(EntrenadorEGEL PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::HttpServer
    mongo::mongocxx_shared
    mongo::bsoncxx_shared
)

# Agrega los servicios y las pantallas al mismo ejecutable.
target_sources(EntrenadorEGEL PRIVATE
    # Registra el lector; el .cpp contiene la implementación que se compila.
    src/datos/LectorBancoJson.hpp
    src/datos/LectorBancoJson.cpp
    src/modelo/SeleccionAreas.hpp
    src/negocio/ServicioSeleccionAreas.hpp
    src/negocio/ServicioSeleccionAreas.cpp
    src/presentacion/VentanaSeleccionAreas.hpp
    src/presentacion/VentanaSeleccionAreas.cpp
    src/negocio/ServicioPractica.hpp
    src/negocio/ServicioPractica.cpp
    src/presentacion/VentanaPractica.hpp
    src/presentacion/VentanaPractica.cpp
)

qt_add_resources(EntrenadorEGEL icono_egel
    PREFIX "/"
    FILES recursos/entrenador_egel.png
)