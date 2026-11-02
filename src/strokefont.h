#pragma once

#include <QByteArray>
#include <QHash>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

// A single open polyline in a stroke glyph.
struct StrokePath {
    QVector<QPointF> points;
};

// One glyph from a Hershey .jhf font. Coordinates are in font units with the
// library's y-down transform; advance is the glyph's horizontal step.
struct StrokeGlyph {
    qreal advance = 0.0;
    QVector<StrokePath> paths;

    QRectF bounds() const;
};

// Parser for the Hershey .jhf vector font format. The glyphs are single-stroke
// polylines, so this is deliberately not a raster font: the renderer draws the
// paths directly with lines.
class StrokeFont {
public:
    bool load(const QByteArray &data);

    const StrokeGlyph *glyph(QChar character) const;
    int glyphCount() const { return static_cast<int>(m_glyphs.size()); }

    qreal measure(const QString &text, qreal letterSpacing = 2.0) const;
    qreal baseline() const { return m_baseline; }
    qreal capHeight() const { return m_capHeight; }
    qreal descent() const { return m_descent; }
    qreal lineHeight() const { return m_capHeight + m_descent; }

private:
    QHash<QChar, StrokeGlyph> m_glyphs;
    qreal m_baseline = 0.0;
    qreal m_capHeight = 0.0;
    qreal m_descent = 0.0;
};
