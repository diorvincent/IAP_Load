#ifndef WIDGET_H
#define WIDGET_H
#include <QWidget>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QFile>
#include <QTimer>
#include <QByteArray>
#include <QIntValidator>
#include <QMap>
#include <QPair>
#include <QMessageBox>
#include <QDebug>
#include <QDateTime>
#include <QFileDialog>
#include <QRegExp>
#include <QRegExpValidator>

#include "plotwidget.h"
#include "qcustomplot.h"
#include "proxyplot.h"
#include "aes_128_decrypt.h"
#include "canbase.h"
#include "zlgcan_s.h"
#include "chuangxincan.h"

// 周立功CAN库头文件
#include "typedef.h"
#include "config.h"
#include "zlgcan.h"
#include "canframe.h"
#include <QThread>
#include <cstdint>
#include <cmath>
#include <string.h>

#include <QWidget>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QLineEdit>
#include <QTabWidget>
#include <QGroupBox>
#include <QComboBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QProgressBar>
#include <QSerialPort>
#include <QTimer>
#include <QMap>
#include <QQueue>
#include <QMessageBox>
#include <QFileDialog>

// 前向声明，避免循环包含
class QSerialPort;
class QTimer;
class ZLGCAN;
class canBase;
class ChuangXinCan;
class ZLGCan_s;


class DoubleSlider : public QSlider
{
    Q_OBJECT
public:
    explicit DoubleSlider(QWidget *parent = nullptr)
        : QSlider(parent), m_minValue(-10.0), m_maxValue(10.0), m_decimals(2) {
        setOrientation(Qt::Horizontal);
       connect(this, &QSlider::valueChanged, this, &DoubleSlider::onIntValueChanged);
    }

    void setDoubleRange(double min, double max) {
        m_minValue = min;
        m_maxValue = max;
        double scale = std::pow(10.0, m_decimals);
        int intMin = static_cast<int>(std::round(min * scale));
        int intMax = static_cast<int>(std::round(max * scale));
        QSlider::setRange(intMin, intMax);
    }

    void setDoubleValue(double value) {
        value = qBound(m_minValue, value, m_maxValue);
        double scale = std::pow(10.0, m_decimals);
        int intValue = static_cast<int>(std::round(value * scale));
        blockSignals(true);
        QSlider::setValue(intValue);
        blockSignals(false);
    }

    double getDoubleValue() const {
        double scale = std::pow(10.0, m_decimals);
        return static_cast<double>(QSlider::value()) / scale;
    }

    void setDecimals(int decimals) {
        if (decimals < 0 || decimals > 6) return;
        m_decimals = decimals;
        setDoubleRange(m_minValue, m_maxValue);
    }

signals:

    void doubleValueChanged(double value);

private slots:

    void onIntValueChanged(int intValue) {
        double scale = std::pow(10.0, m_decimals);
        double doubleValue = static_cast<double>(intValue) / scale;
        emit doubleValueChanged(doubleValue);
    }

private:
    double m_minValue;   // 小数最小值（支持负数）
    double m_maxValue;   // 小数最大值
    int m_decimals;      // 小数位数
};



namespace Ui {
class Widget;
}

// 协议常量
const quint8 PACKET_HEADER1 = 0xAA;
const quint8 PACKET_HEADER2 = 0x55;
#define SFID_FIXED 0x10
#define COMBINED_PAYLOAD_LEN 26
#define REAL_DATA_PER_PARAM 4

enum CmdId {
    // 原有升级/版本指令
    CMD_HW_VERSION = 0x11,          // 读取硬件版本号
    CMD_SW_VERSION = 0x12,          // 读取软件版本号
    CMD_SILENCE = 0x13,             // 升级静默
    CMD_PREPARE = 0x14,             // 升级准备包
    CMD_DATA = 0x15,                // 升级数据包
    CMD_RESULT = 0x16,              // 升级结果包
    // 标定指令（0x17-0x1D）
    CMD_CALIB_MODE = 0x17,          // 标定模式
    CMD_ELEC_ANGLE_ZERO = 0x18,     // 零位校准

