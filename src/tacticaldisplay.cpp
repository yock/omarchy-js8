#include "tacticaldisplay.h"

#include "spectrum.h"

#include <QColor>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetricsF>
#include <QMutexLocker>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QPainter>

#include "raylib.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace {

void *glLoader(const char *name)
{
    auto *context = QOpenGLContext::currentContext();
    return context ? reinterpret_cast<void *>(context->getProcAddress(name)) : nullptr;
}

QColor colorOr(const QString &value, const QColor &fallback)
{
    const QColor color(value);
    return color.isValid() ? color : fallback;
}

QColor mix(const QColor &a, const QColor &b, double t)
{
    t = std::clamp(t, 0.0, 1.0);
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t,
                            a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t);
}

QColor displayAccent(const QString &name)
{
    QColor accent = colorOr(name, QColor("#FFD60A"));
    if (accent.lightnessF() < 0.62)
        accent = accent.lighter(155);
    return accent;
}

QColor displayBackground(const QString &name)
{
    return colorOr(name, QColor("#05070b"));
}

struct Rgba {
    unsigned char r = 0;
    unsigned char g = 0;
    unsigned char b = 0;
    unsigned char a = 255;
};

Rgba rgba(const QColor &color, int alpha = 255)
{
    return {static_cast<unsigned char>(color.red()),
            static_cast<unsigned char>(color.green()),
            static_cast<unsigned char>(color.blue()),
            static_cast<unsigned char>(alpha)};
}

struct Palette {
    Rgba bg;
    Rgba primary;
    Rgba dim;
    Rgba grid;
    Rgba active;
    Rgba warning;
    Rgba alert;
};

Palette makePalette(const QString &accentName, const QString &backgroundName)
{
    const QColor accent = displayAccent(accentName);
    const QColor background = displayBackground(backgroundName);
    return {
        rgba(background),
        rgba(accent),
        rgba(mix(background, accent, 0.42)),
        rgba(mix(background, accent, 0.20)),
        rgba(QColor("#5cff9a")),
        rgba(QColor("#ffb000")),
        rgba(QColor("#ff5252")),
    };
}

} // namespace

TacticalDisplay::TacticalDisplay(QQuickItem *parent)
    : QQuickFramebufferObject(parent)
{
    setFlag(ItemHasContents, true);
    buildFontAtlas();
}

void TacticalDisplay::buildFontAtlas()
{
    // Omarchy stores its monospace choice in fontconfig; Qt's FixedFont is the
    // same resolution, so this follows `omarchy font set` at startup. TX-02
    // and similar OpenType fonts are CFF outlines, which stb_truetype cannot
    // load, so Qt rasterises the atlas and raylib draws the textured quads.
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPixelSize(64);
    m_fontFamily = QFontInfo(font).family();

    const QFontMetricsF metrics(font);
    m_fontPixelSize = 64;
    m_fontAscent = static_cast<int>(std::ceil(metrics.ascent()));
    m_fontCapHeight = static_cast<int>(std::ceil(metrics.capHeight()));
    const int descent = static_cast<int>(std::ceil(metrics.descent()));
    m_fontCellWidth = static_cast<int>(std::ceil(metrics.horizontalAdvance(QStringLiteral("M")))) + 4;
    m_fontCellHeight = m_fontAscent + descent + 4;
    m_fontFirst = 32;
    m_fontLast = 255;
    m_fontColumns = 16;

    const int glyphCount = m_fontLast - m_fontFirst + 1;
    const int rows = (glyphCount + m_fontColumns - 1) / m_fontColumns;
    QImage image(m_fontColumns * m_fontCellWidth, rows * m_fontCellHeight,
                 QImage::Format_RGBA8888);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setFont(font);
    painter.setPen(Qt::white);
    for (int i = 0; i < glyphCount; ++i) {
        const int column = i % m_fontColumns;
        const int row = i / m_fontColumns;
        const QChar character(m_fontFirst + i);
        painter.drawText(QPointF(column * m_fontCellWidth + 2,
                                 row * m_fontCellHeight + 2 + m_fontAscent),
                         QString(character));
    }
    painter.end();

    m_fontImage = image;
    ++m_fontGeneration;
    qWarning("js8: console font %s", qPrintable(m_fontFamily));
}

