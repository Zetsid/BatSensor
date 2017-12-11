/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QAudioDeviceInfo>
#include <QAudioOutput>
#include <QDebug>
#include <QVBoxLayout>
#include <Qtmath>
#include <math.h>
#include <qendian.h>
#include <QByteArray>
#include <QDateTime>
#include <QTimer>
#include <QElapsedTimer>
#include <QObject>
#include <QVector>
#include <QAudioFormat>


#include "audiooutput.h"

using namespace std;

#define PUSH_MODE_LABEL "Enable push mode"
#define PULL_MODE_LABEL "Enable pull mode"
#define SUSPEND_LABEL   "Suspend playback"
#define RESUME_LABEL    "Resume playback"
#define VOLUME_LABEL    "Volume:"

const int ToneSampleRateHz = 1000;
const int DataSampleRateHz = 44100;
const int BufferSize      = 32768;
const int DurationSeconds = 1.0;
const int CarrierSampleRateHz = 1000;
const qreal CarrierAmplitudeN = 0.001;
const int PayloadCarrierSampleRateHz=DataSampleRateHz/8;
const int audioSampleSizeInBits = 16;
const int audioTimerIntervalMs = 10; // notify time won't work if value is too small
const int audioNotifyIntevalTargetMs = audioTimerIntervalMs*2; // tries to use this

static quint64 periodsForBit =100;


Generator::Generator(const QAudioFormat &format,
                     qint64 durationUs,
                     int sampleRate,
                     QObject *parent)
    :   QIODevice(parent)
    ,   m_pos(0)
{
    if (format.isValid())
        generateData(format, durationUs, sampleRate);
}

Generator::~Generator()
{

}

void Generator::start()
{
    open(QIODevice::ReadOnly);
}

void Generator::stop()
{
    m_pos = 0;
    close();
}

void Generator::sendAudio(QVector<qint16> audioData)
{
    emit sendAudio(audioData);
}

quint64 Generator::getSampleTimeUs() {
    return (1.0/static_cast<qreal>(mCurrentAudioFormat.sampleRate()))*1000000.0;
}

qreal Generator::getBitTimeMs()
{
    qreal bitTimeMs;
    bitTimeMs=static_cast<qreal>(periodsForBit)*1.0/PayloadCarrierSampleRateHz*1000.0;
    return bitTimeMs;
}

void Generator::sendMessage(quint8 messageCharacter)
{
    QByteArray message;
    message.append(messageCharacter);

    QString("Hello World") = message;
/* Now only short messages can be set because bit clock is synced

     * in start of firt bit. Clock drifts too much if message is longer than one byte */
    QVector<bool> bitStream;
    qDebug()<< "Tx: " << message.toHex();
    for ( int byteIndex = 0; byteIndex<message.size();byteIndex++) {
        quint8 byte = message.at(byteIndex);
        /* Add start bit. This is needed for start of frame detection and
         * end of transmission (silence after message ) */
        bitStream.append(true);
        for ( uint bitIndex = 0; bitIndex<sizeof(quint8)*8;bitIndex++) {
            quint8 bitValue = byte >> bitIndex & 0x01;
            bool bit;
            if (bitValue>0) {
                bit=true;
            }
            else {
                bit=false;
            }
            bitStream.append(bit);
        }
        /* Add stop bit */
        bitStream.append(true);
    }

    QVector<qint16> audioMessage;
    audioMessage = generateSignalling(bitStream);
    sendAudio(audioMessage);
}

QVector<qint16> Generator::generateSignalling(QVector<bool> bitStream)

