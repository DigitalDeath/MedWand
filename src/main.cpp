#include "main_window.h"
#include <QApplication>

int main(int argc, char **argv) {
    mw::configureGraphics();
    QApplication app(argc, argv);
    mw::MainWindow window;
    window.show();
    return app.exec();
}
