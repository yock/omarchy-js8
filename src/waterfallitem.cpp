#include "waterfallitem.h"

#include "spectrum.h"

#include <QColor>
#include <QMetaObject>
#include <QMutexLocker>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QQuickWindow>

#include "raylib.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>

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

} // namespace

WaterfallItem::WaterfallItem(QQuickItem *parent)
    : QQuickFramebufferObject(parent)
{
    setFlag(ItemHasContents, true);
}

void WaterfallItem::setSource(QObject *source)
{
    if (source == m_source)
        return;

    if (m_source && m_connection)
        disconnect(m_connection);
    m_source = source;
    {
        QMutexLocker locker(&m_mutex);
        m_rows.clear();
        ++m_generation;
    }
    if (m_source) {
        m_connection = connect(m_source, SIGNAL(spectrumRow(QByteArray)),
                               this, SLOT(appendRow(QByteArray)));
    }
    update();
    emit sourceChanged();
}

void WaterfallItem::setAccent(const QString &accent)
{
    if (accent == m_accent)
        return;
    m_accent = accent;
    {
        QMutexLocker locker(&m_mutex);
        ++m_generation;
    }
    update();
    emit accentChanged();
}

void WaterfallItem::setBackground(const QString &background)
{
    if (background == m_background)
        return;
    m_background = background;
    {
        QMutexLocker locker(&m_mutex);
        ++m_generation;
    }
    update();
    emit backgroundChanged();
}

void WaterfallItem::appendRow(const QByteArray &row)
{
    if (row.size() != Spectrum::Bins)
        return;

    {
        QMutexLocker locker(&m_mutex);
        m_rows.append(row);
        while (m_rows.size() > WaterfallItem::Rows)
            m_rows.removeFirst();
        ++m_generation;
    }
    update();
}

class WaterfallRenderer : public QQuickFramebufferObject::Renderer {
public:
    void render() override
    {
        const int width = std::max(1, m_fboSize.width());
        const int height = std::max(1, m_fboSize.height());

        if (!m_ready)
            initialise(width, height);

        const QColor background = colorOr(m_background, QColor("#0b0b0e"));
        glViewport(0, 0, width, height);
        glClearColor(background.redF(), background.greenF(), background.blueF(), 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (m_paletteDirty || m_rowsDirty) {
            if (m_paletteDirty)
                rebuildPalette(background);
            rebuildTexture();
        }

        rlMatrixMode(RL_PROJECTION);
        rlLoadIdentity();
        rlOrtho(0, width, height, 0, -1, 1);
        rlMatrixMode(RL_MODELVIEW);
        rlLoadIdentity();

        rlSetTexture(m_texture);
        rlBegin(RL_QUADS);
        rlColor4ub(255, 255, 255, 255);
        rlTexCoord2f(0, 1); rlVertex2f(0, 0);
        rlTexCoord2f(0, 0); rlVertex2f(0, height);
        rlTexCoord2f(1, 0); rlVertex2f(width, height);
        rlTexCoord2f(1, 1); rlVertex2f(width, 0);
        rlEnd();
        rlSetTexture(0);
        rlDrawRenderBatchActive();
    }

    void synchronize(QQuickFramebufferObject *item) override
    {
        auto *waterfall = static_cast<WaterfallItem *>(item);
        QMutexLocker locker(&waterfall->m_mutex);

        if (m_generation == waterfall->m_generation)
            return;

        m_generation = waterfall->m_generation;
        m_accent = waterfall->m_accent;
        m_background = waterfall->m_background;
        m_rows = waterfall->m_rows;
        m_rowsDirty = true;
        m_paletteDirty = true;
    }

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override
    {
        m_fboSize = size;
        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        return new QOpenGLFramebufferObject(size, format);
    }

private:
    void initialise(int width, int height)
    {
        rlLoadExtensions(reinterpret_cast<void *>(glLoader));
        rlglInit(width, height);
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);

        m_pixels.assign(static_cast<std::size_t>(Spectrum::Bins) * WaterfallItem::Rows * 4, 0);
        m_texture = rlLoadTexture(m_pixels.data(), Spectrum::Bins, WaterfallItem::Rows,
                                  RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);

        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        m_ready = true;
        m_paletteDirty = true;
        m_rowsDirty = true;
    }

    void rebuildPalette(const QColor &background)
    {
        const QColor accent = colorOr(m_accent, QColor("#FFD60A"));
        const QColor hot("#ffffff");
        m_palette.resize(256 * 4);
        for (int i = 0; i < 256; ++i) {
            const double t = i / 255.0;
            // Most of the range is the theme accent; only the strongest signals
            // burn toward white, so the waterfall reads at a glance.
            const QColor color = t < 0.68
                ? mix(background, accent, std::pow(t / 0.68, 1.45))
                : mix(accent, hot, (t - 0.68) / 0.32);
            m_palette[i * 4 + 0] = static_cast<char>(color.red());
            m_palette[i * 4 + 1] = static_cast<char>(color.green());
            m_palette[i * 4 + 2] = static_cast<char>(color.blue());
            m_palette[i * 4 + 3] = static_cast<char>(255);
        }
        m_paletteDirty = false;
    }

    void rebuildTexture()
    {
        const auto *palette = reinterpret_cast<const unsigned char *>(m_palette.constData());
        const int backgroundIndex = 0;
        const int columns = Spectrum::Bins;

        for (int y = 0; y < WaterfallItem::Rows; ++y) {
            unsigned char *pixels = reinterpret_cast<unsigned char *>(m_pixels.data())
                + static_cast<std::size_t>(y) * columns * 4;
            const int rowIndex = m_rows.size() - 1 - y;
            if (rowIndex < 0) {
                for (int x = 0; x < columns; ++x) {
                    pixels[x * 4 + 0] = palette[backgroundIndex * 4 + 0];
                    pixels[x * 4 + 1] = palette[backgroundIndex * 4 + 1];
                    pixels[x * 4 + 2] = palette[backgroundIndex * 4 + 2];
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

        rlUpdateTexture(m_texture, 0, 0, columns, WaterfallItem::Rows,
                        RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, m_pixels.data());
        m_rowsDirty = false;
    }

    bool m_ready = false;
    bool m_paletteDirty = true;
    bool m_rowsDirty = true;
    quint64 m_generation = 0;
    QSize m_fboSize;
    QString m_accent;
    QString m_background;
    QVector<QByteArray> m_rows;

    unsigned int m_texture = 0;
    std::vector<unsigned char> m_pixels;
    QByteArray m_palette;
};

QQuickFramebufferObject::Renderer *WaterfallItem::createRenderer() const
{
    return new WaterfallRenderer;
}
