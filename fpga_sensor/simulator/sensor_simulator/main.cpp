#include "mainwindow.h"

#include <QApplication>

#ifdef _DEBUG
//#define ENABLE_UNIT_TESTS
#endif

#include "dds.h"

int main(int argc, char *argv[])
{
//    qDebug("sin table 1, 1024x9bit scaling 1.0");
//    SinTable sinTable1(10, 9, 1.0);

//    qDebug("sin table 2, 2048x9bit scaling 1.0");
//    SinTable sinTable2(11, 9, 1.0);

//    qDebug("sin table 3, 1024x9bit scaling 9/16");
//    //8.062/16.0
//    SinTable sinTable3(10, 9, 8.0/16.0);

#ifdef ENABLE_UNIT_TESTS
    testDDS();
#endif

    FullSimSuite suite;
    suite.run();

    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
