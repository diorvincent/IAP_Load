#include "widget.h"
#include "ui_widget.h"
#include <QMetaMethod>
#include <QMetaObject>
Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget),
    serial(new QSerialPort(this)),
    OpenStatus(false),
    intValidator(new QIntValidator(1, 4000000, this)),
    currentCommMode(SERIAL_MODE),
    canDeviceHandle(INVALID_DEVICE_HANDLE),
    canChannelHandle(INVALID_CHANNEL_HANDLE),
    canOpenStatus(false),
    canId(0x001),
    canReceiveTimer(new QTimer(this)),
    firmwareFile(nullptr),
    binSize(0),
    totalSent(0),
    currentPacketIndex(0),
    upgradeState(STATE_IDLE),
    timer(new QTimer(this)),
    retryCount(0),
    lastCmdId(0),
    currentPacketSize(0),
    calibTimer(new QTimer(this)),
    currentCalibCmd(0),
    currentPos(0.0),
    currentSpeed(0.0),
    currentTorque(0.0),
    currentAlarm(0),
    m_nCurveDrawFrq(100),
    m_nTryReadT(0),
    m_uBinSize(0),
    alarmBlinkTimer(new QTimer(this)),
    isAlarmLedOn(true),  // 初始默认“亮”（报警时才会触发闪烁）
  // 初始化标定模式为未打开
    isCalibModeOpen(false),
    isMortorEnableOpen(false),
    isControlModeOpen(false),
    m_displayMotorStatus(true),
    m_PauseCapture(true),
    m_CurveDisplayMode(false),
    commTabWidget(nullptr),
    serialTabPage(nullptr),
    canTabPage(nullptr)

{
    QFont globalFont = QApplication::font();
    globalFont.setPointSize(10);
    // globalFont.setBold(true);
    QApplication::setFont(globalFont);

    qDebug() << "程序启动：初始化QApplication成功";
    ui->setupUi(this);

 //+20260709
    m_plotwidget = new PlotWidget(this);
#ifdef QT_NO_DEBUG

    //自定义绘制曲线
    ui->CurveVLayout->addWidget(m_plotwidget);
#endif

#ifdef QT_DEBUG
     m_plotwidget->setVisible(false);
    //使用qcustomplot绘制曲线
    m_customPlot = new QCustomPlot(this);
    m_customPlot->setObjectName(QString::fromUtf8("customPlot"));
    ui->CurveVLayout->addWidget(m_customPlot);
    initQCP();
#endif

    getMotorCurve(); //20260710
    Publish_or_Debug_Ver();//
//_

    ui->motorStopBtn->setStyleSheet(R"(
        QRadioButton::indicator {
            width: 30px;    /* 圆圈大小 */
            height: 30px;
            border-radius: 15px; /* 正圆 */
            background-color: red; /* 圆圈颜色 */
            border: 2px solid black;
        }
        QRadioButton::indicator:checked {
            background-color: red; /* 选中后保持红色 */
            background-image: radial-gradient(white 6px, transparent 6px); /* 选中时加白色圆点 */
        }
    )");

    // 初始化指针
    hybridPage = nullptr;
    hybridKpSpinBox = nullptr;
    hybridKdSpinBox = nullptr;
    hybridPosSpinBox = nullptr;
    hybridSpeedSpinBox = nullptr;
    hybridTorqueSpinBox = nullptr;
    hybridKpSlider = nullptr;
    hybridKdSlider = nullptr;
    hybridPosSlider = nullptr;
    hybridSpeedSlider = nullptr;
    hybridTorqueSlider = nullptr;

    qDebug() << "程序启动：初始化QApplication成功";

   // 连接信号槽
    setupHybridConnections();
    ui->tabWidget->setCurrentIndex(0);

    // CAN配置初始化
    fillCanConfig();
    QRegExp hexRegExp("^(0x)?[0-9a-fA-F]{1,8}$");
    canIdValidator = new QRegExpValidator(hexRegExp, this);
    ui->canIdEdit->setValidator(canIdValidator);
    ui->canIdEdit->setText(QString("0x%1").arg(canId, 8, 16, QChar('0')));
    connect(ui->canIdEdit, &QLineEdit::editingFinished, this, &Widget::on_canIdEdit_editingFinished);
//  canReceiveTimer->setInterval(5); // 轮询接收
//  canReceiveTimer->stop();

    //  十六进制验证
    QRegExp motorIdRegExp("^(0x)?[0-9a-fA-F]{1,8}$");
    motorIdValidator = new QRegExpValidator(motorIdRegExp, this);
    ui->MotorIdEdit->setValidator(motorIdValidator);
    // 未读取时为空
    ui->MotorIdEdit->clear();
    ui->AngleZeroEdit->clear();

    // 串口配置初始化
    ui->baudRateBox->setInsertPolicy(QComboBox::InsertAtTop);
    fillPortsInfo();
    checkCustomBaudRate(ui->baudRateBox->currentIndex());

    // 公共UI初始化
    ui->statusLabel_1->setText("未连接");
    ui->progressBar->setValue(0);
    ui->startUpgradeButton->setEnabled(false);
    ui->cancelButton->setEnabled(false);
    ui->viewButton->setEnabled(false);
    // 报警灯默认绿色
    ui->alarmLed->setStyleSheet("background-color: green; border-radius: 8px;");

    //  报警灯初始样式：常亮绿色圆形（无报警状态）
    ui->alarmLed->setFixedSize(30, 30); // 固定30x30尺寸（圆形）
    ui->alarmLed->setStyleSheet(R"(
        background-color: green;
        border-radius: 15px; /* 半径=宽高1/2，正圆形 */
        border: 1px solid #cccccc; /*  */
    )");
    //  闪烁定时器配置（1秒间隔，循环触发）
    alarmBlinkTimer->setInterval(1000);  // 1秒=1000ms
    alarmBlinkTimer->setSingleShot(true); // 单次触发，读完再启动
    alarmBlinkTimer->stop(); // 初始停止（无报警时不运行）

    // 绑定闪烁超时槽函数
    connect(alarmBlinkTimer, &QTimer::timeout, this, &Widget::onAlarmBlinkTimerTimeout);

    // 标定模式按钮：初始白色背景+文字
    ui->calibModeBtn->setText("打开标定模式");
    ui->calibModeBtn->setStyleSheet(R"(
        QPushButton {
            background-color: white;
            color: black;
        }
    )");

    // 电机使能按钮：初始白色背景+文字
    ui->motorEnableBtn->setText("使能电机");
    ui->motorEnableBtn->setStyleSheet(R"(
        QPushButton {
            background-color: white;
            color: black;
        }
    )");


    // -------------------------- 原有信号槽连接 --------------------------
    connect(ui->baudRateBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Widget::checkCustomBaudRate);
    connect(serial, &QSerialPort::readyRead, this, &Widget::readData);
    connect(timer, &QTimer::timeout, this, &Widget::handleTimeout);
    // 连接定时器到接收函数
//    connect(canReceiveTimer, &QTimer::timeout, this, &Widget::receiveCanData);
//    canReceiveTimer->setInterval(20);          // 20ms 轮询间隔
//    canReceiveTimer->setSingleShot(false);     // 循环触发
    canReceiveTimer = new QTimer(this);
    canReceiveTimer->setInterval(10); // 20ms轮询
    canReceiveTimer->setSingleShot(false); // 固定为循环模式
    bool connRet = connect(canReceiveTimer, &QTimer::timeout, this, &Widget::receiveCanData);
    qDebug() << "定时器信号槽连接结果：" << connRet;
    canReceiveTimer->blockSignals(true);
    canReceiveTimer->stop();


   // -------------------------- 信号槽连接 --------------------------
   // 标定超时连接
   connect(calibTimer, &QTimer::timeout, this, &Widget::handleCalibTimeout);
   connect(ui->connectTabWidget, &QTabWidget::currentChanged,
           this, &Widget::onConnectTabChanged);
   onConnectTabChanged(ui->connectTabWidget->currentIndex());
   // 连接标签页切换信号
   if (ui->modeTabWidget) {
      connect(ui->modeTabWidget, &QTabWidget::currentChanged,
              this, &Widget::onModeTabChanged);
   }

   canReceiveTimer->blockSignals(true);
   // 定时更新电机状态
    //QTimer* statusTimer = new QTimer(this);
    //connect(statusTimer, &QTimer::timeout, this, &Widget::updateMotorStatus);
    //statusTimer->start(1000); // 每秒更新一次
    // 定时器配置
    timer->setInterval(2000);  // 升级超时2秒
    calibTimer->setSingleShot(true); // 标定超时单次触发

    if (ui->tabWidget) {
           connect(ui->tabWidget, &QTabWidget::currentChanged,
                   this, &Widget::onMainTabChanged);
       }

    // 串口配置初始化
    ui->baudRateBox->setInsertPolicy(QComboBox::InsertAtTop);
    checkCustomBaudRate(ui->baudRateBox->currentIndex());

//  QTimer::singleShot(100, this, &Widget::ensureConnectionPageVisible);
//  QTimer::singleShot(1000, this, [this]() {
//     qDebug() << "延迟初始化力位混合界面...";
//     initHybridPage();
//  });

    qDebug() << "Widget构造完成";
//  fillPortsInfo();
//  QTimer::singleShot(50, this, &Widget::refreshSerialPorts);
}

Widget::~Widget()
{
    // 释放顺序：关闭设备，释放对象
    if (canOpenStatus) {
        closeCanDevice();
    }
    if (OpenStatus) {
        serial->close();
    }
    if (firmwareFile && firmwareFile->isOpen()) {
        firmwareFile->close();
        delete firmwareFile;
    }

    if (commTabWidget) {
        delete commTabWidget;
    }

    delete m_plotwidget;

#ifdef QT_DEBUG
    ////delete m_tracer;
    ////delete m_tracerLabel;
    ///
    for(int i = 0; i< 5; i++)
        m_customPlot->removeGraph(i);
    delete m_customPlot;
#endif
    delete intValidator;
    delete canIdValidator;
    delete calibTimer;
    alarmBlinkTimer->stop(); // 停止定时器
    delete alarmBlinkTimer;  // 释放定时器对象
    delete motorIdValidator; // 释放ID验证器
    delete ui;
}

#ifdef QT_DEBUG
/* QCP绘图初始化 */
void Widget::initQCP()
{
    m_customPlot->setObjectName(QString::fromUtf8("customPlot"));                 // 设置QCustomPlot对象的对象名称
    QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);     // 创建一个QSizePolicy对象，用于控制customPlot的尺寸策略
    sizePolicy.setHorizontalStretch(0);         // 设置水平拉伸因子，0表示不拉伸
    sizePolicy.setVerticalStretch(0);           // 设置垂直拉伸因子，0表示不拉伸
    sizePolicy.setHeightForWidth(m_customPlot->sizePolicy().hasHeightForWidth()); // 设置高度与宽度的比例
    m_customPlot->setSizePolicy(sizePolicy);      // 应用sizePolicy到customPlot，影响customPlot在布局中调整大小的方式
    // 刻度显示
    m_customPlot->xAxis->setTicks(true);
    m_customPlot->yAxis->setTicks(true);
    // 刻度值显示
    m_customPlot->xAxis->setTickLabels(true);
    m_customPlot->yAxis->setTickLabels(true);
    // 网格显示
    m_customPlot->xAxis->grid()->setVisible(true);
    m_customPlot->yAxis->grid()->setVisible(true);
    // 子网格显示
    m_customPlot->xAxis->grid()->setSubGridVisible(false);
    m_customPlot->yAxis->grid()->setSubGridVisible(false);
    // 右侧和顶部坐标轴、刻度值显示
    m_customPlot->xAxis2->setVisible(true);
    m_customPlot->yAxis2->setVisible(true);
    m_customPlot->yAxis2->setTicks(true);         // 设置customPlot的第二个Y轴显示刻度线
    m_customPlot->yAxis2->setTickLabels(true);    // 设置customPlot的第二个Y轴显示刻度标签
    connect(m_customPlot->xAxis, SIGNAL(rangeChanged(QCPRange)), m_customPlot->xAxis2, SLOT(setRange(QCPRange)));   //同步修改轴的范围
    connect(m_customPlot->yAxis, SIGNAL(rangeChanged(QCPRange)), m_customPlot->yAxis2, SLOT(setRange(QCPRange)));
    // 暗色主题
    setPlotTheme(Qt::white, Qt::black);
    // 亮色主题
    //setPlotTheme(Qt::black, Qt::white);
    // 可放大缩小和移动
    QCPAxisRect qcpAxis(m_customPlot);
    qcpAxis.setRangeZoom(Qt::Orientation::Horizontal);
    m_customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectAxes);

    // x轴以时间形式显示
    QSharedPointer<QCPAxisTickerTime> timeTicker(new QCPAxisTickerTime);
    timeTicker->setTimeFormat("%m:%s:%z");      // 设置时间格式为分钟:秒:毫秒
    m_customPlot->xAxis->setTicker(timeTicker);   // 将自定义的时间刻度标签设置器应用到customPlot的x轴上
    m_customPlot->axisRect()->setupFullAxesBox(); // 设置轴矩形以显示所有轴的完整轴框
     // 为坐标轴添加标签
    m_customPlot->xAxis->setLabel("x");
    m_customPlot->yAxis->setLabel("y");
    // 默认坐标范围
    m_customPlot->yAxis->setRange(-250, 250);
    m_customPlot->replot();                       // 重新绘制customPlot，以应用新的设置

    //添加电机参数曲线
    initGraphName(CURRENTPOS, 0, Qt::red);
    initGraphName(RPMS, 1, Qt::magenta);
    initGraphName(PHASECURRENT, 2, Qt::green, true);
    initGraphName(MOTORTEMP, 3, Qt::yellow);
    initGraphName(MOSTEMP, 4, Qt::blue);

    //游标显示曲线上的坐标

}

/* 设置绘图主题 */
void Widget::setPlotTheme(QColor axis, QColor background)
{
    // 坐标标注颜色
    m_customPlot->xAxis->setLabelColor(axis);
    m_customPlot->yAxis->setLabelColor(axis);
    // 坐标刻度值颜色
    m_customPlot->xAxis->setTickLabelColor(axis);
    m_customPlot->yAxis->setTickLabelColor(axis);
    // 坐标基线颜色和宽度
    m_customPlot->xAxis->setBasePen(QPen(axis, 1));
    m_customPlot->yAxis->setBasePen(QPen(axis, 1));
    // 坐标主刻度颜色和宽度
    m_customPlot->xAxis->setTickPen(QPen(axis, 1));
    m_customPlot->yAxis->setTickPen(QPen(axis, 1));
    // 坐标子刻度颜色和宽度
    m_customPlot->xAxis->setSubTickPen(QPen(axis, 1));
    m_customPlot->yAxis->setSubTickPen(QPen(axis, 1));
    // 坐标标注颜色
    m_customPlot->xAxis2->setLabelColor(axis);
    m_customPlot->yAxis2->setLabelColor(axis);
    // 坐标刻度值颜色
    m_customPlot->xAxis2->setTickLabelColor(axis);
    m_customPlot->yAxis2->setTickLabelColor(axis);
    // 坐标基线颜色和宽度
    m_customPlot->xAxis2->setBasePen(QPen(axis, 1));
    m_customPlot->yAxis2->setBasePen(QPen(axis, 1));
    // 坐标主刻度颜色和宽度
    m_customPlot->xAxis2->setTickPen(QPen(axis, 1));
    m_customPlot->yAxis2->setTickPen(QPen(axis, 1));
    // 坐标子刻度颜色和宽度
    m_customPlot->xAxis2->setSubTickPen(QPen(axis, 1));
    m_customPlot->yAxis2->setSubTickPen(QPen(axis, 1));
    // 整个画布背景色
    m_customPlot->setBackground(background);
    // 绘图区域背景色
    m_customPlot->axisRect()->setBackground(background);
    // 刷新绘图
    m_customPlot->replot();
}

/* 绘图 */
void Widget::Plotting(QString name, double value)
{
    static QTime timeStart = QTime::currentTime();
    double key = timeStart.msecsTo(QTime::currentTime()) / m_nCurveDrawFrq;
    // 子网格显示
    //m_customPlot->xAxis->grid()->setSubGridVisible(true);
    //m_customPlot->yAxis->grid()->setSubGridVisible(true);

    // 自适应量程:根据数据的大小自动改变绘图坐标量程，使曲线的纵坐标全部显示在绘图区域；
    //m_customPlot->rescaleAxes();        //打开注释将开启自适应量程功能
    // 设置时间轴
    //使键轴的范围与数据滚动在恒定的范围(大小为timeAxis):
    int timeAxis = 10;
    if(key<=timeAxis)
    {
        m_customPlot->xAxis->setRange(timeAxis, timeAxis, Qt::AlignRight);    //数据从右往左,Qt::AlignRight设置 x 轴标签的对齐方式为右对齐
    }
    else
    {
        m_customPlot->xAxis->setRange(key, timeAxis, Qt::AlignRight);         //数据从右往左
    }
    // x轴和y轴全程显示:将曲线的横坐标和纵坐标全部显示在绘图区域；
    //m_customPlot->rescaleAxes();    //打开注释将开启全程显示功能
    // 更新曲线绘图
    m_customPlot->graph(m_nameToGraphMap[name])->addData(key, value);
    qDebug() << "Y:"<<value<< "X:"<<key;
    // 使用rpQueuedReplot参数可以加快绘图速度，避免不必要的重复绘制
    m_customPlot->replot(QCustomPlot::rpQueuedReplot);

    //游标显示曲线上的坐标
    /*
    //生成游标
    m_tracer = new QCPItemTracer(m_customPlot);   //生成游标
    m_tracer->setPen(QPen(Qt::red));              //圆圈轮廓颜色
    m_tracer->setBrush(QBrush(Qt::red));          //圆圈圈内颜色
    m_tracer->setStyle(QCPItemTracer::tsCircle);  //圆圈
    m_tracer->setSize(5);                         //设置大小

    //游标说明
    m_tracerLabel = new QCPItemText(m_customPlot);  //生成游标说明
    m_tracerLabel->setLayer("overlay");           //设置图层为overlay，因为需要频繁刷新
    m_tracerLabel->setPen(QPen(Qt::black));       //设置游标说明颜色
    m_tracerLabel->setPositionAlignment(Qt::AlignLeft | Qt::AlignTop);    //左上
    m_tracerLabel->position->setParentAnchor(m_tracer->position);           //将游标说明锚固在tracer位置处，实现自动跟随

    //信号-槽连接语句
    connect(m_customPlot, SIGNAL(mouseMove(QMouseEvent*)), this, SLOT(mouseMoveData(QMouseEvent*)));

    */


    // 计算和显示帧率（FPS），以及更新数据标签
/*
    // static double lastFpsKey = key;
    // static int frameCount = 0;
    // frameCount++;
    // // 每2秒显示一次帧率
    // if (key-lastFpsKey > 0.5)
    // {
    //     uint64_t sum = 0;
    //     for (int i = 0; i < m_customPlot->plottableCount(); i++)
    //     {
    //         sum += uint64_t(m_customPlot->graph(i)->data()->size());
    //     }
    //     ui->statusLabel->setText(QString("%1 FPS, Total Data points: %2").arg(frameCount/(key-lastFpsKey), 0, 'f', 0).arg(sum));
    //     lastFpsKey = key;
    //     frameCount = 0;
    //     // 更新数据标签
    //     for (int t = 0; t < m_customPlot->plottableCount(); t++)
    //     {
    //         valueLabelVector[t]->setText(QString::number(valueVector[t]));
    //     }
    // }

*/

}

/* 初始化曲线名称与颜色 */
void Widget::initGraphName(QString name, int index, QColor color, bool bVisable)
{
    // 添加曲线
    m_customPlot->addGraph();
    m_customPlot->graph(m_customPlot->graphCount()-1)->setPen(QPen(color));
    if(bVisable)
        m_customPlot->graph(m_customPlot->graphCount()-1)->setVisible(true);
    else
        m_customPlot->graph(m_customPlot->graphCount()-1)->setVisible(false);
    // 将曲线名称与曲线序号一一对应，之后添加数据就可以一一对应
    m_nameToGraphMap[name] = index;
}

void Widget::mouseMoveData(QMouseEvent *loc)
{
    //获得鼠标位置处对应的横坐标数据x
    double x = m_customPlot->xAxis->pixelToCoord(loc->pos().x());
    double y = m_customPlot->yAxis->pixelToCoord(loc->pos().y());
    double xValue, yValue;

    xValue = x; //xValue就是游标的横坐标
    yValue = y; //yValue就是游标的纵坐标

    m_tracer->position->setCoords(xValue, yValue);//设置游标位置
    m_tracerLabel->setText(QString("x = %1, y = %2").arg(xValue).arg(yValue));//设置游标说明内容
    m_customPlot->replot();//绘制器一定要重绘，否则看不到游标位置更新情况
}
#endif

void Widget::Publish_or_Debug_Ver()
{
#if 1
  ui->label_3->setVisible(false);
  ui->label_4->setVisible(false);
  ui->sendTextEdit->setVisible(false);
  ui->receiveTextEdit->setVisible(false);
  ui->groupBox_18->setVisible(false);
  ui->groupBox_20->setVisible(false);
  ui->groupBox_22->setVisible(false);
  ui->viewButton->setVisible(false);
#else
  ui->label_3->setVisible(true);
  ui->label_4->setVisible(true);
  ui->sendTextEdit->setVisible(true);
  ui->receiveTextEdit->setVisible(true);
  ui->groupBox_18->setVisible(true);
  ui->groupBox_20->setVisible(true);
  ui->groupBox_22->setVisible(true);
  ui->viewButton->setVisible(true);
#endif

  m_plotwidget->m_cd.bCurrentVisable = true;
  ui->groupBox_5->setVisible(false);
}

void Widget::getMotorCurve()
{
    // 连接状态变化信号和槽函数
    connect(ui->ckPos, &QCheckBox::toggled, this, &Widget::On_ckPosCheckchanged);
    connect(ui->RPM, &QCheckBox::toggled, this, &Widget::On_RPMCheckchanged);
    connect(ui->PhaseCurrent, &QCheckBox::toggled, this,&Widget::On_PhaseCurrentCheckchanged);
    connect(ui->MotorTemp, &QCheckBox::toggled, this, &Widget::On_MotorTempCheckchanged);
    connect(ui->DrvTemp, &QCheckBox::toggled, this, &Widget::On_DrvTempCheckchanged);
    connect(ui->MotorStatusDisplay, &QCheckBox::toggled, this, &Widget::On_MotorStatusChanged);

    connect(ui->rbBigDataRange, &QRadioButton::toggled, this, &Widget::On_rbBigDataRangeChanged);
    connect(ui->rbSmallDataRange, &QRadioButton::toggled, this, &Widget::On_rbSmallDataRangeChanged);

    connect(ui->pbExportExcel, &QPushButton::clicked, this, &Widget::On_pbExportExcel);
    connect(ui->pbSnapShot, &QPushButton::clicked, this, &Widget::On_pbSnapShot);
    connect(ui->pbPauseCapture, &QPushButton::clicked, this, &Widget::On_pbPauseCapture);

    connect(ui->pbApplyCurveSP, &QPushButton::clicked, this, &Widget::On_pbApplyCurveSP);
    connect(ui->pbCurveMode, &QPushButton::clicked, this, &Widget::On_pbCurveDisplay);

#ifdef QT_NO_DEBUG
    // 勾选X轴缩放
    connect(ui->cbXAxis, &QCheckBox::toggled, this, [=](bool checked){
        //if(checked) ui->cbXAxis->setChecked(false);
        m_plotwidget->setWheelScaleX(checked);
    });
    // 勾选Y轴缩放
    connect(ui->cbYAxis, &QCheckBox::toggled, this, [=](bool checked){
        //if(checked) ui->cbYAxis->setChecked(false);
        m_plotwidget->setWheelScaleY(checked);
    });
#else
    ui->groupBox_3->setVisible(false);
#endif
    //ui->pbCurveMode->setVisible(false);
    //ui->MotorTemp->setEnabled(false);
    //ui->DrvTemp->setEnabled(false);

    ui->gbCureveSelect->setVisible(false);
    ui->rbBigDataRange->setVisible(false);
    ui->rbSmallDataRange->setVisible(false);
}

void Widget::On_pbExportExcel()
{
    QString strPath,strFileName;
    static QTime time = QTime::currentTime();
    strFileName = time.toString("hh_mm_ss");
    strPath = QDir(QCoreApplication::applicationDirPath()).filePath("log");
    QDir dir(strPath);
    if(!dir.exists())
        dir.mkdir(".");
    strFileName = strPath + "/" + strFileName + ".csv";
    m_plotwidget->exportToCsv(strFileName);
}

void Widget::On_pbSnapShot()
{
    QString strPath,strFileName;
    static QTime time = QTime::currentTime();
    strFileName = time.toString("hh_mm_ss");
    strPath = QDir(QCoreApplication::applicationDirPath()).filePath("image");
    QDir dir(strPath);
    if(!dir.exists())
        dir.mkdir(".");
    strFileName = strPath + "/" + strFileName + ".png";

    // 保存图片
 #ifdef QT_DEBUG
    if(true == m_customPlot->savePng(strFileName, 500, 300, 1.0, -1, 255))
#else
    if(true == m_plotwidget->saveScreenshot(strFileName))
#endif
        QMessageBox::information(this, "提示", "曲线截图保存成功！");
    else
        QMessageBox::critical(this, "提示", "曲线截图保存失败！");
}

void Widget::On_pbApplyCurveSP()
{
    m_nCurveDrawFrq = ui->sbCurveSp->value();
}

void Widget::On_pbPauseCapture()
{
    if(!m_PauseCapture)
    {
        ui->pbPauseCapture->setText("暂停采集");
        m_PauseCapture = true;
    }else{
        ui->pbPauseCapture->setText("开始采集");
        m_PauseCapture = false;
    }
}

void Widget::On_pbCurveDisplay()
{
    if(m_CurveDisplayMode){
        ui->pbCurveMode->setText("拖动显示");
        ui->pbCurveMode->setStyleSheet(R"(
            QPushButton {
                background-color: #add8e6;
                color: black;
            }
        )");

        m_CurveDisplayMode =false ;

        // 开启：缩放后新数据自动滚动显示最新
        m_plotwidget->enableXAutoFollow(true);
    }
    else{
        ui->pbCurveMode->setText("实时显示");
        ui->pbCurveMode->setStyleSheet(R"(
            QPushButton {
                background-color: green;
                color: white;
            }
        )");

        m_CurveDisplayMode = true;

        // 关闭：缩放后固定视口，不自动滚动
        m_plotwidget->enableXAutoFollow(false);
    }
}

void Widget::On_rbBigDataRangeChanged(bool checked)
{
    if(checked)
    {
        ui->ckPos->setEnabled(true);
        ui->RPM->setEnabled(true);
        ui->PhaseCurrent->setEnabled(true);

        ui->MotorTemp->setChecked(false);
        ui->DrvTemp->setChecked(false);
    }
    else
    {
        ui->ckPos->setEnabled(false);
        ui->RPM->setEnabled(false);
        ui->PhaseCurrent->setEnabled(false);
    }
    m_plotwidget->m_yMin = -250.0f;
    m_plotwidget->m_yMax = 250.0f;
#ifdef QT_DBUG
    m_customPlot->yAxis->setRange(-250, 250);
#endif
}

void Widget::On_rbSmallDataRangeChanged(bool checked)
{
    if(checked)
    {
        ui->MotorTemp->setEnabled(true);
        ui->DrvTemp->setEnabled(true);

        ui->ckPos->setChecked(false);
        ui->RPM->setChecked(false);
        ui->PhaseCurrent->setChecked(false);
    }
    else
    {
        ui->MotorTemp->setEnabled(false);
        ui->DrvTemp->setEnabled(false);
    }
    m_plotwidget->m_yMin = -50.0f;
    m_plotwidget->m_yMax = 50.0f;
#ifdef QT_DBUG
    m_customPlot->yAxis->setRange(-50, 50);
#endif
}

void Widget::On_ckPosCheckchanged(bool checked)
{
    if(checked)
    {
        m_plotwidget->m_cd.bPosVisable = true;
#ifdef QT_DBUG
        m_customPlot->graph(m_nameToGraphMap[CURRENTPOS])->setVisible(true);
#endif
    }
    else
    {
        m_plotwidget->m_cd.bPosVisable = false;
#ifdef QT_DBUG
        m_customPlot->graph(m_nameToGraphMap[CURRENTPOS])->setVisible(false);
#endif
    }
    m_plotwidget->m_yMin = -50.0f;
    m_plotwidget->m_yMax = 50.0f;
#ifdef QT_DBUG
    m_customPlot->yAxis->setRange(-50, 50);
#endif
}

