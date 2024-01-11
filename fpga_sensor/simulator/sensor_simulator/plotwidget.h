#ifndef PLOTWIDGET_H
#define PLOTWIDGET_H

#include "simutils.h"

#include <QWidget>
#include <QScrollBar>

class PlotWidget : public QWidget
{
    Q_OBJECT

    SimParams * _simParams;
    SimState * _simState;
    int _pixelsPerSample;
    int _scrollX;
    QScrollBar * _scrollBar;
public slots:
    void zoomIn();
    void zoomOut();
public:
    explicit PlotWidget(SimParams * simParams, SimState * simState, QScrollBar * scrollBar, QWidget *parent = nullptr);

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

    void setPixelsPerSample(int n);
    int getPixelsPerSample() { return _pixelsPerSample; }
    void setScrollPercent(int n);
    int getScrollPercent();
protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent * event) override;

signals:

};

#endif // PLOTWIDGET_H
