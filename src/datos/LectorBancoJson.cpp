#include "LectorBancoJson.hpp"

#include <QDirListing>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include <array>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace
{
    // La posición en esta lista corresponde al número de área (1 a 4).
    constexpr std::array<const char*, 4> archivosArea{
        "area_01_analisis.json",
        "area_02_diseno.json",
        "area_03_desarrollo.json",
        "area_04_gestion.json"
    };

    [[noreturn]] void fallar(const QString& contexto, const QString& mensaje)
    {
        throw std::runtime_error((contexto + ": " + mensaje).toStdString());
    }

    std::string leerTexto(const QJsonObject& objeto, const QString& campo,
        const QString& contexto, bool obligatorio = true)
    {
        const auto valor = objeto.value(campo);
        if (!obligatorio && valor.isUndefined()) return {};
        if (!valor.isString() || (obligatorio && valor.toString().trimmed().isEmpty()))
        {
            fallar(contexto, "'" + campo + "' debe ser texto" +
                (obligatorio ? " no vacio." : "."));
        }
        return valor.toString().toStdString();
    }

    QJsonArray leerArreglo(const QJsonObject& objeto, const QString& campo,
        const QString& contexto)
    {
        const auto valor = objeto.value(campo);
        if (!valor.isArray()) fallar(contexto, "'" + campo + "' debe ser un arreglo.");
        return valor.toArray();
    }

    QJsonObject leerArchivo(const QString& ruta)
    {
        QFile archivo(ruta);
        if (!archivo.open(QIODevice::ReadOnly)) fallar(ruta, archivo.errorString());
        const auto contenido = archivo.readAll();
        if (archivo.error() != QFileDevice::NoError) fallar(ruta, archivo.errorString());

        QJsonParseError error{};
        const auto documento = QJsonDocument::fromJson(contenido, &error);
        if (error.error != QJsonParseError::NoError)
        {
            fallar(ruta, "JSON invalido cerca del byte " +
                QString::number(static_cast<qlonglong>(error.offset)) + ": " +
                error.errorString());
        }
        if (!documento.isObject()) fallar(ruta, "Se esperaba un objeto JSON.");
        return documento.object();
    }

    Reactivo leerReactivo(const QJsonObject& objeto, const QString& tema)
    {
        Reactivo reactivo;
        reactivo.codigoReactivo = leerTexto(objeto, "codigoReactivo", tema);
        const auto contexto = tema + " / " + QString::fromStdString(reactivo.codigoReactivo);

        reactivo.version = objeto.value(QStringLiteral("version")).toInt();
        if (reactivo.version < 1)
        {
            fallar(contexto, "'version' debe ser un entero mayor o igual a 1.");
        }

        reactivo.formato = leerTexto(objeto, "formato", contexto);
        if (reactivo.formato != "caso_practico" && reactivo.formato != "analisis_codigo" &&
            reactivo.formato != "interpretacion_tecnica")
        {
            fallar(contexto, "Formato de reactivo desconocido.");
        }

        reactivo.enunciado = leerTexto(objeto, "enunciado", contexto);
        reactivo.supuestos = leerTexto(objeto, "supuestos", contexto, false);
        reactivo.lenguaje = leerTexto(objeto, "lenguaje", contexto, false);
        reactivo.fragmentoCodigo = leerTexto(objeto, "fragmentoCodigo", contexto, false);

        if (reactivo.formato == "analisis_codigo" &&
            (reactivo.lenguaje.empty() || reactivo.fragmentoCodigo.empty()))
        {
            fallar(contexto, "El formato analisis_codigo requiere lenguaje y fragmentoCodigo.");
        }

        // Cada inciso guarda su propia explicación, acertada o no.
        const auto opciones = leerArreglo(objeto, "opciones", contexto);
        if (opciones.size() != 3) fallar(contexto, "Se requieren exactamente tres opciones.");

        constexpr std::array<const char*, 3> ids{"A", "B", "C"};
        std::set<std::string> textos;
        for (std::size_t i = 0; i < ids.size(); ++i)
        {
            const auto valor = opciones.at(static_cast<qsizetype>(i));
            if (!valor.isObject()) fallar(contexto, "Cada opcion debe ser un objeto.");
            const auto opcion = valor.toObject();
            const auto id = leerTexto(opcion, "id", contexto);
            if (id != ids[i]) fallar(contexto, "Los incisos deben estar en orden A, B y C.");
            const auto texto = leerTexto(opcion, "texto", contexto);
            if (!textos.insert(texto).second) fallar(contexto, "Hay opciones con texto repetido.");
            reactivo.opciones[i] = Opcion{id, texto, leerTexto(opcion, "explicacion", contexto)};
        }

        reactivo.opcionCorrectaId = leerTexto(objeto, "opcionCorrectaId", contexto);
        if (reactivo.opcionCorrectaId != "A" && reactivo.opcionCorrectaId != "B" &&
            reactivo.opcionCorrectaId != "C")
        {
            fallar(contexto, "La opcion correcta debe ser A, B o C.");
        }

        // Este comando solo importa preguntas para la fase de práctica.
        reactivo.usoBanco = "practica";
        return reactivo;
    }
}

