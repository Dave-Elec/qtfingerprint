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

#include "qfingerprint.h"
#include <QByteArray>
#include <QBitArray>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QImage>
#include <QDebug>


QFingerprintException::QFingerprintException(const std::string& message) : message_(message) {
}


QFingerprint::QFingerprint(QObject* parent)
    : QObject(parent)
{
}

QFingerprint::~QFingerprint() {
    if (serial()) {
        if (serial()->isOpen()) {
            qDebug() << "Closing port!";
            serial()->close();
        }
    }
}

quint32 QFingerprint::address() const {
    return m_address;
}

// void QFingerprint::setAddress(quint32 address) {
//     m_address = address;
//     emit addressChanged();
// }

quint32 QFingerprint::password() const {
    return m_password;
}

// void QFingerprint::setPassword(quint32 password) {
//     m_password = password;
//     emit passwordChanged();
// }

quint32 QFingerprint::timeout() const {
    return m_timeout;
}

void QFingerprint::setTimeout(quint32 timeout) {
    m_timeout = timeout;
    emit timeoutChanged();
}

QSerialPort* QFingerprint::serial() const {
    return m_serial;
}

void QFingerprint::setSerial(QSerialPort* serial) {
    m_serial = serial;
    emit serialChanged();
}

uint8_t QFingerprint::rightShift(ulong n, int x) {
    return (n >> x & 0xFF);
}

ulong QFingerprint::leftShift(ulong n, int x) {
    return n << x;
}

bool QFingerprint::bitAtPosition(ulong n, uint8_t p) {
    ulong mask = 1 << p;
    return (n & mask) > 0;
}

void QFingerprint::initialize_device(QString port, quint32 baudRate, quint32 address, quint32 password)
{
    if (baudRate < 9600 || baudRate > 115200 || baudRate % 9600 != 0) {
        throw QFingerprintException("Invalid baudrate!");
    }
    if (address < 0x00000000 || address > 0xFFFFFFFF) {
        qDebug() << address;
        throw QFingerprintException("Invalid address!");
    }
    if (password < 0x00000000 || password > 0xFFFFFFFF) {
        qDebug() << password;
        throw QFingerprintException("Invalid password!");
    }

    // this->setAddress(address);
    // this->setPassword(password);
    this->m_address = address;
    this->m_password = password;
    this->setTimeout(500);

    QSerialPort* serialPort = new QSerialPort(this);
    this->setSerial(serialPort);
    serialPort->setPortName(port);
    serialPort->setBaudRate(baudRate);
    serialPort->setDataBits(QSerialPort::Data8);
    serialPort->setParity(QSerialPort::NoParity);
    serialPort->setStopBits(QSerialPort::OneStop);
    serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (serialPort->isOpen()) {
        serialPort->close();
    }

    if(!serialPort->open(QIODevice::ReadWrite)){
        throw QFingerprintException("Connection failed!");
    }
}

// bool QIODevice::putChar(char c) {
//     qDebug() << QString("0x%1").arg((uint8_t)c, 2, 16, QLatin1Char( '0' )).toUpper().toLatin1().data();
//     return true;
// }

// bool QIODevice::getChar(char* c) {
//     int i = *c;
//     *c = QFingerprint::receivedData().at(i);
//     // this->putChar(*c);
//     return true;
// }

void QFingerprint::writePacket(uint8_t packetType, QByteArray packetPayload) {
    QSerialPort* serial = this->serial();
    QByteArray packetData;

    // Write header (one byte at once)
    packetData.append(rightShift(FINGERPRINT_STARTCODE, 8));
    packetData.append(rightShift(FINGERPRINT_STARTCODE, 0));

    packetData.append(rightShift(address(), 24));
    packetData.append(rightShift(address(), 16));
    packetData.append(rightShift(address(), 8));
    packetData.append(rightShift(address(), 0));

    packetData.append(packetType);

    // The packet length = package payload (n bytes) + checksum (2 bytes)
    uint8_t packetLength = packetPayload.length() + 2;

    packetData.append(rightShift(packetLength, 8));
    packetData.append(rightShift(packetLength, 0));

    // The packet checksum = packet type (1 byte) + packet length (2 bytes) + payload (n bytes)
    ulong packetChecksum = packetType + rightShift(packetLength, 8) + rightShift(packetLength, 0);

    // Write payload
    for (char data : packetPayload) {
        packetData.append(data);
        packetChecksum += data;
    }

    // Write checksum (2 bytes)
    packetData.append(rightShift(packetChecksum, 8));
    packetData.append(rightShift(packetChecksum, 0));

    serial->write(packetData);
    if(!serial->waitForBytesWritten(this->timeout())){
        throw QFingerprintException("Write timeout!");
    }
}


