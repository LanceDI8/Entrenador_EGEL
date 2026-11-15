#include "RepositorioMongoDB.hpp"
#include "../modelo/CalculoResumen.hpp"
#include "../modelo/SeleccionAreas.hpp"
#include <set>
#include <stdexcept>
#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/oid.hpp>
#include <bsoncxx/types.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/exception.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/options/find.hpp>
#include <mongocxx/options/update.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/write_concern.hpp>
#include <mongocxx/result/insert_one.hpp>
#include <chrono>


// Funciones de apoyo que solo se utilizan dentro de este archivo.
namespace
{
    // kvp une un nombre con un valor;
    // make_document construye un documento BSON.
    using bsoncxx::builder::basic::kvp;
    using bsoncxx::builder::basic::make_document;

    // Revisa el formato del identificador;
    // no consulta si existe en la base.
    bool esObjectId(const std::string& texto)
    {
        return texto.size() == 24 &&
            texto.find_first_not_of("0123456789abcdefABCDEF") ==
                std::string::npos;
    }

    // Lee un campo BSON y devuelve una copia del texto para utilizarla en C++.
    std::string leerTexto(
        bsoncxx::document::view documento,
        const char* campo)
    {
        const auto elemento = documento[campo];

        if (!elemento || elemento.type() != bsoncxx::type::k_string)
        {
            throw std::runtime_error(
                std::string("Campo de texto ausente o invalido: ") + campo);
        }

        const auto texto = elemento.get_string().value;
        return std::string(texto.data(), texto.size());
    }

    // Reconstruye una respuesta a partir del documento guardado.
    Respuesta convertirARespuesta(bsoncxx::document::view documento)
    {
        Respuesta respuesta;

        respuesta.id = documento["_id"].get_oid().value.to_string();

        respuesta.perfilId =
            documento["perfilId"].get_oid().value.to_string();

        respuesta.sesionId =
            documento["sesionId"].get_oid().value.to_string();

        respuesta.fase = leerTexto(documento, "fase");

        respuesta.metodoSeleccion =
            leerTexto(documento, "metodoSeleccion");

        respuesta.reactivoId =
            documento["reactivoId"].get_oid().value.to_string();

        respuesta.codigoReactivo =
            leerTexto(documento, "codigoReactivo");

        respuesta.versionReactivo =
            documento["versionReactivo"].get_int32().value;

        respuesta.temaId =
            documento["temaId"].get_oid().value.to_string();

        respuesta.area = documento["area"].get_int32().value;
        respuesta.subarea = leerTexto(documento, "subarea");

        respuesta.opcionSeleccionadaId =
            leerTexto(documento, "opcionSeleccionadaId");

        respuesta.confianza = leerTexto(documento, "confianza");
        respuesta.correcta = documento["correcta"].get_bool().value;
        respuesta.puntos = documento["puntos"].get_int32().value;

        respuesta.tiempoRespuestaMs =
            documento["tiempoRespuestaMs"].get_int64().value;

        // Recupera la fecha BSON y la convierte al tipo de nuestro modelo.
        const auto milisegundos = static_cast<std::int64_t>(
            documento["fechaHora"].get_date());

        respuesta.fechaHora = std::chrono::system_clock::time_point{
            std::chrono::duration_cast<std::chrono::system_clock::duration>(
                std::chrono::milliseconds{milisegundos})
        };

        std::size_t posicion = 0;

        for (const auto& elemento :
             documento["ordenOpciones"].get_array().value)
        {
            if (posicion >= respuesta.ordenOpciones.size())
            {
                throw std::runtime_error(
                    "El orden guardado contiene demasiadas opciones.");
            }

            const auto texto = elemento.get_string().value;

            respuesta.ordenOpciones[posicion++] =
                std::string(texto.data(), texto.size());
        }

        if (posicion != respuesta.ordenOpciones.size())
        {
            throw std::runtime_error(
                "El orden guardado debe contener tres opciones.");
        }

        return respuesta;
    }

    // Comprueba los datos necesarios para identificar y clasificar un tema.
    void validarTema(const Tema& tema)
    {
        if (tema.codigoTema.empty() ||
            tema.subarea.empty() ||
            tema.nombre.empty() ||
            tema.estadoRevision.empty())
        {
            throw std::invalid_argument(
                "El tema tiene campos obligatorios vacios.");
        }

        if (tema.area < 1 || tema.area > 4)
        {
            throw std::invalid_argument(
                "El area del tema debe ser un numero del 1 al 4.");
        }
    }

