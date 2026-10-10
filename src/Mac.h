#pragma once
#include <QRect>
#include <QString>
#include <QKeySequence>
#include <functional>
#include <QtGui/qwindowdefs.h>
bool GameWindow(QRect& rect);
QString GameWindowStatus();
void FloatOnGame(WId handle);
bool GameIsFrontmost();
QString ConfigureTeamKeys(QKeySequence previous,QKeySequence next,std::function<void(int)> action);
void EnableTeamKeys(bool enabled);
