#include "settings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include <utility>

Settings::Settings(QString path)
    : path(path.isEmpty() ? defaultPath() : std::move(path))
{
}

QString Settings::defaultPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + "/js8/settings.json";
}

bool Settings::load()
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
    callsign = object.value("callsign").toString().trimmed();
    grid = object.value("grid").toString().trimmed();
    inputDevice = object.value("inputDevice").toString();
    dialMhz = object.value("dialMhz").toDouble(dialMhz);
    monitor = object.value("monitor").toBool(monitor);

    if (dialMhz < 0.1 || dialMhz > 6000.0)
        dialMhz = 7.078;
    return true;
}

bool Settings::save() const
{
    const QString directory = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(directory))
        return false;

    QJsonObject object;
    object.insert("callsign", callsign);
    object.insert("grid", grid);
    object.insert("inputDevice", inputDevice);
    object.insert("dialMhz", dialMhz);
    object.insert("monitor", monitor);

    // Write beside the target and rename, so a crash never leaves half a file.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    return file.commit();
}