    // Convierte un documento de MongoDB a la estructura Tema de C++.
    Tema convertirATema(bsoncxx::document::view documento)
    {
        Tema tema;

        tema.id = documento["_id"].get_oid().value.to_string();
        tema.codigoTema = leerTexto(documento, "codigoTema");
        tema.area = documento["area"].get_int32().value;
        tema.subarea = leerTexto(documento, "subarea");
        tema.nombre = leerTexto(documento, "nombre");
        tema.explicacion = leerTexto(documento, "explicacion");
        tema.ejemplo = leerTexto(documento, "ejemplo");
        tema.fuente = leerTexto(documento, "fuente");

        tema.estadoRevision = leerTexto(
            documento, "estadoRevision");

        validarTema(tema);

        return tema;
    }

    // Comprueba los campos obligatorios y la estructura de las opciones.
    void validarReactivo(const Reactivo& reactivo)
    {
        if (reactivo.codigoReactivo.empty() || reactivo.version < 1 ||
            reactivo.enunciado.empty() || reactivo.formato.empty() ||
            reactivo.usoBanco.empty() || reactivo.estadoRevision.empty())
        {
            throw std::invalid_argument(
                "El reactivo tiene campos obligatorios vacios o version invalida.");
        }

        if (!esObjectId(reactivo.temaId))
        {
            throw std::invalid_argument(
                "temaId debe contener el ObjectId del tema: 24 caracteres hexadecimales.");
        }

        if (reactivo.formato != "caso_practico" &&
            reactivo.formato != "analisis_codigo" &&
            reactivo.formato != "interpretacion_tecnica")
        {
            throw std::invalid_argument(
                "El formato del reactivo no es valido.");
        }

        // El conjunto evita identificadores repetidos entre las opciones.
        // insert(...).second vale false cuando el identificador ya existia.
        std::set<std::string> identificadores;

        for (const auto& opcion : reactivo.opciones)
        {
            if (opcion.id.empty() || opcion.texto.empty() ||
                opcion.explicacion.empty() ||
                !identificadores.insert(opcion.id).second)
            {
                throw std::invalid_argument(
                    "Las tres opciones deben tener identificadores distintos, texto y explicacion.");
            }
        }

        if (identificadores.count(reactivo.opcionCorrectaId) != 1)
        {
            throw std::invalid_argument(
                "La respuesta correcta debe identificar una de las tres opciones.");
        }

        if (reactivo.formato == "analisis_codigo" &&
            (reactivo.lenguaje.empty() || reactivo.fragmentoCodigo.empty()))
        {
            throw std::invalid_argument(
                "Un reactivo de analisis_codigo necesita lenguaje y fragmentoCodigo.");
        }
    }

    // Convierte la estructura Reactivo de C++ al formato BSON de MongoDB.
    bsoncxx::document::value convertirADocumento(
        const Reactivo& reactivo)
    {
        bsoncxx::builder::basic::array opciones;

        for (const auto& opcion : reactivo.opciones)
        {
            opciones.append(make_document(
                kvp("id", opcion.id),
                kvp("texto", opcion.texto),
                kvp("explicacion", opcion.explicacion)
            ));
        }

        return make_document(
            kvp("codigoReactivo", reactivo.codigoReactivo),
            kvp("version", static_cast<std::int32_t>(reactivo.version)),
            kvp("temaId", bsoncxx::oid{reactivo.temaId}),
            kvp("formato", reactivo.formato),
            kvp("enunciado", reactivo.enunciado),
            kvp("lenguaje", reactivo.lenguaje),
            kvp("fragmentoCodigo", reactivo.fragmentoCodigo),
            kvp("supuestos", reactivo.supuestos),
            kvp("opciones", opciones.extract()),
            kvp("opcionCorrectaId", reactivo.opcionCorrectaId),
            kvp("usoBanco", reactivo.usoBanco),
            kvp("estadoRevision", reactivo.estadoRevision)
        );
    }

