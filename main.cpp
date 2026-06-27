#include <QApplication>
#include <QStyleHints>
#include "mainwindow.h"

using chess::MainWindow;

int main(int argc, char *argv[])
{
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    QApplication::setApplicationName("Chinese Chess");
    QApplication::setOrganizationName("ZCode");

    MainWindow w;
    w.show();
    return app.exec();
}
