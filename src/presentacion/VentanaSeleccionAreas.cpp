#include "VentanaSeleccionAreas.hpp"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFont>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QString>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVariant>
#include <QVBoxLayout>

#include <algorithm>
#include <exception>
#include <map>

namespace
{
    constexpr std::size_t MaxEjemplosPorArea = 2;
}

VentanaSeleccionAreas::VentanaSeleccionAreas(
    ServicioSeleccionAreas& seleccion,
    ServicioPractica& practica,
    QWidget* parent)
    : QWidget(parent),
      seleccion_(seleccion),
      practica_(practica)
{
    setWindowTitle("Entrenador EGEL - Seleccion de areas");
    resize(850, 700);

    // La zona de selección se puede desplazar; los botones quedan abajo.
    auto* principal = new QVBoxLayout(this);

    auto* desplazamiento = new QScrollArea(this);
    desplazamiento->setWidgetResizable(true);

    auto* contenido = new QWidget;
    auto* cuerpo = new QVBoxLayout(contenido);

    cuerpo->addWidget(new QLabel(
        "Elige las areas que deseas estudiar.",
        contenido));

    auto* cajaModalidad = new QGroupBox(
        "Modalidad", contenido);

    auto* filaModalidad = new QHBoxLayout(cajaModalidad);

    modalidades_ = new QButtonGroup(this);

    auto* individual = new QRadioButton(
        "Area individual", cajaModalidad);

    auto* varias = new QRadioButton(
        "Varias areas", cajaModalidad);

    // Estos identificadores permiten distinguir la modalidad en leerSeleccion().
    modalidades_->addButton(individual, 1);
    modalidades_->addButton(varias, 2);

    individual->setChecked(true);

    filaModalidad->addWidget(individual);
    filaModalidad->addWidget(varias);

    cuerpo->addWidget(cajaModalidad);

    auto* cajaAreas = new QGroupBox(
        "Areas del EGEL", contenido);

    auto* listaAreas = new QVBoxLayout(cajaAreas);

    grupoAreas_ = new QButtonGroup(this);
    // La modalidad inicial es individual: solo se puede marcar un área.
    grupoAreas_->setExclusive(true);

    const auto& nombres =
        ServicioSeleccionAreas::nombresAreas();

    for (std::size_t i = 0; i < casillas_.size(); ++i)
    {
        casillas_[i] = new QCheckBox(
            QString::number(static_cast<int>(i) + 1) +
                ". " + QString::fromStdString(nombres[i]),
            cajaAreas);

        grupoAreas_->addButton(
            casillas_[i],
            static_cast<int>(i) + 1);

        listaAreas->addWidget(casillas_[i]);
    }

    cuerpo->addWidget(cajaAreas);

    consultar_ = new QPushButton(
        "Ver preguntas disponibles", contenido);

    cuerpo->addWidget(consultar_);

    // El árbol muestra los conteos sin llenar la vista de enunciados.
    auto* cajaPrevia = new QGroupBox(
        "Vista previa", contenido);

    auto* previa = new QVBoxLayout(cajaPrevia);

    resumen_ = new QLabel(cajaPrevia);
    resumen_->setTextFormat(Qt::PlainText);
    resumen_->setWordWrap(true);

    previa->addWidget(resumen_);

    preguntas_ = new QTreeWidget(cajaPrevia);
    preguntas_->setColumnCount(2);
    preguntas_->setHeaderLabels({
        QStringLiteral("Área, subárea o ejemplo"),
        QStringLiteral("Preguntas")});
    preguntas_->header()->setStretchLastSection(false);
    preguntas_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    preguntas_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    preguntas_->setAlternatingRowColors(true);
    preguntas_->setMinimumHeight(190);
    preguntas_->setMaximumHeight(270);
    preguntas_->setAccessibleName(QStringLiteral("Vista previa del banco"));

    previa->addWidget(preguntas_);
    cuerpo->addWidget(cajaPrevia);

    desplazamiento->setWidget(contenido);
    principal->addWidget(desplazamiento, 1);

    auto* acciones = new QHBoxLayout;

    abrir_ = new QPushButton(
        QStringLiteral("Practicar ejemplo"), this);

    iniciarSesion_ = new QPushButton(
        "Iniciar sesion", this);

    auto* cerrar = new QPushButton("Cerrar", this);

    acciones->addWidget(abrir_);
    acciones->addWidget(iniciarSesion_);
    acciones->addWidget(cerrar);

    principal->addLayout(acciones);

    // Cambiar los filtros obliga a consultar de nuevo la vista previa.
    connect(modalidades_, &QButtonGroup::idToggled,
        this, [this](int id, bool marcada)
        {
            if (marcada)
            {
                ajustarModalidad(id == 1);
            }
        });

    connect(grupoAreas_, &QButtonGroup::idToggled,
        this, [this](int, bool)
        {
            invalidarVistaPrevia();
        });

    connect(consultar_, &QPushButton::clicked,
        this, [this]
        {
            consultarPreguntas();
        });

    // Solo un ejemplo visible y seleccionado se puede practicar.
    const auto actualizarBotonPractica = [this]
    {
        abrir_->setEnabled(indiceEjemploSeleccionado().has_value());
    };
    connect(preguntas_, &QTreeWidget::itemSelectionChanged,
        this, actualizarBotonPractica);
    connect(preguntas_, &QTreeWidget::itemCollapsed,
        this, actualizarBotonPractica);
    connect(preguntas_, &QTreeWidget::itemExpanded,
        this, actualizarBotonPractica);

    connect(abrir_, &QPushButton::clicked,
        this, [this]
        {
            abrirPregunta();
        });

    connect(iniciarSesion_, &QPushButton::clicked,
        this, [this]
        {
            abrirSesion();
        });

    connect(cerrar, &QPushButton::clicked,
        this, &QWidget::close);

    invalidarVistaPrevia();
}