    // Reconstruye un Reactivo a partir de un documento recuperado de MongoDB.
    Reactivo convertirAReactivo(
        bsoncxx::document::view documento)
    {
        Reactivo reactivo;

        reactivo.id = documento["_id"].get_oid().value.to_string();

        reactivo.codigoReactivo = leerTexto(
            documento, "codigoReactivo");

        reactivo.version = documento["version"].get_int32().value;

        reactivo.temaId = documento["temaId"].get_oid().value.to_string();

        reactivo.formato = leerTexto(documento, "formato");
        reactivo.enunciado = leerTexto(documento, "enunciado");
        reactivo.lenguaje = leerTexto(documento, "lenguaje");

        reactivo.fragmentoCodigo = leerTexto(
            documento, "fragmentoCodigo");

        reactivo.supuestos = leerTexto(documento, "supuestos");

        reactivo.opcionCorrectaId = leerTexto(
            documento, "opcionCorrectaId");

        reactivo.usoBanco = leerTexto(documento, "usoBanco");

        reactivo.estadoRevision = leerTexto(
            documento, "estadoRevision");

        // El arreglo de Reactivo admite exactamente tres opciones.
        // Comprobamos la cantidad antes de aceptar el documento leido.
        std::size_t posicion = 0;

        for (const auto& elemento :
             documento["opciones"].get_array().value)
        {
            if (posicion >= reactivo.opciones.size())
            {
                throw std::runtime_error(
                    "El documento contiene mas de tres opciones.");
            }

            const auto opcion = elemento.get_document().value;

            reactivo.opciones[posicion++] = Opcion{
                leerTexto(opcion, "id"),
                leerTexto(opcion, "texto"),
                leerTexto(opcion, "explicacion")
            };
        }

        if (posicion != reactivo.opciones.size())
        {
            throw std::runtime_error(
                "El documento debe contener tres opciones.");
        }

        validarReactivo(reactivo);

        return reactivo;
    }

    // Un filtro vacio {} incluye todos los documentos;
    // con temaId solo incluye los documentos de ese tema.
    bsoncxx::document::value crearFiltroTema(
        const std::string& temaId)
    {
        if (temaId.empty())
        {
            return make_document();
        }

        if (!esObjectId(temaId))
        {
            throw std::invalid_argument(
                "El identificador del tema no es valido.");
        }

        return make_document(
            kvp("temaId", bsoncxx::oid{temaId})
        );
    }

    // Las sesiones nuevas fijan el orden y los ObjectId de sus preguntas.
    std::vector<std::string> leerReactivosSesion(bsoncxx::document::view sesion)
    {
        const auto campo = sesion["reactivoIds"];
        if (!campo || campo.type() != bsoncxx::type::k_array)
        {
            throw std::runtime_error("La sesion no contiene una lista de preguntas valida.");
        }
        std::vector<std::string> ids;
        std::set<std::string> unicos;
        for (const auto& elemento : campo.get_array().value)
        {
            const auto id = elemento.get_oid().value.to_string();
            if (!unicos.insert(id).second || ids.size() >= MaxPreguntasPorSesion)
            {
                throw std::runtime_error("La lista de preguntas de la sesion no es valida.");
            }
            ids.push_back(id);
        }
        if (ids.empty()) throw std::runtime_error("La sesion no contiene preguntas.");
        return ids;
    }
}



mongocxx::client RepositorioMongoDB::crearCliente(
    const std::string& uri)
{
    // static mantiene una sola instancia del controlador hasta terminar el programa.
    // La instancia debe existir antes de construir el cliente.
    static mongocxx::instance instancia{};

    return mongocxx::client{mongocxx::uri{uri}};
}

// Prepara el cliente, selecciona la base y configura los indices.
RepositorioMongoDB::RepositorioMongoDB(
    const std::string& uri,
    const std::string& nombreBaseDatos)
    : cliente_(crearCliente(uri)),
      baseDatos_(cliente_[nombreBaseDatos])
{
    // Exigir confirmacion del servidor para las escrituras.
    mongocxx::write_concern escritura;

    escritura.acknowledge_level(
        mongocxx::write_concern::level::k_acknowledged
    );

    baseDatos_.write_concern(escritura);

    // Impide duplicar una misma combinacion de codigo y version.
    baseDatos_["reactivos"].create_index(
        make_document(
            kvp("codigoReactivo", 1),
            kvp("version", 1)
        ),
        make_document(
            kvp("unique", true),
            kvp("name", "reactivo_codigo_version_unico")
        )
    );

    // Cada codigo identifica un solo tema dentro del banco.
    baseDatos_["temas"].create_index(
        make_document(
            kvp("codigoTema", 1)
        ),
        make_document(
            kvp("unique", true),
            kvp("name", "tema_codigo_unico")
        )
    );

    // Impide duplicar respuestas para la misma combinacion.
    baseDatos_["respuestas"].create_index(
        make_document(
            kvp("perfilId", 1),
            kvp("sesionId", 1),
            kvp("fase", 1),
            kvp("codigoReactivo", 1)
        ),
        make_document(
            kvp("unique", true),
            kvp("name", "respuesta_por_sesion_unica")
        )
    );

    // Permite conservar un solo perfil con el mismo código.
    baseDatos_["perfiles"].create_index(
        make_document(
            kvp("codigoPerfil", 1)
        ),
        make_document(
            kvp("unique", true),
            kvp("name", "perfil_codigo_unico")
        )
    );
}

