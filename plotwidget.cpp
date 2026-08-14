/*
模块说明：根据电机实时反馈的系列参数数据(转速、相电流、位置、电机温度、mos温度),绘制实时曲线
时间：20260721
作者：hejingchi
*/

#include "plotwidget.h"
#include "csvexporter.h"
#include <QPainter>
#include <QMouseEvent>
#include <QFile>
#include <QtMath>
#include <QDebug>
#include <QDateTime>
#include <algorithm>

PlotWidget::PlotWidget(QWidget *parent)
    : QWidget(parent),
    m_nDataLen(100)

{
    setMinimumSize(500, 300);
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);

    m_defaultColors << Qt::blue << Qt::red << Qt::green << Qt::magenta
                    << Qt::cyan << Qt::darkYellow << Qt::darkRed << Qt::darkGreen;

    // create each curve memory
    m_curves[CURRENTPOS].buffer = new DataBuffer(m_DisplayXRange, this);
    m_curves[RPMS].buffer = new DataBuffer(m_DisplayXRange, this);
    m_curves[PHASECURRENT].buffer = new DataBuffer(m_DisplayXRange, this);
    m_curves[MOTORTEMP].buffer = new DataBuffer(m_DisplayXRange, this);
    m_curves[MOSTEMP].buffer = new DataBuffer(m_DisplayXRange, this);
}

PlotWidget::~PlotWidget()
{
    for (auto &cd : m_curves) {
        if(cd.buffer!=nullptr)
            delete cd.buffer;
    }
}

void PlotWidget::appendData(const QString &curveId, double x, double y)
{
    // 曲线不存在则创建
    if (!m_curves.contains(curveId))
    {
        CurveData newCd;
        newCd.buffer = new DataBuffer(m_DisplayXRange, this);
        newCd.name = curveId;
        // 默认开启对应曲线可见
        if(curveId == CURRENTPOS) newCd.bPosVisable = true;
        else if(curveId == RPMS) newCd.bRPMVisable = true;
        else if(curveId == PHASECURRENT) newCd.bCurrentVisable = true;
        else if(curveId == MOTORTEMP) newCd.bMotorTempVisable = true;
        else if(curveId == MOSTEMP) newCd.bDrvTempVisable = true;
        m_curves.insert(curveId, newCd);
    }

    CurveData& cd = m_curves[curveId];
    cd.buffer->append(x, y);
    cd.name = curveId;
    m_xNow = x;

    //缩放后(m_autoScaleX=false)，开启跟随则自动右移视口显示最新数据
   if(m_followLatestX && !m_autoScaleX)
   {
       //double xRange = m_xMax - m_xMin;
       // 最新数据超出当前视口右侧，平移X区间
       if(x > m_xMax)
       {
           double offset = x - m_xMax;
           m_xMin += offset;
           m_xMax += offset;

           // 限制X总宽度不超过DisplayXRange
          if(m_xMax - m_xMin > m_DisplayXRange)
          {
              m_xMin = m_xMax - m_DisplayXRange;
          }
       }
   }
   // 自动缩放模式下每次追加重新计算极值
   if(m_autoScaleX || m_autoScaleY)
       autoScale();

   // redraw painting area
   update();

}

QVector<QPointF> PlotWidget::getPartData(const QString &curveId, int key, int nLen)
{
  if (!m_curves.contains(curveId))
          return {};
      QVector<QPointF> vec = m_curves[curveId].buffer->getData();
      // 查找x在[key-100, key+100]区间的数据起点
      int startIdx = 0;
      for(int i=0; i<vec.size(); i++)
      {
          if(vec[i].x() > (key - 100) && vec[i].x() < (key + 100))
          {
              startIdx = i;
              break;
          }
      }
      int takeLen = qMin(nLen, vec.size() - startIdx);
      m_nDataLen = takeLen;
      return vec.mid(startIdx, takeLen);
}

void PlotWidget::clearAllData()
{
    for (auto &cd : m_curves) {
        cd.buffer->clear();
    }
    // 清空后恢复自动缩放+自动跟随
    m_autoScaleX = true;
    m_autoScaleY = true;
    m_followLatestX = true;
    update();
}

