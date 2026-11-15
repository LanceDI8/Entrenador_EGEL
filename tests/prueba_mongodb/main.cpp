#include <exception>
#include <iostream>

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>

int main()
{
    try
    {
        mongocxx::instance instancia{};

        mongocxx::client cliente{
            mongocxx::uri{
                "mongodb://127.0.0.1:27017/"
                "?serverSelectionTimeoutMS=5000"
            }
        };

        auto comando = bsoncxx::builder::basic::make_document(
            bsoncxx::builder::basic::kvp("ping", 1)
        );

        auto resultado = cliente["admin"].run_command(
            comando.view()
        );

        std::cout << "Conexion con MongoDB correcta.\n";
        std::cout << bsoncxx::to_json(resultado.view()) << '\n';

        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Error al conectar con MongoDB: "
                  << error.what() << '\n';

        return 1;
    }
}