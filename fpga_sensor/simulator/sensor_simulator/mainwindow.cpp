#include "mainwindow.h"
#include <QBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QGroupBox>
#include <QComboBox>
#include <QTextEdit>
#include <QLineEdit>
#include <QtMath>
#include <QDebug>
#include <QDoubleValidator>
#include <QScrollBar>

#include "simbatchdialog.h"

//#define RUN_SIMULATION_AFTER_PARAM_CHANGE

void MainWindow::recalculate() {
    qDebug("MainWindow::recalculate()");
    _simParams.recalculate();
    _simState.simulate(&_simParams);
    if (_plotWidget && _plotWidget->isVisible()) {
        _plotWidget->update();
    }

    // show calculated values in readonly editboxes
    QString txt;
    // sensed
    txt = QString("%1").arg(_simState.alignedSensePhaseShift, 0, 'f', 8);
    _measuredPhaseShift->setText(txt);
    txt = QString("%1").arg(_simState.alignedSensePhaseShiftDiff, 0, 'f', 8);
    _measuredPhaseShiftError->setText(txt);
    txt = QString("%1").arg(_simState.alignedSenseExactBits);
    _measuredPhaseExactBits->setText(txt);
    // real frequency recalculated
    txt = QString("%1").arg(_simParams.realFrequency, 0, 'f', 5);
    _realFrequency->setText(txt);

#ifdef RUN_SIMULATION_AFTER_PARAM_CHANGE
    qDebug("================================= PRECISION STATS =================");
    ExactBitStats stats;
    collectSimulationStats(&_simParams, 4, 20, 0.0012345, 20, 0.00156789, stats);
//    for (int i = 0; i < 24; i++) {
//            if (stats.exactBitsPercentLessOrEqual[i] > 0.0000001 && stats.exactBitsPercentMoreOrEqual[i] > 0.1)
//            qDebug("%2d exact bits: %7.3f   <= %7.3f   >= %7.3f", i, stats.exactBitsPercent[i], stats.exactBitsPercentLessOrEqual[i], stats.exactBitsPercentMoreOrEqual[i]);
//    }
    qDebug("Sim params: %s", _simParams.toString().toLocal8Bit().data());
    qDebug("Sim results 10..24: %s", stats.toString().toLocal8Bit().data());

    runSimTestSuite(&_simParams, 10);
#endif
    runBackgroundSim();
}

QComboBox * MainWindow::createComboBox(SimParameter param) {
    QComboBox * cb = new QComboBox();
    const SimParameterMetadata * metadata = SimParameterMetadata::get(param);
    int defIndex = metadata->getDefaultValueIndex();
    for (int i = 0; i < metadata->getValueCount(); i++) {
        cb->addItem(metadata->getValueLabel(i), metadata->getValue(i));
    }
    cb->setCurrentIndex(defIndex);
    connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [=](int index){ metadata->set(&_simParams, cb->currentData());
        recalculate();
    });
    return cb;
}

//QComboBox * MainWindow::createIntComboBox(int * field, const int * values, int multiplier) {
//    QComboBox * cb = new QComboBox();
//    int defIndex = 0;
//    for (int i = 0; values[i] != END_OF_LIST; i++) {
//        int value = values[i];
//        if (value*multiplier == *field)
//            defIndex = i;
//        cb->addItem(QString("%1").arg(value), value);
//    }
//    cb->setCurrentIndex(defIndex);
//    connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged),
//        [=](int index){ *field = cb->currentData().toInt() * multiplier;
//        recalculate();
//    });
//    return cb;
//}

QLineEdit * MainWindow::createReadOnlyEdit() {
    QLineEdit * edit = new QLineEdit();
    edit->setReadOnly(true);
    edit->setDisabled(true);
    return edit;
}

//QComboBox * MainWindow::createDoubleComboBox(double * field, const double * values) {
//    QComboBox * cb = new QComboBox();
//    int defIndex = 0;
//    for (int i = 0; values[i] > END_OF_LIST; i++) {
//        double value = values[i];
//        if ((value >= *field - 0.0000000001) && (value <= *field + 0.0000000001))
//            defIndex = i;
//        cb->addItem(QString("%1").arg(value), value);
//    }
//    cb->setCurrentIndex(defIndex);
//    connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged),
//        [=](int index){ *field = cb->currentData().toDouble();
//        recalculate();
//    });
//    return cb;
//}