{
    QVector<qint16> ret;
    qreal bitTimeMs;
    bitTimeMs = getBitTimeMs();
    QVector<qint16>  bitPulse = generateData2(mCurrentAudioFormat,
                                             bitStream.size()*bitTimeMs*1000.0,
                                             PayloadCarrierSampleRateHz);
    int samplesPerBit = bitPulse.size()/bitStream.size();
    for (int z = 0; z<bitStream.size();z++) {
        qreal amplitudeN = 1.0;
        bool bit = bitStream.at(z);
        if ( bit==true) {
            amplitudeN=1.0;
        }
        else {
            amplitudeN=0.1;
        }
        int offset = z * samplesPerBit;
        for (int y=0;y<samplesPerBit;y++) {
            bitPulse[offset+y]*=amplitudeN;
        }
    }
    ret=bitPulse;
    return ret;
}

QVector<qint16> Generator::generateData2(const QAudioFormat &format, qint64 durationUs, qreal inputSignalFrequency)
{
    QVector<qint16> ret;

    const int channelBytes = format.sampleSize() / 8;

    qint64 length = format.sampleRate() * durationUs / 1000000;
    ret.resize(length);
    unsigned char *ptr = reinterpret_cast<unsigned char *>(ret.data());
    for ( int sampleIndex =0; sampleIndex < length;sampleIndex++) {

        const qreal x = qSin(2 * M_PI * inputSignalFrequency  * qreal(sampleIndex % format.sampleRate()) / format.sampleRate());

        qint16 value = static_cast<qint16>(x * 32767);

        if (format.byteOrder() == QAudioFormat::LittleEndian) {
            qToLittleEndian<qint16>(value, ptr);
        }
        else {
            qToBigEndian<qint16>(value, ptr);
        }
        ptr+=channelBytes;
    }

    return ret;
}

void Generator::generateData(const QAudioFormat &format, qint64 durationUs, int sampleRate)
{
    const int channelBytes = format.sampleSize() / 8;
    const int sampleBytes = format.channelCount() * channelBytes;

    qint64 length = (format.sampleRate() * format.channelCount() * (format.sampleSize() / 8))
                        * durationUs / 100000;

    Q_ASSERT(length % sampleBytes == 0);
    Q_UNUSED(sampleBytes) // suppress warning in release builds

    m_buffer.resize(length);
    unsigned char *ptr = reinterpret_cast<unsigned char *>(m_buffer.data());
    int sampleIndex = 0;

    while (length) {
        const qreal x = qSin(2 * M_PI * sampleRate * qreal(sampleIndex % format.sampleRate()) / format.sampleRate());
        for (int i=0; i<format.channelCount(); ++i) {
            if (format.sampleSize() == 8 && format.sampleType() == QAudioFormat::UnSignedInt) {
                const quint8 value = static_cast<quint8>((1.0 + x) / 2 * 255);
                *reinterpret_cast<quint8*>(ptr) = value;
            } else if (format.sampleSize() == 8 && format.sampleType() == QAudioFormat::SignedInt) {
                const qint8 value = static_cast<qint8>(x * 127);
                *reinterpret_cast<quint8*>(ptr) = value;
            } else if (format.sampleSize() == 16 && format.sampleType() == QAudioFormat::UnSignedInt) {
                quint16 value = static_cast<quint16>((1.0 + x) / 2 * 65535);
                if (format.byteOrder() == QAudioFormat::LittleEndian)
                    qToLittleEndian<quint16>(value, ptr);
                else
                    qToBigEndian<quint16>(value, ptr);
            } else if (format.sampleSize() == 16 && format.sampleType() == QAudioFormat::SignedInt) {
                qint16 value = static_cast<qint16>(x * 32767);
                if (format.byteOrder() == QAudioFormat::LittleEndian)
                    qToLittleEndian<qint16>(value, ptr);
                else
                    qToBigEndian<qint16>(value, ptr);
            }

            ptr += channelBytes;
            length -= channelBytes;
        }
        ++sampleIndex;
    }
}