    CMD_READ_ELEC_LIMIT = 0x1A,     // 电子限位读取/////
    CMD_READ_ENCODER = 0x1B,      // 编码器数据读取/////
    CMD_COGGING_CALIB = 0x1C,       // 齿槽转矩标定/////
    CMD_FRICTION_CALIB = 0x1D,      // 摩擦转矩标定/////

    // 电机控制指令（0x20-0x29）
    CMD_COMBINED_DATA  = 0x20,      // 报警显示和参数实时值
    CMD_CONTROL_MODE = 0x21,        // 控制模式
    CMD_MOTOR_ENABLE = 0x22,        // 电机使能
    CMD_MOTOR_STOP = 0x23,          // 电机停转
    CMD_CLEAR_ALM = 0x24,           // 清除报警信息
    CMD_SET_POS = 0x25,             // 位置指令设置
    CMD_SET_SPEED = 0x26,           // 速度指令设置
    CMD_SET_TORQUE = 0x27,          // 力矩指令设置
    CMD_SET_CURRENT = 0x28,         // 电流指令设置
    CMD_SET_MIT = 0x29,             // 力位混合指令设置

    // 控制参数指令（0x30-0x39）
    CMD_READ_MOTORID = 0x30,        // 读取控制ID和电机型号
    CMD_SET_MOTORID = 0x31,         // 设置控制ID和电机型号
    CMD_READ_CTRL_PARAM = 0x32,     // 控制参数读取
    CMD_SET_MIT_LOOP = 0x33,        // MIT参数设置
    CMD_MECH_LIMIT = 0x34,          // 限位设置
    CMD_SET_POS_LOOP = 0x35,        // 位置环参数设置
    CMD_SET_SPEED_LOOP = 0x36,      // 速度环参数设置
    CMD_SET_CURRENT_LOOP = 0x37,    // 电流环参数设置
    CMD_SET_ENCODER_LOOP = 0x38,    // 编码器参数设置
    CMD_SET_MOTOR_LOOP = 0x39,      // 电机参数设置
    CMD_READ_ANGLE_ZERO = 0x3A,      // 读取机械零位
    CMD_SET_ANGLE_ZERO = 0x3B,      // 设置机械零位


};

enum ResponseAction {
    ACTION_SUCCESS = 0,  // 成功
    ACTION_FAILED = 1    // 失败
};

// 升级状态
enum UpgradeState {
    STATE_IDLE,               // 空闲状态
    STATE_SILENCE,            // 升级静默状态
    STATE_PREPARING,          // 准备升级状态
    STATE_SENDING_DATA,       // 发送数据状态
    STATE_WAITING_RESULT,     // 等待结果状态
    STATE_FINISHED,           // 完成状态
    STATE_ERROR               // 错误状态
};

// 通信模式
enum CommMode {
    CAN_MODE,     // CAN模式（USBCAN-II）
    SERIAL_MODE  // 串口模式

};

// 控制模式枚举（对应0x1E指令）
enum ControlMode {
    MODE_POSITION = 0x01,      // 位置模式
    MODE_SPEED = 0x02,         // 速度模式
    MODE_TORQUE = 0x03,        // 力矩模式
    MODE_CURRENT = 0x04,       // 电流模式
    MODE_MIT = 0x05           // 力位混合模式
};

// 报警码枚举（对应0x1C指令）
enum AlarmCode {
    ALARM_NORMAL                        = 0x00,    // 正常，无报警
    ALARM_OVER_VOLT                     = 0x01,   // 过压
    ALARM_UNDER_VOLT                    = 0x02,  // 欠压
    ALARM_SPEED_OVER                    = 0x04,  // 超速
    ALARM_MOTOR_OVER_TEMP               = 0x08, // 电机过温
    ALARM_MOTOR_UNDER_TEMP              = 0x10, // 电机欠温
    ALARM_SOFT_OVER_CURR                = 0x20, // 软件过流
    ALARM_CALI_RESISTANCE_OUT_OF_RANGE  = 0x40, // 标定错误
    ALARM_ENCODER                       = 0x80, // 编码器错误
    ALARM_ADC                           = 0x100, // ADC采样错误
    ALARM_MOS_OVER_TEMP                 = 0x200, // MOS过温
    ALARM_MOS_UNDER_TEMP                = 0x400, // MOS欠温
    ALARM_BLOCK                         = 0x800,      // 堵转
    ALARM_POS_OVER                      = 0x1000    // 位置超差
};

