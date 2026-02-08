#include "scopewidget.h"

#include <QPainter>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace {
QVector<float> toSamples(const void *data, DWORD bytes, const WAVEFORMATEX &format, ScopeWidget::ChannelMode mode)
{
    QVector<float> samples;
    if (format.wBitsPerSample != 16 || format.nChannels < 1) {
        return samples;
    }

    const int16_t *pcm = reinterpret_cast<const int16_t *>(data);
    const int frames = static_cast<int>(bytes / format.nBlockAlign);
    samples.reserve(frames);

    for (int i = 0; i < frames; ++i) {
        int16_t left = pcm[i * format.nChannels];
        int16_t right = left;
        if (format.nChannels > 1) {
            right = pcm[i * format.nChannels + 1];
        }

        float value = 0.0f;
        switch (mode) {
        case ScopeWidget::ChannelLeft:
            value = static_cast<float>(left) / 32768.0f;
            break;
        case ScopeWidget::ChannelRight:
            value = static_cast<float>(right) / 32768.0f;
            break;
        case ScopeWidget::ChannelStereo:
        default:
            value = (static_cast<float>(left) + static_cast<float>(right)) / (2.0f * 32768.0f);
            break;
        }

        samples.push_back(value);
    }

    return samples;
}
} // namespace

BOOL CALLBACK ScopeWidget::enumDevicesCallback(LPGUID guid, LPCSTR description, LPCSTR, LPVOID context)
{
    auto *devices = reinterpret_cast<QVector<DeviceInfo> *>(context);
    DeviceInfo info;
    if (description && description[0] != '\0') {
        info.name = QString::fromLocal8Bit(description);
    } else {
        info.name = QStringLiteral("DirectSound Capture Device");
    }
    if (guid) {
        info.guid = *guid;
        info.hasGuid = true;
    }
    devices->push_back(info);
    return TRUE;
}

ScopeWidget::ScopeWidget(QWidget *parent)
    : QWidget(parent)
{
    refreshDevices();
    setMinimumHeight(240);
    setAutoFillBackground(false);

    m_timer.setInterval(30);
    connect(&m_timer, &QTimer::timeout, this, &ScopeWidget::pollCapture);
}

ScopeWidget::~ScopeWidget()
{
    stopCapture();
    releaseCapture();
}

QStringList ScopeWidget::deviceNames() const
{
    QStringList names;
    for (const auto &device : m_devices) {
        names.push_back(device.name);
    }
    return names;
}

void ScopeWidget::setDeviceIndex(int index)
{
    if (index < 0 || index >= m_devices.size()) {
        return;
    }
    if (m_deviceIndex == index) {
        return;
    }

    m_deviceIndex = index;
    if (isCapturing()) {
        startCapture();
    }
}

void ScopeWidget::setChannelMode(ChannelMode mode)
{
    m_channelMode = mode;
    update();
}

void ScopeWidget::setTimeScaleMs(int ms)
{
    m_timeScaleMs = std::max(0, ms);
    update();
}

bool ScopeWidget::startCapture()
{
    stopCapture();
    if (!initCapture()) {
        emit statusChanged(QStringLiteral("Capture init failed"));
        return false;
    }

    HRESULT hr = m_buffer->Start(DSCBSTART_LOOPING);
    if (FAILED(hr)) {
        emit statusChanged(QStringLiteral("Capture start failed"));
        releaseCapture();
        return false;
    }

    m_timer.start();
    emit statusChanged(QStringLiteral("Capturing"));
    return true;
}

void ScopeWidget::stopCapture()
{
    if (m_timer.isActive()) {
        m_timer.stop();
    }
    if (m_buffer) {
        m_buffer->Stop();
    }
    m_wave.clear();
    update();
}

bool ScopeWidget::isCapturing() const
{
    return m_timer.isActive();
}

void ScopeWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.fillRect(rect(), QColor(8, 8, 12));

    const int w = width();
    const int h = height();
    const int midY = h / 2;

    painter.setPen(QPen(QColor(40, 40, 60)));
    painter.drawLine(0, midY, w, midY);

    if (m_wave.size() < 2) {
        painter.setPen(QColor(120, 120, 140));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("No signal"));
        return;
    }

    const float peak = 1.0f;
    const float yScale = (static_cast<float>(h) * 0.45f) / peak;

    QVector<QPointF> points;
    points.reserve(m_wave.size());

    for (int i = 0; i < m_wave.size(); ++i) {
        const float x = static_cast<float>(i) / static_cast<float>(m_wave.size() - 1) * static_cast<float>(w - 1);
        const float y = static_cast<float>(midY) - m_wave[i] * yScale;
        points.push_back(QPointF(x, y));
    }

    painter.setPen(QPen(QColor(0, 200, 120), 1.2));
    painter.drawPolyline(points.constData(), points.size());

    if (m_format.nSamplesPerSec > 0) {
        const float durationSec = (m_timeScaleMs > 0)
            ? static_cast<float>(m_timeScaleMs) / 1000.0f
            : static_cast<float>(m_wave.size()) / static_cast<float>(m_format.nSamplesPerSec);
        const int ticks = 5;
        painter.setPen(QPen(QColor(150, 150, 170), 1.0));
        painter.setFont(QFont(painter.font().family(), 8));
        for (int i = 0; i < ticks; ++i) {
            const float t = (ticks > 1) ? (static_cast<float>(i) / static_cast<float>(ticks - 1)) : 0.0f;
            const float x = t * static_cast<float>(w - 1);
            const float ms = durationSec * 1000.0f * t;
            painter.drawLine(QPointF(x, h - 2), QPointF(x, h - 8));
            painter.drawText(QPointF(x + 2.0f, h - 10.0f), QString::number(static_cast<int>(ms)) + QStringLiteral(" ms"));
        }
    }
}

void ScopeWidget::pollCapture()
{
    if (!m_buffer) {
        return;
    }

    DWORD capturePos = 0;
    DWORD readPos = 0;
    HRESULT hr = m_buffer->GetCurrentPosition(&capturePos, &readPos);
    if (FAILED(hr)) {
        emit statusChanged(QStringLiteral("Capture read failed"));
        return;
    }

    DWORD writePos = readPos;
    if (writePos == m_readPos && capturePos != m_readPos) {
        writePos = capturePos;
    }

    DWORD available = 0;
    if (writePos >= m_readPos) {
        available = writePos - m_readPos;
    } else {
        available = (m_bufferBytes - m_readPos) + writePos;
    }

    const DWORD blockAlign = m_format.nBlockAlign;
    if (available < blockAlign) {
        return;
    }

    const DWORD maxBytes = static_cast<DWORD>(m_maxSamples) * blockAlign;
    DWORD toRead = std::min(available, maxBytes);
    toRead = (toRead / blockAlign) * blockAlign;

    void *ptr1 = nullptr;
    void *ptr2 = nullptr;
    DWORD bytes1 = 0;
    DWORD bytes2 = 0;

    hr = m_buffer->Lock(m_readPos, toRead, &ptr1, &bytes1, &ptr2, &bytes2, 0);
    if (FAILED(hr)) {
        emit statusChanged(QStringLiteral("Capture lock failed"));
        return;
    }

    QVector<float> samples;
    if (bytes1 > 0) {
        samples = toSamples(ptr1, bytes1, m_format, m_channelMode);
    }
    if (bytes2 > 0) {
        QVector<float> tail = toSamples(ptr2, bytes2, m_format, m_channelMode);
        samples += tail;
    }

    m_buffer->Unlock(ptr1, bytes1, ptr2, bytes2);

    if (!samples.isEmpty()) {
        appendSamples(samples);
        update();
        emit frameReady(samples, static_cast<int>(m_format.nSamplesPerSec));
    }

    m_readPos = (m_readPos + toRead) % m_bufferBytes;
}