void Widget::On_RPMCheckchanged(bool checked)
{
    if(checked)
    {
        m_plotwidget->m_cd.bRPMVisable = true;
#ifdef QT_DBUG
        m_customPlot->graph(m_nameToGraphMap[RPMS])->setVisible(true);
#endif
    }else{
      m_plotwidget->m_cd.bRPMVisable = false;
#ifdef QT_DBUG
      m_customPlot->graph(m_nameToGraphMap[RPMS])->setVisible(false);
#endif
    }
    m_plotwidget->m_yMin = -100.0f;
    m_plotwidget->m_yMax = 100.0f;
#ifdef QT_DBUG
    m_customPlot->yAxis->setRange(-100, 100);
#endif
}

void Widget::On_PhaseCurrentCheckchanged(bool checked)
{
    if(checked)
    {
        m_plotwidget->m_cd.bCurrentVisable = true;
#ifdef QT_DBUG
        m_customPlot->graph(m_nameToGraphMap[PHASECURRENT])->setVisible(true);
#endif
    }else{
      m_plotwidget->m_cd.bCurrentVisable = false;
#ifdef QT_DBUG
      m_customPlot->graph(m_nameToGraphMap[PHASECURRENT])->setVisible(false);
#endif
    }
    m_plotwidget->m_yMin = -100.0f;
    m_plotwidget->m_yMax = 100.0f;
#ifdef QT_DBUG
    m_customPlot->yAxis->setRange(-100, 100);
#endif
}

void Widget::On_MotorTempCheckchanged(bool checked)
{
    if(checked)
    {
        m_plotwidget->m_cd.bMotorTempVisable = true;
#ifdef QT_DBUG
        m_customPlot->graph(m_nameToGraphMap[MOTORTEMP])->setVisible(true);
#endif
    }
    else
    {
        m_plotwidget->m_cd.bMotorTempVisable = false;
#ifdef QT_DBUG
        m_customPlot->graph(m_nameToGraphMap[MOTORTEMP])->setVisible(false);
#endif
    }
    m_plotwidget->m_yMin = -250.0f;
    m_plotwidget->m_yMax = 250.0f;
#ifdef QT_DBUG
    m_customPlot->yAxis->setRange(-250, 250);
#endif
}

void Widget::On_DrvTempCheckchanged(bool checked)
{
    if(checked)
    {
        m_plotwidget->m_cd.bDrvTempVisable = true;
#ifdef QT_DBUG
        m_customPlot->graph(m_nameToGraphMap[MOSTEMP])->setVisible(true);
#endif
    }
    else
    {
        m_plotwidget->m_cd.bDrvTempVisable = false;
#ifdef QT_DBUG
        m_customPlot->graph(m_nameToGraphMap[MOSTEMP])->setVisible(false);
#endif
    }
    m_plotwidget->m_yMin = -150.0f;
    m_plotwidget->m_yMax = 150.0f;
#ifdef QT_DBUG
    m_customPlot->yAxis->setRange(-150, 150);
#endif
}

void Widget::On_MotorStatusChanged(bool checked)
{
    if(checked)
        m_displayMotorStatus = true;
    else
        m_displayMotorStatus = false;
}

void Widget::initSerialConfigGroup()
{
    if (!ui->serialConfigGroup) {
        qDebug() << "错误: serialConfigGroup为空";
        return;
    }

    qDebug() << "初始化串口配置组...";

    ui->portBox = ui->serialConfigGroup->findChild<QComboBox*>("portBox");
    if (ui->portBox) {
        qDebug() << "找到portBox，初始化端口列表";
        refreshSerialPorts(); // 刷新串口列表
    } else {
        qDebug() << "警告: 未找到portBox";
        ui->openButton->setEnabled(false); // 无端口选择框时禁用打开按钮
    }

    // 查找并初始化波特率下拉框
    ui->baudRateBox = ui->serialConfigGroup->findChild<QComboBox*>("baudRateBox");
    if (ui->baudRateBox) {
        qDebug() << "找到baudRateBox";
        ui->baudRateBox->clear();
        ui->baudRateBox->addItem("9600");
        ui->baudRateBox->addItem("19200");
        ui->baudRateBox->addItem("38400");
        ui->baudRateBox->addItem("57600");
        ui->baudRateBox->addItem("115200");
        ui->baudRateBox->setCurrentText("115200");
    } else {
        qDebug() << "警告: 未找到baudRateBox";
    }

    // 查找并初始化数据位下拉框
    ui->dataBitsBox = ui->serialConfigGroup->findChild<QComboBox*>("dataBitsBox");
    if (ui->dataBitsBox) {
        qDebug() << "找到dataBitsBox";
        ui->dataBitsBox->clear();
        ui->dataBitsBox->addItem("5");
        ui->dataBitsBox->addItem("6");
        ui->dataBitsBox->addItem("7");
        ui->dataBitsBox->addItem("8");
        ui->dataBitsBox->setCurrentIndex(3); // 默认为8
    } else {
        qDebug() << "警告: 未找到dataBitsBox";
    }

    // 查找并初始化停止位下拉框
    ui->stopBitsBox = ui->serialConfigGroup->findChild<QComboBox*>("stopBitsBox");
    if (ui->stopBitsBox) {
        qDebug() << "找到stopBitsBox";
        ui->stopBitsBox->clear();
        ui->stopBitsBox->addItem("1");
        ui->stopBitsBox->addItem("1.5");
        ui->stopBitsBox->addItem("2");
        ui->stopBitsBox->setCurrentIndex(0); // 默认为1
    } else {
        qDebug() << "警告: 未找到stopBitsBox";
    }

    // 查找并初始化校验位下拉框
    ui->parityBox = ui->serialConfigGroup->findChild<QComboBox*>("parityBox");
    if (ui->parityBox) {
        qDebug() << "找到parityBox";
        ui->parityBox->clear();
        ui->parityBox->addItem("None");
        ui->parityBox->addItem("Even");
        ui->parityBox->addItem("Odd");
        ui->parityBox->setCurrentIndex(0); // 默认为None
    } else {
        qDebug() << "警告: 未找到parityBox";
    }

    // 查找并连接刷新按钮
    ui->refreshButton = ui->serialConfigGroup->findChild<QPushButton*>("refreshButton");
    if (ui->refreshButton) {
        qDebug() << "找到refreshButton，连接信号";
        connect(ui->refreshButton, &QPushButton::clicked, this, &Widget::on_refreshButton_clicked);
    } else {
        qDebug() << "警告: 未找到refreshButton";
    }

    // 查找并连接打开串口按钮
    ui->openButton = ui->serialConfigGroup->findChild<QPushButton*>("openButton");
    if (ui->openButton) {
        qDebug() << "找到openButton，连接信号";
        connect(ui->openButton, &QPushButton::clicked, this, &Widget::on_openButton_clicked);
    } else {
        qDebug() << "警告: 未找到openButton";
    }

    // 查找并连接关闭串口按钮
    QPushButton* closeButton = ui->serialConfigGroup->findChild<QPushButton*>("closeButton");
    if (closeButton) {
        qDebug() << "找到closeButton，连接信号";
        connect(closeButton, &QPushButton::clicked, this, [this]() {
            this->closeSerialDevice();
        });
    } else {
        qDebug() << "警告: 未找到closeButton";
    }
}

void Widget::refreshSerialPorts()
{
    qDebug() << "刷新串口列表...";
        if (!ui->portBox) {
            qDebug() << "错误: portBox为空";
            return;
        }
        QString currentPort = ui->portBox->currentData().toString();
        ui->portBox->clear();
        QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();

        foreach (const QSerialPortInfo &info, ports) {

            QSerialPort tempPort(info);
            if (tempPort.open(QIODevice::ReadWrite)) {
                tempPort.close();
                QString displayText = info.portName();
                ui->portBox->addItem(displayText, info.portName());
                qDebug() << "找到可用串口:" << displayText;
            } else {
                qDebug() << "跳过不可用串口:" << info.portName() << " 原因:" << tempPort.errorString();
            }
        }

        if (ui->portBox->count() == 0) {
            ui->portBox->addItem("无可用串口");
            ui->portBox->setEnabled(false);
            ui->openButton->setEnabled(false);
        } else {
            ui->portBox->setEnabled(true);
            ui->openButton->setEnabled(true);
            int index = ui->portBox->findData(currentPort);
            if (index >= 0) ui->portBox->setCurrentIndex(index);
            else ui->portBox->setCurrentIndex(0);
        }
        qDebug() << "串口列表刷新完成，可用串口数:" << ui->portBox->count();
}

void Widget::onConnectTabChanged(int index)
{
    qDebug() << "连接选项卡切换: 索引 =" << index;

        if (index == 0) { // CAN选项卡
            currentCommMode = CAN_MODE;
            qDebug() << "切换到CAN通信模式";

//            // 显示CAN配置，隐藏串口配置
//            if (ui->canConfigGroup) {
//                ui->canConfigGroup->setVisible(true);
//            }
//            if (ui->serialConfigGroup) {
//                ui->serialConfigGroup->setVisible(false);
//            }

        } else if (index == 1) { // 串口选项卡
            currentCommMode = SERIAL_MODE;
            qDebug() << "切换到串口通信模式";

//            // 显示串口配置，隐藏CAN配置
//            if (ui->serialConfigGroup) {
//                ui->serialConfigGroup->setVisible(true);
//            }
//            if (ui->canConfigGroup) {
//                ui->canConfigGroup->setVisible(false);
//            }
        }

        // 清空接收缓存
        recvBuffer.clear();

        // 重置升级状态
        resetUpgradeState();
        ui->startUpgradeButton->setEnabled(false);
        ui->viewButton->setEnabled(false);
}

void Widget::ensureConnectionPageVisible()
{
    qDebug() << "确保连接页面控件可见...";

    if (ui->canConfigGroup) {
        ui->canConfigGroup->setVisible(true);
        ui->canConfigGroup->setEnabled(true);

        QList<QWidget*> canChildren = ui->canConfigGroup->findChildren<QWidget*>();
        for (QWidget* child : canChildren) {
            child->setVisible(true);
            child->setEnabled(true);
        }
    }

    if (ui->serialConfigGroup) {
        ui->serialConfigGroup->setVisible(true);
        ui->serialConfigGroup->setEnabled(true);

        QList<QWidget*> serialChildren = ui->serialConfigGroup->findChildren<QWidget*>();
        for (QWidget* child : serialChildren) {
            child->setVisible(true);
            child->setEnabled(true);
        }
    }
}

void Widget::onMainTabChanged(int index)
{
    QWidget* currentPage = ui->tabWidget ? ui->tabWidget->widget(index) : nullptr;
    if (!currentPage) return;

    QString pageName = currentPage->objectName();
    QString tabText = ui->tabWidget->tabText(index);

    qDebug() << "切换到主标签页: 索引=" << index
             << "，对象名=" << pageName
             << "，标签文本=" << tabText;

    if (pageName.contains("Connect") || tabText.contains("连接")) {
        qDebug() << "进入连接界面";
    }
    else if (pageName.contains("Calibration") || tabText.contains("标定")) {
        qDebug() << "进入标定界面";
    }
    else if (pageName.contains("Control") || tabText.contains("控制参数")) {
        qDebug() << "进入控制参数界面";
    }
    else if (pageName.contains("MotorControl") || tabText.contains("电机控制")) {
        qDebug() << "进入电机控制界面";
        // 切换到电机控制界面时，默认显示伺服位置模式
        if (ui->modeTabWidget) {
            ui->modeTabWidget->setCurrentIndex(0); // 默认显示第一个模式
        }
    }
    else if (pageName.contains("curve")){
        qDebug() << "进入曲线界面";
    }
}

void Widget::delayedInitHybridPage()
{
    qDebug() << "延迟初始化力位混合界面控件";
    initHybridPage();

    if (ui->tabWidget->currentWidget() == ui->pageHybrid) {
        on_tabWidget_currentChanged(ui->tabWidget->currentIndex());
    }
}

void Widget::on_tabWidget_currentChanged(int index)
{
    QWidget *currentWidget = ui->tabWidget->widget(index);
    if (!currentWidget) return;

    QString pageName = currentWidget->objectName();
    qDebug() << "切换到页面:" << pageName << "索引:" << index;

    if (pageName == "pageHybrid") {
        qDebug() << "切换到力位混合控制页面";

        // 确保力位混合界面已初始化
        if (!hybridPageInitialized) {
            initHybridPage();
        }

        // 更新电机状态显示
        updateMotorStatus();

    } else if (pageName == "pageUpgrade") {
        qDebug() << "切换到固件升级页面";
    } else if (pageName == "pageCalibration") {
        qDebug() << "切换到电机标定页面";
    } else if (pageName == "pageDebug") {
        qDebug() << "切换到调试页面";
    } else if (pageName == "pageControlParams") {
        qDebug() << "切换到控制参数页面";
    }
}

void Widget::initHybridPage()
{
    qDebug() << "开始初始化力位混合界面...";
    hybridPage = findHybridPage();
    if (!hybridPage) {
        qDebug() << "错误: 找不到力位混合页面";
        return;
    }

    findHybridControls();


    int foundCount = 0;
    if (hybridKpSpinBox && hybridKpSlider) foundCount++;
    if (hybridKdSpinBox && hybridKdSlider) foundCount++;
    if (hybridPosSpinBox && hybridPosSlider) foundCount++;
    if (hybridSpeedSpinBox && hybridSpeedSlider) foundCount++;
    if (hybridTorqueSpinBox && hybridTorqueSlider) foundCount++;

    if (foundCount == 0) {
        qDebug() << "错误: 未找到任何Slider/SpinBox控件，无法初始化";
        return;
    } else {
        qDebug() << "找到" << foundCount << "/5 对控件（标签可能部分缺失）";
    }
    readMitParamsFromControlPageOptimized();
    initHybridControlValues();
    updateHybridSliderLabels();
    blockHybridSignals(true);
    setupHybridConnections();
    syncSliderFromSpinBox();
    blockHybridSignals(false);

    qDebug() << "力位混合界面初始化完成";
}

void Widget::blockHybridSignals(bool block)
{
    if (hybridKpSpinBox) hybridKpSpinBox->blockSignals(block);
    if (hybridKpSlider) hybridKpSlider->blockSignals(block);
    if (hybridKdSpinBox) hybridKdSpinBox->blockSignals(block);
    if (hybridKdSlider) hybridKdSlider->blockSignals(block);
    if (hybridPosSpinBox) hybridPosSpinBox->blockSignals(block);
    if (hybridPosSlider) hybridPosSlider->blockSignals(block);
    if (hybridSpeedSpinBox) hybridSpeedSpinBox->blockSignals(block);
    if (hybridSpeedSlider) hybridSpeedSlider->blockSignals(block);
    if (hybridTorqueSpinBox) hybridTorqueSpinBox->blockSignals(block);
    if (hybridTorqueSlider) hybridTorqueSlider->blockSignals(block);
}

void Widget::readMitParamsFromControlPageOptimized()
{
    if (!ui->kpLimitMinEdit || !ui->kpLimitMaxEdit) {
            qDebug() << "控制参数界面控件未找到，使用解析后的mitParams值";
            return;
        }

        double kpMin = ui->kpLimitMinEdit->value();
        double kpMax = ui->kpLimitMaxEdit->value();
        if (kpMin != 0.0 || kpMax != 0.0) {  // 启动时均为0，不执行读取
            mitParams.kp_limit_min = static_cast<int32_t>(kpMin * 1000000);
            mitParams.kp_limit_max = static_cast<int32_t>(kpMax * 1000000);
        } else {
            ui->kpLimitMinEdit->setValue(0.0);
            ui->kpLimitMaxEdit->setValue(0.0);
        }
        double kdMin = ui->kdLimitMinEdit->value();
        double kdMax = ui->kdLimitMaxEdit->value();
        if (kdMin != 0.0 || kdMax != 0.0) {
            mitParams.kd_limit_min = static_cast<int32_t>(kdMin * 1000000);
            mitParams.kd_limit_max = static_cast<int32_t>(kdMax * 1000000);
        } else {
            ui->kdLimitMinEdit->setValue(0.0);
            ui->kdLimitMaxEdit->setValue(0.0);
        }

        double posMin = ui->posLimitMinEdit->value();
        double posMax = ui->posLimitMaxEdit->value();
        if (posMin != 0.0 || posMax != 0.0) {
            mitParams.pos_limit_min = static_cast<int32_t>(posMin * 1000000);
            mitParams.pos_limit_max = static_cast<int32_t>(posMax * 1000000);
        } else {
            ui->posLimitMinEdit->setValue(0.0);
            ui->posLimitMaxEdit->setValue(0.0);
        }

        double speedMin = ui->speedLimitMinEdit->value();
        double speedMax = ui->speedLimitMaxEdit->value();
        if (speedMin != 0.0 || speedMax != 0.0) {
            mitParams.speed_limit_min = static_cast<int32_t>(speedMin * 1000000);
            mitParams.speed_limit_max = static_cast<int32_t>(speedMax * 1000000);
        } else {
            ui->speedLimitMinEdit->setValue(0.0);
            ui->speedLimitMaxEdit->setValue(0.0);
        }

        double torqueMin = ui->torqueLimitMinEdit->value();
        double torqueMax = ui->torqueLimitMaxEdit->value();
        if (torqueMin != 0.0 || torqueMax != 0.0) {
            mitParams.torque_limit_min = static_cast<int32_t>(torqueMin * 1000000);
            mitParams.torque_limit_max = static_cast<int32_t>(torqueMax * 1000000);
        } else {
            ui->torqueLimitMinEdit->setValue(0.0);
            ui->torqueLimitMaxEdit->setValue(0.0);
        }

        qDebug() << "优化后读取控制参数界面MIT参数完成（未修改则保持0值）";
}

QWidget* Widget::findHybridPage()
{
    qDebug() << "开始查找力位混合页面...";


    QWidget* page = findChild<QWidget*>("pageHybrid");
    if (page) {
        qDebug() << "成功: 通过对象名称找到pageHybrid";
        return page;
    }
    return nullptr;
}

void Widget::initHybridControlValues()
{
    qDebug() << "初始化力位混合控件值...";
    // 从MIT参数获取限制值（缩放1e6）
    double kpMin = mitParams.kp_limit_min / 1000000.0;
    double kpMax = mitParams.kp_limit_max / 1000000.0;
    double kdMin = mitParams.kd_limit_min / 1000000.0;
    double kdMax = mitParams.kd_limit_max / 1000000.0;
    double posMin = mitParams.pos_limit_min / 1000000.0;
    double posMax = mitParams.pos_limit_max / 1000000.0;
    double speedMin = mitParams.speed_limit_min / 1000000.0;
    double speedMax = mitParams.speed_limit_max / 1000000.0;
    double torqueMin = mitParams.torque_limit_min / 1000000.0;
    double torqueMax = mitParams.torque_limit_max / 1000000.0;


    if (kpMin >= kpMax) { kpMin = 0.0; kpMax = 10.0; }
    if (kdMin >= kdMax) { kdMin = 0.0; kdMax = 10.0; }
    if (posMin >= posMax) { posMin = 0.0; posMax = 100.0; }
    if (speedMin >= speedMax) { speedMin = 0.0; speedMax = 1000.0; }
    if (torqueMin >= torqueMax) { torqueMin = 0.0; torqueMax = 50.0; }


    //  Kp默认值逻辑
    double kpDefault = 0.0; // 默认初始为0
    if (mitParams.kp_limit_min != 0 || mitParams.kp_limit_max != 0) {
        if (kpMin > 0 && kpMax > 0) {
            kpDefault = kpMin;
        } else if (kpMin < 0 && kpMax > 0) {
            kpDefault = 0.0;
        } else {
            kpDefault = kpMin;
        }
    }

    // Kd默认值逻辑（同Kp）
    double kdDefault = 0.0;
    if (mitParams.kd_limit_min != 0 || mitParams.kd_limit_max != 0) {
        if (kdMin > 0 && kdMax > 0) {
            kdDefault = kdMin;
        } else if (kdMin < 0 && kdMax > 0) {
            kdDefault = 0.0;
        } else {
            kdDefault = kdMin;
        }
    }

    // 位置默认值逻辑（同Kp）
    double posDefault = 0.0;
    if (mitParams.pos_limit_min != 0 || mitParams.pos_limit_max != 0) {
        if (posMin > 0 && posMax > 0) {
            posDefault = posMin;
        } else if (posMin < 0 && posMax > 0) {
            posDefault = 0.0;
        } else {
            posDefault = posMin;
        }
    }

    // 速度默认值逻辑（同Kp）
    double speedDefault = 0.0;
    if (mitParams.speed_limit_min != 0 || mitParams.speed_limit_max != 0) {
        if (speedMin > 0 && speedMax > 0) {
            speedDefault = speedMin;
        } else if (speedMin < 0 && speedMax > 0) {
            speedDefault = 0.0;
        } else {
            speedDefault = speedMin;
        }
    }

    // 力矩默认值逻辑（同Kp）
    double torqueDefault = 0.0;
    if (mitParams.torque_limit_min != 0 || mitParams.torque_limit_max != 0) {
        if (torqueMin > 0 && torqueMax > 0) {
            torqueDefault = torqueMin;
        } else if (torqueMin < 0 && torqueMax > 0) {
            torqueDefault = 0.0;
        } else {
            torqueDefault = torqueMin;
        }
    }


    // Kp：范围min~max，
    if (hybridKpSlider && hybridKpSpinBox) {
        hybridKpSlider->setRange(0, 1000); // 整数范围映射
        hybridKpSpinBox->setRange(kpMin, kpMax);
        hybridKpSpinBox->setValue(kpDefault); // 应用默认值

        int sliderInitValue = 0;
        if (kpMax > kpMin) {
            sliderInitValue = qRound((kpDefault - kpMin) / (kpMax - kpMin) * 1000);
        }
        hybridKpSlider->blockSignals(true);
        hybridKpSlider->setValue(sliderInitValue);
        hybridKpSlider->blockSignals(false);
    }

    // Kd：同上
    if (hybridKdSlider && hybridKdSpinBox) {
        hybridKdSlider->setRange(0, 1000);
        hybridKdSpinBox->setRange(kdMin, kdMax);
        hybridKdSpinBox->setValue(kdDefault);
        int sliderInitValue = 0;
        if (kdMax > kdMin) {
            sliderInitValue = qRound((kdDefault - kdMin) / (kdMax - kdMin) * 1000);
        }
        hybridKdSlider->blockSignals(true);
        hybridKdSlider->setValue(sliderInitValue);
        hybridKdSlider->blockSignals(false);
    }

    // 位置：同上
    if (hybridPosSlider && hybridPosSpinBox) {
        hybridPosSlider->setRange(0, 1000);
        hybridPosSpinBox->setRange(posMin, posMax);
        hybridPosSpinBox->setValue(posDefault);
        int sliderInitValue = 0;
        if (posMax > posMin) {
            sliderInitValue = qRound((posDefault - posMin) / (posMax - posMin) * 1000);
        }
        hybridPosSlider->blockSignals(true);
        hybridPosSlider->setValue(sliderInitValue);
        hybridPosSlider->blockSignals(false);
    }

    // 速度：同上
    if (hybridSpeedSlider && hybridSpeedSpinBox) {
        hybridSpeedSlider->setRange(0, 1000);
        hybridSpeedSpinBox->setRange(speedMin, speedMax);
        hybridSpeedSpinBox->setValue(speedDefault);
        int sliderInitValue = 0;
        if (speedMax > speedMin) {
            sliderInitValue = qRound((speedDefault - speedMin) / (speedMax - speedMin) * 1000);
        }
        hybridSpeedSlider->blockSignals(true);
        hybridSpeedSlider->setValue(sliderInitValue);
        hybridSpeedSlider->blockSignals(false);
    }

    // 力矩：同上
    if (hybridTorqueSlider && hybridTorqueSpinBox) {
        hybridTorqueSlider->setRange(0, 1000);
        hybridTorqueSpinBox->setRange(torqueMin, torqueMax);
        hybridTorqueSpinBox->setValue(torqueDefault);
        int sliderInitValue = 0;
        if (torqueMax > torqueMin) {
            sliderInitValue = qRound((torqueDefault - torqueMin) / (torqueMax - torqueMin) * 1000);
        }
        hybridTorqueSlider->blockSignals(true);
        hybridTorqueSlider->setValue(sliderInitValue);
        hybridTorqueSlider->blockSignals(false);
    }

    qDebug() << "力位混合控件值初始化完成（按规则设置默认值）";
}

void Widget::findHybridControls()
{
    if (!hybridPage) {
            qDebug() << "错误: hybridPage为空";
            return;
        }

        QList<QDoubleSpinBox*> allSpinBoxes = hybridPage->findChildren<QDoubleSpinBox*>();
        for (QDoubleSpinBox* spinBox : allSpinBoxes) {
            QString name = spinBox->objectName();
            if (name == "mitKpEdit") hybridKpSpinBox = spinBox;
            else if (name == "mitKdEdit") hybridKdSpinBox = spinBox;
            else if (name == "targetPosEdit") hybridPosSpinBox = spinBox;
            else if (name == "targetSpeedEdit") hybridSpeedSpinBox = spinBox;
            else if (name == "feedforwardTorqueEdit") hybridTorqueSpinBox = spinBox;
        }


        QList<QSlider*> allQSliders = hybridPage->findChildren<QSlider*>();
        for (QSlider* slider : allQSliders) {
            QString name = slider->objectName();

            DoubleSlider* doubleSlider = qobject_cast<DoubleSlider*>(slider);
            if (doubleSlider) {

                if (name == "mitKpSlider") hybridKpSlider = doubleSlider;
                else if (name == "mitKdSlider") hybridKdSlider = doubleSlider;
                else if (name == "targetPosSlider") hybridPosSlider = doubleSlider;
                else if (name == "targetSpeedSlider") hybridSpeedSlider = doubleSlider;
                else if (name == "feedforwardTorqueSlider") hybridTorqueSlider = doubleSlider;
            } else {

                qDebug() << "警告：控件" << name << "不是DoubleSlider类型，强制转换";
                if (name == "mitKpSlider") hybridKpSlider = reinterpret_cast<DoubleSlider*>(slider);
                else if (name == "mitKdSlider") hybridKdSlider = reinterpret_cast<DoubleSlider*>(slider);
                else if (name == "targetPosSlider") hybridPosSlider = reinterpret_cast<DoubleSlider*>(slider);
                else if (name == "targetSpeedSlider") hybridSpeedSlider = reinterpret_cast<DoubleSlider*>(slider);
                else if (name == "feedforwardTorqueSlider") hybridTorqueSlider = reinterpret_cast<DoubleSlider*>(slider);
            }
        }


        QList<QLabel*> allLabels = hybridPage->findChildren<QLabel*>();
        for (QLabel* label : allLabels) {
            QString name = label->objectName();
            if (name == "kp_min") hybridKpMinLabel = label;
            else if (name == "kp_max") hybridKpMaxLabel = label;
            else if (name == "kd_min") hybridKdMinLabel = label;
            else if (name == "kd_max") hybridKdMaxLabel = label;
            else if (name == "pos_min") hybridPosMinLabel = label;
            else if (name == "pos_max") hybridPosMaxLabel = label;
            else if (name == "spd_min") hybridSpeedMinLabel = label;
            else if (name == "spd_max") hybridSpeedMaxLabel = label;
            else if (name == "Torque_min") hybridTorqueMinLabel = label;
            else if (name == "Torque_max") hybridTorqueMaxLabel = label;
        }

        // 打印控件查找结果（调试用）
        qDebug() << "控件查找结果："
                 << "KpSlider=" << (hybridKpSlider ? "找到" : "未找到")
                 << "KpMinLabel=" << (hybridKpMinLabel ? "找到" : "未找到")
                 << "KpSpinBox=" << (hybridKpSpinBox ? "找到" : "未找到");
    }

void Widget::delayedHybridInit()
{
    qDebug() << "延迟初始化力位混合界面...";
    initHybridPage(); // 重新尝试初始化
}

