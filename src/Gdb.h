#pragma once
#include <QThread>
class Reader:public QThread {
 public:
 void run() override;
};