void TacticalDisplay::setSource(QObject *source)
{
    if (source == m_source)
        return;

    if (m_source && m_connection)
        disconnect(m_connection);
    m_source = source;
    {
        QMutexLocker locker(&m_mutex);
        m_rows.clear();
        m_latestSpectrum.clear();
        ++m_generation;
    }
    if (m_source) {
        m_connection = connect(m_source, SIGNAL(spectrumRow(QByteArray)),
                               this, SLOT(appendRow(QByteArray)));
    }
    update();
    emit sourceChanged();
}

void TacticalDisplay::setAccent(const QString &accent)
{
    if (accent == m_accent)
        return;
    m_accent = accent;
    update();
    emit accentChanged();
}

void TacticalDisplay::setBackground(const QString &background)
{
    if (background == m_background)
        return;
    m_background = background;
    update();
    emit backgroundChanged();
}

void TacticalDisplay::setHelpVisible(bool visible)
{
    if (visible == m_helpVisible)
        return;
    m_helpVisible = visible;
    update();
    emit helpVisibleChanged();
}

void TacticalDisplay::setStatus(const QString &status)
{
    if (status == m_status)
        return;
    m_status = status;
    update();
    emit statusChanged();
}

void TacticalDisplay::setError(bool error)
{
    if (error == m_error)
        return;
    m_error = error;
    update();
    emit errorChanged();
}

void TacticalDisplay::setMonitoring(bool monitoring)
{
    if (monitoring == m_monitoring)
        return;
    m_monitoring = monitoring;
    update();
    emit monitoringChanged();
}

void TacticalDisplay::setDemo(bool demo)
{
    if (demo == m_demo)
        return;
    m_demo = demo;
    update();
    emit demoChanged();
}

void TacticalDisplay::setLevel(double level)
{
    if (qFuzzyCompare(level, m_level))
        return;
    m_level = level;
    update();
    emit levelChanged();
}

void TacticalDisplay::setCallsign(const QString &callsign)
{
    if (callsign == m_callsign)
        return;
    m_callsign = callsign;
    update();
    emit callsignChanged();
}

void TacticalDisplay::setGrid(const QString &grid)
{
    if (grid == m_grid)
        return;
    m_grid = grid;
    update();
    emit gridChanged();
}

void TacticalDisplay::setDialMhz(double dialMhz)
{
    if (qFuzzyCompare(dialMhz, m_dialMhz))
        return;
    m_dialMhz = dialMhz;
    update();
    emit dialMhzChanged();
}

void TacticalDisplay::setUtcClock(const QString &utcClock)
{
    if (utcClock == m_utcClock)
        return;
    m_utcClock = utcClock;
    update();
    emit utcClockChanged();
}

void TacticalDisplay::setSecondsToPeriod(int seconds)
{
    if (seconds == m_secondsToPeriod)
        return;
    m_secondsToPeriod = seconds;
    update();
    emit secondsToPeriodChanged();
}

void TacticalDisplay::setPeriodProgress(double progress)
{
    if (qFuzzyCompare(progress, m_periodProgress))
        return;
    m_periodProgress = progress;
    update();
    emit periodProgressChanged();
}

void TacticalDisplay::setInputDeviceName(const QString &name)
{
    if (name == m_inputDeviceName)
        return;
    m_inputDeviceName = name;
    update();
    emit inputDeviceNameChanged();
}

void TacticalDisplay::setTicker(const QVariantList &ticker)
{
    if (ticker == m_ticker)
        return;
    m_ticker = ticker;
    update();
    emit tickerChanged();
}

void TacticalDisplay::appendRow(const QByteArray &row)
{
    if (row.size() != Spectrum::Bins)
        return;

    {
        QMutexLocker locker(&m_mutex);
        m_rows.append(row);
        while (m_rows.size() > TacticalDisplay::Rows)
            m_rows.removeFirst();
        m_latestSpectrum = row;
        ++m_generation;
    }
    update();
}

// ── Renderer ─────────────────────────────────────────────────────────────────