// 串口配置结构体
struct Settings {
    QString name;
    qint32 baudRate;
    QString stringBaudRate;
    QSerialPort::DataBits dataBits;
    QString stringDataBits;
    QSerialPort::Parity parity;
    QString stringParity;
    QSerialPort::StopBits stopBits;
    QString stringStopBits;
    QSerialPort::FlowControl flowControl;
    QString stringFlowControl;
};

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget();


private slots:

    void On_ckPosCheckchanged(bool checked);
    void On_RPMCheckchanged(bool checked);
    void On_PhaseCurrentCheckchanged(bool checked);
    void On_MotorTempCheckchanged(bool checked);
    void On_DrvTempCheckchanged(bool checked);

    void On_rbBigDataRangeChanged(bool checked);
    void On_rbSmallDataRangeChanged(bool checked);
    void On_pbExportExcel();
    void On_pbSnapShot();
    void On_pbPauseCapture();
    void On_pbCurveDisplay();
    void On_MotorStatusChanged(bool checked);
    void On_pbApplyCurveSP();
    void refreshSerialPorts();
    void on_openButton_clicked();
    void readData();
    void checkCustomBaudRate(int idx);
    void on_refreshButton_clicked();

    bool removeDecryptBinFile();

    void on_openCanButton_clicked();

    void receiveCanData();
    void on_canIdEdit_editingFinished();

    void updateSendSt(UINT sendCnt, QByteArray currentFrameData);
    void recvCANFDData(QByteArray recvCANFDData);

    void on_fileButton_clicked();
    void on_viewButton_clicked();
    void on_startUpgradeButton_clicked();
    void on_cancelButton_clicked();
    void sendNextPacket();
    void handleTimeout();
    void on_readHwVersionBtn_clicked();
    void on_readSwVersionBtn_clicked();

    void on_clearVersionBtn_clicked();
    void on_clearUpgradeLogBtn_clicked();

    void on_commModeChanged();
    void on_zero_customContextMenuRequested();
    void closeSerialDevice();

    void on_calibModeBtn_clicked();          // 标定模式切换
    void on_elecAngleZeroBtn_clicked();       // 电角度零位校准
    void on_mechLimitBtn_clicked();          // 机械限位校准
//  void on_readElecLimitBtn_clicked();      // 电子限位读取
//  void on_readEncoderBtn_clicked();        // 编码器数据读取
    void handleCalibTimeout();               // 标定操作超时处理


    // 电机控制
    void on_motorEnableBtn_clicked();        // 电机使能/禁用
    void on_controlModeBtn_clicked();        // 控制模式切换
    void on_readControlBtn_clicked();        // 读取控制参数

    // 控制环参数写入
    void on_writePosLoopBtn_clicked();       // 位置环参数写入
    void on_writeSpeedLoopBtn_clicked();     // 速度环参数写入
    void on_writeCurrentLoopBtn_clicked();   // 电流环参数写入

    // 控制指令写入
    void on_writePosCmdBtn_clicked();        // 位置指令写入
    void on_writeSpeedCmdBtn_clicked();      // 速度指令写入
    void on_writeTorqueCmdBtn_clicked();     // 力矩指令写入

    void on_clearALMBtn_clicked();

    void on_readMotorIDBtn_clicked();

    void on_writeMotorIDBtn_clicked();

    // MIT控制指令设置
    void on_mitSetBtn_clicked();
    // 电机停转
    void on_motorStopBtn_clicked();
//    // 齿槽转矩标定
//    void on_coggingCalibBtn_clicked();
//    // 摩擦转矩标定
//    void on_frictionCalibBtn_clicked();


