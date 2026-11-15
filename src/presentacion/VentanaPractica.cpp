#include "VentanaPractica.hpp"
#include "../negocio/ServicioSeleccionAreas.hpp"

#include <QButtonGroup>
#include <QComboBox>
#include <QCursor>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QString>
#include <QTimer>
#include <QToolButton>
#include <QVariant>
#include <QVBoxLayout>

#include <algorithm>
#include <exception>

namespace
{
    constexpr int ZoomInicial = 110;
    constexpr int ZoomMinimo = 90;
    constexpr int ZoomMaximo = 170;
    constexpr int PasoZoom = 10;

    // La zona conserva el tamaño de lectura aunque se reemplace la pregunta.
    class ZonaLectura final : public QScrollArea
    {
    public:
        explicit ZonaLectura(QWidget* parent)
            : QScrollArea(parent),
              puntosBase_(QFontInfo(font()).pointSizeF())
        {
            setWidgetResizable(true);
            fijarZoom(ZoomInicial);
        }

        int zoom() const { return zoom_; }

        void fijarZoom(int porcentaje)
        {
            zoom_ = std::clamp(porcentaje, ZoomMinimo, ZoomMaximo);
            // Solo fija el tamaño: cada control conserva su tipo de letra.
            QFont fuente;
            fuente.setPointSizeF(puntosBase_ * zoom_ / 100.0);
            setFont(fuente);
        }

    private:
        qreal puntosBase_;
        int zoom_ = ZoomInicial;
    };

    QHBoxLayout* crearControlesLectura(ZonaLectura* lectura, QWidget* parent)
    {
        auto* fila = new QHBoxLayout;
        auto* menos = new QToolButton(parent);
        auto* porcentaje = new QToolButton(parent);
        auto* mas = new QToolButton(parent);

        menos->setText(QStringLiteral("A−"));
        menos->setAccessibleName(QStringLiteral("Reducir tamaño del texto"));
        menos->setToolTip(menos->accessibleName());
        mas->setText(QStringLiteral("A+"));
        mas->setAccessibleName(QStringLiteral("Aumentar tamaño del texto"));
        mas->setToolTip(mas->accessibleName());
        porcentaje->setMinimumWidth(60);
        porcentaje->setToolTip(QStringLiteral("Restablecer el tamaño inicial (%1 %)")
            .arg(ZoomInicial));

        const auto mostrarZoom = [lectura, menos, porcentaje, mas]
        {
            const int valor = lectura->zoom();
            porcentaje->setText(QStringLiteral("%1 %").arg(valor));
            porcentaje->setAccessibleName(QStringLiteral(
                "Tamaño actual: %1 por ciento. Restablecer tamaño del texto.").arg(valor));
            menos->setEnabled(valor > ZoomMinimo);
            mas->setEnabled(valor < ZoomMaximo);
        };
        QObject::connect(menos, &QToolButton::clicked, parent, [lectura, mostrarZoom]
        {
            lectura->fijarZoom(lectura->zoom() - PasoZoom);
            mostrarZoom();
        });
        QObject::connect(mas, &QToolButton::clicked, parent, [lectura, mostrarZoom]
        {
            lectura->fijarZoom(lectura->zoom() + PasoZoom);
            mostrarZoom();
        });
        QObject::connect(porcentaje, &QToolButton::clicked, parent, [lectura, mostrarZoom]
        {
            lectura->fijarZoom(ZoomInicial);
            mostrarZoom();
        });
        mostrarZoom();

        fila->addStretch();
        fila->addWidget(new QLabel(QStringLiteral("Tamaño del texto:"), parent));
        fila->addWidget(menos);
        fila->addWidget(porcentaje);
        fila->addWidget(mas);
        return fila;
    }

    // Muestra texto literal y permite partirlo en varias líneas.
    QLabel* crearEtiqueta(const QString& texto, QWidget* parent)
    {
        auto* etiqueta = new QLabel(texto, parent);
        etiqueta->setTextFormat(Qt::PlainText);
        etiqueta->setWordWrap(true);
        return etiqueta;
    }

    // Esta etiqueta conserva el ajuste de línea y comparte el botón de su opción.
    class EtiquetaOpcion final : public QLabel
    {
    public:
        EtiquetaOpcion(const QString& texto, QRadioButton* boton, QWidget* parent)
            : QLabel(texto, parent), boton_(boton)
        {
            setTextFormat(Qt::PlainText);
            setWordWrap(true);
            setCursor(Qt::PointingHandCursor);
        }

    protected:
        void mousePressEvent(QMouseEvent* evento) override
        {
            if (evento->button() == Qt::LeftButton && boton_ && boton_->isEnabled())
            {
                pulsada_ = true;
                evento->accept();
                return;
            }
            QLabel::mousePressEvent(evento);
        }

