---
title: Техническая разборка monitor
tags: [anime-db/monitor, technical, deep-dive]
created: 2026-06-18
---

# Техническая разборка `anime-db/monitor`

Подробный разбор каждой единицы трансляции, рантайма, сборки и ресурсов. Обзор устройства — в [PROJECT.md](PROJECT.md); найденные дефекты — в [BUGS.md](BUGS.md).

## Стек

| Компонент  | Значение                                               |
|------------|--------------------------------------------------------|
| Язык       | C++ (стиль ближе к C++03/Qt4-эпохи)                    |
| Фреймворк  | Qt 4 или Qt 5 (`QT += core gui`; `widgets` для Qt5+)   |
| Сборка     | qmake → Makefile → `make`/`nmake`/`jom`/`mingw32-make` |
| IDE        | Qt Creator (есть `anime_monitor.pro.user`)             |
| Переводы   | Qt Linguist (`lupdate`/`lrelease`, `ru.ts` → `ru.qm`)  |
| Платформа  | Только Windows                                         |

## Сборка

```bash
qmake anime_monitor.pro      # генерация Makefile из .pro
make                         # сборка (Windows: nmake / jom / mingw32-make)
lrelease anime_monitor.pro   # ru.ts -> ru.qm после правки строк
lupdate anime_monitor.pro    # извлечь tr()-строки из кода в ru.ts
```

`anime_monitor.pro` — единственный источник истины по составу проекта:

```pro
QT       += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets   # Qt5: widgets вынесены из gui
TARGET   = anime_monitor
TEMPLATE = app
RC_FILE  = myapp.rc                               # иконка exe (Windows)
SOURCES  += main.cpp TrayIcon.cpp MSettings.cpp
HEADERS  += TrayIcon.h MSettings.h
RESOURCES += icons.qrc
```

## `main.cpp` — точка входа

### Гарантия единственного экземпляра

```cpp
QSharedMemory memory("anime_monitor", &a);
if (memory.attach(QSharedMemory::ReadOnly)) {   // сегмент уже есть → копия запущена
    memory.detach();
    return 1;                                    // выход
}
if (memory.create(1)) {                          // 1 байт-маркер
    memory.attach(QSharedMemory::ReadOnly);
}
```

- Идентичность экземпляра — по **ключу** `"anime_monitor"`.
- При выходе сегмент освобождается (`detach`). На Windows ОС освобождает сегмент при завершении последнего процесса, поэтому «осиротевший» маркер после краха не блокирует повторный запуск (в отличие от POSIX, но проект Windows-only).
- Между `attach` и `create` есть теоретический TOCTOU-зазор, но для десктоп-трея это несущественно.

### Локализация

```cpp
QLocale curent;
if (curent.country() == QLocale::RussianFederation) {     // ⚠ по СТРАНЕ, не по языку
    translator.load(QDir::cleanPath(applicationDirPath()+"/./ru.qm"));
    a.installTranslator(&translator);
}
```

