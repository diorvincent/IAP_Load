#include "commselectdialog.h"
#include "ui_commselectdialog.h"
#include <QRegExp>
#include <QMessageBox>
#include <QSerialPortInfo>

CommSelectDialog::CommSelectDialog(CommMode currentMode, const Settings &serialSettings,
                                   quint32 canId, int canPort, const QString &canBaudRate, QWidget *parent)
    : QDialog(parent), ui(new Ui::CommSelectDialog),
      m_currentMode(currentMode), m_serialSettings(serialSettings),
      m_canId(canId), m_canPort(canPort), m_canBaudRate(canBaudRate)
{
    ui->setupUi(this);
    setWindowTitle("通信选择");
    m_intValidator = new QIntValidator(1, 4000000, this);

    // CAN ID验证器（十六进制）
    QRegExp hexRegExp("^(0x)?[0-9a-fA-F]{1,8}$");
    m_canIdValidator = new QRegExpValidator(hexRegExp, this);
    ui->canIdEdit->setValidator(m_canIdValidator);

    // 初始化串口和CAN配置
    initSerialConfig(serialSettings);
    initCanConfig(canId, canPort, canBaudRate);
    fillCanBaudRateBox();
    fillSerialPorts();

    // 初始化通信模式选择
    if (currentMode == CAN_MODE) {
        ui->radioBtnCan->setChecked(true);
        ui->canConfigGroup->show();
        ui->serialConfigGroup->hide();
    } else {
        ui->radioBtnSerial->setChecked(true);
        ui->serialConfigGroup->show();
        ui->canConfigGroup->hide();
    }

    // 绑定信号槽
    connect(ui->radioBtnCan, &QRadioButton::toggled, this, &CommSelectDialog::on_radioBtnCan_toggled);
    connect(ui->radioBtnSerial, &QRadioButton::toggled, this, &CommSelectDialog::on_radioBtnSerial_toggled);
    connect(ui->baudRateBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CommSelectDialog::checkCustomBaudRate);
    connect(ui->refreshButton, &QPushButton::clicked, this, &CommSelectDialog::fillSerialPorts);
}

CommSelectDialog::~CommSelectDialog()
{
    delete m_intValidator;
    delete m_canIdValidator;
    delete ui;
}

void CommSelectDialog::initSerialConfig(const Settings &settings)
{
    // 波特率
    ui->baudRateBox->addItem(QStringLiteral("9600"), QSerialPort::Baud9600);
    ui->baudRateBox->addItem(QStringLiteral("19200"), QSerialPort::Baud19200);
    ui->baudRateBox->addItem(QStringLiteral("38400"), QSerialPort::Baud38400);
    ui->baudRateBox->addItem(QStringLiteral("57600"), QSerialPort::Baud57600);
    ui->baudRateBox->addItem(QStringLiteral("115200"), QSerialPort::Baud115200);
    ui->baudRateBox->addItem(QStringLiteral("Custom"));
    int baudIndex = ui->baudRateBox->findData(settings.baudRate);
    if (baudIndex != -1) {
        ui->baudRateBox->setCurrentIndex(baudIndex);
    } else {
        ui->baudRateBox->setCurrentText(settings.stringBaudRate);
    }

    // 数据位
    ui->dataBitsBox->addItem(QStringLiteral("8"), QSerialPort::Data8);
    ui->dataBitsBox->addItem(QStringLiteral("7"), QSerialPort::Data7);
    ui->dataBitsBox->setCurrentIndex(ui->dataBitsBox->findData(settings.dataBits));

    // 校验位
    ui->parityBox->addItem(QStringLiteral("None"), QSerialPort::NoParity);
    ui->parityBox->addItem(QStringLiteral("Even"), QSerialPort::EvenParity);
    ui->parityBox->addItem(QStringLiteral("Odd"), QSerialPort::OddParity);
    ui->parityBox->setCurrentIndex(ui->parityBox->findData(settings.parity));

    // 停止位
    ui->stopBitsBox->addItem(QStringLiteral("1"), QSerialPort::OneStop);
    ui->stopBitsBox->addItem(QStringLiteral("2"), QSerialPort::TwoStop);
    ui->stopBitsBox->setCurrentIndex(ui->stopBitsBox->findData(settings.stopBits));

    // 流控制
    ui->flowControlBox->addItem(QStringLiteral("None"), QSerialPort::NoFlowControl);
    ui->flowControlBox->addItem(QStringLiteral("RTS/CTS"), QSerialPort::HardwareControl);
    ui->flowControlBox->setCurrentIndex(ui->flowControlBox->findData(settings.flowControl));
}

