#include "ServicioSeleccionAreas.hpp"

#include <algorithm>
#include <map>
#include <random>
#include <set>
#include <stdexcept>

ServicioSeleccionAreas::ServicioSeleccionAreas(
    RepositorioMongoDB& repositorio)
    : repositorio_(repositorio)
{
}

const std::array<std::string, 4>& ServicioSeleccionAreas::nombresAreas()
{
    static const std::array<std::string, 4> nombres{
        u8"Análisis de sistemas de software",
        u8"Diseño de sistemas de software",
        "Desarrollo de sistemas de software",
        u8"Gestión de proyectos de software"
    };

    return nombres;
}

void ServicioSeleccionAreas::validarSeleccion(
    const SeleccionAreas& seleccion)
{
    if (seleccion.modalidad != ModalidadAreas::individual &&
        seleccion.modalidad != ModalidadAreas::varias)
    {
        throw std::invalid_argument("La modalidad no es valida.");
    }

    std::set<int> unicas;

    for (const int area : seleccion.areas)
    {
        if (area < 1 || area > 4)
        {
            throw std::invalid_argument("Las areas deben estar entre 1 y 4.");
        }

        if (!unicas.insert(area).second)
        {
            throw std::invalid_argument("Una misma area no debe repetirse.");
        }
    }

    if (seleccion.modalidad == ModalidadAreas::individual &&
        unicas.size() != 1)
    {
        throw std::invalid_argument("Selecciona exactamente un area.");
    }

    if (seleccion.modalidad == ModalidadAreas::varias &&
        (unicas.size() < 2 || unicas.size() > 4))
    {
        throw std::invalid_argument("Selecciona entre dos y cuatro areas.");
    }
}

VistaPreviaAreas ServicioSeleccionAreas::consultar(
    const SeleccionAreas& seleccion)
{
    validarSeleccion(seleccion);

    VistaPreviaAreas vista;
    vista.seleccion = seleccion;
    std::sort(vista.seleccion.areas.begin(), vista.seleccion.areas.end());
    const std::set<int> areas(seleccion.areas.begin(), seleccion.areas.end());

    // El repositorio conserva versiones; el catalogo toma la mas reciente
    // de cada codigo para evitar listar varias veces una misma pregunta.
    std::map<std::string, Reactivo> recientes;

    for (const auto& reactivo : repositorio_.listarReactivos())
    {
        const auto encontrado = recientes.find(reactivo.codigoReactivo);

        if (encontrado == recientes.end() ||
            reactivo.version > encontrado->second.version)
        {
            recientes[reactivo.codigoReactivo] = reactivo;
        }
    }

    // Reutiliza los temas ya consultados al clasificar varias preguntas.
    std::map<std::string, Tema> temas;

    for (const auto& entrada : recientes)
    {
        const auto& reactivo = entrada.second;

        if (reactivo.usoBanco != "practica")
        {
            continue;
        }

        auto tema = temas.find(reactivo.temaId);

        if (tema == temas.end())
        {
            const auto encontrado = repositorio_.buscarTema(reactivo.temaId);

            if (!encontrado)
            {
                throw std::runtime_error(
                    "No se encontro el tema de una pregunta del banco.");
            }

            tema = temas.emplace(reactivo.temaId, *encontrado).first;
        }

        if (areas.count(tema->second.area) == 0)
        {
            continue;
        }

        const auto indice = static_cast<std::size_t>(tema->second.area - 1);
        ++vista.preguntasPorArea[indice];
        vista.preguntas.push_back(PreguntaDisponible{
            reactivo.id,
            tema->second.area,
            tema->second.nombre,
            reactivo.enunciado,
            tema->second.subarea
        });
    }

    std::sort(vista.preguntas.begin(), vista.preguntas.end(),
        [](const PreguntaDisponible& a, const PreguntaDisponible& b)
        {
            if (a.area != b.area) return a.area < b.area;
            if (a.nombreTema != b.nombreTema) return a.nombreTema < b.nombreTema;
            if (a.enunciado != b.enunciado) return a.enunciado < b.enunciado;
            return a.reactivoId < b.reactivoId;
        });

    return vista;
}

std::size_t ServicioSeleccionAreas::limitePorArea(const VistaPreviaAreas& vista)
{
    validarSeleccion(vista.seleccion);
    auto limite = static_cast<std::int64_t>(MaxPreguntasPorArea);
    for (const int area : vista.seleccion.areas)
    {
        const auto indice = static_cast<std::size_t>(area - 1);
        limite = std::min(limite, vista.preguntasPorArea[indice]);
    }
    return static_cast<std::size_t>(std::max<std::int64_t>(0, limite));
}

std::vector<std::string> ServicioSeleccionAreas::prepararSesion(
    const VistaPreviaAreas& vista, std::size_t cantidadPorArea)
{
    validarSeleccion(vista.seleccion);
    if (cantidadPorArea == 0 || cantidadPorArea > MaxPreguntasPorArea)
    {
        throw std::invalid_argument("Elige de 1 a " +
            std::to_string(MaxPreguntasPorArea) + " preguntas por area.");
    }
    const std::set<int> elegidas(vista.seleccion.areas.begin(), vista.seleccion.areas.end());
    std::array<std::vector<std::string>, MaxAreasSesion> grupos;
    std::set<std::string> unicas;
    for (const auto& pregunta : vista.preguntas)
    {
        if (elegidas.count(pregunta.area) == 0 || pregunta.reactivoId.empty() ||
            !unicas.insert(pregunta.reactivoId).second)
        {
            throw std::invalid_argument("La vista previa contiene una pregunta no valida.");
        }
        grupos[static_cast<std::size_t>(pregunta.area - 1)].push_back(pregunta.reactivoId);
    }

    for (const int area : elegidas)
    {
        if (grupos[static_cast<std::size_t>(area - 1)].size() < cantidadPorArea)
        {
            throw std::invalid_argument("El area " + std::to_string(area) +
                " no tiene suficientes preguntas para esa cantidad.");
        }
    }

    // Conserva el orden de las áreas, con sorteo independiente dentro de cada bloque.
    std::mt19937 generador(std::random_device{}());
    std::vector<std::string> ids;
    ids.reserve(cantidadPorArea * elegidas.size());
    for (const int area : elegidas)
    {
        auto& grupo = grupos[static_cast<std::size_t>(area - 1)];
        std::shuffle(grupo.begin(), grupo.end(), generador);
        for (std::size_t i = 0; i < cantidadPorArea; ++i)
        {
            ids.push_back(grupo[i]);
        }
    }
    return ids;
}