void Widget::forceSetLabelTexts()
{
        // Kp参数
        if (mitParams.kp_limit_min != 0 || mitParams.kp_limit_max != 0) {
            if (hybridKpMinLabel) {
                QString text = QString::number(mitParams.kp_limit_min / 1000000.0, 'f', 2);
                hybridKpMinLabel->setText(text);
            }
            if (hybridKpMaxLabel) {
                QString text = QString::number(mitParams.kp_limit_max / 1000000.0, 'f', 2);
                hybridKpMaxLabel->setText(text);
            }
        }
        // Kd参数
        if (mitParams.kd_limit_min != 0 || mitParams.kd_limit_max != 0) {
            if (hybridKdMinLabel) {
                QString text = QString::number(mitParams.kd_limit_min / 1000000.0, 'f', 2);
                hybridKdMinLabel->setText(text);
            }
            if (hybridKdMaxLabel) {
                QString text = QString::number(mitParams.kd_limit_max / 1000000.0, 'f', 2);
                hybridKdMaxLabel->setText(text);
            }
        }
        // 位置参数
        if (mitParams.pos_limit_min != 0 || mitParams.pos_limit_max != 0) {
            if (hybridPosMinLabel) {
                QString text = QString::number(mitParams.pos_limit_min / 1000000.0, 'f', 2);
                hybridPosMinLabel->setText(text);
            }
            if (hybridPosMaxLabel) {
                QString text = QString::number(mitParams.pos_limit_max / 1000000.0, 'f', 2);
                hybridPosMaxLabel->setText(text);
            }
        }
        // 速度参数
        if (mitParams.speed_limit_min != 0 || mitParams.speed_limit_max != 0) {
            if (hybridSpeedMinLabel) {
                QString text = QString::number(mitParams.speed_limit_min / 1000000.0, 'f', 2);
                hybridSpeedMinLabel->setText(text);
            }
            if (hybridSpeedMaxLabel) {
                QString text = QString::number(mitParams.speed_limit_max / 1000000.0, 'f', 2);
                hybridSpeedMaxLabel->setText(text);
            }
        }
        // 力矩参数
        if (mitParams.torque_limit_min != 0 || mitParams.torque_limit_max != 0) {
            if (hybridTorqueMinLabel) {
                QString text = QString::number(mitParams.torque_limit_min / 1000000.0, 'f', 2);
                hybridTorqueMinLabel->setText(text);
            }
            if (hybridTorqueMaxLabel) {
                QString text = QString::number(mitParams.torque_limit_max / 1000000.0, 'f', 2);
                hybridTorqueMaxLabel->setText(text);
            }
        }
        qDebug() << "强制设置标签文本完成（仅非0参数执行）";
}

void Widget::debugHybridPage()
{
    QWidget* hybridPage = ui->modeTabWidget->findChild<QWidget*>("pageHybrid");
    if (!hybridPage) {
        qDebug() << "调试：无法找到pageHybrid";
        return;
    }

    qDebug() << "=== 调试：pageHybrid中的所有控件 ===";

    QList<QLabel*> allLabels = hybridPage->findChildren<QLabel*>();
    qDebug() << "找到" << allLabels.size() << "个标签:";
    for (QLabel* label : allLabels) {
        qDebug() << "  对象名:" << label->objectName()
                 << " 文本:" << label->text();
    }

    QList<QSpinBox*> allSpinBoxes = hybridPage->findChildren<QSpinBox*>();
    qDebug() << "找到" << allSpinBoxes.size() << "个SpinBox:";
    for (QSpinBox* spinBox : allSpinBoxes) {
        qDebug() << "  对象名:" << spinBox->objectName();
    }

    QList<QSlider*> allSliders = hybridPage->findChildren<QSlider*>();
    qDebug() << "找到" << allSliders.size() << "个Slider:";
    for (QSlider* slider : allSliders) {
        qDebug() << "  对象名:" << slider->objectName();
    }

    qDebug() << "=== 调试结束 ===";
}

void Widget::onKpSliderValueChanged(double value)
{
    qDebug() << "Kp DoubleSlider 值改变:" << value;
    if (hybridKpSpinBox) {
        hybridKpSpinBox->blockSignals(true);
        hybridKpSpinBox->setValue(value);
        hybridKpSpinBox->blockSignals(false);
    }
}

void Widget::onKdSliderValueChanged(double value)
{
    qDebug() << "Kd DoubleSlider 值改变:" << value;
    if (hybridKdSpinBox) {
        hybridKdSpinBox->blockSignals(true);
        hybridKdSpinBox->setValue(value);
        hybridKdSpinBox->blockSignals(false);
    }
}

void Widget::onPosSliderValueChanged(double value)
{
    qDebug() << "Pos DoubleSlider 值改变:" << value;
    if (hybridPosSpinBox) {
        hybridPosSpinBox->blockSignals(true);
        hybridPosSpinBox->setValue(value);
        hybridPosSpinBox->blockSignals(false);
    }
}

void Widget::onSpeedSliderValueChanged(double value)
{
    qDebug() << "Speed DoubleSlider 值改变:" << value;
    if (hybridSpeedSpinBox) {
        hybridSpeedSpinBox->blockSignals(true);
        hybridSpeedSpinBox->setValue(value);
        hybridSpeedSpinBox->blockSignals(false);
    }
}

void Widget::onTorqueSliderValueChanged(double value)
{
    qDebug() << "Torque DoubleSlider 值改变:" << value;
    if (hybridTorqueSpinBox) {
        hybridTorqueSpinBox->blockSignals(true);
        hybridTorqueSpinBox->setValue(value);
        hybridTorqueSpinBox->blockSignals(false);
    }
}

void Widget::setupHybridConnections()
{
    qDebug() << "设置力位混合连接...";
       int successCount = 0;

       if (hybridKpSlider && hybridKpSpinBox) {
           qDebug() << "连接Kp控件...";
          connect(hybridKpSlider, &QSlider::valueChanged, this, [this](int sliderValue) {
               if (!hybridKpSpinBox || !hybridKpSlider) return;
               double min = hybridKpSpinBox->minimum();
               double max = hybridKpSpinBox->maximum();
               double spinValue = min + (sliderValue / 1000.0) * (max - min);
               hybridKpSpinBox->blockSignals(true);
               hybridKpSpinBox->setValue(spinValue);
               hybridKpSpinBox->blockSignals(false);
               qDebug() << "Kp滑块值改变:" << sliderValue << "->" << spinValue;
           });

           connect(hybridKpSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double spinValue) {
               if (!hybridKpSpinBox || !hybridKpSlider) return;
               double min = hybridKpSpinBox->minimum();
               double max = hybridKpSpinBox->maximum();
               int sliderValue = 0;
               if (max > min) {
                   double ratio = (spinValue - min) / (max - min);
                   sliderValue = qRound(ratio * 1000);
               }
               hybridKpSlider->blockSignals(true);
               hybridKpSlider->setValue(sliderValue);
               hybridKpSlider->blockSignals(false);
               qDebug() << "KpSpinBox值改变:" << spinValue << "->" << sliderValue;
           });
           successCount++;
       }

       if (hybridKdSlider && hybridKdSpinBox) {
           qDebug() << "连接Kd控件...";
           connect(hybridKdSlider, &QSlider::valueChanged, this, [this](int sliderValue) {
               if (!hybridKdSpinBox || !hybridKdSlider) return;
               double min = hybridKdSpinBox->minimum();
               double max = hybridKdSpinBox->maximum();
               double spinValue = min + (sliderValue / 1000.0) * (max - min);
               hybridKdSpinBox->blockSignals(true);
               hybridKdSpinBox->setValue(spinValue);
               hybridKdSpinBox->blockSignals(false);
               qDebug() << "Kd滑块值改变:" << sliderValue << "->" << spinValue;
           });
           connect(hybridKdSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double spinValue) {
               if (!hybridKdSpinBox || !hybridKdSlider) return;
               double min = hybridKdSpinBox->minimum();
               double max = hybridKdSpinBox->maximum();
               int sliderValue = 0;
               if (max > min) {
                   double ratio = (spinValue - min) / (max - min);
                   sliderValue = qRound(ratio * 1000);
               }
               hybridKdSlider->blockSignals(true);
               hybridKdSlider->setValue(sliderValue);
               hybridKdSlider->blockSignals(false);
               qDebug() << "KdSpinBox值改变:" << spinValue << "->" << sliderValue;
           });
           successCount++;
       }

       if (hybridPosSlider && hybridPosSpinBox) {
           qDebug() << "连接Pos控件...";
           connect(hybridPosSlider, &QSlider::valueChanged, this, [this](int sliderValue) {
               if (!hybridPosSpinBox || !hybridPosSlider) return;
               double min = hybridPosSpinBox->minimum();
               double max = hybridPosSpinBox->maximum();
               double spinValue = min + (sliderValue / 1000.0) * (max - min);
               hybridPosSpinBox->blockSignals(true);
               hybridPosSpinBox->setValue(spinValue);
               hybridPosSpinBox->blockSignals(false);
               qDebug() << "Pos滑块值改变:" << sliderValue << "->" << spinValue;
           });
           connect(hybridPosSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double spinValue) {
               if (!hybridPosSpinBox || !hybridPosSlider) return;
               double min = hybridPosSpinBox->minimum();
               double max = hybridPosSpinBox->maximum();
               int sliderValue = 0;
               if (max > min) {
                   double ratio = (spinValue - min) / (max - min);
                   sliderValue = qRound(ratio * 1000);
               }
               hybridPosSlider->blockSignals(true);
               hybridPosSlider->setValue(sliderValue);
               hybridPosSlider->blockSignals(false);
               qDebug() << "PosSpinBox值改变:" << spinValue << "->" << sliderValue;
           });
           successCount++;
       }

       if (hybridSpeedSlider && hybridSpeedSpinBox) {
           qDebug() << "连接Speed控件...";
           connect(hybridSpeedSlider, &QSlider::valueChanged, this, [this](int sliderValue) {
               if (!hybridSpeedSpinBox || !hybridSpeedSlider) return;
               double min = hybridSpeedSpinBox->minimum();
               double max = hybridSpeedSpinBox->maximum();
               double spinValue = min + (sliderValue / 1000.0) * (max - min);
               hybridSpeedSpinBox->blockSignals(true);
               hybridSpeedSpinBox->setValue(spinValue);
               hybridSpeedSpinBox->blockSignals(false);
               qDebug() << "Speed滑块值改变:" << sliderValue << "->" << spinValue;
           });
           connect(hybridSpeedSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double spinValue) {
               if (!hybridSpeedSpinBox || !hybridSpeedSlider) return;
               double min = hybridSpeedSpinBox->minimum();
               double max = hybridSpeedSpinBox->maximum();
               int sliderValue = 0;
               if (max > min) {
                   double ratio = (spinValue - min) / (max - min);
                   sliderValue = qRound(ratio * 1000);
               }
               hybridSpeedSlider->blockSignals(true);
               hybridSpeedSlider->setValue(sliderValue);
               hybridSpeedSlider->blockSignals(false);
               qDebug() << "SpeedSpinBox值改变:" << spinValue << "->" << sliderValue;
           });
           successCount++;
       }

       if (hybridTorqueSlider && hybridTorqueSpinBox) {
           qDebug() << "连接Torque控件...";
           connect(hybridTorqueSlider, &QSlider::valueChanged, this, [this](int sliderValue) {
               if (!hybridTorqueSpinBox || !hybridTorqueSlider) return;
               double min = hybridTorqueSpinBox->minimum();
               double max = hybridTorqueSpinBox->maximum();
               double spinValue = min + (sliderValue / 1000.0) * (max - min);
               hybridTorqueSpinBox->blockSignals(true);
               hybridTorqueSpinBox->setValue(spinValue);
               hybridTorqueSpinBox->blockSignals(false);
               qDebug() << "Torque滑块值改变:" << sliderValue << "->" << spinValue;
           });
           connect(hybridTorqueSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double spinValue) {
               if (!hybridTorqueSpinBox || !hybridTorqueSlider) return;
               double min = hybridTorqueSpinBox->minimum();
               double max = hybridTorqueSpinBox->maximum();
               int sliderValue = 0;
               if (max > min) {
                   double ratio = (spinValue - min) / (max - min);
                   sliderValue = qRound(ratio * 1000);
               }
               hybridTorqueSlider->blockSignals(true);
               hybridTorqueSlider->setValue(sliderValue);
               hybridTorqueSlider->blockSignals(false);
               qDebug() << "TorqueSpinBox值改变:" << spinValue << "->" << sliderValue;
           });
           successCount++;
       }

       qDebug() << "连接完成:" << successCount << "/5 对控件";
 }

void Widget::disconnectAllHybridConnections()
{
       if (hybridKpSlider) hybridKpSlider->disconnect();
       if (hybridKdSlider) hybridKdSlider->disconnect();
       if (hybridPosSlider) hybridPosSlider->disconnect();
       if (hybridSpeedSlider) hybridSpeedSlider->disconnect();
       if (hybridTorqueSlider) hybridTorqueSlider->disconnect();

       if (hybridKpSpinBox) hybridKpSpinBox->disconnect();
       if (hybridKdSpinBox) hybridKdSpinBox->disconnect();
       if (hybridPosSpinBox) hybridPosSpinBox->disconnect();
       if (hybridSpeedSpinBox) hybridSpeedSpinBox->disconnect();
       if (hybridTorqueSpinBox) hybridTorqueSpinBox->disconnect();

       qDebug() << "已断开所有力位混合连接";
}

void Widget::updateHybridSliderRanges()
{
    if (!hybridPageInitialized) return;

    // 从MIT参数获取限制值（除以1000000转换为浮点数）
    double kpMin = mitParams.kp_limit_min / 1000000.0;
    double kpMax = mitParams.kp_limit_max / 1000000.0;
    double kdMin = mitParams.kd_limit_min / 1000000.0;
    double kdMax = mitParams.kd_limit_max / 1000000.0;
    double posMin = mitParams.pos_limit_min / 1000000.0;
    double posMax = mitParams.pos_limit_max / 1000000.0;
    double speedMin = mitParams.speed_limit_min / 1000000.0;
    double speedMax = mitParams.speed_limit_max / 1000000.0;
    double torqueMin = mitParams.torque_limit_min / 1000000.0;
    double torqueMax = mitParams.torque_limit_max / 1000000.0;


    if (hybridKpSlider && hybridKpSpinBox) {
        hybridKpSlider->setDoubleRange(kpMin, kpMax);
        hybridKpSpinBox->setRange(kpMin, kpMax);

        double currentValue = qBound(kpMin, hybridKpSpinBox->value(), kpMax);
        hybridKpSpinBox->setValue(currentValue);
        hybridKpSlider->setDoubleValue(currentValue);
    }
    if (hybridKpMinLabel) hybridKpMinLabel->setText(QString::number(kpMin, 'f', 2));
    if (hybridKpMaxLabel) hybridKpMaxLabel->setText(QString::number(kpMax, 'f', 2));


    if (hybridKdSlider && hybridKdSpinBox) {
        hybridKdSlider->setDoubleRange(kdMin, kdMax);
        hybridKdSpinBox->setRange(kdMin, kdMax);
        double currentValue = qBound(kdMin, hybridKdSpinBox->value(), kdMax);
        hybridKdSpinBox->setValue(currentValue);
        hybridKdSlider->setDoubleValue(currentValue);
    }
    if (hybridKdMinLabel) hybridKdMinLabel->setText(QString::number(kdMin, 'f', 2));
    if (hybridKdMaxLabel) hybridKdMaxLabel->setText(QString::number(kdMax, 'f', 2));


    if (hybridPosSlider && hybridPosSpinBox) {
        hybridPosSlider->setDoubleRange(posMin, posMax);
        hybridPosSpinBox->setRange(posMin, posMax);
        double currentValue = qBound(posMin, hybridPosSpinBox->value(), posMax);
        hybridPosSpinBox->setValue(currentValue);
        hybridPosSlider->setDoubleValue(currentValue);
    }
    if (hybridPosMinLabel) hybridPosMinLabel->setText(QString::number(posMin, 'f', 2));
    if (hybridPosMaxLabel) hybridPosMaxLabel->setText(QString::number(posMax, 'f', 2));


    if (hybridSpeedSlider && hybridSpeedSpinBox) {
        hybridSpeedSlider->setDoubleRange(speedMin, speedMax);
        hybridSpeedSpinBox->setRange(speedMin, speedMax);
        double currentValue = qBound(speedMin, hybridSpeedSpinBox->value(), speedMax);
        hybridSpeedSpinBox->setValue(currentValue);
        hybridSpeedSlider->setDoubleValue(currentValue);
    }
    if (hybridSpeedMinLabel) hybridSpeedMinLabel->setText(QString::number(speedMin, 'f', 2));
    if (hybridSpeedMaxLabel) hybridSpeedMaxLabel->setText(QString::number(speedMax, 'f', 2));


    if (hybridTorqueSlider && hybridTorqueSpinBox) {
        hybridTorqueSlider->setDoubleRange(torqueMin, torqueMax);
        hybridTorqueSpinBox->setRange(torqueMin, torqueMax);
        double currentValue = qBound(torqueMin, hybridTorqueSpinBox->value(), torqueMax);
        hybridTorqueSpinBox->setValue(currentValue);
        hybridTorqueSlider->setDoubleValue(currentValue);
    }
    if (hybridTorqueMinLabel) hybridTorqueMinLabel->setText(QString::number(torqueMin, 'f', 2));
    if (hybridTorqueMaxLabel) hybridTorqueMaxLabel->setText(QString::number(torqueMax, 'f', 2));

    qDebug() << "力位混合界面滑块范围已更新";
}

void Widget::updateHybridSliderLabelsAndRanges()
{
    qDebug() << "更新力位混合滑块标签和范围...";

        double kpMin = mitParams.kp_limit_min / 1000000.0;  // 除以1000000转换为浮点数
        double kpMax = mitParams.kp_limit_max / 1000000.0;
        double kdMin = mitParams.kd_limit_min / 1000000.0;
        double kdMax = mitParams.kd_limit_max / 1000000.0;
        double posMin = mitParams.pos_limit_min / 1000000.0;
        double posMax = mitParams.pos_limit_max / 1000000.0;
        double speedMin = mitParams.speed_limit_min / 1000000.0;
        double speedMax = mitParams.speed_limit_max / 1000000.0;
        double torqueMin = mitParams.torque_limit_min / 1000000.0;
        double torqueMax = mitParams.torque_limit_max / 1000000.0;

//        qDebug() << "从MIT参数读取的范围:";
//        qDebug() << "  Kp: [" << kpMin << "," << kpMax << "]";
//        qDebug() << "  Kd: [" << kdMin << "," << kdMax << "]";
//        qDebug() << "  位置: [" << posMin << "," << posMax << "]";
//        qDebug() << "  速度: [" << speedMin << "," << speedMax << "]";
//        qDebug() << "  力矩: [" << torqueMin << "," << torqueMax << "]";

        // 更新Kp标签
        if (hybridKpMinLabel) {
            hybridKpMinLabel->setText(QString::number(kpMin, 'f', 2));
//            qDebug() << "设置Kp最小标签:" << kpMin;
        }
        if (hybridKpMaxLabel) {
            hybridKpMaxLabel->setText(QString::number(kpMax, 'f', 2));
//            qDebug() << "设置Kp最大标签:" << kpMax;
        }

        // 更新Kd标签
        if (hybridKdMinLabel) {
            hybridKdMinLabel->setText(QString::number(kdMin, 'f', 2));
//            qDebug() << "设置Kd最小标签:" << kdMin;
        }
        if (hybridKdMaxLabel) {
            hybridKdMaxLabel->setText(QString::number(kdMax, 'f', 2));
//            qDebug() << "设置Kd最大标签:" << kdMax;
        }

        // 更新位置标签
        if (hybridPosMinLabel) {
            hybridPosMinLabel->setText(QString::number(posMin, 'f', 2));
//            qDebug() << "设置位置最小标签:" << posMin;
        }
        if (hybridPosMaxLabel) {
            hybridPosMaxLabel->setText(QString::number(posMax, 'f', 2));
//            qDebug() << "设置位置最大标签:" << posMax;
        }

        // 更新速度标签
        if (hybridSpeedMinLabel) {
            hybridSpeedMinLabel->setText(QString::number(speedMin, 'f', 2));
//            qDebug() << "设置速度最小标签:" << speedMin;
        }
        if (hybridSpeedMaxLabel) {
            hybridSpeedMaxLabel->setText(QString::number(speedMax, 'f', 2));
//            qDebug() << "设置速度最大标签:" << speedMax;
        }

        // 更新力矩标签
        if (hybridTorqueMinLabel) {
            hybridTorqueMinLabel->setText(QString::number(torqueMin, 'f', 2));
//            qDebug() << "设置力矩最小标签:" << torqueMin;
        }
        if (hybridTorqueMaxLabel) {
            hybridTorqueMaxLabel->setText(QString::number(torqueMax, 'f', 2));
//            qDebug() << "设置力矩最大标签:" << torqueMax;
        }

        // 更新SpinBox范围
        if (hybridKpSpinBox) {
            hybridKpSpinBox->setRange(kpMin, kpMax);

            double currentValue = qBound(kpMin, hybridKpSpinBox->value(), kpMax);
            hybridKpSpinBox->setValue(currentValue);
        }

        if (hybridKdSpinBox) {
            hybridKdSpinBox->setRange(kdMin, kdMax);
            double currentValue = qBound(kdMin, hybridKdSpinBox->value(), kdMax);
            hybridKdSpinBox->setValue(currentValue);
        }

        if (hybridPosSpinBox) {
            hybridPosSpinBox->setRange(posMin, posMax);
            double currentValue = qBound(posMin, hybridPosSpinBox->value(), posMax);
            hybridPosSpinBox->setValue(currentValue);
        }

        if (hybridSpeedSpinBox) {
            hybridSpeedSpinBox->setRange(speedMin, speedMax);
            double currentValue = qBound(speedMin, hybridSpeedSpinBox->value(), speedMax);
            hybridSpeedSpinBox->setValue(currentValue);
        }

        if (hybridTorqueSpinBox) {
            hybridTorqueSpinBox->setRange(torqueMin, torqueMax);
            double currentValue = qBound(torqueMin, hybridTorqueSpinBox->value(), torqueMax);
            hybridTorqueSpinBox->setValue(currentValue);
        }


        syncSliderFromSpinBox();

        qDebug() << "力位混合滑块标签和范围更新完成";
}

void Widget::syncSliderFromSpinBox()
{

        if (hybridKpSpinBox && hybridKpSlider) {
            double value = hybridKpSpinBox->value();
            double min = hybridKpSpinBox->minimum();
            double max = hybridKpSpinBox->maximum();
            int sliderValue = 0;

            if (max > min) {
                double ratio = (value - min) / (max - min);
                sliderValue = qRound(ratio * 1000); // 映射到0-1000整数
            }

            hybridKpSlider->blockSignals(true);
            hybridKpSlider->setValue(sliderValue);
            hybridKpSlider->blockSignals(false);
        }

        // KD同步
        if (hybridKdSpinBox && hybridKdSlider) {
            double value = hybridKdSpinBox->value();
            double min = hybridKdSpinBox->minimum();
            double max = hybridKdSpinBox->maximum();
            int sliderValue = 0;
            if (max > min) {
                double ratio = (value - min) / (max - min);
                sliderValue = qRound(ratio * 1000);
            }
            hybridKdSlider->blockSignals(true);
            hybridKdSlider->setValue(sliderValue);
            hybridKdSlider->blockSignals(false);
        }

        // Pos同步
        if (hybridPosSpinBox && hybridPosSlider) {
            double value = hybridPosSpinBox->value();
            double min = hybridPosSpinBox->minimum();
            double max = hybridPosSpinBox->maximum();
            int sliderValue = 0;
            if (max > min) {
                double ratio = (value - min) / (max - min);
                sliderValue = qRound(ratio * 1000);
            }
            hybridPosSlider->blockSignals(true);
            hybridPosSlider->setValue(sliderValue);
            hybridPosSlider->blockSignals(false);
        }

        // Speed同步
        if (hybridSpeedSpinBox && hybridSpeedSlider) {
            double value = hybridSpeedSpinBox->value();
            double min = hybridSpeedSpinBox->minimum();
            double max = hybridSpeedSpinBox->maximum();
            int sliderValue = 0;
            if (max > min) {
                double ratio = (value - min) / (max - min);
                sliderValue = qRound(ratio * 1000);
            }
            hybridSpeedSlider->blockSignals(true);
            hybridSpeedSlider->setValue(sliderValue);
            hybridSpeedSlider->blockSignals(false);
        }

        // Torque同步
        if (hybridTorqueSpinBox && hybridTorqueSlider) {
            double value = hybridTorqueSpinBox->value();
            double min = hybridTorqueSpinBox->minimum();
            double max = hybridTorqueSpinBox->maximum();
            int sliderValue = 0;
            if (max > min) {
                double ratio = (value - min) / (max - min);
                sliderValue = qRound(ratio * 1000);
            }
            hybridTorqueSlider->blockSignals(true);
            hybridTorqueSlider->setValue(sliderValue);
            hybridTorqueSlider->blockSignals(false);
        }
}

void Widget::onHybridSliderValueChanged(int sliderValue, QDoubleSpinBox* spinBox, DoubleSlider* slider, const QString& paramName)
{
    if (!spinBox || !slider) {
        qDebug() << paramName << "错误: 控件指针为空";
        return;
    }

    double spinMin = spinBox->minimum();
    double spinMax = spinBox->maximum();
    int sliderMin = slider->minimum();
    int sliderMax = slider->maximum();

    if (sliderMax == sliderMin) {
        qDebug() << paramName << "错误: 滑块范围为零";
        return;
    }

    double ratio = static_cast<double>(sliderValue - sliderMin) / (sliderMax - sliderMin);
    double spinValue = spinMin + ratio * (spinMax - spinMin);

    spinValue = qRound(spinValue * 100) / 100.0;

    qDebug() << paramName << "滑块:" << sliderValue << "-> SpinBox:" << spinValue;

    spinBox->blockSignals(true);
    spinBox->setValue(spinValue);
    spinBox->blockSignals(false);
}

void Widget::onHybridSpinBoxValueChanged(double spinValue, QDoubleSpinBox* spinBox, DoubleSlider* slider, const QString& paramName)
{
    if (!spinBox || !slider) {
        qDebug() << paramName << "错误: 控件指针为空";
        return;
    }

    double spinMin = spinBox->minimum();
    double spinMax = spinBox->maximum();
    int sliderMin = slider->minimum();
    int sliderMax = slider->maximum();

    if (spinMax == spinMin) {
        qDebug() << paramName << "错误: SpinBox范围为零";
        return;
    }

    double ratio = (spinValue - spinMin) / (spinMax - spinMin);
    int sliderValue = sliderMin + qRound(ratio * (sliderMax - sliderMin));

    sliderValue = qBound(sliderMin, sliderValue, sliderMax);

    qDebug() << paramName << "SpinBox:" << spinValue << "-> 滑块:" << sliderValue;

    slider->blockSignals(true);
    slider->setValue(sliderValue);
    slider->blockSignals(false);
}

void Widget::on_writeMitLoopBtn_clicked()
{
    if (isCalibModeOpen) {
        QMessageBox::warning(this, tr("操作提示"), tr("请先关闭标定模式！"));
        return;
    }

    mitParams.kp_limit_min = static_cast<int32_t>(ui->kpLimitMinEdit->value() * 1000000);
    mitParams.kp_limit_max = static_cast<int32_t>(ui->kpLimitMaxEdit->value() * 1000000);
    mitParams.kd_limit_min = static_cast<int32_t>(ui->kdLimitMinEdit->value() * 1000000);
    mitParams.kd_limit_max = static_cast<int32_t>(ui->kdLimitMaxEdit->value() * 1000000);
    mitParams.pos_limit_min = static_cast<int32_t>(ui->posLimitMinEdit->value() * 1000000);
    mitParams.pos_limit_max = static_cast<int32_t>(ui->posLimitMaxEdit->value() * 1000000);
    mitParams.speed_limit_min = static_cast<int32_t>(ui->speedLimitMinEdit->value() * 1000000);
    mitParams.speed_limit_max = static_cast<int32_t>(ui->speedLimitMaxEdit->value() * 1000000);
    mitParams.torque_limit_min = static_cast<int32_t>(ui->torqueLimitMinEdit->value() * 1000000);
    mitParams.torque_limit_max = static_cast<int32_t>(ui->torqueLimitMaxEdit->value() * 1000000);

    QByteArray payload = getMitParamsBytes();

    if (payload.size() != 40) {
        QMessageBox::warning(this, tr("数据错误"),
                           QString("MIT参数长度错误：%1字节，应为40字节").arg(payload.size()));
        return;
    }

    sendControlCommand(CMD_SET_MIT_LOOP, payload, 1);
    updateStatus("发送MIT参数设置指令");

    if (hybridPage) {
        updateHybridSliderLabelsAndRanges();
    }
}

void Widget::on_mechLimitBtn_clicked()
{
    if (isCalibModeOpen) {
        QMessageBox::warning(this, tr("操作提示"), tr("请先关闭标定模式！"));
        return;
    }

    double mechUpperLimit = ui->mechUpperLimitEdit->value();
    double mechLowerLimit = ui->mechLowerLimitEdit->value();

    double elecUpperLimit = ui->elecUpperLimitEdit->value();
    double elecLowerLimit = ui->elecLowerLimitEdit->value();


    QByteArray payload;
    payload.append(int32ToBigEndian(static_cast<int32_t>(mechUpperLimit * 1000000)));
    payload.append(int32ToBigEndian(static_cast<int32_t>(mechLowerLimit * 1000000)));

    payload.append(int32ToBigEndian(static_cast<int32_t>(elecUpperLimit * 1000000)));
    payload.append(int32ToBigEndian(static_cast<int32_t>(elecLowerLimit * 1000000)));

    sendControlCommand(CMD_MECH_LIMIT, payload, 1);

    updateStatus("发送限位设置指令");


//    sendCalibCommand(CMD_MECH_LIMIT,1);
}

