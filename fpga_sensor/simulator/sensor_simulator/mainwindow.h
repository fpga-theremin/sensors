#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "simutils.h"

#include <QMainWindow>
#include <QBoxLayout>
#include <QComboBox>
#include <QAction>
#include <QMenuBar>
#include "plotwidget.h"
#include "simbatchdialog.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

    QMenuBar * _menuBar;
    QAction * _actionToolsRunSimulationBatch;
    SimParams _simParams;
    SimState _simState;
    QBoxLayout * _topLayout;
    QBoxLayout * _bottomLayout;
    PlotWidget * _plotWidget;
    QLineEdit * _measuredPhaseShift;
    QLineEdit * _measuredPhaseShiftError;
    QLineEdit * _measuredPhaseExactBits;
    QLineEdit * _realFrequency;
    SimResultPlot * _plot;
    QComboBox * _cbResult;

    QLineEdit * createDoubleValueEditor(double * field, double minValue, double maxValue, int precision);
    QComboBox * createIntComboBox(int * field, const int * values, int multiplier = 1);
    QComboBox * createDoubleComboBox(double * field, const double * values);
    QLineEdit * createReadOnlyEdit();

private:
    void createMenu();
    void createControls();
    void recalculate();
public slots:
    void runSimBatch();
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
};
#endif // MAINWINDOW_H
