#include "proxyplot.h"
#include <QFile>
#include <QTextStream>

proxyPlot::proxyPlot(QVBoxLayout * boxLayout)
{
  m_customPlot = new QCustomPlot(this);
  boxLayout->addWidget(m_customPlot);

  m_zoomXEnable = false;
  m_zoomYEnable = true;

  initQCP();

  //1s绘图定时器
  m_plotTimer = new QTimer(this);
  m_plotTimer->setInterval(1000);
  connect(m_plotTimer,&QTimer::timeout,this,&proxyPlot::slotUpdateCurve);
  m_plotTimer->start();
  m_bTimerStarted = true;

  //鼠标移动/缩放，用于游标
  //connect(m_customPlot,SIGNAL(mouseMove(QMouseEvent*)),this,SLOT(slotMouseMove(QMouseEvent*)));
  connect(m_customPlot, SIGNAL(mouseWheel(QWheelEvent*)), this, SLOT(slotWheelEvent(QWheelEvent*)));
}

proxyPlot::~proxyPlot()
{
  delete m_customPlot;
  m_customPlot = nullptr;
}

void proxyPlot::DrawingCurve(bool bRun)
{
  if(bRun)
  {
      if(!m_bTimerStarted)
      {
          connect(m_plotTimer,&QTimer::timeout,this,&proxyPlot::slotUpdateCurve);
          m_plotTimer->start();
          m_bTimerStarted=true;
      }
  }
  else
  {
      m_plotTimer->stop();
      disconnect(m_plotTimer,&QTimer::timeout,this,&proxyPlot::slotUpdateCurve);
      m_bTimerStarted = false;
  }
}

void proxyPlot::initQCP()
{
    m_customPlot->setObjectName(QString::fromUtf8("customPlot"));
    QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);
    sizePolicy.setHeightForWidth(m_customPlot->sizePolicy().hasHeightForWidth());
    m_customPlot->setSizePolicy(sizePolicy);

    m_customPlot->xAxis->setTicks(true);
    m_customPlot->yAxis->setTicks(true);
    m_customPlot->xAxis->setTickLabels(true);
    m_customPlot->yAxis->setTickLabels(true);

    m_customPlot->xAxis->grid()->setVisible(true);
    m_customPlot->yAxis->grid()->setVisible(true);
    m_customPlot->xAxis->grid()->setSubGridVisible(false);
    m_customPlot->yAxis->grid()->setSubGridVisible(false);

    m_customPlot->xAxis2->setVisible(true);
    m_customPlot->yAxis2->setVisible(true);
    m_customPlot->yAxis2->setTicks(true);
    m_customPlot->yAxis2->setTickLabels(true);

    connect(m_customPlot->xAxis, SIGNAL(rangeChanged(QCPRange)), m_customPlot->xAxis2, SLOT(setRange(QCPRange)));
    connect(m_customPlot->yAxis, SIGNAL(rangeChanged(QCPRange)), m_customPlot->yAxis2, SLOT(setRange(QCPRange)));

    setPlotTheme(Qt::white, Qt::black);

    //关闭自带滚轮缩放，仅开启拖拽、坐标轴选中
    m_customPlot->setInteractions(QCP::iRangeDrag | QCP::iSelectAxes);

    QSharedPointer<QCPAxisTickerTime> timeTicker(new QCPAxisTickerTime);
    timeTicker->setTimeFormat("%m:%s:%z");
    m_customPlot->xAxis->setTicker(timeTicker);
    m_customPlot->axisRect()->setupFullAxesBox();

    m_customPlot->xAxis->setLabel("Time");
    m_customPlot->yAxis->setLabel("Value");
    m_customPlot->yAxis->setRange(-5, 5);

    initGraphName("CURRENTPOS", 0, Qt::red);
    initGraphName("RPMS", 1, Qt::magenta);
    initGraphName("PHASECURRENT", 2, Qt::green, true);
    initGraphName("MOTORTEMP", 3, Qt::yellow);
    initGraphName("MOSTEMP", 4, Qt::blue);

    /*
    //初始化游标
    m_tracer = new QCPItemTracer(m_customPlot);
    m_tracer->setPen(QPen(Qt::yellow));
    m_tracer->setBrush(QBrush(Qt::red));
    m_tracer->setStyle(QCPItemTracer::tsCircle);
    m_tracer->setSize(6);

    m_tracerLabel = new QCPItemText(m_customPlot);
    m_tracerLabel->setLayer("overlay");
    m_tracerLabel->setPen(QPen(Qt::white));
    m_tracerLabel->setPositionAlignment(Qt::AlignLeft|Qt::AlignTop);
    m_tracerLabel->position->setParentAnchor(m_tracer->position);

    */


    m_customPlot->replot();
}

