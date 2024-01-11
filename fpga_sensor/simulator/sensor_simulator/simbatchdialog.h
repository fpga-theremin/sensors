#ifndef SIMBATCHDIALOG_H
#define SIMBATCHDIALOG_H

#include <QDialog>
#include <QObject>
#include <QComboBox>

class SimBatchDialog : public QDialog
{
    Q_OBJECT
protected:
    int _variations;
    double _frequencyStep;
    double _phaseStep;
    QComboBox * createIntComboBox(int * field, const int * values, int multiplier = 1);
    QComboBox * createDoubleComboBox(double * field, const double * values);
public:
    SimBatchDialog(QWidget * parent);
};

#endif // SIMBATCHDIALOG_H