void CommSelectDialog::initCanConfig(quint32 canId, int canPort, const QString &canBaudRate)
{
    // CAN ID
    ui->canIdEdit->setText(QString("0x%1").arg(canId, 8, 16, QChar('0')));
    // CAN端口
    ui->canPortBox->setCurrentIndex(canPort);
    // CAN速率
    int baudIndex = ui->canBaudRateBox->findText(canBaudRate);
    if (baudIndex != -1) {
        ui->canBaudRateBox->setCurrentIndex(baudIndex);
    }
}

void CommSelectDialog::fillCanBaudRateBox()
{
    QStringList baudRates = {
        "10Kbps", "20Kbps", "50Kbps", "100Kbps", "125Kbps",
        "250Kbps", "500Kbps", "800Kbps", "1000Kbps"
    };
    ui->canBaudRateBox->addItems(baudRates);
}

void CommSelectDialog::fillSerialPorts()
{
    ui->portBox->clear();
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        ui->portBox->addItem(info.portName());
    }
    if (!ui->portBox->findText(m_serialSettings.name)) {
        ui->portBox->addItem(m_serialSettings.name);
    }
    ui->portBox->setCurrentText(m_serialSettings.name);
}

void CommSelectDialog::on_radioBtnCan_toggled(bool checked)
{
    if (checked) {
        ui->canConfigGroup->show();
        ui->serialConfigGroup->hide();
    }
}

void CommSelectDialog::on_radioBtnSerial_toggled(bool checked)
{
    if (checked) {
        ui->serialConfigGroup->show();
        ui->canConfigGroup->hide();
    }
}

void CommSelectDialog::checkCustomBaudRate(int idx)
{
    const bool isCustom = ui->baudRateBox->currentText() == "Custom";
    ui->baudRateBox->setEditable(isCustom);
    if (isCustom) {
        ui->baudRateBox->setValidator(m_intValidator);
    }
}

void CommSelectDialog::on_openCanButton_clicked()
{
    // 验证CAN ID
    QString input = ui->canIdEdit->text().trimmed();
    int pos = 0;
    if (ui->canIdEdit->validator()->validate(input, pos) != QValidator::Acceptable) {
        QMessageBox::warning(this, "警告", "无效的CAN ID格式！请输入十六进制值（如0x204）");
        return;
    }
    bool ok = false;
    quint32 newId = input.startsWith("0x", Qt::CaseInsensitive)
                    ? input.mid(2).toUInt(&ok, 16) : input.toUInt(&ok, 16);
    if (!ok || newId > 0x1FFFFFFF) {
        QMessageBox::warning(this, "警告", "CAN ID超出范围！有效范围：0~0x1FFFFFFF");
        return;
    }
    m_canId = newId;
    m_canPort = ui->canPortBox->currentIndex();
    m_canBaudRate = ui->canBaudRateBox->currentText();
    m_currentMode = CAN_MODE;
    accept();
}

void CommSelectDialog::on_openButton_clicked()
{
    // 验证串口波特率
    if (ui->baudRateBox->currentText() == "Custom") {
        bool ok;
        qint32 customBaud = ui->baudRateBox->currentText().toInt(&ok);
        if (!ok || customBaud < 1 || customBaud > 4000000) {
            QMessageBox::warning(this, "警告", "无效的自定义波特率！");
            return;
        }
        m_serialSettings.baudRate = customBaud;
        m_serialSettings.stringBaudRate = QString::number(customBaud);
    } else {
        m_serialSettings.baudRate = static_cast<QSerialPort::BaudRate>(
                    ui->baudRateBox->itemData(ui->baudRateBox->currentIndex()).toInt());
        m_serialSettings.stringBaudRate = ui->baudRateBox->currentText();
    }
    m_serialSettings.name = ui->portBox->currentText();
    m_serialSettings.dataBits = static_cast<QSerialPort::DataBits>(
                ui->dataBitsBox->itemData(ui->dataBitsBox->currentIndex()).toInt());
    m_serialSettings.parity = static_cast<QSerialPort::Parity>(
                ui->parityBox->itemData(ui->parityBox->currentIndex()).toInt());
    m_serialSettings.stopBits = static_cast<QSerialPort::StopBits>(
                ui->stopBitsBox->itemData(ui->stopBitsBox->currentIndex()).toInt());
    m_serialSettings.flowControl = static_cast<QSerialPort::FlowControl>(
                ui->flowControlBox->itemData(ui->flowControlBox->currentIndex()).toInt());
    m_currentMode = SERIAL_MODE;
    accept();
}

void CommSelectDialog::on_refreshButton_clicked()
{
    fillSerialPorts();
}

CommMode CommSelectDialog::getSelectedCommMode() const
{
    return m_currentMode;
}

Settings CommSelectDialog::getSerialSettings() const
{
    return m_serialSettings;
}

quint32 CommSelectDialog::getCanId() const
{
    return m_canId;
}

int CommSelectDialog::getCanPort() const
{
    return m_canPort;
}

QString CommSelectDialog::getCanBaudRate() const
{
    return m_canBaudRate;
}
