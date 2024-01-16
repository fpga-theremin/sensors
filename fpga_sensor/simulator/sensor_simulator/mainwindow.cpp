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

    //_lpFilterState->setText(_simState.lpFilterEnabled ? "Enabled" : "Disabled");
    //_movingAvgFilterState->setText(_simState.movingAvgEnabled ? "Enabled" : "Disabled");
    _lpFilterLatency->setText(_simState.lpFilterEnabled ? (QString("%1us").arg(_simState.getLpFilterLatency(), 0, 'g', 4)) : "0us");
    _movingAvgFilterLatency->setText(_simState.movingAvgEnabled ? (QString("%1us").arg(_simState.getMovingAverageLatency(), 0, 'g', 4)) : "0us");

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

QLineEdit * MainWindow::createReadOnlyEdit() {
    QLineEdit * edit = new QLineEdit();
    edit->setReadOnly(true);
    edit->setDisabled(true);
    return edit;
}

QCheckBox * MainWindow::createCheckBox(QString label, int * field) {
    QCheckBox * cb = new QCheckBox(label);
    cb->setChecked(*field);
    connect(cb, QOverload<int>::of(&QCheckBox::stateChanged),
        [=](int state){ *field = (state == Qt::Checked) ? 1 : 0;
        recalculate();
    });
    return cb;
}

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

    _adcParamsLayout->addRow(new QLabel("ADC bits"), createComboBox(SIM_PARAM_ADC_BITS));
    _adcParamsLayout->addRow(new QLabel("ADC interpolation"), createComboBox(SIM_PARAM_ADC_INTERPOLATION));
    _adcParamsLayout->addRow(new QLabel("Noise, LSB"), createComboBox(SIM_PARAM_SENSE_NOISE));
    _adcParamsLayout->addRow(new QLabel("DC offset, LSB"), createComboBox(SIM_PARAM_SENSE_DC_OFFSET));



    // LP Filter
    QFormLayout * _lpFilterParamsLayout = new QFormLayout();
    _lpFilterParamsLayout->setSpacing(10);

    //_lpFilterState = new QLabel("unknown");
    _lpFilterLatency = new QLabel("unknown");

    _lpFilterParamsLayout->addRow(new QLabel(""), createCheckBox("Enabled", &_simParams.lpFilterEnabled));
    _lpFilterParamsLayout->addRow(new QLabel("LP Filt Shift"), createComboBox(SIM_PARAM_LP_FILTER_SHIFT_BITS));
    _lpFilterParamsLayout->addRow(new QLabel("LP Filt Stages"), createComboBox(SIM_PARAM_LP_FILTER_STAGES));
    _lpFilterParamsLayout->addRow(new QLabel("Latency"), _lpFilterLatency);

    // Moving Avg Filter
    QFormLayout * _movingAvgFilterParamsLayout = new QFormLayout();
    _movingAvgFilterParamsLayout->setSpacing(10);

    //_movingAvgFilterState = new QLabel("unknown");
    _movingAvgFilterLatency = new QLabel("unknown");

    _movingAvgFilterParamsLayout->addRow(new QLabel(""), createCheckBox("Enabled", &_simParams.movingAverageFilterEnabled));
    _movingAvgFilterParamsLayout->addRow(new QLabel("Avg periods"), createComboBox(SIM_PARAM_AVG_PERIODS));
    _movingAvgFilterParamsLayout->addRow(new QLabel("Acc drop bits"), createComboBox(SIM_PARAM_ACC_DROP_BITS));
    _movingAvgFilterParamsLayout->addRow(new QLabel("Edge interp"), createComboBox(SIM_PARAM_EDGE_SUBSAMPLING_BITS));
    _movingAvgFilterParamsLayout->addRow(new QLabel("Latency"), _movingAvgFilterLatency);

    // Sense
    QFormLayout * _senseParamsLayout = new QFormLayout();
    _senseParamsLayout->setSpacing(10);

    _senseParamsLayout->addRow(new QLabel("Amplitude"), createComboBox(SIM_PARAM_SENSE_AMPLITUDE));

    _senseParamsLayout->addRow(new QLabel("Mul drop bits"), createComboBox(SIM_PARAM_MUL_DROP_BITS));



    QLineEdit * _edSensePhaseShift = createDoubleValueEditor(&_simParams.sensePhaseShift, -1.0, 1.0, 6);
    //_edFrequency->setMax
    // moved to global
    _globalParamsLayout->addRow(new QLabel("Phase shift 0..1"), _edSensePhaseShift);

    QGroupBox * _gbAdc = new QGroupBox("ADC");
    _gbAdc->setLayout(_adcParamsLayout);
    _topLayout->addWidget(_gbAdc);

    QGroupBox * _gbSense = new QGroupBox("Sense");
   _gbSense->setLayout(_senseParamsLayout);
   _topLayout->addWidget(_gbSense);

    QGroupBox * _gbMovingAvgFilter = new QGroupBox("Moving Avg");
    _gbMovingAvgFilter->setLayout(_movingAvgFilterParamsLayout);
    _topLayout->addWidget(_gbMovingAvgFilter);

    QGroupBox * _gbLpFilter = new QGroupBox("LP Filter");
    _gbLpFilter->setLayout(_lpFilterParamsLayout);
    _topLayout->addWidget(_gbLpFilter);

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
    _plot2 = new SimResultPlot();
    _cbResult = new QComboBox();
    _cbResult2 = new QComboBox();
    for (int i = SIM_PARAM_MIN; i <= SIM_PARAM_MAX; i++) {
        const SimParameterMetadata * metadata = SimParameterMetadata::get((SimParameter)i);
        _cbResult->addItem(metadata->getName(), QVariant(i));
        _cbResult2->addItem(metadata->getName(), QVariant(i));
    }
    _cbResult->setCurrentIndex(SIM_PARAM_AVG_PERIODS);
    _cbResult2->setCurrentIndex(SIM_PARAM_EDGE_SUBSAMPLING_BITS);
    connect(_cbResult, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::plotIndexChanged);
    connect(_cbResult2, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::plotIndexChanged2);
    resultLayout->addWidget(_cbResult);
    resultLayout->addWidget(_plot);
    resultLayout->addWidget(_cbResult2);
    resultLayout->addWidget(_plot2);
    _rightLayout->addItem(resultLayout);

    _topLayout->addStretch(1);
}