// ==================== 修改：MIT参数设置指令发送（使用DoubleSlider的小数值） ====================
void Widget::on_mitSetBtn_clicked()
{
//    if (!isControlModeOpen || !isMortorEnableOpen) {
//        QMessageBox::warning(this, tr("操作提示"), tr("请先打开控制模式并使能电机！"));
//        return;
//    }

    if (!isControlModeOpen) {
        QMessageBox::warning(this, tr("操作提示"), tr("请先打开控制模式！"));
        return;
    }
    if (!isMortorEnableOpen) {
        QMessageBox::warning(this, tr("操作提示"), tr("请先打开电机使能！"));
        return;
    }

    // 从DoubleSlider获取小数（支持负数）
//    double kpValue = hybridKpSlider->getDoubleValue();
//    double kdValue = hybridKdSlider->getDoubleValue();
//    double posValue = hybridPosSlider->getDoubleValue();
//    double speedValue = hybridSpeedSlider->getDoubleValue();
//    double torqueValue = hybridTorqueSlider->getDoubleValue();

    bool ok;
    double kpValue=  ui->mitKpEdit->text().toDouble(&ok);
           if (!ok) {
               QMessageBox::warning(this, tr("Kp输入错误"), tr("请输入有效的Kp值！"));
               return;
           }
           double kdValue=  ui->mitKdEdit->text().toDouble(&ok);
                  if (!ok) {
                      QMessageBox::warning(this, tr("Kd输入错误"), tr("请输入有效的Kd值！"));
                      return;
                  }
                  double posValue=  ui->targetPosEdit->text().toDouble(&ok);
                         if (!ok) {
                             QMessageBox::warning(this, tr("目标位置输入错误"), tr("请输入有效的目标位置值！"));
                             return;
                         }
                         double speedValue=  ui->targetSpeedEdit->text().toDouble(&ok);
                                if (!ok) {
                                    QMessageBox::warning(this, tr("目标转速输入错误"), tr("请输入有效的目标转速值！"));
                                    return;
                                }
                                double torqueValue=  ui->feedforwardTorqueEdit->text().toDouble(&ok);
                                       if (!ok) {
                                           QMessageBox::warning(this, tr("目标力矩输入错误"), tr("请输入有效的目标力矩值！"));
                                           return;
                                       }



    //// 打印解析结果
    //qDebug() << "[MIT获取详情] "
    //         << "kpValue=" << kpValue
    //         << "kdValue=" << kdValue
    //         << "posValue=" << posValue
    //         << "speedValue=" << speedValue
    //         << "torqueValue=" << torqueValue;

    QByteArray payload;
    payload.append(static_cast<quint8>(MODE_MIT));  // 力位混合模式
    payload.append(int32ToBigEndian(static_cast<int32_t>(kpValue * 1000000)));      // Kp×1e6
    payload.append(int32ToBigEndian(static_cast<int32_t>(kdValue * 1000000)));      // Kd×1e6
    payload.append(int32ToBigEndian(static_cast<int32_t>(posValue * 1000000)));    // 位置×1e6
    payload.append(int32ToBigEndian(static_cast<int32_t>(speedValue * 100000)));   // 速度×1e5
    payload.append(int32ToBigEndian(static_cast<int32_t>(torqueValue * 1000000)));  // 力矩×1e6

    sendControlCommand(CMD_SET_MIT, payload, 1);
    updateStatus(QString("发送力位混合指令：Kp=%1, Kd=%2, 位置=%3, 速度=%4, 力矩=%5")
                .arg(kpValue).arg(kdValue).arg(posValue).arg(speedValue).arg(torqueValue));
}

bool Widget::parseMitParams(const QByteArray& data)
{
    if (data.size() != 40) {
            qDebug() << "MIT参数数据长度不足：" << data.size() << "字节";
            return false;
        }

        if (!ui->kpLimitMinEdit || !ui->kpLimitMaxEdit || !ui->kdLimitMinEdit || !ui->kdLimitMaxEdit ||
            !ui->posLimitMinEdit || !ui->posLimitMaxEdit || !ui->speedLimitMinEdit || !ui->speedLimitMaxEdit ||
            !ui->torqueLimitMinEdit || !ui->torqueLimitMaxEdit) {
            qDebug() << "[显示错误] 控制参数页面的显示控件未找到";
            return false;
        }

        int offset = 0;
        mitParams.kp_limit_min = bigEndianToint32(data.mid(offset, 4)); offset += 4;
        mitParams.kp_limit_max = bigEndianToint32(data.mid(offset, 4)); offset += 4;
        mitParams.kd_limit_min = bigEndianToint32(data.mid(offset, 4)); offset += 4;
        mitParams.kd_limit_max = bigEndianToint32(data.mid(offset, 4)); offset += 4;
        mitParams.pos_limit_min = bigEndianToint32(data.mid(offset, 4)); offset += 4;
        mitParams.pos_limit_max = bigEndianToint32(data.mid(offset, 4)); offset += 4;
        mitParams.speed_limit_min = bigEndianToint32(data.mid(offset, 4)); offset += 4;
        mitParams.speed_limit_max = bigEndianToint32(data.mid(offset, 4)); offset += 4;
        mitParams.torque_limit_min = bigEndianToint32(data.mid(offset, 4)); offset += 4;
        mitParams.torque_limit_max = bigEndianToint32(data.mid(offset, 4)); offset += 4;

//        qDebug() << "[MIT解析详情] "
//                 << "kp_min=" << mitParams.kp_limit_min/1000000.0
//                 << "kp_max=" << mitParams.kp_limit_max/1000000.0
//                 << "kd_min=" << mitParams.kd_limit_min/1000000.0
//                 << "kd_max=" << mitParams.kd_limit_max/1000000.0
//                 << "pos_min=" << mitParams.pos_limit_min/1000000.0
//                 << "pos_max=" << mitParams.pos_limit_max/1000000.0
//                 << "speed_min=" << mitParams.speed_limit_min/1000000.0
//                 << "speed_max=" << mitParams.speed_limit_max/1000000.0
//                 << "torque_min=" << mitParams.torque_limit_min/1000000.0
//                 << "torque_max=" << mitParams.torque_limit_max/1000000.0;

        double kpMin = mitParams.kp_limit_min / 1000000.0;
        double kpMax = mitParams.kp_limit_max / 1000000.0;
        double kdMin = mitParams.kd_limit_min / 1000000.0;
        double kdMax = mitParams.kd_limit_max / 1000000.0;
        double posMin = mitParams.pos_limit_min / 1000000.0;
        double posMax = mitParams.pos_limit_max / 1000000.0;
        double speedMin = mitParams.speed_limit_min / 1000000.0;
        double speedMax = mitParams.speed_limit_max / 1000000.0;
        double torqueMin = mitParams.torque_limit_min / 1000000.0;
        double torqueMax = mitParams.torque_limit_max / 1000000.0;

        ui->kpLimitMinEdit->blockSignals(true);
        ui->kpLimitMaxEdit->blockSignals(true);
        ui->kdLimitMinEdit->blockSignals(true);
        ui->kdLimitMaxEdit->blockSignals(true);
        ui->posLimitMinEdit->blockSignals(true);
        ui->posLimitMaxEdit->blockSignals(true);
        ui->speedLimitMinEdit->blockSignals(true);
        ui->speedLimitMaxEdit->blockSignals(true);
        ui->torqueLimitMinEdit->blockSignals(true);
        ui->torqueLimitMaxEdit->blockSignals(true);

        ui->kpLimitMinEdit->setValue(kpMin);
        ui->kpLimitMaxEdit->setValue(kpMax);
        ui->kdLimitMinEdit->setValue(kdMin);
        ui->kdLimitMaxEdit->setValue(kdMax);
        ui->posLimitMinEdit->setValue(posMin);
        ui->posLimitMaxEdit->setValue(posMax);
        ui->speedLimitMinEdit->setValue(speedMin);
        ui->speedLimitMaxEdit->setValue(speedMax);
        ui->torqueLimitMinEdit->setValue(torqueMin);
        ui->torqueLimitMaxEdit->setValue(torqueMax);

        ui->kpLimitMinEdit->blockSignals(false);
        ui->kpLimitMaxEdit->blockSignals(false);
        ui->kdLimitMinEdit->blockSignals(false);
        ui->kdLimitMaxEdit->blockSignals(false);
        ui->posLimitMinEdit->blockSignals(false);
        ui->posLimitMaxEdit->blockSignals(false);
        ui->speedLimitMinEdit->blockSignals(false);
        ui->speedLimitMaxEdit->blockSignals(false);
        ui->torqueLimitMinEdit->blockSignals(false);
        ui->torqueLimitMaxEdit->blockSignals(false);

        if (ui->tabWidget) {
            QWidget* controlPage = ui->tabWidget->findChild<QWidget*>("pageControlParams");
            if (controlPage) {
                controlPage->update();
                controlPage->repaint();
            }
        }
        QApplication::processEvents();

//        qDebug() << "[MIT显示日志] 控制参数页面显示控件已更新"
//                 << "kp_min=" << ui->kpLimitMinEdit->value()
//                 << "kp_max=" << ui->kpLimitMaxEdit->value();

        return true;
    }
// ==================== 获取MIT参数字节数组 ====================
QByteArray Widget::getMitParamsBytes() const
{
    QByteArray payload;

    payload.append(int32ToBigEndian(mitParams.kp_limit_min));
    payload.append(int32ToBigEndian(mitParams.kp_limit_max));
    payload.append(int32ToBigEndian(mitParams.kd_limit_min));
    payload.append(int32ToBigEndian(mitParams.kd_limit_max));
    payload.append(int32ToBigEndian(mitParams.pos_limit_min));
    payload.append(int32ToBigEndian(mitParams.pos_limit_max));
    payload.append(int32ToBigEndian(mitParams.speed_limit_min));
    payload.append(int32ToBigEndian(mitParams.speed_limit_max));
    payload.append(int32ToBigEndian(mitParams.torque_limit_min));
    payload.append(int32ToBigEndian(mitParams.torque_limit_max));

    return payload;
}

void Widget::initControlParams()
{

//    if (ui->posKpEdit) ui->posKpEdit->setText(QString::number(0.0));
//    if (ui->maxPosEdit) ui->maxPosEdit->setText(QString::number(0.0));
//    if (ui->posTrajVelEdit) ui->posTrajVelEdit->setText(QString::number(0.0));
//    if (ui->posTrajAccelEdit) ui->posTrajAccelEdit->setText(QString::number(0.0));
//    if (ui->posTrajDecelEdit) ui->posTrajDecelEdit->setText(QString::number(0.0));

//    if (ui->speedKpEdit) ui->speedKpEdit->setText(QString::number(0.0));
//    if (ui->speedKiEdit) ui->speedKiEdit->setText(QString::number(0.0));
//    if (ui->maxSpeedEdit) ui->maxSpeedEdit->setText(QString::number(0.0));
//    if (ui->speedIncEdit) ui->speedIncEdit->setText(QString::number(0.0));

//    if (ui->currentKpEdit) ui->currentKpEdit->setText(QString::number(0.0));
//    if (ui->currentKiEdit) ui->currentKiEdit->setText(QString::number(0.0));
//    if (ui->currentBandwidthEdit) ui->currentBandwidthEdit->setText(QString::number(0.0));
//    if (ui->currentLoopLimitEdit) ui->currentLoopLimitEdit->setText(QString::number(0.0));
//    if (ui->currentIncEdit) ui->currentIncEdit->setText(QString::number(0.0));

//    if (ui->encoderBandwidthEdit) ui->encoderBandwidthEdit->setText(QString::number(0.0));
//    if (ui->encoderDirEdit) ui->encoderDirEdit->setText(QString::number(0));

//    if (ui->motorKeEdit) ui->motorKeEdit->setText(QString::number(10.0));
//    if (ui->motorPolepairEdit) ui->motorPolepairEdit->setText(QString::number(10));
//    if (ui->motorResistanceEdit) ui->motorResistanceEdit->setText(QString::number(0.0));
//    if (ui->motorInductanceEdit) ui->motorInductanceEdit->setText(QString::number(0.0));
//    if (ui->motorTrefmaxEdit) ui->motorTrefmaxEdit->setText(QString::number(0.0));
//    if (ui->motorIQrefmaxEdit) ui->motorIQrefmaxEdit->setText(QString::number(0.0));
//    if (ui->motorGrEdit) ui->motorGrEdit->setText(QString::number(0.0));

    if (ui->posKpEdit) {
        ui->posKpEdit->setValue(0.0);
    }
    if (ui->maxPosEdit) {
        ui->maxPosEdit->setValue(0.0);
    }
    if (ui->posTrajVelEdit) {
        ui->posTrajVelEdit->setValue(0.0);
    }
    if (ui->posTrajAccelEdit) {
        ui->posTrajAccelEdit->setValue(0.0);
    }
    if (ui->posTrajDecelEdit) {
        ui->posTrajDecelEdit->setValue(0.0);
    }

    if (ui->speedKpEdit) {
        ui->speedKpEdit->setValue(0.0);
    }
    if (ui->speedKiEdit) {
        ui->speedKiEdit->setValue(0.0);
    }
    if (ui->maxSpeedEdit) {
        ui->maxSpeedEdit->setValue(0.0);
    }
    if (ui->speedIncEdit) {
        ui->speedIncEdit->setValue(0.0);
    }

    if (ui->currentKpEdit) {
        ui->currentKpEdit->setValue(0.0);
    }
    if (ui->currentKiEdit) {
        ui->currentKiEdit->setValue(0.0);
    }
    if (ui->currentBandwidthEdit) {
        ui->currentBandwidthEdit->setValue(0.0);
    }
    if (ui->currentLoopLimitEdit) {
        ui->currentLoopLimitEdit->setValue(0.0);
    }
    if (ui->currentIncEdit) {
        ui->currentIncEdit->setValue(0.0);
    }

    if (ui->encoderBandwidthEdit) {
        ui->encoderBandwidthEdit->setValue(0.0);
    }
    if (ui->encoderDirEdit) {
        ui->encoderDirEdit->setValue(0);
    }
    if (ui->motorKeEdit) {
        ui->motorKeEdit->setValue(10.0); // 原默认值10.0
    }
    if (ui->motorPolepairEdit) {
        ui->motorPolepairEdit->setValue(10); // 原默认值10
    }
    if (ui->motorResistanceEdit) {
        ui->motorResistanceEdit->setValue(0.0);
    }
    if (ui->motorInductanceEdit) {
        ui->motorInductanceEdit->setValue(0.0);
    }
    if (ui->motorTrefmaxEdit) {
        ui->motorTrefmaxEdit->setValue(0.0);
    }
    if (ui->motorIQrefmaxEdit) {
        ui->motorIQrefmaxEdit->setValue(0.0);
    }
    if (ui->motorGrEdit) {
        ui->motorGrEdit->setValue(0.0);
    }


    // MIT参数
    if (ui->kpLimitMinEdit) ui->kpLimitMinEdit->setValue(0.0);
    if (ui->kpLimitMaxEdit) ui->kpLimitMaxEdit->setValue(0.0);
    if (ui->kdLimitMinEdit) ui->kdLimitMinEdit->setValue(0.0);
    if (ui->kdLimitMaxEdit) ui->kdLimitMaxEdit->setValue(0.0);
    if (ui->posLimitMinEdit) ui->posLimitMinEdit->setValue(0.0);
    if (ui->posLimitMaxEdit) ui->posLimitMaxEdit->setValue(0.0);
    if (ui->speedLimitMinEdit) ui->speedLimitMinEdit->setValue(0.0);
    if (ui->speedLimitMaxEdit) ui->speedLimitMaxEdit->setValue(0.0);
    if (ui->torqueLimitMinEdit) ui->torqueLimitMinEdit->setValue(0.0);
    if (ui->torqueLimitMaxEdit) ui->torqueLimitMaxEdit->setValue(0.0);

    // 连接控制参数界面按钮
    if (ui->writePosLoopBtn) {
        connect(ui->writePosLoopBtn, &QPushButton::clicked, this, &Widget::on_writePosLoopBtn_clicked);
    }
    if (ui->writeSpeedLoopBtn) {
        connect(ui->writeSpeedLoopBtn, &QPushButton::clicked, this, &Widget::on_writeSpeedLoopBtn_clicked);
    }
    if (ui->writeCurrentLoopBtn) {
        connect(ui->writeCurrentLoopBtn, &QPushButton::clicked, this, &Widget::on_writeCurrentLoopBtn_clicked);
    }
    if (ui->writeEncoderBtn) {
        connect(ui->writeEncoderBtn, &QPushButton::clicked, this, &Widget::on_writeEncoderBtn_clicked);
    }
    if (ui->writeMotorBtn) {
        connect(ui->writeMotorBtn, &QPushButton::clicked, this, &Widget::on_writeMotorBtn_clicked);
    }
    if (ui->writeMitLoopBtn) {
        connect(ui->writeMitLoopBtn, &QPushButton::clicked, this, &Widget::on_writeMitLoopBtn_clicked);
    }
    if (ui->readControlBtn) {
        connect(ui->readControlBtn, &QPushButton::clicked, this, &Widget::on_readControlBtn_clicked);
    }

    // 初始化MIT参数结构体
    initMitParams();
}

void Widget::initMitParams()
{
    memset(&mitParams, 0, sizeof(MITParams));
}

void Widget::initMotorControl()
{

    QWidget* positionPage = ui->modeTabWidget->widget(1);
    if (positionPage) {
        QDoubleSpinBox* posCmdSpinBox = positionPage->findChild<QDoubleSpinBox*>("posCmdEdit");
        if (posCmdSpinBox) {
            posCmdSpinBox->setDecimals(2);
            posCmdSpinBox->setRange(-360.0, 360.0); // 支持负数
            posCmdSpinBox->setValue(0.0);
        }
    }


    QWidget* velocityPage = ui->modeTabWidget->widget(2);
    if (velocityPage) {
        QDoubleSpinBox* speedCmdSpinBox = velocityPage->findChild<QDoubleSpinBox*>("speedCmdEdit");
        if (speedCmdSpinBox) {
            speedCmdSpinBox->setDecimals(2);
            speedCmdSpinBox->setRange(-1000.0, 1000.0);
            speedCmdSpinBox->setValue(0.0);
        }
    }


    QWidget* currentPage = ui->modeTabWidget->widget(3);
    if (currentPage) {
        QDoubleSpinBox* currentCmdSpinBox = currentPage->findChild<QDoubleSpinBox*>("currentCmdEdit");
        if (currentCmdSpinBox) {
            currentCmdSpinBox->setDecimals(2);
            currentCmdSpinBox->setRange(-20.0, 20.0);
            currentCmdSpinBox->setValue(0.0);
        }
    }


    QWidget* torquePage = ui->modeTabWidget->widget(4);
    if (torquePage) {
        QDoubleSpinBox* torqueCmdSpinBox = torquePage->findChild<QDoubleSpinBox*>("torqueCmdEdit");
        if (torqueCmdSpinBox) {
            torqueCmdSpinBox->setDecimals(2);
            torqueCmdSpinBox->setRange(-50.0, 50.0);
            torqueCmdSpinBox->setValue(0.0);
        }
    }

    connect(ui->motorEnableBtn, &QPushButton::clicked, this, &Widget::on_motorEnableBtn_clicked);
    connect(ui->clearALMBtn, &QPushButton::clicked, this, &Widget::on_clearALMBtn_clicked);
    connect(ui->controlModeBtn, &QPushButton::clicked, this, &Widget::on_controlModeBtn_clicked);
}

// ==================== 位置环参数写入 ====================
void Widget::on_writePosLoopBtn_clicked()
{
    if (!ui->posKpEdit || !ui->maxPosEdit || !ui->posTrajVelEdit ||
        !ui->posTrajAccelEdit || !ui->posTrajDecelEdit) {
        qDebug() << "位置环参数控件未找到";
        return;
    }

    double posKp = ui->posKpEdit->value();
    double maxPos = ui->maxPosEdit->value();
    double posTrajVel = ui->posTrajVelEdit->value();
    double posTrajAccel = ui->posTrajAccelEdit->value();
    double posTrajDecel = ui->posTrajDecelEdit->value();

    QByteArray payload;

    payload.append(int32ToBigEndian(static_cast<int32_t>(posKp * 1000000)));
    payload.append(int32ToBigEndian(static_cast<int32_t>(maxPos * 1000000)));
    payload.append(int32ToBigEndian(static_cast<int32_t>(posTrajVel * 1000000)));
    payload.append(int32ToBigEndian(static_cast<int32_t>(posTrajAccel * 1000000)));
    payload.append(int32ToBigEndian(static_cast<int32_t>(posTrajDecel * 1000000)));

    sendControlCommand(CMD_SET_POS_LOOP, payload, 1);
    updateStatus("发送位置环参数设置指令");
}

// ==================== 速度环参数写入 ====================
void Widget::on_writeSpeedLoopBtn_clicked()
{
    if (!ui->speedKpEdit || !ui->speedKiEdit || !ui->maxSpeedEdit || !ui->speedIncEdit) {
        qDebug() << "速度环参数控件未找到";
        return;
    }

    double speedKp = ui->speedKpEdit->value();
    double speedKi = ui->speedKiEdit->value();
    double maxSpeed = ui->maxSpeedEdit->value();
    double speedInc = ui->speedIncEdit->value();

    QByteArray payload;
    payload.append(int32ToBigEndian(static_cast<int32_t>(speedKp * 1000000)));
    payload.append(int32ToBigEndian(static_cast<int32_t>(speedKi * 1000000)));
    payload.append(int32ToBigEndian(static_cast<int32_t>(maxSpeed * 100000)));
    payload.append(int32ToBigEndian(static_cast<int32_t>(speedInc * 1000000)));

    sendControlCommand(CMD_SET_SPEED_LOOP, payload, 1);
    updateStatus("发送速度环参数设置指令");
}

// ==================== 电流环参数写入 ====================
void Widget::on_writeCurrentLoopBtn_clicked()
{
    if (!ui->currentKpEdit || !ui->currentKiEdit || !ui->currentBandwidthEdit ||
        !ui->currentLoopLimitEdit || !ui->currentIncEdit) {
        qDebug() << "电流环参数控件未找到";
        return;
    }

    double currentKp = ui->currentKpEdit->value();
    double currentKi = ui->currentKiEdit->value();
    int currentBandwidth = ui->currentBandwidthEdit->value();
    double currentLoopLimit = ui->currentLoopLimitEdit->value();
    double currentInc = ui->currentIncEdit->value();

    QByteArray payload;
    payload.append(int32ToBigEndian(static_cast<int32_t>(currentKp * 1000000)));
    payload.append(int32ToBigEndian(static_cast<int32_t>(currentKi * 1000000)));
    payload.append(int32ToBigEndian(static_cast<int32_t>(currentLoopLimit * 1000000)));
    payload.append(int32ToBigEndian(static_cast<int32_t>(currentBandwidth )));
    payload.append(int32ToBigEndian(static_cast<int32_t>(currentInc * 1000000)));

    sendControlCommand(CMD_SET_CURRENT_LOOP, payload, 1);
    updateStatus("发送电流环参数设置指令");
}

// ==================== 编码器参数写入 ====================
void Widget::on_writeEncoderBtn_clicked()
{
    if (!ui->encoderBandwidthEdit || !ui->encoderDirEdit) {
        qDebug() << "编码器参数控件未找到";
        return;
    }

    int encoderBandwidth = ui->encoderBandwidthEdit->value();
    int encoderDir = static_cast<int>(ui->encoderDirEdit->value()); // 方向转为整数

    QByteArray payload;
    payload.append(int32ToBigEndian(static_cast<int32_t>(encoderBandwidth)));
    payload.append(int32ToBigEndian(static_cast<int32_t>(encoderDir)));

    sendControlCommand(CMD_SET_ENCODER_LOOP, payload, 1);
    updateStatus("发送编码器参数设置指令");
}

// ==================== 电机参数写入 ====================
void Widget::on_writeMotorBtn_clicked()
{
    if (!ui->motorKeEdit || !ui->motorPolepairEdit || !ui->motorResistanceEdit ||
        !ui->motorInductanceEdit || !ui->motorTrefmaxEdit ||
        !ui->motorIQrefmaxEdit || !ui->motorGrEdit) {
        qDebug() << "电机参数控件未找到";
        return;
    }

    double motorKe = ui->motorKeEdit->value();
    int motorPolepair = static_cast<int>(ui->motorPolepairEdit->value()); // 极对数转为整数
    double motorResistance = ui->motorResistanceEdit->value();
    double motorInductance = ui->motorInductanceEdit->value();
    double motorTrefmax = ui->motorTrefmaxEdit->value();
    double motorIQrefmax = ui->motorIQrefmaxEdit->value();
    double motorGr = ui->motorGrEdit->value();

    QByteArray payload;
    payload.append(int32ToBigEndian(static_cast<int32_t>(motorResistance * 1000000)));
    payload.append(int32ToBigEndian(static_cast<int32_t>(motorInductance * 1000000)));
    payload.append(int32ToBigEndian(static_cast<int32_t>(motorTrefmax * 1000000)));
    payload.append(int32ToBigEndian(static_cast<int32_t>(motorIQrefmax *  1000000)));
    payload.append(int32ToBigEndian(static_cast<int32_t>(motorKe * 1000000)));
    payload.append(int32ToBigEndian(static_cast<int32_t>(motorPolepair* 1000000)));
    payload.append(int32ToBigEndian(static_cast<int32_t>(motorGr * 1000000)));

    sendControlCommand(CMD_SET_MOTOR_LOOP, payload, 1);
    updateStatus("发送电机参数设置指令");
}

void Widget::on_sendParamsBtn_clicked()
{
    QByteArray payload;
        sendControlCommand(CMD_READ_CTRL_PARAM, payload, 1);
        updateStatus("发送读取控制参数指令...");
}

void Widget::on_controlModeBtn_clicked()
{

    bool isEnable = (ui->controlModeBtn->text() == "打开控制模式") ? 1 : 0;
    QByteArray payload;
    payload.append(isEnable);
    if (isCalibModeOpen == true)
    {
        QMessageBox::warning(this, tr("操作提示"), tr("请先关闭标定模式！"), QMessageBox::Ok);
        return;
    }
    if (isMortorEnableOpen == true)
    {
        QMessageBox::warning(this, tr("操作提示"), tr("请先关闭电机使能！"), QMessageBox::Ok);
        return;
    }
    // 发送电机来控制模式打开和关闭指令（无Payload，enable=1打开模式  enable=0关闭模式）
    if(isEnable ==1)
    {
        updateStatus("发送电机控制模式打开指令...");
        qDebug() << "控制模式已打开";
    }
    else
    {
         updateStatus("发送电机控制模式关闭指令...");
         qDebug() << "控制模式已关闭";
    }
    sendControlCommand(CMD_CONTROL_MODE, payload, isEnable);

}

void Widget::onModeTabChanged(int index)
{
    QWidget* currentPage = ui->modeTabWidget ? ui->modeTabWidget->widget(index) : nullptr;
        if (currentPage && currentPage->objectName() == "pageHybrid") {
            qDebug() << "切换到力位混合界面，更新标签和范围";


            if (!hybridKpMinLabel || !hybridKpMaxLabel) {
                qDebug() << "标签指针为空，重新初始化力位混合界面";
                initHybridPage();
            }

            readMitParamsFromControlPage();

            updateHybridSliderLabelsAndRanges();

            updateMotorStatus();
        }
}

void Widget::onControlTabChanged(int index)
{
    QWidget* currentPage = ui->tabWidget ? ui->tabWidget->widget(index) : nullptr;
    if (currentPage && currentPage->objectName() == "Debug") {
        qDebug() << "切换到控制参数界面，更新显示";
    }
}

// 从控制参数界面读取MIT参数
void Widget::readMitParamsFromControlPage()
{

    if (!ui->kpLimitMinEdit || !ui->kpLimitMaxEdit) {
        qDebug() << "控制参数界面控件未找到，使用默认值";
        return;
    }

    mitParams.kp_limit_min = static_cast<int32_t>(ui->kpLimitMinEdit->value() * 1000000);
    mitParams.kp_limit_max = static_cast<int32_t>(ui->kpLimitMaxEdit->value() * 1000000);
    mitParams.kd_limit_min = static_cast<int32_t>(ui->kdLimitMinEdit->value() * 1000000);
    mitParams.kd_limit_max = static_cast<int32_t>(ui->kdLimitMaxEdit->value() * 1000000);
    mitParams.pos_limit_min = static_cast<int32_t>(ui->posLimitMinEdit->value() * 1000000);
    mitParams.pos_limit_max = static_cast<int32_t>(ui->posLimitMaxEdit->value() * 1000000);
    mitParams.speed_limit_min = static_cast<int32_t>(ui->speedLimitMinEdit->value() * 1000000);
    mitParams.speed_limit_max = static_cast<int32_t>(ui->speedLimitMaxEdit->value() * 1000000);
    mitParams.torque_limit_min = static_cast<int32_t>(ui->torqueLimitMinEdit->value() * 1000000);
    mitParams.torque_limit_max = static_cast<int32_t>(ui->torqueLimitMaxEdit->value() * 1000000);

    qDebug() << "从控制参数界面读取MIT参数完成";
}

