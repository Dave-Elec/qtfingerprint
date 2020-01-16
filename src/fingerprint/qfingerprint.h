/****************************************************************************
**
** This file is part of the QtFingerprint module for Qt5.
** Copyright (C) 2020 Dawit Abate.
** Contact: dawitabate2@gmail.com
**
** $QT_BEGIN_LICENSE:GPLV3$
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.

** You should have received a copy of the GNU General Public License
** along with this program.  If not, see <https://www.gnu.org/licenses/>.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QFINGERPRINT_H
#define QFINGERPRINT_H

#include <QObject>
#include <QSerialPort>
#include <QException>
#include <exception>
#include <QDebug>

// Baotou start byte
#define FINGERPRINT_STARTCODE 0xEF01

// Packet identification
#define FINGERPRINT_COMMANDPACKET 0x01
#define FINGERPRINT_ACKPACKET 0x07
#define FINGERPRINT_DATAPACKET 0x02
#define FINGERPRINT_ENDDATAPACKET 0x08

// Instruction codes
#define FINGERPRINT_VERIFYPASSWORD 0x13
#define FINGERPRINT_SETPASSWORD 0x12
#define FINGERPRINT_SETADDRESS 0x15
#define FINGERPRINT_SETSYSTEMPARAMETER 0x0E
#define FINGERPRINT_GETSYSTEMPARAMETERS 0x0F
#define FINGERPRINT_TEMPLATEINDEX 0x1F
#define FINGERPRINT_TEMPLATECOUNT 0x1D
#define FINGERPRINT_READIMAGE 0x01
#define FINGERPRINT_DOWNLOADIMAGE 0x0A
#define FINGERPRINT_CONVERTIMAGE 0x02
#define FINGERPRINT_CREATETEMPLATE 0x05
#define FINGERPRINT_STORETEMPLATE 0x06
#define FINGERPRINT_SEARCHTEMPLATE 0x04
#define FINGERPRINT_LOADTEMPLATE 0x07
#define FINGERPRINT_DELETETEMPLATE 0x0C
#define FINGERPRINT_CLEARDATABASE 0x0D
#define FINGERPRINT_GENERATERANDOMNUMBER 0x14
#define FINGERPRINT_COMPARECHARACTERISTICS 0x03
#define FINGERPRINT_UPLOADCHARACTERISTICS 0x09
#define FINGERPRINT_DOWNLOADCHARACTERISTICS 0x08

// Parameters of setSystemParameter()
#define FINGERPRINT_SETSYSTEMPARAMETER_BAUDRATE 4
#define FINGERPRINT_SETSYSTEMPARAMETER_SECURITY_LEVEL 5
#define FINGERPRINT_SETSYSTEMPARAMETER_PACKAGE_SIZE 6

// Packet reply confirmations
#define FINGERPRINT_OK 0x00
#define FINGERPRINT_ERROR_COMMUNICATION 0x01
#define FINGERPRINT_ERROR_WRONGPASSWORD 0x13
#define FINGERPRINT_ERROR_INVALIDREGISTER 0x1A
#define FINGERPRINT_ERROR_NOFINGER 0x02
#define FINGERPRINT_ERROR_READIMAGE 0x03
#define FINGERPRINT_ERROR_MESSYIMAGE 0x06
#define FINGERPRINT_ERROR_FEWFEATUREPOINTS 0x07
#define FINGERPRINT_ERROR_INVALIDIMAGE 0x15
#define FINGERPRINT_ERROR_CHARACTERISTICSMISMATCH 0x0A
#define FINGERPRINT_ERROR_INVALIDPOSITION 0x0B
#define FINGERPRINT_ERROR_FLASH 0x18
#define FINGERPRINT_ERROR_NOTEMPLATEFOUND 0x09
#define FINGERPRINT_ERROR_LOADTEMPLATE 0x0C
#define FINGERPRINT_ERROR_DELETETEMPLATE 0x10
#define FINGERPRINT_ERROR_CLEARDATABASE 0x11
#define FINGERPRINT_ERROR_NOTMATCHING 0x08
#define FINGERPRINT_ERROR_DOWNLOADIMAGE 0x0F
#define FINGERPRINT_ERROR_DOWNLOADCHARACTERISTICS 0x0D

// Unknown error codes
#define FINGERPRINT_ADDRCODE 0x20
#define FINGERPRINT_PASSVERIFY 0x21
#define FINGERPRINT_PACKETRESPONSEFAIL 0x0E
#define FINGERPRINT_ERROR_TIMEOUT 0xFF
#define FINGERPRINT_ERROR_BADPACKET 0xFE

// Char buffers
#define FINGERPRINT_CHARBUFFER1 0x01
#define FINGERPRINT_CHARBUFFER2 0x02


class QFingerprintException: public std::exception {
private:
    std::string message_;
public:
    explicit QFingerprintException(const std::string& message);
    virtual const char* what() const throw() {
        return message_.c_str();
    }
};


class QFingerprint : public QObject {
    Q_OBJECT
    Q_PROPERTY(quint32 address MEMBER m_address)
    Q_PROPERTY(quint32 password MEMBER m_password)
    Q_PROPERTY(quint32 timeout MEMBER m_timeout)
    Q_PROPERTY(QSerialPort* serial MEMBER m_serial)

public:
    explicit QFingerprint(QObject* parent=nullptr);
    ~QFingerprint();

    quint32 address() const;
    bool setAddress(quint32 newAddress);

    quint32 password() const;
    bool setPassword(quint32 newPassword);

    quint32 timeout() const;
    void setTimeout(quint32 timeout);

    QSerialPort* serial() const;
    void setSerial(QSerialPort* serial);

    void initialize_device(QString port="/dev/ttyUSB0",
                           quint32 baudRate=57600,
                           quint32 address=0xFFFFFFFF,
                           quint32 password=0x00000000);

    void writePacket(uint8_t packetType, QByteArray packetPayload);
    QByteArray readPacket();
    bool verifyPassword();
    bool setPassword();

    bool setSystemParameter(uint8_t parameterNumber, uint8_t parameterValue);
    bool setBaudRate(quint32 baudRate);
    bool setSecurityLevel(uint8_t securityLevel);
    bool setMaxPacketSize(uint8_t packetSize);
    QByteArray getSystemParameters();
    quint16 getStorageCapacity();
    quint16 getSecurityLevel();
    quint16 getMaxPacketSize();
    quint16 getBaudRate();
    QBitArray getTemplateIndex(uint8_t page);
    quint16 getTemplateCount();
    bool readImage();
    void downloadImage(QString imageDestination);
    bool convertImage(uint8_t charBufferNumber = FINGERPRINT_CHARBUFFER1);
    bool createTemplate();
    quint16 storeTemplate(qint16 positionNumber = -1,
                          uint8_t charBufferNumber = FINGERPRINT_CHARBUFFER1);

    QList<qint16> searchTemplate(uint8_t charBufferNumber = FINGERPRINT_CHARBUFFER1,
                                  quint16 positionStart = 0,
                                  qint16 count = -1);
    bool loadTemplate(quint16 positionNumber,
                      uint8_t charBufferNumber = FINGERPRINT_CHARBUFFER1);
    bool deleteTemplate(quint16 positionNumber, quint16 count);
    bool clearDatabase();
    quint16 compareCharacteristics();
    bool uploadCharacteristics(uint8_t charBufferNumber = FINGERPRINT_CHARBUFFER1,
                               QList<uint8_t> characteristicsData = {0});
    quint32 generateRandomNumber();
    QList<uint8_t> downloadCharacteristics(uint8_t charBufferNumber = FINGERPRINT_CHARBUFFER1);


signals:
    void addressChanged();
    void passwordChanged();
    void timeoutChanged();
    void serialChanged();

private:
    quint32 m_address;
    quint32 m_password;
    quint32 m_timeout;
    QSerialPort* m_serial = nullptr;

    uint8_t rightShift(ulong n, int x);
    ulong leftShift(ulong n, int x);
    bool bitAtPosition(ulong n, uint8_t p);
};

#endif /* end of include guard */