QByteArray QFingerprint::readPacket() {
    QSerialPort* serial = this->serial();
    QByteArray receivedPacketData;
    int i = 0;

    while(true) {
        char receivedFragment;
        if (serial->bytesAvailable() < 1) {
            if(!serial->waitForReadyRead(this->timeout())) {
                throw QFingerprintException("Read timeout!");
            }
        }
        if (serial->getChar(&receivedFragment)) {
            // Insert byte if packet seems valid
            receivedPacketData.insert(i, (uint8_t)receivedFragment);
            i++;
        }

        // Packet could be complete (the minimal packet size is 12 bytes)
        if (i >= 12) {
            // Check the packet header
            if (receivedPacketData[0] != rightShift(FINGERPRINT_STARTCODE, 8) || receivedPacketData[1] != rightShift(FINGERPRINT_STARTCODE, 0)) {
                throw QFingerprintException("The received packet do not begin with a valid header!");
            }

            //  Calculate packet payload length (combine the 2 length bytes)
            uint8_t packetPayloadLength;
            packetPayloadLength = leftShift((uint8_t)receivedPacketData[7], 8) | leftShift((uint8_t)receivedPacketData[8], 0);

            // Check if the packet is still fully received
            // Condition: index counter < packet payload length + packet frame
            if ( i < packetPayloadLength + 9 ) {
                continue;
            }

            // At this point the packet should be fully received
            uint8_t packetType = (uint8_t)receivedPacketData[6];

            // Calculate checksum:
            // checksum = packet type (1 byte) + packet length (2 bytes) + packet payload (n bytes)
            ulong packetChecksum = packetType + packetPayloadLength;

            QByteArray packetData;
            packetData.append(packetType);
            for (int j = 9; j < packetPayloadLength + 9 - 2; j++) {
                packetData.append((uint8_t)receivedPacketData[j]);
                packetChecksum += (uint8_t)receivedPacketData[j];
            }

            // Calculate full checksum of the 2 separate checksum bytes
            ulong receivedChecksum = leftShift((uint8_t)receivedPacketData[i - 2], 8) | leftShift((uint8_t)receivedPacketData[i - 1], 0);

            if ( receivedChecksum != packetChecksum ) {
                qDebug() << "Calculated Checksum:" << packetChecksum;
                qDebug() << "Received Checksum:" << receivedChecksum;
                throw QFingerprintException("The received packet is corrupted (the checksum is wrong)!");
            }

            return packetData;
        }
    }
}

bool QFingerprint::verifyPassword() {
    QByteArray packetPayload;
    packetPayload.append(FINGERPRINT_VERIFYPASSWORD)
                 .append(rightShift(password(), 24))
                 .append(rightShift(password(), 16))
                 .append(rightShift(password(), 8))
                 .append(rightShift(password(), 0));

    this->writePacket(FINGERPRINT_COMMANDPACKET, packetPayload);

    QByteArray receivedPacket = this->readPacket();
    uint8_t receivedPacketType = receivedPacket[0];
    QByteArray receivedPacketPayload = receivedPacket.mid(1);

    if (receivedPacketType != FINGERPRINT_ACKPACKET) {
        throw QFingerprintException("The received packet is no ack packet!");
    }

    if (receivedPacketPayload[0] == FINGERPRINT_OK) {
        return true;
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_COMMUNICATION) {
        throw QFingerprintException("Communication error");
    }else if (receivedPacketPayload[0] == FINGERPRINT_ADDRCODE) {
        throw QFingerprintException("The address is wrong");
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_WRONGPASSWORD) {
        return false;
    }else {
        QString message("Unknown error 0x");
        message += receivedPacketPayload.left(1).toHex();
        throw QFingerprintException(message.toStdString());
    }
}

bool QFingerprint::setPassword(quint32 newPassword) {
    if (newPassword < 0x00000000 || newPassword > 0xFFFFFFFF){
        throw QFingerprintException("The given password is invalid!");
    }

    QByteArray packetPayload;
    packetPayload.append(FINGERPRINT_SETPASSWORD)
                 .append(rightShift(newPassword, 24))
                 .append(rightShift(newPassword, 16))
                 .append(rightShift(newPassword, 8))
                 .append(rightShift(newPassword, 0));

    this->writePacket(FINGERPRINT_COMMANDPACKET, packetPayload);

    QByteArray receivedPacket = this->readPacket();
    uint8_t receivedPacketType = receivedPacket[0];
    QByteArray receivedPacketPayload = receivedPacket.mid(1);

    if (receivedPacketType != FINGERPRINT_ACKPACKET) {
        throw QFingerprintException("The received packet is no ack packet!");
    }

    if (receivedPacketPayload[0] == FINGERPRINT_OK) {
        this->m_password = newPassword;
        return true;
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_COMMUNICATION) {
        throw QFingerprintException("Communication error");
    }else {
        QString message("Unknown error 0x");
        message += receivedPacketPayload.left(1).toHex();
        throw QFingerprintException(message.toStdString());
    }
}

