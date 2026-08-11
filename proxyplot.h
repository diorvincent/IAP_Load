#ifndef PROXYPLOT_H
#define PROXYPLOT_H

#include <QWidget>
#include <qcustomplot.h>
#include <QTimer>
#include <QMap>
#include <QVector>
#include <QTime>
#include <QMouseEvent>

#define MAX_DATABUFFER 1000
//单次采样结构体
struct MotorSample
{
    double pos;
    double rpm;
    double phaseCurrent;
    double motorTemp;
    double mosTemp;
    double timeKey;
};


class proxyPlot : public QWidget
{
  Q_OBJECT
public:
  explicit proxyPlot(QVBoxLayout * boxLayout);
  ~proxyPlot();

private slots:
    void slotUpdateCurve();
    void slotMouseMove(QMouseEvent *event);
    void slotWheelEvent(QWheelEvent *event);

public slots:
    void setZoomX(bool enable);
    void setZoomY(bool enable);
    bool saveImage(QString filename);
    void slotCanFdRecv(double key, double pos,double rpm,double phaseCurrent,double motorTemp,double mosTemp);

public:
    void setGrapVisable(int nIndex, bool bVisable);
    void setGrapYRange(double B, double T);
    void DrawingCurve(bool bRun = true);

    bool export2CSVFile(QString fileName);
private:
    QCustomPlot *m_customPlot;

    QMap<QString,int> m_nameToGraphMap;
    QVector<MotorSample> m_sampleBuf;
    QTimer *m_plotTimer;

    const int timeAxis = 10;
    static QTime timeStart;
    bool m_bTimerStarted;
    bool m_zoomXEnable;
    bool m_zoomYEnable;

    //游标
    QCPItemTracer *m_tracer;
    QCPItemText *m_tracerLabel;

    void initQCP();
    void setPlotTheme(QColor axis, QColor background);
    void initGraphName(QString name, int index, QColor color, bool bVisable = false);

    QCPAxis* getMouseAxis(QPoint mousePos);
protected:
    //void wheelEvent(QWheelEvent *event) override;
};

#endif // PROXYPLOT_H
