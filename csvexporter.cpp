#include "csvexporter.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QTimeZone>

bool CsvExporter::exportToFile(const QString &fileName,
                               const QVector<QString> &headers,
                               const QVector<QVector<QPointF>> &data)
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
    for (const QString &h : headers) {
        stream << ",        " << h;
    }
    stream << "\n";

    // Find maximum data length
    int maxLen = 0;
    for (const auto &d : data) {
        maxLen = qMax(maxLen, d.size());
    }

    // Data rows
    QTimeZone tz = QTimeZone::utc();
    for (int i = 0; i < maxLen; ++i)
    {
        if (!data.isEmpty() && !data.first().isEmpty())
        {
            stream << QDateTime::fromMSecsSinceEpoch(
                          static_cast<qint64>(data.first()[i].x()),tz)
                          .toString("hh:mm:ss:zzz");
        }

        for (const auto &curveData : data)
        {
            if (i < curveData.size()) {
                stream << "," << curveData[i].y()<<"        ";
            } else {
                stream << ",";
            }
        }
        stream << "\n";
    }

    file.close();

    return true;
}