void proxyPlot::setPlotTheme(QColor axis, QColor background)
{
    m_customPlot->xAxis->setLabelColor(axis);
    m_customPlot->yAxis->setLabelColor(axis);
    m_customPlot->xAxis->setTickLabelColor(axis);
    m_customPlot->yAxis->setTickLabelColor(axis);
    m_customPlot->xAxis->setBasePen(QPen(axis,1));
    m_customPlot->yAxis->setBasePen(QPen(axis,1));
    m_customPlot->xAxis->setTickPen(QPen(axis,1));
    m_customPlot->yAxis->setTickPen(QPen(axis,1));
    m_customPlot->xAxis->setSubTickPen(QPen(axis,1));
    m_customPlot->yAxis->setSubTickPen(QPen(axis,1));

    m_customPlot->xAxis2->setLabelColor(axis);
    m_customPlot->yAxis2->setLabelColor(axis);
    m_customPlot->xAxis2->setTickLabelColor(axis);
    m_customPlot->yAxis2->setTickLabelColor(axis);
    m_customPlot->xAxis2->setBasePen(QPen(axis,1));
    m_customPlot->yAxis2->setBasePen(QPen(axis,1));
    m_customPlot->xAxis2->setTickPen(QPen(axis,1));
    m_customPlot->yAxis2->setTickPen(QPen(axis,1));
    m_customPlot->xAxis2->setSubTickPen(QPen(axis,1));
    m_customPlot->yAxis2->setSubTickPen(QPen(axis,1));

    m_customPlot->setBackground(background);
    m_customPlot->axisRect()->setBackground(background);
    m_customPlot->replot();
}

void proxyPlot::initGraphName(QString name, int index, QColor color, bool bVisable)
{
    m_customPlot->addGraph();
    //int curIdx = m_customPlot->graphCount()-1;
    m_customPlot->graph(index)->setPen(QPen(color));
    m_customPlot->graph(index)->setVisible(bVisable);
    m_nameToGraphMap[name] = index;
}

void proxyPlot::setGrapVisable(int nIndex, bool bVisable)
{
  m_customPlot->graph(nIndex)->setVisible(bVisable);
}

