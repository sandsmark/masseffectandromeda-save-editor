#include "SaveFile.h"
#include <QDebug>
#include <QBuffer>

SaveFile::SaveFile(QObject *parent) : QObject(parent)
{

}

static inline quint32 calculateCrc32(const QByteArray &data, const quint32 seed)
{
    quint32 crc32 = ~seed;
    for (const char &c : data) {
        quint32 val = (crc32 ^ c) & 0xff;
        for (int i=0; i<8; i++) {
            val = (val & 1) ? (val >>1 ) ^ 0xedb88320 : val >> 1;
        }
        crc32 = val ^ (crc32 >> 8);
    }

    return ~crc32;
}

bool SaveFile::load(QIODevice *input)
{
    Q_ASSERT(input->isReadable());
    m_input = input;

    m_ok = false;

    static constexpr quint64 fileHeader(0x534B4E5548434246); // FBCHUNKS

    const QByteArray magic = input->read(sizeof(quint64));
    if (magic.size() != sizeof(quint64)) {
        qWarning() << "failed to read header";
        return false;
    }

    if (qFromLittleEndian<qint64>(magic) == fileHeader) {
        qDebug() << "little endian";
        m_endian = QSysInfo::LittleEndian;
    } else if (qFromBigEndian<qint64>(magic) == fileHeader) {
        qDebug() << "big endian";
        m_endian = QSysInfo::BigEndian;
    } else {
        qWarning() << "Unknown endianness" << magic;
        return false;
    }

    m_ok = true;

    const quint16 version = read<quint16>();
    qDebug() << "Version" << version;

    const quint32 headerLength = read<quint32>();
    qDebug() << "header length" << headerLength;

    const quint32 dataLength = read<quint32>();
    qDebug() << "dataLength" << dataLength;

    { // read header
        quint32 headerChecksum = read<quint32>();
        QByteArray header = read(headerLength - sizeof(headerChecksum));
        const quint32 calculatedHeaderChecksum = calculateCrc32(header, 0x12345678);
        if (headerChecksum != calculatedHeaderChecksum) {
            qWarning() << "Invalid header checksum" << headerChecksum << "expected" << calculatedHeaderChecksum;
            m_ok = false;
            return false;
        }
        qDebug() << "header checksum correct";

        QBuffer headerBuffer(&header);
        headerBuffer.open(QIODevice::ReadOnly);
        if (!m_header.load(&headerBuffer, m_endian)) {
            qWarning() << "Failed to load header";
            m_ok = false;
            return false;
        }
    }

    { // read data
        quint32 dataChecksum = read<quint32>();
        qDebug() << "data start" << m_input->pos();
        QByteArray data = read(dataLength - sizeof(dataChecksum));
        const quint32 calculatedDataChecksum = calculateCrc32(data, 0x12345678);
        if (dataChecksum != calculatedDataChecksum) {
            qWarning() << "Invalid data checksum" << dataChecksum << "expected" << calculatedDataChecksum;
            m_ok = false;
            return false;
        }
        qDebug() << "data checksum correct";

//        QBuffer dataBuffer(&data);
//        dataBuffer.open(QIODevice::ReadOnly);
        bits::bitstream bitstream(reinterpret_cast<quint8*>(data.data()));
        if (!m_data.load(&bitstream, m_endian)) {
            qWarning() << "Failed to load data";
            m_ok = false;
            return false;
        }
    }


    return m_ok;
}

bool SaveHeader::load(QIODevice *input, const QSysInfo::Endian endian)
{
    m_ok = true;

    m_endian = endian;
    m_input = input;

    if (!readMagic("FBHEADER")) {
        return false;
    }

    const quint16 version = read<quint16>();
    qDebug() << "Header version" << version;

    const quint32 entryCount = read<quint32>();
    if (entryCount != NumEntries) {
        qWarning() << "Invalid number of entries" << entryCount << "expected" << NumEntries;
        m_ok = false;
        return false;
    }
    qDebug() << "entry count" << entryCount;
    m_values.resize(entryCount);
    for (Value &entry : m_values) {
        entry.hash = read<quint32>();
//        const quint16 entryLength = read<quint16>();
//        entry.value = QString::fromUtf8(read(entryLength));
        entry.value = readString();
        qDebug() << entry.value;
    }

    return m_ok;
}

bool BaseSave::load()
{
    Q_ASSERT(m_input);

    m_ok = true;

//    if (!readMagic("FB\0SAVE\n")) {
//    if (!readMagic(qToBigEndian<quint64>(0x0A45564153004246ul))) {
    if (!readMagic(qToLittleEndian<quint64>(0x0A45564153004246ul))) {
        return false;
    }

    m_hasUnknown = m_input->read<quint8>(4);
    if (m_hasUnknown) {
        qDebug() << "Skipped";
        m_input->skip(27);
    }


    return m_ok;
}

bool SaveData::load(bits::bitstream *input, const QSysInfo::Endian endian)
{
    m_endian = endian;
    m_input = input;

    if (!BaseSave::load()) {
        return false;
    }
//    m_input->skip(30);
    qDebug() << "Has unknown?" << m_hasUnknown;

    // TODO: gibbed's code reads 64 bits here, but there's just 32 until the string starts
    const quint64 rawTimestamp = read<quint32>();
    qDebug() << "raw timestamp" << rawTimestamp;
    m_timestamp = QDateTime::fromSecsSinceEpoch(rawTimestamp);
    qDebug() << "timestamp" << m_timestamp;
    qDebug() << qFromBigEndian<quint32>(rawTimestamp)<< qFromLittleEndian<quint64>(rawTimestamp);


    m_saveFileName = readString();
    qDebug() << m_saveFileName << m_saveFileName.length();

    m_gameVersion = read<quint16>();
    if (m_gameVersion != 3) {
        qWarning() << "unsupported game version" << m_gameVersion;
        m_ok = false;
//        return false;
    }
    m_saveVersion = read<quint16>();
    if (m_saveVersion < 20 || m_saveVersion > 22) {
        qWarning() << "unsupported save version" << m_saveVersion;
        m_ok = false;
        return false;
    }

    m_unknown1 = read<quint16>();
    m_unknown2 = read<quint16>();
    m_userBuildInfo = read<quint32>();

    qDebug() << "game version" << m_gameVersion;
    qDebug() << "save version" << m_saveVersion;
    qDebug() << "unknown1" << m_unknown1;
    qDebug() << "unknown2" << m_unknown2;
    qDebug() << "user build info" << m_unknown2;

    m_levelName = readString();
    m_unknown3 = read<quint32>();
    qDebug() << "level name" << m_levelName << "probably related unknown:" << m_unknown3;
    if (1){
        const size_t size = 200;
        std::string data(size, 0);
        m_input->peekstring(reinterpret_cast<quint8*>(data.data()), size * 8);
        qDebug() << QByteArray::fromStdString(data);
        qDebug() << QByteArray::fromStdString(data).toHex(':');
//        return false;
    }

    m_preloadedBundles = readStringList();

    return m_ok;
}
