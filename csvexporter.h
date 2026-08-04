#ifndef CSVEXPORTER_H
#define CSVEXPORTER_H

#include <QString>
#include <QVector>
#include <QPointF>

class CsvExporter
{
public:
    static bool exportToFile(const QString &fileName,
                             const QVector<QString> &headers,
                             const QVector<QVector<QPointF>> &data);
};

#endif // CSVEXPORTER_H