        void mouseReleaseEvent(QMouseEvent* evento) override
        {
            if (evento->button() == Qt::LeftButton && pulsada_)
            {
                pulsada_ = false;
                evento->accept();

                // Solo marca la opción si el clic termina dentro de su texto.
                if (boton_ && boton_->isEnabled() &&
                    rect().contains(evento->position().toPoint()))
                {
                    boton_->setFocus(Qt::MouseFocusReason);
                    boton_->click();
                }
                return;
            }
            QLabel::mouseReleaseEvent(evento);
        }

    private:
        QPointer<QRadioButton> boton_;
        bool pulsada_ = false;
    };
}

// Este constructor abre una práctica de una sola pregunta.
VentanaPractica::VentanaPractica(ServicioPractica& servicio,
    const std::string& reactivoId, QWidget* parent)
    : QWidget(parent), servicio_(servicio),
      reactivo_(servicio_.iniciarPrueba(reactivoId))
{
    construirVentana();
    mostrarPregunta();
}

// Este constructor abre una sesión con la lista de preguntas elegidas.
VentanaPractica::VentanaPractica(ServicioPractica& servicio,
    const std::vector<std::string>& reactivoIds, QWidget* parent)
    : QWidget(parent), servicio_(servicio),
      reactivo_(servicio_.iniciarSesion(reactivoIds)), modoSesion_(true)
{
    construirVentana();
    mostrarPregunta();
}

void VentanaPractica::construirVentana()
{
    setWindowTitle("Entrenador EGEL - Practica");
    resize(820, 700);
    // El progreso queda arriba y la pregunta ocupa la zona desplazable.
    auto* principal = new QVBoxLayout(this);
    progreso_ = crearEtiqueta(QString{}, this);
    principal->addWidget(progreso_);

    // El recuadro permanece visible al desplazar la pregunta.
    areaActual_ = crearEtiqueta(QString{}, this);
    areaActual_->setFrameShape(QFrame::StyledPanel);
    areaActual_->setMargin(8);
    auto fuenteArea = areaActual_->font();
    fuenteArea.setBold(true);
    areaActual_->setFont(fuenteArea);
    areaActual_->hide();
    principal->addWidget(areaActual_);

    auto* lectura = new ZonaLectura(this);
    area_ = lectura;
    principal->addLayout(crearControlesLectura(lectura, this));
    principal->addWidget(area_, 1);

    // Los botones quedan fuera de la zona desplazable, siempre al alcance.
    auto* acciones = new QHBoxLayout;
    responder_ = new QPushButton("Responder", this);
    siguiente_ = new QPushButton("Siguiente", this);
    cerrar_ = new QPushButton("Cerrar", this);
    siguiente_->setVisible(modoSesion_);
    acciones->addWidget(responder_);
    acciones->addWidget(siguiente_);
    acciones->addWidget(cerrar_);
    principal->addLayout(acciones);
    // Conecta cada clic con la acción que ejecuta la ventana.
    connect(responder_, &QPushButton::clicked, this, [this] { registrarRespuesta(); });
    connect(siguiente_, &QPushButton::clicked, this, [this] { continuar(); });
    connect(cerrar_, &QPushButton::clicked, this, &QWidget::close);
}

