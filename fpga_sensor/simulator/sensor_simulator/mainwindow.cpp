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

void MainWindow::recalculate() {
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

    qDebug("================================= PRECISION STATS =================");
    ExactBitStats stats;
    collectSimulationStats(&_simParams, 4, 20, 0.001, 20, 0.001, stats);
    for (int i = 0; i < 24; i++) {
        if (stats.exactBitsPercentLessOrEqual[i] > 0.0000001 && stats.exactBitsPercentMoreOrEqual[i] > 0.1)
            qDebug("%2d exact bits: %7.3f   <= %7.3f   >= %7.3f", i, stats.exactBitsPercent[i], stats.exactBitsPercentLessOrEqual[i], stats.exactBitsPercentMoreOrEqual[i]);
    }
}

QComboBox * MainWindow::createIntComboBox(int * field, const int * values) {
    QComboBox * cb = new QComboBox();
    int defIndex = 0;
    for (int i = 0; values[i] > -1000000; i++) {
        int value = values[i];
        if (value == *field)
            defIndex = i;
        cb->addItem(QString("%1").arg(value), value);
    }
    cb->setCurrentIndex(defIndex);
    connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [=](int index){ *field = cb->currentData().toInt();
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

QComboBox * MainWindow::createDoubleComboBox(double * field, const double * values) {
    QComboBox * cb = new QComboBox();
    int defIndex = 0;
    for (int i = 0; values[i] > -1000000; i++) {
        double value = values[i];
        if ((value >= *field - 0.0000000001) && (value <= *field + 0.0000000001))
            defIndex = i;
        cb->addItem(QString("%1").arg(value), value);
    }
    cb->setCurrentIndex(defIndex);
    connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [=](int index){ *field = cb->currentData().toDouble();
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

//    connect(lineEdit, QOverload<void>::of(&QLineEdit::textEdited),
//        [=](const QString &text) {
//        bool ok = false;
//        double newValue = text.toDouble(&ok);
//        if (!ok || newValue < minValue || newValue > maxValue) {
//            //lineEdit->set
//        } else {

//        }
//        _simParams.sampleRate = _cbSampleRate->currentData().toInt()*1000000;
//        recalculate();
//    });

    return lineEdit;
}

void MainWindow::createControls() {

    QFormLayout * _globalParamsLayout = new QFormLayout();
    _globalParamsLayout->setSpacing(10);
    QLineEdit * _edFrequency = createDoubleValueEditor(&_simParams.frequency, 100000, 4000000, 2);
    //_edFrequency->setMax
    _globalParamsLayout->addRow(new QLabel("Signal freq Hz"), _edFrequency);

    const int sampleRates[] = {200000000, 150000000, 125000000, 105000000, 100000000, 80000000, 65000000, 40000000, 20000000, 25000000, -1000000};
    QComboBox * _cbSampleRate = createIntComboBox(&_simParams.sampleRate, sampleRates);
    _globalParamsLayout->addRow(new QLabel("Sample rate Hz"), _cbSampleRate);

    _realFrequency = new QLineEdit();
    _realFrequency->setReadOnly(true);
    _realFrequency->setDisabled(true);
    _globalParamsLayout->addRow(new QLabel("Real frequency"), _realFrequency);

    QGroupBox * _gbGlobal = new QGroupBox("Global");
    _gbGlobal->setLayout(_globalParamsLayout);
    _topLayout->addWidget(_gbGlobal);

    QFormLayout * _ncoParamsLayout = new QFormLayout();
    _ncoParamsLayout->setSpacing(10);

    const int phaseBits[] = {24, 26, 28, 30, 32, 34, 36, -1000000};
    _ncoParamsLayout->addRow(new QLabel("Phase bits"), createIntComboBox(&_simParams.ncoPhaseBits, phaseBits));

    const int ncoValueBits[] = {8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, -1000000};
    _ncoParamsLayout->addRow(new QLabel("Value bits"), createIntComboBox(&_simParams.ncoValueBits, ncoValueBits));

    const int sinTableBits[] = {7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, -1000000};
    _ncoParamsLayout->addRow(new QLabel("Sin table size"), createIntComboBox(&_simParams.ncoSinTableSizeBits, sinTableBits));

    QGroupBox * _gbNco = new QGroupBox("NCO");
    _gbNco->setLayout(_ncoParamsLayout);

    _topLayout->addWidget(_gbNco);

    QFormLayout * _adcParamsLayout = new QFormLayout();
    _adcParamsLayout->setSpacing(10);

    const int adcValueBits[] = {6, 7, 8, 9, 10, 11, 12, 13, 14, 16, -1000000};
    _adcParamsLayout->addRow(new QLabel("ADC bits"), createIntComboBox(&_simParams.adcBits, adcValueBits));

    const double adcNoise[] = {0.0, 0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, -1000000};
    _adcParamsLayout->addRow(new QLabel("Noise, LSB"), createDoubleComboBox(&_simParams.adcNoise, adcNoise));

    const double adcDCOffset[] = {-10.0, -5.0, -2.5, -1.0, -0.5, 0, 0.5, 1.0, 2.5, 5.0, 10.0, -1000000};
    _adcParamsLayout->addRow(new QLabel("DC offset, LSB"), createDoubleComboBox(&_simParams.adcDCOffset, adcDCOffset));

    QGroupBox * _gbAdc = new QGroupBox("ADC");
    _gbAdc->setLayout(_adcParamsLayout);
    _topLayout->addWidget(_gbAdc);



    // Sense
    QFormLayout * _senseParamsLayout = new QFormLayout();
    _senseParamsLayout->setSpacing(10);

    const double senseAmplitude[] = {0.1, 0.25, 0.5, 0.8, 0.9, 0.95, -1000000};
    _senseParamsLayout->addRow(new QLabel("Amplitude"), createDoubleComboBox(&_simParams.senseAmplitude, senseAmplitude));

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
    _topLayout->addWidget(_gbMeasured);

    _topLayout->addStretch(1);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Theremin Sensor Simulator v0.2");
    QWidget * _mainWidget = new QWidget();
    QLayout * _mainLayout = new QBoxLayout(QBoxLayout::Direction::TopToBottom);
    _topLayout = new QBoxLayout(QBoxLayout::Direction::LeftToRight);
    _topLayout->setSpacing(10);
    _bottomLayout = new QBoxLayout(QBoxLayout::Direction::TopToBottom);
    _bottomLayout->setSpacing(10);

    createControls();

    QScrollBar * _scrollBar = new QScrollBar(Qt::Orientation::Horizontal);
    _plotWidget = new PlotWidget(&_simParams, &_simState, _scrollBar);

    _bottomLayout->addWidget(_plotWidget, 1);

    _scrollBar->setRange(0, 100);
    _bottomLayout->addWidget(_scrollBar);

    _mainLayout->addItem(_topLayout);
    _mainLayout->addItem(_bottomLayout);

    _mainWidget->setLayout(_mainLayout);
    setCentralWidget(_mainWidget);

    _simParams.recalculate();
    _simState.simulate(&_simParams);

    recalculate();
}

MainWindow::~MainWindow()
{
}

