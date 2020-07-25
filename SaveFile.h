#ifndef SAVEFILE_H
#define SAVEFILE_H

#include <QObject>
#include <QtEndian>
#include <QIODevice>
#include <QHash>
#include <QMetaEnum>
#include <QDebug>
#include <QDateTime>

#include "bits/bits-stream.h"

class QIODevice;

template <typename ENUM> static QString enumToString(const ENUM val) {
    static_assert(std::is_enum<ENUM>());
    return QString::fromUtf8(QMetaEnum::fromType<ENUM>().valueToKey(val));
}

template <typename ENUM> static QString enumFromString(const QString &string, bool *ok) {
    static_assert(std::is_enum<ENUM>());
    ENUM ret = ENUM(QMetaEnum::fromType<ENUM>().keyToValue(string.toUtf8().constData(), ok));
    return ret;

}

struct Serializable
{
protected:
    QByteArray read(const qint64 size) {
        QByteArray data = m_input->read(size);
        if (data.size() != size) {
            m_ok = false;
            return {};
        }

        if (m_endian == QSysInfo::BigEndian) {
            qFromBigEndian<char>(data.constData(), data.size(), data.data());
        } else {
            qFromLittleEndian<char>(data.constData(), data.size(), data.data());
        }
        for (int i=0; i<data.count(); i++) {
            data[i] = data[i];
        }
        return data;
    }

    template<typename T>
    T read() {
        QByteArray data = m_input->read(sizeof(T));
        if (data.size() != sizeof(T)) {
            m_ok = false;
            return {};
        }

        if (m_endian == QSysInfo::BigEndian) {
            return qFromBigEndian<T>(data);
        } else {
            return qFromLittleEndian<T>(data);
        }
    }

    QString readString() {
        const quint16 length = read<quint16>();
        if (length > 1000) { //arbitrary
            qWarning() << "Unrealistically long string" << length;
            m_ok = false;
            return {};
        }
        return QString::fromUtf8(read(length));
    }

    bool readMagic(const char *raw) {
        const QByteArray expected(raw, sizeof(quint64));
        const QByteArray magic = read(expected.size());
        if (magic.length() != sizeof(quint64)) {
            qWarning() << "short read of magic" << magic.size();
            m_ok = false;
        }
        if (!m_ok) {
            return false;
        }
        if (magic != expected) {
            qWarning() << "Invalid magic" << magic << "expected" << expected;
            m_ok = false;
            return false;
        }
        qDebug() << "magic vlaid" << magic << expected;
        return true;
    }

    QSysInfo::Endian m_endian;
    bool m_ok = true;

    QIODevice *m_input = nullptr;
};

struct SaveHeader : public Serializable
{
    Q_GADGET

public:
    bool load(QIODevice *input, const QSysInfo::Endian endian);

    enum EntryId {
        AreaNameStringId = 0,
        AreaThumbnailTextureId,
        GameVersion,
        RequiredDLC,
        RequiredInstallGroup,
        ProfileName,
        ProfileUniqueName,
        ProfileId,
        LevelID,
        PlayerLevel,
        GameCompleted,
        TrialMode,
        CompletionPercentage,
        DateTime,
        LevelTitleID,
        LevelFloorID,
        LevelRegionID,
        TotalPlaytime,
        NameOverrideStringId,

        NumEntries
    };
    Q_ENUM(EntryId) // can convert to and from string

    struct Value {
        quint32 hash;
        QString value;
    };

    QVector<Value> m_values; // list to preserve ordering
};

struct BaseSave
{
    bool load();

    QByteArray read(const quint64 size) {
        std::string data(size, 0);
        m_input->readstring(reinterpret_cast<quint8*>(data.data()), size * 8);
        if (data.size() != size) {
            m_ok = false;
            return {};
        }

//        if (m_endian == QSysInfo::BigEndian) {
//            qFromBigEndian<char>(data.data(), data.size(), data.data());
//        } else {
//            qFromLittleEndian<char>(data.data(), data.size(), data.data());
//        }
        return QByteArray::fromStdString(data);
    }

    template<typename T,
             std::enable_if_t<std::is_same<T, bool>::value, int> = 0
             >
    T read() {
        return m_input->read<quint8>(1);
    }

    template<typename T,
             std::enable_if_t<std::negation<std::is_same<T, bool>>::value, int> = 0
             >
    T read() {
        T data = m_input->read<T>(sizeof(T) * 8);
//        return data;
        if (m_endian == QSysInfo::LittleEndian) { // bitstream swaps under us?
            return qFromBigEndian<T>(data);
        } else {
            return qFromLittleEndian<T>(data);
        }
    }

    QString readString() {
        const quint16 length = read<quint16>();
        qDebug() << "String length" << length;
        if (length == 0) {
            return {};
        }
        if (length > 1000) { //arbitrary
            qWarning() << "Unrealistically long string" << length;
            m_ok = false;
            return {};
        }
        QByteArray content =read(length);
        for (char &c : content) {
            c &= 0x7f; // wtffff
        }
        qDebug() << content;
        qDebug() << content.toHex(':');
        return QString::fromUtf8(content, length);
//        return QString::fromUtf8(read(length), length);
    }

    QStringList readStringList() {
        const quint16 length = read<quint16>();
        qDebug() << "String list length:" << length;

        QStringList ret;
        for (int i=0; i<length; i++) {
            ret.append(readString());
            if (!m_ok) {
                qDebug() << ret;
                return {};
            }
        }
        return ret;
    }
    QHash<QString, QString> readDictionary() {
        const quint16 length = read<quint16>();

        QHash<QString, QString> ret;
        for (int i=0; i<length; i++) {
            QString key = readString();
            if (!m_ok) {
                return {};
            }
            QString value = readString();
            if (!m_ok) {
                return {};
            }
            qDebug() << key << value;
            ret.insert(key, value);
        }
        return ret;
    }

//    bool readMagic(const char *raw) {
    bool readMagic(const quint64 expected) {
//        const QByteArray expected(raw, sizeof(quint64));
//        const QByteArray magic = read(expected.size());
//        if (magic.length() != sizeof(quint64)) {
//            qWarning() << "short read of magic" << magic.size();
//            m_ok = false;
//        }
//        if (!m_ok) {
//            return false;
//        }
        const quint64 magic = read<quint64>();
//        const quint64 expected = 0x0A45564153004246ul;
        if (magic != expected) {
            qWarning() << "Invalid magic" << magic << "expected" << expected;
            m_ok = false;
            return false;
        }
//        qDebug() << "magic vlaid" << magic << expected;
        return true;
    }

    QSysInfo::Endian m_endian;
    bool m_ok = true;

    bool m_hasUnknown = false;
    quint32 m_unknown[27];

    bits::bitstream *m_input = nullptr;
};

struct SaveData : public BaseSave
{
    Q_GADGET

public:
    bool load(bits::bitstream *input, const QSysInfo::Endian endian);

    QDateTime m_timestamp;
    QString m_saveFileName;
    quint16 m_gameVersion;
    quint16 m_saveVersion;
    quint16 m_unknown1;
    quint16 m_unknown2;
    quint32 m_unknown3;

    quint32 m_userBuildInfo;
    QString m_levelName;

    QStringList m_preloadedBundles;
};


class SaveFile : public QObject, public Serializable
{
    Q_OBJECT

public:
    explicit SaveFile(QObject *parent = nullptr);

    bool load(QIODevice *input);

signals:

private:
    SaveHeader m_header;
    SaveData m_data;
};

#endif // SAVEFILE_H