// ==================== 更新力位混合滑块标签 ====================
void Widget::updateHybridSliderLabels()
{
    // 从MIT参数获取限制值（缩放1e6）
        double kpMin = mitParams.kp_limit_min / 1000000.0;
        double kpMax = mitParams.kp_limit_max / 1000000.0;
        double kdMin = mitParams.kd_limit_min / 1000000.0;
        double kdMax = mitParams.kd_limit_max / 1000000.0;
        double posMin = mitParams.pos_limit_min / 1000000.0;
        double posMax = mitParams.pos_limit_max / 1000000.0;
        double speedMin = mitParams.speed_limit_min / 1000000.0;
        double speedMax = mitParams.speed_limit_max / 1000000.0;
        double torqueMin = mitParams.torque_limit_min / 1000000.0;
        double torqueMax = mitParams.torque_limit_max / 1000000.0;

        // Kp：左侧=kpMin，右侧=kpMax
        if (hybridKpMinLabel) hybridKpMinLabel->setText(QString::number(kpMin, 'f', 2));
        if (hybridKpMaxLabel) hybridKpMaxLabel->setText(QString::number(kpMax, 'f', 2));
        // Kd：左侧=kdMin，右侧=kdMax
        if (hybridKdMinLabel) hybridKdMinLabel->setText(QString::number(kdMin, 'f', 2));
        if (hybridKdMaxLabel) hybridKdMaxLabel->setText(QString::number(kdMax, 'f', 2));
        // 位置：左侧=posMin，右侧=posMax
        if (hybridPosMinLabel) hybridPosMinLabel->setText(QString::number(posMin, 'f', 2));
        if (hybridPosMaxLabel) hybridPosMaxLabel->setText(QString::number(posMax, 'f', 2));
        // 速度：左侧=speedMin，右侧=speedMax
        if (hybridSpeedMinLabel) hybridSpeedMinLabel->setText(QString::number(speedMin, 'f', 2));
        if (hybridSpeedMaxLabel) hybridSpeedMaxLabel->setText(QString::number(speedMax, 'f', 2));
        // 力矩：左侧=torqueMin，右侧=torqueMax
        if (hybridTorqueMinLabel) hybridTorqueMinLabel->setText(QString::number(torqueMin, 'f', 2));
        if (hybridTorqueMaxLabel) hybridTorqueMaxLabel->setText(QString::number(torqueMax, 'f', 2));

        if (hybridKpSpinBox) {
            hybridKpSpinBox->setRange(kpMin, kpMax);
            hybridKpSpinBox->setValue(qBound(kpMin, hybridKpSpinBox->value(), kpMax));
        }
        if (hybridKdSpinBox) {
            hybridKdSpinBox->setRange(kdMin, kdMax);
            hybridKdSpinBox->setValue(qBound(kdMin, hybridKdSpinBox->value(), kdMax));
        }
        if (hybridPosSpinBox) {
            hybridPosSpinBox->setRange(posMin, posMax);
            hybridPosSpinBox->setValue(qBound(posMin, hybridPosSpinBox->value(), posMax));
        }
        if (hybridSpeedSpinBox) {
            hybridSpeedSpinBox->setRange(speedMin, speedMax);
            hybridSpeedSpinBox->setValue(qBound(speedMin, hybridSpeedSpinBox->value(), speedMax));
        }
        if (hybridTorqueSpinBox) {
            hybridTorqueSpinBox->setRange(torqueMin, torqueMax);
            hybridTorqueSpinBox->setValue(qBound(torqueMin, hybridTorqueSpinBox->value(), torqueMax));
        }

        // 同步Slider与SpinBox
        syncSliderFromSpinBox();

        qDebug() << "力位混合滑块标签已更新（左侧min，右侧max）";
}

void Widget::sendControlParam(const QString& paramName, double value)
{
       if (paramName == "mitKp" || paramName == "mitKd" || paramName == "mitKi" ||
           paramName == "mitMax" || paramName == "mitMin") {
           // MIT参数，使用力位混合模式指令
           sendMixedControlCommand(0x05, paramName, value);
       } else if (paramName == "posKp" || paramName == "maxPos" || paramName == "posTrajVel" ||
                  paramName == "posTrajAccel" || paramName == "posTrajDecel") {
           // 位置环参数，使用位置模式指令
           sendMixedControlCommand(0x01, paramName, value);
       } else if (paramName == "speedKp" || paramName == "speedKi" || paramName == "maxSpeed" ||
                  paramName == "speedInc") {
           // 速度环参数，使用速度模式指令
           sendMixedControlCommand(0x02, paramName, value);
       } else if (paramName == "currentKp" || paramName == "currentKi" || paramName == "currentBandwidth" ||
                  paramName == "currentLoopLimit" || paramName == "currentInc") {
           // 电流环参数，使用电流模式指令
           sendMixedControlCommand(0x04, paramName, value);
       } else if (paramName == "motorTremax" || paramName == "motorQrefmax") {
           // 力矩相关参数，使用力矩模式指令
           sendMixedControlCommand(0x03, paramName, value);
       } else {
              QByteArray payload;
              sendControlCommand(0, payload, 1);
       }

       qDebug() << "发送参数到设备:" << paramName << "=" << value;
}
// ==================== 发送混合控制指令 ====================
void Widget::sendMixedControlCommand(quint8 mode, const QString& paramName, double value)
{
    QByteArray data;

    data.append(mode);

    QByteArray paramData(20, 0x00); // 初始化为20个0x00

    if (mode == 0x05) { // 力位混合模式
        if (paramName == "mitKp") {
            float kpValue = static_cast<float>(value);
            QByteArray kpBytes(reinterpret_cast<const char*>(&kpValue), sizeof(float));
            paramData.replace(0, 4, kpBytes); // 前4字节
        } else if (paramName == "mitKd") {
            float kdValue = static_cast<float>(value);
            QByteArray kdBytes(reinterpret_cast<const char*>(&kdValue), sizeof(float));
            paramData.replace(4, 4, kdBytes); // 4-8字节
        } else if (paramName == "mitKi") {
            float kiValue = static_cast<float>(value);
            QByteArray kiBytes(reinterpret_cast<const char*>(&kiValue), sizeof(float));
            paramData.replace(8, 4, kiBytes); // 8-12字节
        } else if (paramName == "mitMax") {
            float maxValue = static_cast<float>(value);
            QByteArray maxBytes(reinterpret_cast<const char*>(&maxValue), sizeof(float));
            paramData.replace(12, 4, maxBytes); // 12-16字节
        } else if (paramName == "mitMin") {
            float minValue = static_cast<float>(value);
            QByteArray minBytes(reinterpret_cast<const char*>(&minValue), sizeof(float));
            paramData.replace(16, 4, minBytes); // 16-20字节
        }
    } else if (mode == 0x01) { // 位置模式
        float posValue = static_cast<float>(value);
        QByteArray posBytes(reinterpret_cast<const char*>(&posValue), sizeof(float));
        paramData.replace(0, 4, posBytes);
    } else if (mode == 0x02) { // 速度模式
        float speedValue = static_cast<float>(value);
        QByteArray speedBytes(reinterpret_cast<const char*>(&speedValue), sizeof(float));
        paramData.replace(0, 4, speedBytes);
    } else if (mode == 0x03) { // 力矩模式
        float torqueValue = static_cast<float>(value);
        QByteArray torqueBytes(reinterpret_cast<const char*>(&torqueValue), sizeof(float));
        paramData.replace(0, 4, torqueBytes);
    } else if (mode == 0x04) { // 电流模式
        float currentValue = static_cast<float>(value);
        QByteArray currentBytes(reinterpret_cast<const char*>(&currentValue), sizeof(float));
        paramData.replace(0, 4, currentBytes);
    }

    data.append(paramData);

    if (currentCommMode == SERIAL_MODE && serial->isOpen()) {
        serial->write(data);
        qDebug() << "发送混合控制指令:" << QString(data.toHex(' '));
    } else if (currentCommMode == CAN_MODE && canOpenStatus) {
        // CAN
        sendCanData(data);//模式未传
    }

    updateStatus(QString("发送%1模式指令: %2 = %3")
                 .arg(getModeName(mode))
                 .arg(paramName)
                 .arg(value));
}
QString Widget::getModeName(quint8 mode) {
    switch(mode) {
        case MODE_POSITION: return "位置";
        case MODE_SPEED: return "速度";
        case MODE_TORQUE: return "力矩";
        case MODE_CURRENT: return "电流";
        case MODE_MIT: return "力位混合";
        default: return "未知";
    }
}
// ==================== 更新电机状态显示 ====================
void Widget::updateMotorStatus()
{
    static int counter = 0;
    counter++;

//    double position = currentPos;
//    double speed = currentSpeed;
//    double current = currentPhaseCurrent;
//    double torque  = currentTorque;
//    double motorTemp = currentMotorTem;
//    double mosTemp = currentMosTem;

//    // 更新显示
//    if (ui->realPosEdit) {
//        ui->realPosEdit->setText(QString::number(position, 'f', 2) );
//    }

//    if (ui->realSpeedEdit) {
//        ui->realSpeedEdit->setText(QString::number(speed, 'f', 2) );
//    }

//    if (ui->realSpeedEdit_1) {
//        ui->realSpeedEdit_1->setText(QString::number(speed, 'f', 2) );
//    }

//    if (ui->realTorqueEdit) {
//        ui->realTorqueEdit->setText(QString::number(torque, 'f', 2) );
//    }

//    if (ui->realPhaseCurrentEdit) {
//        ui->realPhaseCurrentEdit->setText(QString::number(current, 'f', 2) );
//    }

//    if (ui->realMotorTemEdit) {
//        ui->realMotorTemEdit->setText(QString::number(motorTemp, 'f', 1) );
//    }

//    if (ui->realMosTemEdit) {
//        ui->realMosTemEdit->setText(QString::number(mosTemp, 'f', 1) );
//    }

    qDebug() << "currentPos：" << currentPos;
    qDebug() << "currentSpeed" << currentSpeed;
    qDebug() << "currentPhaseCurrent" << currentPhaseCurrent;
    qDebug() << "currentTorque" << currentTorque;
    qDebug() << "currentMotorTem" << currentMotorTem;
    qDebug() << "currentMosTem" << currentMosTem;
    qDebug() << "电机状态已更新";
}

void Widget::on_clearVersionBtn_clicked()
{
    QMessageBox::StandardButton result = QMessageBox::question(
        this, tr("清除确认"), tr("确定要清除版本查询日志吗？"),
        QMessageBox::Yes | QMessageBox::No
    );
    if (result == QMessageBox::Yes) {
        ui->versionDisplay->clear();
        updateStatus("版本查询日志已清除");
    }
}

void Widget::on_clearUpgradeLogBtn_clicked()
{
    QMessageBox::StandardButton result = QMessageBox::question(
        this, tr("清除确认"), tr("确定要清除升级日志和收发数据吗？"),
        QMessageBox::Yes | QMessageBox::No
    );
    if (result == QMessageBox::Yes) {
        ui->logTextEdit->clear();
        ui->sendTextEdit->clear();
        ui->receiveTextEdit->clear();
        updateStatus("升级日志已清除");
    }
}

void Widget::fillCanConfig()
{
    ui->canPortBox->clear();
    ui->canPortBox->addItem("通道0");
    ui->canPortBox->addItem("通道1");
    ui->canBaudRateBox->clear();
    ui->canPortBox->clear();
    ui->canPortBox->addItem("通道0");
    ui->canPortBox->addItem("通道1");
    ui->canBaudRateBox->clear();

    ui->canBaudRateBox->addItem("(1M仲裁+5M数据)");

    ui->openCanButton->setText("打开CANFD设备");

//    canFdBaudMap.insert("(500K仲裁+5M数据)", qMakePair(104286U, 66055U));
//    canFdBaudMap.insert("(1M仲裁+2M数据)",    qMakePair(101946U, 66057U));
//    // 仲裁：1M     数据：5M
//    canFdBaudMap.insert("(1M仲裁+5M数据)",    qMakePair(101946U, 66055U));
//    // 仲裁：2M     数据：2M
//    canFdBaudMap.insert("(2M仲裁+2M数据)",    qMakePair(100606U, 66057U));
//    // 仲裁：2M     数据：5M
//    canFdBaudMap.insert("(2M仲裁+5M数据)",    qMakePair(100606U, 66055U));

//    for(auto key : canFdBaudMap.keys()){
//        ui->canBaudRateBox->addItem(key);
//    }

//    ui->openCanButton->setText("打开CANFD设备");
}

//QPair<BYTE, BYTE> Widget::getCanTiming(const QString &baudRate)
//{
//    return canBaudRateMap.contains(baudRate) ? canBaudRateMap[baudRate] : qMakePair<BYTE, BYTE>(0x00, 0x1C);
//}
QPair<quint32, quint32> Widget::getCanFdTiming(const QString &baudText)
{
    if(canFdBaudMap.contains(baudText)){
        return canFdBaudMap[baudText];
    }
    return qMakePair(104286U, 66055U);
}

QString Widget::getCanErrorInfo()
{
    if (canChannelHandle == INVALID_CHANNEL_HANDLE) {
        return "通道未初始化";
    }
    ZCAN_CHANNEL_ERR_INFO errInfo;
    if (ZCAN_ReadChannelErrInfo(canChannelHandle, &errInfo) != STATUS_OK) {
        return "获取错误信息失败";
    }
    QString errStr = "错误码: 0x" + QString::number(errInfo.error_code, 16);
    errStr += ", 消极错误: " + QString::number(errInfo.passive_ErrData[0]);
    errStr += ", 仲裁丢失: " + QString::number(errInfo.arLost_ErrData);
    return errStr;
}

bool Widget::openCanDevice()
{
        if (canDeviceHandle != INVALID_DEVICE_HANDLE) {
            ZCAN_CloseDevice(canDeviceHandle);
        }
        canDeviceHandle = ZCAN_OpenDevice(ZCAN_USBCANFD_200U, 0, 0);
        if (canDeviceHandle == INVALID_DEVICE_HANDLE) {
            QMessageBox::critical(nullptr, tr("CANFD错误"), tr("打开设备失败！"));
            return false;
        }

        int chn_idx = ui->canPortBox->currentIndex();
        qDebug() << "通道：" << chn_idx ;
        char path[24];
        // 仲裁段波特：1Mbps
        sprintf(path, "%d/canfd_abit_baud_rate", chn_idx);
        if (ZCAN_SetValue(canDeviceHandle, path, "1000000") != STATUS_OK) {
            qDebug() << "设置仲裁1M波特失败";
        }
        // 数据段波特：5Mbps
        sprintf(path, "%d/canfd_dbit_baud_rate", chn_idx);
        if (ZCAN_SetValue(canDeviceHandle, path, "5000000") != STATUS_OK) {
            qDebug() << "设置数据5M波特失败";
        }

        ZCAN_CHANNEL_INIT_CONFIG canInit;
        memset(&canInit, 0, sizeof(canInit));
        canInit.can_type = TYPE_CANFD;
        canInit.canfd.mode = 0;
        canInit.canfd.acc_code = 0;
        canInit.canfd.acc_mask = 0x000000FF;
        canInit.canfd.filter = 0;

//      canInit.canfd.abit_timing = 0x0005000F;
//      canInit.canfd.dbit_timing = 0x0001000F;

        canChannelHandle = ZCAN_InitCAN(canDeviceHandle, chn_idx, &canInit);
        if (canChannelHandle == INVALID_CHANNEL_HANDLE) {
             qDebug() << "初始化通道失败：" << canChannelHandle ;
            ZCAN_CloseDevice(canDeviceHandle);
            canDeviceHandle = INVALID_DEVICE_HANDLE;
            return false;
        }

        sprintf(path, "%d/initenal_resistance", chn_idx);
        int res = ZCAN_SetValue(canDeviceHandle, path, "1");
        if (res != STATUS_OK) {
               qDebug() << "终端电阻启用失败，返回码：" << res;
               QMessageBox::warning(this, "提示", "终端电阻启用失败，可能影响通信！");
           } else {
               qDebug() << "终端电阻启用成功（通道" << chn_idx << "）";
           }
        if (ZCAN_StartCAN(canChannelHandle) != STATUS_OK) {
            QMessageBox::critical(nullptr, tr("CANFD错误"), tr("通道启动失败！"));
            qDebug() << "启动失败（通道" << chn_idx << "）";
            ZCAN_ResetCAN(canChannelHandle);
            ZCAN_CloseDevice(canDeviceHandle);
            canChannelHandle = INVALID_CHANNEL_HANDLE;
            canDeviceHandle = INVALID_DEVICE_HANDLE;
            return false;
        }

        canOpenStatus = true;
        ui->openCanButton->setText("关闭CANFD设备");
        ui->statusLabel_1->setText("CANFD已连接");
        ui->canPortBox->setEnabled(false);
        ui->canBaudRateBox->setEnabled(false);
        ui->canIdEdit->setEnabled(false);

        canReceiveTimer->blockSignals(false);
        canReceiveTimer->start();
        qDebug() << "接收定时器已启动，间隔1ms";
        qDebug() << "打开CANFD设备成功，通道：" << chn_idx;
        return true;
}

void Widget::closeCanDevice()
{
    if (canChannelHandle != INVALID_CHANNEL_HANDLE) {
        ZCAN_ResetCAN(canChannelHandle);
        canChannelHandle = INVALID_CHANNEL_HANDLE;
    }
    canReceiveTimer->stop();
    canReceiveTimer->blockSignals(true);
    if (canDeviceHandle != INVALID_DEVICE_HANDLE) {
        ZCAN_CloseDevice(canDeviceHandle);
        canDeviceHandle = INVALID_DEVICE_HANDLE;
    }

    canOpenStatus = false;
    ui->openCanButton->setText("打开CANFD设备");
    ui->openCanButton->setStyleSheet(R"(
           QPushButton {
               background-color: white;
               color: black;
           }
       )");
    ui->statusLabel_1->setText("CAN已断开");
    ui->startUpgradeButton->setEnabled(false);
    ui->fileButton->setEnabled(true);
    ui->viewButton->setEnabled(false);

      ui->canPortBox->setEnabled(true);
      ui->canBaudRateBox->setEnabled(true);
      ui->canIdEdit->setEnabled(true);
}

void Widget::on_openCanButton_clicked()
{
       if (canOpenStatus)
       {
           closeCanDevice();
       }
       else
       {
             if (ui->connectTabWidget->currentIndex() != 0)
             {
                 QMessageBox::warning(this, "提示", "请先切换到【CANFD通信】标签页，再打开CANFD设备！");
                 return;
             }
                  if (OpenStatus)
                  {
                      QMessageBox::information(this, tr("CANFD连接失败"),
                                             tr("串口设备已打开，请先关闭后再打开CANFD！"),
                                             QMessageBox::Ok);
                     return; // 直接返回，不执行后续打开逻辑
                  }
           if (openCanDevice())
           {
               ui->openCanButton->setText("关闭CANFD设备");
               ui->openCanButton->setStyleSheet(R"(
                   QPushButton {
                       background-color: #4CAF50;
                       color: white;
                   }
               )");
           }
       }
}

void Widget::on_canIdEdit_editingFinished()
{
    if (canOpenStatus) {
            QMessageBox::warning(this, tr("CAN警告"), tr("CAN设备已打开，无法修改ID！请先关闭设备。"));
            ui->canIdEdit->setText(QString("0x%1").arg(canId, 8, 16, QChar('0')));
            return;
        }

        QString input = ui->canIdEdit->text().trimmed();
        int pos = 0;
        if (ui->canIdEdit->validator()->validate(input, pos) != QValidator::Acceptable) {
            QMessageBox::warning(this, tr("CAN警告"), tr("无效的十六进制格式！请输入如0x001或1的格式"));
            ui->canIdEdit->setText(QString("0x%1").arg(canId, 8, 16, QChar('0')));
            return;
        }

        bool ok = false;
        quint32 newCanId = 0;
        if (input.startsWith("0x", Qt::CaseInsensitive)) {
            newCanId = input.mid(2).toUInt(&ok, 16);
        } else {
            newCanId = input.toUInt(&ok, 16);
        }

        if (ok && newCanId <= 0x000000FF) {
            canId = newCanId;
            QString displayText = QString("0x%1").arg(canId, 8, 16, QChar('0'));
            ui->canIdEdit->setText(displayText);
            updateStatus(tr("CAN扩展ID已更新为：%1").arg(displayText));
            qDebug() << "CAN ID更新成功：" << displayText;
        } else {
            QMessageBox::warning(this, tr("CAN警告"), tr("无效的CAN ID！范围：0~0x000000FF"));
            ui->canIdEdit->setText(QString("0x%1").arg(canId, 8, 16, QChar('0')));
        }
}

void Widget::sendCanData(const QByteArray &data)
{
  if (currentCommMode != CAN_MODE) {
      updateStatus("当前为非CAN模式，无法通过CAN发送");
      return;
  }

  if (!canOpenStatus || canChannelHandle == INVALID_CHANNEL_HANDLE) {
      updateStatus("CANFD未打开，发送失败");
      return;
  }

  // CAN FD 单帧最大 64 字节
      const int MAX_FD_DATA_LEN = 64;
      int dataPos = 0;
      int totalLen = data.size();
      const int FRAME_INTERVAL = 2;

      while (dataPos < totalLen) {
        int currentFrameLen = qMin(MAX_FD_DATA_LEN, totalLen - dataPos);
        QByteArray currentFrameData = data.mid(dataPos, currentFrameLen);

        canfd_frame fdFrame;
        memset(&fdFrame, 0, sizeof(canfd_frame));
        fdFrame.can_id = MAKE_CAN_ID(canId, 0, 0, 0);
        fdFrame.len = currentFrameLen;
        fdFrame.flags = 0x00;

        memcpy(fdFrame.data, currentFrameData.data(), currentFrameLen);

        ZCAN_TransmitFD_Data txFdData;
        memset(&txFdData, 0, sizeof(txFdData));
        txFdData.frame = fdFrame;
        txFdData.transmit_type = 0; // 0=正常发送

        UINT sendCnt = ZCAN_TransmitFD(canChannelHandle, &txFdData, 1);
        if (sendCnt > 0) {
            QString timeStr = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
            QString log = QString("%1 [CANFD TX] 数据:%2")
                         .arg(timeStr).arg(QString(currentFrameData.toHex().toUpper()));
            ui->sendTextEdit->append(log);
            QThread::msleep(FRAME_INTERVAL);
        } else {
            updateStatus(QString("CANFD发送失败！%1").arg(getCanErrorInfo()));
            break;
        }
        dataPos += currentFrameLen;
    }
}

void Widget::receiveCanData()
{
    if (!canOpenStatus || canChannelHandle == INVALID_CHANNEL_HANDLE)
    {
        canReceiveTimer->start();
        return;
    }

    //ZCAN_ReceiveFD_Data rxFdData[5];
    //UINT recvCnt = ZCAN_ReceiveFD(canChannelHandle, rxFdData, 5, 10);
    ZCAN_ReceiveFD_Data rxFdData[1];
    UINT recvCnt = ZCAN_ReceiveFD(canChannelHandle, rxFdData, 1, 1);
    //qDebug() << "【接收轮询】本次读到帧数：" << recvCnt;

    if (recvCnt > 0)
    {
        for (UINT i = 0; i < recvCnt; i++)
        {
            canfd_frame rxFrame = rxFdData[i].frame;
            bool isExtFrame = (rxFrame.can_id & CAN_EFF_FLAG) != 0;
            quint32 recvPureId = rxFrame.can_id & CAN_EFF_MASK;

#ifdef QT_DEBUG
            QString frameType = isExtFrame ? "扩展帧" : "标准帧";
            qDebug() << "帧类型：" << frameType << "  原始ID：0x" << QString::number(recvPureId, 16);
#endif
            // 只处理标准帧 + 匹配目标ID
            if (!isExtFrame && recvPureId == canId)
            {
                QByteArray recvBuf(reinterpret_cast<const char*>(rxFrame.data), rxFrame.len);
                recvBuffer.append(recvBuf);
#ifdef QT_DEBUG
                qDebug() << "匹配成功，接收数据：" << recvBuf.toHex();
#endif
            }
        }
        processRecvBuffer();
    }

// // 强制重启定时器，持续轮询
//    canReceiveTimer->start();
}

// -------------------------- 串口操作 --------------------------
void Widget::fillPortsInfo()
{
    ui->baudRateBox->clear();
    ui->baudRateBox->addItem(QStringLiteral("9600"), QSerialPort::Baud9600);
    ui->baudRateBox->addItem(QStringLiteral("19200"), QSerialPort::Baud19200);
    ui->baudRateBox->addItem(QStringLiteral("38400"), QSerialPort::Baud38400);
    ui->baudRateBox->addItem(QStringLiteral("57600"), QSerialPort::Baud57600);
    ui->baudRateBox->addItem(QStringLiteral("115200"), QSerialPort::Baud115200);
/*
    ui->baudRateBox->addItem(QStringLiteral("230400"), QSerialPort::Baud230400);
    ui->baudRateBox->addItem(QStringLiteral("460800"), QSerialPort::Baud460800);
    ui->baudRateBox->addItem(QStringLiteral("921600"), QSerialPort::Baud921600);
    ui->baudRateBox->addItem(QStringLiteral("1000000"), QSerialPort::Baud1000000);
    ui->baudRateBox->addItem(QStringLiteral("2000000"), QSerialPort::Baud2000000);
    ui->baudRateBox->addItem(QStringLiteral("3000000"), QSerialPort::Baud3000000);
    ui->baudRateBox->addItem(QStringLiteral("4000000"), QSerialPort::Baud4000000);
    ui->baudRateBox->addItem(QStringLiteral("5000000"), QSerialPort::Baud5000000);
*/

    ui->baudRateBox->addItem(QStringLiteral("Custom"));
    ui->baudRateBox->setCurrentIndex(4);

    ui->dataBitsBox->clear();
    ui->dataBitsBox->addItem(QStringLiteral("8"), QSerialPort::Data8);
    ui->dataBitsBox->addItem(QStringLiteral("7"), QSerialPort::Data7);
    ui->dataBitsBox->addItem(QStringLiteral("6"), QSerialPort::Data6);
    ui->dataBitsBox->addItem(QStringLiteral("5"), QSerialPort::Data5);
    ui->dataBitsBox->setCurrentIndex(0);

    ui->parityBox->clear();
    ui->parityBox->addItem(QStringLiteral("None"), QSerialPort::NoParity);
    ui->parityBox->addItem(QStringLiteral("Even"), QSerialPort::EvenParity);
    ui->parityBox->addItem(QStringLiteral("Odd"), QSerialPort::OddParity);
    ui->parityBox->addItem(QStringLiteral("Space"), QSerialPort::SpaceParity);
    ui->parityBox->addItem(QStringLiteral("Mark"), QSerialPort::MarkParity);
    ui->parityBox->setCurrentIndex(0);

    ui->stopBitsBox->clear();
    ui->stopBitsBox->addItem(QStringLiteral("1"), QSerialPort::OneStop);
    ui->stopBitsBox->addItem(QStringLiteral("1.5"), QSerialPort::OneAndHalfStop);
    ui->stopBitsBox->addItem(QStringLiteral("2"), QSerialPort::TwoStop);
    ui->stopBitsBox->setCurrentIndex(0);

    ui->flowControlBox->clear();
    ui->flowControlBox->addItem(QStringLiteral("None"), QSerialPort::NoFlowControl);
    ui->flowControlBox->addItem(QStringLiteral("RTS/CTS"), QSerialPort::HardwareControl);
    ui->flowControlBox->addItem(QStringLiteral("XON/XOFF"), QSerialPort::SoftwareControl);
    ui->flowControlBox->setCurrentIndex(0);
}

void Widget::checkCustomBaudRate(int idx)
{
    const bool isCustom = !ui->baudRateBox->itemData(idx).isValid();
    ui->baudRateBox->setEditable(isCustom);
    if (isCustom) {
        ui->baudRateBox->setValidator(intValidator);
    }
}

void Widget::closeSerialDevice()
{
    serial->close();
      OpenStatus = false;
      ui->openButton->setText("打开串口");
      ui->openButton->setStyleSheet(R"(
          QPushButton {
              background-color: white;
              color: black;
          }
      )");
      ui->statusLabel_1->setText("串口已断开");
      ui->portBox->setEnabled(true);
      ui->baudRateBox->setEnabled(true);
      ui->dataBitsBox->setEnabled(true);
      ui->parityBox->setEnabled(true);
      ui->stopBitsBox->setEnabled(true);
      ui->flowControlBox->setEnabled(true);
      ui->refreshButton->setEnabled(true);
      ui->fileButton->setEnabled(true);
      ui->viewButton->setEnabled(false);

      ui->openButton->setEnabled(true);
}

