// GUI-thread rendering only. Called with the monitor mutex held.
struct Theme { QColor accent,bg,surface,border,muted; };
static Theme zoneTheme(){
 QString n=state["area"].toObject()["name_it"].toString(state["area"].toObject()["name"].toString()).toLower();QColor a("#69b5f6");
 if(n.contains("pietrelettrica"))a=QColor("#65c9ff");
 else if(n.contains("abissi")||n=="percorso 17"||n=="percorso 21"||n.contains("marino"))a=QColor("#6ba9eb");
 else if(n.contains("gigante")||n.contains("iceberg")||n.contains("frigo")||n.contains("antipodi"))a=QColor("#a7dce9");
 else if(n.contains("deserto")||n=="percorso 4"||n.contains("sepolto")||n.contains("rovine")||n.contains("arida"))a=QColor("#e8bd79");
 else if(n.contains("torre")||n.contains("cimiter")||n.contains("bizzarra"))a=QColor("#b69bea");
 else if(n.contains("fogne")||n.contains("palude"))a=QColor("#a3bf94");
 else if(n.contains("grotta")||n.contains("cava")||n.contains("caverna")||n.contains("monte")||n.contains("passo")||n.contains("sotterranea"))a=QColor("#a5a8df");
 else if(n.contains("mare")||n.contains("baia")||n.contains("ponte")||n.contains("grecal")||n.contains("spiraria")||n=="percorso 13"||n=="percorso 18")a=QColor("#6bd7d0");
 else if(n.contains("bosco")||n.contains("foresta")||n.contains("fattoria")||n.contains("percorso")||n.contains("giardino")||n.contains("radura")||n.contains("riserva")||n.contains("sentiero"))a=QColor(state["season"].toInt()==2?"#dbac76":state["season"].toInt()==3?"#aac9e4":"#8bcea1");
 else if(n.contains("lega")||n.contains("vittoria"))a=QColor("#d2b5f1");
 else if(n.contains("libeccio")||n.contains("austro")||n.contains("scirocco"))a=QColor("#d9b992");
 return {a,QColor::fromRgbF(a.redF()*.15+.045,a.greenF()*.15+.045,a.blueF()*.15+.045),QColor::fromRgbF(a.redF()*.26+.07,a.greenF()*.26+.07,a.blueF()*.26+.07),QColor::fromRgbF(a.redF()*.34+.20,a.greenF()*.34+.20,a.blueF()*.34+.20),QColor("#e3eaf0")};
}
static QColor typeColor(QString type){static const QStringList colors={"#a8a897","#d57864","#9bade0","#b487c4","#cfad76","#c0ae79","#a6ba6a","#9a8fbf","#99b5c2","#ec9971","#7dbbe7","#93c580","#e3cc76","#dc91b0","#99d5d8","#a395dc","#a3968f"};int i=types.indexOf(type);return QColor(i<0?"#8fa0b0":colors[i]);}
// Optional Latin bitmap glyphs are extracted locally from the user's own ROM.
// NFTR glyph cells include three metrics bytes before their 2-bit pixels.
static void text(QPainter&p,QRectF r,QString s,QColor color,int size=13,bool bold=false,int alignment=Qt::AlignLeft|Qt::AlignVCenter){
 Q_UNUSED(bold);static QJsonArray fonts=readJson(resources+"/game-font.json")["fonts"].toArray();
 if(fonts.isEmpty()){QFont f("Monaco");f.setPixelSize(size);p.setFont(f);p.setPen(color);p.drawText(r,alignment,s);return;}
 int index=size<=10?1:0;size=std::min(int(r.height()),size+2);auto font=fonts[index].toObject();auto glyphs=font["glyphs"].toObject();int w=font["width"].toInt(),h=font["height"].toInt();int visibleHeight=h-2;double scale=double(size)/visibleHeight;
 s.replace(QChar(0x00b7),".").replace(QChar(0x2039),QString(QChar(0x2190))).replace(QChar(0x203a),QString(QChar(0x2192))).replace(QChar(0x2014),"-").replace(QChar(0x2013),"-").replace(QChar(0x2019),"'").replace(QChar(0x25be),"v");
 auto glyph=[&](QChar c){auto g=glyphs[QString::number(c.unicode())].toObject();return g.isEmpty()?glyphs[QString::number('?')].toObject():g;};
 auto width=[&](QString value){double total=0;for(QChar c:value)total+=glyph(c)["advance"].toInt()*scale;return total;};
 if(width(s)>r.width()){QString dots=glyphs.contains(QString::number(0x2026))?QString(QChar(0x2026)):QString("...");while(!s.isEmpty()&&width(s+dots)>r.width())s.chop(1);s+=dots;}
 double x=r.left();if(alignment&Qt::AlignHCenter)x+=(r.width()-width(s))/2;else if(alignment&Qt::AlignRight)x+=r.width()-width(s);
 double y=r.top()+(r.height()-size)/2;static QMap<QString,QImage> cache;
 p.save();p.setRenderHint(QPainter::SmoothPixmapTransform,false);p.setClipRect(r,Qt::IntersectClip);
 for(QChar c:s){auto g=glyph(c);QString key=QString::number(index)+":"+QString::number(c.unicode())+":"+color.name(QColor::HexArgb);
  if(!cache.contains(key)){QImage image(w,h,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);auto pixels=g["pixels"].toString();for(int py=0;py<h;py++)for(int px=0;px<w;px++){int value=pixels[py*w+px].digitValue();if(value==1)image.setPixelColor(px,py,color);else if(value>1)image.setPixelColor(px,py,QColor(0,0,0,color.alpha()/3));}cache[key]=image;}
  p.drawImage(QRectF(x,y,w*scale,size),cache[key],QRectF(0,0,w,visibleHeight));x+=g["advance"].toInt()*scale;
 }p.restore();
}
static QPolygonF menuShape(QRectF r,double cut=3){return QPolygonF{QPointF(r.left()+cut,r.top()),QPointF(r.right()-cut,r.top()),QPointF(r.right()-cut,r.top()+cut),QPointF(r.right(),r.top()+cut),QPointF(r.right(),r.bottom()-cut),QPointF(r.right()-cut,r.bottom()-cut),QPointF(r.right()-cut,r.bottom()),QPointF(r.left()+cut,r.bottom()),QPointF(r.left()+cut,r.bottom()-cut),QPointF(r.left(),r.bottom()-cut),QPointF(r.left(),r.top()+cut),QPointF(r.left()+cut,r.top()+cut)};}
static void box(QPainter&p,QRectF r,QColor fill,QColor border,int radius=10){
 Q_UNUSED(radius);p.save();p.setRenderHint(QPainter::Antialiasing,false);bool outer=r.height()>180;double cut=outer?5:2;
 QLinearGradient gradient(r.topLeft(),r.bottomLeft());gradient.setColorAt(0,fill.lighter(145));gradient.setColorAt(.48,fill.lighter(112));gradient.setColorAt(.5,fill);gradient.setColorAt(1,fill.darker(115));
 p.setBrush(gradient);p.setPen(QPen(outer?QColor("#d4d9dc"):border.lighter(160),outer?2:1));p.drawPolygon(menuShape(r,cut));p.setBrush(Qt::NoBrush);p.setPen(QPen(outer?QColor("#526579"):fill.darker(160),1));p.drawPolygon(menuShape(r.adjusted(2,2,-2,-2),std::max(1.,cut-1)));
 
 p.restore();
}
static void pill(QPainter&p,QRectF r,QString label,QColor color){QColor fill=color.darker(175);box(p,r,fill,color.lighter(115),2);text(p,r.adjusted(4,0,-4,0),label,Qt::white,10,true,Qt::AlignCenter);}
static QString nickname(QJsonObject m){return m["nickname"].toString().isEmpty()?m["species"].toString():m["nickname"].toString();}
static void sprite(QPainter&p,QRectF r,QJsonObject m){
 static QMap<QString,QImage> images;QString key=QString::number(m["species_id"].toInt())+(m["shiny"].toBool()?"-shiny":"");
 if(!images.contains(key)){QImage image(resources+"/sprites/"+key+".png");QRect bounds;for(int y=0;y<image.height();y++)for(int x=0;x<image.width();x++)if(qAlpha(image.pixel(x,y)))bounds=bounds.united(QRect(x,y,1,1));images[key]=bounds.isEmpty()?image:image.copy(bounds);}
 auto image=images[key];if(image.isNull())return;QSizeF fitted=image.size();fitted.scale(r.size(),Qt::KeepAspectRatio);p.drawImage(QRectF(r.center()-QPointF(fitted.width()/2,fitted.height()/2),fitted),image);
}
static void typeBadges(QPainter&p,QRectF r,QJsonObject m,int size=9){
 int count=0;std::set<int> seen;for(auto value:m["types"].toArray()){int i=value.toInt();if(i<0||i>=17||!seen.insert(i).second)continue;QRectF badge(r.left(),r.top()+count*(r.height()+3),r.width(),r.height());auto color=typeColor(types[i]);box(p,badge,color.darker(185),color,2);text(p,badge.adjusted(4,0,-4,0),types[i],Qt::white,size,true,Qt::AlignCenter);if(++count==2)break;}
}
static void ball(QPainter&p,QPointF c,QColor color){p.save();p.setRenderHint(QPainter::Antialiasing,false);QRectF r(c.x()-7,c.y()-7,14,14);p.setPen(QPen(QColor("#172431"),1));p.setBrush(QColor("#e5e8e9"));p.drawEllipse(r);p.setClipRect(QRectF(r.x(),r.y(),14,7),Qt::IntersectClip);p.setBrush(color);p.drawEllipse(r);p.setClipping(false);p.setPen(QPen(QColor("#172431"),2));p.drawLine(c+QPointF(-7,0),c+QPointF(7,0));p.setBrush(QColor("#f2f2e9"));p.drawEllipse(c,2.5,2.5);p.restore();}
static void bar(QPainter&p,QRectF r,double fraction,QColor color){fraction=std::clamp(fraction,0.,1.);p.save();p.setRenderHint(QPainter::Antialiasing,false);p.fillRect(r.adjusted(-1,-1,1,1),QColor("#dce3e2"));p.fillRect(r,QColor("#152b33"));if(fraction>0){r.setWidth(std::floor(r.width()*fraction));p.fillRect(r,color.darker(125));p.fillRect(QRectF(r.x(),r.y(),r.width(),std::max(1.,r.height()/2)),color.lighter(135));}p.restore();}
static QString areaCode(){return ledger["areas"].toObject()[QString::number(state["area"].toObject()["name_id"].toInt())].toString("unknown");}
static QJsonArray encounterCards(){
 auto methods=encounters();if(methods.isEmpty())return {};methodSelected=std::clamp(methodSelected,0,int(methods.size())-1);std::map<int,QJsonObject> rows;
 for(auto v:methods[methodSelected].toObject()["slots"].toArray()){auto s=v.toObject();int id=s["species_id"].toInt();auto sp=db["species"].toArray();if(id<=0||id>=sp.size())continue;auto m=rows[id];if(m.isEmpty()){m=sp[id].toObject();m["species_id"]=id;m["chance"]=0;m["min"]=s["min"];m["max"]=s["max"];}m["chance"]=m["chance"].toInt()+s["chance"].toInt();m["min"]=std::min(m["min"].toInt(),s["min"].toInt());m["max"]=std::max(m["max"].toInt(),s["max"].toInt());rows[id]=m;}
 std::set<int> families;auto species=db["species"].toArray();for(auto v:ledger["pokemon"].toObject()){int id=v.toObject()["species_id"].toInt();if(id>0&&id<species.size())families.insert(species[id].toObject()["family"].toInt());}
 std::vector<QJsonObject> ordered;for(auto [id,m]:rows){m["duplicate"]=bool(families.count(m["family"].toInt()));m["blocked"]=m["duplicate"].toBool()||areaCode()=="used"||areaCode()=="lost";ordered.push_back(m);}std::sort(ordered.begin(),ordered.end(),[](auto a,auto b){return a["chance"].toInt()>b["chance"].toInt();});QJsonArray out;for(auto m:ordered)out.append(m);return out;
}
static QJsonObject nextBoss(){auto all=db["bosses"].toArray();int badges=state["badges"].toInt();if(badges<8&&badges<all.size())return all[badges].toObject();auto wins=ledger["league_wins"].toArray();for(int i=8;i<12&&i<all.size();i++){auto boss=all[i].toObject();bool won=false;for(auto v:wins)if(v.toInt()==boss["trainer_id"].toInt())won=true;if(!won&&boss["trainer_id"].toInt()==leagueTarget)return boss;}for(int i=8;i<12&&i<all.size();i++){bool won=false;for(auto v:wins)if(v.toInt()==all[i].toObject()["trainer_id"].toInt())won=true;if(!won)return all[i].toObject();}return all.size()>12?all[12].toObject():QJsonObject{};}
// Match the bitmap renderer's advances when wrapping type summaries.
static double pixelWidth(QString value,int size){
 static QJsonArray fonts=readJson(resources+"/game-font.json")["fonts"].toArray();if(fonts.isEmpty())return value.size()*size*.6;
 auto f=fonts[size<=10?1:0].toObject();auto glyphs=f["glyphs"].toObject();double width=0;value.replace(QChar(0x00b7),".");
 for(QChar c:value){auto g=glyphs[QString::number(c.unicode())].toObject();if(g.isEmpty())g=glyphs["63"].toObject();width+=g["advance"].toInt();}return width*(size+2)/(f["height"].toInt()-2.);
}
static void defenseIcon(QPainter&p,QPointF at,bool danger,QColor color){
 p.save();p.setPen(QPen(color,1.5));p.setBrush(QColor(color.red(),color.green(),color.blue(),35));
 if(danger){p.drawPolygon(QPolygonF{at+QPointF(0,-6),at+QPointF(6,5),at+QPointF(-6,5)});p.drawLine(at+QPointF(0,-2),at+QPointF(0,1));p.drawPoint(at+QPointF(0,3));}
 else{p.drawPolygon(QPolygonF{at+QPointF(-6,-5),at+QPointF(6,-5),at+QPointF(5,2),at+QPointF(0,6),at+QPointF(-5,2)});}p.restore();
}
static void matchups(QPainter&p,QRectF r,QJsonObject m,Theme t){
 struct Item{QString name;double factor;};std::vector<Item> weak,resist;
 for(int i=0;i<17;i++){double factor=effectiveness(i,m);if(factor>1)weak.push_back({types[i]+(factor!=2?QString(" ×%1").arg(factor):QString{}),factor});else if(factor<1)resist.push_back({types[i],factor});}
 box(p,r,t.surface,t.border,3);int size=9;double available=r.width()-31;
 auto rows=[&](const std::vector<Item>& items){int n=1;double x=0;for(auto item:items){double w=pixelWidth(item.name,size)+12+(item.factor==0?12:0);if(x&&x+w>available){n++;x=0;}x+=w+3;}return n;};
 while((rows(weak)+rows(resist))*(size+8)+8>r.height()&&size>7)--size;
 double height=size+6,step=height+2,y=r.top()+4;
 auto group=[&](const std::vector<Item>& items,bool danger){QColor color=danger?QColor("#ffae91"):QColor("#95e5bc");defenseIcon(p,QPointF(r.left()+10,y+height/2),danger,color);double x=r.left()+23;
  if(items.empty()){text(p,QRectF(x,y,available,height),"—",t.muted,size);y+=step;return;}
  for(auto item:items){double width=pixelWidth(item.name,size)+12+(item.factor==0?12:0);if(x>r.left()+23&&x+width>r.right()-6){x=r.left()+23;y+=step;}QColor accent=item.factor==0?QColor("#9cdff6"):color;QRectF badge(x,y,width,height);box(p,badge,accent.darker(item.factor==.25?270:340),accent,2);text(p,badge.adjusted(5,0,item.factor==0?-17:-5,0),item.name,accent,size,true,Qt::AlignCenter);
   if(item.factor==0){QPointF c(badge.right()-8,badge.center().y());p.setPen(QPen(accent,1));p.setBrush(Qt::NoBrush);p.drawEllipse(c,4,4);p.drawLine(c+QPointF(-3,3),c+QPointF(3,-3));}x+=width+3;
  }y+=step;
 };group(weak,true);y+=3;group(resist,false);
}
static void categoryIcon(QPainter&p,QRectF r,QString category){
 p.save();QPointF c=r.center();p.setPen(QPen(category=="Physical"?QColor("#ffd197"):category=="Special"?QColor("#a6deff"):QColor("#dde1e8"),1));p.setBrush(Qt::NoBrush);
 if(category=="Physical"){QPolygonF shape;for(int i=0;i<16;i++){double angle=i*3.141592653589793/8,radius=i%2?3:6;shape<<c+QPointF(std::cos(angle)*radius,std::sin(angle)*radius);}p.drawPolygon(shape);}
 else if(category=="Special"){p.drawEllipse(c,6,4);p.drawEllipse(c,3,2);p.drawPoint(c);}
 else{p.drawEllipse(c,5,5);p.drawLine(c+QPointF(-3,0),c+QPointF(3,0));}p.restore();
}
static void pokemonCard(QPainter&p,QJsonObject m,QJsonObject foe,Theme t){
 if(m.isEmpty())return;box(p,QRectF(1,1,598,222),t.bg,t.border,13);
 pill(p,QRectF(16,5,68,18),"Lv. "+QString::number(m["level"].toInt()),m["level"].toInt()>state["cap"].toInt()?QColor("#f3a49a"):t.accent);
 text(p,QRectF(105,5,272,15),foe.isEmpty()?"SELECTED POKÉMON":"BATTLE · "+(m["pid"]==state["enemy"].toObject()["pid"]?QString("OPPONENT"):QString("YOUR TEAM")),t.accent,10,true);
 box(p,QRectF(8,26,88,82),t.surface.lighter(130),t.border,3);sprite(p,QRectF(12,29,78,58),m);
 text(p,QRectF(105,23,272,26),nickname(m)+(m["shiny"].toBool()?" ★":""),Qt::white,22,true);
 text(p,QRectF(105,49,103,17),m["species"].toString(),t.muted,12);int tx=214;std::set<int> seen;for(auto v:m["types"].toArray()){int i=v.toInt();if(i>=0&&i<17&&seen.insert(i).second){pill(p,QRectF(tx,49,76,18),types[i],typeColor(types[i]));tx+=82;}}
 text(p,QRectF(105,70,272,17),m["ability"].toString()+" · "+m["nature"].toString(),QColor("#e3eaf0"),13);
 text(p,QRectF(105,90,178,17),"Item · "+m["item"].toString(),t.muted,12);text(p,QRectF(285,90,92,17),m["run_state"]=="dead"?"FAINTED":m["status_label"].toString(),m["run_state"]=="dead"?QColor("#ef9399"):t.muted,11,false,Qt::AlignRight|Qt::AlignVCenter);
 int hp=m["hp"].toInt(),maximum=m["max_hp"].toInt();QColor health=hp*4<maximum?QColor("#fa6767"):hp*2<maximum?QColor("#ffd75a"):QColor("#41d990");text(p,QRectF(13,91,78,14),QString("HP %1/%2").arg(hp).arg(maximum),health,10,true,Qt::AlignCenter);bar(p,QRectF(14,105,76,3),double(hp)/std::max(1,maximum),health);
 matchups(p,QRectF(387,7,201,100),m,t);
 auto stats=m["stats"].toArray();QStringList labels={"ATTACK","DEFENSE","SP. ATK","SP. DEF","SPEED"};for(int i=0;i<5;i++){int x=12+i*116;box(p,QRectF(x,111,110,21),t.surface,t.border,6);text(p,QRectF(x+7,111,70,21),labels[i],t.muted,10,true);text(p,QRectF(x+75,111,28,21),QString::number(i<stats.size()?stats[i].toInt():0),Qt::white,14,true,Qt::AlignRight|Qt::AlignVCenter);}
 auto moves=m["moves"].toArray();for(int i=0;i<4;i++){
  auto mv=i<moves.size()?moves[i].toObject():QJsonObject{};int x=12+(i%2)*292,y=136+(i/2)*43;box(p,QRectF(x,y,284,42),t.surface,t.border,6);
  if(mv.isEmpty()){text(p,QRectF(x+9,y+3,266,32),i<moves.size()?"??? · not revealed yet":"—",t.muted,14);continue;}
  auto color=typeColor(mv["type"].toString());p.fillRect(QRectF(x+2,y+5,3,32),color);
  bool stab=false;for(auto v:m["types"].toArray())if(v.toInt()==types.indexOf(mv["type"].toString()))stab=true;stab=stab&&mv["category"]!="Status";int priority=mv["priority"].toInt();QString bonus=stab?"STAB":"";if(priority)bonus+=(bonus.isEmpty()?QString{}:QString(" · "))+QString("Pr.%1").arg(priority);double bonusWidth=priority&&stab?61:priority?39:31;
  text(p,QRectF(x+10,y+1,bonus.isEmpty()?195:196-bonusWidth,18),mv["name"].toString(),Qt::white,15,true);
  if(!bonus.isEmpty()){QRectF badge(x+206-bonusWidth,y+3,bonusWidth,14);QColor gold("#ead38d");box(p,badge,gold.darker(340),gold,2);text(p,badge.adjusted(2,0,-2,0),bonus,gold,8,true,Qt::AlignCenter);}
  text(p,QRectF(x+211,y+1,65,18),QString("PP %1/%2").arg(mv["pp"].toInt()).arg(mv["max_pp"].toInt()),mv["pp"].toInt()?QColor("#e6edf4"):QColor("#ef9399"),12,true,Qt::AlignRight|Qt::AlignVCenter);bar(p,QRectF(x+214,y+20,61,2),double(mv["pp"].toInt())/std::max(1,mv["max_pp"].toInt()),color);
  double factor=foe.isEmpty()||mv["category"]=="Status"?1:effectiveness(types.indexOf(mv["type"].toString()),foe);QColor edge=factor>1?QColor("#99e6b3"):factor<1?QColor("#f1ae9f"):color;QString typeLabel=mv["type"].toString();if(factor!=1)typeLabel+=factor==0?" 0":factor==4?" ×4":factor>1?" ↑":" ↓";
  box(p,QRectF(x+9,y+25,82,14),color.darker(200),edge,2);text(p,QRectF(x+12,y+25,76,14),typeLabel,Qt::white,8,true,Qt::AlignCenter);
  categoryIcon(p,QRectF(x+95,y+25,13,14),mv["category"].toString());text(p,QRectF(x+111,y+25,34,14),mv["category"].toString()=="Special"?"Spec.":mv["category"].toString()=="Physical"?"Phys.":"Status",t.muted,8);
  QString power=mv["power"].toInt()>0?QString::number(mv["power"].toInt()):"—",accuracy=mv["accuracy"].toInt()>100?"∞":QString::number(mv["accuracy"].toInt())+"%";
  text(p,QRectF(x+149,y+25,54,14),"Pwr "+power,t.muted,9);text(p,QRectF(x+209,y+25,66,14),"Acc "+accuracy,t.muted,9,false,Qt::AlignRight|Qt::AlignVCenter);


 }
}
static double teamRow(double height){return std::min(128.,(height-68)/6.);}
static int encounterCapacity(double height){return std::max(1,int((height-112)/86));}
static void sidebarCard(QPainter&p,QRectF r,QJsonObject m,bool team,bool active,Theme t){
 auto fill=active?t.surface.lighter(155):t.surface;auto border=active?QColor("#ffb24a"):t.border;box(p,r,fill,border,3);
 if(active){p.setPen(QPen(border,2));p.setBrush(Qt::NoBrush);p.drawPolygon(menuShape(r.adjusted(-1,-1,1,1),3));}
 double pictureWidth=std::min(82.,r.width()*.43),contentX=r.left()+pictureWidth+11,contentWidth=r.right()-contentX-6;bool compact=r.height()<76;
 sprite(p,QRectF(r.left()+5,r.top()+5,pictureWidth,team?r.height()-31:r.height()-10),m);
 QString name=team?nickname(m)+(m["shiny"].toBool()?" ★":""):m["name"].toString();double nameWidth=contentWidth-(!team&&m["blocked"].toBool()?16:0);
 text(p,QRectF(contentX,r.top()+4,nameWidth,18),name,m["run_state"]=="dead"?QColor("#ef9399"):Qt::white,13,true);
 auto monTypes=m["types"].toArray();std::set<int> unique;for(auto v:monTypes)if(v.toInt()>=0&&v.toInt()<17)unique.insert(v.toInt());
 if(compact){int n=std::max(1,int(unique.size())),i=0;for(int type:unique){QRectF badge(contentX+i*(contentWidth/n),r.top()+23,contentWidth/n-2,12);auto color=typeColor(types[type]);box(p,badge,color.darker(185),color,2);text(p,badge.adjusted(2,0,-2,0),types[type],Qt::white,7,true,Qt::AlignCenter);i++;}}
 else typeBadges(p,QRectF(contentX,r.top()+25,std::min(96.,contentWidth),15),m,9);
 QString meta=team?QString("Lv.%1 · %2/%3 HP").arg(m["level"].toInt()).arg(m["hp"].toInt()).arg(m["max_hp"].toInt()):QString::number(m["chance"].toInt())+"% · "+(m["min"]==m["max"]?QString("Lv.%1").arg(m["min"].toInt()):QString("Lv.%1-%2").arg(m["min"].toInt()).arg(m["max"].toInt()));
 text(p,QRectF(contentX,r.bottom()-(team?24:20),contentWidth,14),meta,t.muted,9);
 if(team){text(p,QRectF(r.left()+5,r.bottom()-23,pictureWidth,14),m["species"].toString(),t.muted,9,false,Qt::AlignCenter);int hp=m["hp"].toInt(),maximum=m["max_hp"].toInt();bar(p,QRectF(r.left()+10,r.bottom()-7,r.width()-20,3),double(hp)/std::max(1,maximum),hp*4<maximum?QColor("#fa6767"):hp*2<maximum?QColor("#ffd75a"):QColor("#41d990"));if(m["pid"]==state["active_pid"]){p.setPen(Qt::NoPen);p.setBrush(t.accent);p.drawPolygon(QPolygonF{QPointF(r.left()+4,r.top()+4),QPointF(r.left()+10,r.top()+8),QPointF(r.left()+4,r.top()+12)});}}
 else if(m["blocked"].toBool())ball(p,QPointF(r.right()-11,r.top()+13),m["duplicate"].toBool()?QColor("#e3c38a"):t.muted);
}
void DrawPanel(QPainter&p,const QRect&rect,PanelKind kind){
 QMutexLocker lock(&mutex);if(!loaded||db.isEmpty()||rect.isEmpty())return;auto t=zoneTheme();p.save();p.setClipRect(rect);p.translate(rect.topLeft());p.setRenderHint(QPainter::Antialiasing,false);p.setRenderHint(QPainter::SmoothPixmapTransform,false);auto party=state["party"].toArray();selected=std::clamp(selected.load(),0,std::max(0,int(party.size())-1));
 if(kind==PokemonCard||kind==AreaCard){p.scale(rect.width()/600.,rect.height()/224.);if(kind==PokemonCard){if(!party.isEmpty())pokemonCard(p,party[selected].toObject(),state["enemy"].toObject(),t);}else{
  auto enemy=state["enemy"].toObject();if(!enemy.isEmpty())pokemonCard(p,enemy,party.isEmpty()?QJsonObject{}:party[selected].toObject(),t);
  else {box(p,QRectF(1,1,598,222),t.bg,t.border,13);text(p,QRectF(16,8,393,29),state["area"].toObject()["name"].toString(),Qt::white,23,true);pill(p,QRectF(420,13,163,24),areaStatus(),areaCode()=="available"?QColor("#8bd8a9"):QColor("#e7c087"));
   auto boss=nextBoss();box(p,QRectF(12,44,576,121),t.surface,t.border,9);text(p,QRectF(23,49,270,18),state["badges"].toInt()<8?"NEXT GYM":"NEXT LEAGUE BATTLE",t.accent,12,true);text(p,QRectF(23,71,388,27),boss["name"].toString()+" · "+boss["place"].toString(),Qt::white,22,true);pill(p,QRectF(425,50,74,24),boss["type"].toString(),typeColor(boss["type"].toString()));pill(p,QRectF(507,50,68,24),"CAP "+QString::number(state["cap"].toInt()),t.accent);
   auto team=boss["team"].toArray();int columns=std::max(1,std::min(3,int(team.size()))),rows=std::max(1,(int(team.size())+columns-1)/columns);double width=548./columns,cellHeight=54./rows;
   for(int i=0;i<team.size();i++){auto m=team[i].toObject();double x=23+(i%columns)*width,y=103+(i/columns)*cellHeight,size=std::min(44.,cellHeight);sprite(p,QRectF(x,y,size,size),m);text(p,QRectF(x+size+3,y,width-size-6,rows==1?22:14),m["name"].toString(),QColor("#edf2f5"),rows==1?14:11,true);text(p,QRectF(x+size+3,y+(rows==1?24:14),width-size-6,rows==1?20:13),"Lv. "+QString::number(m["level"].toInt()),t.muted,rows==1?12:10);}
   int steps=state["repel_steps"].toInt();box(p,QRectF(12,172,276,43),t.surface,t.border,8);text(p,QRectF(23,175,253,19),steps>0?state["repel_item"].toString()+" active":"No Repel active",steps>0?t.accent:t.muted,13,true);text(p,QRectF(23,197,253,15),steps>0?QString("%1 steps left · lead Pokémon Lv.%2").arg(steps).arg(party.isEmpty()?0:party[0].toObject()["level"].toInt()):"Remaining steps update automatically",t.muted,11);
   box(p,QRectF(296,172,292,43),t.surface,t.border,8);text(p,QRectF(307,175,270,19),areaCode()=="used"?"Encounter already used in this area":areaCode()=="lost"?"Encounter lost · area used":areaCode()=="unknown"?"Check this area in the ledger":"Previously obtained families excluded",QColor("#edf2f5"),13,true);text(p,QRectF(307,197,270,15),"Gifts and static encounters use the area",t.muted,11);

  }
 }}else{
  double w=rect.width(),h=rect.height();box(p,QRectF(1,1,w-2,h-2),t.bg,t.border,12);text(p,QRectF(13,11,w-26,24),kind==TeamStrip?"YOUR TEAM":"AREA ENCOUNTERS",t.accent,12,true);
  if(kind==TeamStrip){text(p,QRectF(13,36,w-26,18),"Browse · "+teamKeyLabels,t.muted,10);double row=teamRow(h);for(int i=0;i<party.size();i++)sidebarCard(p,QRectF(10,62+i*row,w-20,row-7),party[i].toObject(),true,i==selected,t);}
  else{auto methods=encounters();auto cards=encounterCards();text(p,QRectF(13,35,w-26,29),methods.isEmpty()?"No random encounters":methods[methodSelected].toObject()["method"].toString()+"  ▾",t.muted,10,true);int capacity=encounterCapacity(h);int pages=std::max(1,int((cards.size()+capacity-1)/capacity));encounterPage=std::clamp(encounterPage,0,pages-1);double row=std::min(118.,(h-112)/std::max(1,std::min(capacity,int(cards.size()))));for(int slot=0;slot<capacity;slot++){int i=encounterPage*capacity+slot;if(i>=cards.size())break;sidebarCard(p,QRectF(10,70+slot*row,w-20,row-5),cards[i].toObject(),false,false,t);}

   text(p,QRectF(13,h-39,w-26,15),pages>1?QString("‹   Page %1 / %2   ›").arg(encounterPage+1).arg(pages):"Poké Ball · encounter excluded",t.muted,10,false,Qt::AlignCenter);text(p,QRectF(13,h-22,w-26,14),"Original odds · shinies exempt",t.muted,9,false,Qt::AlignCenter);
  }
 }p.restore();
}
void Draw(QPainter&p,const QRect&left,const QRect&right){DrawPanel(p,left,PokemonCard);DrawPanel(p,right,AreaCard);}
bool ClickPanel(const QPoint&pos,const QRect&r,PanelKind kind,QWidget*parent){
 if(!r.contains(pos))return false;
 if(kind==TeamStrip){auto snap=State();double row=teamRow(r.height());int i=int((pos.y()-r.y()-62)/row);if(pos.y()>=r.y()+62)SelectPokemon(i);return true;}
 if(kind==EncounterStrip){QMutexLocker lock(&mutex);auto methods=encounters();if(pos.y()-r.y()<67&&methods.size()>1){methodSelected=(methodSelected+1)%methods.size();encounterPage=0;return true;}if(pos.y()-r.y()>r.height()-43){int capacity=encounterCapacity(r.height());int pages=std::max(1,int((encounterCards().size()+capacity-1)/capacity));encounterPage=(encounterPage+(pos.x()-r.x()<r.width()/2?-1:1)+pages)%pages;return true;}lock.unlock();return Click(pos,QRect(-10000,-10000,1,1),r,parent);}
 return Click(pos,kind==PokemonCard?r:QRect(-10000,-10000,1,1),kind==AreaCard?r:QRect(-10000,-10000,1,1),parent);
}