QLineEdit * MainWindow::createDoubleValueEditor(double * field, double minValue, double maxValue, int precision) {
    QString text = QString("%1").arg(*field, 0, 'f', precision);
    QLineEdit * lineEdit = new QLineEdit(text);
    QDoubleValidator * validator = new QDoubleValidator(minValue, maxValue, precision);
    lineEdit->setValidator(validator);

    connect(lineEdit, QOverload<void>::of(&QLineEdit::returnPressed),
        [=]() {
        bool ok = false;
        QString txt = lineEdit->text();
        double newValue = txt.toDouble(&ok);
        if (!ok) {
            //lineEdit->set
            txt = QString("%1").arg(*field, 0, 'f', precision);
            recalculate();
        } else {
            *field = newValue;
            recalculate();
        }
    });


    return lineEdit;
}

void MainWindow::createControls() {

    QFormLayout * _globalParamsLayout = new QFormLayout();
    _globalParamsLayout->setSpacing(10);
    QLineEdit * _edFrequency = createDoubleValueEditor(&_simParams.frequency, 100000, 9000000, 2);
    //_edFrequency->setMax
    _globalParamsLayout->addRow(new QLabel("Signal freq Hz"), _edFrequency);

    //const int sampleRates[] = {200, 150, 125, 105, 100, 80, 65, 40, 25, 20, 10, 4, 3, END_OF_LIST};
    QComboBox * _cbSampleRate = createComboBox(SIM_PARAM_ADC_SAMPLE_RATE);
    _globalParamsLayout->addRow(new QLabel("Sample rate MHz"), _cbSampleRate);

    _realFrequency = new QLineEdit();
    _realFrequency->setReadOnly(true);
    _realFrequency->setDisabled(true);
    _globalParamsLayout->addRow(new QLabel("Real frequency"), _realFrequency);

    QGroupBox * _gbGlobal = new QGroupBox("Global");
    _gbGlobal->setLayout(_globalParamsLayout);
    _topLayout->addWidget(_gbGlobal);

    QFormLayout * _ncoParamsLayout = new QFormLayout();
    _ncoParamsLayout->setSpacing(10);

    //const int phaseBits[] = {24, 26, 28, 30, 32, 34, 36, END_OF_LIST};
    _ncoParamsLayout->addRow(new QLabel("Phase bits"), createComboBox(SIM_PARAM_PHASE_BITS));

    //const int ncoValueBits[] = {8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, END_OF_LIST};
    _ncoParamsLayout->addRow(new QLabel("Value bits"), createComboBox(SIM_PARAM_SIN_VALUE_BITS));

    //const int sinTableBits[] = {7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, END_OF_LIST};
    _ncoParamsLayout->addRow(new QLabel("Sin table size"), createComboBox(SIM_PARAM_SIN_TABLE_SIZE_BITS));

    QGroupBox * _gbNco = new QGroupBox("NCO");
    _gbNco->setLayout(_ncoParamsLayout);

    _topLayout->addWidget(_gbNco);

    QFormLayout * _adcParamsLayout = new QFormLayout();
    _adcParamsLayout->setSpacing(10);

    //const int adcValueBits[] = {6, 7, 8, 9, 10, 11, 12, 13, 14, 16, END_OF_LIST};
    _adcParamsLayout->addRow(new QLabel("ADC bits"), createComboBox(SIM_PARAM_ADC_BITS));

    //const int adcInterpolationRate[] = {1, 2, 3, 4, END_OF_LIST};
    _adcParamsLayout->addRow(new QLabel("ADC interpolation"), createComboBox(SIM_PARAM_ADC_INTERPOLATION));

    //const double adcNoise[] = {0.0, 0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, END_OF_LIST};
    _adcParamsLayout->addRow(new QLabel("Noise, LSB"), createComboBox(SIM_PARAM_SENSE_NOISE));

    //const double adcDCOffset[] = {-10.0, -5.0, -2.5, -1.0, -0.5, 0, 0.5, 1.0, 2.5, 5.0, 10.0, END_OF_LIST};
    _adcParamsLayout->addRow(new QLabel("DC offset, LSB"), createComboBox(SIM_PARAM_SENSE_DC_OFFSET));

    QGroupBox * _gbAdc = new QGroupBox("ADC");
    _gbAdc->setLayout(_adcParamsLayout);
    _topLayout->addWidget(_gbAdc);



    // Sense
    QFormLayout * _senseParamsLayout = new QFormLayout();
    _senseParamsLayout->setSpacing(10);

    //const double senseAmplitude[] = {0.1, 0.25, 0.5, 0.8, 0.9, 0.95, 1.0, END_OF_LIST};
    _senseParamsLayout->addRow(new QLabel("Amplitude"), createComboBox(SIM_PARAM_SENSE_AMPLITUDE));

    //const int adcAveragingPeriods[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, END_OF_LIST};
    _senseParamsLayout->addRow(new QLabel("Avg periods"), createComboBox(SIM_PARAM_AVG_PERIODS));

    //const int edgeAccInterpolation[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, END_OF_LIST};
    _senseParamsLayout->addRow(new QLabel("Edge interp"), createComboBox(SIM_PARAM_ADC_INTERPOLATION));

    QLineEdit * _edSensePhaseShift = createDoubleValueEditor(&_simParams.sensePhaseShift, -1.0, 1.0, 6);
    //_edFrequency->setMax
    _senseParamsLayout->addRow(new QLabel("Phase shift 0..1"), _edSensePhaseShift);

    QGroupBox * _gbSense = new QGroupBox("Sense");
    _gbSense->setLayout(_senseParamsLayout);
    _topLayout->addWidget(_gbSense);

    // measured values display
    QFormLayout * _measuredLayout = new QFormLayout();
    _measuredLayout->setSpacing(10);
    _measuredPhaseShift = createReadOnlyEdit();
    _measuredPhaseShiftError = createReadOnlyEdit();
    _measuredPhaseExactBits = createReadOnlyEdit();
    _measuredLayout->addRow(new QLabel("Measured phase"), _measuredPhaseShift);
    _measuredLayout->addRow(new QLabel("Measured error"), _measuredPhaseShiftError);
    _measuredLayout->addRow(new QLabel("Exact phase bits"), _measuredPhaseExactBits);

    QGroupBox * _gbMeasured = new QGroupBox("Measured");
    _gbMeasured->setLayout(_measuredLayout);
    _rightLayout->addWidget(_gbMeasured);

    QVBoxLayout * resultLayout = new QVBoxLayout();
    resultLayout->setSpacing(15);
    _plot = new SimResultPlot();
    _cbResult = new QComboBox();
    for (int i = SIM_PARAM_MIN; i <= SIM_PARAM_MAX; i++) {
        const SimParameterMetadata * metadata = SimParameterMetadata::get((SimParameter)i);
        _cbResult->addItem(metadata->getName(), QVariant(i));
    }
    _cbResult->setCurrentIndex(0);
    connect(_cbResult, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::plotIndexChanged);
    resultLayout->addWidget(_cbResult);
    resultLayout->addWidget(_plot);
    _rightLayout->addItem(resultLayout);

    _topLayout->addStretch(1);
}