void MainWindow::plotIndexChanged(int index) {
    runBackgroundSim();
    //updatePlot();
}

void MainWindow::plotIndexChanged2(int index) {
    runBackgroundSim();
    //updatePlot();
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
        SimParameter type = (SimParameter)_cbResult->currentData().toInt();
        SimResultsItem * result1 = _simResults->byType(type);
        if (result1)
            _plot->setSimResults(result1);
        SimParameter type2 = (SimParameter)_cbResult2->currentData().toInt();
        SimResultsItem * result2 = _simResults->byType(type2);
        if (result2)
            _plot2->setSimResults(result2);
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), _running(false), _simRestartAfterResults(false), _simResults(nullptr)
{
    SimParameterMetadata::applyDefaults(&_simParams);
    _simParams.freqVariations = 3;
    _simParams.phaseVariations = 3;
    _simParams.freqStep = M_PI/1000; //0.000142466432;
    _simParams.phaseStep = M_PI / 345; //0.0116544187634;
    _simParams.bitFractionCount = 3;

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
    if (_running) {
        _simRestartAfterResults = true;
        return;
    }
    SimParams * p = new SimParams();
    *p = _simParams;
    SimParameter type1 = (SimParameter)_cbResult->currentData().toInt();
    SimParameter type2 = (SimParameter)_cbResult2->currentData().toInt();
    p->paramTypeMask = (1<<type1) | (1<<type2);
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
    if (_simRestartAfterResults) {
        _simRestartAfterResults = false;
        runBackgroundSim();
    }
}


MainWindow::~MainWindow()
{
    qDebug("MainWindow::~MainWindow()");
    _workerThread.quit();
    _workerThread.wait();
    if (_simResults)
        delete _simResults;
}