class TacticalRenderer : public QQuickFramebufferObject::Renderer {
public:
    void render() override
    {
        if (!m_ready)
            initialise();

        const int width = std::max(1, m_fboSize.width());
        const int height = std::max(1, m_fboSize.height());

        const QColor background = displayBackground(m_background);
        glViewport(0, 0, width, height);
        glClearColor(background.redF(), background.greenF(), background.blueF(), 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        rlMatrixMode(RL_PROJECTION);
        rlLoadIdentity();
        rlOrtho(0, width, 0, height, -1, 1);
        rlMatrixMode(RL_MODELVIEW);
        rlLoadIdentity();
        rlSetTexture(0); // raylib's 1x1 white texture for colour-only geometry

        if (m_paletteDirty)
            rebuildPalette();
        if (m_rowsDirty)
            rebuildTexture();
        if (m_fontDirty)
            uploadFontTexture();

        drawConsole(width, height);
        if (m_helpVisible)
            drawHelp(width, height);

        rlSetTexture(0);
        rlDrawRenderBatchActive();
    }

    void synchronize(QQuickFramebufferObject *item) override
    {
        auto *display = static_cast<TacticalDisplay *>(item);

        const QString accent = display->m_accent;
        const QString background = display->m_background;
        if (accent != m_accent || background != m_background) {
            m_accent = accent;
            m_background = background;
            m_paletteDirty = true;
        }

        m_helpVisible = display->m_helpVisible;
        m_status = display->m_status;
        m_error = display->m_error;
        m_monitoring = display->m_monitoring;
        m_demo = display->m_demo;
        m_level = display->m_level;
        m_callsign = display->m_callsign;
        m_grid = display->m_grid;
        m_dialMhz = display->m_dialMhz;
        m_utcClock = display->m_utcClock;
        m_secondsToPeriod = display->m_secondsToPeriod;
        m_periodProgress = display->m_periodProgress;
        m_inputDeviceName = display->m_inputDeviceName;
        m_ticker = display->m_ticker;

        if (display->m_fontGeneration != m_fontGeneration) {
            m_fontGeneration = display->m_fontGeneration;
            m_fontImage = display->m_fontImage;
            m_fontCellWidth = display->m_fontCellWidth;
            m_fontCellHeight = display->m_fontCellHeight;
            m_fontAscent = display->m_fontAscent;
            m_fontCapHeight = display->m_fontCapHeight;
            m_fontPixelSize = display->m_fontPixelSize;
            m_fontColumns = display->m_fontColumns;
            m_fontFirst = display->m_fontFirst;
            m_fontLast = display->m_fontLast;
            m_fontDirty = true;
        }

        {
            QMutexLocker locker(&display->m_mutex);
            if (display->m_generation != m_generation) {
                m_generation = display->m_generation;
                m_rows = display->m_rows;
                m_latestSpectrum = display->m_latestSpectrum;
                m_rowsDirty = true;
            }
        }
    }

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override
    {
        m_fboSize = size;
        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        format.setSamples(4);
        return new QOpenGLFramebufferObject(size, format);
    }

private:
    void initialise()
    {
        const int width = std::max(1, m_fboSize.width());
        const int height = std::max(1, m_fboSize.height());

        rlLoadExtensions(reinterpret_cast<void *>(glLoader));
        rlglInit(width, height);
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_MULTISAMPLE);

        m_pixels.assign(static_cast<std::size_t>(Spectrum::Bins) * TacticalDisplay::Rows * 4, 0);
        m_texture = rlLoadTexture(m_pixels.data(), Spectrum::Bins, TacticalDisplay::Rows,
                                  RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        m_ready = true;
        m_paletteDirty = true;
        m_rowsDirty = true;
    }

    void rebuildPalette()
    {
        const QColor accent = displayAccent(m_accent);
        const QColor background = displayBackground(m_background);
        const QColor hot = mix(accent, QColor("#ffffff"), 0.78);

        m_palette.resize(256 * 4);
        for (int i = 0; i < 256; ++i) {
            const double t = i / 255.0;
            const QColor color = t < 0.68
                ? mix(background, accent, std::pow(t / 0.68, 1.45))
                : mix(accent, hot, (t - 0.68) / 0.32);
            m_palette[i * 4 + 0] = static_cast<char>(color.red());
            m_palette[i * 4 + 1] = static_cast<char>(color.green());
            m_palette[i * 4 + 2] = static_cast<char>(color.blue());
            m_palette[i * 4 + 3] = static_cast<char>(255);
        }
        m_paletteDirty = false;
        m_rowsDirty = true;
    }

    void rebuildTexture()
    {
        if (m_palette.isEmpty())
            return;

        const auto *palette = reinterpret_cast<const unsigned char *>(m_palette.constData());
        const int columns = Spectrum::Bins;

        for (int y = 0; y < TacticalDisplay::Rows; ++y) {
            unsigned char *pixels = reinterpret_cast<unsigned char *>(m_pixels.data())
                + static_cast<std::size_t>(y) * columns * 4;
            const int rowIndex = m_rows.size() - 1 - y;
            if (rowIndex < 0) {
                for (int x = 0; x < columns; ++x) {
                    pixels[x * 4 + 0] = palette[0];
                    pixels[x * 4 + 1] = palette[1];
                    pixels[x * 4 + 2] = palette[2];
                    pixels[x * 4 + 3] = 255;
                }
                continue;
            }
            const unsigned char *row = reinterpret_cast<const unsigned char *>(m_rows.at(rowIndex).constData());
            for (int x = 0; x < columns; ++x) {
                const int value = row[x];
                pixels[x * 4 + 0] = palette[value * 4 + 0];
                pixels[x * 4 + 1] = palette[value * 4 + 1];
                pixels[x * 4 + 2] = palette[value * 4 + 2];
                pixels[x * 4 + 3] = 255;
            }
        }

        rlUpdateTexture(m_texture, 0, 0, columns, TacticalDisplay::Rows,
                        RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, m_pixels.data());
        m_rowsDirty = false;
    }

    void uploadFontTexture()
    {
        if (m_fontImage.isNull())
            return;
        if (m_fontTexture)
            rlUnloadTexture(m_fontTexture);
        m_fontTexture = rlLoadTexture(m_fontImage.constBits(), m_fontImage.width(),
                                      m_fontImage.height(),
                                      RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
        glBindTexture(GL_TEXTURE_2D, m_fontTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
        m_fontDirty = false;
    }

    float textWidth(const QString &text, float size) const
    {
        if (m_fontTexture == 0 || size <= 0.0f || m_fontCellWidth <= 4)
            return 0.0f;
        const float advance = (m_fontCellWidth - 4) * size / m_fontCapHeight;
        return text.size() * advance;
    }

    float drawText(const QString &text, float x, float baseline, float size, Rgba color,
                   float maxX = std::numeric_limits<float>::max())
    {
        if (m_fontTexture == 0 || size <= 0.0f || m_fontCellWidth <= 4 || m_fontImage.isNull())
            return x;

        const float scale = size / m_fontCapHeight;
        const float advance = (m_fontCellWidth - 4) * scale;
        const float cellWidth = m_fontCellWidth * scale;
        const float cellHeight = m_fontCellHeight * scale;
        const float top = baseline - (m_fontAscent + 2) * scale;
        const float imageWidth = m_fontImage.width();
        const float imageHeight = m_fontImage.height();
        const int glyphCount = m_fontLast - m_fontFirst + 1;
        float pen = x;

        rlSetTexture(m_fontTexture);
        for (QChar character : text) {
            const int glyph = character.unicode() - m_fontFirst;
            if (glyph < 0 || glyph >= glyphCount) {
                pen += advance;
                continue;
            }
            if (pen + advance > maxX)
                break;

            const int column = glyph % m_fontColumns;
            const int row = glyph / m_fontColumns;
            const float u0 = column * m_fontCellWidth / imageWidth;
            const float v0 = row * m_fontCellHeight / imageHeight;
            const float u1 = (column + 1) * m_fontCellWidth / imageWidth;
            const float v1 = (row + 1) * m_fontCellHeight / imageHeight;

            rlBegin(RL_QUADS);
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlTexCoord2f(u0, v0); rlVertex2f(pen, top);
            rlTexCoord2f(u0, v1); rlVertex2f(pen, top + cellHeight);
            rlTexCoord2f(u1, v1); rlVertex2f(pen + cellWidth, top + cellHeight);
            rlTexCoord2f(u1, v0); rlVertex2f(pen + cellWidth, top);
            rlEnd();

            pen += advance;
        }
        rlSetTexture(0);
        rlDrawRenderBatchActive();
        return pen;
    }

    void drawTextRight(const QString &text, float right, float baseline, float size, Rgba color)
    {
        drawText(text, right - textWidth(text, size), baseline, size, color);
    }

    void drawTextCentered(const QString &text, float center, float baseline, float size, Rgba color)
    {
        drawText(text, center - textWidth(text, size) / 2.0f, baseline, size, color);
    }

    void drawLine(float x1, float y1, float x2, float y2, float width, Rgba color)
    {
        const float dx = x2 - x1;
        const float dy = y2 - y1;
        const float length = std::hypot(dx, dy);
        if (length < 0.001f)
            return;
        const float nx = -dy / length * width * 0.5f;
        const float ny = dx / length * width * 0.5f;
        rlBegin(RL_QUADS);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(x1 + nx, y1 + ny);
        rlVertex2f(x1 - nx, y1 - ny);
        rlVertex2f(x2 - nx, y2 - ny);
        rlVertex2f(x2 + nx, y2 + ny);
        rlEnd();
    }

    void fillRect(float x, float y, float width, float height, Rgba color)
    {
        if (width <= 0.0f || height <= 0.0f)
            return;
        rlBegin(RL_QUADS);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex2f(x, y);
        rlVertex2f(x, y + height);
        rlVertex2f(x + width, y + height);
        rlVertex2f(x + width, y);
        rlEnd();
    }

    void strokeRect(float x, float y, float width, float height, float thickness, Rgba color)
    {
        drawLine(x, y, x + width, y, thickness, color);
        drawLine(x + width, y, x + width, y + height, thickness, color);
        drawLine(x + width, y + height, x, y + height, thickness, color);
        drawLine(x, y + height, x, y, thickness, color);
    }

    void drawConsole(int width, int height)
    {
        const Palette palette = makePalette(m_accent, m_background);
        const float u = height / 720.0f;
        const float margin = 18.0f * u;
        const float gap = 8.0f * u;
        const float titleSize = 22.0f * u;
        const float statusSize = 11.0f * u;
        const float small = 9.0f * u;
        const float tickerSize = 10.0f * u;
        const float legendSize = 8.5f * u;
        const float topHeight = titleSize + statusSize + 10.0f * u;
        const float legendHeight = legendSize + 2.0f * u;
        const float tickerLineHeight = tickerSize * 1.55f;
        const float tickerHeight = 4.0f * tickerLineHeight + 7.0f * u;
        const float right = width - margin;
        const float bottom = height - margin;
        const float tickerTop = bottom - legendHeight - gap - tickerHeight;
        const float waterfallBottom = tickerTop - gap;
        const float waterfallTop = margin + topHeight + gap;
        const float waterfallHeight = std::max(50.0f * u, waterfallBottom - waterfallTop);

        const float frameX = margin;
        const float frameY = waterfallTop;
        const float frameW = right - margin;
        const float frameH = waterfallHeight;

        // Outer console frame and corner brackets.
        strokeRect(frameX, frameY, frameW, frameH, 1.0f * u, palette.grid);
        const float bracket = 16.0f * u;
        const float bracketThickness = 1.6f * u;
        drawLine(frameX, frameY, frameX + bracket, frameY, bracketThickness, palette.primary);
        drawLine(frameX, frameY, frameX, frameY + bracket, bracketThickness, palette.primary);
        drawLine(frameX + frameW, frameY, frameX + frameW - bracket, frameY, bracketThickness, palette.primary);
        drawLine(frameX + frameW, frameY, frameX + frameW, frameY + bracket, bracketThickness, palette.primary);
        drawLine(frameX, frameY + frameH, frameX + bracket, frameY + frameH, bracketThickness, palette.primary);
        drawLine(frameX, frameY + frameH, frameX, frameY + frameH - bracket, bracketThickness, palette.primary);
        drawLine(frameX + frameW, frameY + frameH, frameX + frameW - bracket, frameY + frameH, bracketThickness, palette.primary);
        drawLine(frameX + frameW, frameY + frameH, frameX + frameW, frameY + frameH - bracket, bracketThickness, palette.primary);

        // Status strip.
        const float row1 = margin + titleSize;
        float x = margin;
        x = drawText(QStringLiteral("JS8"), x, row1, titleSize, palette.primary) + 14.0f * u;
        x = drawText(QStringLiteral("//"), x, row1, statusSize, palette.dim) + 10.0f * u;

        const QString state = m_demo ? QStringLiteral("SIM")
                            : m_monitoring ? QStringLiteral("RX")
                            : QStringLiteral("STOP");
        const Rgba stateColor = m_demo ? palette.warning
                              : m_monitoring ? palette.active
                              : palette.dim;
        x = drawText(state, x, row1, statusSize, stateColor) + 14.0f * u;

        QString dial = QString::number(m_dialMhz, 'f', 4);
        if (dial.size() < 7)
            dial.prepend(QString(7 - dial.size(), QLatin1Char('0')));
        x = drawText(dial + QStringLiteral(" MHZ"), x, row1, statusSize, palette.primary) + 14.0f * u;
        x = drawText(QStringLiteral("JS8-N"), x, row1, statusSize, palette.primary) + 14.0f * u;
        x = drawText(QStringLiteral("UTC ") + m_utcClock, x, row1, statusSize, palette.dim) + 14.0f * u;

        const float barWidth = 110.0f * u;
        const float barX = right - barWidth;
        const float barY = row1 - 5.0f * u;
        const float barHeight = 3.0f * u;
        x = drawText(QStringLiteral("NEXT ") + QString::number(m_secondsToPeriod).rightJustified(2, QLatin1Char('0'))
                     + QStringLiteral(" S"), x, row1, statusSize, palette.dim, barX - gap);
        strokeRect(barX, barY, barWidth, barHeight, 1.0f * u, palette.grid);
        fillRect(barX, barY, barWidth * static_cast<float>(std::clamp(m_periodProgress, 0.0, 1.0)),
                 barHeight, palette.primary);

        const float row2 = row1 + statusSize + 6.0f * u;
        float x2 = margin;
        x2 = drawText(QStringLiteral("DEV"), x2, row2, small, palette.dim) + 6.0f * u;
        x2 = drawText(m_inputDeviceName.isEmpty() ? QStringLiteral("NONE") : m_inputDeviceName,
                      x2, row2, small, palette.primary, right - 300.0f * u) + 16.0f * u;
        x2 = drawText(QStringLiteral("CALL"), x2, row2, small, palette.dim) + 6.0f * u;
        x2 = drawText(m_callsign.isEmpty() ? QStringLiteral("--") : m_callsign,
                      x2, row2, small, palette.primary) + 16.0f * u;
        x2 = drawText(QStringLiteral("GRID"), x2, row2, small, palette.dim) + 6.0f * u;
        x2 = drawText(m_grid.isEmpty() ? QStringLiteral("--") : m_grid,
                      x2, row2, small, palette.primary, right - 150.0f * u);

        const float levelWidth = 74.0f * u;
        const float levelX = right - levelWidth;
        const float levelY = row2 - 4.0f * u;
        drawText(QStringLiteral("LVL"), levelX - 30.0f * u, row2, small, palette.dim);
        strokeRect(levelX, levelY, levelWidth, 4.0f * u, 1.0f * u, palette.grid);
        fillRect(levelX, levelY, levelWidth * static_cast<float>(std::clamp(m_level, 0.0, 1.0)),
                 4.0f * u, m_level > 0.85 ? palette.alert : palette.active);

        // Spectrum trace and waterfall.
        const float spectrumHeight = std::clamp(frameH * 0.26f, 42.0f * u, 130.0f * u);
        const float spectrumBottom = frameY + spectrumHeight;
        const float imageTop = spectrumBottom + gap * 0.4f;
        const float imageBottom = frameY + frameH - 13.0f * u;

        drawLine(frameX, spectrumBottom, frameX + frameW, spectrumBottom, 1.0f * u, palette.grid);
        for (int i = 1; i <= 3; ++i) {
            const float y = spectrumBottom - spectrumHeight * i / 4.0f;
            drawLine(frameX, y, frameX + frameW, y, 1.0f * u, palette.grid);
        }

        if (m_latestSpectrum.size() == Spectrum::Bins) {
            float previousX = frameX;
            float previousY = spectrumBottom - 5.0f * u;
            bool havePrevious = false;
            for (int i = 0; i < Spectrum::Bins; i += 2) {
                const auto value = static_cast<unsigned char>(m_latestSpectrum.at(i));
                const float px = frameX + (static_cast<float>(i) / (Spectrum::Bins - 1)) * frameW;
                const float py = spectrumBottom - 5.0f * u
                    - (value / 255.0f) * (spectrumHeight - 10.0f * u);
                if (havePrevious)
                    drawLine(previousX, previousY, px, py, 1.1f * u, palette.primary);
                previousX = px;
                previousY = py;
                havePrevious = true;
            }
        }

        if (imageBottom > imageTop) {
            rlSetTexture(m_texture);
            rlBegin(RL_QUADS);
            rlColor4ub(255, 255, 255, 255);
            rlTexCoord2f(0, 0); rlVertex2f(frameX + 1, imageTop);
            rlTexCoord2f(0, 1); rlVertex2f(frameX + 1, imageBottom);
            rlTexCoord2f(1, 1); rlVertex2f(frameX + frameW - 1, imageBottom);
            rlTexCoord2f(1, 0); rlVertex2f(frameX + frameW - 1, imageTop);
            rlEnd();
            rlSetTexture(0);
            rlDrawRenderBatchActive();

            for (int i = 0; i <= 6; ++i) {
                const float px = frameX + frameW * i / 6.0f;
                drawLine(px, imageTop, px, imageBottom, 1.0f * u, palette.grid);
                const QString label = QString::number(i * 500);
                const float labelWidth = textWidth(label, small);
                float labelX = px - labelWidth / 2.0f;
                if (i == 0)
                    labelX = frameX + 8.0f * u;
                else if (i == 6)
                    labelX = frameX + frameW - labelWidth - 3.0f * u;
                else
                    labelX = std::clamp(labelX, frameX, frameX + frameW - labelWidth);
                drawText(label, labelX, frameY + frameH - 3.0f * u, small, palette.dim);
            }
            drawLine(frameX, imageTop, frameX + frameW, imageTop, 1.0f * u, palette.primary);
        }

        // Decode ticker.
        const float header = tickerTop + tickerSize;
        drawLine(margin, tickerTop, right, tickerTop, 1.0f * u, palette.grid);
        drawText(QStringLiteral("TIME"), margin, header, small, palette.dim);
        drawText(QStringLiteral("FREQ"), margin + 90.0f * u, header, small, palette.dim);
        drawText(QStringLiteral("SNR"), margin + 175.0f * u, header, small, palette.dim);
        drawText(QStringLiteral("MESSAGE"), margin + 260.0f * u, header, small, palette.dim);

        const float lineHeight = tickerLineHeight;
        if (m_ticker.isEmpty()) {
            drawText(QStringLiteral("NO DECODES"), margin + 260.0f * u, header + lineHeight,
                     tickerSize, palette.dim);
        } else {
            for (int i = 0; i < 3 && i < m_ticker.size(); ++i) {
                const QVariantMap line = m_ticker.at(i).toMap();
                const float y = header + (i + 1) * lineHeight;
                const int frequency = line.value(QStringLiteral("frequency")).toInt();
                const int snr = line.value(QStringLiteral("snr")).toInt();
                const bool demo = line.value(QStringLiteral("demo")).toBool();
                const QString message = line.value(QStringLiteral("message")).toString();

                drawText(line.value(QStringLiteral("time")).toString(), margin, y, tickerSize, palette.dim);
                drawText(QString::number(frequency) + QStringLiteral(" HZ"),
                         margin + 90.0f * u, y, tickerSize, palette.primary);
                const QString snrText = (snr >= 0 ? QStringLiteral("+") : QString())
                    + QString::number(snr) + QStringLiteral(" DB");
                drawText(snrText, margin + 175.0f * u, y, tickerSize,
                         snr > -12 ? palette.primary : palette.dim);
                drawText(message, margin + 260.0f * u, y, tickerSize,
                         demo ? palette.warning : palette.primary,
                         right - margin - (demo ? 30.0f * u : 0.0f));
                if (demo)
                    drawText(QStringLiteral("SIM"), right - margin - 22.0f * u, y, small, palette.warning);
            }
        }

        // Bottom status and key legend.
        const QString legend = QStringLiteral("SPACE RX   D SIM   I INPUT   R RELOAD   ? HELP   Q QUIT");
        const float legendWidth = textWidth(legend, legendSize);
        const float legendX = right - legendWidth;
        drawText(legend, legendX, bottom, legendSize, palette.dim);
        const Rgba statusColor = m_error ? palette.alert : palette.dim;
        drawText(m_status, margin, bottom, legendSize, statusColor, legendX - gap);
    }

    void drawHelp(int width, int height)
    {
        const Palette palette = makePalette(m_accent, m_background);
        const float u = height / 720.0f;
        const float titleSize = 19.0f * u;
        const float rowSize = 10.0f * u;
        const float small = 8.5f * u;
        const float pad = 22.0f * u;
        const float rowHeight = 23.0f * u;
        const float keyWidth = 78.0f * u;
        const float keyHeight = 16.0f * u;
        const char *rows[][2] = {
            {"SPACE", "START / STOP RECEIVER"},
            {"D", "TOGGLE DEMO SIGNAL"},
            {"I", "NEXT INPUT DEVICE"},
            {"R", "RELOAD INPUT DEVICES"},
            {"?", "TOGGLE THIS HELP"},
            {"Q", "QUIT"},
            {"ESC", "CLOSE HELP"},
        };
        const int rowCount = 7;

        fillRect(0, 0, width, height, {0, 0, 0, 150});

        const float panelWidth = std::min(width * 0.72f, 620.0f * u);
        const float panelHeight = pad * 2.0f + titleSize * 1.8f + rowCount * rowHeight;
        const float panelX = (width - panelWidth) / 2.0f;
        const float panelY = (height - panelHeight) / 2.0f;

        fillRect(panelX, panelY, panelWidth, panelHeight, palette.bg);
        strokeRect(panelX, panelY, panelWidth, panelHeight, 1.2f * u, palette.primary);

        const float titleY = panelY + pad + titleSize;
        drawText(QStringLiteral("KEYBOARD"), panelX + pad, titleY, titleSize, palette.primary);
        drawLine(panelX + pad, titleY + 8.0f * u, panelX + panelWidth - pad, titleY + 8.0f * u,
                 1.0f * u, palette.grid);

        for (int i = 0; i < rowCount; ++i) {
            const float y = titleY + 8.0f * u + (i + 1) * rowHeight;
            const float boxX = panelX + pad;
            const float boxY = y - keyHeight + 4.0f * u;
            strokeRect(boxX, boxY, keyWidth, keyHeight, 1.0f * u, palette.dim);
            drawTextCentered(QString::fromLatin1(rows[i][0]), boxX + keyWidth / 2.0f,
                             y, small, palette.primary);
            drawText(QString::fromLatin1(rows[i][1]), boxX + keyWidth + 14.0f * u,
                     y, rowSize, palette.dim);
        }
    }

    bool m_ready = false;
    QSize m_fboSize;
    unsigned int m_texture = 0;
    std::vector<unsigned char> m_pixels;
    QByteArray m_palette;
    bool m_paletteDirty = true;
    bool m_rowsDirty = true;
    quint64 m_generation = 0;

    unsigned int m_fontTexture = 0;
    QImage m_fontImage;
    int m_fontCellWidth = 0;
    int m_fontCellHeight = 0;
    int m_fontAscent = 0;
    int m_fontCapHeight = 0;
    int m_fontPixelSize = 64;
    int m_fontColumns = 16;
    int m_fontFirst = 32;
    int m_fontLast = 255;
    bool m_fontDirty = true;
    quint64 m_fontGeneration = 0;

    QString m_accent;
    QString m_background;
    bool m_helpVisible = false;
    QString m_status;
    bool m_error = false;
    bool m_monitoring = false;
    bool m_demo = false;
    double m_level = 0.0;
    QString m_callsign;
    QString m_grid;
    double m_dialMhz = 7.078;
    QString m_utcClock;
    int m_secondsToPeriod = 0;
    double m_periodProgress = 0.0;
    QString m_inputDeviceName;
    QVariantList m_ticker;
    QVector<QByteArray> m_rows;
    QByteArray m_latestSpectrum;
};

QQuickFramebufferObject::Renderer *TacticalDisplay::createRenderer() const
{
    return new TacticalRenderer;
}
