#include "qv_mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
   // QApplication::setStyle("fusion");
    QV_MainWindow core;
    core.show();
    return a.exec();
}