void Widget::on_openButton_clicked()
{

    if (OpenStatus)
    {
            closeSerialDevice();
            return;
    }
    else
    {
        if (ui->connectTabWidget->currentIndex() != 1) {
                QMessageBox::warning(this, "提示", "请在【串口通信】标签页打开串口！");
                return;
            }
        if (canOpenStatus)
                        {
                            QMessageBox::information(this, tr("串口连接失败"),
                                                   tr("CAN设备已打开，请先关闭后再打开串口！"),
                                                   QMessageBox::Ok);
                           return;
                        }
                settings.name = ui->portBox->currentData().toString();
                if (settings.name.isEmpty() || ui->portBox->currentText() == "无可用串口") {
                    QMessageBox::warning(this, tr("串口错误"), tr("请选择有效的串口！"));
                    return;
                }

                if (ui->baudRateBox->currentText() == "Custom") {
                    bool ok;
                    const qint32 customBaud = ui->baudRateBox->currentText().toInt(&ok);
                    if (!ok || customBaud < 1200 || customBaud > 4000000) { // 限制有效波特率范围
                        QMessageBox::critical(this, tr("串口错误"), tr("无效的自定义波特率！范围：1200~4000000"));
                        return;
                    }
                    settings.baudRate = customBaud;
                    settings.stringBaudRate = QString::number(customBaud);
                } else {
                    settings.baudRate = static_cast<QSerialPort::BaudRate>(
                                ui->baudRateBox->itemData(ui->baudRateBox->currentIndex()).toInt());
                    settings.stringBaudRate = ui->baudRateBox->currentText();
                }

                settings.dataBits = static_cast<QSerialPort::DataBits>(
                            ui->dataBitsBox->itemData(ui->dataBitsBox->currentIndex()).toInt());
                settings.stringDataBits = ui->dataBitsBox->currentText();
                settings.parity = static_cast<QSerialPort::Parity>(
                            ui->parityBox->itemData(ui->parityBox->currentIndex()).toInt());
                settings.stringParity = ui->parityBox->currentText();
                settings.stopBits = static_cast<QSerialPort::StopBits>(
                            ui->stopBitsBox->itemData(ui->stopBitsBox->currentIndex()).toInt());
                settings.stringStopBits = ui->stopBitsBox->currentText();
                settings.flowControl = static_cast<QSerialPort::FlowControl>(
                            ui->flowControlBox->itemData(ui->flowControlBox->currentIndex()).toInt());
                settings.stringFlowControl = ui->flowControlBox->currentText();

                serial->setPortName(settings.name);
                if (!serial->open(QIODevice::ReadWrite)) {
                    QString errMsg = tr("打开串口%1失败！可能原因：\n").arg(settings.name);
                    errMsg += tr("1. 串口线接触不良或设备未通电\n");
                    errMsg += tr("2. 串口被其他程序占用（如串口助手）\n");
                    errMsg += tr("3. 端口选择错误（无此串口）\n");
                    errMsg += tr("4. 权限不足（需管理员身份运行）");
                    QMessageBox::critical(this, tr("串口错误"), errMsg);
                    return;
                }

        if (!serial->setBaudRate(settings.baudRate)) {
            QMessageBox::critical(this, tr("串口错误"), tr("设置波特率失败：%1").arg(serial->errorString()));
            serial->close();
            return;
        }
        if (!serial->setDataBits(settings.dataBits)) {
            QMessageBox::critical(this, tr("串口错误"), tr("设置数据位失败：%1").arg(serial->errorString()));
            serial->close();
            return;
        }
        if (!serial->setParity(settings.parity)) {
            QMessageBox::critical(this, tr("串口错误"), tr("设置校验位失败：%1").arg(serial->errorString()));
            serial->close();
            return;
        }
        if (!serial->setStopBits(settings.stopBits)) {
            QMessageBox::critical(this, tr("串口错误"), tr("设置停止位失败：%1").arg(serial->errorString()));
            serial->close();
            return;
        }
        if (!serial->setFlowControl(settings.flowControl)) {
            QMessageBox::critical(this, tr("串口错误"), tr("设置流控失败：%1").arg(serial->errorString()));
            serial->close();
            return;
        }

        OpenStatus = true;
        ui->openButton->setText("关闭串口");
        ui->openButton->setStyleSheet(R"(QPushButton { background-color: #4CAF50; color: white; })");
        ui->statusLabel_1->setText(tr("串口已连接：%1 %2").arg(settings.name).arg(settings.stringBaudRate));
        ui->portBox->setEnabled(false);
        ui->baudRateBox->setEnabled(false);
        ui->dataBitsBox->setEnabled(false);
        ui->parityBox->setEnabled(false);
        ui->stopBitsBox->setEnabled(false);
        ui->flowControlBox->setEnabled(false);
        ui->refreshButton->setEnabled(false);
        ui->fileButton->setEnabled(true);
        if (!fileLocation.isEmpty()) ui->viewButton->setEnabled(true);

        qDebug() << "打开uart设备成功，currentCommMode =" << currentCommMode;
    }
}

void Widget::on_refreshButton_clicked()
{
//    fillPortsCom();
    refreshSerialPorts();
}

void Widget::readData()
{
    if (currentCommMode != SERIAL_MODE || !OpenStatus) {
        return;
    }

    QByteArray newData = serial->readAll();
    if (newData.isEmpty()) {
        return;
    }

//    QString timeStr = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
//    ui->receiveTextEdit->append(timeStr + " [Serial RX] " + newData.toHex().toUpper());
    recvBuffer.append(newData);
    processRecvBuffer();
}

void Widget::sendSerialData(const QByteArray &data)
{
       if (currentCommMode != SERIAL_MODE) {
           updateStatus("当前为非串口模式，无法通过串口发送");
           return;
       }

       if (!OpenStatus) {
           updateStatus("串口未打开，发送失败");
           return;
       }

    serial->write(data);
    serial->flush();
    QString timeStr = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    ui->sendTextEdit->append(timeStr + " [Serial TX] " + data.toHex().toUpper());
}

void Widget::on_fileButton_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("选择固件文件"),
                                                  "", tr("Bin Files (*.bin)"));
    if (fileName.isEmpty()) {
        return;
    }
    if (openFirmwareFile(fileName)) {
        fileLocation = fileName;
        ui->filePathLabel->setText(fileName);
        ui->fileSizeLabel->setText(QString("%1 KB").arg(binSize / 1024.0, 0, 'f', 2));
        bool isCommOpen = (currentCommMode == SERIAL_MODE && OpenStatus) ||
                          (currentCommMode == CAN_MODE && canOpenStatus);
        ui->startUpgradeButton->setEnabled(isCommOpen);
        ui->viewButton->setEnabled(true);
    }
}

bool Widget::openFirmwareFile(const QString &fileName)
{
    if (firmwareFile && firmwareFile->isOpen()) {
        firmwareFile->close();
        delete firmwareFile;
    }
    firmwareFile = new QFile(fileName);
    if (!firmwareFile->open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, tr("文件错误"),
                             tr("无法打开固件文件：%1").arg(firmwareFile->errorString()));
        delete firmwareFile;
        firmwareFile = nullptr;
        return false;
    }
    binSize = firmwareFile->size();
    if (binSize == 0) {
        QMessageBox::warning(this, tr("文件警告"), tr("所选固件文件为空！"));
        return false;
    }
    updateStatus(tr("固件文件打开成功：%1 KB").arg(binSize / 1024.0, 0, 'f', 2));

    //decrypt .bin file for download real data
    //each encrypt block size 160 byte
    int nBlockSize = 160;
    quint64 uBinSize = binSize;
    int nReadTimes = uBinSize/nBlockSize + 1;
    char pBuff[nBlockSize];
    QFile* binFile=nullptr;
    aes_128_decrypt aes128Decrypt;

    m_binArr.clear();
    for(int i = 0; i< nReadTimes; i++)
    {
        memset(pBuff, 0, sizeof(pBuff));
        firmwareFile->seek(i * nBlockSize);
        if(i==nReadTimes - 1)
        {
          nBlockSize = uBinSize % nBlockSize;
          char pBuff2[nBlockSize];
          firmwareFile->read(pBuff2, nBlockSize);
          memcpy(pBuff, pBuff2, sizeof(pBuff2));
        }
        else
        {
          firmwareFile->read(pBuff, nBlockSize);
        }

        //要解密的内容
        unsigned char sourceMsg[nBlockSize];
        unsigned char decrypt_data[nBlockSize];

        memset(decrypt_data, 0, nBlockSize);
        memcpy(sourceMsg, pBuff, nBlockSize);

        aes128Decrypt.PrintData("sourceMsg", sourceMsg, nBlockSize);
        aes128Decrypt.my_aes_decrypt(sourceMsg, decrypt_data, nBlockSize);

        //保存加密后的文件
        //获取系统临时目录的路径
        QString tempDirPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        m_strNewPath =tempDirPath + "/decrypt.bin";

        binFile = new QFile(m_strNewPath);
        if (!binFile->open(QIODevice::WriteOnly | QIODevice::Append)) {
            QMessageBox::critical(this, tr("文件错误"),
                                 tr("无法打开固件文件：%1").arg(binFile->errorString()));
            delete binFile;
            binFile = nullptr;
            return false;
        }
        else
        {
            QByteArray baDecrypt;
            for(int i = 0; i< nBlockSize; i++)
            {
                baDecrypt.append(decrypt_data[i]);
                if(m_binArr.size()<1000)
                  m_binArr.append(decrypt_data[i]);
            }
            binFile->write(baDecrypt , nBlockSize);

            binFile->close();
            delete binFile;
            binFile = nullptr;
        }
    }

    //open decrypt .bin file for...
    firmwareFile->close();
    firmwareFile = new QFile(m_strNewPath);
    if (!firmwareFile->open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, tr("文件错误"),
                             tr("无法打开固件文件：%1").arg(firmwareFile->errorString()));
        delete firmwareFile;
        firmwareFile = nullptr;
        return false;
    }
    binSize = firmwareFile->size();
    if (binSize == 0) {
        QMessageBox::warning(this, tr("文件警告"), tr("所选固件文件为空！"));
        return false;
    }

    return true;
}

void Widget::on_viewButton_clicked()
{
    if (!firmwareFile || !firmwareFile->isOpen()) {
        QMessageBox::warning(this, tr("文件警告"), tr("固件文件未打开！"));
        return;
    }
    //firmwareFile->seek(0);
    QByteArray data = m_binArr.mid(0, 999);  //firmwareFile->read(1024); QByteArray最大支持1000byte
    QString hexStr;
    for (int i = 0; i < data.size(); ++i) {
        hexStr += QString("%1 ").arg(static_cast<quint8>(data[i]), 2, 16, QChar('0')).toUpper();
        if ((i + 1) % 16 == 0) {
            hexStr += "\n";
        }
    }
    QMessageBox::information(this, tr("固件内容预览（前1024字节）"), hexStr);
}

void Widget::on_startUpgradeButton_clicked()
{
    bool isCommOpen = (currentCommMode == SERIAL_MODE && OpenStatus) ||
                      (currentCommMode == CAN_MODE && canOpenStatus);
    if (!isCommOpen) {
        QMessageBox::warning(this, tr("升级警告"),
                           tr("请先打开%1设备！").arg(currentCommMode == SERIAL_MODE ? "串口" : "CAN"));
        return;
    }
    if (!firmwareFile || !firmwareFile->isOpen()) {
        if (!openFirmwareFile(fileLocation)) {
            return;
        }
    }
    resetUpgradeState();
    updateStatus("开始IAP升级流程...");
    upgradeState = STATE_SILENCE;
    ui->startUpgradeButton->setEnabled(false);
    ui->cancelButton->setEnabled(true);
    sendSilenceCommand(true);
}

void Widget::on_cancelButton_clicked()
{
    if (upgradeState != STATE_IDLE) {
        sendSilenceCommand(false);
    }
    resetUpgradeState();
    updateStatus("IAP升级已取消");
    ui->startUpgradeButton->setEnabled(true);
    ui->cancelButton->setEnabled(false);
}

void Widget::resetUpgradeState()
{
    upgradeState = STATE_IDLE;
    timer->stop();
    retryCount = 0;
    currentPacketIndex = 0;
    totalSent = 0;
    currentPacketSize = 0;
    recvBuffer.clear();
    updateProgress(0);
    if (firmwareFile && firmwareFile->isOpen()) {
        firmwareFile->seek(0);
    }
}

QByteArray Widget::buildPacket(quint8 index, quint8 cmdId, quint8 action, const QByteArray &payload)
{
    QByteArray packet;
    packet.append(PACKET_HEADER1);
    packet.append(PACKET_HEADER2);
    packet.append(index);
    packet.append(cmdId);
    packet.append(action);
    quint16 payloadLen = payload.size();
    packet.append(static_cast<quint8>((payloadLen >> 8) & 0xFF));
    packet.append(static_cast<quint8>(payloadLen & 0xFF));
    packet.append(payload);
    packet.append(calculateChecksum(packet));
    return packet;
}

quint8 Widget::calculateChecksum(const QByteArray &data)
{
    quint8 checksum = 0;
    for (char c : data) {
        checksum += static_cast<quint8>(c);
    }
    return checksum;
}

void Widget::sendSilenceCommand(bool enable)
{
    lastSentCmdId = CMD_SILENCE;
    updateStatus(enable ? "发送升级静默指令..." : "发送解除静默指令...");
    quint8 action = enable ? 1 : 0;
    QByteArray packet = buildPacket(0, CMD_SILENCE, action, QByteArray());

    if (currentCommMode == SERIAL_MODE) {

        sendSerialData(packet);
    } else if (currentCommMode == CAN_MODE) {

        sendCanData(packet);
    }
    timer->start();

}

void Widget::sendPrepareCommand()
{
    lastSentCmdId = CMD_PREPARE;
    updateStatus("发送升级准备包...");
    QByteArray payload;
    quint32 fileSize = binSize;
    payload.append(static_cast<quint8>((fileSize >> 24) & 0xFF));
    payload.append(static_cast<quint8>((fileSize >> 16) & 0xFF));
    payload.append(static_cast<quint8>((fileSize >> 8) & 0xFF));
    payload.append(static_cast<quint8>(fileSize & 0xFF));
    QByteArray packet = buildPacket(0, CMD_PREPARE, 1, payload);
    if (currentCommMode == SERIAL_MODE) {
        sendSerialData(packet);
    } else if (currentCommMode == CAN_MODE) {
        sendCanData(packet);
    }
    timer->start();
}

void Widget::sendDataCommand(quint8 index, const QByteArray &data)
{
    lastSentCmdId = CMD_DATA;
    updateStatus(QString("发送数据包 %1...").arg(index));
    currentPacketSize = data.size();
    QByteArray packet = buildPacket(index, CMD_DATA, 1, data);
    if (currentCommMode == SERIAL_MODE) {
        sendSerialData(packet);
        QString timeStr = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
        QString dataHex = packet.left(16).toHex().toUpper() + (packet.size() > 16 ? "..." : "");
        ui->sendTextEdit->append(QString("%1 [TX] 包%2: %3 (长度: %4字节)")
                                .arg(timeStr).arg(index).arg(dataHex).arg(packet.size()));
    } else if (currentCommMode == CAN_MODE) {
        sendCanData(packet);
    }
    timer->start();
}

void Widget::sendNextPacket()
{
    if (!firmwareFile || !firmwareFile->isOpen()) {
        updateStatus("固件文件未打开");
        upgradeState = STATE_ERROR;
        ui->startUpgradeButton->setEnabled(true);
        ui->cancelButton->setEnabled(false);
        return;
    }
    quint64 offset = (currentPacketIndex - 1) * 1024;
    if (offset >= binSize) {
        updateStatus("所有数据包已发送完毕");

        if(firmwareFile->exists())
        {
            firmwareFile->close();
            firmwareFile->remove(m_strNewPath);
        }
        return;
    }
    QByteArray data = readFirmwareData(offset, 1024);
    if (data.isEmpty() && offset < binSize) {
        updateStatus("读取固件数据失败");
        upgradeState = STATE_ERROR;
        ui->startUpgradeButton->setEnabled(true);
        ui->cancelButton->setEnabled(false);
        return;
    }
    sendDataCommand(currentPacketIndex, data);
}

QByteArray Widget::readFirmwareData(qint64 offset, qint64 maxSize)
{
  //bool bOffsetReadOK = true;
  if (!firmwareFile || !firmwareFile->isOpen()) {
      return QByteArray();
  }

  if (!firmwareFile->seek(offset)) {
      updateStatus(QString("固件文件定位失败：偏移%1字节").arg(offset));
      return QByteArray();
  }
  return firmwareFile->read(maxSize);

/*
  //判断数据结尾
  if(m_binArr.at(offset)=='\0')
    m_nTryReadT++;
  if(m_nTryReadT>0)
  {
    int nReadTimes=1;
    while(offset + nReadTimes < (qint64)m_uBinSize)
    {
      if(m_binArr.at(offset + nReadTimes)=='\0')
        nReadTimes++;

      if(nReadTimes>0x0f)
      {
          bOffsetReadOK = false;
          break;
      }
    }
  }
  if(!bOffsetReadOK)
  {
      updateStatus(QString("固件文件定位失败：偏移%1字节").arg(offset));
      return QByteArray();
  }

  return m_binArr.mid(offset, maxSize);
  */

}

// -------------------------- 接收数据解析 --------------------------
void Widget::processRecvBuffer()
{
    if (recvBuffer.size() > 4096) {
        updateStatus("接收缓存溢出，清空无效数据");
        recvBuffer.clear();
        return;
    }

    while (recvBuffer.size() >= 8) //最小应用层数据包长度（AA55头+6字节固定字段+1字节校验）
    {
        int headerIdx = recvBuffer.indexOf(QByteArray::fromHex("AA55"));
        if (headerIdx < 0) {
            recvBuffer.clear();//无帧头，清空无效数据
            break;
        } else if (headerIdx > 0) {
            recvBuffer.remove(0, headerIdx);
            continue;
        }
        // 解析数据包长度（数据长度字段在第5-6字节，大端）
        quint16 dataLen = (static_cast<quint8>(recvBuffer[5]) << 8) | static_cast<quint8>(recvBuffer[6]);
        quint16 fullPacketLen = 7 + dataLen + 1;// 7字节固定头 + 数据长度 + 1字节校验
        if (recvBuffer.size() < fullPacketLen) {
            break;//数据不完整，等待后续帧
        }

        QByteArray fullPacket = recvBuffer.left(fullPacketLen);
        recvBuffer.remove(0, fullPacketLen);
        quint8 cmdId = 0, action = 0, index = 0;
        if (parseResponse(fullPacket, cmdId, action, index, dataLen))
        {
            if(cmdId == CMD_COMBINED_DATA)//曲线绘制
            {
                if(m_PauseCapture)
                    DrawRealTimeCurve(cmdId, action, fullPacket, dataLen);
                return;
            }
            else if (cmdId != CMD_COMBINED_DATA)
            { //非合并包才打印
                QString timeStr = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
                QString dataHex = fullPacket.toHex().toUpper();
                QString rxFlag;
                // 根据当前通信模式选择RX标识
                if (currentCommMode == CAN_MODE) {
                    rxFlag = "[CAN RX]";
                } else if (currentCommMode == SERIAL_MODE) {
                    rxFlag = "[Serial RX]";
                } else {
                    rxFlag = "[Unknown RX]"; // 异常模式容错
                }

                QString log = QString("%1 %2 数据:%3\n").arg(timeStr).arg(rxFlag).arg(dataHex);
                ui->receiveTextEdit->append(log);
            }

            processResponse(cmdId, action, index, fullPacket, dataLen);
        }
        else
        {
            // 解析失败，重试逻辑
            retryCount++;
            if (retryCount >= 2) {
                updateStatus("多次解析失败，操作终止");
                upgradeState = STATE_ERROR;
                timer->stop();
                ui->startUpgradeButton->setEnabled(true);
                ui->cancelButton->setEnabled(false);
                retryCount = 0;
            } else {
                switch (upgradeState) {
                case STATE_SILENCE: sendSilenceCommand(true); break;
                case STATE_PREPARING: sendPrepareCommand(); break;
                case STATE_SENDING_DATA: sendNextPacket(); break;
                default: break;
                }
            }
        }
    }
}

int Widget::getExpectedLen(quint8 cmdId)
{
    switch (cmdId) {
    case CMD_CALIB_MODE: return 0;
    case CMD_ELEC_ANGLE_ZERO: return 4;
//    case CMD_MECH_LIMIT: return 16;
    case CMD_READ_ELEC_LIMIT: return 8;
//    case CMD_READ_ENCODER: return 2;
    default: return -1;
    }
}

bool Widget::parseResponse(const QByteArray &data, quint8 &cmdId, quint8 &action, quint8 &index, quint16 &length)
{
    if (data.size() < 8) {
            qDebug() << "[解析失败] 数据包长度不足8字节";
            return false;
    }
    //帧头校验（严格匹配AA55）
    if (static_cast<quint8>(data[0]) != PACKET_HEADER1 ||
        static_cast<quint8>(data[1]) != PACKET_HEADER2) {
        qDebug() << "[解析失败] 帧头错误：" << QString(data.mid(0,2).toHex().toUpper());
        return false;
    }

    index = static_cast<quint8>(data[2]);
    cmdId = static_cast<quint8>(data[3]);
    action = static_cast<quint8>(data[4]);
    length = (static_cast<quint8>(data[5]) << 8) | static_cast<quint8>(data[6]);

    if (data.size() != 7 + length + 1) return false;
    QByteArray content = data.left(data.size() - 1);
    if (calculateChecksum(content) != static_cast<quint8>(data[data.size() - 1]))
      return false;

    if ((cmdId == CMD_HW_VERSION || cmdId == CMD_SW_VERSION) && length != 64) {
        qDebug() << "失败1";
        return false;
    }

    bool lenValid = true;
    switch (cmdId) {
        case CMD_CALIB_MODE: lenValid = (length == 0); break;
    //  case CMD_MECH_LIMIT: lenValid = (length == 16); break;
        case CMD_READ_ELEC_LIMIT: lenValid = (length == 0); break;
    //  case CMD_READ_ENCODER: lenValid = (length == 2); break;
        default: break;
    }
    if (!lenValid) {
        updateStatus(QString("解析错误：指令0x%1要求长度%2，实际%3")
                     .arg(cmdId, 2, 16, QChar('0')).arg(getExpectedLen(cmdId)).arg(length));
         qDebug() << "失败2";
        return false;
    }
    return true;
}

bool Widget::DrawRealTimeCurve(quint8 cmdId, quint8 action, const QByteArray &fullPacket, quint16 dataLen)
{
    bool bRight = false;
    if(CMD_COMBINED_DATA == cmdId)
    {
        timer->stop();
        retryCount = 0;
        calibTimer->stop();

        if (action == ACTION_SUCCESS) {
         if (dataLen != COMBINED_PAYLOAD_LEN) {
             updateStatus(QString("合并包数据长度错误！预期%1字节，实际%2字节")
                          .arg(COMBINED_PAYLOAD_LEN).arg(dataLen));
             lastSentCmdId = 0; // 重置
             return bRight;
         }

          QByteArray combinedPayload = fullPacket.mid(7, dataLen);
          if(m_displayMotorStatus){
             quint16 alarmCode = (static_cast<quint8>(combinedPayload[0]) << 8) | static_cast<quint8>(combinedPayload[1]); // 从索引0开始，取2字节
             processAlarmData(alarmCode);
          }
         QByteArray realTimePayload = combinedPayload.mid(2, 24); // 从索引2开始，取24字节
         processRealTimeData(realTimePayload); // 修正后的实时值解析
       } else {
           updateStatus(QString("合并包接收失败！响应码：0x%1").arg(action, 2, 16, QChar('0')));
       }
    }
    return bRight;
}

