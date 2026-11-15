#pragma once

#include "Tipos.hpp"
#include "SeleccionAreas.hpp"
#include <limits>
#include <set>
#include <stdexcept>
#include <vector>

// Calcula los totales sin depender de Qt ni de una conexion con MongoDB.
inline ResumenPractica calcularResumenPractica(
    const std::string& sesionId,
    const std::vector<std::string>& asignadas,
    const std::vector<Respuesta>& respuestas,
    bool finalizada)
{
    const std::set<std::string> esperadas(asignadas.begin(), asignadas.end());
    if (asignadas.empty() || asignadas.size() > MaxPreguntasPorSesion ||
        esperadas.size() != asignadas.size())
    {
        throw std::runtime_error("La lista de preguntas de la sesion no es valida.");
    }

    ResumenPractica resumen;
    resumen.sesionId = sesionId;
    resumen.totalPreguntas = static_cast<int>(asignadas.size());
    resumen.finalizada = finalizada;
    std::set<std::string> contadas;
    for (const auto& respuesta : respuestas)
    {
        // Rechaza registros ajenos o repetidos en lugar de inflar los totales.
        if (respuesta.sesionId != sesionId || respuesta.fase != "practica" ||
            esperadas.count(respuesta.reactivoId) == 0 ||
            !contadas.insert(respuesta.reactivoId).second ||
            respuesta.metodoSeleccion != "catalogo_por_areas" ||
            respuesta.puntos != (respuesta.correcta ? 1 : 0) ||
            respuesta.tiempoRespuestaMs < 0 ||
            (respuesta.confianza != "con_seguridad" &&
             respuesta.confianza != "con_duda" &&
             respuesta.confianza != "sin_indicar"))
        {
            throw std::runtime_error("Hay una respuesta que no coincide con la sesion.");
        }
        if (respuesta.tiempoRespuestaMs >
            std::numeric_limits<std::int64_t>::max() - resumen.tiempoRespuestaMs)
        {
            throw std::runtime_error("El tiempo acumulado no es valido.");
        }
        ++resumen.respondidas;
        if (respuesta.correcta) ++resumen.aciertos;
        else ++resumen.errores;
        if (respuesta.confianza == "con_seguridad") ++resumen.conSeguridad;
        else if (respuesta.confianza == "con_duda") ++resumen.conDuda;
        else ++resumen.sinIndicar;
        resumen.puntos += respuesta.puntos;
        resumen.tiempoRespuestaMs += respuesta.tiempoRespuestaMs;
    }
    if (finalizada && resumen.respondidas != resumen.totalPreguntas)
    {
        throw std::runtime_error("La sesion figura como finalizada pero faltan respuestas.");
    }
    return resumen;
}