bool QFingerprint::setAddress(quint32 newAddress) {
    if (newAddress < 0x00000000 || newAddress > 0xFFFFFFFF){
        throw QFingerprintException("The given address is invalid!");
    }

    QByteArray packetPayload;
    packetPayload.append(FINGERPRINT_SETADDRESS)
                 .append(rightShift(newAddress, 24))
                 .append(rightShift(newAddress, 16))
                 .append(rightShift(newAddress, 8))
                 .append(rightShift(newAddress, 0));

    this->writePacket(FINGERPRINT_COMMANDPACKET, packetPayload);

    QByteArray receivedPacket = this->readPacket();
    uint8_t receivedPacketType = receivedPacket[0];
    QByteArray receivedPacketPayload = receivedPacket.mid(1);

    if (receivedPacketType != FINGERPRINT_ACKPACKET) {
        throw QFingerprintException("The received packet is no ack packet!");
    }

    if (receivedPacketPayload[0] == FINGERPRINT_OK) {
        this->m_address = newAddress;
        return true;
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_COMMUNICATION) {
        throw QFingerprintException("Communication error");
    }else {
        QString message("Unknown error 0x");
        message += receivedPacketPayload.left(1).toHex();
        throw QFingerprintException(message.toStdString());
    }
}

bool QFingerprint::setSystemParameter(uint8_t parameterNumber, uint8_t parameterValue) {
    // Validate the baudrate parameter
    if (parameterNumber == FINGERPRINT_SETSYSTEMPARAMETER_BAUDRATE) {
        if(parameterValue < 1 or parameterValue > 12) {
            throw QFingerprintException("The given parameter is invalid!");
        }
    // Validate the security level parameter
    } else if (parameterNumber == FINGERPRINT_SETSYSTEMPARAMETER_SECURITY_LEVEL) {
        if (parameterValue < 1 or parameterValue > 5) {
            throw QFingerprintException("The given security level parameter is invalid!");
        }
    // Validate the package length parameter
    } else if (parameterNumber == FINGERPRINT_SETSYSTEMPARAMETER_PACKAGE_SIZE) {
        if (parameterValue < 0 or parameterValue > 3) {
            throw QFingerprintException("The given length parameter is invalid!");
        }
    // The parameter number is not valid
    } else{
        throw QFingerprintException("The given parameter number is invalid!");
    }

    QByteArray packetPayload;
    packetPayload.append(FINGERPRINT_SETSYSTEMPARAMETER)
                 .append(parameterNumber)
                 .append(parameterValue);

    this->writePacket(FINGERPRINT_COMMANDPACKET, packetPayload);

    QByteArray receivedPacket = this->readPacket();
    uint8_t receivedPacketType = receivedPacket[0];
    QByteArray receivedPacketPayload = receivedPacket.mid(1);

    if (receivedPacketType != FINGERPRINT_ACKPACKET) {
        throw QFingerprintException("The received packet is no ack packet!");
    }

    if (receivedPacketPayload[0] == FINGERPRINT_OK) {
        return true;
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_COMMUNICATION) {
        throw QFingerprintException("Communication error");
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_INVALIDREGISTER) {
        throw QFingerprintException("Invalid register number");
    }else {
        QString message("Unknown error 0x");
        message += receivedPacketPayload.left(1).toHex();
        throw QFingerprintException(message.toStdString());
    }
}

bool QFingerprint::setBaudRate(quint32 baudRate) {
    if (baudRate % 9600 !=0 ) {
        throw QFingerprintException("Invalid baudrate");
    }

    return this->setSystemParameter(FINGERPRINT_SETSYSTEMPARAMETER_BAUDRATE, (uint8_t)(baudRate / 9600));
}

bool QFingerprint::setSecurityLevel(uint8_t securityLevel) {
    return this->setSystemParameter(FINGERPRINT_SETSYSTEMPARAMETER_SECURITY_LEVEL, securityLevel);
}

bool QFingerprint::setMaxPacketSize(uint8_t packetSize) {
    std::map<uint8_t, uint8_t> packetSizes = {{32, 0}, {64, 1}, {128, 2}, {256, 3}};

    if (packetSizes.find(packetSize) == packetSizes.end()) {
        throw QFingerprintException("Invalid packet size");
    }

    uint8_t packageSizeType = packetSizes[packetSize];

    return this->setSystemParameter(FINGERPRINT_SETSYSTEMPARAMETER_PACKAGE_SIZE, packageSizeType);
}

QByteArray QFingerprint::getSystemParameters() {
    QByteArray packetPayload;
    packetPayload.append(FINGERPRINT_GETSYSTEMPARAMETERS);

    this->writePacket(FINGERPRINT_COMMANDPACKET, packetPayload);

    QByteArray receivedPacket = this->readPacket();
    uint8_t receivedPacketType = receivedPacket[0];
    QByteArray receivedPacketPayload = receivedPacket.mid(1);

    if (receivedPacketType != FINGERPRINT_ACKPACKET) {
        throw QFingerprintException("The received packet is no ack packet!");
    }

    if (receivedPacketPayload[0] == FINGERPRINT_OK) {
        return receivedPacketPayload.mid(1);
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_COMMUNICATION) {
        throw QFingerprintException("Communication error");
    }else {
        QString message("Unknown error 0x");
        message += receivedPacketPayload.left(1).toHex();
        throw QFingerprintException(message.toStdString());
    }
}

quint16 QFingerprint::getStorageCapacity() {
    QByteArray systemParameters;
    systemParameters = this->getSystemParameters();

    return this->leftShift((uint8_t)systemParameters[4], 8) | this->leftShift((uint8_t)systemParameters[5], 0);
}

