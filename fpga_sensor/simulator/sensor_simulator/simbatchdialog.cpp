#include "simbatchdialog.h"
#include <QBoxLayout>
#include <QFormLayout>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QLabel>
#include <QCloseEvent>
#include <QSpacerItem>
#include <QClipboard>
#include <QGuiApplication>

#define END_OF_LIST -1000000

//void SimBatchDialog::runSimulation(std::unique_ptr<SimParams> params) {

//}

void SimBatchDialog::startPressed() {
    qDebug("SimBatchDialog::startPressed()");
    if (_running)
        return;
    SimParams * p = new SimParams();
    *p = _params;
    _btnStart->setEnabled(false);
    _btnClose->setEnabled(false);
    _btnCopy->setEnabled(false);
    _textEdit->setPlainText(QString("Starting simulation batch.\n" + _params.toString() + "\n"));
    _running = true;
    emit runSimulation(p);
}

void SimBatchDialog::closePressed() {
    qDebug("SimBatchDialog::closePressed()");
    close();
}

void SimBatchDialog::copyPressed() {
    qDebug("SimBatchDialog::copyPressed()");
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setText(_textEdit->toPlainText());
}

//void SimBatchDialog::singleTestDone(std::shared_ptr<SimResultsItem> singleTestResults) {

//}

void SimBatchDialog::updateProgress(int currentStage, int totalStages, QString msg) {
    qDebug("SimBatchDialog::updateProgress currentStage=%d, totalStages=%d, msg=%s", currentStage, totalStages, msg.toLocal8Bit().data());
    QString state = QString("[%1 / %2] : %3").arg(currentStage).arg(totalStages).arg(msg);
    _textEdit->appendPlainText(state);
    _statusLabel->setText(state);
}

void SimBatchDialog::allTestsDone(SimResultsHolder * allResults) {
    qDebug("SimBatchDialog::allTestsDone()");
    // finalize
    _btnStart->setEnabled(true);
    _btnClose->setEnabled(true);
    _btnCopy->setEnabled(true);
    _textEdit->clear();
    _textEdit->setPlainText(allResults->text.join("\n"));
    _statusLabel->setText("Simulation is done");
    _running = false;
    if (_simResults)
        delete _simResults;
    _simResults = allResults;
    updatePlot();
}

SimBatchDialog::~SimBatchDialog() {
    _workerThread.quit();
    _workerThread.wait();
    if (_simResults)
        delete _simResults;
}