void MainWindow::plotIndexChanged(int index) {
    updatePlot();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (!_running) {
         event->accept();
    } else {
         event->ignore();
    }
}

void MainWindow::runSimBatch() {
    SimBatchDialog * dialog = new SimBatchDialog(&_simParams, this);
    dialog->show();
    dialog->setModal(true);
    dialog->raise();
    dialog->activateWindow();

}

void MainWindow::createMenu() {
    _menuBar = new QMenuBar(this);
    QMenu * fileMenu = _menuBar->addMenu("&File");
    QMenu * viewMenu = _menuBar->addMenu("&View");
    QMenu * toolsMenu = _menuBar->addMenu("Tools");
    QAction * _actionExit = new QAction("Exit", this);
    _actionExit->setShortcuts(QKeySequence::Close);
    _actionToolsRunSimulationBatch = new QAction("Run simulation", this);
    connect(_actionToolsRunSimulationBatch, &QAction::triggered, this, &MainWindow::runSimBatch);
    QAction * _actionViewHZoomIn = new QAction("H Zoom In", this);
    _actionViewHZoomIn->setShortcut(QKeySequence::ZoomIn);
    connect(_actionViewHZoomIn, &QAction::triggered, _plotWidget, &PlotWidget::zoomIn);
    QAction * _actionViewHZoomOut = new QAction("H Zoom Out", this);
    _actionViewHZoomOut->setShortcut(QKeySequence::ZoomOut);
    connect(_actionViewHZoomOut, &QAction::triggered, _plotWidget, &PlotWidget::zoomOut);
    fileMenu->addAction(_actionExit);
    toolsMenu->addAction(_actionToolsRunSimulationBatch);
    viewMenu->addAction(_actionViewHZoomIn);
    viewMenu->addAction(_actionViewHZoomOut);
}

