#include "simresultplot.h"
#include <QPainter>
#include <QGuiApplication>
#include <QScreen>

SimResultPlot::SimResultPlot(QWidget *parent) : QWidget(parent)
{
    initGraphics();
}

QSize SimResultPlot::sizeHint() const
{
    return QSize(800, 550);
}

QSize SimResultPlot::minimumSizeHint() const
{
    return QSize(550, 380);
}


void SimResultPlot::setSimResults(SimResultsItem * results) {
    _results = *results;
    update();
}

bool isHighDPI() {
    QList<QScreen*> screens = QGuiApplication::screens();
    if (screens.length() > 0) {
        double pixelRatio = screens.first()->devicePixelRatio();
        qDebug("Screens: %d  screen[0] pixelRatio=%f",  screens.length(), pixelRatio);
        return pixelRatio > 160;
    } else {
        qDebug("Cannot get screens - unknown dpi");
        return false;
    }
}

void SimResultPlot::initGraphics() {
    bool hdpi = true; //isHighDPI();
    int penWidth = hdpi ? 5 : 3;
    Qt::PenStyle style = Qt::PenStyle(Qt::SolidLine);
    Qt::PenCapStyle cap = Qt::PenCapStyle(Qt::RoundCap);
    Qt::PenJoinStyle join = Qt::PenJoinStyle(Qt::RoundJoin);
    int c0 = 0;
    int c1 = 64;
    int c2 = 128;
    int c3 = 140;
    int c4 = 192;
    int c5 = 224;
    int c6 = 240;
    int c7 = 255;
    _colors[0] = QColor(c0, c4, c0); // current
    _colors[1] = QColor(c1, c1, c6); // +1
    _colors[2] = QColor(c4, c1, c2); // +2
    _colors[3] = QColor(c0, c3, c2); // +3
    _colors[4] = QColor(c2, c1, c3); // +4
    _colors[5] = QColor(c4, c1, c0); // +5
    _colors[6] = QColor(c2, c4, c2); // +6
    _colors[7] = QColor(c0, c2, c3); // +7
    _colors[8] = QColor(c2, c1, c4); // +8
    _colors[9]  = QColor(c4, c2, c1); // -7
    _colors[10] = QColor(c3, c1, c1); // -6
    _colors[11] = QColor(c1, c3, c1); // -5
    _colors[12] = QColor(c3, c1, c5); // -4
    _colors[13] = QColor(c5, c3, c1); // -3
    _colors[14] = QColor(c1, c3, c4); // -2
    _colors[15] = QColor(c4, c0, c0); // -1
    _pens[0] = QPen(_colors[0], penWidth+(hdpi ? 4 : 2), style, cap, join);
    for (int i = 1; i < 16; i++)
        _pens[i] = QPen(_colors[i], penWidth, style, cap, join);
}

void SimResultPlot::paintEvent(QPaintEvent * /* event */)
{
    int w = width();
    int h = height();
    QPainter painter(this);
    QBrush brush(QColor(255, 255, 250));
    QBrush brushLegend(QColor(240, 240, 237));
    QBrush brushCurrentValue(QColor(224, 255, 224));

    QBrush brushHaxis(QColor(212, 212, 212));
    QBrush brushVaxis(QColor(224, 224, 224));
    QBrush brushVaxisEven(QColor(200, 212, 200));
    painter.setBrush(brush);
    painter.fillRect(QRect(0, 0, w, h), brush);
    int fh = painter.fontMetrics().height();
    int haxish = fh * 130/100;
    int haxisy = h - fh/4 - haxish;

    painter.fillRect(QRect(0, haxisy, w, 2), brushHaxis);
    int minbits = 8;
    int maxbits = 24;
    int fracbits = (_results.byParameterValue.length() == 0) ? 2 : _results.byParameterValue[0].bitStats.bitFractionCount();
    int maxPercent = (int)_results.maxPercent(minbits, maxbits);
    //maxPercent = (maxPercent + 4) / 5 * 5;
    if (maxPercent < 10)
        maxPercent = 10;
    maxPercent = maxPercent * 120/100;
    double yscalef = (haxisy / maxPercent);
    int yscale = (int)yscalef;
    for (int k = 10; k < maxPercent; k += 10) {
        int y = haxisy - k*yscale;
        if (y > 0)
            painter.fillRect(QRect(0, y, w, 1), brushHaxis);
    }

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

    int legendx0 = 10;
    int legendy0 = fh/4;
    if (_results.name.length()>0) {
        painter.drawText(QPoint(legendx0, legendy0+fh), _results.name);
        int namew = painter.fontMetrics().horizontalAdvance(_results.name);
        legendx0 += namew + fh;
    }

    if (_results.byParameterValue.length() == 0)
        return; // nothing to draw

    int legendx = legendx0;
    int legendy = legendy0;
    int underlineOffset = fh + fh/4 + 3;
    for (int i = 0; i < _results.byParameterValue.length(); i++) {
        QString param = _results.byParameterValue[i].parameterValue;
        int paramw = painter.fontMetrics().horizontalAdvance(param);
        if (legendx + paramw + fh/4 > w && legendx > legendx0) {
            legendx = legendx0;
            legendy += fh + fh/2;
        }
        int penIndex = (i - _results.originalValueIndex) & 15;
        bool isCurrent = (i == _results.originalValueIndex);
        if (isCurrent) {
            painter.fillRect(QRect(legendx-fh/4, legendy - fh/5, paramw+fh/4+fh/4, fh+fh/5+fh/5), brushCurrentValue);
        }
        painter.setPen(_pens[penIndex]);
        painter.drawText(QPoint(legendx, legendy+fh), param);
        painter.drawLine(QPoint(legendx-fh/4, legendy+underlineOffset), QPoint(legendx+paramw+fh/4, legendy+underlineOffset));
        legendx += paramw + fh;
    }

    //int

    for (int i = 0; i < _results.byParameterValue.length(); i++) {
        //bool isCurrent = (i == _results.originalValueIndex);
        int penIndex = (i - _results.originalValueIndex) & 15;
        painter.setPen(_pens[penIndex]);
        int pointCount = 0;
        QPoint exactPoints[32*5];
        for (int j = minbits*fracbits; j <= maxbits*fracbits; j++) {
            int x = minbitsx + (j - minbits*fracbits) * bitstep / fracbits;
            ExactBitStats * stats = &_results.byParameterValue[i].bitStats;
            double v = stats->getPercent(j, minbits, maxbits);
            double nextv = stats->getPercent(j+1, minbits, maxbits);

            if (v > 0.0000000001 || nextv > 0.0000000001 || pointCount > 0) {
                int y = (int)(v*yscale);
                QPoint pt = QPoint(x, haxisy - y);
                exactPoints[pointCount++] = pt;
            }
        }
        painter.drawPolyline(exactPoints, pointCount);
    }
}