//        void on_writePosLoopBtn_clicked();
//        void on_writeSpeedLoopBtn_clicked();
//        void on_writeCurrentLoopBtn_clicked();
        void on_writeEncoderBtn_clicked();
        void on_writeMotorBtn_clicked();
//        void on_mitSetBtn_clicked();
        void on_sendParamsBtn_clicked();


//        void on_mortorEnableBtn_clicked();
//        void on_clearALMBtn_clicked();
//        void on_controlModeBtn_clicked();

           void onModeTabChanged(int index);
           void onControlTabChanged(int index);

            void readMitParamsFromControlPage();
        // 更新电机状态
        void updateMotorStatus();
        void on_writeMitLoopBtn_clicked();  // MIT参数发送按钮

    // 力位混合界面滑块和SpinBox连接
    void onHybridSpinBoxValueChanged(double spinValue, QDoubleSpinBox* spinBox, DoubleSlider* slider, const QString& paramName);
    void onHybridSliderValueChanged(int sliderValue, QDoubleSpinBox* spinBox, DoubleSlider* slider, const QString& paramName);

    void on_writeCurrentCmdBtn_clicked();
    void onMainTabChanged(int index);

    void onKpSliderValueChanged(double value);
    void onKdSliderValueChanged(double value);
    void onPosSliderValueChanged(double value);
    void onSpeedSliderValueChanged(double value);
    void onTorqueSliderValueChanged(double value);

    //void on_readMotorIDBtn_2_clicked();
    //void on_writeMotorIDBtn_2_clicked();
    void on_readAngleZeroBtn_clicked();
    void on_writeAngleZeroBtn_clicked();

private:
    Ui::Widget *ui;
    //20260709
    PlotWidget* m_plotwidget;//curve widget for realtime motor params
    void getMotorCurve();

#ifdef QT_DEBUG

    proxyPlot* m_ProxyPlot;


    //20260720 use qcustomplot for drawing each motor params curve
    QCustomPlot *m_customPlot;
    QCPItemTracer* m_tracer;
    QCPItemText* m_tracerLabel;

    void initQCP();
    void initGraphName(QString name, int index, QColor color, bool bVisable=false);
    void mouseMoveData(QMouseEvent *loc);
    void setPlotTheme(QColor axis, QColor background);
    void Plotting(QString name, double value);