// Reúne la modalidad y las áreas marcadas para enviarlas al servicio.
SeleccionAreas VentanaSeleccionAreas::leerSeleccion() const
{
    SeleccionAreas seleccion;

    seleccion.modalidad = modalidades_->checkedId() == 1
        ? ModalidadAreas::individual
        : ModalidadAreas::varias;

    for (std::size_t i = 0; i < casillas_.size(); ++i)
    {
        if (casillas_[i]->isChecked())
        {
            // El arreglo empieza en 0, pero las áreas se numeran del 1 al 4.
            seleccion.areas.push_back(
                static_cast<int>(i) + 1);
        }
    }

    return seleccion;
}

void VentanaSeleccionAreas::ajustarModalidad(bool individual)
{
    // Al regresar a individual, conserva solo la primera area marcada.
    // Silencia las señales del grupo mientras se ajustan las casillas.
    const QSignalBlocker bloqueo(grupoAreas_);

    grupoAreas_->setExclusive(false);

    bool conservada = false;

    if (individual)
    {
        for (auto* casilla : casillas_)
        {
            if (casilla->isChecked())
            {
                if (conservada)
                {
                    casilla->setChecked(false);
                }
                else
                {
                    conservada = true;
                }
            }
        }
    }

    grupoAreas_->setExclusive(individual);
    invalidarVistaPrevia();
}

void VentanaSeleccionAreas::invalidarVistaPrevia()
{
    // Descarta las preguntas anteriores para que no se usen con otros filtros.
    vistaVigente_ = false;
    vista_ = VistaPreviaAreas{};

    preguntas_->clear();
    preguntas_->hide();

    abrir_->setEnabled(false);
    iniciarSesion_->setEnabled(false);

    cantidadElegida_ = 1;

    resumen_->setText(
        "Selecciona exactamente un area, o de dos a cuatro "
        "en Varias areas. Despues consulta las preguntas disponibles.");
}

void VentanaSeleccionAreas::consultarPreguntas()
{
    invalidarVistaPrevia();
    consultar_->setEnabled(false);

    try
    {
        // El servicio valida la selección y devuelve las preguntas disponibles.
        vista_ = seleccion_.consultar(leerSeleccion());
        vistaVigente_ = true;

        const auto total = static_cast<qulonglong>(vista_.preguntas.size());
        QString resumen = QStringLiteral("Total disponible: %1 %2.")
            .arg(total)
            .arg(total == 1 ? QStringLiteral("pregunta") : QStringLiteral("preguntas"));
        const auto limite = ServicioSeleccionAreas::limitePorArea(vista_);
        if (vista_.preguntas.empty())
        {
            resumen += QStringLiteral("\nNo hay preguntas de práctica para esta selección.");
        }
        else if (limite == 0)
        {
            resumen += QStringLiteral(
                "\nTodas las áreas elegidas deben tener preguntas para iniciar una sesión."
                " Puedes practicar los ejemplos disponibles.");
        }
        else
        {
            resumen += QStringLiteral(
                "\nAbre un área para ver sus subáreas y ejemplos. "
                "La sesión sortea preguntas de todo el banco de las áreas elegidas.");
        }

        resumen_->setText(resumen);

        actualizarArbolPrevia();
        cantidadElegida_ = limite > 0 ? static_cast<int>(limite) : 1;
        iniciarSesion_->setEnabled(limite > 0);

        consultar_->setEnabled(true);
    }
    catch (const std::exception& error)
    {
        invalidarVistaPrevia();
        consultar_->setEnabled(true);

        QMessageBox::warning(
            this,
            "Revisa la seleccion",
            QString::fromUtf8(error.what()));
    }
}

