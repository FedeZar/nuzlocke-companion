#include "Gdb.h"
#include "Nuzlocke.h"
#include <QTcpSocket>
#include <QElapsedTimer>
#include <QSet>
#include <vector>
#include <stdexcept>
#include <cstring>
namespace {
uint32_t word(const QByteArray& b,int offset){if(offset<0||offset+4>b.size())return 0;const auto p=(const unsigned char*)b.constData()+offset;return p[0]|p[1]<<8|p[2]<<16|uint32_t(p[3])<<24;}
bool valid(uint32_t a,int size){return a>=0x02000000 && uint64_t(a)+size<=0x02400000;}
class Link {
 public:
 QTcpSocket socket;QByteArray incoming;
 void ack(){QElapsedTimer clock;clock.start();for(;;){if(!incoming.isEmpty()){char c=incoming[0];incoming.remove(0,1);if(c=='+')return;if(c=='-')throw std::runtime_error("Request rejected by melonDS");throw std::runtime_error("Unexpected handshake response");}if(clock.elapsed()>1500||socket.state()!=QAbstractSocket::ConnectedState)throw std::runtime_error("melonDS closed the initial connection");if(socket.bytesAvailable()||socket.waitForReadyRead(100))incoming+=socket.readAll();}}
 void connect(){socket.connectToHost("127.0.0.1",3333);if(!socket.waitForConnected(750))throw std::runtime_error("TCP connection unavailable");socket.setSocketOption(QAbstractSocket::LowDelayOption,1);socket.write("+");socket.waitForBytesWritten(500);ack();packet("c");ack();}
 void packet(const QByteArray& text){unsigned sum=0;for(unsigned char c:text)sum+=c;socket.write("$"+text+"#"+QByteArray::number(sum&255,16).rightJustified(2,'0'));if(socket.bytesToWrite()&&!socket.waitForBytesWritten(500))throw std::runtime_error("write");}
 QByteArray reply(){QElapsedTimer clock;clock.start();for(;;){int start=incoming.indexOf('$');if(start>=0){int hash=incoming.indexOf('#',start+1);if(hash>=0&&incoming.size()>=hash+3){auto payload=incoming.mid(start+1,hash-start-1);bool ok;unsigned expected=incoming.mid(hash+1,2).toUInt(&ok,16),sum=0;for(unsigned char c:payload)sum+=c;incoming.remove(0,hash+3);if(!ok||(sum&255)!=expected){socket.write("-");continue;}socket.write("+");socket.flush();if(payload.startsWith('S')||payload.startsWith('T'))continue;return payload;}}if(clock.elapsed()>1500||socket.state()!=QAbstractSocket::ConnectedState)throw std::runtime_error("No response while reading game memory");if(socket.bytesAvailable()||socket.waitForReadyRead(150))incoming+=socket.readAll();}}
 QByteArray read(uint32_t address,int size){QByteArray data;for(int offset=0;offset<size;offset+=512){int length=std::min(512,size-offset);packet("m"+QByteArray::number(address+offset,16)+","+QByteArray::number(length,16));auto hex=reply();if(hex.size()!=length*2)throw std::runtime_error("Incomplete memory response");auto part=QByteArray::fromHex(hex);if(part.size()!=length)throw std::runtime_error("hex");data+=part;}return data;}
 void detach(){if(socket.state()==QAbstractSocket::ConnectedState){packet("D");reply();socket.flush();socket.disconnectFromHost();socket.waitForDisconnected(300);}}
};
}
void Reader::run(){
 while(!isInterruptionRequested()) {
  Link link;
  try{
   link.connect();QByteArray ram(0x400000,0);int box=0;
   auto fetch=[&](uint32_t address,int size){if(!valid(address,size))return false;auto data=link.read(address,size);std::memcpy(ram.data()+address-0x02000000,data.constData(),size);return true;};
   auto get=[&](uint32_t a){return valid(a,4)?word(ram,a-0x02000000):0u;};
   uint32_t previousSetup=0;
   while(!isInterruptionRequested()) {
    QElapsedTimer elapsed;elapsed.start();fetch(0x023ffe00,32);
    if(ram.mid(0x3ffe0c,4)!="IRDI"){Nuzlocke::Disconnected("Start Pokémon White 2, Italian edition (IRDI).");msleep(750);continue;}
    fetch(0x0221e328,4+6*220);fetch(0x0221eaa4,4);fetch(0x02226628,4);fetch(0x02226e50,4);fetch(0x02141034,4);
    uint32_t gs=get(0x02141034),mgr=0,rootProc=0;
    // GameSystem owns the current Field. The save spawn zone is only a fallback.
    uint32_t field=0;
    if(fetch(gs,0x24)){mgr=get(gs+8);if(fetch(mgr,32))rootProc=get(mgr+8);field=get(gs+0x20);fetch(field,0xf0);}
    std::vector<uint32_t> queue{rootProc};QSet<uint32_t> visited;
    for(size_t p=0;p<queue.size()&&p<16;p++){
     auto proc=queue[p];if(visited.contains(proc)||!fetch(proc,32))continue;visited.insert(proc);queue.push_back(get(proc+24));queue.push_back(get(proc+28));uint32_t module=get(proc+20);
     // Read the small header before fetching a candidate battle structure.
     if(!fetch(module,16))continue;uint32_t setup=get(module);if(!fetch(setup,0xb0)||get(setup)>4||get(setup+4)>3)continue;
     if(!fetch(module+0xc8,0xe8)||get(module+0xc8)!=module)continue;
     fetch(module+0x428,8);fetch(module+8,4);fetch(get(module+8),0x50);fetch(get(setup+0x4c),40);previousSetup=setup;
     QSet<uint32_t> combatants;
     for(int c=0;c<4;c++){uint32_t bp=module+0xcc+c*28;int count=(unsigned char)ram[bp-0x02000000+24];if(count>6)continue;for(int i=0;i<count;i++)combatants.insert(get(bp+4*i));}
     for(int i=0;i<24;i++)combatants.insert(get(module+0x14c+4*i));
     for(auto bm:combatants){if(!fetch(bm,0x1f8))continue;fetch(get(bm),220);}
    }
    if(previousSetup)fetch(previousSetup,0xb0);
    // Spread box reads across updates, avoiding a large blocking dump.
    fetch(0x02205924+box*0x1000,30*136);bool boxesReady=++box==24;if(boxesReady)box=0;
    // If a process transition occurred during reads, wait for the next sample.
    auto check=valid(mgr,32)?link.read(mgr+8,4):QByteArray{};
    auto fieldCheck=valid(gs,0x24)?link.read(gs+0x20,4):QByteArray{};
    if((check.isEmpty()||word(check,0)==rootProc)&&(fieldCheck.isEmpty()||word(fieldCheck,0)==field))Nuzlocke::AcceptRAM(ram,boxesReady);
    int rest=250-int(elapsed.elapsed());if(rest>0)msleep(rest);
   }
   link.detach();
  }catch(const std::exception& error){
   link.socket.abort();
   QString reason=QString::fromUtf8(error.what());Nuzlocke::Disconnected(reason=="TCP connection unavailable"?"Waiting for melonDS. Open White 2 and continue your game. Check Connection help if it is already running.":QString("Connection interrupted: %1. Save in-game and restart melonDS if needed. Enable GDB ARM9 port 3333 and disable JIT.").arg(reason));
   for(int i=0;i<10&&!isInterruptionRequested();i++)msleep(100);
  }
 }
}
