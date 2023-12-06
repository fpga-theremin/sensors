#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "simutils.h"

#include <QMainWindow>
#include <QBoxLayout>
#include <QComboBox>
#include "plotwidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

    SimParams _simParams;
    SimState _simState;
    QBoxLayout * _topLayout;
    QBoxLayout * _bottomLayout;
    QComboBox * _cbNcoPhaseBits;
    QComboBox * _cbNcoValueBits;
    QComboBox * _cbAdcValueBits;
    QComboBox * _cbAdcNoise;
    QComboBox * _cbAdcAmplitude;
    PlotWidget * _plotWidget;
    QLineEdit * _measuredPhaseShift;
    QLineEdit * _measuredPhaseShiftError;
    QLineEdit * _realFrequency;

    QLineEdit * createDoubleValueEditor(double * field, double minValue, double maxValue, int precision);

private:
    void createControls();
    void recalculate();
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
};
#endif // MAINWINDOW_H