// El comando ping comprueba que el servidor responde;
// si falla por una excepcion de MongoDB, devuelve false.
bool RepositorioMongoDB::comprobarConexion()
{
    try
    {
        const auto resultado = baseDatos_.run_command(
            make_document(kvp("ping", 1))
        );

        return true;
    }
    catch (const mongocxx::exception&)
    {
        return false;
    }
}

// Registra una version nueva o devuelve el identificador de la version ya guardada.
std::string RepositorioMongoDB::guardarReactivo(
    const Reactivo& reactivo)
{
    validarReactivo(reactivo);

    // Estos campos permiten reconocer el reactivo al volver a cargar el banco.
    const auto filtro = make_document(
        kvp("codigoReactivo", reactivo.codigoReactivo),
        kvp("version", static_cast<std::int32_t>(reactivo.version))
    );

    const auto datos = convertirADocumento(reactivo);

    // $setOnInsert asigna los datos solamente al crear un documento nuevo.
    const auto actualizacion = make_document(
        kvp("$setOnInsert", datos.view())
    );

    mongocxx::options::update opciones;

    // upsert permite insertar cuando el filtro no encuentra una coincidencia.
    opciones.upsert(true);

    auto coleccion = baseDatos_["reactivos"];

    const auto resultado = coleccion.update_one(
        filtro.view(),
        actualizacion.view(),
        opciones
    );

    if (!resultado)
    {
        throw std::runtime_error(
            "El servidor no confirmo el guardado del reactivo.");
    }

    // Recupera el _id tanto si se inserto ahora como si el documento ya existia.
    const auto guardado = coleccion.find_one(filtro.view());

    if (!guardado)
    {
        throw std::runtime_error(
            "No fue posible recuperar el reactivo guardado.");
    }

    return guardado->view()["_id"].get_oid().value.to_string();
}

// std::nullopt representa un identificador invalido
// o una busqueda sin coincidencias.
std::optional<Reactivo> RepositorioMongoDB::buscarReactivo(
    const std::string& id)
{
    if (!esObjectId(id))
    {
        return std::nullopt;
    }

    const auto encontrado = baseDatos_["reactivos"].find_one(
        make_document(kvp("_id", bsoncxx::oid{id}))
    );

    if (!encontrado)
    {
        return std::nullopt;
    }

    return convertirAReactivo(encontrado->view());
}

std::vector<Reactivo> RepositorioMongoDB::listarReactivos(
    const std::string& temaId)
{
    const auto filtro = crearFiltroTema(temaId);

    mongocxx::options::find opciones;

    // El valor 1 indica orden ascendente:
    // primero por codigo y despues por version.
    opciones.sort(
        make_document(
            kvp("codigoReactivo", 1),
            kvp("version", 1)
        )
    );

    std::vector<Reactivo> reactivos;

    // Recorre los documentos encontrados y forma el vector de reactivos de C++.
    for (const auto& documento :
         baseDatos_["reactivos"].find(filtro.view(), opciones))
    {
        reactivos.push_back(convertirAReactivo(documento));
    }

    return reactivos;
}

// Cuenta documentos que coinciden con el filtro.
// Cada version de un reactivo cuenta como un documento.
std::int64_t RepositorioMongoDB::contarReactivos(
    const std::string& temaId)
{
    const auto filtro = crearFiltroTema(temaId);

    return baseDatos_["reactivos"].count_documents(
        filtro.view()
    );
}

