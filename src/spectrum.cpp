#include "spectrum.h"

#include <algorithm>
#include <cmath>
#include <numbers>

Spectrum::Spectrum()
    : m_window(FftSize)
    , m_re(FftSize)
    , m_im(FftSize)
{
    // Hann window: keeps a strong carrier from smearing across neighbouring
    // bins without hiding the weak signals the waterfall is meant to show.
    for (int i = 0; i < FftSize; ++i)
        m_window[i] = 0.5f * (1.0f - std::cos(2.0f * std::numbers::pi_v<float> * i / (FftSize - 1)));
}

void Spectrum::transform(std::vector<float> &re, std::vector<float> &im) const
{
    const int n = static_cast<int>(re.size());

    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(re[i], re[j]), std::swap(im[i], im[j]);
    }

    for (int len = 2; len <= n; len <<= 1) {
        const float angle = -2.0f * std::numbers::pi_v<float> / static_cast<float>(len);
        const float wRe = std::cos(angle), wIm = std::sin(angle);
        for (int i = 0; i < n; i += len) {
            float curRe = 1.0f, curIm = 0.0f;
            for (int k = 0; k < len / 2; ++k) {
                const int a = i + k, b = i + k + len / 2;
                const float xRe = re[b] * curRe - im[b] * curIm;
                const float xIm = re[b] * curIm + im[b] * curRe;
                re[b] = re[a] - xRe;
                im[b] = im[a] - xIm;
                re[a] += xRe;
                im[a] += xIm;
                const float nextRe = curRe * wRe - curIm * wIm;
                curIm = curRe * wIm + curIm * wRe;
                curRe = nextRe;
            }
        }
    }
}

QByteArray Spectrum::row(const qint16 *samples, int count) const
{
    QByteArray out(Bins, '\0');
    if (!samples || count < FftSize)
        return out;

    for (int i = 0; i < FftSize; ++i) {
        const float windowed = samples[i] * (1.0f / 32768.0f) * m_window[i];
        m_re[i] = windowed;
        m_im[i] = 0.0f;
    }
    transform(m_re, m_im);

    constexpr float lowDb = -95.0f;
    constexpr float highDb = -20.0f;
    constexpr float scale = 255.0f / (highDb - lowDb);

    for (int k = 0; k < Bins; ++k) {
        if (k < 3) {
            // The soundcard's DC offset would otherwise paint a permanent
            // bright stripe on the left edge.
            out[k] = 0;
            continue;
        }
        const float magnitude = std::hypot(m_re[k], m_im[k]) * (2.0f / FftSize);
        const float db = 20.0f * std::log10(magnitude + 1e-7f);
        const float value = std::clamp((db - lowDb) * scale, 0.0f, 255.0f);
        out[k] = static_cast<char>(value);
    }
    return out;
}
