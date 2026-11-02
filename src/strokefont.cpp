#include "strokefont.h"

#include <algorithm>
#include <limits>

QRectF StrokeGlyph::bounds() const
{
    qreal left = std::numeric_limits<qreal>::max();
    qreal top = std::numeric_limits<qreal>::max();
    qreal right = std::numeric_limits<qreal>::lowest();
    qreal bottom = std::numeric_limits<qreal>::lowest();

    for (const StrokePath &path : paths) {
        for (const QPointF &point : path.points) {
            left = std::min(left, point.x());
            top = std::min(top, point.y());
            right = std::max(right, point.x());
            bottom = std::max(bottom, point.y());
        }
    }

    if (left > right)
        return {};
    return QRectF(QPointF(left, top), QPointF(right, bottom));
}

// The .jhf format: columns 0-4 are the glyph number, 5-7 the number of
// coordinate pairs (including the left/right pair), 8 the left position, 9 the
// right position, then single-character coordinate pairs. All coordinates are
// offset by ASCII 'R', and " R" is a pen-up. The first line is space (ASCII 32).
bool StrokeFont::load(const QByteArray &data)
{
    m_glyphs.clear();
    m_baseline = 0.0;
    m_capHeight = 0.0;
    m_descent = 0.0;

    const QList<QByteArray> lines = data.split('\n');
    int code = 32;

    for (QByteArray line : lines) {
        if (line.endsWith('\r'))
            line.chop(1);
        if (line.isEmpty())
            continue;
        if (code > 126)
            break;
        if (line.size() < 10)
            return false;

        bool ok = false;
        const int pairs = line.mid(5, 3).trimmed().toInt(&ok);
        if (!ok || pairs < 1)
            return false;

        const int left = line.at(8) - 'R';
        const int right = line.at(9) - 'R';
        if (right < left || line.size() < 10 + (pairs - 1) * 2)
            return false;

        StrokeGlyph glyph;
        glyph.advance = right - left;

        StrokePath path;
        for (int i = 0; i < pairs - 1; ++i) {
            const char x = line.at(10 + i * 2);
            const char y = line.at(10 + i * 2 + 1);
            if (x == ' ' && y == 'R') {
                if (!path.points.isEmpty()) {
                    glyph.paths.append(path);
                    path.points.clear();
                }
                continue;
            }
            // Match libhersheyfont's transform so the baseline and descenders
            // line up the same way as the reference renderer.
            path.points.append(QPointF((x - 'R') - left, 9.0 - (y - 'R')));
        }
        if (!path.points.isEmpty())
            glyph.paths.append(path);

        m_glyphs.insert(QChar(code), glyph);
        ++code;
    }

    if (m_glyphs.size() < 95)
        return false;

    // The .jhf y transform puts the bottom of capital letters at y=0 and
    // increases upward. Descenders go below zero.
    m_baseline = 0.0;
    qreal capTop = 0.0;
    for (char c = 'A'; c <= 'Z'; ++c) {
        const auto it = m_glyphs.constFind(QChar(c));
        if (it == m_glyphs.constEnd())
            continue;
        const QRectF box = it->bounds();
        if (!box.isNull())
            capTop = std::max(capTop, box.bottom());
    }

    if (capTop <= 0.0)
        return false;

    m_capHeight = capTop;

    qreal descender = 0.0;
    for (auto it = m_glyphs.constBegin(); it != m_glyphs.constEnd(); ++it) {
        const QRectF box = it->bounds();
        if (!box.isNull())
            descender = std::max(descender, -box.top());
    }
    m_descent = descender;

    return true;
}

const StrokeGlyph *StrokeFont::glyph(QChar character) const
{
    const auto it = m_glyphs.constFind(character);
    return it == m_glyphs.constEnd() ? nullptr : &it.value();
}

qreal StrokeFont::measure(const QString &text, qreal letterSpacing) const
{
    qreal width = 0.0;
    for (int i = 0; i < text.size(); ++i) {
        const StrokeGlyph *item = glyph(text.at(i));
        width += item ? item->advance : 0.0;
        if (i + 1 < text.size())
            width += letterSpacing;
    }
    return width;
}
