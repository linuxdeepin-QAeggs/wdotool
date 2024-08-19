#ifndef CONFIG_H
#define CONFIG_H

#include <QFile>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

#include "datastruct.h"


class Config : public QObject
{
    Q_OBJECT
public:
    explicit Config(QObject *parent = nullptr);
public:
  Info m_info;
};

#endif // CONFIG_H
