#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QSaveFile>
#include <QDialog>
#include <QFormLayout>
#include <QKeySequenceEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QJsonArray>
#include <QImage>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QFileDialog>
#include <array>
#include "Nuzlocke.h"
#include "Gdb.h"
#include "Mac.h"
#include "ScreenLayout.h"
QString emuDirectory;
QString resourceDirectory;
class Panel:public QWidget {
 Nuzlocke::PanelKind kind;
 public:
 Panel(Nuzlocke::PanelKind type):QWidget(nullptr,Qt::Tool|Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint|Qt::WindowDoesNotAcceptFocus),kind(type){setAttribute(Qt::WA_ShowWithoutActivating);setWindowTitle(QStringList{"Nuzlocke — Team","Nuzlocke — Pokémon","Nuzlocke — Encounters","Nuzlocke — Area and opponent"}[int(kind)]);}
 void paintEvent(QPaintEvent*)override {QPainter p(this);Nuzlocke::DrawPanel(p,rect(),kind);}
 void mousePressEvent(QMouseEvent*e)override{if(e->button()==Qt::LeftButton)Nuzlocke::ClickPanel(e->position().toPoint(),rect(),kind,this);}
};
static std::array<QRect,4> areas(QRect game,bool forceFullscreen=false){
 QFile f(QDir::homePath()+"/Library/Preferences/melonDS/melonDS.toml");QString cfg;if(f.open(QIODevice::ReadOnly))cfg=QString::fromUtf8(f.readAll());
 auto read=[&](QString name,int fallback){QRegularExpression rx("(?m)^"+name+" = (\\d+)");auto m=rx.match(cfg);return m.hasMatch()?m.captured(1).toInt():fallback;};
 auto flag=[&](QString name){return cfg.contains(QRegularExpression("(?m)^"+name+" = true$"));};
 QRect viewport=game;bool full=forceFullscreen;for(auto screen:QGuiApplication::screens())if(game==screen->geometry())full=true;if(!full)viewport.adjust(0,25,0,0);
 ScreenLayout layout;layout.Setup(viewport.width(),viewport.height(),ScreenLayoutType(read("ScreenLayout",0)),ScreenRotation(read("ScreenRotation",0)),ScreenSizing(read("ScreenSizing",1)),read("ScreenGap",0),flag("IntegerScaling"),flag("ScreenSwap"),1,1);
 float matrices[18];int kinds[3];int count=layout.GetScreenTransforms(matrices,kinds);QRect bottom,top;
 for(int i=0;i<count;i++){float*m=matrices+6*i;auto r=QTransform(m[0],m[1],m[2],m[3],m[4],m[5]).mapRect(QRectF(0,0,256,192)).toAlignedRect();if(kinds[i]==1)bottom=r;else if(kinds[i]==0)top=r;}
 if(bottom.isEmpty()||top.isEmpty())return {};
 int y=std::max(0,bottom.top()),h=std::min(viewport.height()-y,bottom.height());
 std::array<QRect,4> out{QRect(4,std::max(3,top.top()+3),std::max(0,top.left()-8),std::max(0,y-std::max(0,top.top())-6)),QRect(4,y+3,std::max(0,bottom.left()-8),h-6),QRect(top.right()+5,std::max(3,top.top()+3),std::max(0,viewport.width()-top.right()-9),std::max(0,y-std::max(0,top.top())-6)),QRect(bottom.right()+5,y+3,std::max(0,viewport.width()-bottom.right()-9),h-6)};
 for(auto&r:out)r.translate(viewport.topLeft());return out;
}
int main(int argc,char**argv){
 QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);QApplication app(argc,argv);app.setOrganizationName("NuzlockeCompanion");app.setApplicationName("Nuzlocke Companion");app.setQuitOnLastWindowClosed(true);
 app.setStyleSheet("QWidget {font-family:Monaco; font-size:12px; color:#e6edf4;} QDialog,QMessageBox,QWidget#companionControl {background:#182a3d; color:#e6edf4;} QLabel,QCheckBox {color:#e6edf4;} QTextBrowser {background:#213d52; color:#e6edf4; border:3px double #bdcbd6; border-radius:0px;} QComboBox,QKeySequenceEdit {background:#294c65; color:#f4f4ef; border:2px solid #93a9b7; padding:6px;} QPushButton {background:#294c65; color:#f4f4ef; border:3px double #c4d0d8; border-radius:0px; padding:7px;} QPushButton:hover {border-color:#ffb24a; background:#385c72;} QPushButton:pressed {background:#18334c;} ");
 emuDirectory=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);QDir().mkpath(emuDirectory);
 resourceDirectory=QDir(QCoreApplication::applicationDirPath()+"/../Resources/nuzlocke").absolutePath();
 if(argc==4&&QString::fromUtf8(argv[1])=="--render-preview"){
  QTemporaryDir temporary;emuDirectory=temporary.path();QFile ram(QString::fromUtf8(argv[2]));if(!ram.open(QIODevice::ReadOnly))return 2;Nuzlocke::AcceptRAM(ram.readAll(),false);
  QImage image(3024,1928,QImage::Format_ARGB32_Premultiplied);image.fill(QColor("#080d13"));QPainter p(&image);p.scale(2,2);
  auto positions=areas(QRect(0,0,1512,964),true);for(int i=0;i<4;i++)Nuzlocke::DrawPanel(p,positions[i],Nuzlocke::PanelKind(i));p.end();return image.save(QString::fromUtf8(argv[3]))?0:3;
 }
 QWidget control;control.setObjectName("companionControl");control.setWindowTitle("Nuzlocke Companion — melonDS overlay");control.resize(580,370);QVBoxLayout layout(&control);
 QLabel intro("Open melonDS and continue your run.\nTeam and encounters at the sides; Pokémon and area details below.");intro.setWordWrap(true);layout.addWidget(&intro);
 QLabel status("Waiting for connection…");status.setWordWrap(true);layout.addWidget(&status);
 QPushButton settings("Keys and preferences");layout.addWidget(&settings);
 QJsonObject preferences;{QFile f(emuDirectory+"/preferences.json");if(f.open(QIODevice::ReadOnly))preferences=QJsonDocument::fromJson(f.readAll()).object();}
 if(!QFile::exists(resourceDirectory+"/database.json"))resourceDirectory=preferences["resource_path"].toString();
 if(!QFile::exists(resourceDirectory+"/database.json")){
  QMessageBox::information(&control,"Prepare local resources","Run tools/prepare_resources.py with your own Italian White 2 ROM first, as explained in the README. Then select the resulting resources/nuzlocke folder. No game data is included in this app.");
  resourceDirectory=QFileDialog::getExistingDirectory(&control,"Select prepared resources/nuzlocke folder");
  QFile database(resourceDirectory+"/database.json");
  if(!database.open(QIODevice::ReadOnly)||QJsonDocument::fromJson(database.readAll()).object()["game_code"]!="IRDI"){QMessageBox::information(&control,"Resources unavailable","A valid IRDI database.json is required. Follow the resource setup instructions in the README.");return 2;}
  preferences["resource_path"]=resourceDirectory;QSaveFile saved(emuDirectory+"/preferences.json");if(saved.open(QIODevice::WriteOnly)){saved.write(QJsonDocument(preferences).toJson());saved.commit();}
 }
 QKeySequence previous(preferences["previous_key"].toString("[")),next(preferences["next_key"].toString("]"));
 Nuzlocke::SetDeadBox(preferences["dead_box"].toInt());Nuzlocke::SetLeagueTarget(preferences["league_target"].toInt());Nuzlocke::SetFollowActive(preferences["follow_active"].toBool(true));Nuzlocke::SetTeamKeyLabels(previous.toString(QKeySequence::NativeText),next.toString(QKeySequence::NativeText));
 QString keyError=ConfigureTeamKeys(previous,next,[](int direction){Nuzlocke::CyclePokemon(direction);});
 QObject::connect(&settings,&QPushButton::clicked,&control,[&]{
  EnableTeamKeys(false);QDialog dialog(&control);dialog.setWindowTitle("Keys and preferences");QFormLayout form(&dialog);QKeySequenceEdit prevEdit(previous),nextEdit(next);prevEdit.setMaximumSequenceLength(1);nextEdit.setMaximumSequenceLength(1);form.addRow("Previous Pokémon",&prevEdit);form.addRow("Next Pokémon",&nextEdit);QCheckBox follow("Follow your active Pokémon when it switches");follow.setChecked(preferences["follow_active"].toBool(true));form.addRow(&follow);QComboBox deadBox;deadBox.addItem("None · mark deaths manually",0);for(int i=1;i<=24;i++)deadBox.addItem(QString("Box %1").arg(i),i);deadBox.setCurrentIndex(std::max(0,deadBox.findData(preferences["dead_box"].toInt())));form.addRow("Dead Pokémon box (new imports)",&deadBox);QComboBox league;league.addItem("Automatic · first undefeated member",0);league.addItem("Shauntal · Ghost",38);league.addItem("Marshal · Fighting",39);league.addItem("Grimsley · Dark",40);league.addItem("Caitlin · Psychic",41);league.setCurrentIndex(std::max(0,league.findData(preferences["league_target"].toInt())));form.addRow("Next Elite Four member",&league);QLabel hint("Shortcuts work while melonDS is in front. Avoid keys used by the game. Bracket shortcuts adapt to your keyboard layout.");hint.setWordWrap(true);form.addRow(&hint);QDialogButtonBox buttons(QDialogButtonBox::Save|QDialogButtonBox::Cancel);buttons.button(QDialogButtonBox::Save)->setText("Save");buttons.button(QDialogButtonBox::Cancel)->setText("Cancel");form.addRow(&buttons);QObject::connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);QObject::connect(&buttons,&QDialogButtonBox::accepted,&dialog,[&]{auto error=ConfigureTeamKeys(prevEdit.keySequence(),nextEdit.keySequence(),[](int d){Nuzlocke::CyclePokemon(d);});if(!error.isEmpty()){QMessageBox::information(&dialog,"Choose different keys",error);return;}previous=prevEdit.keySequence();next=nextEdit.keySequence();preferences["previous_key"]=previous.toString();preferences["next_key"]=next.toString();preferences["follow_active"]=follow.isChecked();preferences["dead_box"]=deadBox.currentData().toInt();Nuzlocke::SetDeadBox(deadBox.currentData().toInt());preferences["league_target"]=league.currentData().toInt();Nuzlocke::SetLeagueTarget(league.currentData().toInt());Nuzlocke::SetFollowActive(follow.isChecked());Nuzlocke::SetTeamKeyLabels(previous.toString(QKeySequence::NativeText),next.toString(QKeySequence::NativeText));QSaveFile f(emuDirectory+"/preferences.json");if(f.open(QIODevice::WriteOnly)){f.write(QJsonDocument(preferences).toJson());f.commit();}keyError.clear();dialog.accept();});dialog.exec();
 });
 QPushButton open("Open melonDS"),help("Connection help"),registry("Open ledger folder"),report("Save a diagnostic report"),toggle("Hide / show overlay");for(auto b:{&open,&help,&registry,&report,&toggle})layout.addWidget(b);
 QObject::connect(&open,&QPushButton::clicked,[]{QDesktopServices::openUrl(QUrl::fromLocalFile(QFileDialog::getExistingDirectory(nullptr,"Choose melonDS.app","/Applications")));});
 QObject::connect(&help,&QPushButton::clicked,&control,[&]{QMessageBox::information(&control,"Connect to melonDS","In melonDS:\n1. Config → Emu settings: disable JIT.\n2. Devtools: enable GDB, ARM9 port 3333.\n3. Disable both Break on startup options.\n4. Restart melonDS and continue your game.\n\nUse a large top screen and a small centered bottom screen.\nPause using your melonDS pause shortcut before reading detail dialogs.");});
 QObject::connect(&registry,&QPushButton::clicked,[]{QDesktopServices::openUrl(QUrl::fromLocalFile(emuDirectory));});
 QObject::connect(&report,&QPushButton::clicked,&control,[&]{QSaveFile f(emuDirectory+"/diagnostic-report.json");if(f.open(QIODevice::WriteOnly)){f.write(QJsonDocument(Nuzlocke::State()).toJson());if(f.commit())QMessageBox::information(&control,"Report saved","diagnostic-report.json is in the ledger folder. It includes your Pokémon nicknames and run history. Review it before sharing; it contains no ROM or screenshots.");}});
 bool visible=true;QString panelStatus;QLabel panels;panels.setWordWrap(true);layout.addWidget(&panels);QObject::connect(&toggle,&QPushButton::clicked,[&]{visible=!visible;});Panel team(Nuzlocke::TeamStrip),left(Nuzlocke::PokemonCard),wild(Nuzlocke::EncounterStrip),right(Nuzlocke::AreaCard);std::array<Panel*,4> overlay{&team,&left,&wild,&right};Reader reader;reader.start();QTimer timer;
 QObject::connect(&timer,&QTimer::timeout,&control,[&]{auto state=Nuzlocke::State();bool connected=state["connected"].toBool();if(connected)status.setText(state["ready"].toBool()?QString("Connected · %1 · Cap %2\n%3").arg(state["phase"]=="fight"?"Battle":"Exploring").arg(state["cap"].toInt()).arg(state["area"].toObject()["name"].toString()):"Connected · continue your game.");else status.setText(state["connection_message"].toString("Waiting for melonDS."));
  QRect game;bool found=GameWindow(game);auto positions=found?areas(game):std::array<QRect,4>{};bool show=visible&&connected&&state["ready"].toBool()&&positions[1].width()>120&&positions[1].height()>70&&positions[3].width()>120;
  panelStatus=!visible?"Overlay hidden: click Show overlay.":!connected?"Overlay waiting for a connection.":show?"Overlay active · "+previous.toString(QKeySequence::NativeText)+" / "+next.toString(QKeySequence::NativeText)+" to browse your team.":!found?GameWindowStatus():"Enlarge melonDS; use a large top screen and a small centered bottom screen.";if(!keyError.isEmpty())panelStatus+="\n"+keyError;panels.setText(panelStatus);toggle.setText(visible?"Hide overlay":"Show overlay");EnableTeamKeys(show&&GameIsFrontmost());
  for(int i=0;i<4;i++){auto view=overlay[i];bool enough=positions[i].width()>(i%2==0?100:120)&&positions[i].height()>(i%2==0?420:70);if(!show||!enough){view->hide();continue;}view->setGeometry(positions[i]);if(!view->isVisible()){FloatOnGame(view->winId());view->show();}FloatOnGame(view->winId());view->update();}

 });timer.start(200);control.show();int result=app.exec();EnableTeamKeys(false);reader.requestInterruption();reader.wait(5000);return result;
}
