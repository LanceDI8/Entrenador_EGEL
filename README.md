# Entrenador EGEL

Aplicación de escritorio para practicar contenidos de Ingeniería de Software. La versión actual permite seleccionar áreas, consultar el banco, practicar con retroalimentación y guardar respuestas en MongoDB local. La comparación entre métodos y las evaluaciones del piloto siguen previstas.

## Preparación en Windows

Requisitos: CMake 3.25 o posterior, Ninja, Qt 6.12 con MinGW, MongoDB local en ejecución y el controlador C++ de MongoDB 4.6 compatible con el mismo compilador. Instala los módulos de Qt requeridos por CMake, incluido Qt HTTP Server.

Adapta las tres rutas a tu instalación. Usa una carpeta de compilación nueva si cambias de compilador.

```powershell
$mingwEgel = "C:\Qt\Tools\mingw1510_64\mingw64\bin"
$qtEgel = "C:\Qt\6.12.0\mingw_64"
$mongoEgel = "C:\ruta\a\mongocxx"
$env:Path = "$mingwEgel;$qtEgel\bin;$mongoEgel\bin;$env:Path"
cmake -S . -B build/aplicacion -G Ninja "-DCMAKE_CXX_COMPILER=$mingwEgel/g++.exe" "-DCMAKE_PREFIX_PATH=$qtEgel;$mongoEgel" -DCMAKE_BUILD_TYPE=Release
cmake --build build/aplicacion --parallel 2
```

## Validar el banco e iniciar el entrenador

Si tienes la carpeta `banco/`, valida los archivos y cárgalos en MongoDB al instalar el banco por primera vez o al agregar lotes. Para abrir el entrenador con un banco ya cargado, ejecuta solo la última línea.

```powershell
& ".\build\aplicacion\EntrenadorEGEL.exe" --validar-banco ".\banco"
& ".\build\aplicacion\EntrenadorEGEL.exe" --cargar-banco ".\banco"
& ".\build\aplicacion\EntrenadorEGEL.exe" --inicio-estudio
```

El banco de práctica documentado en la Semana 11 contiene 600 reactivos, 150 por área. Las preguntas reservadas para el piloto se mantienen separadas del banco de práctica.

## Comprobar el registro de respuestas

Inicia una sesión de seis reactivos, completa las preguntas y abre el resumen. Cierra el entrenador y abre MongoDB Compass. En la base `entrenador_egel`, localiza la sesión en la colección `sesiones`. Copia su `_id` y úsalo para filtrar la colección `respuestas` por `sesionId` y por la fase `practica`. La consulta debe devolver las seis respuestas de esa sesión.

## Preparar la entrega en Git

Desde la carpeta donde está `CMakeLists.txt`, revisa el estado del repositorio y si tiene un remoto:

```powershell
git status -sb
git remote -v
```

Si la carpeta todavía no es un repositorio, inicialízalo con `git init -b main`. Revisa el `.gitignore` antes de preparar los archivos. Guarda el código fuente, las pruebas, los recursos y la documentación del proyecto. Deja fuera `build/`, las credenciales, los datos locales de MongoDB y la bitácora firmada. Agrega `banco/` únicamente si puedes compartir su contenido.

```powershell
git add -- .gitignore README.md CMakeLists.txt src recursos tests
git diff --cached --name-status
git commit -m "Agrega el prototipo del Entrenador EGEL"
```

Si todavía no hay un remoto, crea un repositorio vacío en GitHub y copia su URL. Si ya existe `origin`, verifica que apunte al repositorio correcto antes de subir el código.

```powershell
git remote add origin "URL_COPIADA_DE_GITHUB"
git push -u origin HEAD
```

Agrega el enlace real al informe y verifica que el evaluador pueda abrirlo. Si el repositorio es privado, necesitará acceso.

Referencia: [Agregar código hospedado localmente a GitHub](https://docs.github.com/es/migrations/importing-source-code/using-the-command-line-to-import-source-code/adding-locally-hosted-code-to-github).