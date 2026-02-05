#include "MainWindow.h"
#include <QApplication>
#include <QFont>
#include <QFile>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
        QString path = "C:\\Users\\m1760\\Desktop\\heightVision\\res\\dark_style.qss";
    bool qssLoaded = false;
    QFile qssFile(path);
    if (qssFile.open(QFile::ReadOnly | QFile::Text)) {
        QString m_loadedStyleSheet = QString::fromUtf8(qssFile.readAll());
        a.setStyleSheet(m_loadedStyleSheet);
        qssFile.close();
        qDebug() << "加载样式表成功:" << path;
        qssLoaded = true;
    }
    if (!qssLoaded) {
        qWarning() << "样式表加载失败：请确认已编译 resources.qrc，或将 dark_style.qss 放到宿主程序目录的 res 目录下";
    }

    // QFont font("Microsoft YaHei");
    // font.setPointSize(9);
    // a.setFont(font);
    MainWindow w;
    w.show();
    return a.exec();

}