void MainWindow::updatePlot() {
    if (_simResults) {
        int index = _cbResult->currentIndex();
        _plot->setSimResults(&_simResults->byTest[index]);
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), _running(false), _simResults(nullptr)
{
    SimParameterMetadata::applyDefaults(&_simParams);
    _simParams.freqVariations = 3;
    _simParams.phaseVariations = 3;
    _simParams.freqStep = 0.00142466;
    _simParams.phaseStep = 0.0065441;
    _simParams.bitFractionCount = 2;

    _layoutSpacing = 10;

    setWindowTitle("Theremin Sensor Simulator v0.3");
    QWidget * _mainWidget = new QWidget();
    QVBoxLayout * _mainLayout = new QVBoxLayout();
    _mainLayout->setSpacing(_layoutSpacing);

    _topLayout = new QHBoxLayout();
    _topLayout->setSpacing(_layoutSpacing);
    _bottomLayout = new QVBoxLayout();
    _bottomLayout->setSpacing(_layoutSpacing);

    QHBoxLayout * _leftRightLayout = new QHBoxLayout();
    _leftRightLayout->setSpacing(_layoutSpacing*2);
    _leftLayout = new QVBoxLayout();
    _leftLayout->setSpacing(_layoutSpacing*2);
    _rightLayout = new QVBoxLayout();
    _rightLayout->setSpacing(_layoutSpacing*2);
    _leftRightLayout->addItem(_leftLayout);
    _leftRightLayout->addItem(_rightLayout);
    _leftLayout->addItem(_topLayout);
    _leftLayout->addItem(_bottomLayout);
    _mainLayout->addItem(_leftRightLayout);


    createControls();

    QScrollBar * _scrollBar = new QScrollBar(Qt::Orientation::Horizontal);
    _plotWidget = new PlotWidget(&_simParams, &_simState, _scrollBar);

    _bottomLayout->addWidget(_plotWidget, 1);

    _scrollBar->setRange(0, 100);
    _bottomLayout->addWidget(_scrollBar);





    _mainWidget->setLayout(_mainLayout);
    setCentralWidget(_mainWidget);

    createMenu();
    setMenuBar(_menuBar);

    _simParams.recalculate();
    //_simState.simulate(&_simParams);

    _simThread = new SimThread();
    _simThread->moveToThread(&_workerThread);
    connect(&_workerThread, &QThread::finished, _simThread, &QObject::deleteLater);
    connect(this, &MainWindow::runSimulation, _simThread, &SimThread::runSimulation);
    connect(_simThread, &SimThread::allTestsDone, this, &MainWindow::allTestsDone);
    _workerThread.start();

    recalculate();
}

void MainWindow::runBackgroundSim() {
    qDebug("MainWindow::runBackgroundSim()");
    if (_running)
        return;
    SimParams * p = new SimParams();
    *p = _simParams;
    _running = true;
    emit runSimulation(p);
}


void MainWindow::allTestsDone(SimResultsHolder * allResults) {
    qDebug("MainWindow::allTestsDone()");
    _running = false;
    if (_simResults)
        delete _simResults;
    _simResults = allResults;
    updatePlot();
}


MainWindow::~MainWindow()
{
    qDebug("MainWindow::~MainWindow()");
    _workerThread.quit();
    _workerThread.wait();
    if (_simResults)
        delete _simResults;
}

