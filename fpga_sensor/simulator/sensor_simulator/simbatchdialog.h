#ifndef SIMBATCHDIALOG_H
#define SIMBATCHDIALOG_H

#include <QDialog>
#include <QObject>
#include <QComboBox>
#include <QThread>
#include "simutils.h"

class SimThread : public QThread {
    Q_OBJECT
public:
    void run();

public slots:
    void runSimulation(SimParams * params, int freqVariations, double freqStep, int phaseVariations, double phaseStep );
signals:
    void singleTestDone(SimResultsItem * singleTestResults);
    void allTestsDone(SimResultsHolder * allResults);
};


class SimBatchDialog : public QDialog
{
    Q_OBJECT
protected:
    int _freqVariations;
    int _phaseVariations;
    double _frequencyStep;
    double _phaseStep;
    QComboBox * createIntComboBox(int * field, const int * values, int multiplier = 1);
    QComboBox * createDoubleComboBox(double * field, const double * values);
public:
    SimBatchDialog(QWidget * parent);
};


#endif // SIMBATCHDIALOG_H
