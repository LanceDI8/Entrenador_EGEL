#pragma once

#include "../negocio/ServicioSeleccionAreas.hpp"
#include "VentanaPractica.hpp"

#include <QPointer>
#include <QWidget>
#include <array>
#include <optional>

// Declaraciones adelantadas: aquí basta con conocer los nombres de estos tipos.
class QButtonGroup;
class QCheckBox;
class QLabel;
class QPushButton;
class QTreeWidget;

// Pantalla inicial: elige áreas y permite iniciar una práctica.
class VentanaSeleccionAreas : public QWidget
{
public:
    explicit VentanaSeleccionAreas(
        ServicioSeleccionAreas& seleccion,
        ServicioPractica& practica,
        QWidget* parent = nullptr);

private:
    // Lee la modalidad y las áreas marcadas en los controles.
    SeleccionAreas leerSeleccion() const;

    // En modo individual conserva como máximo una casilla marcada.
    void ajustarModalidad(bool individual);
    // Al cambiar los filtros, descarta la lista anterior y bloquea la práctica.
    void invalidarVistaPrevia();
    // Pide al servicio las preguntas disponibles y actualiza la vista previa.
    void consultarPreguntas();
    // Dibuja los conteos y una muestra pequeña sin recortar la consulta.
    void actualizarArbolPrevia();
    // El índice pertenece al banco consultado, no a la fila del árbol.
    std::optional<std::size_t> indiceEjemploSeleccionado() const;
    // Abre la pregunta de ejemplo que se seleccionó en el árbol.
    void abrirPregunta();
    // Pide la cantidad de preguntas antes de iniciar una sesión.
    void abrirSesion();
    // Muestra la práctica y bloquea el menú hasta que se cierre.
    void mostrarVentana(VentanaPractica* ventana);

    // Servicios prestados: deben existir mientras esta ventana los use.
    ServicioSeleccionAreas& seleccion_;
    ServicioPractica& practica_;

    // Un grupo controla la modalidad y el otro agrupa las casillas de área.
    QButtonGroup* modalidades_ = nullptr;
    QButtonGroup* grupoAreas_ = nullptr;

    // El índice 0 corresponde al área 1, y así sucesivamente.
    std::array<QCheckBox*, 4> casillas_{};

    // La vista previa agrupa las preguntas por área y subárea.
    QLabel* resumen_ = nullptr;
    QTreeWidget* preguntas_ = nullptr;

    // Botones para consultar, practicar una pregunta o iniciar una sesión.
    QPushButton* consultar_ = nullptr;
    QPushButton* abrir_ = nullptr;
    QPushButton* iniciarSesion_ = nullptr;

    // Recuerda la ultima cantidad aceptada para esta vista previa.
    int cantidadElegida_ = 1;

    // Guarda el resultado de la consulta; la bandera indica si sigue vigente.
    VistaPreviaAreas vista_;
    bool vistaVigente_ = false;

    // Qt limpia este puntero al destruir la ventana de practica.
    QPointer<VentanaPractica> practicaAbierta_;
};
