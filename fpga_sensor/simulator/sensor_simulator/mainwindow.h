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
    QThread _workerThread;
    SimThread * _simThread;
    bool _running;
    SimResultsHolder * _simResults;
    int _layoutSpacing;

    QMenuBar * _menuBar;
    QAction * _actionToolsRunSimulationBatch;
    SimParams _simParams;
    SimState _simState;
    QBoxLayout * _topLayout;
    QBoxLayout * _bottomLayout;
    QVBoxLayout * _leftLayout;
    QVBoxLayout * _rightLayout;
    PlotWidget * _plotWidget;
    QLineEdit * _measuredPhaseShift;
    QLineEdit * _measuredPhaseShiftError;
    QLineEdit * _measuredPhaseExactBits;
    QLineEdit * _realFrequency;
    SimResultPlot * _plot;
    QComboBox * _cbResult;

    QLineEdit * createDoubleValueEditor(double * field, double minValue, double maxValue, int precision);
    //QComboBox * createIntComboBox(int * field, const int * values, int multiplier = 1);
    //QComboBox * createDoubleComboBox(double * field, const double * values);
    QComboBox * createComboBox(SimParameter param);
    QLineEdit * createReadOnlyEdit();

private:
    void createMenu();
    void createControls();
    void recalculate();
    void updatePlot();
    void runBackgroundSim();
public slots:
    void runSimBatch();
    void allTestsDone(SimResultsHolder * allResults);
    void plotIndexChanged(int index);
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    void closeEvent(QCloseEvent *event) override;
signals:
    void runSimulation(SimParams * params);
};
#endif // MAINWINDOW_H