void Widget::processResponse(quint8 cmdId, quint8 action, quint8 index, const QByteArray &fullPacket, quint16 dataLen)
{
    //debug
    //qDebug() <<"receive CANFD message::0x" << QString::number(cmdId, 16);
    //_
/*
    //  主动上报指令（仅CMD_COMBINED_DATA=0x20）：跳过ID校验，直接处理
    bool isActiveReport =(cmdId == CMD_COMBINED_DATA) || (cmdId == CMD_RESULT);
    //  主动发送指令：必须校验“响应ID == 最后发送ID”，否则忽略

    if (!isActiveReport)
    {
        if (cmdId != lastSentCmdId) {
            //qDebug() << "[响应ID不匹配] 发送0x" << QString::number(lastSentCmdId, 16)
            //         << "，接收0x" << QString::number(cmdId, 16) << "→ 忽略该响应";
            qDebug() << "[Response ID mismatch] send0x" << QString::number(lastSentCmdId, 16)
                     << "，receive0x" << QString::number(cmdId, 16) << "→ this response be ignored.";
            return;
        }
    }
*/
    timer->stop();
    retryCount = 0;
    calibTimer->stop();

    switch (cmdId) {
    case CMD_SILENCE:
        if (action == ACTION_SUCCESS) {
            updateStatus("升级静默指令执行成功，进入准备阶段...");
            upgradeState = STATE_PREPARING;
            lastSentCmdId = 0;
            sendPrepareCommand();
        } else {
            updateStatus("升级静默指令执行失败！");
            upgradeState = STATE_ERROR;
            ui->startUpgradeButton->setEnabled(true);
            ui->cancelButton->setEnabled(false);
            lastSentCmdId = 0;
        }

        break;
    case CMD_PREPARE:
        if (action == ACTION_SUCCESS) {
            updateStatus("升级准备包执行成功，开始发送数据...");
            upgradeState = STATE_SENDING_DATA;
            currentPacketIndex = 1;
            lastSentCmdId = 0;
            sendNextPacket();
        } else {
            updateStatus("升级准备包执行失败！");
            upgradeState = STATE_ERROR;
            ui->startUpgradeButton->setEnabled(true);
            ui->cancelButton->setEnabled(false);
            lastSentCmdId = 0;
        }

        break;
    case CMD_DATA:
        if (action == ACTION_SUCCESS) {
            totalSent += currentPacketSize;
            updateProgress(static_cast<int>((totalSent * 100.0) / binSize));
            currentPacketIndex++;
            lastSentCmdId = 0; // 处理完成重置发送ID
            sendNextPacket();
        } else {
            updateStatus(QString("数据包%1发送失败！重试中...").arg(index));
            retryCount++;
            if (retryCount >= MAX_RETRY) {
                updateStatus(QString("数据包%1重试超过%2次，升级失败！")
                             .arg(index).arg(MAX_RETRY));
                upgradeState = STATE_ERROR;
                ui->startUpgradeButton->setEnabled(true);
                ui->cancelButton->setEnabled(false);
            } else {
                sendDataCommand(index, readFirmwareData((index - 1) * 1024, 1024));
            }
        }

        break;
    case CMD_RESULT:
        if (action == ACTION_SUCCESS) {
            updateStatus("IAP升级成功！");
             ui->statusLabel->setText("IAP升级成功！");
            upgradeState = STATE_FINISHED;
        } else {
            updateStatus("IAP升级失败！响应码：0x" + QString::number(action, 16));
            ui->statusLabel->setText("IAP升级失败！");
            upgradeState = STATE_ERROR;
        }

        ui->startUpgradeButton->setEnabled(true);
        ui->cancelButton->setEnabled(false);
         lastSentCmdId = 0;
        break;

    case  CMD_READ_MOTORID://读取电机ID
        if (action == ACTION_SUCCESS) {
                if (dataLen != 4) {
                    ui->statusLabel_3->setText("电机ID读取回复长度错误");
                    updateStatus("CMD_READ_MOTORID：数据长度错误（需4字节）");
                    lastSentCmdId = 0; // 重置
                    break;
                }
                QByteArray payload = fullPacket.mid(7, dataLen);
                MotorId = bigEndianToUint8(payload.mid(0, 4));
                QString idText = QString("0x%1").arg(MotorId, 0, 16, QChar('0')).toLower();
                ui->MotorIdEdit->setText(idText);
                ui->statusLabel_3->setText("电机ID读取成功");
                updateStatus("电机ID读取成功");
            } else {
                ui->statusLabel_3->setText("电机ID读取失败，Flash中无电机ID");
                updateStatus(QString("电机ID读取失败！响应码：0x%1").arg(action, 2, 16, QChar('0')));
                ui->MotorIdEdit->clear();
            }
        lastSentCmdId = 0; // 处理完成，重置发送ID
       break;
    case  CMD_SET_MOTORID://设置电机ID
        if (action == ACTION_SUCCESS) {
                ui->statusLabel_3->setText("电机ID写入成功");
                updateStatus("电机ID写入成功");

                QString idText = QString("0x%1").arg(MotorId, 0, 16, QChar('0')).toLower();
                ui->MotorIdEdit->setText(idText);
            } else {
                ui->statusLabel_3->setText("电机ID写入失败");
                updateStatus(QString("电机ID写入失败！响应码：0x%1").arg(action, 2, 16, QChar('0')));
            }
        lastSentCmdId = 0; // 重置发送ID
        break;
    case  CMD_READ_ANGLE_ZERO://读取机械零位
        if (action == ACTION_SUCCESS) {
                    if (dataLen != 4) {
                        ui->statusLabel_3->setText("机械零位读取回复长度错误");
                        updateStatus("CMD_READ_ANGLE_ZERO：数据长度错误（需4字节）");
                        lastSentCmdId = 0;
                        break;
                    }

                    QByteArray payload = fullPacket.mid(7, dataLen);
                    int32_t zeroData = bigEndianToint32(payload); // 需实现大端转int32函数
                    float angleZeroValue = static_cast<float>(zeroData) / 1000000.0f;

                    QString zeroText = QString::number(angleZeroValue, 'f', 2);
                    ui->AngleZeroEdit->setText(zeroText);

                    ui->statusLabel_3->setText("机械零位读取成功");
                    updateStatus(QString("机械零位读取成功：%1 rad").arg(angleZeroValue, 0, 'f', 2));
                } else {

                    ui->statusLabel_3->setText("机械零位读取失败，Flash中无零位数据");
                    updateStatus(QString("机械零位读取失败！响应码：0x%1").arg(action, 2, 16, QChar('0')));
                    ui->AngleZeroEdit->clear();
                }
        lastSentCmdId = 0;
       break;
    case  CMD_SET_ANGLE_ZERO://设置机械零位
        if (action == ACTION_SUCCESS) {
                ui->statusLabel_3->setText("机械零位设置成功");
                float currentZero = ui->AngleZeroEdit->text().toFloat();
                updateStatus(QString("机械零位设置成功：%1 rad").arg(currentZero, 0, 'f', 2));
            } else {
                ui->statusLabel_3->setText("机械零位设置失败");
                updateStatus(QString("机械零位设置失败，响应码0x%1").arg(action, 2, 16, QChar('0')));
            }
        lastSentCmdId = 0;
        break;

    case CMD_HW_VERSION:
    case CMD_SW_VERSION: {
        if (cmdId != lastCmdId) {
            return;
        }
        QString versionType = (cmdId == CMD_HW_VERSION) ? "硬件" : "软件";
        if (action == ACTION_SUCCESS) {
            ui->versionDisplay->clear();
            QByteArray payload = fullPacket.mid(7, dataLen);
            QString version = QString::fromLatin1(payload.left(64).trimmed());
            updateStatus(QString("%1版本读取成功：%2").arg(versionType).arg(version));
            ui->versionDisplay->append(QString("%1版本: %2").arg(versionType).arg(version));
            lastCmdId = 0;
        } else {
            updateStatus(QString("%1版本读取失败！响应码：0x%2")
                         .arg(versionType).arg(action, 2, 16, QChar('0')));
            lastCmdId = 0;
        }
    }
        lastSentCmdId = 0; // 重置发送ID
        break;

    case CMD_CALIB_MODE:
        if (action == ACTION_SUCCESS) {
            if (ui->calibModeBtn->text() == "打开标定模式") {
                ui->calibModeBtn->setText("关闭标定模式");
                ui->calibModeBtn->setStyleSheet(R"(
                    QPushButton {
                        background-color: #4CAF50;
                        color: white;
                    }
                )");
                isCalibModeOpen = true; // 标定模式激活

                ui->statusLabel_2->setText("标定模式已打开");
                updateStatus("标定模式已打开");
            } else {
                ui->calibModeBtn->setText("打开标定模式");
                ui->calibModeBtn->setStyleSheet(R"(
                    QPushButton {
                        background-color: white;
                        color: black;
                    }
                )");
                isCalibModeOpen = false; // 标定模式关闭
                ui->statusLabel_2->setText("标定模式关闭");
                updateStatus("标定模式已关闭");
            }

        } else {
            ui->statusLabel_2->setText("标定模式切换失败");
            updateStatus("标定模式切换失败");
        }
        lastSentCmdId = 0; // 重置
        break;
    case CMD_ELEC_ANGLE_ZERO:
        if (action == ACTION_SUCCESS) {
            ui->statusLabel_2->setText("零位校准成功");
            updateStatus("零位校准成功");
        } else {
            ui->statusLabel_2->setText("零位校准失败");
            updateStatus("零位校准失败");
        }
        lastSentCmdId = 0; // 重置
        break;
    case CMD_MECH_LIMIT:
        if (action == ACTION_SUCCESS) {
            ui->statusLabel_3->setText("限位参数设置成功");
            updateStatus("限位参数设置成功");
        } else {
            ui->statusLabel_3->setText("限位参数设置失败");
            updateStatus(QString("限位参数设置失败，响应码0x%1").arg(action, 2, 16, QChar('0')));
        }
        lastSentCmdId = 0;
        break;
    case CMD_READ_ELEC_LIMIT:
        if (action == ACTION_SUCCESS) {
                if (dataLen != 8) {
                    updateStatus("CMD_READ_ELEC_LIMIT：数据长度错误（需8字节）");
                    lastSentCmdId = 0; // 重置
                    break;
                }
                QByteArray payload = fullPacket.mid(7, dataLen);
                quint32 rawUpper = bigEndianToUint8(payload.mid(0, 4));
                double upperLimit = rawUpper / 1000000.0;
                quint32 rawLower = bigEndianToUint8(payload.mid(4, 4));
                double lowerLimit = rawLower / 1000000.0;

                ui->elecUpperLimitEdit->setValue(upperLimit);
                ui->elecLowerLimitEdit->setValue(lowerLimit);
                ui->statusLabel_2->setText("电子限位读取成功");
                updateStatus("电子限位读取成功");
            } else {
                ui->statusLabel_2->setText("电子限位读取失败");
                updateStatus("电子限位读取失败");
            }
        lastSentCmdId = 0; // 重置
        break;

    case CMD_CLEAR_ALM:
        if (action == ACTION_SUCCESS) {
            ui->statusLabel_2->setText("清除报警成功");
            updateStatus(QString("指令0x%1执行成功").arg(cmdId, 2, 16, QChar('0')));
        } else {
            ui->statusLabel_2->setText("清除报警成功");
            updateStatus(QString("指令0x%1执行失败，响应码0x%2")
                         .arg(cmdId, 2, 16, QChar('0')).arg(action, 2, 16, QChar('0')));
        }
        lastSentCmdId = 0; // 重置
        break;
    case CMD_COMBINED_DATA:
        if (action == ACTION_SUCCESS) {
           if (dataLen != COMBINED_PAYLOAD_LEN) {
               updateStatus(QString("合并包数据长度错误！预期%1字节，实际%2字节")
                            .arg(COMBINED_PAYLOAD_LEN).arg(dataLen));
               lastSentCmdId = 0; // 重置
               break;
           }

           QByteArray combinedPayload = fullPacket.mid(7, dataLen);
           quint16 alarmCode = (static_cast<quint8>(combinedPayload[0]) << 8) | static_cast<quint8>(combinedPayload[1]); // 从索引0开始，取2字节
           processAlarmData(alarmCode);

           QByteArray realTimePayload = combinedPayload.mid(2, 24); // 从索引2开始，取24字节
           processRealTimeData(realTimePayload); // 修正后的实时值解析

          //updateStatus("合并包（报警+实时值）解析成功");
       } else {
           updateStatus(QString("合并包接收失败！响应码：0x%1").arg(action, 2, 16, QChar('0')));
       }
    lastSentCmdId = 0; // 重置
    break;

    case CMD_MOTOR_ENABLE:
        if (action == ACTION_SUCCESS)
        {
            if (ui->motorEnableBtn->text() == "使能电机")
            {
                ui->motorEnableBtn->setText("禁用电机");
                ui->motorEnableBtn->setStyleSheet(R"(
                                QPushButton {
                                    background-color: #4CAF50; /* 绿色背景（成功状态） */
                                    color: white; /* 白色文字 */
                                }
                            )");
                isMortorEnableOpen = true;
                ui->statusLabel_2->setText("电机使能成功");
                updateStatus(QString("电机%1成功").arg(ui->motorEnableBtn->text() == "禁用电机" ? "使能" : "禁用"));
            }
            else
            {
                ui->motorEnableBtn->setText("使能电机");
                ui->motorEnableBtn->setStyleSheet(R"(
                    QPushButton {
                        background-color: white;
                        color: black;
                    }
                )");
                isMortorEnableOpen = false;
                ui->statusLabel_2->setText("电机禁用成功");
                updateStatus(QString("电机%1成功").arg(ui->motorEnableBtn->text() == "禁用电机" ? "使能" : "禁用"));
            }
        }
        else
        {
            ui->statusLabel_2->setText("控制使能失败");
            updateStatus(QString("电机%1失败").arg(ui->motorEnableBtn->text() == "禁用电机" ? "使能" : "禁用"));
        }
        lastSentCmdId = 0; // 重置
        break;
    case CMD_CONTROL_MODE:
        qDebug() <<"CMD_CONTROL_MODE entry..............";
        if (action == ACTION_SUCCESS)
        {
            if (ui->controlModeBtn->text() == "打开控制模式")
            {
                ui->controlModeBtn->setText("关闭控制模式");
                ui->controlModeBtn->setStyleSheet(R"(
                    QPushButton {
                        background-color: #4CAF50;
                        color: white;
                    }
                )");
                isControlModeOpen = true; // 控制模式激活
            }
            else
            {
                ui->controlModeBtn->setText("打开控制模式");
                ui->controlModeBtn->setStyleSheet(R"(
                    QPushButton {
                        background-color: white;
                        color: black;
                    }
                )");
                isControlModeOpen = false; // 控制模式关闭
            }
            ui->statusLabel_2->setText("控制模式切换成功");
            updateStatus("控制模式切换成功");
        }
        else
        {
            ui->statusLabel_2->setText("控制模式切换失败");
            updateStatus("控制模式切换失败");
        }
        lastSentCmdId = 0; // 重置
        qDebug() <<"CMD_CONTROL_MODE out..............";
        break;
    case CMD_READ_CTRL_PARAM:

           if (action == ACTION_SUCCESS)
           {
                QByteArray payload = fullPacket.mid(7, dataLen);
                if (payload.size() < 148)
                {
                            ui->statusLabel_3->setText("控制参数数据长度不足");
                            updateStatus(QString("控制参数读取失败：数据长度%1字节，至少需%2字节").arg(payload.size()).arg(148));
                            lastSentCmdId = 0;
                             qDebug() << "失败1";
                            break;
                }
                        if (!hybridKpMinLabel) {
                            qDebug() << "[MIT解析警告] 标签未初始化，先执行initHybridPage";
                            initHybridPage();
                        }
                        bool mitParseSuccess = parseMitParams(payload.mid(0, 40));
                        if (mitParseSuccess) {
                            syncMitParamsToControls();

//                            qDebug() << "[最终显示校验] "
//                                     << "kp_min标签文本：" << (hybridKpMinLabel ? hybridKpMinLabel->text() : "无")
//                                     << "kp_max标签文本：" << (hybridKpMaxLabel ? hybridKpMaxLabel->text() : "无")
//                                     << "kd_min标签文本：" << (hybridKdMinLabel ? hybridKdMinLabel->text() : "无")
//                                     << "kd_max标签文本：" << (hybridKdMaxLabel ? hybridKdMaxLabel->text() : "无")
//                                     << "pos_min标签文本：" << (hybridPosMinLabel ? hybridPosMinLabel->text() : "无")
//                                     << "pos_max标签文本：" << (hybridPosMaxLabel ? hybridPosMaxLabel->text() : "无")
//                                     << "speed_min标签文本：" << (hybridSpeedMinLabel ? hybridSpeedMinLabel->text() : "无")
//                                     << "speed_max标签文本：" << (hybridSpeedMaxLabel ? hybridSpeedMaxLabel->text() : "无")
//                                     << "Torque_min标签文本：" << (hybridTorqueMinLabel ? hybridTorqueMinLabel->text() : "无")
//                                     << "Torque_max标签文本：" << (hybridTorqueMaxLabel ? hybridTorqueMaxLabel->text() : "无")
//                                     << "所有标签是否可见：" << (hybridPage ? (hybridPage->isVisible() ? "是" : "否") : "页面不存在");
                        }


                //限位参数
                if (ui->mechUpperLimitEdit) ui->mechUpperLimitEdit->setValue(bigEndianToint32(payload.mid(40, 4)) / 1000000.0);
                if (ui->mechLowerLimitEdit) ui->mechLowerLimitEdit->setValue(bigEndianToint32(payload.mid(44, 4)) / 1000000.0);
                if (ui->elecUpperLimitEdit) ui->elecUpperLimitEdit->setValue(bigEndianToint32(payload.mid(48, 4)) / 1000000.0);
                if (ui->elecLowerLimitEdit) ui->elecLowerLimitEdit->setValue(bigEndianToint32(payload.mid(52, 4)) / 1000000.0);

                if (ui->posKpEdit) ui->posKpEdit->setValue(bigEndianToint32(payload.mid(56, 4)) / 1000000.0);
                if (ui->maxPosEdit) ui->maxPosEdit->setValue(bigEndianToint32(payload.mid(60, 4)) / 1000000.0);
                if (ui->posTrajVelEdit) ui->posTrajVelEdit->setValue(bigEndianToint32(payload.mid(64, 4)) / 1000000.0);
                if (ui->posTrajAccelEdit) ui->posTrajAccelEdit->setValue(bigEndianToint32(payload.mid(68, 4)) / 1000000.0);
                if (ui->posTrajDecelEdit) ui->posTrajDecelEdit->setValue(bigEndianToint32(payload.mid(72, 4)) / 1000000.0);

                if (ui->speedKpEdit) ui->speedKpEdit->setValue(bigEndianToint32(payload.mid(76, 4)) / 1000000.0);
                if (ui->speedKiEdit) ui->speedKiEdit->setValue(bigEndianToint32(payload.mid(80, 4)) / 1000000.0);
                if (ui->maxSpeedEdit) ui->maxSpeedEdit->setValue(bigEndianToint32(payload.mid(84, 4)) / 100000.0);
                if (ui->speedIncEdit) ui->speedIncEdit->setValue(bigEndianToint32(payload.mid(88, 4)) / 1000000.0);

                if (ui->currentKpEdit) ui->currentKpEdit->setValue(bigEndianToint32(payload.mid(92, 4)) / 1000000.0);
                if (ui->currentKiEdit) ui->currentKiEdit->setValue(bigEndianToint32(payload.mid(96, 4)) / 1000000.0);
                if (ui->currentLoopLimitEdit) ui->currentLoopLimitEdit->setValue(bigEndianToint32(payload.mid(100, 4)) / 1000000.0);
                if (ui->currentBandwidthEdit) ui->currentBandwidthEdit->setValue(bigEndianToint32(payload.mid(104, 4)));
                if (ui->currentIncEdit) ui->currentIncEdit->setValue(bigEndianToint32(payload.mid(108, 4)) / 1000000.0);

                //编码器参数
                if (ui->encoderBandwidthEdit) ui->encoderBandwidthEdit->setValue(bigEndianToint32(payload.mid(112, 4)));
                if (ui->encoderDirEdit) ui->encoderDirEdit->setValue(bigEndianToint32(payload.mid(116, 4)));

                //电机参数
                if (ui->motorResistanceEdit) ui->motorResistanceEdit->setValue(bigEndianToint32(payload.mid(120, 4)) / 1000000.0);
                if (ui->motorInductanceEdit) ui->motorInductanceEdit->setValue(bigEndianToint32(payload.mid(124, 4)) / 1000000.0);
                if (ui->motorTrefmaxEdit) ui->motorTrefmaxEdit->setValue(bigEndianToint32(payload.mid(128, 4)) / 1000000.0);
                if (ui->motorIQrefmaxEdit) ui->motorIQrefmaxEdit->setValue(bigEndianToint32(payload.mid(132, 4)) / 1000000.0);
                if (ui->motorKeEdit) ui->motorKeEdit->setValue(bigEndianToint32(payload.mid(136, 4)) / 1000000.0);
                if (ui->motorPolepairEdit) ui->motorPolepairEdit->setValue(bigEndianToint32(payload.mid(140, 4)) / 1000000.0);
                if (ui->motorGrEdit) ui->motorGrEdit->setValue(bigEndianToint32(payload.mid(144, 4)) / 1000000.0);

                ui->statusLabel_3->setText("控制参数读取成功");
                updateStatus("控制参数读取成功");
                qDebug() << "电机参数读取完成";

//                        QString statusText = mitParseSuccess ?
//                            (hybridKpMinLabel && hybridKpMinLabel->text() != "0.00" ? "控制参数读取成功" : "控制参数解析成功") :
//                            "控制参数读取成功，MIT参数解析失败";
//                        ui->statusLabel_3->setText(statusText);
//                        updateStatus(statusText);
////                        qDebug() << "电机参数读取完成，MIT解析状态：" << mitParseSuccess
////                                 << "，kp_min标签值：" << (hybridKpMinLabel ? hybridKpMinLabel->text() : "无")
////                                 << "，kp_max标签值：" << (hybridKpMaxLabel ? hybridKpMaxLabel->text() : "无")
////                                 << "，kd_min标签值：" << (hybridKdMinLabel ? hybridKdMinLabel->text() : "无")
////                                 << "，kd_max标签值：" << (hybridKdMaxLabel ? hybridKdMaxLabel->text() : "无");
            }
            else
            {
                ui->statusLabel_3->setText("控制参数读取失败");
                updateStatus(QString("控制参数读取失败,flash中无数据或falsh读取错误，响应码0x%1").arg(action, 2, 16, QChar('0')));
                 qDebug() << "失败2";
            }
            lastSentCmdId = 0;
         break;


    case CMD_COGGING_CALIB:
        if (action == ACTION_SUCCESS) {
            ui->statusLabel_2->setText("齿槽转矩标定成功");
            ui->statusLabel_2->setStyleSheet("color: green; font-weight: bold;");
            updateStatus("齿槽转矩标定成功");
        } else {
            ui->statusLabel_2->setText("齿槽转矩标定失败");
            ui->statusLabel_2->setStyleSheet("color: red; font-weight: bold;");
            updateStatus(QString("齿槽转矩标定失败，响应码0x%1").arg(action, 2, 16, QChar('0')));
        }
        lastSentCmdId = 0;
        break;

    case CMD_FRICTION_CALIB:
        if (action == ACTION_SUCCESS) {
            ui->statusLabel_2->setText("摩擦转矩标定成功");
            ui->statusLabel_2->setStyleSheet("color: green; font-weight: bold;");
            updateStatus("摩擦转矩标定成功");
        } else {
            ui->statusLabel_2->setText("摩擦转矩标定失败");
            ui->statusLabel_2->setStyleSheet("color: red; font-weight: bold;");
            updateStatus(QString("摩擦转矩标定失败，响应码0x%1").arg(action, 2, 16, QChar('0')));
        }
        lastSentCmdId = 0;
        break;

    case CMD_MOTOR_STOP:
        if (action == ACTION_SUCCESS) {
            ui->statusLabel_2->setText("电机停转成功");
            updateStatus("电机停转成功");
            // 停转后自动禁用电机使能
            isMortorEnableOpen = false;
            ui->motorEnableBtn->setText("使能电机");
            ui->motorEnableBtn->setStyleSheet("QPushButton { background-color: white; color: black; }");
        } else {
            ui->statusLabel_2->setText("电机停转失败");
            updateStatus(QString("电机停转失败，响应码0x%1").arg(action, 2, 16, QChar('0')));
        }
        lastSentCmdId = 0;
        break;
                case CMD_SET_POS_LOOP:
                    if (action == ACTION_SUCCESS) {
                        ui->statusLabel_3->setText("位置环参数设置成功");
                        updateStatus("位置环参数设置成功");
                    } else {
                        ui->statusLabel_3->setText("位置环参数设置失败");
                        updateStatus(QString("位置环参数设置失败，响应码0x%1").arg(action, 2, 16, QChar('0')));
                    }
                    lastSentCmdId = 0;
                    break;

                case CMD_SET_SPEED_LOOP:
                    if (action == ACTION_SUCCESS) {
                        ui->statusLabel_3->setText("速度环参数设置成功");
                        updateStatus("速度环参数设置成功");
                    } else {
                        ui->statusLabel_3->setText("速度环参数设置失败");
                        updateStatus(QString("速度环参数设置失败，响应码0x%1").arg(action, 2, 16, QChar('0')));
                    }
                    lastSentCmdId = 0;
                    break;

                case CMD_SET_CURRENT_LOOP:
                    if (action == ACTION_SUCCESS) {
                        ui->statusLabel_3->setText("电流环参数设置成功");
                        updateStatus("电流环参数设置成功");
                    } else {
                        ui->statusLabel_3->setText("电流环参数设置失败");
                        updateStatus(QString("电流环参数设置失败，响应码0x%1").arg(action, 2, 16, QChar('0')));
                    }
                    lastSentCmdId = 0;
                    break;

                case CMD_SET_ENCODER_LOOP:
                    if (action == ACTION_SUCCESS) {
                        ui->statusLabel_3->setText("编码器参数设置成功");
                        updateStatus("编码器参数设置成功");
                    } else {
                        ui->statusLabel_3->setText("编码器参数设置失败");
                        updateStatus(QString("编码器参数设置失败，响应码0x%1").arg(action, 2, 16, QChar('0')));
                    }
                    lastSentCmdId = 0;
                    break;

                case CMD_SET_MOTOR_LOOP:
                    if (action == ACTION_SUCCESS) {
                        ui->statusLabel_3->setText("电机参数设置成功");
                        updateStatus("电机参数设置成功");
                    } else {
                        ui->statusLabel_3->setText("电机参数设置失败");
                        updateStatus(QString("电机参数设置失败，响应码0x%1").arg(action, 2, 16, QChar('0')));
                    }
                    lastSentCmdId = 0;
                    break;

                case CMD_SET_MIT_LOOP:
                    if (action == ACTION_SUCCESS) {
                        ui->statusLabel_3->setText("MIT参数设置成功");
                        updateStatus("MIT参数设置成功");
                    } else {
                        ui->statusLabel_3->setText("MIT参数设置失败");
                        updateStatus(QString("MIT参数设置失败，响应码0x%1").arg(action, 2, 16, QChar('0')));
                    }
                    lastSentCmdId = 0;
                    break;

//                case CMD_READ_CTRL_PARAM:
//                    if (action == ACTION_SUCCESS) {
//                        QByteArray payload = fullPacket.mid(7, dataLen);
//                        if (parseMitParams(payload)) {
//                            ui->statusLabel_3->setText("控制参数读取成功");
//                            updateStatus("控制参数读取成功");
//                        } else {
//                            ui->statusLabel_3->setText("控制参数解析失败");
//                            updateStatus("控制参数解析失败");
//                        }
//                    } else {
//                        ui->statusLabel_3->setText("控制参数读取失败");
//                        updateStatus(QString("控制参数读取失败，响应码0x%1").arg(action, 2, 16, QChar('0')));
//                    }
//                    lastSentCmdId = 0;
                    break;

       case CMD_SET_POS:
        if (action == ACTION_SUCCESS) {
          ui->statusLabel_2->setText("位置指令发送成功");
            updateStatus("位置指令执行成功（设备响应成功）");
          } else {
           ui->statusLabel_2->setText("位置指令发送失败");
           updateStatus(QString("位置指令执行失败，响应码0x%1").arg(action, 2, 16, QChar('0')));
           }
          lastSentCmdId = 0;
      break;
    case CMD_SET_SPEED:
     if (action == 0x00 || action == ACTION_SUCCESS) {
       ui->statusLabel_2->setText("速度指令发送成功");
         updateStatus("速度指令执行成功（设备响应成功）");
       } else {
        ui->statusLabel_2->setText("速度指令发送失败");
        updateStatus(QString("速度指令执行失败，响应码0x%1").arg(action, 2, 16, QChar('0')));
        }
       lastSentCmdId = 0;
   break;
    case CMD_SET_TORQUE:
     if (action == 0x00 || action == ACTION_SUCCESS) {
       ui->statusLabel_2->setText("力矩指令发送成功");
         updateStatus("力矩指令执行成功（设备响应成功）");
       } else {
        ui->statusLabel_2->setText("转矩指令发送失败");
        updateStatus(QString("力矩指令执行失败，响应码0x%1").arg(action, 2, 16, QChar('0')));
        }
       lastSentCmdId = 0;
   break;
    case CMD_SET_CURRENT:
     if (action == 0x00 || action == ACTION_SUCCESS) {
       ui->statusLabel_2->setText("电流指令发送成功");
         updateStatus("电流指令执行成功（设备响应成功）");
       } else {
        ui->statusLabel_2->setText("电流指令发送失败");
        updateStatus(QString("电流指令执行失败，响应码0x%1").arg(action, 2, 16, QChar('0')));
        }
       lastSentCmdId = 0;
   break;
    case CMD_SET_MIT:
     if (action == 0x00 || action == ACTION_SUCCESS) {
       ui->statusLabel_2->setText("MIT指令发送成功");
         updateStatus("MIT指令执行成功（设备响应成功）");
       } else {
        ui->statusLabel_2->setText("MIT指令发送失败");
        updateStatus(QString("MIT指令执行失败，响应码0x%1").arg(action, 2, 16, QChar('0')));
        }
       lastSentCmdId = 0;
   break;
    default:
        updateStatus(QString("收到未知指令：0x%1").arg(cmdId, 2, 16, QChar('0')));
        lastSentCmdId = 0;
        break;
    }
}

void Widget::handleTimeout()
{
    lastSentCmdId = 0;
    if (lastCmdId != 0) {
        QString versionType = (lastCmdId == CMD_HW_VERSION) ? "硬件" : "软件";
        updateStatus(QString("%1版本查询超时！").arg(versionType));
        lastCmdId = 0;
        return;
    }

    switch (upgradeState) {
    case STATE_SILENCE:
        updateStatus("升级静默指令超时！");
        break;
    case STATE_PREPARING:
        updateStatus("升级准备指令超时！");
        break;
    case STATE_SENDING_DATA:
        updateStatus(QString("数据包%1发送超时！").arg(currentPacketIndex));
        break;
    case STATE_WAITING_RESULT:
        updateStatus("等待升级结果超时！");
        break;
    default:
        return;
    }

    upgradeState = STATE_ERROR;
    ui->startUpgradeButton->setEnabled(true);
    ui->cancelButton->setEnabled(false);
    timer->stop();
    retryCount = 0;
}

