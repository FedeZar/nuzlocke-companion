#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <Carbon/Carbon.h>
#include "Mac.h"
static QString windowStatus;
QString GameWindowStatus(){return windowStatus;}
bool GameIsFrontmost(){return [NSWorkspace.sharedWorkspace.frontmostApplication.bundleIdentifier isEqualToString:@"net.kuribo64.melonDS"];}
bool GameWindow(QRect& rect) {
 @autoreleasepool {
  NSRunningApplication* front=NSWorkspace.sharedWorkspace.frontmostApplication;
  QString frontId=QString::fromUtf8(front.bundleIdentifier.UTF8String?:"");
  if(frontId!="net.kuribo64.melonDS" && frontId!="io.github.fedezar.nuzlocke-companion"){windowStatus="Bring melonDS to the front to see the overlay.";return false;}
  pid_t pid=0;
  for(NSRunningApplication* app in NSWorkspace.sharedWorkspace.runningApplications)
   if([app.bundleIdentifier isEqualToString:@"net.kuribo64.melonDS"]){pid=app.processIdentifier;break;}
  if(!pid){windowStatus="Open melonDS to see the overlay.";return false;}
  NSArray* list=CFBridgingRelease(CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly|kCGWindowListExcludeDesktopElements,kCGNullWindowID));
  for(NSDictionary* entry in list) {
   if([entry[(id)kCGWindowOwnerPID] intValue]!=pid || [entry[(id)kCGWindowLayer] intValue]!=0)continue;
   CGRect bounds; if(!CGRectMakeWithDictionaryRepresentation((__bridge CFDictionaryRef)entry[(id)kCGWindowBounds],&bounds))continue;
   if(bounds.size.width<400||bounds.size.height<350)continue;
   rect=QRect(qRound(bounds.origin.x),qRound(bounds.origin.y),qRound(bounds.size.width),qRound(bounds.size.height));windowStatus=QString("melonDS window: %1 × %2").arg(rect.width()).arg(rect.height());return true;
  }
  windowStatus="melonDS window is not visible: bring it to the front and enlarge it.";return false;
 }
}
namespace {
EventHotKeyRef keys[2]={nullptr,nullptr};EventHandlerRef keyHandler=nullptr;
std::function<void(int)> cycle;bool keyEnabled=false;QKeySequence sequences[2];
struct KeySpec{UInt32 code=0,modifiers=0;bool valid=false;};
KeySpec translateKey(QKeySequence sequence){
 if(sequence.isEmpty())return {};auto k=sequence[0];UInt32 modifiers=0;
 if(k.keyboardModifiers()&Qt::ControlModifier)modifiers|=cmdKey;
 if(k.keyboardModifiers()&Qt::MetaModifier)modifiers|=controlKey;
 if(k.keyboardModifiers()&Qt::AltModifier)modifiers|=optionKey;
 if(k.keyboardModifiers()&Qt::ShiftModifier)modifiers|=shiftKey;
 switch(k.key()){case Qt::Key_PageUp:return {kVK_PageUp,modifiers,true};case Qt::Key_PageDown:return {kVK_PageDown,modifiers,true};case Qt::Key_Left:return {kVK_LeftArrow,modifiers,true};case Qt::Key_Right:return {kVK_RightArrow,modifiers,true};case Qt::Key_Up:return {kVK_UpArrow,modifiers,true};case Qt::Key_Down:return {kVK_DownArrow,modifiers,true};default:break;}
 if(k.key()>=Qt::Key_F1&&k.key()<=Qt::Key_F12){UInt32 codes[]={kVK_F1,kVK_F2,kVK_F3,kVK_F4,kVK_F5,kVK_F6,kVK_F7,kVK_F8,kVK_F9,kVK_F10,kVK_F11,kVK_F12};return {codes[k.key()-Qt::Key_F1],modifiers,true};}
 if(k.key()<32||k.key()>0xffff)return {};UniChar desired=QChar(int(k.key())).toLower().unicode();
 TISInputSourceRef source=TISCopyCurrentKeyboardLayoutInputSource();if(!source)return {};
 CFDataRef data=(CFDataRef)TISGetInputSourceProperty(source,kTISPropertyUnicodeKeyLayoutData);KeySpec out;
 if(data){auto layout=(const UCKeyboardLayout*)CFDataGetBytePtr(data);UInt32 possibilities[]={0,shiftKey,optionKey,shiftKey|optionKey};for(UInt32 extra:possibilities){for(UInt16 code=0;code<128;code++){UInt32 dead=0;UniChar chars[4]={};UniCharCount length=0;auto status=UCKeyTranslate(layout,code,kUCKeyActionDown,((modifiers|extra)&(shiftKey|optionKey))>>8,LMGetKbdType(),kUCKeyTranslateNoDeadKeysBit,&dead,4,&length,chars);if(status==noErr&&length==1&&QChar(chars[0]).toLower().unicode()==desired){out={code,modifiers|extra,true};break;}}if(out.valid)break;}}
 CFRelease(source);return out;
}
OSStatus teamKeyHandler(EventHandlerCallRef,EventRef event,void*){EventHotKeyID id;GetEventParameter(event,kEventParamDirectObject,typeEventHotKeyID,nullptr,sizeof(id),nullptr,&id);if(GameIsFrontmost()&&cycle&&id.signature=='NZLK')cycle(id.id==1?-1:1);return noErr;}
void removeKeys(){for(auto& key:keys)if(key){UnregisterEventHotKey(key);key=nullptr;}}
QString registerKeys(){removeKeys();for(int i=0;i<2;i++){auto spec=translateKey(sequences[i]);if(!spec.valid){removeKeys();return "Unsupported key. Choose a letter, [ / ], an arrow or a function key.";}EventHotKeyID id={'NZLK',UInt32(i+1)};if(RegisterEventHotKey(spec.code,spec.modifiers,id,GetApplicationEventTarget(),0,&keys[i])!=noErr){removeKeys();return "A key is already used by macOS or another app. Choose a different shortcut.";}}return {};}
}
QString ConfigureTeamKeys(QKeySequence previous,QKeySequence next,std::function<void(int)> action){
 if(previous.isEmpty()||next.isEmpty()||previous==next)return "Choose two different shortcuts.";
 auto a=translateKey(previous),b=translateKey(next);if(!a.valid||!b.valid)return "Shortcut not supported by the current keyboard layout.";
 if(a.code==b.code&&a.modifiers==b.modifiers)return "Both shortcuts map to the same key.";
 if(!keyHandler){EventTypeSpec type={kEventClassKeyboard,kEventHotKeyPressed};InstallApplicationEventHandler(teamKeyHandler,1,&type,nullptr,&keyHandler);}
 auto old0=sequences[0],old1=sequences[1];sequences[0]=previous;sequences[1]=next;cycle=action;QString error=registerKeys();if(!error.isEmpty()){sequences[0]=old0;sequences[1]=old1;if(keyEnabled)registerKeys();}else if(!keyEnabled)removeKeys();return error;
}
void EnableTeamKeys(bool enabled){if(enabled==keyEnabled)return;keyEnabled=enabled;if(enabled)registerKeys();else removeKeys();}
void FloatOnGame(WId handle) {
 @autoreleasepool {
  NSView* view=(__bridge NSView*)(void*)handle;NSWindow* window=view.window;
  // A non-activating panel may accompany another application's fullscreen Space.
  // Keep it above fullscreen content without taking keyboard focus from melonDS.
  if([window isKindOfClass:NSPanel.class])window.styleMask|=NSWindowStyleMaskNonactivatingPanel;
  window.level=NSStatusWindowLevel;window.hidesOnDeactivate=NO;
  window.collectionBehavior=NSWindowCollectionBehaviorCanJoinAllSpaces|NSWindowCollectionBehaviorFullScreenAuxiliary|NSWindowCollectionBehaviorCanJoinAllApplications|NSWindowCollectionBehaviorStationary;
  if(window.visible)[window orderFrontRegardless];
 }
}