std::string RepositorioMongoDB::guardarTema(const Tema& tema)
{
    validarTema(tema);

    // El codigo permite localizar el mismo tema en futuras cargas.
    const auto filtro = make_document(
        kvp("codigoTema", tema.codigoTema)
    );

    const auto datos = make_document(
        kvp("codigoTema", tema.codigoTema),
        kvp("area", static_cast<std::int32_t>(tema.area)),
        kvp("subarea", tema.subarea),
        kvp("nombre", tema.nombre),
        kvp("explicacion", tema.explicacion),
        kvp("ejemplo", tema.ejemplo),
        kvp("fuente", tema.fuente),
        kvp("estadoRevision", tema.estadoRevision)
    );

    // Actualiza los campos del tema conservando su identificador.
    const auto actualizacion = make_document(
        kvp("$set", datos.view())
    );

    mongocxx::options::update opciones;

    // Si el codigo todavia no existe, permite insertar el tema.
    opciones.upsert(true);

    auto coleccion = baseDatos_["temas"];

    const auto resultado = coleccion.update_one(
        filtro.view(),
        actualizacion.view(),
        opciones
    );

    if (!resultado)
    {
        throw std::runtime_error(
            "El servidor no confirmo el guardado del tema.");
    }

    // Recupera el identificador tanto si se inserto como si se actualizo.
    const auto guardado = coleccion.find_one(filtro.view());

    if (!guardado)
    {
        throw std::runtime_error(
            "No fue posible recuperar el tema guardado.");
    }

    return guardado->view()["_id"].get_oid().value.to_string();
}

std::optional<Tema> RepositorioMongoDB::buscarTema(
    const std::string& id)
{
    if (!esObjectId(id))
    {
        return std::nullopt;
    }

    const auto filtro = make_document(
        kvp("_id", bsoncxx::oid{id})
    );

    const auto encontrado = baseDatos_["temas"].find_one(
        filtro.view()
    );

    if (!encontrado)
    {
        return std::nullopt;
    }

    return convertirATema(encontrado->view());
}

std::string RepositorioMongoDB::obtenerOCrearPerfilLocal()
{
    const auto filtro = make_document(
        kvp("codigoPerfil", "local")
    );

    const auto datos = make_document(
        kvp("codigoPerfil", "local"),
        kvp("nombre", "Perfil local"),
        kvp("fechaCreacion",
            bsoncxx::types::b_date{std::chrono::system_clock::now()})
    );

    // Conserva el mismo perfil cuando volvemos a ejecutar el programa.
    const auto actualizacion = make_document(
        kvp("$setOnInsert", datos.view())
    );

    mongocxx::options::update opciones;
    opciones.upsert(true);

    auto coleccion = baseDatos_["perfiles"];

    const auto resultado = coleccion.update_one(
        filtro.view(), actualizacion.view(), opciones
    );

    if (!resultado)
    {
        throw std::runtime_error(
            "El servidor no confirmo el registro del perfil.");
    }

    const auto perfil = coleccion.find_one(filtro.view());

    if (!perfil)
    {
        throw std::runtime_error("No fue posible recuperar el perfil.");
    }

    return perfil->view()["_id"].get_oid().value.to_string();
}

std::string RepositorioMongoDB::crearSesionPrueba(
    const std::string& perfilId,
    const std::string& reactivoId)
{
    if (!esObjectId(perfilId) || !esObjectId(reactivoId))
    {
        throw std::invalid_argument(
            "El identificador del perfil o del reactivo no es valido.");
    }

    const auto perfil = baseDatos_["perfiles"].find_one(
        make_document(kvp("_id", bsoncxx::oid{perfilId}))
    );

    if (!perfil || !buscarReactivo(reactivoId))
    {
        throw std::invalid_argument(
            "El perfil o el reactivo no existen.");
    }

    // Esta sesion tecnica contiene una sola pregunta.
    const auto datos = make_document(
        kvp("perfilId", bsoncxx::oid{perfilId}),
        kvp("reactivoId", bsoncxx::oid{reactivoId}),
        kvp("tipo", "prueba_tecnica"),
        kvp("fase", "practica"),
        kvp("metodoSeleccion", "prueba_tecnica"),
        kvp("fechaInicio",
            bsoncxx::types::b_date{std::chrono::system_clock::now()})
    );

    const auto resultado = baseDatos_["sesiones"].insert_one(
        datos.view()
    );

    if (!resultado)
    {
        throw std::runtime_error(
            "El servidor no confirmo la creacion de la sesion.");
    }

    return resultado->inserted_id().get_oid().value.to_string();
}

