#include <QApplication>
#include <QLabel>
#include <QHttpServer>

int main(int argc, char* argv[])
{
    QApplication aplicacion(argc, argv);

    QHttpServer servidor;

    QLabel ventana("Prueba de C++ y Qt para el entrenador EGEL");
    ventana.setWindowTitle("Entrenador EGEL - Prueba de entorno");
    ventana.setAlignment(Qt::AlignCenter);
    ventana.resize(520, 160);
    ventana.show();

    return aplicacion.exec();
}