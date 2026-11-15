#pragma once

#include <array>
#include <string>
#include <chrono>
#include <cstdint>

struct Opcion {
    std::string id;
    std::string texto;
    std::string explicacion;
};

struct Reactivo {
    std::string id;
    std::string codigoReactivo;
    int version = 1;

    std::string temaId;
    std::string formato;
    std::string enunciado;

    std::string lenguaje;
    std::string fragmentoCodigo;
    std::string supuestos;

    std::array<Opcion, 3> opciones;
    std::string opcionCorrectaId;

    std::string usoBanco = "practica";
    std::string estadoRevision = "pendiente";
};

// Representa un tema de estudio al que se pueden asociar reactivos.
struct Tema {
    // Identificador asignado por MongoDB al guardar el tema.
    std::string id;

    // Codigo estable para reconocer el tema y evitar duplicarlo.
    std::string codigoTema;

    // Numero del area: 1, 2, 3, 4.
    // El valor 0 indica que todavia no se asignado.
    int area = 0;
    
    std::string subarea;
    std::string nombre;

    // Contenido de apoyo para estudiar el tema.
    std::string explicacion;
    std::string ejemplo;

    // Referencia del material utilizado parar preparar el contenido.
    std::string fuente;

    std::string estadoRevision = "pendiente";
};

// Representa la respuesta de una persona a un reactivo en una sesion.
struct Respuesta
{
    std::string id;

    // Relacionan el intento con la persona y la sesion de practica.
    std::string perfilId;
    std::string sesionId;

    std::string fase = "practica";
    std::string metodoSeleccion;

    // Identifican exactamente la pregunta que se contesto.
    std::string reactivoId;
    std::string codigoReactivo;
    int versionReactivo = 1;

    // Conservan la clasificacion que tenia al momento de responder.
    std::string temaId;
    int area = 0;
    std::string subarea;

    std::string opcionSeleccionadaId;

    // Guarda el orden mostrado, aunque despues mezclemos las opciones.
    std::array<std::string, 3> ordenOpciones;

    // Valores: con_seguridad, con_duda o sin_indicar.
    std::string confianza;

    // La aplicacion calculara estos valores al evaluar la respuesta.
    bool correcta = false;
    int puntos = 0;

    // Momento del envio y tiempo empleado para contestar.
    std::chrono::system_clock::time_point fechaHora{};
    std::int64_t tiempoRespuestaMs = 0;
};

// Totales obtenidos de las respuestas realmente guardadas en la sesion.
struct ResumenPractica
{
    std::string sesionId;

    int totalPreguntas = 0;
    int respondidas = 0;

    int aciertos = 0;
    int errores = 0;
    int puntos = 0;

    int conSeguridad = 0;
    int conDuda = 0;
    int sinIndicar = 0;

    std::int64_t tiempoRespuestaMs = 0;
    bool finalizada = false;
};