// Native, read-only Gen V monitor. Game state is never written by this module.
#include "Nuzlocke.h"
#include "main.h"
#include <QSaveFile>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QDialog>
#include <QDate>
#include <QWidget>
#include <QVBoxLayout>
#include <QTextBrowser>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QDialogButtonBox>
#include <algorithm>
#include <array>
#include <atomic>
#include <vector>
#include <set>
#include <cmath>
namespace Nuzlocke {
static QMutex mutex;
static QJsonObject state,db,ledger;
static QString resources;
static bool loaded=false,dirty=false;
static std::atomic<int> deadBox{0};
void SetDeadBox(int box){deadBox=std::clamp(box,0,24);}
static std::atomic<int> selected{0};
static std::atomic<bool> followActive{true};
static QString teamKeyLabels="[ / ]";
static std::atomic<int> leagueTarget{0};
void SetLeagueTarget(int trainer){leagueTarget=trainer;}
void SetTeamKeyLabels(QString previous,QString next){QMutexLocker lock(&mutex);teamKeyLabels=previous+" / "+next;}
void SelectPokemon(int index){QMutexLocker lock(&mutex);int n=state["party"].toArray().size();if(n)selected=std::clamp(index,0,n-1);}
void CyclePokemon(int direction){QMutexLocker lock(&mutex);int n=state["party"].toArray().size();if(n)selected=(selected.load()+direction+n)%n;}
void SetFollowActive(bool enabled){followActive=enabled;}
static int methodSelected=0;
static int encounterPage=0;
static const QStringList types={"Normal","Fighting","Flying","Poison","Ground","Rock","Bug","Ghost","Steel","Fire","Water","Grass","Electric","Psychic","Ice","Dragon","Dark"};
static const QStringList natures={"Hardy","Lonely","Brave","Adamant","Naughty","Bold","Docile","Relaxed","Impish","Lax","Timid","Hasty","Serious","Jolly","Naive","Modest","Mild","Quiet","Bashful","Rash","Calm","Gentle","Sassy","Careful","Quirky"};
static uint16_t u16(const uint8_t*p,int off=0){return p[off]|p[off+1]<<8;}
static uint32_t u32(const uint8_t*p,int off=0){return uint32_t(u16(p,off))|(uint32_t(u16(p,off+2))<<16);}
static QJsonObject readJson(QString path){QFile f(path);if(!f.open(QIODevice::ReadOnly))return {};return QJsonDocument::fromJson(f.readAll()).object();}
static void flush(){if(!dirty)return;QSaveFile f(emuDirectory+"/nuzlocke-run.json");if(f.open(QIODevice::WriteOnly)){f.write(QJsonDocument(ledger).toJson());if(f.commit())dirty=false;}}
static void initialize(){
 if(loaded)return;loaded=true;resources=resourceDirectory;db=readJson(resources+"/database.json");
 ledger=readJson(emuDirectory+"/nuzlocke-run.json");if(!ledger.isEmpty()){auto areas=ledger["areas"].toObject();for(auto it=areas.begin();it!=areas.end();++it)if(it.value()=="pending"){it.value()="lost";dirty=true;}ledger["areas"]=areas;flush();return;}
 // Start with a clean ledger. Existing Pokémon are imported from live memory.
 ledger=QJsonObject{{"version",1},{"pokemon",QJsonObject{}},{"areas",QJsonObject{}},{"league_wins",QJsonArray{}}};dirty=true;flush();
}
static QJsonObject move(int id,int pp,int max){auto a=db["moves"].toArray();if(id<=0||id>=a.size())return {};auto m=a[id].toObject();m["pp"]=pp;m["max_pp"]=max;return m;}
static QString statusText(int code){QStringList names={"Normal","Paralyzed","Asleep","Frozen","Burned","Poisoned"};return names[std::clamp<int>(code&7,0,5)];}
static int capForBadges(int badges){int caps[]={13,18,24,30,33,39,48,51,58};return badges>=8?(ledger["league_wins"].toArray().size()>=4?59:58):caps[std::clamp(badges,0,8)];}
static QJsonObject decode(const uint8_t*raw,int len){
 if(len!=136&&len!=220)return {};uint32_t pid=u32(raw),seed=u16(raw,6);std::array<uint8_t,220>d{};std::copy(raw,raw+len,d.begin());
 for(int p=8;p<136;p+=2){seed=seed*0x41C64E6D+0x6073;uint16_t x=u16(raw,p)^(seed>>16);d[p]=x;d[p+1]=x>>8;}
 unsigned sum=0;for(int p=8;p<136;p+=2)sum+=u16(d.data(),p);if((sum&65535)!=u16(raw,6))return {};
 std::array<int,4> perm={0,1,2,3};int sv=((pid>>13)&31)%24;for(int i=0;i<sv;i++)std::next_permutation(perm.begin(),perm.end());auto shuffled=d;for(int pos=0;pos<4;pos++)std::copy(shuffled.begin()+8+32*pos,shuffled.begin()+40+32*pos,d.begin()+8+32*perm[pos]);
 int sid=u16(d.data(),8);auto species=db["species"].toArray();if(sid<1||sid>=species.size()||d[65]>=25)return {};auto sp=species[sid].toObject();
 QString nick;for(int p=72;p<94;p+=2){int ch=u16(d.data(),p);if(ch==0||ch==65535)break;nick+=QChar(ch);}
 QJsonArray moves,ivs,evs,stats;for(int i=0;i<4;i++){int id=u16(d.data(),40+i*2);if(!id)continue;auto md=db["moves"].toArray();if(id>=md.size())return {};moves.append(move(id,d[48+i],md[id].toObject()["base_pp"].toInt()*(5+d[52+i])/5));}
 uint32_t iv=u32(d.data(),56);for(int i:{0,1,2,4,5,3}){ivs.append(int((iv>>(i*5))&31));evs.append(int(d[24+i]));}
 QString key=QString::number(pid,16).rightJustified(8,'0').toUpper();auto abilities=db["abilities"].toArray();auto items=db["items"].toArray();if(d[21]>=abilities.size()||u16(d.data(),10)>=items.size())return {};
 QJsonObject m{{"pid",key},{"species_id",sid},{"species",sp["name"]},{"nickname",nick},{"types",sp["types"]},{"family",sp["family"]},{"nature",natures[d[65]]},{"ability_id",int(d[21])},{"ability",abilities[d[21]]},{"item",items[u16(d.data(),10)]},{"experience",int(u32(d.data(),16))},{"ivs",ivs},{"evs",evs},{"moves",moves},{"shiny",(u16(d.data(),12)^u16(d.data(),14)^(pid&65535)^(pid>>16))<8},{"met_location",int(u16(d.data(),128))},{"met_level",int(d[132]&127)}};
 if(len==220){seed=pid;for(int p=136;p<len;p+=2){seed=seed*0x41C64E6D+0x6073;uint16_t x=u16(raw,p)^(seed>>16);d[p]=x;d[p+1]=x>>8;}if(!d[140]||d[140]>100||u16(d.data(),144)>1000)return {};for(int p:{146,148,152,154,150})stats.append(int(u16(d.data(),p)));m["level"]=int(d[140]);m["hp"]=int(u16(d.data(),142));m["max_hp"]=int(u16(d.data(),144));m["stats"]=stats;m["status"]=int(u32(d.data(),136));m["status_label"]=statusText(u32(d.data(),136));}
 auto old=ledger["pokemon"].toObject()[key].toObject();m["run_state"]=old["run_state"].toString("alive");return m;
}
static void registerMon(QJsonObject m){if(m.isEmpty())return;auto mons=ledger["pokemon"].toObject();auto key=m["pid"].toString();if(!mons.contains(key)){mons[key]=m;ledger["pokemon"]=mons;if(!m["shiny"].toBool()){auto a=ledger["areas"].toObject();a[QString::number(m["met_location"].toInt())]="used";ledger["areas"]=a;}dirty=true;}}

struct BattleSession { uint32_t setup=0,module=0;QString key;int area=-1,trainer=0;QString enemyPID;bool wild=false,eligible=false,victoryEvidence=false; };
static BattleSession battle;
static bool validAddress(uint32_t a,int len=512){return a>=0x02000000 && uint64_t(a)+len<=0x02400000;}
static void setArea(int id,QString code){auto areas=ledger["areas"].toObject();auto key=QString::number(id);if(areas[key]==code)return;areas[key]=code;ledger["areas"]=areas;dirty=true;}
static void finishBattle(const uint8_t*ram){
 if(!battle.setup)return;
 if(validAddress(battle.setup)){
  auto setup=ram+battle.setup-0x02000000;int result=u32(setup,0xa8);
  if(battle.wild && battle.eligible && result>=0 && result<=5)setArea(battle.area,(result==5||ledger["pokemon"].toObject().contains(battle.enemyPID))?"used":"lost");
  if(!battle.wild&&battle.victoryEvidence&&result==1&&battle.trainer>=38&&battle.trainer<=41){auto wins=ledger["league_wins"].toArray();bool exists=false;for(auto v:wins)if(v.toInt()==battle.trainer)exists=true;if(!exists){wins.append(battle.trainer);ledger["league_wins"]=wins;dirty=true;}}
 }
 battle={};state.remove("enemy");state.remove("enemies");state.remove("active_pid");
}
static void readBattle(const uint8_t*ram,QJsonArray&party){
 auto ptr=[&](uint32_t a,int len=512)->const uint8_t*{return validAddress(a,len)?ram+a-0x02000000:nullptr;};
 auto gp=ptr(0x02141034);auto gs=gp?ptr(u32(gp)):nullptr;auto mgr=gs?ptr(u32(gs,8)):nullptr;
 const uint8_t* proc=nullptr;const uint8_t* module=nullptr;const uint8_t* setup=nullptr;
 std::vector<uint32_t> queue;if(mgr)queue.push_back(u32(mgr,8));std::set<uint32_t>visited;QJsonArray trace;
 for(size_t i=0;i<queue.size()&&i<16;i++){
  uint32_t address=queue[i];if(!visited.insert(address).second)continue;auto candidate=ptr(address);if(!candidate)continue;
  queue.push_back(u32(candidate,24));queue.push_back(u32(candidate,28));
  auto candidateModule=ptr(u32(candidate,20),0x450);auto candidateSetup=candidateModule?ptr(u32(candidateModule)):nullptr;
  trace.append(QJsonObject{{"process",QString::number(address,16)},{"overlay",int(u32(candidate,4))},{"module",QString::number(u32(candidate,20),16)},{"setup_type",candidateSetup?int(u32(candidateSetup)):-1},{"setup_style",candidateSetup?int(u32(candidateSetup,4)):-1},{"pokecon_valid",candidateModule&&u32(candidateModule,0xc8)==u32(candidate,20)}});
  // Recognize a live module by its self-referencing PokeCon and valid parties,
  // including battle parents while a child process owns the current screen.
  if(!candidateSetup||u32(candidateSetup)>4||u32(candidateSetup,4)>3||u32(candidateModule,0xc8)!=u32(candidate,20))continue;
  bool own=false,foe=false;
  for(int c=0;c<4;c++){auto bp=candidateModule+0xcc+c*28;if(bp[24]<1||bp[24]>6)continue;auto bm=ptr(u32(bp));auto source=bm?ptr(u32(bm),220):nullptr;auto m=source?decode(source,220):QJsonObject{};if(m.isEmpty()||m["species_id"].toInt()!=u16(bm,12))continue;bool ours=false;for(auto v:party)if(v.toObject()["pid"]==m["pid"])ours=true;if(ours)own=true;else foe=true;}
  if(!own||!foe)continue;proc=candidate;module=candidateModule;setup=candidateSetup;break;
 }
 state["monitor"]=QJsonObject{{"processes",trace},{"source","melonDS GDB ARM9"},{"version","0.4.0"}};
 if(!module){finishBattle(ram);return;}auto client=module+0xc8;
 int style=u32(setup,4),frontCount=style==0||style==3?2:style==1?4:6;
 uint32_t setupAddress=u32(module);QString sessionKey=QString::number(setupAddress)+":"+QString::number(u32(setup,0x24))+":"+QString::number(u32(setup,0x28));
 if(battle.setup && battle.key!=sessionKey)finishBattle(ram);
 if(!battle.setup){battle.setup=setupAddress;battle.module=u32(proc,20);battle.key=sessionKey;battle.wild=u32(setup)==0;battle.area=state["area"].toObject()["name_id"].toInt();auto trainer=ptr(u32(setup,0x4c));battle.trainer=trainer?u32(trainer):0;}
 // Each client's party is reordered by the game when a Pokemon switches in.
 QJsonArray enemies;std::array<int,4>indices{};int activeOwn=-1;bool allFoesFainted=true,hasFoes=false;
 for(int c=0;c<4;c++){auto bp=client+4+c*28;if(!bp[24]||bp[24]>6)continue;auto first=ptr(u32(bp));auto source=first?ptr(u32(first),220):nullptr;auto m=source?decode(source,220):QJsonObject{};bool own=false;for(auto v:party)if(v.toObject()["pid"]==m["pid"])own=true;if(m.isEmpty()||own)continue;hasFoes=true;for(int i=0;i<bp[24];i++){auto bm=ptr(u32(bp,i*4));if(!bm||u16(bm,16)>0)allFoesFainted=false;}}
 if(hasFoes&&allFoesFainted)battle.victoryEvidence=true;
 for(int position=0;position<frontCount;position++){
  int owner=module[0x428+position];if(owner>3)continue;int index=indices[owner]++;auto bp=client+4+28*owner;if(index>=bp[24])continue;auto bm=ptr(u32(bp,index*4));if(!bm)continue;auto src=ptr(u32(bm),220);if(!src)continue;auto m=decode(src,220);if(m.isEmpty()||m["species_id"].toInt()!=u16(bm,12)||m["level"].toInt()!=bm[24])continue;
  int status=0;for(int condition=1;condition<=5;condition++)if(u32(bm,0x1c+condition*4)&7){status=condition;break;}m["status"]=status;m["status_label"]=statusText(status);auto items=db["items"].toArray();int held=u16(bm,18);if(held<items.size())m["item"]=items[held];m["hp"]=int(u16(bm,16));m["max_hp"]=int(u16(bm,14));int aid=u16(bm,0x13c);auto abilities=db["abilities"].toArray();if(aid>=0&&aid<abilities.size()){m["ability_id"]=aid;m["ability"]=abilities[aid];}m["types"]=QJsonArray{int(bm[0xf8]),int(bm[0xf9])};
  QJsonArray stats,stages;for(int j=0;j<5;j++){int stage=bm[0xfc+j]-6;double scale=stage>=0?(2.0+stage)/2.0:2.0/(2.0-stage);stats.append(int(std::floor(u16(bm,0xee +j*2)*scale)));stages.append(stage);}m["stats"]=stats;m["stages"]=stages;
  bool own=false;int partyIndex=-1;for(int i=0;i<party.size();i++)if(party[i].toObject()["pid"]==m["pid"]){own=true;partyIndex=i;break;}
  QJsonArray moves;for(int j=0;j<4;j++){auto core=bm+0x104+j*14;int id=u16(core);if(!id)continue;bool revealed=own||core[5]!=0||u16(bm,0x14a)==id||u16(bm,0x14c)==id;moves.append(revealed?QJsonValue(move(id,core[2],core[3])):QJsonValue(QJsonObject{}));}m["moves"]=moves;
  if(own){party[partyIndex]=m;if(activeOwn<0)activeOwn=partyIndex;if(m["hp"].toInt()==0){auto mons=ledger["pokemon"].toObject();auto previous=mons[m["pid"].toString()].toObject();if(previous["run_state"]!="dead"){previous["run_state"]="dead";mons[m["pid"].toString()]=previous;ledger["pokemon"]=mons;dirty=true;}m["run_state"]="dead";party[partyIndex]=m;}}
  else{enemies.append(m);if(battle.wild&&!m["shiny"].toBool()){int family=db["species"].toArray()[m["species_id"].toInt()].toObject()["family"].toInt();bool dup=false;for(auto v:ledger["pokemon"].toObject()){int sid=v.toObject()["species_id"].toInt();if(db["species"].toArray()[sid].toObject()["family"].toInt()==family){dup=true;break;}}auto area=ledger["areas"].toObject()[QString::number(battle.area)].toString("unknown");if(!dup&&area!="used"&&area!="lost"){battle.eligible=true;battle.enemyPID=m["pid"].toString();setArea(battle.area,"pending");}}}
 }
 if(activeOwn>=0){QString pid=party[activeOwn].toObject()["pid"].toString();if(state["active_pid"].toString()!=pid&&followActive)selected=activeOwn;state["active_pid"]=pid;}state["enemies"]=enemies;if(!enemies.isEmpty())state["enemy"]=enemies[0];else state.remove("enemy");
 // Zero HP is monitored across all six own combatants, including recoil and switches.
 for(int i=0;i<6;i++){auto bm=ptr(u32(client,0x84+i*4));if(!bm||u16(bm,16)!=0||u16(bm,14)==0)continue;auto src=ptr(u32(bm),220);if(!src)continue;auto m=decode(src,220);auto mons=ledger["pokemon"].toObject();auto key=m["pid"].toString();if(!m.isEmpty()&&mons.contains(key)){auto old=mons[key].toObject();if(old["run_state"]!="dead"){old["run_state"]="dead";mons[key]=old;ledger["pokemon"]=mons;dirty=true;}}}
 int result=u32(setup,0xa8);if(battle.wild&&battle.eligible&&(result==3||result==4||result==5))setArea(battle.area,(result==5||ledger["pokemon"].toObject().contains(battle.enemyPID))?"used":"lost");
}
QJsonObject State(){QMutexLocker lock(&mutex);auto snapshot=state;snapshot["selected_index"]=selected.load();return snapshot;}
void Disconnected(QString reason){QMutexLocker lock(&mutex);state["connected"]=false;state["connection_message"]=reason;}
void AcceptRAM(const QByteArray& bytes,bool boxesReady){
 if(bytes.size()!=0x400000)return;QMutexLocker lock(&mutex);initialize();if(db.isEmpty())return;
 const auto ram=(const uint8_t*)bytes.constData();auto base=ram+0x205524;
 state["connected"]=true;int count=u32(base,0x18e04);if(count<1||count>6){state["ready"]=false;state["phase"]="loading";return;}
 QJsonArray party;for(int i=0;i<count;i++){auto m=decode(base+0x18e08+220*i,220);if(m.isEmpty()){state["ready"]=false;return;}registerMon(m);party.append(m);}state["ready"]=true;
 if(boxesReady)for(int b=0;b<24;b++)for(int slot=0;slot<30;slot++){auto mon=decode(base+0x400+b*0x1000+slot*136,136);if(!mon.isEmpty()&&b+1==deadBox)mon["run_state"]="dead";registerMon(mon);}
 // Field::ZoneID is live; PlayerSaveData::NowSpawnZone can lag until saving.
 auto maps=db["maps"].toArray();int savedMap=u32(base,0x19580),map=-1;QString areaSource="previous";
 uint32_t gsAddress=u32(ram+0x141034);
 if(validAddress(gsAddress,0x24)){
  const auto gs=ram+gsAddress-0x02000000;uint32_t fieldAddress=u32(gs,0x20);
  if(validAddress(fieldAddress,0xf0)){
   const auto field=ram+fieldAddress-0x02000000;int zone=u16(field,0xe0);
   if(u32(field,4)==gsAddress&&u32(field,8)==u32(gs,0x1c)&&u32(field,0xd0)==1&&u32(field,0xd4)==3&&zone<maps.size()){map=zone;areaSource="field";}
  }
 }
 // Keep the last valid zone during warps, menus and battle transitions.
 if(map<0&&state["area"].toObject().isEmpty()&&savedMap>=0&&savedMap<maps.size()){map=savedMap;areaSource="save_fallback";}
 if(map>=0){if(state["area"].toObject()["id"].toInt(-1)!=map){methodSelected=0;encounterPage=0;}state["area"]=maps[map];}
 state["location_monitor"]=QJsonObject{{"source",areaSource},{"saved_map",savedMap},{"map",state["area"].toObject()["id"]}};
 int badges=__builtin_popcount(base[0x21104]);state["badges"]=badges;state["cap"]=capForBadges(badges);state["season"]=(QDate::currentDate().month()-1)%4;
 state["repel_steps"]=int(base[0x2192d]);int repelItem=u16(base,0x2192e);state["repel_item"]=repelItem<db["items"].toArray().size()?db["items"].toArray()[repelItem]:QJsonValue("Repel");
 readBattle(ram,party);state["phase"]=state.value("enemy").toObject().isEmpty()?"field":"fight";state["party"]=party;state["cap"]=capForBadges(badges);state["ledger"]=ledger;flush();
}
// Generation V chart: row attacking type, column defending type.
static double effectiveness(int attack,const QJsonObject&m){
 if(attack<0||attack>=17)return 1;
 static const int strong[17][6]={{-1},{0,5,8,14,16,-1},{1,6,11,-1},{11,-1},{3,5,8,9,12,-1},{2,6,9,14,-1},{11,13,16,-1},{7,13,-1},{5,14,-1},{6,8,11,14,-1},{4,5,9,-1},{4,5,10,-1},{2,10,-1},{1,3,-1},{2,4,11,15,-1},{15,-1},{7,13,-1}};
 static const int weak[17][8]={{5,8,-1},{2,3,6,13,-1},{5,8,12,-1},{3,4,5,7,-1},{6,11,-1},{1,4,8,-1},{1,2,3,7,8,9,-1},{8,16,-1},{8,9,10,12,-1},{5,9,10,15,-1},{10,11,15,-1},{2,3,6,8,9,11,15,-1},{11,12,15,-1},{8,13,-1},{8,9,10,14,-1},{8,-1},{1,8,16,-1}};
 double result=1;std::set<int>seen;for(auto v:m["types"].toArray()){int t=v.toInt();if(t<0||t>16||!seen.insert(t).second)continue;if((attack==0&&t==7)||(attack==1&&t==7)||(attack==3&&t==8)||(attack==4&&t==2)||(attack==12&&t==4)||(attack==13&&t==16)||(attack==7&&t==0))return 0;for(int x:strong[attack]){if(x<0)break;if(x==t)result*=2;}for(int x:weak[attack]){if(x<0)break;if(x==t)result*=.5;}}
 int a=m["ability_id"].toInt();if((attack==4&&a==26)||(attack==12&&(a==10||a==31||a==78))||(attack==10&&(a==11||a==114))||(attack==9&&a==18)||(attack==11&&a==157))return 0;if(a==25&&result<=1)return 0;if(a==47&&(attack==9||attack==14))result*=.5;if(a==85&&attack==9)result*=.5;if(a==87&&attack==9)result*=1.25;if(a==87&&attack==10)return 0;if((a==111||a==116)&&result>1)result*=.75;return result;
}
static QString typeText(QJsonObject m){QStringList a;std::set<int>s;for(auto v:m["types"].toArray()){int t=v.toInt();if(t>=0&&t<17&&s.insert(t).second)a<<types[t];}return a.join(" / ");}
static QString weaknesses(QJsonObject m,bool weak){QStringList a;for(int t=0;t<17;t++){double e=effectiveness(t,m);if((weak&&e>1)||(!weak&&e<1))a<<types[t]+QString(" ×%1").arg(e);}return a.join(" · ");}
static QString numberList(QJsonArray a){QStringList t;for(auto v:a)t<<QString::number(v.toInt());return t.join(" / ");}
static QString areaStatus(){auto a=state["area"].toObject();QString code=ledger["areas"].toObject()[QString::number(a["name_id"].toInt())].toString("unknown");return code=="used"?"Encounter used":code=="lost"?"Encounter lost":code=="available"?"Encounter available":code=="pending"?"Encounter in progress":"Check encounter history";}
static QJsonArray encounters(){auto z=state["area"].toObject();int wild=z["wild"].toInt(255);auto all=db["encounters"].toArray();if(wild>=all.size())return {};auto seasons=all[wild].toArray();if(seasons.isEmpty())return {};return seasons[std::min<int>(state["season"].toInt(),seasons.size()-1)].toArray();}
static QString encounterText(QJsonObject method,bool html=false){
 struct Slot{int chance=0,lo=101,hi=0;};std::map<int,Slot>agg;for(auto v:method["slots"].toArray()){auto s=v.toObject();auto &x=agg[s["species_id"].toInt()];x.chance+=s["chance"].toInt();x.lo=std::min<int>(x.lo,s["min"].toInt());x.hi=std::max(x.hi,s["max"].toInt());}
 std::set<int>families;auto species=db["species"].toArray();for(auto v:ledger["pokemon"].toObject()){int sid=v.toObject()["species_id"].toInt();if(sid>0&&sid<species.size())families.insert(species[sid].toObject()["family"].toInt());}
 QStringList lines;for(auto [id,s]:agg){if(id<1||id>=species.size())continue;auto m=species[id].toObject();bool dup=families.count(m["family"].toInt());lines<<QString("%1%2   %3%   Lv.%4%5").arg(dup?"↪ ":"",m["name"].toString()).arg(s.chance).arg(s.lo).arg(s.lo==s.hi?"":QString("–%1").arg(s.hi));}return lines.join(html?"<br>":"\n");
}
#include "Overlay.inc"
static QString details(QJsonObject m){
 QString s="<h2>"+m["species"].toString().toHtmlEscaped()+QString(" · Level %1</h2>").arg(m["level"].toInt());s+="<p>"+typeText(m)+" · "+m["ability"].toString()+" · "+m["nature"].toString()+"<br>Held item: "+m["item"].toString()+QString("<br>HP: %1/%2 · Experience: %3</p>").arg(m["hp"].toInt()).arg(m["max_hp"].toInt()).arg(m["experience"].toInt());s+="<p>Status: "+m["status_label"].toString()+"<br>Stats (Atk, Def, Sp. Atk, Sp. Def, Speed): "+numberList(m["stats"].toArray())+"<br>Battle stages: "+numberList(m["stages"].toArray())+"</p>";s+="<p>IV/EV order: HP, Attack, Defense, Sp. Attack, Sp. Defense, Speed<br>IV: "+numberList(m["ivs"].toArray())+"<br>EV: "+numberList(m["evs"].toArray())+"</p><table cellpadding='7'><tr><th>Move</th><th>Type</th><th>Category</th><th>Power</th><th>Accuracy</th><th>PP</th></tr>";
 for(auto v:m["moves"].toArray()){auto mv=v.toObject();if(mv.isEmpty()){s+="<tr><td>??? · not revealed yet</td></tr>";continue;}s+=QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>%4</td><td>%5</td><td>%6/%7</td></tr>").arg(mv["name"].toString(),mv["type"].toString(),mv["category"].toString()).arg(mv["power"].toInt()).arg(mv["accuracy"].toInt()>100?"—":QString::number(mv["accuracy"].toInt())+"%").arg(mv["pp"].toInt()).arg(mv["max_pp"].toInt());}s+="</table>";for(auto v:m["moves"].toArray()){auto mv=v.toObject();if(!mv.isEmpty())s+="<p><b>"+mv["name"].toString().toHtmlEscaped()+"</b> · Priority "+QString::number(mv["priority"].toInt())+"<br>"+mv["description"].toString().toHtmlEscaped()+"</p>";}return s+"<p>Weaknesses: "+weaknesses(m,true)+"<br>Resistances / immunities: "+weaknesses(m,false)+"</p>";
}
bool Click(const QPoint&pos,const QRect&left,const QRect&right,QWidget*parent){
 if(!left.contains(pos)&&!right.contains(pos))return false;auto snap=State();if(!snap["ready"].toBool())return true;
 if(left.contains(pos)){QDialog dlg(parent);dlg.setWindowTitle("Team details");dlg.resize(760,560);QVBoxLayout v(&dlg);QComboBox combo;for(auto x:snap["party"].toArray())combo.addItem(x.toObject()["nickname"].toString()+" · "+x.toObject()["species"].toString());combo.setCurrentIndex(selected);v.addWidget(&combo);QTextBrowser browser;v.addWidget(&browser);browser.setHtml(details(snap["party"].toArray()[selected].toObject()));QObject::connect(&combo,qOverload<int>(&QComboBox::currentIndexChanged),&dlg,[&](int i){selected=i;browser.setHtml(details(snap["party"].toArray()[i].toObject()));});dlg.exec();return true;}
 QDialog dlg(parent);dlg.setWindowTitle("Encounters and run ledger");dlg.resize(760,630);QVBoxLayout layout(&dlg);QTextBrowser browser;QString html;{QMutexLocker lock(&mutex);html="<h2>"+state["area"].toObject()["name"].toString()+" · "+areaStatus()+"</h2>";for(auto v:encounters())html+="<h3>"+v.toObject()["method"].toString()+"</h3>"+encounterText(v.toObject(),true);html+="<p>Original encounter probabilities. ↪ marks a species family already obtained: you may skip it under the dupes clause.</p>";for(auto v:state["enemies"].toArray())html+=details(v.toObject());}browser.setHtml(html);layout.addWidget(&browser);
 QComboBox area;area.addItems({"Check history","Available","Used","Lost"});layout.addWidget(new QLabel("Edit the encounter status of the current area:"));layout.addWidget(&area);QStringList codes={"unknown","available","used","lost"};area.setCurrentIndex(std::max(0,int(codes.indexOf(snap["ledger"].toObject()["areas"].toObject()[QString::number(snap["area"].toObject()["name_id"].toInt())].toString("unknown")))));
 QComboBox mon,life;QStringList keys;for(auto v:snap["ledger"].toObject()["pokemon"].toObject()){auto m=v.toObject();keys<<m["pid"].toString();mon.addItem(m["species"].toString()+" · "+m["pid"].toString()+" · "+(m["run_state"]=="dead"?"dead":"alive"));}life.addItems({"Alive","Dead"});layout.addWidget(new QLabel("Edit a Pokémon in your run ledger:"));layout.addWidget(&mon);layout.addWidget(&life);QObject::connect(&mon,qOverload<int>(&QComboBox::currentIndexChanged),&dlg,[&](int i){life.setCurrentIndex(snap["ledger"].toObject()["pokemon"].toObject()[keys[i]].toObject()["run_state"]=="dead"?1:0);});if(!keys.isEmpty())life.setCurrentIndex(snap["ledger"].toObject()["pokemon"].toObject()[keys[0]].toObject()["run_state"]=="dead"?1:0);
 QPushButton save("Save corrections");layout.addWidget(&save);QObject::connect(&save,&QPushButton::clicked,&dlg,[&]{QMutexLocker lock(&mutex);auto a=ledger["areas"].toObject();a[QString::number(snap["area"].toObject()["name_id"].toInt())]=codes[area.currentIndex()];ledger["areas"]=a;if(battle.area==snap["area"].toObject()["name_id"].toInt())battle.eligible=false;if(mon.currentIndex()>=0){auto all=ledger["pokemon"].toObject();auto m=all[keys[mon.currentIndex()]].toObject();m["run_state"]=life.currentIndex()?"dead":"alive";all[keys[mon.currentIndex()]]=m;ledger["pokemon"]=all;}dirty=true;flush();dlg.accept();});dlg.exec();return true;
}

}
