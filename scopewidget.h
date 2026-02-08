#pragma once

#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>
#include <QWidget>

#include <windows.h>
#include <dsound.h>

class ScopeWidget : public QWidget
{
    Q_OBJECT

public:
    enum ChannelMode {
        ChannelLeft = 0,
        ChannelRight = 1,
        ChannelStereo = 2
    };

    explicit ScopeWidget(QWidget *parent = nullptr);
    ~ScopeWidget() override;

    QStringList deviceNames() const;
    void setDeviceIndex(int index);
    void setChannelMode(ChannelMode mode);
    void setTimeScaleMs(int ms);

    bool startCapture();
    void stopCapture();
    bool isCapturing() const;

signals:
    void statusChanged(const QString &text);
    void frameReady(const QVector<float> &samples, int sampleRate);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void pollCapture();

private:
    struct DeviceInfo {
        QString name;
        GUID guid{};
        bool hasGuid = false;
    };

    static BOOL CALLBACK enumDevicesCallback(LPGUID guid, LPCSTR description, LPCSTR module, LPVOID context);
    void refreshDevices();
    bool initCapture();
    void releaseCapture();
    bool tryFormat(int sampleRate, int channels, int bitsPerSample);
    void appendSamples(const QVector<float> &samples);

    QVector<DeviceInfo> m_devices;
    int m_deviceIndex = 0;
    ChannelMode m_channelMode = ChannelStereo;

    IDirectSoundCapture8 *m_capture = nullptr;
    IDirectSoundCaptureBuffer8 *m_buffer = nullptr;
    WAVEFORMATEX m_format{};
    DWORD m_bufferBytes = 0;
    DWORD m_readPos = 0;

    QTimer m_timer;
    QVector<float> m_wave;
    int m_maxSamples = 2048;
    float m_displayPeak = 0.05f;
    float m_envState = 0.0f;
    float m_envDecay = 1.0f;
    int m_envSampleRate = 0;
    int m_timeScaleMs = 0;
};