quint16 QFingerprint::getSecurityLevel() {
    QByteArray systemParameters;
    systemParameters = this->getSystemParameters();

    return this->leftShift((uint8_t)systemParameters[6], 8) | this->leftShift((uint8_t)systemParameters[7], 0);
}

quint16 QFingerprint::getMaxPacketSize() {
    QByteArray systemParameters;
    systemParameters = this->getSystemParameters();

    quint16 packetMaxSizeType =  this->leftShift((uint8_t)systemParameters[12], 8) | this->leftShift((uint8_t)systemParameters[13], 0);

    quint16 packetSizes[] = {32, 64, 128, 256};

    if (packetMaxSizeType < 0 || packetMaxSizeType > 3) {
        throw QFingerprintException("Invalid packet size");
    }
    return packetSizes[packetMaxSizeType];
}

quint16 QFingerprint::getBaudRate() {
    QByteArray systemParameters;
    systemParameters = this->getSystemParameters();

    quint16 baudRateType =  this->leftShift((uint8_t)systemParameters[14], 8) | this->leftShift((uint8_t)systemParameters[15], 0);

    return baudRateType * 9600;
}

QBitArray QFingerprint::getTemplateIndex(uint8_t page) {
    if (page < 0 | page > 3) {
        throw QFingerprintException("The given index page is invalid!");
    }

    QByteArray packetPayload;
    packetPayload.append(FINGERPRINT_TEMPLATEINDEX)
                 .append(page);

    this->writePacket(FINGERPRINT_COMMANDPACKET, packetPayload);

    QByteArray receivedPacket = this->readPacket();
    uint8_t receivedPacketType = receivedPacket[0];
    QByteArray receivedPacketPayload = receivedPacket.mid(1);

    if (receivedPacketType != FINGERPRINT_ACKPACKET) {
        throw QFingerprintException("The received0 packet is no ack packet!");
    }

    if (receivedPacketPayload[0] == FINGERPRINT_OK) {
        QByteArray pageElements = receivedPacketPayload.mid(1);
        QBitArray templateIndex(pageElements.size()*8);
        for (int i = 0; i < pageElements.size(); i++) {
            for (int j = 0; j < 8; j++) {
                // bool positionIsUsed = this->bitAtPosition(byte, i) == 1;
                templateIndex.setBit(bitAtPosition(pageElements.at(i), j), i * 8 + j);
            }
        }
        return templateIndex;
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_COMMUNICATION) {
        throw QFingerprintException("Communication error");
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_INVALIDPOSITION) {
        throw QFingerprintException("Invalid position");
    }else {
        QString message("Unknown error 0x");
        message += receivedPacketPayload.left(1).toHex();
        throw QFingerprintException(message.toStdString());
    }
}

quint16 QFingerprint::getTemplateCount() {
    QByteArray packetPayload;
    packetPayload.append(FINGERPRINT_TEMPLATECOUNT);

    this->writePacket(FINGERPRINT_COMMANDPACKET, packetPayload);

    QByteArray receivedPacket = this->readPacket();
    uint8_t receivedPacketType = receivedPacket[0];
    QByteArray receivedPacketPayload = receivedPacket.mid(1);

    if (receivedPacketType != FINGERPRINT_ACKPACKET) {
        throw QFingerprintException("The received packet is no ack packet!");
    }

    if (receivedPacketPayload[0] == FINGERPRINT_OK) {
        quint16 templateCount;
        templateCount = this->leftShift((uint8_t)receivedPacketPayload[1], 8);
        templateCount |= this->leftShift((uint8_t)receivedPacketPayload[2], 0);
        return templateCount;
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_COMMUNICATION) {
        throw QFingerprintException("Communication error");
    }else {
        QString message("Unknown error 0x");
        message += receivedPacketPayload.left(1).toHex();
        throw QFingerprintException(message.toStdString());
    }
}

bool QFingerprint::readImage() {
    QByteArray packetPayload;
    packetPayload.append(FINGERPRINT_READIMAGE);


    this->writePacket(FINGERPRINT_COMMANDPACKET, packetPayload);

    QByteArray receivedPacket = this->readPacket();
    uint8_t receivedPacketType = receivedPacket[0];
    QByteArray receivedPacketPayload = receivedPacket.mid(1);

    if (receivedPacketType != FINGERPRINT_ACKPACKET) {
        throw QFingerprintException("The received packet is no ack packet!");
    }

    if (receivedPacketPayload[0] == FINGERPRINT_OK) {
        return true;
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_COMMUNICATION) {
        throw QFingerprintException("Communication error");
    // No finger found
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_NOFINGER) {
        return false;
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_READIMAGE) {
        throw QFingerprintException("Could not read image");
    }else {
        QString message("Unknown error 0x");
        message += receivedPacketPayload.left(1).toHex();
        throw QFingerprintException(message.toStdString());
    }
}

