/****************************************************************************
** Meta object code from reading C++ file 'mainwindow.h'
**
** Created: Tue Dec 27 15:24:43 2011
**      by: The Qt Meta Object Compiler version 62 (Qt 4.7.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "mainwindow.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'mainwindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 62
#error "This file was generated using the moc from 4.7.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_MainWindow[] = {

 // content:
       5,       // revision
       0,       // classname
       0,    0, // classinfo
      11,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: signature, parameters, type, tag, flags
      12,   11,   11,   11, 0x08,
      22,   11,   11,   11, 0x08,
      34,   11,   11,   11, 0x08,
      52,   11,   11,   11, 0x08,
      73,   11,   11,   11, 0x08,
      94,   11,   11,   11, 0x08,
     109,   11,   11,   11, 0x08,
     132,   11,   11,   11, 0x08,
     159,   11,   11,   11, 0x08,
     182,   11,   11,   11, 0x08,
     203,   11,   11,   11, 0x08,

       0        // eod
};

static const char qt_meta_stringdata_MainWindow[] = {
    "MainWindow\0\0exitApp()\0showAbout()\0"
    "setLuxBallScene()\0setLuxBallHDRScene()\0"
    "setLuxBallSkyScene()\0setSalaScene()\0"
    "setBenchmarkGPUsMode()\0"
    "setBenchmarkCPUsGPUsMode()\0"
    "setBenchmarkCPUsMode()\0setInteractiveMode()\0"
    "setPauseMode()\0"
};

const QMetaObject MainWindow::staticMetaObject = {
    { &QMainWindow::staticMetaObject, qt_meta_stringdata_MainWindow,
      qt_meta_data_MainWindow, 0 }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &MainWindow::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_MainWindow))
        return static_cast<void*>(const_cast< MainWindow*>(this));
    return QMainWindow::qt_metacast(_clname);
}

int MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: exitApp(); break;
        case 1: showAbout(); break;
        case 2: setLuxBallScene(); break;
        case 3: setLuxBallHDRScene(); break;
        case 4: setLuxBallSkyScene(); break;
        case 5: setSalaScene(); break;
        case 6: setBenchmarkGPUsMode(); break;
        case 7: setBenchmarkCPUsGPUsMode(); break;
        case 8: setBenchmarkCPUsMode(); break;
        case 9: setInteractiveMode(); break;
        case 10: setPauseMode(); break;
        default: ;
        }
        _id -= 11;
    }
    return _id;
}
QT_END_MOC_NAMESPACE
