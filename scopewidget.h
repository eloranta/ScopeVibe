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
    QStringList outputDeviceNames() const;
    void setDeviceIndex(int index);
    void setChannelMode(ChannelMode mode);
    void setTimeScaleMs(int ms);
    void setGain(float gain);
    void setOutputDeviceIndex(int index);

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
    void refreshOutputDevices();
    bool initCapture();
    bool initPlayback();
    void releaseCapture();
    void releasePlayback();
    bool tryFormat(int sampleRate, int channels, int bitsPerSample);
    void appendSamples(const QVector<float> &samples);
    void outputSamples(const QVector<float> &samples);

    QVector<DeviceInfo> m_devices;
    int m_deviceIndex = 0;
    QVector<DeviceInfo> m_outputDevices;
    int m_outputDeviceIndex = 0;
    ChannelMode m_channelMode = ChannelStereo;

    IDirectSoundCapture8 *m_capture = nullptr;
    IDirectSoundCaptureBuffer8 *m_buffer = nullptr;
    IDirectSound8 *m_play = nullptr;
    IDirectSoundBuffer *m_playBuffer = nullptr;
    WAVEFORMATEX m_format{};
    DWORD m_bufferBytes = 0;
    DWORD m_readPos = 0;
    DWORD m_playBufferBytes = 0;
    DWORD m_playWritePos = 0;

    QTimer m_timer;
    QVector<float> m_wave;
    int m_maxSamples = 2048;
    float m_displayPeak = 0.05f;
    float m_gain = 10.0f;
    int m_timeScaleMs = 0;
};