#endif

    quint16 m_nCurveDrawFrq;
    QMap<QString, int> m_nameToGraphMap;

    //20260724 //内部调试还是发行版本 (发行版本需要屏蔽部分控件信息)
    void Publish_or_Debug_Ver();
    //_

    //20260812+ 创芯CAN适配器
    canBase* m_canBase;
    ChuangXinCan* m_CXCan;
    //_


    bool hybridPageInitialized = false;
    // 串口相关
    QSerialPort *serial;
    Settings settings;
    bool OpenStatus;
    QIntValidator *intValidator;

    // CAN相关
    CommMode currentCommMode;
    DEVICE_HANDLE canDeviceHandle;
    CHANNEL_HANDLE canChannelHandle;
    bool canOpenStatus;
    quint32 canId;
    QRegExpValidator *canIdValidator;
    QTimer *canReceiveTimer;

    QMap<QString, QPair<quint32, quint32>> canFdBaudMap;

    // 升级相关
    quint8 m_nTryReadT;
    quint64 m_uBinSize;
    QByteArray m_binArr;//解密后的bin数据

    QString fileLocation;
    QFile* m_decryptFile;
    QFile *firmwareFile;
    QString m_strNewPath;
    quint64 binSize;
    quint64 totalSent;
    quint8 currentPacketIndex;
    UpgradeState upgradeState;
    QTimer *timer;
    int retryCount;
    const int MAX_RETRY = 3;
    quint8 lastCmdId;
    QByteArray recvBuffer;
    quint32 currentPacketSize;

    QTimer *calibTimer;
    quint8 currentCalibCmd;

    // 实时数据存储
    double currentPos;      // 实时位置（弧度）
    double currentSpeed;    // 实时速度（rpm）
    double currentPhaseCurrent;   // 实时相电流（A）
    double currentTorque;   // 实时力矩（A）
    double currentMotorTem;   // 实时电机温度（℃）
    double currentMosTem;   // 实时MOS温度（℃）
    quint8 currentAlarm;    // 当前报警码

    // 报警灯闪烁控制（亮=红色，暗=浅灰色）
    QTimer *alarmBlinkTimer;  // 闪烁定时器（1秒间隔）
    bool isAlarmLedOn;        // 闪烁状态：true=亮红色，false=暗灰色

    // 新增：标定模式状态标识（true=已打开，false=未打开）
    bool isCalibModeOpen;

    // 新增：电机使能模式状态标识（true=已打开，false=未打开）
    bool isMortorEnableOpen;

    // 新增：控制模式状态标识（true=已打开，false=未打开）
    bool isControlModeOpen;
    //绘制曲线时，是否显示电机状态
    bool m_displayMotorStatus;
    //暂停曲线采集
    bool m_PauseCapture;
    //曲线显示模式(false:滚动显示； true:实时显示)
    bool m_CurveDisplayMode;

    quint32 MotorId; // 新增：设备ID存储，与CAN ID格式一致
    QRegExpValidator *motorIdValidator; // 新增：设备ID输入验证器
    quint8 lastSentCmdId = 0; // 记录上位机主动发送的指令ID（0=无未完成响应）

    // 新增：选项卡切换相关
    QTabWidget* commTabWidget;  // 通信模式选项卡
    QWidget* serialTabPage;     // 串口选项卡页面
    QWidget* canTabPage;        // CAN选项卡页面

    // 存储控制参数
    QMap<QString, double> controlParams;
    // 添加 hybridPage 声明
    QWidget* hybridPage;

    // 存储MIT参数
    struct MITParams {
        int32_t kp_limit_min;
           int32_t kp_limit_max;
           int32_t kd_limit_min;
           int32_t kd_limit_max;
           int32_t pos_limit_min;
           int32_t pos_limit_max;
           int32_t speed_limit_min;
           int32_t speed_limit_max;
           int32_t torque_limit_min;
           int32_t torque_limit_max;
    } mitParams;

    QDoubleSpinBox* hybridKpSpinBox = nullptr;
    QDoubleSpinBox* hybridKdSpinBox = nullptr;
    QDoubleSpinBox* hybridPosSpinBox = nullptr;
    QDoubleSpinBox* hybridSpeedSpinBox = nullptr;
    QDoubleSpinBox* hybridTorqueSpinBox = nullptr;


    DoubleSlider* hybridKpSlider = nullptr;
    DoubleSlider* hybridKdSlider = nullptr;
    DoubleSlider* hybridPosSlider = nullptr;
    DoubleSlider* hybridSpeedSlider = nullptr;
    DoubleSlider* hybridTorqueSlider = nullptr;

    //DoubleSlider* targetPosSlider = nullptr;    // 伺服位置页
    //DoubleSlider* targetVelSlider = nullptr;    // 伺服速度页
    //DoubleSlider* targetCurrentSlider = nullptr;// 电流页
    //DoubleSlider* targetTorqueSlider = nullptr; // 力矩页

    QLabel* hybridKpMinLabel = nullptr;
    QLabel* hybridKpMaxLabel = nullptr;
    QLabel* hybridKdMinLabel = nullptr;
    QLabel* hybridKdMaxLabel = nullptr;
    QLabel* hybridPosMinLabel = nullptr;
    QLabel* hybridPosMaxLabel = nullptr;
    QLabel* hybridSpeedMinLabel = nullptr;
    QLabel* hybridSpeedMaxLabel = nullptr;
    QLabel* hybridTorqueMinLabel = nullptr;
    QLabel* hybridTorqueMaxLabel = nullptr;

    void initControlParams();
    //void initHybridPage();
    void initMitParams();
    void initConnectionInterface();
    void initCANConfig();
    void initSerialConfig();

    void initMotorControl();
    void initCanConfigGroup();
    void initSerialConfigGroup();

    void onConnectTabChanged(int index);
    void ensureConnectionPageVisible();

    void sendControlParam(const QString& paramName, double value);
    void updateStatusDisplay();


    // 力位混合界面初始化
    void initHybridPage();
    // 更新力位混合界面的滑块标签和范围
    void updateHybridSliderLabelsAndRanges();
    // 力位混合界面滑块和SpinBox连接
    void setupHybridConnections();
    // MIT参数处理函数
    bool parseMitParams(const QByteArray& data);
    QByteArray getMitParamsBytes() const;
    void updateHybridSliderLabels();

    void fillPortsInfo();
    void fillPortsCom();
    void sendSerialData(const QByteArray &data);
    void fillCanConfig();