void QFingerprint::downloadImage(QString imageDestination) {
    QFileInfo fileInfo(imageDestination);
    QDir destinationdirectory = fileInfo.dir();

    if (!destinationdirectory.exists() || !QFileInfo(destinationdirectory.path()).isWritable()) {
        throw QFingerprintException("The given destination directory " + destinationdirectory.path().toStdString() + " is not writable!");
    }

    QByteArray packetPayload;
    packetPayload.append(FINGERPRINT_DOWNLOADIMAGE);

    this->writePacket(FINGERPRINT_COMMANDPACKET, packetPayload);

    // Get the first reply packet
    QByteArray receivedPacket = this->readPacket();
    uint8_t receivedPacketType = receivedPacket[0];
    QByteArray receivedPacketPayload = receivedPacket.mid(1);

    if (receivedPacketType != FINGERPRINT_ACKPACKET) {
        throw QFingerprintException("The received packet is no ack packet!");
    }

    // The sensor will send follow-up packets
    if (receivedPacketPayload[0] == FINGERPRINT_OK) {
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_COMMUNICATION) {
        throw QFingerprintException("Communication error");
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_DOWNLOADIMAGE) {
        throw QFingerprintException("Could not download image");
    }else {
        QString message("Unknown error 0x");
        message += receivedPacketPayload.left(1).toHex();
        throw QFingerprintException(message.toStdString());
    }

    QByteArrayList imageData;

    // Get follow-up data packets until the last data packet is recieved
    while (receivedPacketType != FINGERPRINT_ENDDATAPACKET) {
        receivedPacket = this->readPacket();
        receivedPacketType = receivedPacket[0];
        receivedPacketPayload = receivedPacket.mid(1);

        if (receivedPacketType != FINGERPRINT_DATAPACKET && receivedPacketType != FINGERPRINT_ENDDATAPACKET) {
            throw QFingerprintException("The received packet is no data packet!");
        }

        imageData.append(receivedPacketPayload);
    }
    QImage img(256, 288, QImage::Format_Grayscale8);
    uchar* start = img.bits();
    uchar* ptr = start + img.width() * img.height();

    quint16 x=0, row=0, col=0;
    for (; ptr > start; ptr--) {
        if (x % 2 == 0) {
            *ptr = (imageData[row][col] >> 4) * 17;
        } else {
            *ptr = (imageData[row][col] & 0x0F) * 17;
            col++;
        }
        x++;
        if (col >= imageData[col].size()) {
            x=0; col = 0;
            row++;
        }
    }
    img.save(imageDestination);
}

bool QFingerprint::convertImage(uint8_t charBufferNumber) {
    if (charBufferNumber != FINGERPRINT_CHARBUFFER1 && charBufferNumber != FINGERPRINT_CHARBUFFER2) {
        throw QFingerprintException("The given charbuffer number is invalid!");
    }

    QByteArray packetPayload;
    packetPayload.append(FINGERPRINT_CONVERTIMAGE)
                 .append(charBufferNumber);

    this->writePacket(FINGERPRINT_COMMANDPACKET, packetPayload);

    QByteArray receivedPacket = this->readPacket();
    uint8_t receivedPacketType = receivedPacket[0];
    QByteArray receivedPacketPayload = receivedPacket.mid(1);

    if (receivedPacketType != FINGERPRINT_ACKPACKET) {
        throw QFingerprintException("The received packet is no ack packet!");
    }

    if (receivedPacketPayload[0] == FINGERPRINT_OK) {
        return true;
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_COMMUNICATION) {
        throw QFingerprintException("Communication error");
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_MESSYIMAGE) {
        throw QFingerprintException("The image is too messy");
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_FEWFEATUREPOINTS) {
        throw QFingerprintException("The image contains too few feature points");
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_INVALIDIMAGE) {
        throw QFingerprintException("The image is invalid");
    }else {
        QString message("Unknown error 0x");
        message += receivedPacketPayload.left(1).toHex();
        throw QFingerprintException(message.toStdString());
    }
}

bool QFingerprint::createTemplate() {
    QByteArray packetPayload;
    packetPayload.append(FINGERPRINT_CREATETEMPLATE);

    this->writePacket(FINGERPRINT_COMMANDPACKET, packetPayload);

    QByteArray receivedPacket = this->readPacket();
    uint8_t receivedPacketType = receivedPacket[0];
    QByteArray receivedPacketPayload = receivedPacket.mid(1);

    if (receivedPacketType != FINGERPRINT_ACKPACKET) {
        throw QFingerprintException("The received packet is no ack packet!");
    }

    // Template created successful
    if (receivedPacketPayload[0] == FINGERPRINT_OK) {
        return true;
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_COMMUNICATION) {
        throw QFingerprintException("Communication error");
    // The characteristics not matching
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_CHARACTERISTICSMISMATCH) {
        return false;
    }else {
        QString message("Unknown error 0x");
        message += receivedPacketPayload.left(1).toHex();
        throw QFingerprintException(message.toStdString());
    }
}

