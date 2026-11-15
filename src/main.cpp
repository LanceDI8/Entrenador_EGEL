// El lector convierte los JSON del banco en temas y preguntas del proyecto.
#include "datos/LectorBancoJson.hpp"
#include "datos/RepositorioMongoDB.hpp"
#include "negocio/ServicioPractica.hpp"
#include "negocio/ServicioSeleccionAreas.hpp"
#include "presentacion/VentanaPractica.hpp"
#include "presentacion/VentanaSeleccionAreas.hpp"

#include <QApplication>
#include <QIcon>

#include <array>
#include <chrono>
#include <exception>
#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
    try
    {
        // Sin argumentos, realiza la comprobacion de la base principal.
        if (argc == 1)
        {
            RepositorioMongoDB repositorio;

            if (!repositorio.comprobarConexion())
            {
                std::cerr << "No fue posible comprobar la conexion.\n";
                return 1;
            }

            std::cout << "Conexion con MongoDB correcta.\n";
            std::cout << "Documentos de reactivos guardados: "
                      << repositorio.contarReactivos() << '\n';
            return 0;
        }

        // argc incluye el nombre del programa; argv contiene sus argumentos.
        const std::string comando = argv[1];

        const bool guardar = argc == 2 && comando == "--guardar-tema";
        const bool cargarPruebas = argc == 2 && comando == "--cargar-pruebas-sesion";
        // Los dos comandos del banco pueden recibir una carpeta como segundo argumento.
        const bool aceptaCarpeta = argc == 2 || argc == 3;
        const bool validarBanco = aceptaCarpeta && comando == "--validar-banco";
        const bool cargarBanco = aceptaCarpeta && comando == "--cargar-banco";
        const bool buscar = argc == 3 && comando == "--buscar-tema";

        const bool guardarReactivo =
            argc == 3 && comando == "--guardar-reactivo";

        const bool buscarReactivo =
            argc == 3 && comando == "--buscar-reactivo";
        const bool practicar =
            argc == 3 && comando == "--practicar";

        const bool inicio =
            argc == 2 && comando == "--inicio";
        const bool inicioEstudio =
            argc == 2 && comando == "--inicio-estudio";

        const bool practicarGui =
            argc == 3 && comando == "--practicar-gui";

        // Solo aceptamos comandos cuya cantidad de argumentos sea correcta.
        if (!guardar && !buscar && !guardarReactivo &&
            !buscarReactivo && !practicar && !practicarGui && !inicio && !cargarPruebas &&
            !inicioEstudio && !validarBanco && !cargarBanco)
        {
            std::cerr << "Comandos disponibles:\n"
                      << "  --inicio\n"
                      << "  --inicio-estudio\n"
                      << "  --validar-banco [CARPETA]\n"
                      << "  --cargar-banco [CARPETA]\n"
                      << "  --cargar-pruebas-sesion\n"
                      << "  --guardar-tema\n"
                      << "  --buscar-tema ID_TEMA\n"
                      << "  --guardar-reactivo ID_TEMA\n"
                      << "  --buscar-reactivo ID_REACTIVO\n"
                      << "  --practicar ID_REACTIVO\n"
                      << "  --practicar-gui ID_REACTIVO\n";
            return 1;
        }

        // Ambos comandos revisan los archivos. Solo cargar-banco abre el repositorio.
        if (validarBanco || cargarBanco)
        {
            // Si no se indica una carpeta, busca banco/ en el directorio actual.
            auto banco = leerBancoJson(argc == 3 ? argv[2] : "banco");

            if (cargarBanco)
            {
                // El banco de estudio se guarda en la base principal.
                RepositorioMongoDB repositorio;
                for (auto& grupo : banco)
                {
                    // MongoDB asigna el identificador que relaciona tema y reactivos.
                    const auto temaId = repositorio.guardarTema(grupo.tema);
                    for (auto& reactivo : grupo.reactivos)
                    {
                        reactivo.temaId = temaId;
                        repositorio.guardarReactivo(reactivo);
                    }
                }
            }

            // El área 1 corresponde al índice 0; el conteo es de los JSON leídos.
            std::array<std::size_t, 4> porArea{};
            std::size_t total = 0;
            for (const auto& grupo : banco)
            {
                porArea[static_cast<std::size_t>(grupo.tema.area - 1)] += grupo.reactivos.size();
                total += grupo.reactivos.size();
            }

            std::cout << (cargarBanco ? "Banco cargado en entrenador_egel.\n" : "Banco valido.\n");
            for (std::size_t i = 0; i < porArea.size(); ++i)
            {
                std::cout << "Area " << i + 1 << ": " << porArea[i] << '\n';
            }
            std::cout << "Total en los archivos: " << total << '\n';
            return 0;
        }

        // La ventana de estudio consulta la misma base que recibe el banco.
        // Los comandos anteriores de prueba conservan su propia base.
        RepositorioMongoDB repositorio{
            "mongodb://127.0.0.1:27017/?serverSelectionTimeoutMS=5000",
            inicioEstudio ? "entrenador_egel" : "entrenador_egel_pruebas"
        };

        if (cargarPruebas)
        {
            // Ejemplos tecnicos para probar el recorrido; no son contenido del EGEL.
            // Los codigos propios permiten repetir la carga sin duplicar preguntas.
            for (int i = 0; i < 3; ++i)
            {
                Tema tema;
                tema.codigoTema = i == 0 ? "DEMO-SESION-TEMA-AREA1" : "DEMO-SESION-TEMA-AREA3";
                tema.area = i == 0 ? 1 : 3;
                tema.subarea = "Pruebas de funcionamiento";
                tema.nombre = "Demostracion tecnica de sesiones";
                tema.explicacion = "Datos de prueba del recorrido de varias preguntas.";

                Reactivo reactivo;
                reactivo.temaId = repositorio.guardarTema(tema);
                reactivo.codigoReactivo = "DEMO-SESION-00" + std::to_string(i + 1);
                reactivo.formato = "caso_practico";
                reactivo.enunciado = "Prueba de sesion " + std::to_string(i + 1) +
                    ": selecciona la opcion " + std::string(1, static_cast<char>('A' + i)) + ".";
                reactivo.opcionCorrectaId = std::string(1, static_cast<char>('A' + i));
                reactivo.opciones = {
                    Opcion{"A", "Opcion tecnica A", "Alternativa A de demostracion."},
                    Opcion{"B", "Opcion tecnica B", "Alternativa B de demostracion."},
                    Opcion{"C", "Opcion tecnica C", "Alternativa C de demostracion."}
                };
                repositorio.guardarReactivo(reactivo);
                std::cout << "Preparada " << reactivo.codigoReactivo << " (area " << tema.area << ").\n";
            }
            return 0;
        }

        // Las dos entradas comparten la ventana; cambia la base que consulta.
        if (inicio || inicioEstudio)
        {
            QApplication aplicacion{argc, argv};

            QApplication::setWindowIcon(
                QIcon(QStringLiteral(":/recursos/entrenador_egel.png")));

            // El menu y la ventana de pregunta comparten estos servicios.
            // Permanecen vivos hasta que se cierre la aplicacion.
            ServicioSeleccionAreas seleccion{repositorio};
            ServicioPractica practica{repositorio};
            VentanaSeleccionAreas ventana{seleccion, practica};
            ventana.show();

            return aplicacion.exec();
        }

        if (practicarGui)
        {
            // Copia el ID antes de que Qt procese los argumentos que reconoce.
            const std::string reactivoId = argv[2];

            // QApplication prepara Qt antes de crear cualquier ventana.
            QApplication aplicacion{argc, argv};
            // La ventana usa este servicio, que a su vez usa el repositorio.
            // Ambos permanecen vivos durante aplicacion.exec().
            ServicioPractica servicio{repositorio};
            VentanaPractica ventana{servicio, reactivoId};
            ventana.show();

            // Atiende los botones y mantiene la ventana abierta.
            return aplicacion.exec();
        }

        if (guardar)
        {
            Tema tema;
            tema.codigoTema = "TEMA-PRUEBA-001";
            tema.area = 3;
            tema.subarea = "Prueba tecnica";
            tema.nombre = "Tema de prueba";
            tema.explicacion = "Verifica el guardado y la lectura en MongoDB.";

            // Imprime solo el ID para que PowerShell pueda guardarlo.
            std::cout << repositorio.guardarTema(tema) << '\n';
            return 0;
        }

        if (guardarReactivo)
        {
            // Primero verificamos que el tema exista en la base de pruebas.
            const auto tema = repositorio.buscarTema(argv[2]);

            if (!tema)
            {
                std::cerr << "No se encontro el tema indicado.\n";
                return 1;
            }

            Reactivo reactivo;
            reactivo.codigoReactivo = "REACTIVO-PRUEBA-001";
            reactivo.temaId = tema->id;
            reactivo.formato = "caso_practico";
            reactivo.enunciado = "Reactivo para verificar el almacenamiento.";

            reactivo.opciones = {
                Opcion{"A", "Opcion de prueba A", "Marcada como correcta."},
                Opcion{"B", "Opcion de prueba B", "Distractor de prueba."},
                Opcion{"C", "Opcion de prueba C", "Distractor de prueba."}
            };
            reactivo.opcionCorrectaId = "A";

            // Imprime solo el ID para que PowerShell pueda capturarlo.
            std::cout << repositorio.guardarReactivo(reactivo) << '\n';
            return 0;
        }

        if (buscarReactivo)
        {
            const auto reactivo = repositorio.buscarReactivo(argv[2]);

            if (!reactivo)
            {
                std::cerr << "No se encontro el reactivo indicado.\n";
                return 1;
            }

            std::cout << "Reactivo recuperado: "
                      << reactivo->codigoReactivo << '\n';
            std::cout << "ID del tema asociado: "
                      << reactivo->temaId << '\n';
            std::cout << "Opciones recuperadas:\n";

            for (const auto& opcion : reactivo->opciones)
            {
                std::cout << "  " << opcion.id
                          << ". " << opcion.texto << '\n';
            }

            return 0;
        }

        if (practicar)
        {
            // El servicio prepara la pregunta y su sesion de practica.
            ServicioPractica servicio{repositorio};
            const auto reactivo = servicio.iniciarPrueba(argv[2]);

            std::cout << reactivo.enunciado << "\n\n";

            // Muestra el fragmento cuando la pregunta incluye codigo.
            if (!reactivo.fragmentoCodigo.empty())
            {
                std::cout << reactivo.fragmentoCodigo << "\n\n";
            }

            for (const auto& opcion : reactivo.opciones)
            {
                std::cout << opcion.id << ". " << opcion.texto << '\n';
            }

            std::cout << "\nElige una opcion (A, B o C): ";
            std::string eleccion;

            if (!std::getline(std::cin, eleccion))
            {
                return 1;
            }

            bool opcionValida = false;

            for (const auto& opcion : reactivo.opciones)
            {
                if (opcion.id == eleccion)
                {
                    opcionValida = true;
                    break;
                }
            }

            if (!opcionValida)
            {
                std::cerr << "Opcion invalida. Escribe A, B o C.\n";
                return 1;
            }

            // La confianza es opcional; Enter conserva el valor sin_indicar.
            std::cout << "\nComo respondiste? (opcional)\n"
                      << "1. Con seguridad\n"
                      << "2. Con duda\n"
                      << "Elige 1 o 2, o pulsa Enter para omitir: ";

            std::string confianza;

            if (!std::getline(std::cin, confianza) ||
                (!confianza.empty() && confianza != "1" && confianza != "2"))
            {
                std::cerr << "Confianza invalida.\n";
                return 1;
            }

            // El servicio prepara el intento y obtiene el resultado registrado.
            const auto resultado = servicio.responder(
                eleccion,
                confianza == "1" ? "con_seguridad" :
                confianza == "2" ? "con_duda" : "sin_indicar"
            );

            // Presenta el resultado que se recupero de MongoDB.
            std::cout << "\nRespuesta guardada. ID: "
                      << resultado.id << '\n';

            std::cout << (resultado.correcta
                ? "Correcto.\n"
                : "Incorrecto.\n");

            for (const auto& opcion : reactivo.opciones)
            {
                if (opcion.id == resultado.opcionSeleccionadaId)
                {
                    std::cout << "Explicacion: "
                              << opcion.explicacion << '\n';
                }
            }

            std::cout << "Puntos: " << resultado.puntos << '\n';
            std::cout << "Confianza: " << resultado.confianza << '\n';

            if (!resultado.correcta)
            {
                std::cout << "La opcion correcta era: "
                          << reactivo.opcionCorrectaId << '\n';
            }

            return 0;
        }

        // Esta ejecucion solo consulta el tema guardado anteriormente.
        const auto tema = repositorio.buscarTema(argv[2]);

        if (!tema)
        {
            std::cerr << "No se encontro un tema con ese ID.\n";
            return 1;
        }

        std::cout << "Tema recuperado: " << tema->nombre << '\n';
        std::cout << "Codigo: " << tema->codigoTema << '\n';
        std::cout << "Area: " << tema->area << '\n';
        return 0;
    }
    // Los errores de lectura o de conexión se muestran en la terminal.
    catch (const std::exception& error)
    {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}