void VentanaSeleccionAreas::actualizarArbolPrevia()
{
    // Los índices apuntan a vista_.preguntas, que conserva el banco completo.
    using Subareas = std::map<std::string, std::vector<std::size_t>>;
    std::array<Subareas, MaxAreasSesion> agrupadas;
    for (std::size_t i = 0; i < vista_.preguntas.size(); ++i)
    {
        const auto& pregunta = vista_.preguntas[i];
        agrupadas.at(static_cast<std::size_t>(pregunta.area - 1))[pregunta.subarea]
            .push_back(i);
    }

    const auto& nombres = ServicioSeleccionAreas::nombresAreas();
    bool primeraAreaConPreguntas = true;
    for (const int area : vista_.seleccion.areas)
    {
        const auto indice = static_cast<std::size_t>(area - 1);
        auto* grupo = new QTreeWidgetItem(preguntas_);
        grupo->setText(0, QStringLiteral("%1. %2")
            .arg(area).arg(QString::fromStdString(nombres[indice])));
        grupo->setText(1, QString::number(
            static_cast<qlonglong>(vista_.preguntasPorArea[indice])));
        grupo->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        auto fuente = grupo->font(0);
        fuente.setBold(true);
        grupo->setFont(0, fuente);
        grupo->setFont(1, fuente);

        const auto& subareas = agrupadas[indice];
        std::size_t restantes = MaxEjemplosPorArea;
        for (const auto& [nombre, indices] : subareas)
        {
            auto* subarea = new QTreeWidgetItem(grupo);
            subarea->setText(0, nombre.empty() ? QStringLiteral("Sin subárea")
                : QString::fromStdString(nombre));
            subarea->setText(1, QString::number(static_cast<qulonglong>(indices.size())));
            subarea->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);

            // Si hay varias subáreas, toma ejemplos de subáreas distintas.
            const auto cantidad = std::min(indices.size(),
                subareas.size() == 1 ? restantes : std::min(restantes, std::size_t{1}));
            for (std::size_t j = 0; j < cantidad; ++j)
            {
                const auto indicePregunta = indices[j];
                const auto& pregunta = vista_.preguntas[indicePregunta];
                const auto tema = QString::fromStdString(pregunta.nombreTema);
                auto* ejemplo = new QTreeWidgetItem(subarea);
                // El árbol solo anuncia el tema; el enunciado aparece al practicar.
                ejemplo->setText(0, QStringLiteral("Ejemplo · %1").arg(tema));
                ejemplo->setToolTip(0, QStringLiteral("<p>%1</p>").arg(tema.toHtmlEscaped()));
                ejemplo->setData(0, Qt::UserRole, static_cast<qulonglong>(indicePregunta));
            }
            restantes -= cantidad;
            subarea->setExpanded(cantidad > 0);
        }
        if (primeraAreaConPreguntas && !subareas.empty())
        {
            grupo->setExpanded(true);
            primeraAreaConPreguntas = false;
        }
    }

    preguntas_->show();
}

std::optional<std::size_t> VentanaSeleccionAreas::indiceEjemploSeleccionado() const
{
    if (!vistaVigente_) return std::nullopt;
    const auto seleccionados = preguntas_->selectedItems();
    if (seleccionados.isEmpty()) return std::nullopt;
    const auto* elegido = seleccionados.front();
    for (auto* padre = elegido->parent(); padre; padre = padre->parent())
    {
        if (!padre->isExpanded()) return std::nullopt;
    }
    bool valido = false;
    const auto indice = elegido->data(0, Qt::UserRole).toULongLong(&valido);
    if (!valido || indice >= vista_.preguntas.size()) return std::nullopt;
    return static_cast<std::size_t>(indice);
}

