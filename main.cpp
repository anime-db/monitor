#include "TrayIcon.h"

#include <QDir>
#include <QTranslator>
#include <QLocale>
#include <QMessageBox>
#include <QApplication>
#include <QSharedMemory>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QSharedMemory memory("anime_monitor", &a);

    if (memory.attach(QSharedMemory::ReadOnly)) {
        memory.detach();
        return 1;
    }

    if (memory.create(1)) {
        memory.attach(QSharedMemory::ReadOnly);
    }

    QTranslator translator;

    QLocale curent;
    if (curent.country() == QLocale::RussianFederation) {
        translator.load(QDir::cleanPath(QCoreApplication::applicationDirPath()+"/./ru.qm"));
        a.installTranslator(&translator);
    }

    TrayIcon trayIcon;
    trayIcon.show();

    int execRes = a.exec();

    if(memory.isAttached())
        memory.detach();

    return execRes;
}
