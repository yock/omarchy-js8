#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <QRectF>

#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

#include "DecodedText.h"
#include "gfsk8modem.h"
#include "settings.h"
#include "spectrum.h"
#include "strokefont.h"
#include "theme.h"
#include "translator.h"

class Js8Tests : public QObject {
    Q_OBJECT

private slots:
    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
        QCOMPARE(theme.radius(), 0);
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\n");

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n");
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        write(response, "{\"int\":0}");

        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        write(response, "{\"int\":6}");
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        write(dir.path() + "/pt.json", R"({"Save": "Guardar", "Open": "Abrir"})");
        write(dir.path() + "/pt-BR.json", R"({"Save": "Salvar"})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("pt-BR"));
        QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void translatorPicksPluralForms()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "ru\n");
        write(dir.path() + "/en.json", R"({"%n messages": {"one": "%n message", "other": "%n messages"}})");
        write(dir.path() + "/ru.json", R"({"%n messages": {"one": "%n сообщение", "few": "%n сообщения", "many": "%n сообщений", "other": "%n сообщения"}})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "%n messages", nullptr, 21), QString("%n сообщение"));
        QCOMPARE(translator.translate("Main", "%n messages", nullptr, 22), QString("%n сообщения"));
        QCOMPARE(translator.translate("Main", "%n messages", nullptr, 25), QString("%n сообщений"));
        QCOMPARE(Translator::pluralCategory("ar", 11), QString("many"));
        QCOMPARE(Translator::pluralCategory("ja", 1), QString("other"));
    }

    void strokeFontLoads()
    {
        QFile file(QStringLiteral(JS8_SOURCE_DIR "/src/fonts/hershey/rowmans.jhf"));
        QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.fileName()));

        StrokeFont font;
        QVERIFY(font.load(file.readAll()));
        QVERIFY(font.glyphCount() >= 95);

        const StrokeGlyph *h = font.glyph('H');
        QVERIFY(h);
        QCOMPARE(static_cast<int>(h->paths.size()), 3);
        QVERIFY(h->advance > 0);
        QVERIFY(font.measure(QStringLiteral("HELLO")) > 0);

        const StrokeGlyph *space = font.glyph(' ');
        QVERIFY(space);
        QVERIFY(space->paths.isEmpty());

        const QRectF bounds = h->bounds();
        QVERIFY(qAbs(bounds.top() - font.baseline()) < 0.001);
        QVERIFY(qAbs(bounds.bottom() - font.capHeight()) < 0.001);
        QVERIFY(font.capHeight() > 0);
        QVERIFY(font.descent() > 0);
        QVERIFY(font.lineHeight() > font.capHeight());
    }

    void settingsRoundTrip()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("js8/settings.json");

        Settings saved(path);
        saved.callsign = "N0CALL";
        saved.grid = "FN31PR";
        saved.inputDevice = "USB Audio";
        saved.dialMhz = 14.078;
        saved.monitor = false;
        QVERIFY(saved.save());

        Settings loaded(path);
        QVERIFY(loaded.load());
        QCOMPARE(loaded.callsign, QString("N0CALL"));
        QCOMPARE(loaded.grid, QString("FN31PR"));
        QCOMPARE(loaded.inputDevice, QString("USB Audio"));
        QCOMPARE(loaded.dialMhz, 14.078);
        QCOMPARE(loaded.monitor, false);
    }

    void settingsIgnoresBadDial()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("settings.json");
        write(path, R"({"dialMhz": 0})");

        Settings loaded(path);
        QVERIFY(loaded.load());
        QCOMPARE(loaded.dialMhz, 7.078);
    }

    void spectrumFindsTone()
    {
        constexpr int size = Spectrum::FftSize;
        constexpr double frequency = 1500.0;
        std::vector<qint16> samples(size);
        for (int i = 0; i < size; ++i) {
            const double phase = 2.0 * 3.141592653589793 * frequency * i / Spectrum::SampleRate;
            samples[i] = static_cast<qint16>(std::sin(phase) * 16000.0);
        }

        Spectrum spectrum;
        const QByteArray row = spectrum.row(samples.data(), size);
        QCOMPARE(row.size(), Spectrum::Bins);

        int peak = 0;
        for (int i = 1; i < row.size(); ++i)
            if (static_cast<unsigned char>(row[i]) > static_cast<unsigned char>(row[peak]))
                peak = i;

        const double expected = frequency / (Spectrum::SampleRate / static_cast<double>(Spectrum::FftSize));
        QVERIFY2(std::abs(peak - expected) <= 2.0,
                 qPrintable(QString("peak %1, expected %2").arg(peak).arg(expected)));
    }

    void spectrumIsQuietAtSilence()
    {
        std::vector<qint16> samples(Spectrum::FftSize, 0);
        Spectrum spectrum;
        const QByteArray row = spectrum.row(samples.data(), samples.size());
        for (char value : row)
            QCOMPARE(static_cast<unsigned char>(value), 0);
    }

    // This is the integration that matters: the app's message path must round
    // trip through gfsk8-modem's pack, modulate, and decode.
    void modemDecodesPackedMessage()
    {
        const auto frames = gfsk8::pack("N0CALL", "FN31PR", "HELLO FROM JS8",
                                        gfsk8::Submode::Normal);
        QVERIFY(!frames.empty());

        const auto pcm = gfsk8::modulate(gfsk8::Submode::Normal, frames.front().frameType,
                                         frames.front().payload, 1500.0);
        QVERIFY(!pcm.empty());

        constexpr int snapshotSize = 60 * 12000;
        constexpr int periodSize = 15 * 12000;
        std::vector<qint16> snapshot(snapshotSize, 0);
        const int position = snapshotSize - periodSize;
        const int count = std::min<int>(pcm.size(), periodSize);
        for (int i = 0; i < count; ++i)
            snapshot[position + i] = static_cast<qint16>(
                std::clamp(pcm[i], -1.0f, 1.0f) * 16000.0f);

        gfsk8::Decoder decoder(1 << 0);
        QString message;
        decoder.decode(std::span<const qint16>(snapshot.data(), snapshot.size()), 0,
                       [&](const gfsk8::Decoded &decoded) {
            DecodedText text(decoded.message, decoded.frameType, decoded.submode);
            message = QString::fromStdString(text.message()).trimmed();
        });

        QCOMPARE(message, QString("HELLO FROM JS8"));
    }

private:
    static void write(const QString &path, const QByteArray &contents)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(Js8Tests)
#include "js8_tests.moc"