void PlotWidget::exportToCsv(const QString &fileName)
{
    QVector<QString> headers;
    QVector<QVector<QPointF>> data;

    QVector<QPointF> vec = m_curves[PHASECURRENT].buffer->getData();
    if(vec.size() >0)
      {
        for (const auto &cd : m_curves) {
            headers.append(cd.name);
            data.append(cd.buffer->getData());
        }

        if(CsvExporter::exportToFile(fileName, headers, data))
            QMessageBox::information(this, "提示", "测试数据保存成功！");
        else
            QMessageBox::warning(this, "提示", "保存测试数据失败！");
      }
    else
      QMessageBox::information(this, "提示", "无测试数据！");
}

bool PlotWidget::saveScreenshot(const QString &fileName)
{
  bool bRight = false;
    QPixmap pixmap(size());
    render(&pixmap);
    if(pixmap.save(fileName, "PNG"))
     {
        QMessageBox::information(this, "提示", "曲线截图保存成功！");
        bRight = true;
     }
  return bRight;
}

void PlotWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    drawGrid(painter);
    drawCurves(painter);
    drawLegend(painter);
}

void PlotWidget::drawGrid(QPainter &painter)
{
    painter.setPen(QPen(Qt::lightGray, 1, Qt::DotLine));

    // Horizontal grid lines
    int hLines = 5;
    for (int i = 0; i <= hLines; ++i)
    {
        double y = m_yMin + (m_yMax - m_yMin) * i / hLines;
        int py = dataToScreen(QPointF(0, y)).y();
        painter.drawLine(m_leftMargin, py, width() - m_rightMargin, py);

        // Y-axis labels
        painter.setPen(Qt::black);
        painter.drawText(5, py - 6, 50, 12, Qt::AlignRight | Qt::AlignVCenter,
                         QString::number(y, 'f', 2));
        painter.setPen(QPen(Qt::lightGray, 1, Qt::DotLine));
    }

    // Vertical grid lines
    int vLines = 5;
    for (int i = 0; i <= vLines; ++i) {
        double x = m_xMin + (m_xMax - m_xMin) * i / vLines; // x = 总毫秒
        int px = dataToScreen(QPointF(x, 0)).x();
        painter.drawLine(px, m_topMargin, px, height() - m_bottomMargin);
        // ========== ms switch to mm:ss.zzz ==========
        qint64 totalMs = qRound(x);
        qint64 ms = totalMs % 1000;
        qint64 secAll = totalMs / 1000;
        qint64 sec = secAll % 60;
        qint64 min = secAll / 60;

        QString timeStr = QString("%1:%2.%3")
            .arg(min, 2, 10, QChar('0'))
            .arg(sec, 2, 10, QChar('0'))
            .arg(ms, 3, 10, QChar('0'));
        // ===========================================
        // X-axis labels
        painter.setPen(Qt::black);
        painter.drawText(px - 30, height() - m_bottomMargin + 5, 80, 20,
                         Qt::AlignHCenter | Qt::AlignTop, timeStr);
                         //QString::number(x, 'f', 1));
        painter.setPen(QPen(Qt::lightGray, 1, Qt::DotLine));
    }

    // Draw axes
    painter.setPen(QPen(Qt::black, 1));
    painter.drawLine(m_leftMargin, m_topMargin, m_leftMargin, height() - m_bottomMargin);
    painter.drawLine(m_leftMargin, height() - m_bottomMargin,
                     width() - m_rightMargin, height() - m_bottomMargin);
}

