#include "spectrumwidget.h"

#include <QPainter>

#include <algorithm>
#include <cmath>
#include <complex>

SpectrumWidget::SpectrumWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(180);
    setAutoFillBackground(false);
}

void SpectrumWidget::setSamples(const QVector<float> &samples)
{
    m_samples = samples;
    computeSpectrum();
    update();
}

void SpectrumWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.fillRect(rect(), QColor(10, 10, 14));

    const int w = width();
    const int h = height();

    painter.setPen(QPen(QColor(40, 40, 60)));
    painter.drawLine(0, h - 1, w, h - 1);

    if (m_bins.isEmpty()) {
        painter.setPen(QColor(120, 120, 140));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("No spectrum"));
        return;
    }

    const float maxValue = *std::max_element(m_bins.begin(), m_bins.end());
    const float scale = (maxValue > 0.0f) ? (static_cast<float>(h - 4) / maxValue) : 1.0f;

    const int count = m_bins.size();
    const float barWidth = static_cast<float>(w) / static_cast<float>(count);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 140, 220));

    for (int i = 0; i < count; ++i) {
        const float magnitude = m_bins[i];
        const float barHeight = magnitude * scale;
        const float x = i * barWidth;
        const float y = h - barHeight;
        painter.drawRect(QRectF(x, y, barWidth * 0.9f, barHeight));
    }
}

void SpectrumWidget::computeSpectrum()
{
    m_bins.clear();
    if (m_samples.size() < 8) {
        return;
    }

    const int n = nextPow2(m_samples.size());
    if (n < 8) {
        return;
    }

    QVector<std::complex<float>> data;
    data.resize(n);

    for (int i = 0; i < n; ++i) {
        const float sample = (i < m_samples.size()) ? m_samples[i] : 0.0f;
        const float pi = std::acos(-1.0f);
        const float window = 0.5f * (1.0f - std::cos(2.0f * pi * i / (n - 1)));
        data[i] = std::complex<float>(sample * window, 0.0f);
    }

    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(data[i], data[j]);
        }
    }

    for (int len = 2; len <= n; len <<= 1) {
        const float pi = std::acos(-1.0f);
        const float angle = -2.0f * pi / static_cast<float>(len);
        std::complex<float> wlen(std::cos(angle), std::sin(angle));
        for (int i = 0; i < n; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            const int half = len / 2;
            for (int j = 0; j < half; ++j) {
                const std::complex<float> u = data[i + j];
                const std::complex<float> v = data[i + j + half] * w;
                data[i + j] = u + v;
                data[i + j + half] = u - v;
                w *= wlen;
            }
        }
    }

    const int bins = n / 2;
    m_bins.resize(bins);
    for (int i = 0; i < bins; ++i) {
        const float mag = std::abs(data[i]) / static_cast<float>(n);
        m_bins[i] = mag;
    }
}

int SpectrumWidget::nextPow2(int value) const
{
    int n = 1;
    while (n < value) {
        n <<= 1;
    }
    return n;
}