void VentanaPractica::mostrarPregunta()
{
    progreso_->setText("Pregunta " + QString::number(
        static_cast<qulonglong>(servicio_.numeroPregunta())) + " de " +
        QString::number(static_cast<qulonglong>(servicio_.totalPreguntas())) +
        "\nSelecciona una respuesta. Indicar tu confianza es opcional.");

    const int areaActual = servicio_.areaPreguntaActual();
    const auto& nombres = ServicioSeleccionAreas::nombresAreas();
    if (areaActual > 0 && static_cast<std::size_t>(areaActual) <= nombres.size())
    {
        // Solo la primera pregunta del siguiente bloque anuncia el cambio.
        const auto formato = servicio_.esCambioDeArea()
            ? QStringLiteral("Nueva área: %1. %2")
            : QStringLiteral("Área %1: %2");
        areaActual_->setText(formato.arg(areaActual).arg(
            QString::fromStdString(nombres[static_cast<std::size_t>(areaActual - 1)])));
        areaActual_->show();
    }
    else
    {
        areaActual_->hide();
    }

    // Cada pregunta recibe controles nuevos: no hereda marcas ni explicaciones.
    auto* contenido = new QWidget;
    auto* cuerpo = new QVBoxLayout(contenido);
    cuerpo->addWidget(crearEtiqueta(QString::fromStdString(reactivo_.enunciado), contenido));
    if (!reactivo_.supuestos.empty())
    {
        cuerpo->addWidget(crearEtiqueta(
            "Supuestos: " + QString::fromStdString(reactivo_.supuestos), contenido));
    }
    // Si el reactivo trae código, se muestra en un recuadro de solo lectura.
    if (!reactivo_.fragmentoCodigo.empty())
    {
        cuerpo->addWidget(crearEtiqueta("Codigo / pseudocodigo: " +
            QString::fromStdString(reactivo_.lenguaje), contenido));
        auto* codigo = new QPlainTextEdit(contenido);
        codigo->setReadOnly(true);
        codigo->setPlainText(QString::fromStdString(reactivo_.fragmentoCodigo));
        // Conserva la familia monoespaciada y hereda el tamaño de lectura.
        QFont monoespaciada;
        monoespaciada.setFamily(QFontDatabase::systemFont(QFontDatabase::FixedFont).family());
        codigo->setFont(monoespaciada);
        codigo->setLineWrapMode(QPlainTextEdit::NoWrap);
        codigo->setMinimumHeight(150);
        codigo->setMaximumHeight(250);
        cuerpo->addWidget(codigo);
    }

    cajaOpciones_ = new QGroupBox("Opciones", contenido);
    auto* lista = new QVBoxLayout(cajaOpciones_);
    // Los botones del grupo permiten marcar una sola alternativa.
    opciones_ = new QButtonGroup(contenido);
    for (std::size_t i = 0; i < reactivo_.opciones.size(); ++i)
    {
        const auto& opcion = reactivo_.opciones[i];
        auto* fila = new QHBoxLayout;
        auto* boton = new QRadioButton(QString::fromStdString(opcion.id) + ".", cajaOpciones_);
        boton->setAccessibleName(QString::fromStdString(opcion.id + ". " + opcion.texto));
        opciones_->addButton(boton, static_cast<int>(i) + 1);
        fila->addWidget(boton, 0, Qt::AlignTop);
        fila->addWidget(new EtiquetaOpcion(
            QString::fromStdString(opcion.texto), boton, cajaOpciones_), 1);
        lista->addLayout(fila);
    }
    cuerpo->addWidget(cajaOpciones_);
    cuerpo->addWidget(crearEtiqueta("Como respondiste? (opcional)", contenido));
    // Cada opción tiene un texto visible y un valor que recibe el servicio.
    confianza_ = new QComboBox(contenido);
    confianza_->addItem("Sin indicar (opcional)", QStringLiteral("sin_indicar"));
    confianza_->addItem("Con seguridad", QStringLiteral("con_seguridad"));
    confianza_->addItem("Con duda", QStringLiteral("con_duda"));
    cuerpo->addWidget(confianza_);
    // La explicación se mantiene oculta hasta confirmar la respuesta.
    resultado_ = crearEtiqueta(QString{}, contenido);
    resultado_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    resultado_->hide();
    cuerpo->addWidget(resultado_);
    cuerpo->addStretch();

    // QScrollArea destruye el contenido anterior junto con sus controles hijos.
    area_->setWidget(contenido);
    contenido->show();
    // En una pregunta nueva se puede responder, pero todavía no avanzar.
    respondida_ = false;
    responder_->setText("Responder");
    responder_->setEnabled(true);
    siguiente_->setEnabled(false);
    siguiente_->setText(servicio_.esUltima() ? "Ver resumen" : "Siguiente");
}

void VentanaPractica::registrarRespuesta()
{
    if (respondida_) return;
    const int seleccion = opciones_->checkedId();
    if (seleccion < 1 || seleccion > static_cast<int>(reactivo_.opciones.size()))
    {
        QMessageBox::warning(this, "Faltan datos",
            "Selecciona una opcion antes de responder.");
        return;
    }
    // Los IDs de los botones empiezan en 1; las posiciones del arreglo, en 0.
    const auto& opcion = reactivo_.opciones[static_cast<std::size_t>(seleccion - 1)];
    const auto confianza = confianza_->currentData().toString().toStdString();
    // Fija la selección para que un reintento use los mismos datos.
    cajaOpciones_->setEnabled(false);
    confianza_->setEnabled(false);
    responder_->setEnabled(false);

    try
    {
        // Solo muestra retroalimentacion despues de recuperar el registro guardado.
        const auto respuesta = servicio_.responder(opcion.id, confianza);
        QString texto = respuesta.correcta ? "Correcto." : "Incorrecto.";
        texto += "\nRespuesta registrada: " + QString::fromStdString(respuesta.opcionSeleccionadaId);
        texto += "\nPuntos: " + QString::number(respuesta.puntos);
        QString confianzaMostrada = "Sin indicar";
        if (respuesta.confianza == "con_seguridad") confianzaMostrada = "Con seguridad";
        else if (respuesta.confianza == "con_duda") confianzaMostrada = "Con duda";
        texto += "\nConfianza: " + confianzaMostrada;
        // Busca la explicación de la alternativa que quedó registrada.
        for (const auto& alternativa : reactivo_.opciones)
        {
            if (alternativa.id == respuesta.opcionSeleccionadaId)
            {
                texto += "\nExplicacion: " + QString::fromStdString(alternativa.explicacion);
            }
        }
        if (!respuesta.correcta)
        {
            texto += "\nLa opcion correcta era: " + QString::fromStdString(reactivo_.opcionCorrectaId);
        }
        resultado_->setText(texto);
        resultado_->show();
        respondida_ = true;
        responder_->setText("Respuesta registrada");
        siguiente_->setEnabled(modoSesion_);
        // Desplaza la vista después de que Qt acomode el resultado en la pantalla.
        QTimer::singleShot(0, resultado_, [this] { area_->ensureWidgetVisible(resultado_); });
    }
    catch (const std::exception& error)
    {
        responder_->setText("Reintentar");
        responder_->setEnabled(true);
        QMessageBox::warning(this, "No se pudo confirmar el resultado",
            "Tu seleccion se conserva. Pulsa Reintentar para confirmar el registro.\n\n" +
            QString::fromUtf8(error.what()));
    }
}