qint64 Generator::readData(char *data, qint64 len)
{

    qint64 total = 0;

    if (!m_buffer.isEmpty()) {

            while (len - total > 0) {
            const qint64 chunk = qMin((m_buffer.size() - m_pos), len - total);

            qDebug() << " memcpy from buffer at" << m_pos << len;

//            QDateTime currentTime = QDateTime::currentDateTime();

//            qint64 t = currentTime.currentSecsSinceEpoch();
//            qreal modulationIndex =0;

//            if(t % 2 == 0){
//                modulationIndex = 1.0;
//            }else {
//                modulationIndex = 0.2;
//            }

//            qDebug() << "modulation index" << modulationIndex;

//            for(int z=0;z<modified.size();z++){
//                modified[z] = modified.at(z) * modulationIndex;
//            }

            //memcpy(data + total,modified.constData()+m_pos,chunk);

            memcpy(data + total, m_buffer.constData() + m_pos, chunk);
            m_pos = (m_pos + chunk) % m_buffer.size();
            total += chunk;
        }
    }
    return total;
}

qint64 Generator::writeData(const char *data, qint64 len)
{
    Q_UNUSED(data);
    Q_UNUSED(len);

    return 0;
}

qint64 Generator::bytesAvailable() const
{
    return m_buffer.size() + QIODevice::bytesAvailable();
}

AudioTest::AudioTest()
    :   m_pushTimer(new QTimer(this))
    ,   m_modeButton(0)
    ,   m_suspendResumeButton(0)
    ,   m_deviceBox(0)
    ,   m_device(QAudioDeviceInfo::defaultOutputDevice())
    ,   m_generator(0)
    ,   m_audioOutput(0)
    ,   m_output(0)
    ,   m_pullMode(true)
    ,   m_buffer(BufferSize, 0)
{
    initializeWindow();
    initializeAudio();
}

void AudioTest::initializeWindow()
{
    QScopedPointer<QWidget> window(new QWidget);
    QScopedPointer<QVBoxLayout> layout(new QVBoxLayout);

    m_deviceBox = new QComboBox(this);
    const QAudioDeviceInfo &defaultDeviceInfo = QAudioDeviceInfo::defaultOutputDevice();
    m_deviceBox->addItem(defaultDeviceInfo.deviceName(), qVariantFromValue(defaultDeviceInfo));
    foreach (const QAudioDeviceInfo &deviceInfo, QAudioDeviceInfo::availableDevices(QAudio::AudioOutput)) {
        if (deviceInfo != defaultDeviceInfo)
            m_deviceBox->addItem(deviceInfo.deviceName(), qVariantFromValue(deviceInfo));
    }
    connect(m_deviceBox,SIGNAL(activated(int)),SLOT(deviceChanged(int)));
    layout->addWidget(m_deviceBox);

    m_modeButton = new QPushButton(this);
    m_modeButton->setText(tr(PUSH_MODE_LABEL));
    connect(m_modeButton, SIGNAL(clicked()), SLOT(toggleMode()));
    layout->addWidget(m_modeButton);

    m_suspendResumeButton = new QPushButton(this);
    m_suspendResumeButton->setText(tr(SUSPEND_LABEL));
    connect(m_suspendResumeButton, SIGNAL(clicked()), SLOT(toggleSuspendResume()));
    layout->addWidget(m_suspendResumeButton);

    QHBoxLayout *volumeBox = new QHBoxLayout;
    m_volumeLabel = new QLabel;
    m_volumeLabel->setText(tr(VOLUME_LABEL));
    m_volumeSlider = new QSlider(Qt::Horizontal);
    m_volumeSlider->setMinimum(0);
    m_volumeSlider->setMaximum(100);
    m_volumeSlider->setSingleStep(10);
    connect(m_volumeSlider, SIGNAL(valueChanged(int)), this, SLOT(volumeChanged(int)));
    volumeBox->addWidget(m_volumeLabel);
    volumeBox->addWidget(m_volumeSlider);
    layout->addLayout(volumeBox);

    window->setLayout(layout.data());
    layout.take(); // ownership transferred

    setCentralWidget(window.data());
    QWidget *const windowPtr = window.take(); // ownership transferred
    windowPtr->show();
}