`translator` — локальная переменная `main()`, живёт до возврата из `a.exec()`. Загрузка перевода привязана к **стране**, а не к языку — баг локализации, см. [BUGS.md#b4](BUGS.md).

## `TrayIcon` — ядро

Наследник `QSystemTrayIcon`. Два дочерних процесса — **value-члены** класса:

```cpp
QProcess routerProc;        // встроенный веб-сервер PHP
QProcess taskSchedulerProc; // планировщик задач Symfony
```

В конструкторе им передаётся `this` как QObject-родитель (`routerProc(this)`). Это безопасно: при разрушении дочерний `QObject` сам снимается со списка детей родителя, поэтому двойного удаления value-члена не происходит.

### Конструктор: проверки и меню

1. **Иконка и tooltip.** `:/icons/tray.png`, tooltip `"Anime DB"`.
2. **Путь приложения** `appDirPath = applicationDirPath() + "/"`.
3. **Путь к PHP.** Из настроек, дефолт `./bin\php\php.exe`; если начинается с `./`, резолвится от `appDirPath` через `QDir::cleanPath`.
4. **Валидация пути** регуляркой `[^A-Za-z0-9\.\:\/\[\]\(\)\-\_\\]` (case-insensitive). Любой символ вне ASCII-набора (пробел, кириллица, спецсимволы) → критическая ошибка и выход. Причина: встроенный сервер PHP не работает с такими путями. Это **защита, а не баг**.
5. **Сброс кэша при переезде.** Если `lastAppPath != appDirPath`, вызывается `clearCash()` и сохраняется новый `lastAppPath`. Иначе Symfony падает на устаревших абсолютных путях в `app/cache/`.
6. **Меню.** Контекстное (правый клик): About / Exit. Отдельное `leftMenu`: Start / Stop / Restart.
7. **Автозапуск.** `QTimer::singleShot(1000, this, SLOT(startSlot()))`.

### Слоты управления

```cpp
void TrayIcon::startSlot() {
    lunchProcesses();                                   // (sic) launch
    openUrl(QString("http://%1:%2/").arg("localhost").arg(port));  // ⚠ сразу, до готовности
}
void TrayIcon::stopSlot()  { routerProc.kill(); taskSchedulerProc.kill(); }
void TrayIcon::restartSlot(){ stopSlot(); QTimer::singleShot(1000, this, SLOT(lunchProcesses())); }
```

- `startSlot` открывает браузер **сразу** после запуска процессов — гонка, см. [BUGS.md#b5](BUGS.md).
- `restartSlot` НЕ переоткрывает URL (в отличие от `startSlot`) — несогласованность, см. [BUGS.md#b10](BUGS.md).

### Левый клик: магическое число

```cpp
void TrayIcon::activatedSlot(QSystemTrayIcon::ActivationReason reason) {
    if (reason == 3) { // == QSystemTrayIcon::Trigger (левый клик)
        leftMenu->show();
        // ручное позиционирование меню у курсора с коррекцией выхода за край экрана
    }
}
```

`3` — это `QSystemTrayIcon::Trigger`. Использование литерала вместо enum — запах кода, см. [RECOMMENDATIONS.md](RECOMMENDATIONS.md).

### Запуск процессов — ключевое место и источник багов

```cpp
void TrayIcon::lunchProcesses() {
    if (routerProc.state() != QProcess::NotRunning
            || taskSchedulerProc.state() != QProcess::NotRunning)
        return;                                         // ⚠ выход если ЛЮБОЙ жив (см. BUGS#b7)

    QString routerProcCommand = pathToPhp + " -S " + addr + ":" + port +
            " -t " + QDir::cleanPath(appDirPath + "./web") + " " +
            QDir::cleanPath(appDirPath + "./app/router.php") + " >nul 2>&1";  // ⚠ см. BUGS#b2

    QString taskSchedulerProcCommand = pathToPhp + " -f " +
            QDir::cleanPath(appDirPath + "./app/console") + " animedb:task-scheduler";

    routerProc.start(routerProcCommand);
    taskSchedulerProc.start(taskSchedulerProcCommand);
}
```

**Критично понимать семантику `QProcess::start(const QString &command)`:** этот перегруз **не запускает шелл**. Qt сам разбивает строку на программу и аргументы по пробелам (с учётом кавычек) и вызывает `CreateProcess` напрямую. Следствия:

- `>nul 2>&1` **не является редиректом** — токены `>nul` и `2>&1` попадают в `php` как обычные аргументы командной строки. Вывод сервера не подавляется, а PHP может ругнуться на лишние аргументы. См. [BUGS.md#b2](BUGS.md).
- Если в `pathToPhp`, `addr` или `port` окажется пробел/спецсимвол (значения берутся из `config.ini`), разбиение сломается, возможна инъекция аргументов. `appDirPath` валидируется, а эти значения — нет. См. [BUGS.md](BUGS.md).

### Очистка кэша

```cpp
void TrayIcon::clearCash() {
    QStringList params;
    params << QDir::cleanPath(appDirPath + "./app/console")
           << "cache:clear" << "--env=prod" << "--no-debug";
    if (QProcess::execute(pathToPhp, params)) {         // блокирующий вызов
        // критическая ошибка + выход
    }
}
```

Здесь использован **правильный** перегруз `execute(program, QStringList)` — без шелла, аргументы передаются списком. Любой ненулевой код возврата (включая `-2` «не удалось запустить» и `-1` «крах») трактуется как ошибка очистки кэша.

## `MSettings` — настройки

Синглтон поверх `QSettings`:

```cpp
MSettings::MSettings() : QSettings("config.ini", QSettings::IniFormat, 0) { }
```

- **Путь `"config.ini"` — относительный** и резолвится `QSettings` от **текущего рабочего каталога процесса (CWD)**, а НЕ от `applicationDirPath()`. При запуске из ярлыка с заданным «Рабочей папкой» файл окажется не там, где exe. См. [BUGS.md#b3](BUGS.md).
- `value()` имеет **побочный эффект**: при отсутствии ключа записывает дефолт обратно в файл и возвращает его. То есть чтение настройки может изменить `config.ini`. Поведение намеренное (материализация дефолтов), но неожиданное.
- Синглтон `ptr` никогда не освобождается — утечка на время жизни процесса, безвредная.
- `//#include "MLogger.h"` и закомментированные `MLogger::info(...)` — следы вырезанного логгера. Логирования в проекте нет.

## Интернационализация

- Исходные строки в коде — английские ASCII, обёрнуты в `tr()`.
- `ru.ts` — исходник перевода (Qt Linguist), привязан к `<location filename=... line=...>`. При сдвиге строк в `TrayIcon.cpp` локации устаревают — после правок нужно `lupdate` + `lrelease`.
- `ru.qm` — скомпилированный бинарь, грузится в рантайме.
- Все переводы помечены `type="unfinished"` — формально перевод не «подтверждён» в Linguist, но строки заполнены и работают.

## Ресурсы и метаданные

| Артефакт               | Механизм                         | Что даёт                                                 |
|------------------------|----------------------------------|----------------------------------------------------------|
| `icons.qrc`            | Qt Resource System (`RESOURCES`) | `:/icons/tray.png`, `:/icons/favicon.ico` встроены в exe |
| `myapp.rc` + `RC_FILE` | Windows Resource Compiler        | Иконка самого `anime_monitor.exe`                        |
| `favicon.ico` (158 КБ) | И в qrc, и в .rc                 | Крупный файл — кандидат на оптимизацию                   |
| `tray.png` (685 Б)     | qrc                              | Иконка в трее                                            |

## Рантайм-зависимости (вне репозитория)

`monitor` во время выполнения ожидает рядом с exe:
- `bin/php/php.exe` — интерпретатор PHP;
- `web/` и `app/router.php` — фронт-контроллер веб-приложения;
- `app/console` — Symfony-консоль;
- `app/cache/` — каталог кэша (чистится при переезде).

Эти артефакты поставляются дистрибутивом Anime DB, а не данным репозиторием.
