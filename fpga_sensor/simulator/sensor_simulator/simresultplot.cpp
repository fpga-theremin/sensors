#include "simresultplot.h"
#include <QPainter>

SimResultPlot::SimResultPlot(QWidget *parent) : QWidget(parent)
{
    initGraphics();
}

QSize SimResultPlot::sizeHint() const
{
    return QSize(400, 250);
}

QSize SimResultPlot::minimumSizeHint() const
{
    return QSize(350, 180);
}


void SimResultPlot::setSimResults(SimResultsItem * results) {
    _results = *results;
    update();
}

void SimResultPlot::initGraphics() {
    int penWidth = 2;
    Qt::PenStyle style = Qt::PenStyle(Qt::SolidLine);
    Qt::PenCapStyle cap = Qt::PenCapStyle(Qt::RoundCap);
    Qt::PenJoinStyle join = Qt::PenJoinStyle(Qt::RoundJoin);
    _pens[0] = QPen(QColor(128, 192, 64), penWidth, style, cap, join);
    _pens[1] = QPen(QColor(192, 64, 128), penWidth, style, cap, join);
    _pens[2] = QPen(QColor(64,  64, 192), penWidth, style, cap, join);
    _pens[3] = QPen(QColor(128, 200, 64), penWidth, style, cap, join);
    _pens[4] = QPen(QColor(64,  192, 128), penWidth, style, cap, join);
    _pens[5] = QPen(QColor(128, 192, 64), penWidth, style, cap, join);
    _pens[6] = QPen(QColor(128, 192, 64), penWidth, style, cap, join);
    _pens[7] = QPen(QColor(128, 192, 64), penWidth, style, cap, join);
    _pens[8] = QPen(QColor(128, 192, 64), penWidth, style, cap, join);
    _pens[9] = QPen(QColor(128, 192, 64), penWidth, style, cap, join);
    _pens[10] = QPen(QColor(128, 192, 64), penWidth, style, cap, join);
    _pens[11] = QPen(QColor(128, 192, 64), penWidth, style, cap, join);
    _pens[12] = QPen(QColor(128, 192, 64), penWidth, style, cap, join);
    _pens[13] = QPen(QColor(128, 192, 64), penWidth, style, cap, join);
    _pens[14] = QPen(QColor(128, 192, 64), penWidth, style, cap, join);
    _pens[15] = QPen(QColor(128, 192, 64), penWidth, style, cap, join);
}

void SimResultPlot::paintEvent(QPaintEvent * /* event */)
{
    int w = width();
    int h = height();
    QPainter painter(this);
    QBrush brush(QColor(255, 255, 250));
    QBrush brushLegend(QColor(240, 240, 237));
    QBrush brushHaxis(QColor(212, 212, 212));
    QBrush brushVaxis(QColor(224, 224, 224));
    QBrush brushVaxisEven(QColor(200, 212, 200));
    painter.setBrush(brush);
    painter.fillRect(QRect(0, 0, w, h), brush);
    int fh = painter.fontMetrics().height();
    int legendh = fh * 120/100;
    int legendy = h - legendh;
    int haxish = fh * 130/100;
    int haxisy = h - legendh - haxish;
    painter.fillRect(QRect(0, legendy, w, legendh), brushLegend);

    painter.fillRect(QRect(0, haxisy, w, 2), brushHaxis);
    double maxPercent = 36.0;
    double yscalef = (haxisy / maxPercent);
    int yscale = (int)yscalef;
    for (int k = 10; k < maxPercent; k += 10) {
        int y = haxisy - k*yscale;
        if (y > 0)
            painter.fillRect(QRect(0, y, w, 1), brushHaxis);
    }

    int minbits = 8;
    int maxbits = 24;
    int fracbits = (_results.byParameter.length() == 0) ? 2 : _results.byParameter[0].bitStats.bitFractionCount();
    int bitstep = w / (maxbits - minbits);
    int xwidth = bitstep * (maxbits - minbits);
    int minbitsx = (w - xwidth) / 2;
    int lastLabelX = -fh/4;
    for (int i = minbits; i <= maxbits; i++) {
        int x = minbitsx + (i-minbits) * bitstep;
        painter.fillRect(QRect(x, 0, 1, haxisy), (i & 1) ? brushVaxis : brushVaxisEven);
        if ((i & 1) == 0) {
            QString label = QString("%1").arg(i);
            int labelw = painter.fontMetrics().horizontalAdvance(label);
            if (lastLabelX < x - labelw/2 - fh/2 && x + labelw/2 + fh/4 < w) {
                painter.drawText(QPoint(x - labelw/2, haxisy + fh * 9/10), label);
            }
        }
    }

    if (_results.name.length()>0)
        painter.drawText(QPoint(5, fh), _results.name);
    painter.drawText(QPoint(5, legendy+legendh*9/10), "Legend:");

    if (_results.byParameter.length() == 0)
        return; // nothing to draw


    for (int i = 0; i < _results.byParameter.length(); i++) {
        //
        painter.setPen(_pens[i&15]);
        int pointCount = 0;
        QPoint exactPoints[32*5];
        for (int j = minbits*fracbits; j <= maxbits*fracbits; j++) {
            int x = minbitsx + (j - minbits*fracbits) * bitstep;
            ExactBitStats * stats = &_results.byParameter[i].bitStats;
            double v = stats->getPercent(j, minbits, maxbits);

            if (v > 0.0000000001 || pointCount > 0) {
                int y = haxisy + (int)(v*yscale);
                QPoint pt = QPoint(x, haxisy - y);
                exactPoints[pointCount++] = pt;
            }
        }
        painter.drawPolyline(exactPoints, pointCount);
    }
}
