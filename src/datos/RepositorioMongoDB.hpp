#pragma once

#include "../modelo/Tipos.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>

class RepositorioMongoDB {
    public:
        explicit RepositorioMongoDB(
            const std::string& uri = 
                "mongodb://127.0.0.1:27017/"
                "?serverSelectionTimeoutMS=5000",
            const std::string& nombreBaseDatos = "entrenador_egel");
        
        bool comprobarConexion();

        // Guarda un tema nuevo o actualiza sus datos usando su código.
        std::string guardarTema(const Tema& tema);

        // Recupera un tema por su identificador asignado por MongoDB.
        std::optional<Tema> buscarTema(
            const std::string& id);

            // Conserva la primera respuesta a un reactivo dentro de una sesion.
        std::string guardarRespuesta(const Respuesta& respuesta);

        // Recupera un intento previamente registrado.
        std::optional<Respuesta> buscarRespuesta(const std::string& id);

        std::string guardarReactivo(const Reactivo& reactivo);

        std::optional<Reactivo> buscarReactivo(
            const std::string& id
        );

        std::vector<Reactivo> listarReactivos(
            const std::string& temaId = ""
        );

        std::int64_t contarReactivos(
            const std::string& temaId = ""
        );

        // Reutiliza el perfil local para conservar su historial.
        std::string obtenerOCrearPerfilLocal();

        // Cada ejecucion de prueba tendra su propia sesion.
        std::string crearSesionPrueba(
        const std::string& perfilId,
        const std::string& reactivoId);

        // Guarda una lista ordenada: todas sus respuestas comparten sesionId.
        std::string crearSesionPractica(
        const std::string& perfilId,
        const std::vector<std::string>& reactivoIds);

        ResumenPractica obtenerResumenSesion(
        const std::string& sesionId);

        ResumenPractica finalizarSesion(
        const std::string& sesionId);
        
        private:
        static mongocxx::client crearCliente(
            const std::string& uri
        );

        mongocxx::client cliente_;
        mongocxx::database baseDatos_;
};