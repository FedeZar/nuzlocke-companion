#pragma once
#include <QJsonObject>
#include <QPainter>
#include <QRect>
#include <QByteArray>
namespace Nuzlocke {
void SetDeadBox(int box);
void AcceptRAM(const QByteArray& ram, bool boxesReady);
void Disconnected(QString reason);
QJsonObject State();
void Draw(QPainter& painter,const QRect& left,const QRect& right);
enum PanelKind { TeamStrip, PokemonCard, EncounterStrip, AreaCard };
void DrawPanel(QPainter& painter,const QRect& rect,PanelKind kind);
bool ClickPanel(const QPoint& position,const QRect& rect,PanelKind kind,QWidget* parent);
void SelectPokemon(int index);
void CyclePokemon(int direction);
void SetFollowActive(bool enabled);
void SetTeamKeyLabels(QString previous,QString next);
void SetLeagueTarget(int trainer);
bool Click(const QPoint& position,const QRect& left,const QRect& right,QWidget* parent);
}
