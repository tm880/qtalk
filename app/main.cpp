#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QCoreApplication::setOrganizationName("chloerei");
    QCoreApplication::setOrganizationDomain("chloerei.com");
    QCoreApplication::setApplicationName("qtalk");

    MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
