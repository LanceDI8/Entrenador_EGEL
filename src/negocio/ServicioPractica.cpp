#include "ServicioPractica.hpp"
#include "../modelo/SeleccionAreas.hpp"

#include <set>
#include <stdexcept>
#include <utility>

ServicioPractica::ServicioPractica(RepositorioMongoDB& repositorio)
    : repositorio_(repositorio)
{
}

Reactivo ServicioPractica::iniciarPrueba(const std::string& reactivoId)
{
    const auto reactivo = repositorio_.buscarReactivo(reactivoId);
    const auto tema = reactivo ? repositorio_.buscarTema(reactivo->temaId) : std::nullopt;
    if (!reactivo || !tema)
    {
        throw std::invalid_argument("La pregunta o su tema no existen.");
    }

    const auto perfil = repositorio_.obtenerOCrearPerfilLocal();
    const auto sesion = repositorio_.crearSesionPrueba(perfil, reactivo->id);
    preguntas_ = {{*reactivo, tema->area}};
    perfilId_ = perfil;
    sesionId_ = sesion;
    metodoSeleccion_ = "prueba_tecnica";
    sesionMultiple_ = false;
    finalizada_ = false;
    posicion_ = 0;
    prepararPregunta();
    return preguntas_.front().reactivo;
}

Reactivo ServicioPractica::iniciarSesion(
    const std::vector<std::string>& reactivoIds)
{
    if (reactivoIds.empty() || reactivoIds.size() > MaxPreguntasPorSesion)
    {
        throw std::invalid_argument("La sesion necesita de 1 a " +
            std::to_string(MaxPreguntasPorSesion) + " preguntas.");
    }

    std::vector<PreguntaDePractica> preguntas;
    std::set<std::string> codigos;
    for (const auto& id : reactivoIds)
    {
        const auto reactivo = repositorio_.buscarReactivo(id);
        if (!reactivo || reactivo->usoBanco != "practica")
        {
            throw std::invalid_argument("Una pregunta no esta disponible para practicar.");
        }
        const auto tema = repositorio_.buscarTema(reactivo->temaId);
        if (!tema)
        {
            throw std::invalid_argument("Una pregunta no esta disponible para practicar.");
        }
        if (!codigos.insert(reactivo->codigoReactivo).second)
        {
            throw std::invalid_argument("Una sesion no puede repetir un codigo de pregunta.");
        }
        preguntas.push_back({*reactivo, tema->area});
    }

    // Verifica todo antes de crear la sesion y reemplazar el estado anterior.
    const auto perfil = repositorio_.obtenerOCrearPerfilLocal();
    const auto sesion = repositorio_.crearSesionPractica(perfil, reactivoIds);
    preguntas_ = std::move(preguntas);
    perfilId_ = perfil;
    sesionId_ = sesion;
    metodoSeleccion_ = "catalogo_por_areas";
    sesionMultiple_ = true;
    finalizada_ = false;
    posicion_ = 0;
    prepararPregunta();
    return preguntas_.front().reactivo;
}

void ServicioPractica::prepararPregunta()
{
    respuestaId_.clear();
    intentoPendiente_.reset();
    respuestaConfirmada_.reset();
    inicio_ = std::chrono::steady_clock::now();
}

Respuesta ServicioPractica::responder(
    const std::string& opcionId, const std::string& confianza)
{
    if (preguntas_.empty())
    {
        throw std::logic_error("Primero debes iniciar una practica.");
    }
    if (!respuestaId_.empty()) return recuperarRespuestaGuardada();

    if (!intentoPendiente_)
    {
        const auto& reactivo = preguntas_.at(posicion_).reactivo;
        bool valida = false;
        for (const auto& opcion : reactivo.opciones)
        {
            if (opcion.id == opcionId) valida = true;
        }
        if (!valida ||
            (!confianza.empty() && confianza != "con_seguridad" &&
             confianza != "con_duda" && confianza != "sin_indicar"))
        {
            throw std::invalid_argument("La opcion o la confianza no son validas.");
        }

        Respuesta intento;
        intento.perfilId = perfilId_;
        intento.sesionId = sesionId_;
        intento.reactivoId = reactivo.id;
        intento.fase = "practica";
        intento.metodoSeleccion = metodoSeleccion_;
        intento.opcionSeleccionadaId = opcionId;
        // Omitir la confianza no significa haber respondido con duda.
        intento.confianza = confianza.empty() ? "sin_indicar" : confianza;
        for (std::size_t i = 0; i < reactivo.opciones.size(); ++i)
        {
            intento.ordenOpciones[i] = reactivo.opciones[i].id;
        }

        // Mide hasta el primer envio; los reintentos no aumentan este tiempo.
        intento.tiempoRespuestaMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - inicio_).count();
        intentoPendiente_ = intento;
    }

    respuestaId_ = repositorio_.guardarRespuesta(*intentoPendiente_);
    return recuperarRespuestaGuardada();
}

Respuesta ServicioPractica::recuperarRespuestaGuardada()
{
    const auto respuesta = repositorio_.buscarRespuesta(respuestaId_);
    if (!respuesta || respuesta->sesionId != sesionId_ ||
        respuesta->perfilId != perfilId_ ||
        respuesta->reactivoId != preguntas_.at(posicion_).reactivo.id)
    {
        throw std::runtime_error("No fue posible confirmar la respuesta registrada.");
    }
    respuestaConfirmada_ = *respuesta;
    return *respuesta;
}

Reactivo ServicioPractica::avanzar()
{
    if (!sesionMultiple_ || !respuestaConfirmada_ || esUltima() || finalizada_)
    {
        throw std::logic_error("Confirma la respuesta actual antes de avanzar.");
    }
    ++posicion_;
    prepararPregunta();
    return preguntas_.at(posicion_).reactivo;
}

ResumenPractica ServicioPractica::finalizarSesion()
{
    if (!sesionMultiple_ || !esUltima() || !respuestaConfirmada_)
    {
        throw std::logic_error("Debes responder todas las preguntas antes del resumen.");
    }
    // El repositorio comprueba las respuestas y permite repetir esta consulta.
    const auto resumen = repositorio_.finalizarSesion(sesionId_);
    finalizada_ = resumen.finalizada;
    return resumen;
}

std::size_t ServicioPractica::numeroPregunta() const
{
    return preguntas_.empty() ? 0 : posicion_ + 1;
}

std::size_t ServicioPractica::totalPreguntas() const
{
    return preguntas_.size();
}

bool ServicioPractica::esUltima() const
{
    return !preguntas_.empty() && posicion_ + 1 == preguntas_.size();
}

int ServicioPractica::areaPreguntaActual() const
{
    return preguntas_.empty() ? 0 : preguntas_[posicion_].area;
}

bool ServicioPractica::esCambioDeArea() const
{
    // La primera pregunta presenta su área, pero todavía no hay un cambio.
    return posicion_ > 0 &&
        preguntas_[posicion_ - 1].area != areaPreguntaActual();
}