std::vector<TemaBanco> leerBancoJson(const std::string& carpeta)
{
    const QDir directorio(QString::fromLocal8Bit(carpeta.c_str()));
    std::vector<TemaBanco> banco;
    std::map<std::string, std::size_t> indiceTemas;
    std::set<std::string> codigosReactivo;

    // Se valida todo el banco antes de que se escriba en MongoDB.
    const auto cargarArchivo = [&](const QString& ruta, int area)
    {
        const auto raiz = leerArchivo(ruta);
        if (raiz.value(QStringLiteral("area")).toInt() != area)
        {
            fallar(ruta, "'area' debe ser " + QString::number(area) + ".");
        }

        const auto temas = leerArreglo(raiz, "temas", ruta);
        for (const auto valor : temas)
        {
            if (!valor.isObject()) fallar(ruta, "Cada tema debe ser un objeto.");
            const auto objeto = valor.toObject();
            Tema tema;
            tema.area = area;
            tema.codigoTema = leerTexto(objeto, "codigoTema", ruta);
            const auto contexto = ruta + " / " + QString::fromStdString(tema.codigoTema);

            tema.subarea = leerTexto(objeto, "subarea", contexto);
            tema.nombre = leerTexto(objeto, "nombre", contexto);
            tema.explicacion = leerTexto(objeto, "explicacion", contexto, false);
            tema.ejemplo = leerTexto(objeto, "ejemplo", contexto, false);
            tema.fuente = leerTexto(objeto, "fuente", contexto, false);

            // Un tema puede continuar en otro lote si su definición es la misma.
            const auto resultado = indiceTemas.emplace(tema.codigoTema, banco.size());
            if (resultado.second)
            {
                banco.push_back(TemaBanco{std::move(tema), {}});
            }
            else
            {
                const auto& previo = banco.at(resultado.first->second).tema;
                if (previo.area != tema.area || previo.subarea != tema.subarea ||
                    previo.nombre != tema.nombre || previo.explicacion != tema.explicacion ||
                    previo.ejemplo != tema.ejemplo || previo.fuente != tema.fuente)
                {
                    fallar(contexto, "El codigo de tema ya existe con datos diferentes.");
                }
            }
            auto& grupo = banco.at(resultado.first->second);

            const auto reactivos = leerArreglo(objeto, "reactivos", contexto);
            for (const auto entrada : reactivos)
            {
                if (!entrada.isObject()) fallar(contexto, "Cada reactivo debe ser un objeto.");
                auto reactivo = leerReactivo(entrada.toObject(), contexto);
                if (!codigosReactivo.insert(reactivo.codigoReactivo).second)
                {
                    fallar(contexto, "Codigo de reactivo repetido: " +
                        QString::fromStdString(reactivo.codigoReactivo) + ".");
                }
                grupo.reactivos.push_back(std::move(reactivo));
            }
        }
    };

    // Los cuatro archivos originales conservan su nombre y siguen funcionando solos.
    for (std::size_t i = 0; i < archivosArea.size(); ++i)
    {
        cargarArchivo(directorio.filePath(QString::fromLatin1(archivosArea[i])),
            static_cast<int>(i) + 1);
    }

    const QDir lotes(directorio.filePath(QStringLiteral("lotes")));
    if (lotes.exists())
    {
        const QRegularExpression patron(QStringLiteral(R"(^area_(0[1-4])_lote_[0-9]{2,}\.json$)"));
                // Busca los JSON directamente en lotes y en sus subcarpetas.
        using F = QDirListing::IteratorFlag;
        const QDirListing archivos(lotes.absolutePath(),
            QStringList{QStringLiteral("*.json")},
            F::FilesOnly | F::Recursive);

        for (const auto& archivo : archivos)
        {
            const auto ruta = archivo.filePath();
            const auto coincidencia = patron.match(archivo.fileName());

            if (!coincidencia.hasMatch())
            {
                fallar(ruta, "El nombre debe seguir el formato area_01_lote_02.json.");
            }

            cargarArchivo(ruta, coincidencia.captured(1).toInt());
        }
    }
    return banco;
}