quint16 QFingerprint::storeTemplate(qint16 positionNumber, uint8_t charBufferNumber) {
    // Find a free index
    if (positionNumber = -1) {
        for (int page; page < 4; page++) {
            // Free index found
            if (positionNumber >=0) {
                break;
            }

            QBitArray templateIndex = this->getTemplateIndex(page);
            for (int i=0; i < templateIndex.size(); i++) {
                // Index not used?
                if (!templateIndex[i]) {
                    positionNumber = templateIndex.size() * page + i;
                    break;
                }
            }
        }
    }

    if (positionNumber < 0x0000 or positionNumber >= this->getStorageCapacity()) {
        throw QFingerprintException("The given position number is invalid!");
    }

    if (charBufferNumber != FINGERPRINT_CHARBUFFER1 && charBufferNumber != FINGERPRINT_CHARBUFFER2) {
        throw QFingerprintException("The given charbuffer number is invalid!");
    }

    QByteArray packetPayload;
    packetPayload.append(FINGERPRINT_STORETEMPLATE)
                 .append(charBufferNumber)
                 .append(this->rightShift(positionNumber, 8))
                 .append(this->rightShift(positionNumber, 0));

    this->writePacket(FINGERPRINT_COMMANDPACKET, packetPayload);

    QByteArray receivedPacket = this->readPacket();
    uint8_t receivedPacketType = receivedPacket[0];
    QByteArray receivedPacketPayload = receivedPacket.mid(1);

    if (receivedPacketType != FINGERPRINT_ACKPACKET) {
        throw QFingerprintException("The received packet is no ack packet!");
    }

    // Template stored successful
    if (receivedPacketPayload[0] == FINGERPRINT_OK) {
        return (quint8)positionNumber;
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_COMMUNICATION) {
        throw QFingerprintException("Communication error");
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_INVALIDPOSITION) {
        throw QFingerprintException("Could not store template in that position");
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_FLASH) {
        throw QFingerprintException("Error writing to flash");
    }else {
        QString message("Unknown error 0x");
        message += receivedPacketPayload.left(1).toHex();
        throw QFingerprintException(message.toStdString());
    }
}

QList<qint16> QFingerprint::searchTemplate(uint8_t charBufferNumber, quint16 positionStart, qint16 count) {
    if (charBufferNumber != FINGERPRINT_CHARBUFFER1 && charBufferNumber != FINGERPRINT_CHARBUFFER2) {
        throw QFingerprintException("The given charbuffer number is invalid!");
    }

    quint16 templatesCount;
    if (count > 0) {
        templatesCount = count;
    }else {
        templatesCount = this->getStorageCapacity();
    }

    QByteArray packetPayload;
    packetPayload.append(FINGERPRINT_SEARCHTEMPLATE)
                 .append(charBufferNumber)
                 .append(this->rightShift(positionStart, 8))
                 .append(this->rightShift(positionStart, 0))
                 .append(this->rightShift(templatesCount, 8))
                 .append(this->rightShift(templatesCount, 0));

    this->writePacket(FINGERPRINT_COMMANDPACKET, packetPayload);

    QByteArray receivedPacket = this->readPacket();
    uint8_t receivedPacketType = receivedPacket[0];
    QByteArray receivedPacketPayload = receivedPacket.mid(1);

    if (receivedPacketType != FINGERPRINT_ACKPACKET) {
        throw QFingerprintException("The received packet is no ack packet!");
    }

    // Template stored successful
    if (receivedPacketPayload[0] == FINGERPRINT_OK) {
        quint16 positionNumber = this->leftShift((uint8_t)receivedPacketPayload[1], 8);
        positionNumber = positionNumber | this->leftShift((uint8_t)receivedPacketPayload[2], 0);
        quint16 accuracyScore = this->leftShift((uint8_t)receivedPacketPayload[3], 8);
        accuracyScore = accuracyScore | this->leftShift((uint8_t)receivedPacketPayload[4], 0);
        return {positionNumber, accuracyScore};
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_COMMUNICATION) {
        throw QFingerprintException("Communication error");
    // Did not find a matching template
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_NOTEMPLATEFOUND) {
        return {-1, -1};
    }else {
        QString message("Unknown error 0x");
        message += receivedPacketPayload.left(1).toHex();
        throw QFingerprintException(message.toStdString());
    }
}

bool QFingerprint::loadTemplate(quint16 positionNumber, uint8_t charBufferNumber) {
    if (positionNumber < 0x0000 || positionNumber >= this->getStorageCapacity()) {
        throw QFingerprintException("The given positionNumber is invalid!");
    }

    if ( charBufferNumber != FINGERPRINT_CHARBUFFER1 && charBufferNumber != FINGERPRINT_CHARBUFFER2 ) {
        throw QFingerprintException("The given charbuffer number is invalid!");
    }

    QByteArray packetPayload;
    packetPayload.append(FINGERPRINT_LOADTEMPLATE)
                 .append(charBufferNumber)
                 .append(this->rightShift(positionNumber, 8))
                 .append(this->rightShift(positionNumber, 0));

    this->writePacket(FINGERPRINT_COMMANDPACKET, packetPayload);

    QByteArray receivedPacket = this->readPacket();
    uint8_t receivedPacketType = receivedPacket[0];
    QByteArray receivedPacketPayload = receivedPacket.mid(1);

    if (receivedPacketType != FINGERPRINT_ACKPACKET) {
        throw QFingerprintException("The received packet is no ack packet!");
    }

    // Template loaded successful
    if (receivedPacketPayload[0] == FINGERPRINT_OK) {
        return true;
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_COMMUNICATION) {
        throw QFingerprintException("Communication error");
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_LOADTEMPLATE) {
        throw QFingerprintException("The template could not be read");
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_INVALIDPOSITION) {
        throw QFingerprintException("Could not load template from that position");
    }else {
        QString message("Unknown error 0x");
        message += receivedPacketPayload.left(1).toHex();
        throw QFingerprintException(message.toStdString());
    }
}