void Widget::syncMitParamsToControls()
{
    // 计算参数值（缩放1e6）
        double kpMin = mitParams.kp_limit_min / 1000000.0;
        double kpMax = mitParams.kp_limit_max / 1000000.0;
        double kdMin = mitParams.kd_limit_min / 1000000.0;
        double kdMax = mitParams.kd_limit_max / 1000000.0;
        double posMin = mitParams.pos_limit_min / 1000000.0;
        double posMax = mitParams.pos_limit_max / 1000000.0;
        double speedMin = mitParams.speed_limit_min / 1000000.0;
        double speedMax = mitParams.speed_limit_max / 1000000.0;
        double torqueMin = mitParams.torque_limit_min / 1000000.0;
        double torqueMax = mitParams.torque_limit_max / 1000000.0;


        if (kpMin > kpMax) { kpMin = 0.0; kpMax = 10.0; }
        if (kdMin > kdMax) { kdMin = 0.0; kdMax = 10.0; }
        if (posMin > posMax) { posMin = 0.0; posMax = 100.0; }
        if (speedMin > speedMax) { speedMin = 0.0; speedMax = 1000.0; }
        if (torqueMin > torqueMax) { torqueMin = 0.0; torqueMax = 50.0; }

        // Kp默认值
        double kpDefault = 0.0;
        if (kpMin > 0 && kpMax > 0) {
            kpDefault = kpMin;
        } else if (kpMin < 0 && kpMax > 0) {
            kpDefault = 0.0;
        } else {
            kpDefault = kpMin;
        }

        // Kd默认值
        double kdDefault = 0.0;
        if (kdMin > 0 && kdMax > 0) {
            kdDefault = kdMin;
        } else if (kdMin < 0 && kdMax > 0) {
            kdDefault = 0.0;
        } else {
            kdDefault = kdMin;
        }

        // 位置默认值
        double posDefault = 0.0;
        if (posMin > 0 && posMax > 0) {
            posDefault = posMin;
        } else if (posMin < 0 && posMax > 0) {
            posDefault = 0.0;
        } else {
            posDefault = posMin;
        }

        // 速度默认值
        double speedDefault = 0.0;
        if (speedMin > 0 && speedMax > 0) {
            speedDefault = speedMin;
        } else if (speedMin < 0 && speedMax > 0) {
            speedDefault = 0.0;
        } else {
            speedDefault = speedMin;
        }

        // 力矩默认值
        double torqueDefault = 0.0;
        if (torqueMin > 0 && torqueMax > 0) {
            torqueDefault = torqueMin;
        } else if (torqueMin < 0 && torqueMax > 0) {
            torqueDefault = 0.0;
        } else {
            torqueDefault = torqueMin;
        }

        ui->kpLimitMinEdit->blockSignals(true);
        ui->kpLimitMaxEdit->blockSignals(true);
        ui->kdLimitMinEdit->blockSignals(true);
        ui->kdLimitMaxEdit->blockSignals(true);
        ui->posLimitMinEdit->blockSignals(true);
        ui->posLimitMaxEdit->blockSignals(true);
        ui->speedLimitMinEdit->blockSignals(true);
        ui->speedLimitMaxEdit->blockSignals(true);
        ui->torqueLimitMinEdit->blockSignals(true);
        ui->torqueLimitMaxEdit->blockSignals(true);

        ui->kpLimitMinEdit->setValue(kpMin);
        ui->kpLimitMaxEdit->setValue(kpMax);
        ui->kdLimitMinEdit->setValue(kdMin);
        ui->kdLimitMaxEdit->setValue(kdMax);
        ui->posLimitMinEdit->setValue(posMin);
        ui->posLimitMaxEdit->setValue(posMax);
        ui->speedLimitMinEdit->setValue(speedMin);
        ui->speedLimitMaxEdit->setValue(speedMax);
        ui->torqueLimitMinEdit->setValue(torqueMin);
        ui->torqueLimitMaxEdit->setValue(torqueMax);

        ui->kpLimitMinEdit->blockSignals(false);
        ui->kpLimitMaxEdit->blockSignals(false);
        ui->kdLimitMinEdit->blockSignals(false);
        ui->kdLimitMaxEdit->blockSignals(false);
        ui->posLimitMinEdit->blockSignals(false);
        ui->posLimitMaxEdit->blockSignals(false);
        ui->speedLimitMinEdit->blockSignals(false);
        ui->speedLimitMaxEdit->blockSignals(false);
        ui->torqueLimitMinEdit->blockSignals(false);
        ui->torqueLimitMaxEdit->blockSignals(false);

        if (hybridKpSpinBox) {
            hybridKpSpinBox->setRange(kpMin, kpMax);
            hybridKpSpinBox->setValue(kpDefault);
        }
        if (hybridKdSpinBox) {
            hybridKdSpinBox->setRange(kdMin, kdMax);
            hybridKdSpinBox->setValue(kdDefault);
        }
        if (hybridPosSpinBox) {
            hybridPosSpinBox->setRange(posMin, posMax);
            hybridPosSpinBox->setValue(posDefault);
        }
        if (hybridSpeedSpinBox) {
            hybridSpeedSpinBox->setRange(speedMin, speedMax);
            hybridSpeedSpinBox->setValue(speedDefault);
        }
        if (hybridTorqueSpinBox) {
            hybridTorqueSpinBox->setRange(torqueMin, torqueMax);
            hybridTorqueSpinBox->setValue(torqueDefault);
        }
        syncSliderFromSpinBox();

        qDebug() << "[参数同步完成] 控制参数页面显示控件已更新，kp默认值=" << kpDefault;
}

void Widget::updateStatus(const QString &status)
{
    QString timeStr = QDateTime::currentDateTime().toString("HH:mm:ss");
    ui->logTextEdit->append(timeStr + " - " + status);
    qDebug() << timeStr << " - " << status;
}

void Widget::updateProgress(int value)
{
    ui->progressBar->setValue(value);
}

void Widget::sendCalibCommand(quint8 cmdId,quint8 enable)
{
    qDebug() << "[sendCalibCommand] 调用一次，指令ID：0x" << QString::number(cmdId, 16)
               << "，enable：" << static_cast<int>(enable);
    currentCalibCmd = cmdId;
    lastSentCmdId = cmdId;
    QByteArray packet = buildPacket(0, cmdId, enable, QByteArray());
    if (currentCommMode == SERIAL_MODE) {
        sendSerialData(packet);
    } else if (currentCommMode == CAN_MODE) {
        sendCanData(packet);
    }
    int timeoutMs = (cmdId == CMD_ELEC_ANGLE_ZERO  || cmdId == CMD_COGGING_CALIB || cmdId == CMD_FRICTION_CALIB)
                    ? 3 * 60 * 1000 : 10 * 1000;
    calibTimer->start(timeoutMs);
    updateStatus(QString("发送标定指令0x%1，等待响应...").arg(cmdId, 2, 16, QChar('0')));
}

void Widget::handleCalibTimeout()
{
    lastSentCmdId = 0;
    updateStatus(QString("标定指令0x%1超时未响应！")
                 .arg(currentCalibCmd, 2, 16, QChar('0')));
}

void Widget::on_motorStopBtn_clicked()
{
    if (!isMortorEnableOpen) {
        QMessageBox::warning(this, tr("操作提示"), tr("电机未使能，无需停转！"), QMessageBox::Ok);
        return;
    }

    // 发送电机停转指令（无Payload，Action=1执行）
    sendControlCommand(CMD_MOTOR_STOP, QByteArray(), 1);
    updateStatus("发送电机停转指令...");
}

//void Widget::on_coggingCalibBtn_clicked()
//{
//    if (!isCalibModeOpen || isMortorEnableOpen) {
//        QMessageBox::warning(this, tr("操作提示"), tr("请打开标定模式并关闭电机使能！"), QMessageBox::Ok);
//        return;
//    }

//    sendCalibCommand(CMD_COGGING_CALIB, 1);
//    QByteArray payload;
//    payload.append(static_cast<quint8>(1)); // 开始标定标识
//    sendControlCommand(CMD_COGGING_CALIB, payload, 1);

//    ui->statusLabel_2->setText("齿槽转矩标定中...");
//    ui->statusLabel_2->setStyleSheet("color: orange; font-weight: bold;");
//    updateStatus("发送齿槽转矩标定指令...");
//}

//void Widget::on_frictionCalibBtn_clicked()
//{
//    if (!isCalibModeOpen || isMortorEnableOpen) {
//        QMessageBox::warning(this, tr("操作提示"), tr("请打开标定模式并关闭电机使能！"), QMessageBox::Ok);
//        return;
//    }

//    sendCalibCommand(CMD_FRICTION_CALIB, 1);

//    QByteArray payload;
//    payload.append(static_cast<quint8>(1)); // 开始标定标识
//    sendControlCommand(CMD_FRICTION_CALIB, payload, 1);

//    ui->statusLabel_2->setText("摩擦转矩标定中...");
//    ui->statusLabel_2->setStyleSheet("color: orange; font-weight: bold;");
//    updateStatus("发送摩擦转矩标定指令...");
//}


void Widget::sendControlCommand(quint8 cmdId, const QByteArray &payload,quint8 action)
{
    lastSentCmdId = cmdId; // 记录主动发送的指令ID
    QByteArray packet = buildPacket(0, cmdId, action, payload);
    if (currentCommMode == SERIAL_MODE) {
        sendSerialData(packet);
    } else if (currentCommMode == CAN_MODE) {
        sendCanData(packet);
    }
    updateStatus(QString("发送控制指令0x%1，等待响应...").arg(cmdId, 2, 16, QChar('0')));
}

void Widget::processRealTimeData(const QByteArray &payload)
{
    if (payload.size() != 24) {
        updateStatus(QString("实时值数据长度错误！预期24字节，实际%1字节").arg(payload.size()));
        return;
    }

    int rawPos = bigEndianToint32(payload.mid(0, REAL_DATA_PER_PARAM)); // 0~3字节
    currentPos = rawPos / 1000000.0;

    int rawSpeed = bigEndianToint32(payload.mid(4, REAL_DATA_PER_PARAM)); // 4~7字节
    currentSpeed = rawSpeed/1000;

    int rawPhaseCurrent = bigEndianToint32(payload.mid(8, REAL_DATA_PER_PARAM)); // 8~11字节
    currentPhaseCurrent = rawPhaseCurrent / 1000000.0;

    int rawTorque = bigEndianToint32(payload.mid(12, REAL_DATA_PER_PARAM)); // 12~15字节
    currentTorque = rawTorque / 1000000.0;

    int rawMotorTem = bigEndianToint32(payload.mid(16, REAL_DATA_PER_PARAM)); // 16~19字节
    currentMotorTem = rawMotorTem / 1000000.0;

    int rawMosTem = bigEndianToint32(payload.mid(20, REAL_DATA_PER_PARAM)); // 20~23字节
    currentMosTem = rawMosTem / 1000000.0;

    if(m_displayMotorStatus){
        ui->realPosEdit->setText(QString::number(currentPos, 'f', 2));
        ui->realSpeedEdit->setText(QString::number(currentSpeed, 'f', 0));
        ui->realSpeedEdit_1->setText(QString::number((currentSpeed*0.104719755), 'f', 2));
        ui->realPhaseCurrentEdit->setText(QString::number(currentPhaseCurrent, 'f', 2));
        ui->realTorqueEdit->setText(QString::number(currentTorque, 'f', 2));
        ui->realMotorTemEdit->setText(QString::number(currentMotorTem, 'f', 1));
        ui->realMosTemEdit->setText(QString::number(currentMosTem, 'f', 1));
    }

    //20260709 x轴以时间形式显示
    static QTime timeStart = QTime::currentTime();
    double key = timeStart.msecsTo(QTime::currentTime()); //unit select sencond for display X-Axis

    //customplot add data 20260720
    if(m_PauseCapture)
      {
        m_plotwidget->appendData(CURRENTPOS, key, currentPos);
        m_plotwidget->appendData(RPMS, key, currentSpeed*0.104719755);
        m_plotwidget->appendData(PHASECURRENT, key, currentPhaseCurrent);
        m_plotwidget->appendData(MOTORTEMP, key, currentMotorTem);
        m_plotwidget->appendData(MOSTEMP, key, currentMosTem);

#ifdef QT_DEBUG

        Plotting(CURRENTPOS, currentPos);
        Plotting(RPMS, currentSpeed*0.104719755);
        Plotting(PHASECURRENT, currentPhaseCurrent);
        Plotting(MOTORTEMP, currentMotorTem);
        Plotting(MOSTEMP, currentMosTem);

#endif
    }
}

void Widget::processAlarmData(quint16 alarmCode)
{
    currentAlarm = alarmCode;
    if (alarmCode != 0) {
            // 有报警：启动闪烁定时器，初始亮红色
            if (!alarmBlinkTimer->isActive()) {
                alarmBlinkTimer->start();
                isAlarmLedOn = true; // 初始亮态
                ui->alarmLed->setStyleSheet(R"(
                    background-color: #FF0000;
                    border-radius: 15px;
                    border: 1px solid #cc0000;
                )");
            }
        } else {
            // 无报警：停止闪烁，恢复常亮绿色
            alarmBlinkTimer->stop();
            ui->alarmLed->setStyleSheet(R"(
                background-color: green;
                border-radius: 15px;
                border: 1px solid #cccccc;
            )");
        }
    QString alarmStr;
    switch (alarmCode) {

        case ALARM_NORMAL: alarmStr = "无报警"; break;
        case ALARM_UNDER_VOLT: alarmStr = "欠压"; break;
        case ALARM_OVER_VOLT: alarmStr = "过压"; break;
        case ALARM_SPEED_OVER: alarmStr = "超速"; break;
        case ALARM_MOTOR_OVER_TEMP: alarmStr = "电机过温"; break;
        case ALARM_MOTOR_UNDER_TEMP: alarmStr = "电机欠温"; break;
        case ALARM_SOFT_OVER_CURR: alarmStr = "软件过流"; break;
        case ALARM_CALI_RESISTANCE_OUT_OF_RANGE: alarmStr = "标定错误"; break;
        case ALARM_ENCODER: alarmStr = "编码器错误"; break;
        case ALARM_ADC: alarmStr = "ADC采样错误"; break;
        case ALARM_MOS_OVER_TEMP: alarmStr = "MOS过温"; break;
        case ALARM_MOS_UNDER_TEMP: alarmStr = "MOS欠温"; break;
        case ALARM_BLOCK: alarmStr = "堵转"; break;
        case ALARM_POS_OVER: alarmStr = "位置超差"; break;

    default: alarmStr = "未知报警（0x" + QString::number(alarmCode, 16) + "）"; break;
    }
    ui->alarmInfoEdit->setText(alarmStr);
}

QByteArray Widget::int32ToBigEndian(int32_t value) const
{
    QByteArray data;
    data.append(static_cast<quint8>((value >> 24) & 0xFF));
    data.append(static_cast<quint8>((value >> 16) & 0xFF));
    data.append(static_cast<quint8>((value >> 8) & 0xFF));
    data.append(static_cast<quint8>(value & 0xFF));
    return data;
}

QByteArray Widget::uint32ToBigEndian(quint32 value)
{
    QByteArray data;
    data.append(static_cast<quint8>((value >> 24) & 0xFF));
    data.append(static_cast<quint8>((value >> 16) & 0xFF));
    data.append(static_cast<quint8>((value >> 8) & 0xFF));
    data.append(static_cast<quint8>(value & 0xFF));
    return data;
}
//QByteArray Widget::int32ToBigEndian(int value)
//{
//    QByteArray data;
//    data.append(static_cast<quint8>((value >> 24) & 0xFF));
//    data.append(static_cast<quint8>((value >> 16) & 0xFF));
//    data.append(static_cast<quint8>((value >> 8) & 0xFF));
//    data.append(static_cast<quint8>(value & 0xFF));
//    return data;
//}
QByteArray Widget::uint16ToBigEndian(quint16 value)
{
    QByteArray data;
    data.append(static_cast<quint8>((value >> 8) & 0xFF));
    data.append(static_cast<quint8>(value & 0xFF));
    return data;
}
QByteArray Widget::doubleToBigEndian(double value)
{
    union {
        double d;
        quint8 bytes[8];
    } u;
    u.d = value;
    QByteArray data;
    for (int i = 7; i >= 0; --i) {
        data.append(u.bytes[i]);
    }
    return data;
}

quint32 Widget::bigEndianToUint32(const QByteArray &data)
{
    if (data.size() < 4) return 0;
    return (static_cast<quint32>(data[0]) << 24) |
           (static_cast<quint32>(data[1]) << 16) |
           (static_cast<quint32>(data[2]) << 8) |
           static_cast<quint32>(data[3]);
}

quint32 Widget::bigEndianToUint8(const QByteArray &data)
{
    if (data.size() < 4) return 0;
    return (static_cast<quint8>(data[0]) << 24) |
           (static_cast<quint8>(data[1]) << 16) |
           (static_cast<quint8>(data[2]) << 8) |
           static_cast<quint8>(data[3]);
}

int  Widget::bigEndianToint32(const QByteArray &data)
{
    if (data.size() < 4) {
           updateStatus("bigEndianToInt32：数据长度不足4字节，返回0");
           return 0;
       }
       quint32 rawUnsigned = (static_cast<quint8>(data[0]) << 24) |
                            (static_cast<quint8>(data[1]) << 16) |
                            (static_cast<quint8>(data[2]) << 8) |
                            static_cast<quint8>(data[3]);
        return static_cast<int32_t>(rawUnsigned);
}

double Widget::bigEndianToDouble(const QByteArray &data)
{
    if (data.size() < 8) return 0.0;
      union {
          double d;
          quint8 bytes[8];
      } u;
      for (int i = 0; i < 8; ++i) {
          u.bytes[i] = data[7 - i];
      }
      return u.d;
}

void Widget::on_calibModeBtn_clicked()
{
       if (isControlModeOpen == true)
       {
           QMessageBox::warning(this, tr("操作提示"), tr("请先关闭控制模式！"), QMessageBox::Ok);
           return;
       }
       if (isMortorEnableOpen == true)
       {
           QMessageBox::warning(this, tr("操作提示"), tr("请先关闭电机使能！"), QMessageBox::Ok);
           return;
       }
    quint8 enable = (ui->calibModeBtn->text() == "打开标定模式") ? 1 : 0;
    sendCalibCommand(CMD_CALIB_MODE,enable);
}

void Widget::on_elecAngleZeroBtn_clicked()
{
    if (!isCalibModeOpen) {
           QMessageBox::warning(this, tr("操作提示"), tr("请先打开标定模式！"), QMessageBox::Ok);
           return;
       }
    if (isMortorEnableOpen != true)
    {
        QMessageBox::warning(this, tr("操作提示"), tr("请先打开电机使能！"), QMessageBox::Ok);
        return;
    }


    sendCalibCommand(CMD_ELEC_ANGLE_ZERO,1);
}

//void Widget::on_readElecLimitBtn_clicked()
//{
//    if (isMortorEnableOpen == true)
//    {
//        QMessageBox::warning(this, tr("操作提示"), tr("请先关闭电机使能！"), QMessageBox::Ok);
//        return;
//    }
//       if (!isCalibModeOpen) {
//           QMessageBox::warning(this, tr("操作提示"), tr("请先打开标定模式！"), QMessageBox::Ok);
//           return;
//       }
//    sendCalibCommand(CMD_READ_ELEC_LIMIT,1);
//}

//void Widget::on_readEncoderBtn_clicked()
//{
//       if (isMortorEnableOpen == true)
//       {
//           QMessageBox::warning(this, tr("操作提示"), tr("请先关闭电机使能！"), QMessageBox::Ok);
//           return;
//       }
//       if (!isCalibModeOpen) {
//           QMessageBox::warning(this, tr("操作提示"), tr("请先打开标定模式！"), QMessageBox::Ok);
//           return;
//       }
//    sendCalibCommand(CMD_READ_ENCODER,1);
//}

void Widget::on_readControlBtn_clicked()
{
    if (isCalibModeOpen == true)
    {
        QMessageBox::warning(this, tr("操作提示"), tr("请先关闭标定模式！"), QMessageBox::Ok);
        return;
    }
    if (isMortorEnableOpen == true)
    {
        QMessageBox::warning(this, tr("操作提示"), tr("请先关闭电机使能！"), QMessageBox::Ok);
        return;
    }
    QByteArray payload;
    sendControlCommand(CMD_READ_CTRL_PARAM, payload,1);
}

void Widget::on_writePosCmdBtn_clicked()
{
       if (isCalibModeOpen) {
           QMessageBox::warning(this, tr("操作提示"), tr("请先关闭标定模式！"));
           return;
       }
       if (!isControlModeOpen) {
           QMessageBox::warning(this, tr("操作提示"), tr("请先打开控制模式！"));
           return;
       }
       if (!isMortorEnableOpen) {
           QMessageBox::warning(this, tr("操作提示"), tr("请先打开电机使能！"));
           return;
       }

       bool ok;
       double posValue = ui->posCmdEdit->text().toDouble(&ok);
       if (!ok) {
           QMessageBox::warning(this, tr("输入错误"), tr("请输入有效的位置值！"));
           return;
       }

       QByteArray payload;
       payload.append(static_cast<quint8>(MODE_POSITION));

       int32_t posData = static_cast<int32_t>(posValue * 1000000);
       payload.append(int32ToBigEndian(posData));

       sendControlCommand(CMD_SET_POS, payload, 1);
       updateStatus(QString("发送位置指令：%1 rad").arg(posValue));
}

void Widget::on_writeSpeedCmdBtn_clicked()
{

        if (isCalibModeOpen) {
            QMessageBox::warning(this, tr("操作提示"), tr("请先关闭标定模式！"));
            return;
        }
        if (!isControlModeOpen) {
            QMessageBox::warning(this, tr("操作提示"), tr("请先打开控制模式！"));
            return;
        }
        if (!isMortorEnableOpen) {
            QMessageBox::warning(this, tr("操作提示"), tr("请先打开电机使能！"));
            return;
        }

        bool ok;
        double speedValue = ui->speedCmdEdit->text().toDouble(&ok);
        if (!ok) {
            QMessageBox::warning(this, tr("输入错误"), tr("请输入有效的速度值！"));
            return;
        }

        QByteArray payload;
        payload.append(static_cast<quint8>(MODE_SPEED));  // 模式字节：0x02

        int32_t speedData = static_cast<int32_t>(speedValue * 100000);
        payload.append(int32ToBigEndian(speedData));

        sendControlCommand(CMD_SET_SPEED, payload, 1);
        updateStatus(QString("发送速度指令：%1 rpm").arg(speedValue));
}

void Widget::on_writeTorqueCmdBtn_clicked()
{

        if (isCalibModeOpen) {
            QMessageBox::warning(this, tr("操作提示"), tr("请先关闭标定模式！"));
            return;
        }
        if (!isControlModeOpen) {
            QMessageBox::warning(this, tr("操作提示"), tr("请先打开控制模式！"));
            return;
        }
        if (!isMortorEnableOpen) {
            QMessageBox::warning(this, tr("操作提示"), tr("请先打开电机使能！"));
            return;
        }

        bool ok;
        double torqueValue = ui->torqueCmdEdit->text().toDouble(&ok);
        if (!ok) {
            QMessageBox::warning(this, tr("输入错误"), tr("请输入有效的力矩值！"));
            return;
        }

        QByteArray payload;
        payload.append(static_cast<quint8>(MODE_TORQUE));  // 模式字节：0x03

        int32_t torqueData = static_cast<int32_t>(torqueValue * 1000000);
        payload.append(int32ToBigEndian(torqueData));

        sendControlCommand(CMD_SET_TORQUE, payload, 1);
        updateStatus(QString("发送力矩指令：%1 A").arg(torqueValue));
}

void Widget::on_writeCurrentCmdBtn_clicked()
{
    if (isCalibModeOpen) {
        QMessageBox::warning(this, tr("操作提示"), tr("请先关闭标定模式！"));
        return;
    }
    if (!isControlModeOpen) {
        QMessageBox::warning(this, tr("操作提示"), tr("请先打开控制模式！"));
        return;
    }
    if (!isMortorEnableOpen) {
        QMessageBox::warning(this, tr("操作提示"), tr("请先打开电机使能！"));
        return;
    }

    bool ok;
    double currentValue = ui->currentCmdEdit->text().toDouble(&ok);
    if (!ok) {
        QMessageBox::warning(this, tr("输入错误"), tr("请输入有效的电流值！"));
        return;
    }

    QByteArray payload;
    payload.append(static_cast<quint8>(MODE_CURRENT));  // 模式字节：0x04

    int32_t currentData = static_cast<int32_t>(currentValue * 1000000);
    payload.append(int32ToBigEndian(currentData));

    sendControlCommand(CMD_SET_CURRENT, payload, 1);
    updateStatus(QString("发送电流指令：%1 A").arg(currentValue));
}

void Widget::on_clearALMBtn_clicked()
{
    QByteArray payload;
    sendControlCommand(CMD_CLEAR_ALM, payload,1);
}

void Widget::on_readHwVersionBtn_clicked()
{
    lastCmdId = CMD_HW_VERSION;
    sendVersionCommand(CMD_HW_VERSION);
}

void Widget::on_readSwVersionBtn_clicked()
{
    lastCmdId = CMD_SW_VERSION;
    sendVersionCommand(CMD_SW_VERSION);
}

void Widget::sendVersionCommand(quint8 cmdId)
{
    lastSentCmdId = cmdId;
    QByteArray packet = buildPacket(0, cmdId, 1, QByteArray());
     qDebug() << "currentCommMode的数值：" << currentCommMode;
    if (currentCommMode == SERIAL_MODE) {
        sendSerialData(packet);
    } else
        if (currentCommMode == CAN_MODE) {
        sendCanData(packet);
    }
    timer->start(2000);
}

void Widget::onAlarmBlinkTimerTimeout()
{
    // 翻转闪烁状态：亮→暗、暗→亮
    isAlarmLedOn = !isAlarmLedOn;

    // 根据状态设置样式
    if (isAlarmLedOn) {
        // 亮态：红色圆形
        ui->alarmLed->setStyleSheet(R"(
            background-color: #FF0000; /* 鲜艳红色 */
            border-radius: 15px;
            border: 1px solid #cc0000;
        )");
    } else {
        // 暗态：浅灰色圆形
        ui->alarmLed->setStyleSheet(R"(
            background-color: #E0E0E0; /* 浅灰色 */
            border-radius: 15px;
            border: 1px solid #cccccc;
        )");
    }
}
// -------------------------- 编译兼容函数 --------------------------
void Widget::on_commModeChanged()
{

}

void Widget::on_zero_customContextMenuRequested()
{

}

void Widget::on_readMotorIDBtn_clicked()
{
    QByteArray payload;
    sendControlCommand(CMD_READ_MOTORID, payload,1);
}

void Widget::on_writeMotorIDBtn_clicked()
{
        if (isCalibModeOpen || isControlModeOpen || isMortorEnableOpen) {
            QString tip = isCalibModeOpen ? "标定模式" :
                          (isControlModeOpen ? "控制模式" : "电机使能");
            QMessageBox::warning(this, tr("操作提示"), tr("请先关闭%1！").arg(tip), QMessageBox::Ok);
            return;
        }

        QString input = ui->MotorIdEdit->text().trimmed();
        if (input.isEmpty()) {
            QMessageBox::warning(this, tr("电机ID警告"), tr("请输入有效的十六进制ID！"), QMessageBox::Ok);
            return;
        }

        int pos = 0;
        if (ui->MotorIdEdit->validator()->validate(input, pos) != QValidator::Acceptable) {
            QMessageBox::warning(this, tr("电机ID警告"), tr("无效格式！请输入如0x001或1的十六进制值"), QMessageBox::Ok);
            return;
        }

        bool ok = false;
        quint32 newMotorId = input.startsWith("0x", Qt::CaseInsensitive)
                            ? input.mid(2).toUInt(&ok, 16)
                            : input.toUInt(&ok, 16);

        if (!ok || newMotorId > 0x000000FF) {
            QMessageBox::warning(this, tr("电机ID警告"), tr("ID范围：0~0x000000FF！"), QMessageBox::Ok);
            return;
        }

        MotorId = newMotorId;
        QByteArray payload = uint32ToBigEndian(MotorId);
        sendControlCommand(CMD_SET_MOTORID, payload, 1);
}

void Widget::on_motorEnableBtn_clicked()
{

    bool isEnable = (ui->motorEnableBtn->text() == "使能电机") ? 1 : 0;
    QByteArray payload;
    payload.append(isEnable);
//    if (isCalibModeOpen == true)
//    {
//        QMessageBox::warning(this, tr("操作提示"), tr("请先关闭标定模式！"), QMessageBox::Ok);
//        return;
//    }
       if((isControlModeOpen != true) &&(isCalibModeOpen != true))
       {
           QMessageBox::warning(this, tr("操作提示"), tr("请先打开控制模式或标定模式！"), QMessageBox::Ok);
           return;
       }

       if(isEnable ==1)
       {
           updateStatus("发送电机使能打开指令...");
           qDebug() << "电机使能已打开";
       }
       else
       {
            updateStatus("发送电机使能关闭指令...");
            qDebug() << "电机使能已关闭";
       }
    sendControlCommand(CMD_MOTOR_ENABLE, payload,isEnable);
}

void Widget::on_readAngleZeroBtn_clicked()
{
    QByteArray payload;
    sendControlCommand(CMD_READ_ANGLE_ZERO, payload,1);
}

void Widget::on_writeAngleZeroBtn_clicked()
{
        if (isCalibModeOpen || isControlModeOpen || isMortorEnableOpen) {
            QString tip = isCalibModeOpen ? "标定模式" :
                          (isControlModeOpen ? "控制模式" : "电机使能");
            QMessageBox::warning(this, tr("操作提示"), tr("请先关闭%1！").arg(tip), QMessageBox::Ok);
            return;
        }

            bool ok;
            float zeroValue = ui->AngleZeroEdit->text().toFloat(&ok);
            if (!ok) {
                QMessageBox::warning(this, tr("输入错误"), tr("请输入有效的浮点数值（如：0.0、1.57）！"));
                return;
            }

//            const float MIN_ANGLE = -3.1416f; // 示例：-π
//            const float MAX_ANGLE = 3.1416f;  // 示例：π
//            if (zeroValue < MIN_ANGLE || zeroValue > MAX_ANGLE) {
//                QMessageBox::warning(this, tr("输入超限"),
//                                     tr("零位值需在%1 ~ %2 rad之间！").arg(MIN_ANGLE).arg(MAX_ANGLE));
//                return;
//            }

            QByteArray payload;
            int32_t zeroData = static_cast<int32_t>(zeroValue * 1000000.0f);
            payload.append(int32ToBigEndian(zeroData));

            sendControlCommand(CMD_SET_ANGLE_ZERO, payload, 1);
//            ui->statusLabel_2->setText("正在设置机械零位...");
//            updateStatus(QString("发送设置零位指令：%1 rad（原始数据：%2）")
//                         .arg(zeroValue, 0, 'f', 2)
//                         .arg(zeroData));
}