std::string RepositorioMongoDB::guardarRespuesta(
    const Respuesta& respuesta)
{
    if (!esObjectId(respuesta.perfilId) ||
        !esObjectId(respuesta.sesionId) ||
        !esObjectId(respuesta.reactivoId))
    {
        throw std::invalid_argument(
            "Los identificadores de perfil, sesion y reactivo no son validos.");
    }

    if (respuesta.confianza != "con_seguridad" &&
        respuesta.confianza != "con_duda" &&
        respuesta.confianza != "sin_indicar")
    {
        throw std::invalid_argument(
            "La confianza debe ser con_seguridad, con_duda o sin_indicar.");
    }

    if (respuesta.fase != "inicial" &&
        respuesta.fase != "practica" &&
        respuesta.fase != "final")
    {
        throw std::invalid_argument("La fase no es valida.");
    }

    if (respuesta.metodoSeleccion.empty() ||
        respuesta.tiempoRespuestaMs < 0)
    {
        throw std::invalid_argument(
            "Falta el metodo de seleccion o el tiempo no es valido.");
    }

    const auto perfil = baseDatos_["perfiles"].find_one(
        make_document(
            kvp("_id", bsoncxx::oid{respuesta.perfilId})
        )
    );

    // Primero comprueba el contexto; abajo revisa la pertenencia de la pregunta.
    const auto sesion = baseDatos_["sesiones"].find_one(
        make_document(
            kvp("_id", bsoncxx::oid{respuesta.sesionId}),
            kvp("perfilId", bsoncxx::oid{respuesta.perfilId}),
            kvp("fase", respuesta.fase),
            kvp("metodoSeleccion", respuesta.metodoSeleccion)
        )
    );

    if (!perfil || !sesion)
    {
        throw std::invalid_argument(
            "El perfil o los datos de su sesion no coinciden.");
    }

    const auto contexto = sesion->view();
    const bool multiple = static_cast<bool>(contexto["reactivoIds"]);
    std::vector<std::string> asignadas;
    if (multiple)
    {
        if (leerTexto(contexto, "tipo") != "practica_areas" ||
            respuesta.metodoSeleccion != "catalogo_por_areas" ||
            respuesta.fase != "practica")
        {
            throw std::invalid_argument("El contexto de la sesion de practica no es valido.");
        }
        asignadas = leerReactivosSesion(contexto);
        bool pertenece = false;
        for (const auto& id : asignadas)
        {
            if (bsoncxx::oid{id} == bsoncxx::oid{respuesta.reactivoId}) pertenece = true;
        }
        if (!pertenece)
        {
            throw std::invalid_argument("La pregunta no pertenece a esta sesion.");
        }
    }
    else if (!contexto["reactivoId"] ||
        contexto["reactivoId"].get_oid().value != bsoncxx::oid{respuesta.reactivoId})
    {
        // Tambien admite las sesiones individuales guardadas anteriormente.
        throw std::invalid_argument("La pregunta no pertenece a la sesion de prueba.");
    }

    const auto reactivo = buscarReactivo(respuesta.reactivoId);

    if (!reactivo)
    {
        throw std::invalid_argument("El reactivo no existe.");
    }

    const auto tema = buscarTema(reactivo->temaId);

    if (!tema)
    {
        throw std::runtime_error("El tema del reactivo no existe.");
    }

    // Verifica que la eleccion y el orden mostrado correspondan al reactivo.
    std::set<std::string> identificadores;

    for (const auto& opcion : reactivo->opciones)
    {
        identificadores.insert(opcion.id);
    }

    const std::set<std::string> presentados{
        respuesta.ordenOpciones.begin(),
        respuesta.ordenOpciones.end()
    };

    if (identificadores.count(respuesta.opcionSeleccionadaId) != 1 ||
        presentados != identificadores)
    {
        throw std::invalid_argument(
            "La opcion elegida o el orden de las opciones no son validos.");
    }

    // El resultado se calcula con la solucion almacenada en MongoDB.
    const bool correcta =
        respuesta.opcionSeleccionadaId == reactivo->opcionCorrectaId;

    bsoncxx::builder::basic::array orden;

    for (const auto& identificador : respuesta.ordenOpciones)
    {
        orden.append(identificador);
    }

    const auto filtro = make_document(
        kvp("perfilId", bsoncxx::oid{respuesta.perfilId}),
        kvp("sesionId", bsoncxx::oid{respuesta.sesionId}),
        kvp("fase", respuesta.fase),
        kvp("codigoReactivo", reactivo->codigoReactivo)
    );

    auto coleccion = baseDatos_["respuestas"];
    const auto existente = coleccion.find_one(filtro.view());
    if (existente)
    {
        // Incluso despues de finalizar, un reintento recupera la primera respuesta.
        if (existente->view()["reactivoId"].get_oid().value != bsoncxx::oid{reactivo->id})
        {
            throw std::runtime_error("La sesion ya contiene otra version de esta pregunta.");
        }
        return existente->view()["_id"].get_oid().value.to_string();
    }

    if (multiple)
    {
        if (leerTexto(contexto, "estado") != "en_curso")
        {
            throw std::logic_error("La sesion ya esta finalizada.");
        }
        std::set<std::string> contestadas;
        for (const auto& documento : coleccion.find(make_document(
            kvp("sesionId", bsoncxx::oid{respuesta.sesionId}),
            kvp("perfilId", bsoncxx::oid{respuesta.perfilId}),
            kvp("fase", respuesta.fase))))
        {
            contestadas.insert(documento["reactivoId"].get_oid().value.to_string());
        }
        for (const auto& id : asignadas)
        {
            if (id == reactivo->id) break;
            if (contestadas.count(id) == 0)
            {
                throw std::logic_error("Primero debes confirmar las preguntas anteriores.");
            }
        }
    }

    // La clasificacion y los puntos se obtienen de los registros consultados.
    const auto datos = make_document(
        kvp("perfilId", bsoncxx::oid{respuesta.perfilId}),
        kvp("sesionId", bsoncxx::oid{respuesta.sesionId}),
        kvp("fase", respuesta.fase),
        kvp("metodoSeleccion", respuesta.metodoSeleccion),
        kvp("reactivoId", bsoncxx::oid{reactivo->id}),
        kvp("codigoReactivo", reactivo->codigoReactivo),
        kvp("versionReactivo",
            static_cast<std::int32_t>(reactivo->version)),
        kvp("temaId", bsoncxx::oid{tema->id}),
        kvp("area", static_cast<std::int32_t>(tema->area)),
        kvp("subarea", tema->subarea),
        kvp("opcionSeleccionadaId", respuesta.opcionSeleccionadaId),
        kvp("ordenOpciones", orden.extract()),
        kvp("confianza", respuesta.confianza),
        kvp("correcta", correcta),
        kvp("puntos", static_cast<std::int32_t>(correcta ? 1 : 0)),
        kvp("fechaHora",
            bsoncxx::types::b_date{std::chrono::system_clock::now()}),
        kvp("tiempoRespuestaMs", respuesta.tiempoRespuestaMs)
    );

    // Conserva la primera respuesta cuando se repite el mismo envio.
    const auto actualizacion = make_document(
        kvp("$setOnInsert", datos.view())
    );

    mongocxx::options::update opciones;
    opciones.upsert(true);

    const auto resultado = coleccion.update_one(
        filtro.view(),
        actualizacion.view(),
        opciones
    );

    if (!resultado)
    {
        throw std::runtime_error(
            "El servidor no confirmo el registro de la respuesta.");
    }

    const auto guardada = coleccion.find_one(filtro.view());

    if (!guardada)
    {
        throw std::runtime_error(
            "No fue posible recuperar la respuesta registrada.");
    }

    return guardada->view()["_id"].get_oid().value.to_string();
}