bool QFingerprint::deleteTemplate(quint16 positionNumber, quint16 count) {
    quint16 capacity = this->getStorageCapacity();
    if (positionNumber < 0x0000 || positionNumber >= capacity) {
        throw QFingerprintException("The given position number is invalid!");
    }

    if (count < 0x0000 || count > capacity - positionNumber) {
        throw QFingerprintException("The given count is invalid!");
    }

    QByteArray packetPayload;
    packetPayload.append(FINGERPRINT_DELETETEMPLATE)
                 .append(this->rightShift(positionNumber, 8))
                 .append(this->rightShift(positionNumber, 0))
                 .append(this->rightShift(count, 8))
                 .append(this->rightShift(count, 0));

    this->writePacket(FINGERPRINT_COMMANDPACKET, packetPayload);

    QByteArray receivedPacket = this->readPacket();
    uint8_t receivedPacketType = receivedPacket[0];
    QByteArray receivedPacketPayload = receivedPacket.mid(1);

    if (receivedPacketType != FINGERPRINT_ACKPACKET) {
        throw QFingerprintException("The received packet is no ack packet!");
    }

    // Template deleted successful
    if (receivedPacketPayload[0] == FINGERPRINT_OK) {
        return true;
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_COMMUNICATION) {
        throw QFingerprintException("Communication error");
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_INVALIDPOSITION) {
        throw QFingerprintException("Invalid position");
    // Could not delete template
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_DELETETEMPLATE) {
        return false;
    }else {
        QString message("Unknown error 0x");
        message += receivedPacketPayload.left(1).toHex();
        throw QFingerprintException(message.toStdString());
    }
}
bool QFingerprint::clearDatabase() {
    QByteArray packetPayload;
    packetPayload.append(FINGERPRINT_CLEARDATABASE);

    this->writePacket(FINGERPRINT_COMMANDPACKET, packetPayload);

    QByteArray receivedPacket = this->readPacket();
    uint8_t receivedPacketType = receivedPacket[0];
    QByteArray receivedPacketPayload = receivedPacket.mid(1);

    if (receivedPacketType != FINGERPRINT_ACKPACKET) {
        throw QFingerprintException("The received packet is no ack packet!");
    }

    // Database cleared successful
    if (receivedPacketPayload[0] == FINGERPRINT_OK) {
        return true;
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_COMMUNICATION) {
        throw QFingerprintException("Communication error");
    // Could not clear database
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_CLEARDATABASE) {
        return false;
    }else {
        QString message("Unknown error 0x");
        message += receivedPacketPayload.left(1).toHex();
        throw QFingerprintException(message.toStdString());
    }
}

quint16 QFingerprint::compareCharacteristics() {
    QByteArray packetPayload;
    packetPayload.append(FINGERPRINT_COMPARECHARACTERISTICS);

    this->writePacket(FINGERPRINT_COMMANDPACKET, packetPayload);

    QByteArray receivedPacket = this->readPacket();
    uint8_t receivedPacketType = receivedPacket[0];
    QByteArray receivedPacketPayload = receivedPacket.mid(1);

    if (receivedPacketType != FINGERPRINT_ACKPACKET) {
        throw QFingerprintException("The received packet is no ack packet!");
    }

    // Comparison successful
    if (receivedPacketPayload[0] == FINGERPRINT_OK) {
        quint16 accuracyScore = this->leftShift((uint8_t)receivedPacketPayload[1], 8);
        accuracyScore = accuracyScore | this->leftShift((uint8_t)receivedPacketPayload[2], 0);
        return accuracyScore;
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_COMMUNICATION) {
        throw QFingerprintException("Communication error");
    // The characteristics do not match
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_CLEARDATABASE) {
        return 0;
    }else {
        QString message("Unknown error 0x");
        message += receivedPacketPayload.left(1).toHex();
        throw QFingerprintException(message.toStdString());
    }
}

