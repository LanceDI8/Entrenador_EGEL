#pragma once

#include "../modelo/Tipos.hpp"

#include <string>
#include <vector>

// Agrupa la ficha de un tema con todas las preguntas que le pertenecen.
struct TemaBanco
{
    Tema tema;
    std::vector<Reactivo> reactivos;
};

// Lee los cuatro JSON base y los archivos opcionales de la subcarpeta lotes.
// Valida todo antes de conectarse a MongoDB.
std::vector<TemaBanco> leerBancoJson(const std::string& carpeta);