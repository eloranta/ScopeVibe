#pragma once

#include <QVector>
#include <QWidget>

class SpectrumWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SpectrumWidget(QWidget *parent = nullptr);

public slots:
    void setSamples(const QVector<float> &samples, int sampleRate);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void computeSpectrum();
    int nextPow2(int value) const;

    QVector<float> m_samples;
    QVector<float> m_bins;
    int m_sampleRate = 0;
};