std::optional<Respuesta> RepositorioMongoDB::buscarRespuesta(
    const std::string& id)
{
    if (!esObjectId(id))
    {
        return std::nullopt;
    }

    const auto encontrada = baseDatos_["respuestas"].find_one(
        make_document(
            kvp("_id", bsoncxx::oid{id})
        )
    );

    if (!encontrada)
    {
        return std::nullopt;
    }

    return convertirARespuesta(encontrada->view());
}

std::string RepositorioMongoDB::crearSesionPractica(
    const std::string& perfilId,
    const std::vector<std::string>& reactivoIds)
{
    if (!esObjectId(perfilId) || reactivoIds.empty() ||
        reactivoIds.size() > MaxPreguntasPorSesion)
    {
        throw std::invalid_argument("La sesion necesita un perfil y de 1 a " +
            std::to_string(MaxPreguntasPorSesion) + " preguntas.");
    }
    if (!baseDatos_["perfiles"].find_one(make_document(kvp("_id", bsoncxx::oid{perfilId}))))
    {
        throw std::invalid_argument("El perfil no existe.");
    }

    bsoncxx::builder::basic::array ids;
    std::set<std::string> codigos;
    for (const auto& id : reactivoIds)
    {
        const auto reactivo = buscarReactivo(id);
        if (!reactivo || reactivo->usoBanco != "practica" ||
            !buscarTema(reactivo->temaId))
        {
            throw std::invalid_argument("Una pregunta o su tema no estan disponibles.");
        }
        if (!codigos.insert(reactivo->codigoReactivo).second)
        {
            throw std::invalid_argument("La sesion no puede repetir un codigo de pregunta.");
        }
        ids.append(bsoncxx::oid{reactivo->id});
    }

    // La lista conserva la version exacta de cada pregunta aunque el banco crezca.
    const auto datos = make_document(
        kvp("perfilId", bsoncxx::oid{perfilId}),
        kvp("reactivoIds", ids.extract()),
        kvp("tipo", "practica_areas"),
        kvp("fase", "practica"),
        kvp("metodoSeleccion", "catalogo_por_areas"),
        kvp("estado", "en_curso"),
        kvp("fechaInicio", bsoncxx::types::b_date{std::chrono::system_clock::now()})
    );
    const auto resultado = baseDatos_["sesiones"].insert_one(datos.view());
    if (!resultado) throw std::runtime_error("No se confirmo la creacion de la sesion.");
    return resultado->inserted_id().get_oid().value.to_string();
}

