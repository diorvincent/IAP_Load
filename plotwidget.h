#ifndef PLOTWIDGET_H
#define PLOTWIDGET_H

#include <QWidget>
#include <QMap>
#include <QTimer>
#include <QVector>
#include <QPointF>
#include <QMessageBox>
#include "databuffer.h"

#define CURRENTPOS "CurrentPos"
#define RPMS "RPM"
#define PHASECURRENT "PhaseCurrent"
#define MOTORTEMP "MotorTemp"
#define MOSTEMP "MosTemp"

class CurveData
{
public:
    QString name;
    DataBuffer *buffer = nullptr;
    bool bRPMVisable = false;
    bool bPosVisable = false;
    bool bCurrentVisable = false;
    bool bMotorTempVisable = false;
    bool bDrvTempVisable = false;
};

class PlotWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PlotWidget(QWidget *parent = nullptr);
    ~PlotWidget() override;

    void appendData(const QString &curveId, double x, double y);
    QVector<QPointF> getPartData(const QString &curveId, int key, int nLen);
    void clearAllData();

    void exportToCsv(const QString &fileName);
    bool saveScreenshot(const QString &fileName);
    // 接口：开启/关闭X自动跟随最新数据
    void enableXAutoFollow(bool enable) { m_followLatestX = enable; }

    void setWheelScaleX(bool bChecked);
    void setWheelScaleY(bool bChecked);

    CurveData m_cd;
    int m_nDataLen;
    // View settings
    const double m_DisplayXRange=6000;
    double m_xMin = 0, m_xMax = 2000, m_winWidth = 320;
    double m_yMin = -250, m_yMax = 250;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void drawGrid(QPainter &painter);
    void drawCurves(QPainter &painter);
    void drawLegend(QPainter &painter);
    void autoScale();
    QPointF dataToScreen(const QPointF &dataPoint) const;
    QPointF screenToData(const QPoint &screenPoint) const;

    QMap<QString, CurveData> m_curves;
    double m_xOffset=0;//全局偏移量，控制左右滚动
    double m_xNow = 0;//当前x轴坐标

    // View settings
    bool m_autoScaleX = true;
    bool m_autoScaleY = true;

    // 新增：缩放后是否自动跟随最新X数据
    bool m_followLatestX = true;
    bool m_wheelScaleX = false;  // 滚轮是否缩放X
    bool m_wheelScaleY = true;   // 默认滚轮缩放Y，兼容旧行为
    // Pan/Zoom
    bool m_panning = false;
    QPoint m_lastMousePos;
    double m_xRangeBefore = 10;
    double m_yRangeBefore = 2;

    // Margins
    int m_leftMargin = 60;
    int m_rightMargin = 20;
    int m_topMargin = 20;
    int m_bottomMargin = 40;

    // Colors
    QVector<QColor> m_defaultColors;
};

#endif // PLOTWIDGET_H
