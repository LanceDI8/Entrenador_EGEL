#pragma once

#include "../negocio/ServicioPractica.hpp"

#include <QWidget>
#include <string>
#include <vector>

class QButtonGroup;
class QComboBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QScrollArea;

// Presenta las preguntas; el servicio controla el avance y el registro.
class VentanaPractica : public QWidget
{
public:
    explicit VentanaPractica(
        ServicioPractica& servicio,
        const std::string& reactivoId,
        QWidget* parent = nullptr);

    explicit VentanaPractica(
        ServicioPractica& servicio,
        const std::vector<std::string>& reactivoIds,
        QWidget* parent = nullptr);

private:
    void construirVentana();
    void mostrarPregunta();
    void registrarRespuesta();
    void continuar();
    void mostrarResumen(const ResumenPractica& resumen);

    // El servicio debe existir mientras la ventana este abierta.
    ServicioPractica& servicio_;
    Reactivo reactivo_;

    bool modoSesion_ = false;
    bool respondida_ = false;

    QLabel* progreso_ = nullptr;
    
    // Recuadro que indica el area de la pregunta actual
    QLabel* areaActual_ = nullptr;
    
    QScrollArea* area_ = nullptr;
    QButtonGroup* opciones_ = nullptr;
    QGroupBox* cajaOpciones_ = nullptr;
    QComboBox* confianza_ = nullptr;

    QPushButton* responder_ = nullptr;
    QPushButton* siguiente_ = nullptr;
    QPushButton* cerrar_ = nullptr;

    QLabel* resultado_ = nullptr;
};