SimBatchDialog::SimBatchDialog(SimParams * params, QWidget * parent)
    : QDialog(parent), _simResults(nullptr)
{
    int spacing = 15;
    _running = false;
    _params = *params;
    _params.freqVariations = 10;
    _params.phaseVariations = 10;
    _params.freqStep = 0.00142466;
    _params.phaseStep = 0.0065441;
    _params.bitFractionCount = 2;

    _simThread = new SimThread();
    _simThread->moveToThread(&_workerThread);
    connect(&_workerThread, &QThread::finished, _simThread, &QObject::deleteLater);
    connect(this, &SimBatchDialog::runSimulation, _simThread, &SimThread::runSimulation);
    connect(_simThread, &SimThread::allTestsDone, this, &SimBatchDialog::allTestsDone);
    connect(_simThread, &SimThread::updateProgress, this, &SimBatchDialog::updateProgress);
    //connect(worker, &Worker::resultReady, this, &Controller::handleResults);
    _workerThread.start();



    setWindowTitle("Run simulation batch");
    QLayout * layout = new QVBoxLayout();
    // label on the top
    layout->addWidget(new QLabel(params->toString()));
    layout->setSpacing(spacing);

    QHBoxLayout * leftRightLayout = new QHBoxLayout();
    QVBoxLayout * leftLayout = new QVBoxLayout();
    QVBoxLayout * rightLayout = new QVBoxLayout();
    leftRightLayout->setSpacing(spacing);
    leftLayout->setSpacing(spacing);
    rightLayout->setSpacing(spacing);
    leftRightLayout->addItem(leftLayout);
    leftRightLayout->addItem(rightLayout);
    layout->addItem(leftRightLayout);

    QFormLayout * _simParamsLayout = new QFormLayout();
    QVBoxLayout * resultViewLayout = new QVBoxLayout();
    QHBoxLayout * buttonsLayout = new QHBoxLayout();
    _simParamsLayout->setSpacing(spacing);
    buttonsLayout->setSpacing(spacing);
    resultViewLayout->setSpacing(spacing);
    rightLayout->addItem(_simParamsLayout);
    rightLayout->addItem(buttonsLayout);
    rightLayout->addItem(resultViewLayout);
    rightLayout->addStretch();

    resultViewLayout->addWidget(new QLabel("Simulation result"));
    _cbResultSelection = new QComboBox();
    for (int i = SIM_PARAM_MIN; i <= SIM_PARAM_MAX; i++) {
        const SimParameterMetadata * metadata = SimParameterMetadata::get((SimParameter)i);
        _cbResultSelection->addItem(metadata->getName(), QVariant(i));
    }
    _cbResultSelection->setCurrentIndex(0);
    connect(_cbResultSelection, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SimBatchDialog::plotIndexChanged);
    resultViewLayout->addWidget(_cbResultSelection);

    //QTextEdit * resultPlot = new QTextEdit(this);
    _plot = new SimResultPlot();
    //_plot->setMinimumSize(QSize(600, 400));
    resultViewLayout->addWidget(_plot);


    _simParamsLayout->setSpacing(10);
    const int variations[] = {10, 20, 30, 40, 50, 100, END_OF_LIST};
    QComboBox * _cbFreqVariations = createIntComboBox(&_params.freqVariations, variations, 1);
    QComboBox * _cbPhaseVariations = createIntComboBox(&_params.phaseVariations, variations, 1);

    const int simMaxSamples[] = {10000, 20000, 50000, 100000, 500000, END_OF_LIST};
    QComboBox * _cbMaxSamples = createIntComboBox(&_params.simMaxSamples, simMaxSamples, 1);
    //QLineEdit * _edFrequency = createDoubleValueEditor(&_simParams.frequency, 100000, 9000000, 2);
    //_edFrequency->setMax

    _simParamsLayout->addRow(new QLabel("Max samples"), _cbMaxSamples);

    _simParamsLayout->addRow(new QLabel("Frequency variations"), _cbFreqVariations);

    const double freqStep[] = {0.00025294, 0.00142466, 0.0135345, 0.03837645, END_OF_LIST};
    _simParamsLayout->addRow(new QLabel("Frequency step mult"), createDoubleComboBox(&_params.freqStep, freqStep));

    _simParamsLayout->addRow(new QLabel("Phase variations"), _cbPhaseVariations);

    const double phaseStep[] = {0.000182734, 0.0024453565, 0.0065441, 0.01134385, END_OF_LIST};
    _simParamsLayout->addRow(new QLabel("Phase step"), createDoubleComboBox(&_params.phaseStep, phaseStep));

    _btnStart = new QPushButton("Start");
    connect(_btnStart, &QPushButton::pressed, this, &SimBatchDialog::startPressed);
    _btnCopy = new QPushButton("Copy");
    _btnCopy->setEnabled(false);
    connect(_btnCopy, &QPushButton::pressed, this, &SimBatchDialog::copyPressed);
    _btnClose = new QPushButton("Close");
    connect(_btnClose, &QPushButton::pressed, this, &QDialog::accept);
    buttonsLayout->addWidget(_btnStart);
    buttonsLayout->addWidget(_btnCopy);
    buttonsLayout->addWidget(_btnClose);


    _textEdit = new QPlainTextEdit(this);
    _textEdit->setMinimumSize(QSize(600, 400));
    _textEdit->setReadOnly(true);
    _textEdit->setLineWrapMode(QPlainTextEdit::LineWrapMode::NoWrap);
    _textEdit->setTabChangesFocus(false);
    _textEdit->setTabStopDistance(100);
    //_textEdit->setTa
    leftLayout->addWidget(_textEdit);
    _statusBar = new QStatusBar(this);
    layout->addWidget(_statusBar);

    _statusLabel = new QLabel("Press Start to execute simulation suite", this);
    _statusBar->addWidget(_statusLabel);

    setLayout(layout);
    setSizeGripEnabled(true);
}

void SimBatchDialog::updatePlot() {
    if (_simResults) {
        SimParameter type = (SimParameter)_cbResultSelection->currentData().toInt();
        SimResultsItem * result1 = _simResults->byType(type);
        if (result1)
            _plot->setSimResults(result1);
    }
}

void SimBatchDialog::plotIndexChanged(int index) {
    updatePlot();
}

void SimBatchDialog::closeEvent(QCloseEvent *event) {
    if (!_running) {
         event->accept();
    } else {
         event->ignore();
    }
}

QComboBox * SimBatchDialog::createIntComboBox(int * field, const int * values, int multiplier) {
    QComboBox * cb = new QComboBox();
    int defIndex = 0;
    for (int i = 0; values[i] != END_OF_LIST; i++) {
        int value = values[i];
        if (value*multiplier == *field)
            defIndex = i;
        cb->addItem(QString("%1").arg(value), value);
    }
    cb->setCurrentIndex(defIndex);
    connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [=](int index){ *field = cb->currentData().toInt() * multiplier;
        //recalculate();
    });
    return cb;
}

QComboBox * SimBatchDialog::createDoubleComboBox(double * field, const double * values) {
    QComboBox * cb = new QComboBox();
    int defIndex = 0;
    for (int i = 0; values[i] > END_OF_LIST; i++) {
        double value = values[i];
        if ((value >= *field - 0.0000000001) && (value <= *field + 0.0000000001))
            defIndex = i;
        cb->addItem(QString("%1").arg(value), value);
    }
    cb->setCurrentIndex(defIndex);
    connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [=](int index){ *field = cb->currentData().toDouble();
        //recalculate();
    });
    return cb;
}

void SimThread::onProgress(int currentStage, int totalStages, QString msg) {
    qDebug("SimThread::onProgress");
    emit updateProgress(currentStage, totalStages, msg);
}

void SimThread::runSimulation(SimParams * newParams) {
    qDebug("SimThread::runSimulation");
    SimParams params = *newParams;
    delete newParams;
    FullSimSuite suite(this);
    suite.setParams(&params);
    suite.run();
    SimResultsHolder * results = suite.cloneResults();
    qDebug("SimThread::runSimulation : emitting allTestsDone");
    emit allTestsDone(results);
}
