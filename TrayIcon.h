#ifndef TRAYICON_H
#define TRAYICON_H

#include <QSystemTrayIcon>
#include <QProcess>

class QMenu;

class TrayIcon : public QSystemTrayIcon
{
    Q_OBJECT
public:
    explicit TrayIcon(QObject *parent = 0);

signals:

public slots:
    void activatedSlot(QSystemTrayIcon::ActivationReason);

    void startSlot();
    void stopSlot();
    void restartSlot();
    void lunchProcesses();

    void aboutSlot();

private:
    void clearCash();
    void openUrl(QString);

    QString appDirPath;
    bool isPathChanged();
    QMenu * leftMenu;
    QString addr;
    QString port;
    QString pathToPhp;

    QProcess routerProc;
    QProcess taskSchedulerProc;
};

#endif // TRAYICON_H