void AudioTest::initializeAudio()
{
    connect(m_pushTimer, SIGNAL(timeout()), SLOT(pushTimerExpired()));

    m_format.setSampleRate(DataSampleRateHz);
    m_format.setChannelCount(1);
    m_format.setSampleSize(16);
    m_format.setCodec("audio/pcm");
    m_format.setByteOrder(QAudioFormat::LittleEndian);
    m_format.setSampleType(QAudioFormat::SignedInt);

    QAudioDeviceInfo info(m_device);
    if (!info.isFormatSupported(m_format)) {
        qWarning() << "Default format not supported - trying to use nearest";
        m_format = info.nearestFormat(m_format);
    }

    if (m_generator)
        delete m_generator;
    m_generator = new Generator(m_format, DurationSeconds*1000000, ToneSampleRateHz, this);

    createAudioOutput();
}

void AudioTest::createAudioOutput()
{
    qDebug() << __FUNCTION__;
    delete m_audioOutput;
    m_audioOutput = 0;
    m_audioOutput = new QAudioOutput(m_device, m_format, this);
    m_generator->start();
    m_audioOutput->start(m_generator);

    qreal initialVolume = QAudio::convertVolume(m_audioOutput->volume(),
                                                QAudio::LinearVolumeScale,
                                                QAudio::LogarithmicVolumeScale);
    m_volumeSlider->setValue(qRound(initialVolume * 20));
}

AudioTest::~AudioTest()
{

}

void AudioTest::deviceChanged(int index)
{
    m_pushTimer->stop();
    m_generator->stop();
    m_audioOutput->stop();
    m_audioOutput->disconnect(this);
    m_device = m_deviceBox->itemData(index).value<QAudioDeviceInfo>();
    initializeAudio();
}

void AudioTest::volumeChanged(int value)
{
    if (m_audioOutput) {
        qreal linearVolume =  QAudio::convertVolume(value / qreal(100),
                                                    QAudio::LogarithmicVolumeScale,
                                                    QAudio::LinearVolumeScale);

        m_audioOutput->setVolume(linearVolume);
    }
}

void AudioTest::pushTimerExpired()
{
    if (m_audioOutput && m_audioOutput->state() != QAudio::StoppedState) {
        int chunks = m_audioOutput->bytesFree()/m_audioOutput->periodSize();
        while (chunks) {
           const qint64 len = m_generator->read(m_buffer.data(), m_audioOutput->periodSize());
           if (len)
               m_output->write(m_buffer.data(), len);
           if (len != m_audioOutput->periodSize())
               break;
           --chunks;
        }
    }
}

void AudioTest::toggleMode()
{
    m_pushTimer->stop();
    m_audioOutput->stop();

    if (m_pullMode) {
        //switch to push mode (periodically push to QAudioOutput using a timer)
        m_modeButton->setText(tr(PULL_MODE_LABEL));
        m_output = m_audioOutput->start();
        m_pullMode = false;
        m_pushTimer->start(20);
    } else {
        //switch to pull mode (QAudioOutput pulls from Generator as needed)
        m_modeButton->setText(tr(PUSH_MODE_LABEL));
        m_pullMode = true;
        m_audioOutput->start(m_generator);
    }

    m_suspendResumeButton->setText(tr(SUSPEND_LABEL));
}

void AudioTest::toggleSuspendResume()
{
    if (m_audioOutput->state() == QAudio::SuspendedState) {
        m_audioOutput->resume();
        m_suspendResumeButton->setText(tr(SUSPEND_LABEL));
    } else if (m_audioOutput->state() == QAudio::ActiveState) {
        m_audioOutput->suspend();
        m_suspendResumeButton->setText(tr(RESUME_LABEL));
    } else if (m_audioOutput->state() == QAudio::StoppedState) {
        m_audioOutput->resume();
        m_suspendResumeButton->setText(tr(SUSPEND_LABEL));
    } else if (m_audioOutput->state() == QAudio::IdleState) {
        // no-op
    }
}

