#include "TrayIcon.h"
#include "MSettings.h"

#include <QPainter>
#include <QMessageBox>
#include <QApplication>
#include <QCursor>
#include <QUrl>
#include <QDesktopWidget>
#include <QDesktopServices>
#include <QDir>
#include <QProcess>
#include <QCoreApplication>
#include <QIcon>
#include <QMenu>
#include <QTimer>
#include <QAction>
#include <QDebug>

TrayIcon::TrayIcon(QObject *parent) :
    QSystemTrayIcon(parent),
    routerProc(this),
    taskSchedulerProc(this)
{
    setIcon(QPixmap(":/icons/tray.png"));
    setToolTip("Anime DB");

    appDirPath = QCoreApplication::applicationDirPath() + "/";

    addr = MSettings::value("addr", "0.0.0.0").toString();
    port = MSettings::value("port", "56780").toString();
    pathToPhp = MSettings::value("php", "./bin\\php\\php.exe").toString();

    if (pathToPhp.startsWith("./")) {
        pathToPhp = QDir::cleanPath(appDirPath + pathToPhp);
    }

    QRegExp pathChecking("[^A-Za-z0-9\\.\\:\\/\\[\\]\\(\\)\\-\\_\\\\]",
                         Qt::CaseInsensitive);

    /*    Checking the app path    */;
    if (appDirPath.contains(pathChecking)) {
        QMessageBox::critical(0, tr("Error. Anime DB"),
                              tr("The application can not be run in the current folder. Move it to another folder, for example, in the folder: C:\\"));
        QTimer::singleShot(10, qApp, SLOT(quit()));

    } else {
        QString lastAppPath = MSettings::value("lastAppPath", appDirPath).toString();

        if (lastAppPath != appDirPath) {
            clearCash();
            MSettings::setValue("lastAppPath", appDirPath);
        }
    }


    QMenu * menu = new QMenu();
    setContextMenu(menu);

    QAction * action;

    action = new QAction(tr("About"), this);
    connect(action, SIGNAL(triggered()), this, SLOT(aboutSlot()));
    menu->addAction(action);

    action = new QAction(tr("Exit"), this);
    connect(action, SIGNAL(triggered()), QCoreApplication::instance(),
            SLOT(quit()));
    menu->addAction(action);

    connect(this, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this,
            SLOT(activatedSlot(QSystemTrayIcon::ActivationReason)));

    leftMenu = new QMenu();

    action = new QAction(tr("Start"), this);
    connect(action, SIGNAL(triggered()), this, SLOT(startSlot()));
    leftMenu->addAction(action);

    action = new QAction(tr("Stop"), this);
    connect(action, SIGNAL(triggered()), this, SLOT(stopSlot()));
    leftMenu->addAction(action);

    action = new QAction(tr("Restart"), this);
    connect(action, SIGNAL(triggered()), this, SLOT(restartSlot()));
    leftMenu->addAction(action);
    QTimer::singleShot(1000, this, SLOT(startSlot()));
}

void TrayIcon::startSlot() {
    lunchProcesses();
    openUrl(QString("http://%1:%2/").arg("localhost").arg(port));
}

void TrayIcon::stopSlot() {
    routerProc.kill();
    taskSchedulerProc.kill();
}

void TrayIcon::restartSlot() {
    stopSlot();
    QTimer::singleShot(1000, this, SLOT(lunchProcesses()));

}

void TrayIcon::activatedSlot(QSystemTrayIcon::ActivationReason reason) {
    if (reason == 3) { // left click on the icon.
        leftMenu->show();

        QPoint cursor = QCursor::pos();
        cursor.ry() -= leftMenu->rect().height();

        int overrade = cursor.x() + leftMenu->rect().width()
                - QApplication::desktop()->width();
        if (overrade > 0)
            cursor.rx() -= overrade;

        if(cursor.y() - leftMenu->rect().height() < 0)
            cursor.ry() += leftMenu->rect().height();

        leftMenu->move(cursor);
    }
}

void TrayIcon::aboutSlot() {
    openUrl("http://anime-db.org/");
}

void TrayIcon::clearCash() {
    QStringList params;
    params << QDir::cleanPath(appDirPath + "./app/console");
    params << "cache:clear";
    params << "--env=prod";
    params << "--no-debug";

    if (QProcess::execute(pathToPhp, params)) {
        QMessageBox::critical(0, tr("Error. Anime DB"),
                              tr("Could not clear application cache. Delete the folder app/cache/ and try again."));
        QTimer::singleShot(100, qApp, SLOT(quit()));
    }

}

void TrayIcon::lunchProcesses() {

    if (routerProc.state() != QProcess::NotRunning
            || taskSchedulerProc.state() != QProcess::NotRunning)
        return;

    QString routerProcCommand = pathToPhp + " -S " + addr + ":" + port +
            " -t " + QDir::cleanPath(appDirPath + "./web") + " " +
            QDir::cleanPath(appDirPath + "./app/router.php") + " >nul 2>&1";

    QString taskSchedulerProcCommand = pathToPhp + " -f " +
            QDir::cleanPath(appDirPath + "./app/console") +
            " animedb:task-scheduler";

    routerProc.start(routerProcCommand);
    taskSchedulerProc.start(taskSchedulerProcCommand);
}

void TrayIcon::openUrl(QString url) {
    QDesktopServices::openUrl(QUrl(url));
}
