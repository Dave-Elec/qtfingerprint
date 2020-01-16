# Qt Fingerprint library for ZFM fingerprint sensors

QtFingerprint is an unofficial fingerprint sensor library for the Qt Framework (http://qt.io).

The QtFingerprint library supports ZhianTec ZFM-20, ZFM-60, ZFM-70 and ZFM-100 Fingerprint sensors.

**Note:** The library is inspired by the Python library <https://github.com//bastianraschke/pyfingerprint>

### Usage(1): Use QtFingerprint as Qt5's addon module

#### Building the module

> **Note**: Perl is needed in this step.

* Clone this repository.
* Go to the top directory of the project and run

```bash
    qmake
    make
    make install
```
The library, the header files, and others will be installed to your system.
#### Using the module

* Add following line to your qmake's project file:

```
    QT += xlsx
```

* Then, import library in your code

```cpp
    #include <QtFingerprint>
```

### Usage(2): Use source code directly

The package contains a **qtfingerprint.pri** file that allows you to integrate the component into applications that use qmake for the build step.

* Download the source code.

* Put the source code in any directory you like. For example, 3rdparty:

```
    |-- project.pro
    |-- ....
    |-- 3rdparty\
    |     |-- qtingerprint\
    |     |
```

* Add following line to your qmake project file:

```
    include(3rdparty/qtfingerprint/src/fingerprint/qtfingerprint.pri)
```
* Then, import library in your code

```cpp
    #include <qfingerprint.h>
```


## License
[![License: GPL v3](https://img.shields.io/badge/License-GPL%20v3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