void PlotWidget::drawCurves(QPainter &painter)
{
    if (m_curves.isEmpty()) return;

    if (m_autoScaleX || m_autoScaleY) {
        autoScale();
    }

    for (const auto &cd : m_curves)
    {
        QVector<QPointF> data = cd.buffer->getData();
        if (data.size() < 2) continue;

        bool visible = false;
        // 设置画笔+可见判断
        if(cd.name == CURRENTPOS && m_cd.bPosVisable)
        {
            painter.setPen(QPen(Qt::red, 2));
            visible = true;
        }
        else if (cd.name == RPMS && m_cd.bRPMVisable)
        {
            painter.setPen(QPen(Qt::magenta, 2));
            visible = true;
        }
        else if (cd.name == PHASECURRENT && m_cd.bCurrentVisable)
        {
            painter.setPen(QPen(Qt::green, 2));
            visible = true;
        }
        else if (cd.name == MOTORTEMP && m_cd.bMotorTempVisable)
        {
            painter.setPen(QPen(Qt::blue, 2));//蓝色
            visible = true;
        }
        else if (cd.name == MOSTEMP && m_cd.bDrvTempVisable)
        {
            painter.setPen(QPen(Qt::black, 2));//黑色
            visible = true;
        }
        if(!visible) continue;

        // 降采样优化性能：每个像素最多2个点
        int plotWidth = width() - m_leftMargin - m_rightMargin;
        int step = qMax(1, data.size() / (plotWidth * 2));
        QPolygonF polyline;

        // ========== 两种模式统一使用dataToScreen ==========
        //全部用真实数据x做坐标转换
        for (int i = 0; i < data.size(); i += step)
        {
            // dataToScreen 内部自动使用 m_xMin/m_xMax，缩放后自动生效
            QPointF screenPt = dataToScreen(data[i]);
            // 裁剪画布外点，优化绘制
            if (screenPt.x() >= m_leftMargin && screenPt.x() <= width() - m_rightMargin &&
                screenPt.y() >= m_topMargin && screenPt.y() <= height() - m_bottomMargin)
            {
                polyline.append(screenPt);
            }
        }


        /*
        if(!m_bDisplayMode)
        {
            // 实时滚动模式：自动缩放/平移生效
            double stepX = (double)plotWidth/m_DisplayXRange;
            for (int i = 0; i < data.size()-1; i += step) {
                QPointF screenPt = dataToScreen(data[i]);
                screenPt.setX(i * stepX);
                if (screenPt.x() >= 0 && screenPt.x() <= width() - m_rightMargin &&
                    screenPt.y() >= m_topMargin && screenPt.y() <= height() - m_bottomMargin)
                {
                    screenPt.setX(i * stepX+ m_leftMargin);
                    polyline.append(screenPt);
                }
            }
        }
        else
        {
            // 历史回放模式：同样复用视口缩放平移，删除原i*stepX硬写死X逻辑
            for (int i = 0; i < data.size()-1; i += step) {
                QPointF screenPt = dataToScreen(data[i]);
                if (screenPt.x() >= m_leftMargin && screenPt.x() <= width() - m_rightMargin &&
                    screenPt.y() >= m_topMargin && screenPt.y() <= height() - m_bottomMargin)
                {
                    polyline.append(screenPt);
                }
            }
        }
*/



        if (polyline.size() >= 2) {
            painter.drawPolyline(polyline);
        }
  }
}

void PlotWidget::drawLegend(QPainter &painter)
{
    int x = width() - m_rightMargin - 150;
    int y = m_topMargin + 10;
    int lineHeight = 18;

    painter.setPen(Qt::black);
    painter.setBrush(QColor(255, 255, 255, 200));
    painter.drawRect(x - 5, y - 5, 140, m_curves.size() * lineHeight + 10);

    int idx = 0;
    for (const auto &cd : m_curves)
    {
        // 匹配曲线颜色
        if(cd.name == CURRENTPOS)
            painter.setPen(Qt::red);
        else if(cd.name == RPMS)
            painter.setPen(Qt::magenta);
        else if(cd.name == PHASECURRENT)
            painter.setPen(Qt::green);
        else if(cd.name == MOTORTEMP)
            painter.setPen(Qt::yellow);
        else if(cd.name == MOSTEMP)
            painter.setPen(Qt::gray);

        painter.drawLine(x, y + idx * lineHeight + 8, x + 20, y + idx * lineHeight + 8);

        painter.setPen(Qt::black);
        painter.drawText(x + 25, y + idx * lineHeight, 120, lineHeight,
                         Qt::AlignLeft | Qt::AlignVCenter, cd.name);
        idx++;
    }
}

void PlotWidget::autoScale()
{
    if (m_curves.isEmpty()) return;

    double xMin = 1e300, xMax = -1e300;
    double yMin = 1e300, yMax = -1e300;
    bool hasData = false;

    for (const auto &cd : m_curves) {
        if (cd.buffer->isEmpty()) continue;

        auto data = cd.buffer->getData();
        for (const auto &pt : data) {
            xMin = qMin(xMin, pt.x());
            xMax = qMax(xMax, pt.x());
            yMin = qMin(yMin, pt.y());
            yMax = qMax(yMax, pt.y());
            hasData = true;
        }
    }

    if (!hasData) return;

    if (m_autoScaleX) {
        m_xMin = xMin;
        m_xMax = xMax;
        if (m_xMax <= m_xMin) m_xMax = m_xMin + 1;
    }

    if (m_autoScaleY) {
        double yMargin = (yMax - yMin) * 0.1;
        if (yMargin < 0.001) yMargin = 0.1;
        m_yMin = yMin - yMargin;
        m_yMax = yMax + yMargin;
    }
}

