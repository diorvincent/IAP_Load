#ifndef COMMSELECTDIALOG_H
#define COMMSELECTDIALOG_H

#include <QDialog>
#include "widget.h"  // 新增：包含定义CommMode和Settings的头文件

namespace Ui {
class CommSelectDialog;
}

class CommSelectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CommSelectDialog(CommMode currentMode, const Settings &serialSettings,
                              quint32 canId, int canPort, const QString &canBaudRate, QWidget *parent = nullptr);
    ~CommSelectDialog();

    // 获取配置结果
    CommMode getSelectedCommMode() const;
    Settings getSerialSettings() const;
    quint32 getCanId() const;
    int getCanPort() const;
    QString getCanBaudRate() const;

private slots:
    void on_radioBtnCan_toggled(bool checked);
    void on_radioBtnSerial_toggled(bool checked);
    void on_openCanButton_clicked();
    void on_openButton_clicked();
    void on_refreshButton_clicked();
    void checkCustomBaudRate(int idx);

private:
    Ui::CommSelectDialog *ui;
    CommMode m_currentMode;
    Settings m_serialSettings;
    quint32 m_canId;
    int m_canPort;
    QString m_canBaudRate;
    QIntValidator *m_intValidator;
    QRegExpValidator *m_canIdValidator;

    // 初始化UI数据
    void initSerialConfig(const Settings &settings);
    void initCanConfig(quint32 canId, int canPort, const QString &canBaudRate);
    void fillCanBaudRateBox();
    void fillSerialPorts();
};

#endif // COMMSELECTDIALOG_H