void VentanaPractica::continuar()
{
    if (!modoSesion_ || !respondida_) return;
    siguiente_->setEnabled(false);
    try
    {
        // Con la última respuesta se cierra la sesión; si quedan preguntas, se carga la siguiente.
        if (servicio_.esUltima()) mostrarResumen(servicio_.finalizarSesion());
        else
        {
            reactivo_ = servicio_.avanzar();
            mostrarPregunta();
        }
    }
    catch (const std::exception& error)
    {
        siguiente_->setEnabled(true);
        QMessageBox::warning(this, "No se pudo continuar",
            "Puedes volver a intentarlo.\n\n" + QString::fromUtf8(error.what()));
    }
}

void VentanaPractica::mostrarResumen(const ResumenPractica& resumen)
{
    progreso_->setText("Sesion finalizada");
    areaActual_->hide();
    auto* contenido = new QWidget;
    auto* cuerpo = new QVBoxLayout(contenido);
    // Calcula los aciertos sobre el total de preguntas y evita dividir entre cero.
    const double porcentaje = resumen.totalPreguntas == 0 ? 0.0 :
        100.0 * resumen.aciertos / resumen.totalPreguntas;
    QString texto = "Resumen de la practica";
    texto += "\n\nPreguntas respondidas: " + QString::number(resumen.respondidas) +
        " de " + QString::number(resumen.totalPreguntas);
    texto += "\nAciertos: " + QString::number(resumen.aciertos);
    texto += "\nErrores: " + QString::number(resumen.errores);
    texto += "\nPuntos: " + QString::number(resumen.puntos) +
        " de " + QString::number(resumen.totalPreguntas);
    texto += "\nPorcentaje de aciertos: " + QString::number(porcentaje, 'f', 1) + "%";
    texto += "\n\nCon seguridad: " + QString::number(resumen.conSeguridad);
    texto += "\nCon duda: " + QString::number(resumen.conDuda);
    texto += "\nSin indicar: " + QString::number(resumen.sinIndicar);
    // Usa unidades completas para cambiar de formato al llegar a la hora.
    const qlonglong milisegundos = resumen.tiempoRespuestaMs;
    const qlonglong segundosTotales = milisegundos / 1000;
    const qlonglong minutosTotales = segundosTotales / 60;
    const qlonglong horas = minutosTotales / 60;
    const qlonglong minutos = minutosTotales % 60;

    texto += "\nTiempo acumulado para responder: ";

    if (horas > 0)
    {
        // A partir de una hora, se muestran solo horas y minutos.
        texto += QString::number(horas) + " h " +
            QString::number(minutos) + " min";
    }
    else
    {
        if (minutos > 0)
        {
            texto += QString::number(minutos) + " min ";
        }

        // Los residuos dan los segundos del minuto actual y su décima.
        const qlonglong segundos = segundosTotales % 60;
        const qlonglong decima = (milisegundos % 1000) / 100;
        texto += QString::number(segundos) + "." +
            QString::number(decima) + " s";
    }
    cuerpo->addWidget(crearEtiqueta(texto, contenido));
    cuerpo->addStretch();
    area_->setWidget(contenido);
    contenido->show();

    // El resumen no presenta los identificadores internos de MongoDB.
    // Al reemplazar la pregunta, se destruyen sus controles; vacía sus punteros.
    opciones_ = nullptr;
    cajaOpciones_ = nullptr;
    confianza_ = nullptr;
    resultado_ = nullptr;
    responder_->hide();
    siguiente_->hide();
    cerrar_->setText("Volver");
}