//    QPair<BYTE, BYTE> getCanTiming(const QString &baudRate);

    QPair<quint32, quint32> getCanFdTiming(const QString &baudText);
    bool openCanDevice();
    void closeCanDevice();
    void sendCanData(const QByteArray &data);

//    void sendCANData(quint8 mode, const QByteArray& data);
     QString getModeName(quint8 mode);

    QString getCanErrorInfo();
    void updateStatus(const QString &status);
    void updateProgress(int value);
    void resetUpgradeState();
    bool openFirmwareFile(const QString &fileName);
    QByteArray readFirmwareData(qint64 offset, qint64 maxSize);
    void processRecvBuffer();
    QByteArray buildPacket(quint8 index, quint8 cmdId, quint8 action, const QByteArray &payload);
    quint8 calculateChecksum(const QByteArray &data);
    bool parseResponse(const QByteArray &data, quint8 &cmdId, quint8 &action, quint8 &index, quint16 &length);
    void processResponse(quint8 cmdId, quint8 action, quint8 index, const QByteArray &fullPacket, quint16 dataLen);
    bool DrawRealTimeCurve(quint8 cmdId, quint8 action, const QByteArray &fullPacket, quint16 dataLen);
    void sendSilenceCommand(bool enable);
    void sendPrepareCommand();
    void sendDataCommand(quint8 index, const QByteArray &data);
    void sendVersionCommand(quint8 cmdId);

    void sendCalibCommand(quint8 cmdId,quint8 enable);

    void sendControlCommand(quint8 cmdId, const QByteArray &payload,quint8 action);

    void processRealTimeData(const QByteArray &payload);

    void processAlarmData(quint16 alarmCode);

    QByteArray uint32ToBigEndian(quint32 value);
    QByteArray int32ToBigEndian(int32_t value) const;
    QByteArray doubleToBigEndian(double value);
    quint32 bigEndianToUint32(const QByteArray &data);
    quint32 bigEndianToUint8(const QByteArray &data);
    int bigEndianToint32(const QByteArray &data);
    double bigEndianToDouble(const QByteArray &data);
    QByteArray uint16ToBigEndian(quint16 value);

    void onAlarmBlinkTimerTimeout();
    int getExpectedLen(quint8 cmdId);
    void sendMixedControlCommand(quint8 mode, const QString& paramName, double value);

    void printWidgetTree(QWidget* widget, int depth = 0);
    void debugHybridPage();
    void syncSliderFromSpinBox();

    //template<typename T>
    //T* findChildRecursive(QWidget* parent, const QString& name) {
    //    if (!parent) return nullptr;
    //    return parent->findChild<T>(name);
    //}
    void delayedInitHybridPage();
    void on_tabWidget_currentChanged(int index);
    void updateHybridSliderRanges();
    void forceSetLabelTexts();

    void testBasicConnection();
    void delayedHybridInit();
    void disconnectAllHybridConnections();

    void initHybridControlValues();
    void findHybridControls();
    QWidget* findHybridPage();
    void printAllWidgets(QWidget* parent);

    void initCommTabSwitch();
    void onCommTabChanged(int index);
    void updateCommConfigVisibility();
    void syncMitParamsToControls();
    void blockHybridSignals(bool block);
    void readMitParamsFromControlPageOptimized();

};

#endif // WIDGET_H