bool QFingerprint::uploadCharacteristics(uint8_t charBufferNumber, QList<uint8_t> characteristicsData) {
    if ( charBufferNumber != FINGERPRINT_CHARBUFFER1 && charBufferNumber != FINGERPRINT_CHARBUFFER2 ) {
        throw QFingerprintException("The given charbuffer number is invalid!");
    }

    if (characteristicsData == QList<uint8_t>({0})) {
        throw QFingerprintException("The characteristics data required!");
    }

    quint16 maxPacketSize = this->getMaxPacketSize();

    // Upload command
    QByteArray packetPayload;
    packetPayload.append(FINGERPRINT_UPLOADCHARACTERISTICS)
                 .append(charBufferNumber);

    this->writePacket(FINGERPRINT_COMMANDPACKET, packetPayload);

    // Get first reply packet
    QByteArray receivedPacket = this->readPacket();
    uint8_t receivedPacketType = receivedPacket[0];
    QByteArray receivedPacketPayload = receivedPacket.mid(1);

    if (receivedPacketType != FINGERPRINT_ACKPACKET) {
        throw QFingerprintException("The received packet is no ack packet!");
    }

    // The sensor will wait for follow-up packets
    if (receivedPacketPayload[0] == FINGERPRINT_OK) {
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_COMMUNICATION) {
        throw QFingerprintException("Communication error");
    }else if (receivedPacketPayload[0] == FINGERPRINT_PACKETRESPONSEFAIL) {
        throw QFingerprintException("Could not upload characteristics");
    }else {
        QString message("Unknown error 0x");
        message += receivedPacketPayload.left(1).toHex();
        throw QFingerprintException(message.toStdString());
    }

    // Upload data packets
    quint16 packetNbr = characteristicsData.size() / maxPacketSize;

    QByteArray characteristicsPayload;
    for (uint8_t byte : characteristicsData) {
        characteristicsPayload.append(byte);
    }
    if (packetNbr <= 1) {
        this->writePacket(FINGERPRINT_ENDDATAPACKET, characteristicsPayload);
    }else {
        quint16 i=0;
        quint16 lfrom, lto;
        while (i < packetNbr) {
            lfrom = (i-1) * maxPacketSize;
            lto = lfrom + maxPacketSize;

            this->writePacket(FINGERPRINT_DATAPACKET, characteristicsPayload.mid(lfrom, lto));
            i++;
        }

        lfrom = (i-1) * maxPacketSize;
        lto = lfrom + maxPacketSize;
        this->writePacket(FINGERPRINT_ENDDATAPACKET, characteristicsPayload.mid(lfrom, lto));
    }

    // Verify uploaded characteristics
    QList<uint8_t> characteristics = this->downloadCharacteristics(charBufferNumber);
    qDebug()<<characteristics;
    return characteristics == characteristicsData;
}

quint32 QFingerprint::generateRandomNumber() {
    QByteArray packetPayload;
    packetPayload.append(FINGERPRINT_GENERATERANDOMNUMBER);

    this->writePacket(FINGERPRINT_COMMANDPACKET, packetPayload);

    // Get first reply packet
    QByteArray receivedPacket = this->readPacket();
    uint8_t receivedPacketType = receivedPacket[0];
    QByteArray receivedPacketPayload = receivedPacket.mid(1);

    if (receivedPacketType != FINGERPRINT_ACKPACKET) {
        throw QFingerprintException("The received packet is no ack packet!");
    }

    if (receivedPacketPayload[0] == FINGERPRINT_OK) {
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_COMMUNICATION) {
        throw QFingerprintException("Communication error");
    }else {
        QString message("Unknown error 0x");
        message += receivedPacketPayload.left(1).toHex();
        throw QFingerprintException(message.toStdString());
    }

    quint32 number = 0;
    number = number | this->leftShift((uint8_t)receivedPacketPayload[1], 24);
    number = number | this->leftShift((uint8_t)receivedPacketPayload[2], 16);
    number = number | this->leftShift((uint8_t)receivedPacketPayload[3], 8);
    number = number | this->leftShift((uint8_t)receivedPacketPayload[4], 0);

    return number;
}

QList<uint8_t> QFingerprint::downloadCharacteristics(uint8_t charBufferNumber) {
    if ( charBufferNumber != FINGERPRINT_CHARBUFFER1 && charBufferNumber != FINGERPRINT_CHARBUFFER2 ) {
        throw QFingerprintException("The given charbuffer number is invalid!");
    }

    QByteArray packetPayload;
    packetPayload.append(FINGERPRINT_DOWNLOADCHARACTERISTICS)
                 .append(charBufferNumber);

    this->writePacket(FINGERPRINT_COMMANDPACKET, packetPayload);

    // Get first reply packet
    QByteArray receivedPacket = this->readPacket();
    uint8_t receivedPacketType = receivedPacket[0];
    QByteArray receivedPacketPayload = receivedPacket.mid(1);

    if (receivedPacketType != FINGERPRINT_ACKPACKET) {
        throw QFingerprintException("The received packet is no ack packet!");
    }

    // The sensor will send follow-up packets
    if (receivedPacketPayload[0] == FINGERPRINT_OK) {
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_COMMUNICATION) {
        throw QFingerprintException("Communication error");
    }else if (receivedPacketPayload[0] == FINGERPRINT_ERROR_DOWNLOADCHARACTERISTICS) {
        throw QFingerprintException("Could not download characteristics");
    }else {
        QString message("Unknown error 0x");
        message += receivedPacketPayload.left(1).toHex();
        throw QFingerprintException(message.toStdString());
    }

    // Get follow-up data packets until last data packet is received
    QList<uint8_t> completePayload;

    while (receivedPacketType != FINGERPRINT_ENDDATAPACKET) {
        receivedPacket = this->readPacket();
        receivedPacketType = receivedPacket[0];
        receivedPacketPayload = receivedPacket.mid(1);

        if (receivedPacketType != FINGERPRINT_DATAPACKET && receivedPacketType != FINGERPRINT_ENDDATAPACKET) {
            throw QFingerprintException("The received packet is no data packet!");
        }

        for (uint8_t byte : receivedPacketPayload) {
            completePayload.append(byte);
        }
    }

    return completePayload;
}

