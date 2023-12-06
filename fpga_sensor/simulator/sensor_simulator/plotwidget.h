#ifndef PLOTWIDGET_H
#define PLOTWIDGET_H

#include "simutils.h"

#include <QWidget>

class PlotWidget : public QWidget
{
    Q_OBJECT

    SimParams * _simParams;
    SimState * _simState;
    int _pixelsPerSample;
public:
    explicit PlotWidget(SimParams * simParams, SimState * simState, QWidget *parent = nullptr);

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

    void setPixelsPerSample(int n);
    int getPixelsPerSample() { return _pixelsPerSample; }

protected:
    void paintEvent(QPaintEvent *event) override;

signals:

};

#endif // PLOTWIDGET_H
