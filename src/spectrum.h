#pragma once

#include <QByteArray>
#include <vector>

// One row of the waterfall: 1024 bins covering 0–3000 Hz of the 12 kHz JS8
// audio, quantised to 8 bits. The FFT is a small radix-2 implementation so the
// app does not need another FFT dependency beyond the one inside gfsk8-modem.
class Spectrum {
public:
    static constexpr int SampleRate = 12000;
    static constexpr int FftSize = 4096;
    static constexpr int Bins = 1024;
    static constexpr int MaxHz = 3000;

    Spectrum();

    QByteArray row(const qint16 *samples, int count) const;

private:
    void transform(std::vector<float> &re, std::vector<float> &im) const;

    std::vector<float> m_window;
    mutable std::vector<float> m_re;
    mutable std::vector<float> m_im;
};
