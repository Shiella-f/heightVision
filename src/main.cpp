#include "MainWindow.h"
#include <QApplication>
#include <QFont>
#include <QFile>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QFont font("Microsoft YaHei");
    font.setPointSize(9);
    a.setFont(font);
    MainWindow w;
    w.show();
    return a.exec();

}