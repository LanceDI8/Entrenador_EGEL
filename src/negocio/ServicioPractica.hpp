#pragma once

#include "../datos/RepositorioMongoDB.hpp"

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

// Coordina las preguntas; la evaluacion y la persistencia son del repositorio.
class ServicioPractica
{
public:
    explicit ServicioPractica(RepositorioMongoDB& repositorio);

    Reactivo iniciarPrueba(const std::string& reactivoId);

    Reactivo iniciarSesion(
        const std::vector<std::string>& reactivoIds);

    Respuesta responder(
        const std::string& opcionId,
        const std::string& confianza);

    Reactivo avanzar();
    ResumenPractica finalizarSesion();

    std::size_t numeroPregunta() const;
    std::size_t totalPreguntas() const;
    bool esUltima() const;

    // Permiten que la ventana muestre el area y anuncie sus cambios.
    int areaPreguntaActual() const;
    bool esCambioDeArea() const;

private:
    // Cada pregunta conserva el area del tema al que pertenece.
    struct PreguntaDePractica
    {
        Reactivo reactivo;
        int area;
    };

    void prepararPregunta();
    Respuesta recuperarRespuestaGuardada();

    RepositorioMongoDB& repositorio_;

    std::vector<PreguntaDePractica> preguntas_;
    std::size_t posicion_ = 0;

    std::string perfilId_;
    std::string sesionId_;
    std::string metodoSeleccion_;
    std::string respuestaId_;

    bool sesionMultiple_ = false;
    bool finalizada_ = false;

    // Un fallo de conexion no permite cambiar el intento que ya se envio.
    std::optional<Respuesta> intentoPendiente_;
    std::optional<Respuesta> respuestaConfirmada_;

    std::chrono::steady_clock::time_point inicio_{};
};