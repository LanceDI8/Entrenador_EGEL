#pragma once

#include "../datos/RepositorioMongoDB.hpp"
#include "../modelo/SeleccionAreas.hpp"
#include <cstddef>

// Valida las areas y consulta el catalogo de preguntas de practica.
class ServicioSeleccionAreas
{
public:
    explicit ServicioSeleccionAreas(RepositorioMongoDB& repositorio);

    static void validarSeleccion(const SeleccionAreas& seleccion);
    static const std::array<std::string, 4>& nombresAreas();

    VistaPreviaAreas consultar(const SeleccionAreas& seleccion);

    // El máximo común se obtiene del área seleccionada con menos preguntas.
    static std::size_t limitePorArea(const VistaPreviaAreas& vista);

    // Sortea la misma cantidad por área y entrega los bloques en orden.
    static std::vector<std::string> prepararSesion(
        const VistaPreviaAreas& vista, std::size_t cantidadPorArea);

private:
    RepositorioMongoDB& repositorio_;
};