ResumenPractica RepositorioMongoDB::obtenerResumenSesion(const std::string& sesionId)
{
    if (!esObjectId(sesionId)) throw std::invalid_argument("El ID de sesion no es valido.");
    const auto sesion = baseDatos_["sesiones"].find_one(make_document(
        kvp("_id", bsoncxx::oid{sesionId}),
        kvp("tipo", "practica_areas"),
        kvp("fase", "practica"),
        kvp("metodoSeleccion", "catalogo_por_areas")
    ));
    if (!sesion) throw std::invalid_argument("La sesion de practica no existe.");

    const auto contexto = sesion->view();
    const auto asignadas = leerReactivosSesion(contexto);
    const auto estado = leerTexto(contexto, "estado");
    if (estado != "en_curso" && estado != "finalizada")
    {
        throw std::runtime_error("El estado de la sesion no es valido.");
    }

    std::vector<Respuesta> respuestas;

    // Se suman registros confirmados; la ventana no proporciona la calificacion.
    for (const auto& documento : baseDatos_["respuestas"].find(make_document(
        kvp("sesionId", bsoncxx::oid{sesionId}),
        kvp("perfilId", contexto["perfilId"].get_oid().value),
        kvp("fase", "practica"))))
    {
        respuestas.push_back(convertirARespuesta(documento));
    }

    return calcularResumenPractica(contexto["_id"].get_oid().value.to_string(),
        asignadas, respuestas, estado == "finalizada");
}

ResumenPractica RepositorioMongoDB::finalizarSesion(const std::string& sesionId)
{
    const auto previo = obtenerResumenSesion(sesionId);
    if (previo.respondidas != previo.totalPreguntas)
    {
        throw std::logic_error("Todavia faltan preguntas por responder.");
    }
    if (previo.finalizada) return previo;

    // Solo el primer cierre asigna fechaFin; reintentar no la cambia.
    const auto resultado = baseDatos_["sesiones"].update_one(
        make_document(kvp("_id", bsoncxx::oid{sesionId}), kvp("estado", "en_curso")),
        make_document(kvp("$set", make_document(
            kvp("estado", "finalizada"),
            kvp("fechaFin", bsoncxx::types::b_date{std::chrono::system_clock::now()})
        )))
    );
    if (!resultado) throw std::runtime_error("No se confirmo el cierre de la sesion.");

    const auto resumen = obtenerResumenSesion(sesionId);
    if (!resumen.finalizada)
    {
        throw std::runtime_error("No fue posible confirmar el cierre de la sesion.");
    }
    return resumen;
}
