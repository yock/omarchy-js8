#pragma once

#include <QString>

// Small, explicit settings file so a bad value can never take the app down.
// Stored under ~/.config/js8/settings.json and written atomically.
struct Settings {
    QString callsign;
    QString grid;
    QString inputDevice;
    double dialMhz = 7.078;
    bool monitor = true;
    QString path;

    explicit Settings(QString path = {});

    static QString defaultPath();

    bool load();
    bool save() const;
};