QPointF PlotWidget::dataToScreen(const QPointF &dataPoint) const
{
    double plotWidth = width() - m_leftMargin - m_rightMargin;
    double plotHeight = height() - m_topMargin - m_bottomMargin;

    double xScale = plotWidth / (m_xMax - m_xMin);
    double yScale = plotHeight / (m_yMax - m_yMin);

    double x = m_leftMargin + (dataPoint.x() - m_xMin) * xScale;
    double y = height() - m_bottomMargin - (dataPoint.y() - m_yMin) * yScale;

    return QPointF(x, y);
}

QPointF PlotWidget::screenToData(const QPoint &screenPoint) const
{
    double plotWidth = width() - m_leftMargin - m_rightMargin;
    double plotHeight = height() - m_topMargin - m_bottomMargin;

    double x = m_xMin + (screenPoint.x() - m_leftMargin) * (m_xMax - m_xMin) / plotWidth;
    double y = m_yMin + (height() - m_bottomMargin - screenPoint.y()) * (m_yMax - m_yMin) / plotHeight;

    return QPointF(x, y);
}

void PlotWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}

void PlotWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_panning = true;
        m_lastMousePos = event->pos();
        m_xRangeBefore = m_xMax - m_xMin;
        m_yRangeBefore = m_yMax - m_yMin;
    }
}

void PlotWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_panning) {
        QPoint delta = event->pos() - m_lastMousePos;

        double plotWidth = width() - m_leftMargin - m_rightMargin;
        double plotHeight = height() - m_topMargin - m_bottomMargin;

        double dx = -delta.x() * (m_xRangeBefore) / plotWidth;
        double dy = delta.y() * (m_yRangeBefore) / plotHeight;

        m_xMin += dx;
        m_xMax += dx;
        m_yMin += dy;
        m_yMax += dy;

        m_autoScaleX = false;
        m_autoScaleY = false;
        // 拖拽后停止自动跟随最新数据
        m_followLatestX = false;
        m_lastMousePos = event->pos();
        update();
    }
}

void PlotWidget::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event)
    m_panning = false;
}

void PlotWidget::wheelEvent(QWheelEvent *event)
{
    double scaleFactor = (event->angleDelta().y() > 0) ? 0.9 : 1.1;
    QPointF mouseData = screenToData(event->position().toPoint());

    // 原始区间
    double oldXRange = m_xMax - m_xMin;
    double oldYRange = m_yMax - m_yMin;
    const double minXRange = 10;
    const double minYRange = 0.1;

    // 仅勾选X时修改X范围，仅勾选Y时修改Y范围
    if (m_wheelScaleX)
    {
        double newXRange = oldXRange * scaleFactor;
        if(newXRange < minXRange) newXRange = minXRange;
        double xRatio = (mouseData.x() - m_xMin) / oldXRange;
        m_xMin = mouseData.x() - xRatio * newXRange;
        m_xMax = mouseData.x() + (1 - xRatio) * newXRange;
    }
    if (m_wheelScaleY)
    {
        double newYRange = oldYRange * scaleFactor;
        if(newYRange < minYRange) newYRange = minYRange;
        double yRatio = (mouseData.y() - m_yMin) / oldYRange;
        m_yMin = mouseData.y() - yRatio * newYRange;
        m_yMax = mouseData.y() + (1 - yRatio) * newYRange;
    }
    m_autoScaleX = false;
    m_autoScaleY = false;
    m_followLatestX = false;
    update();
}


void PlotWidget::setWheelScaleX(bool bChecked)
{
  if(bChecked)
    m_wheelScaleX = true;
  else
    m_wheelScaleX = false;

}
void PlotWidget::setWheelScaleY(bool bChecked)
{
  if(bChecked)
    m_wheelScaleY = true;
  else
    m_wheelScaleY = false;
}
