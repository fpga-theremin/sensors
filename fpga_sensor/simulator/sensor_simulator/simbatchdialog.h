#ifndef SIMBATCHDIALOG_H
#define SIMBATCHDIALOG_H

#include <QDialog>
#include <QObject>
#include <QComboBox>
#include <QThread>
#include <QTextEdit>
#include <QStatusBar>
#include <QLabel>
#include "simutils.h"
#include "simresultplot.h"

class SimThread : public QObject, public ProgressListener {
    Q_OBJECT
public:
    SimThread(QObject * parent = nullptr) : QObject(parent) {
    }
    ~SimThread() override {}
    //void run() override;
    void onProgress(int currentStage, int totalStages, QString msg) override;

public slots:
    void runSimulation(SimParams * params);
signals:
    //void singleTestDone(std::unique_ptr<SimResultsItem> singleTestResults);
    void allTestsDone(SimResultsHolder * allResults);
    void updateProgress(int currentStage, int totalStages, QString msg);
};


class SimBatchDialog : public QDialog
{
    Q_OBJECT
    QThread _workerThread;
protected:
    SimParams _params;
    SimThread * _simThread;
    bool _running;
    QPushButton * _btnStart;
    QPushButton * _btnClose;
    QTextEdit * _textEdit;
    QStatusBar * _statusBar;
    QLabel * _statusLabel;
    SimResultsHolder * _simResults;
    QComboBox * _cbResultSelection;
    SimResultPlot * _plot;
    QComboBox * createIntComboBox(int * field, const int * values, int multiplier = 1);
    QComboBox * createDoubleComboBox(double * field, const double * values);
public:
    SimBatchDialog(SimParams * params, QWidget * parent);
    ~SimBatchDialog() override;
    void closeEvent(QCloseEvent *event) override;
public slots:
    void startPressed();
    void closePressed();
    //void singleTestDone(std::shared_ptr<SimResultsItem> singleTestResults);
    void allTestsDone(SimResultsHolder * allResults);
    void updateProgress(int currentStage, int totalStages, QString msg);
signals:
    void runSimulation(SimParams * params);
};


#endif // SIMBATCHDIALOG_H
