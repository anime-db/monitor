# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

`monitor` — десктопная утилита-лаунчер для Windows (иконка в System Tray), запускающая локальную инсталляцию Anime DB. Компонент проекта [anime-db](https://github.com/anime-db) (репозиторий `anime-db/monitor`).

## Документация-индекс

Полный аудит и описание проекта — в [`docs/`](docs/index.md):

- [docs/index.md](docs/index.md) — карта документов
- [docs/AUDIT.md](docs/AUDIT.md) — сводка аудита: вердикт, оценки, топ находок
- [docs/PROJECT.md](docs/PROJECT.md) — устройство проекта и место в экосистеме anime-db
- [docs/TECHNICAL.md](docs/TECHNICAL.md) — глубокая техническая разборка
- [docs/BUGS.md](docs/BUGS.md) — найденные баги с серьёзностью и фиксами
- [docs/RECOMMENDATIONS.md](docs/RECOMMENDATIONS.md) — улучшения: безопасность, поддерживаемость, гигиена

## Agent docs (.claude-docs/)

Толстый слой знаний для агентов (читать по необходимости). Перед правкой `TrayIcon.cpp`/`MSettings.cpp`/`main.cpp` сначала смотри `gotchas.md`.

| Файл                                                 | Что внутри                                                                       |
|------------------------------------------------------|----------------------------------------------------------------------------------|
| [.claude-docs/index.md](.claude-docs/index.md)       | Таблица маршрутизации: какой файл читать для какой задачи                        |
| [.claude-docs/gotchas.md](.claude-docs/gotchas.md)   | Нетривиальные ловушки: «выглядит понятно, но ломается» / «странно, но намеренно» |
| [.claude-docs/glossary.md](.claude-docs/glossary.md) | Имена и термины, легко трактуемые неверно (опечатки-символы, нейминг)            |
| [.claude-docs/context.md](.claude-docs/context.md)   | Кто вызывает, модель угроз, легаси-решения и их статус                           |

## Назначение

Это **не само приложение**, а супервизор. Anime DB — это Symfony/PHP-приложение, которое monitor поднимает локально через встроенный веб-сервер PHP и открывает в браузере. Монитор управляет двумя дочерними процессами PHP и живёт в трее.

## Стек и сборка

- **Qt** (Qt4/Qt5, модули `core gui widgets`), C++, система сборки **qmake**.
- Целевая платформа — **только Windows** (пути с `\`, `>nul 2>&1`, `*.exe`); кроссплатформенной поддержки нет.

```bash
qmake anime_monitor.pro      # сгенерировать Makefile
make                         # сборка (на Windows: nmake / jom / mingw32-make)
lrelease anime_monitor.pro   # перекомпилировать переводы ru.ts -> ru.qm после правок строк
```

Обычно проект открывают и собирают в **Qt Creator** (`anime_monitor.pro`, состояние IDE — `anime_monitor.pro.user`). Тестов в репозитории нет.

## Архитектура

Три единицы трансляции (`anime_monitor.pro` — единственный источник списка файлов):

- **`main.cpp`** — точка входа. Гарантирует **один экземпляр** через `QSharedMemory("anime_monitor")` (если сегмент уже существует — выходит с кодом 1). Грузит перевод `ru.qm`, только если локаль страны == `QLocale::RussianFederation`. Создаёт и показывает `TrayIcon`.
- **`TrayIcon`** (`TrayIcon.{h,cpp}`) — ядро. Наследник `QSystemTrayIcon`. Держит два `QProcess`:
  - `routerProc` — встроенный веб-сервер PHP: `php -S <addr>:<port> -t web app/router.php`
  - `taskSchedulerProc` — планировщик: `php -f app/console animedb:task-scheduler`

  Меню: правый клик → About / Exit; левый клик (`activatedSlot`, `reason == 3`) → ручное меню Start / Stop / Restart. Через 1 c после старта процессы поднимаются автоматически и открывается `http://localhost:<port>/`.
- **`MSettings`** (`MSettings.{h,cpp}`) — синглтон поверх `QSettings`, файл **`config.ini`** (`IniFormat`). ⚠️ Путь относительный (`"config.ini"`) → резолвится от **рабочего каталога (CWD)**, а НЕ от папки exe (баг, [docs/BUGS.md#b3](docs/BUGS.md#b3)). Особенность: `value()` при отсутствии ключа **записывает дефолт обратно** в файл. Ключи: `addr` (`0.0.0.0` — слушает всю сеть, [docs/BUGS.md#b1](docs/BUGS.md#b1)), `port` (`56780`), `php` (`./bin\php\php.exe`), `lastAppPath`.

Ресурсы и метаданные: `icons.qrc` (`tray.png`, `favicon.ico`), `myapp.rc` + `favicon.ico` (иконка exe через `RC_FILE`), переводы `ru.ts`/`ru.qm`.

## Ловушки (gotchas)

- **Путь к приложению должен быть только ASCII.** `TrayIcon` проверяет `appDirPath` регуляркой и отказывается стартовать при пробелах/кириллице/спецсимволах — встроенный сервер PHP не переваривает такие пути. Не «баг», а защита.
- **Смена папки = сброс кэша.** Если `appDirPath != lastAppPath`, вызывается `clearCash()` (`app/console cache:clear --env=prod`), иначе Symfony падает на старых абсолютных путях в кэше.
- **Опечатки в именах — это реальные символы**, не переименовывать вслепую: `lunchProcesses` (launch), `clearCash` (cache).
- **`reason == 3`** в `activatedSlot` — магическое число `QSystemTrayIcon::Trigger` (левый клик).
- Пути PHP по умолчанию относительные (`./...`) и резолвятся от `applicationDirPath()`; команды процессов собираются строкой с Windows-редиректами.
- ⚠️ **Редирект `>nul 2>&1` не работает**: `QProcess::start(QString)` не запускает шелл, токены уходят в `php` как аргументы ([docs/BUGS.md#b2](docs/BUGS.md#b2)). Не «фича».
- ⚠️ **Дефолт `addr=0.0.0.0`** открывает dev-сервер PHP всей локальной сети — проблема безопасности ([docs/BUGS.md#b1](docs/BUGS.md#b1)).
- После правки пользовательских строк в коде обновляй `ru.ts`/`ru.qm` (`lupdate`/`lrelease`), иначе перевод разъедется с `<location line=...>`.

## Границы

- Изменения здесь — это изменения **внешнего репозитория** `anime-db/monitor`. В рамках воркспейса `anime-db-workspace` коммитить в `docs/*/` напрямую нельзя — правки уезжают через `push_docs.sh` (ветка `feature/update-cross-links`). Если работаешь прямо в клоне monitor — обычный git-флоу.
- Лицензия — **GPLv3** (`LICENSE`); шапки файлов должны содержать копирайт и ссылку на лицензию.
- Документация — на русском языке.