void VentanaSeleccionAreas::abrirPregunta()
{
    const auto indice = indiceEjemploSeleccionado();
    if (!indice)
    {
        return;
    }

    // Si ya hay una práctica abierta, trae esa ventana al frente.
    if (practicaAbierta_)
    {
        practicaAbierta_->raise();
        practicaAbierta_->activateWindow();
        return;
    }

    try
    {
        const auto& pregunta = vista_.preguntas[*indice];

        auto* ventana = new VentanaPractica(
            practica_, pregunta.reactivoId, this);

        mostrarVentana(ventana);
    }
    catch (const std::exception& error)
    {
        QMessageBox::warning(
            this,
            "No se pudo abrir la pregunta",
            QString::fromUtf8(error.what()));
    }
}

void VentanaSeleccionAreas::abrirSesion()
{
    if (!vistaVigente_ || vista_.preguntas.empty())
    {
        return;
    }

    if (practicaAbierta_)
    {
        practicaAbierta_->raise();
        practicaAbierta_->activateWindow();
        return;
    }

    try
    {
        const int limite = static_cast<int>(ServicioSeleccionAreas::limitePorArea(vista_));
        if (limite == 0) return;

        const int areas = static_cast<int>(vista_.seleccion.areas.size());
        QDialog dialogo(this);
        dialogo.setWindowTitle(QStringLiteral("Preguntas por área"));
        auto* diseno = new QVBoxLayout(&dialogo);
        diseno->addWidget(new QLabel(
            QStringLiteral("Áreas seleccionadas: %1\nPreguntas disponibles: %2")
                .arg(areas).arg(static_cast<qulonglong>(vista_.preguntas.size())),
            &dialogo));

        auto* etiqueta = new QLabel(QStringLiteral("Preguntas por área:"), &dialogo);
        auto* cantidades = new QComboBox(&dialogo);
        etiqueta->setBuddy(cantidades);
        cantidades->setAccessibleName(etiqueta->text());
        for (int cantidad = 1; cantidad <= limite; ++cantidad)
        {
            cantidades->addItem(QString::number(cantidad));
        }
        cantidades->setCurrentIndex(std::clamp(cantidadElegida_, 1, limite) - 1);
        diseno->addWidget(etiqueta);
        diseno->addWidget(cantidades);

        auto* total = new QLabel(&dialogo);
        const auto actualizarTotal = [total, areas](int indice)
        {
            total->setText(QStringLiteral("Total de la sesión: %1")
                .arg((indice + 1) * areas));
        };
        actualizarTotal(cantidades->currentIndex());
        connect(cantidades, &QComboBox::currentIndexChanged, &dialogo, actualizarTotal);
        diseno->addWidget(total);

        auto* botones = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialogo);
        botones->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Iniciar"));
        botones->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Cancelar"));
        connect(botones, &QDialogButtonBox::accepted, &dialogo, &QDialog::accept);
        connect(botones, &QDialogButtonBox::rejected, &dialogo, &QDialog::reject);
        diseno->addWidget(botones);

        // Cancelar regresa al menu antes de crear una sesion.
        if (dialogo.exec() != QDialog::Accepted)
        {
            return;
        }

        cantidadElegida_ = cantidades->currentIndex() + 1;

        // El servicio elige los ID que formarán parte de la sesión.
        const auto ids =
            ServicioSeleccionAreas::prepararSesion(
                vista_,
                static_cast<std::size_t>(cantidadElegida_));

        mostrarVentana(
            new VentanaPractica(practica_, ids, this));
    }
    catch (const std::exception& error)
    {
        QMessageBox::warning(
            this,
            "No se pudo iniciar la sesion",
            QString::fromUtf8(error.what()));
    }
}

void VentanaSeleccionAreas::mostrarVentana(
    VentanaPractica* ventana)
{
    // Conserva una sola practica activa en el servicio.
    ventana->setWindowFlag(Qt::Window, true);
    // Mientras la práctica está abierta, el menú no recibe interacción.
    ventana->setWindowModality(Qt::WindowModal);
    // Al cerrar la práctica, Qt elimina la ventana y vacía el QPointer.
    ventana->setAttribute(Qt::WA_DeleteOnClose);

    practicaAbierta_ = ventana;
    ventana->show();
}
