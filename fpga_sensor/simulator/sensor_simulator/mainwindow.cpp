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

void MainWindow::recalculate() {
    _simParams.recalculate();
    _simState.simulate(&_simParams);
    if (_plotWidget && _plotWidget->isVisible()) {
        _plotWidget->update();
    }
}

void MainWindow::createControls() {

    QFormLayout * _globalParamsLayout = new QFormLayout();
    _globalParamsLayout->setSpacing(10);
    QLineEdit * _edFrequency = new QLineEdit("1012356");
    //_edFrequency->setMax
    _globalParamsLayout->addRow(new QLabel("Signal freq Hz"), _edFrequency);

    QComboBox * _cbSampleRate = new QComboBox();
    _cbSampleRate->addItem("100", 100);
    _cbSampleRate->addItem("90",  90);
    _cbSampleRate->addItem("80",  80);
    _cbSampleRate->addItem("60",  60);
    _cbSampleRate->addItem("50",  50);
    _cbSampleRate->addItem("30",  30);
    _cbSampleRate->addItem("25",  25);
    _cbSampleRate->setCurrentIndex(0);
    _globalParamsLayout->addRow(new QLabel("Sample rate MHz"), _cbSampleRate);
    connect(_cbSampleRate, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [=](int index){ _simParams.sampleRate = _cbSampleRate->currentData().toInt()*1000000;
        recalculate();
    });


    QGroupBox * _gbGlobal = new QGroupBox("Global");
    _gbGlobal->setLayout(_globalParamsLayout);
    _topLayout->addWidget(_gbGlobal);

    QFormLayout * _ncoParamsLayout = new QFormLayout();
    _ncoParamsLayout->setSpacing(10);

    _cbNcoPhaseBits = new QComboBox();
    _cbNcoPhaseBits->addItem("24", 24);
    _cbNcoPhaseBits->addItem("26", 26);
    _cbNcoPhaseBits->addItem("28", 28);
    _cbNcoPhaseBits->addItem("30", 30);
    _cbNcoPhaseBits->addItem("32", 32);
    _cbNcoPhaseBits->setCurrentIndex(4);
    connect(_cbNcoPhaseBits, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [=](int index){ _simParams.ncoPhaseBits = _cbNcoPhaseBits->currentData().toInt();
        recalculate();
    });
    _ncoParamsLayout->addRow(new QLabel("Phase bits"), _cbNcoPhaseBits);

    _cbNcoValueBits = new QComboBox();
    _cbNcoValueBits->addItem("8", 8);
    _cbNcoValueBits->addItem("9", 9);
    _cbNcoValueBits->addItem("10", 10);
    _cbNcoValueBits->addItem("11", 11);
    _cbNcoValueBits->addItem("12", 12);
    _cbNcoValueBits->setCurrentIndex(0);
    _ncoParamsLayout->addRow(new QLabel("Value bits"), _cbNcoValueBits);
    connect(_cbNcoValueBits, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [=](int index){ _simParams.ncoValueBits = _cbNcoValueBits->currentData().toInt();
        recalculate();
    });

    QGroupBox * _gbNco = new QGroupBox("NCO");
    _gbNco->setLayout(_ncoParamsLayout);

    _topLayout->addWidget(_gbNco);

    QFormLayout * _adcParamsLayout = new QFormLayout();
    _adcParamsLayout->setSpacing(10);

    _cbAdcValueBits = new QComboBox();
    _cbAdcValueBits->addItem("6", 6);
    _cbAdcValueBits->addItem("7", 7);
    _cbAdcValueBits->addItem("8", 8);
    _cbAdcValueBits->addItem("9", 9);
    _cbAdcValueBits->addItem("10", 10);
    _cbAdcValueBits->addItem("11", 11);
    _cbAdcValueBits->addItem("12", 12);
    _cbAdcValueBits->setCurrentIndex(2);
    _adcParamsLayout->addRow(new QLabel("ADC bits"), _cbAdcValueBits);
    connect(_cbAdcValueBits, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [=](int index){ _simParams.adcBits = _cbAdcValueBits->currentData().toInt();
        recalculate();
    });

    _cbAdcNoise = new QComboBox();
    _cbAdcNoise->addItem("0.0", 0.0);
    _cbAdcNoise->addItem("0.1", 0.1);
    _cbAdcNoise->addItem("0.5", 0.5);
    _cbAdcNoise->addItem("1.0", 1.0);
    _cbAdcNoise->addItem("2.0", 2.0);
    _cbAdcNoise->addItem("5.0", 5.0);
    _cbAdcNoise->addItem("10.0", 10.0);
    _cbAdcNoise->setCurrentIndex(0);
    _adcParamsLayout->addRow(new QLabel("Noise, LSB"), _cbAdcNoise);
    connect(_cbAdcNoise, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [=](int index){ _simParams.adcNoise = _cbAdcNoise->currentData().toDouble();
        recalculate();
    });


    QGroupBox * _gbAdc = new QGroupBox("ADC");
    _gbAdc->setLayout(_adcParamsLayout);
    _topLayout->addWidget(_gbAdc);



    // Sense
    QFormLayout * _senseParamsLayout = new QFormLayout();
    _senseParamsLayout->setSpacing(10);

    _cbAdcAmplitude = new QComboBox();
    _cbAdcAmplitude->addItem("0.95", 0.95);
    _cbAdcAmplitude->addItem("0.9", 0.9);
    _cbAdcAmplitude->addItem("0.8", 0.8);
    _cbAdcAmplitude->addItem("0.7", 0.7);
    _cbAdcAmplitude->addItem("0.5", 0.5);
    _cbAdcAmplitude->addItem("0.3", 0.3);
    _cbAdcAmplitude->addItem("0.1", 0.1);
    _cbAdcAmplitude->setCurrentIndex(2);
    _senseParamsLayout->addRow(new QLabel("Amplitude"), _cbAdcAmplitude);
    connect(_cbAdcAmplitude, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [=](int index){ _simParams.senseAmplitude = _cbAdcAmplitude->currentData().toDouble();
        recalculate();
    });

    QLineEdit * _edSensePhaseShift = new QLineEdit("-0.0765");
    //_edFrequency->setMax
    _senseParamsLayout->addRow(new QLabel("Phase shift 0..1"), _edSensePhaseShift);

    QGroupBox * _gbSense = new QGroupBox("Sense");
    _gbSense->setLayout(_senseParamsLayout);
    _topLayout->addWidget(_gbSense);

    _topLayout->addStretch(1);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Theremin Sensor Simulator v0.1");
    QWidget * _mainWidget = new QWidget();
    QLayout * _mainLayout = new QBoxLayout(QBoxLayout::Direction::TopToBottom);
    _topLayout = new QBoxLayout(QBoxLayout::Direction::LeftToRight);
    _topLayout->setSpacing(10);
    _bottomLayout = new QBoxLayout(QBoxLayout::Direction::TopToBottom);
    _bottomLayout->setSpacing(10);

    createControls();

    _plotWidget = new PlotWidget(&_simParams, &_simState);

    _bottomLayout->addWidget(_plotWidget);
    //_bottomLayout->addWidget(new QLabel("Body"));

    _mainLayout->addItem(_topLayout);
    _mainLayout->addItem(_bottomLayout);

    _mainWidget->setLayout(_mainLayout);
    setCentralWidget(_mainWidget);

    _simParams.recalculate();
    _simState.simulate(&_simParams);

//    setCentralWidget(new QLabel("Test label - very very long label"));
}

MainWindow::~MainWindow()
{
}

