#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// La cantidad del selector se aplica por área, no a toda la sesión.
inline constexpr std::size_t MaxPreguntasPorArea = 30;
inline constexpr std::size_t MaxAreasSesion = 4;
inline constexpr std::size_t MaxPreguntasPorSesion =
    MaxPreguntasPorArea * MaxAreasSesion;

enum class ModalidadAreas
{
    individual,
    varias
};

// Describe el filtro elegido en la pantalla inicial.
struct SeleccionAreas
{
    ModalidadAreas modalidad = ModalidadAreas::individual;
    std::vector<int> areas;
};

// Datos necesarios para listar una pregunta antes de abrirla.
// El identificador se utiliza internamente; la interfaz no lo muestra.
struct PreguntaDisponible
{
    std::string reactivoId;
    int area = 0;
    std::string nombreTema;
    std::string enunciado;
    std::string subarea;
};

struct VistaPreviaAreas
{
    SeleccionAreas seleccion;
    std::array<std::int64_t, MaxAreasSesion> preguntasPorArea{};
    std::vector<PreguntaDisponible> preguntas;
};