void ScopeWidget::refreshDevices()
{
    m_devices.clear();
    DirectSoundCaptureEnumerateA(enumDevicesCallback, &m_devices);

    if (m_devices.isEmpty()) {
        DeviceInfo fallback;
        fallback.name = QStringLiteral("Default device");
        fallback.hasGuid = false;
        m_devices.push_back(fallback);
    }
    m_deviceIndex = 0;
}

bool ScopeWidget::initCapture()
{
    releaseCapture();
    if (m_devices.isEmpty()) {
        refreshDevices();
    }

    const DeviceInfo &device = m_devices.value(m_deviceIndex);
    HRESULT hr = DirectSoundCaptureCreate8(device.hasGuid ? &device.guid : nullptr, &m_capture, nullptr);
    if (FAILED(hr)) {
        return false;
    }

    const int rates[] = {48000, 44100, 32000, 22050};
    for (int rate : rates) {
        if (tryFormat(rate, 2, 16)) {
            emit statusChanged(QStringLiteral("Format %1 Hz, 16-bit, stereo").arg(rate));
            return true;
        }
        if (tryFormat(rate, 1, 16)) {
            emit statusChanged(QStringLiteral("Format %1 Hz, 16-bit, mono").arg(rate));
            return true;
        }
    }

    releaseCapture();
    return false;
}

void ScopeWidget::releaseCapture()
{
    if (m_buffer) {
        m_buffer->Release();
        m_buffer = nullptr;
    }
    if (m_capture) {
        m_capture->Release();
        m_capture = nullptr;
    }
    m_bufferBytes = 0;
    m_readPos = 0;
}

bool ScopeWidget::tryFormat(int sampleRate, int channels, int bitsPerSample)
{
    if (!m_capture) {
        return false;
    }

    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = static_cast<WORD>(channels);
    format.nSamplesPerSec = static_cast<DWORD>(sampleRate);
    format.wBitsPerSample = static_cast<WORD>(bitsPerSample);
    format.nBlockAlign = static_cast<WORD>((format.nChannels * format.wBitsPerSample) / 8);
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

    DSCBUFFERDESC desc{};
    desc.dwSize = sizeof(desc);
    desc.dwBufferBytes = format.nAvgBytesPerSec * 2;
    desc.dwBufferBytes = (desc.dwBufferBytes / format.nBlockAlign) * format.nBlockAlign;
    desc.lpwfxFormat = &format;

    IDirectSoundCaptureBuffer *buffer = nullptr;
    HRESULT hr = m_capture->CreateCaptureBuffer(&desc, &buffer, nullptr);
    if (FAILED(hr)) {
        return false;
    }

    IDirectSoundCaptureBuffer8 *buffer8 = nullptr;
    hr = buffer->QueryInterface(IID_IDirectSoundCaptureBuffer8, reinterpret_cast<void **>(&buffer8));
    buffer->Release();
    if (FAILED(hr)) {
        return false;
    }

    m_buffer = buffer8;
    m_format = format;
    m_bufferBytes = desc.dwBufferBytes;
    m_readPos = 0;
    return true;
}

void ScopeWidget::appendSamples(const QVector<float> &samples)
{
    QVector<float> absSamples;
    absSamples.reserve(samples.size());

    float maxAbs = 0.0f;
    for (float sample : samples) {
        const float value = std::fabs(sample);
        absSamples.push_back(value);
        maxAbs = std::max(maxAbs, value);
    }
    m_displayPeak = std::max(maxAbs, m_displayPeak * 0.95f);

    if (absSamples.size() >= m_maxSamples) {
        m_wave = absSamples.mid(absSamples.size() - m_maxSamples);
        return;
    }

    const int overflow = (m_wave.size() + absSamples.size()) - m_maxSamples;
    if (overflow > 0) {
        m_wave.remove(0, overflow);
    }
    m_wave += absSamples;
}