void proxyPlot::setGrapYRange(double B, double T)
{
  m_customPlot->yAxis->setRange(B,T);
  m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void proxyPlot::slotCanFdRecv(double key, double pos, double rpm, double phaseCurrent, double motorTemp, double mosTemp)
{
    MotorSample sample;
    sample.pos = pos;
    sample.rpm = rpm;
    sample.phaseCurrent = phaseCurrent;
    sample.motorTemp = motorTemp;
    sample.mosTemp = mosTemp;
    sample.timeKey = key;

    m_sampleBuf.append(sample);
    if(m_sampleBuf.size()>MAX_DATABUFFER)
    {
        QString strPath,strFileName;
        static QTime time = QTime::currentTime();
        strFileName = time.toString("hh_mm_ss");
        strPath = QDir(QCoreApplication::applicationDirPath()).filePath("log");
        QDir dir(strPath);
        if(!dir.exists())
            dir.mkdir(".");
        strFileName = strPath + "/" + strFileName + ".csv";
        export2CSVFile(strFileName);

        m_sampleBuf.clear(); //removeFirst();
    }
}

bool proxyPlot::export2CSVFile(QString fileName)
{
  QFile file(fileName);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
      return false;

  //write UTF-8 BOM head
  file.write("\xEF\xBB\xBF");
  QTextStream stream(&file);
  stream.setCodec("UTF-8");

  // Header
  stream << "Time";
  QVector<QString> headers;
  headers.append("CURRENTPOS");
  headers.append("RPMS");
  headers.append("PHASECURRENT");
  headers.append("MOTORTEMP");
  headers.append("MOSTEMP");

  for (const QString &h : headers) {
      stream << ",        " << h;
  }
  stream << "\n";

  QTimeZone tz = QTimeZone::utc();
  for(MotorSample &sample: m_sampleBuf)
  {
    stream <<QDateTime::fromMSecsSinceEpoch(
               static_cast<qint64>(sample.timeKey * 1000.0f),tz)
               .toString("hh:mm:ss:zzz")
              << "," <<sample.pos<< "        " <<","<<
              sample.rpm<< "        " <<","<<sample.phaseCurrent<< "        " <<","<<
              sample.motorTemp<< "        " <<","<<sample.mosTemp;
    stream << "\n";
  }

  file.close();

  return true;
}

void proxyPlot::slotUpdateCurve()
{
    if(m_sampleBuf.isEmpty())
        return;

    for(auto &sp : m_sampleBuf)
    {
        m_customPlot->graph(m_nameToGraphMap["CURRENTPOS"])->addData(sp.timeKey, sp.pos);
        m_customPlot->graph(m_nameToGraphMap["RPMS"])->addData(sp.timeKey, sp.rpm);
        m_customPlot->graph(m_nameToGraphMap["PHASECURRENT"])->addData(sp.timeKey, sp.phaseCurrent);
        m_customPlot->graph(m_nameToGraphMap["MOTORTEMP"])->addData(sp.timeKey, sp.motorTemp);
        m_customPlot->graph(m_nameToGraphMap["MOSTEMP"])->addData(sp.timeKey, sp.mosTemp);
    }

    double lastKey = m_sampleBuf.back().timeKey;
    if(lastKey <= timeAxis)
    {
        m_customPlot->xAxis->setRange(timeAxis, timeAxis, Qt::AlignRight);
    }
    else
    {
        m_customPlot->xAxis->setRange(lastKey, timeAxis, Qt::AlignRight);
    }

    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

QCPAxis* proxyPlot::getMouseAxis(QPoint mousePos)
{
  auto rect = m_customPlot->axisRectAt(mousePos);
  if(!rect) return nullptr;
  QList<QCPAxis*> axes = rect->axes();
  for(auto ax : axes)
  {
      if(ax->selectTest(mousePos, false) > 0)
          return ax;
  }
  return nullptr;
}

//滚轮缩放，以鼠标位置为中心点缩放
void proxyPlot::slotWheelEvent(QWheelEvent *event)
{
    QPoint pos = m_customPlot->mapFromGlobal(event->globalPos());
    if(!m_customPlot->rect().contains(pos))
        return;

    double scale = 1.1;
    if(event->angleDelta().y() < 0)
        scale = 1.0 / scale;

    //QCPAxis *activeAxis = getMouseAxis(pos);

    if(/*activeAxis == m_customPlot->xAxis &&*/ m_zoomXEnable)
    {
        double mouseCoord = m_customPlot->xAxis->pixelToCoord(pos.x());
        QCPRange rg = m_customPlot->xAxis->range();
        double L = mouseCoord - (mouseCoord - rg.lower)/scale;
        double R = mouseCoord + (rg.upper - mouseCoord)/scale;
        m_customPlot->xAxis->setRange(L,R);
    }
    if(/*activeAxis == m_customPlot->yAxis &&*/ m_zoomYEnable)
    {
        double mouseCoord = m_customPlot->yAxis->pixelToCoord(pos.y());
        QCPRange rg = m_customPlot->yAxis->range();
        double B = mouseCoord - (mouseCoord - rg.lower)/scale;
        double T = mouseCoord + (rg.upper - mouseCoord)/scale;
        m_customPlot->yAxis->setRange(B,T);
    }
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void proxyPlot::setZoomX(bool enable)
{
    m_zoomXEnable = enable;
}

void proxyPlot::setZoomY(bool enable)
{
    m_zoomYEnable = enable;
}

//鼠标移动‑游标拾取最近曲线点
void proxyPlot::slotMouseMove(QMouseEvent *event)
{
    QPoint pos = m_customPlot->mapFromGlobal(event->globalPos());
    double x = m_customPlot->xAxis->pixelToCoord(pos.x());
    int mainGraph = m_nameToGraphMap["PHASECURRENT"];

    if(m_customPlot->graph(0)->visible())
    {
      mainGraph = m_nameToGraphMap["CURRENTPOS"];
      m_tracer->setGraph(m_customPlot->graph(mainGraph));
      m_tracer->setGraphKey(x);
      m_tracerLabel->setText(QString("X:%1\nY:%2")
                                  .arg(m_tracer->position->key(),0,'f',3)
                                  .arg(m_tracer->position->value(),0,'f',3));
    }

    if(m_customPlot->graph(1)->visible())
    {
      mainGraph = m_nameToGraphMap["RPMS"];
      m_tracer->setGraph(m_customPlot->graph(mainGraph));
      m_tracer->setGraphKey(x);
      m_tracerLabel->setText(QString("X:%1\nY:%2")
                                  .arg(m_tracer->position->key(),0,'f',3)
                                  .arg(m_tracer->position->value(),0,'f',3));
    }

    if(m_customPlot->graph(2)->visible())
    {
      mainGraph = m_nameToGraphMap["PHASECURRENT"];
      m_tracer->setGraph(m_customPlot->graph(mainGraph));
      m_tracer->setGraphKey(x);
      m_tracerLabel->setText(QString("X:%1\nY:%2")
                                  .arg(m_tracer->position->key(),0,'f',3)
                                  .arg(m_tracer->position->value(),0,'f',3));
    }

    if(m_customPlot->graph(3)->visible())
    {
      mainGraph = m_nameToGraphMap["MOTORTEMP"];
      m_tracer->setGraph(m_customPlot->graph(mainGraph));
      m_tracer->setGraphKey(x);
      m_tracerLabel->setText(QString("X:%1\nY:%2")
                                  .arg(m_tracer->position->key(),0,'f',3)
                                  .arg(m_tracer->position->value(),0,'f',3));
    }

    if(m_customPlot->graph(4)->visible())
    {
      mainGraph = m_nameToGraphMap["MOSTEMP"];
      m_tracer->setGraph(m_customPlot->graph(mainGraph));
      m_tracer->setGraphKey(x);
      m_tracerLabel->setText(QString("X:%1\nY:%2")
                                  .arg(m_tracer->position->key(),0,'f',3)
                                  .arg(m_tracer->position->value(),0,'f',3));
    }
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);
}

bool proxyPlot::saveImage(QString filename)
{
  return m_customPlot->saveJpg(filename, 1024, 768, 1.0, -1, 255, QCP::ResolutionUnit::ruDotsPerInch);
}
