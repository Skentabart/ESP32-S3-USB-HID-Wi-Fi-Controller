/*
  ESP32-S3 USB HID Wi-Fi Controller
  ==================================

  Hardware:
    ESP32-S3 Super Mini

  USB:
    Composite HID:
      - Mouse
      - Keyboard

  Wi-Fi:
    - Access Point
    - Captive Portal
    - Mobile Web UI

  Features:
    - Touchpad
    - LMB / RMB
    - Mouse wheel
    - Virtual keyboard
    - EN / RU keyboard
    - Shift+Alt language switching
    - Sticky CTRL / SHIFT / ALT / WIN
    - Arbitrary key combinations
    - Macro editor
    - Macro action editor
    - Flash storage
    - Quick actions
    - No pull-to-refresh
    - No page scrolling during mouse control
    - Vertical auto-returning continuous scroll slider
    - Smartphone-style keyboard with large Space / Enter / Backspace

  Arduino IDE:
    Board:
      ESP32S3 Dev Module

    USB Mode:
      USB-OTG (TinyUSB)

    USB CDC On Boot:
      Disabled

  IMPORTANT:
    Native USB must be used.
*/

#include <Arduino.h>
#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDMouse.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>

// ============================================================
// USB
// ============================================================

USBHIDKeyboard Keyboard;
USBHIDMouse Mouse;

// ============================================================
// NETWORK
// ============================================================

WebServer server(80);
DNSServer dnsServer;

IPAddress apIP(192, 168, 4, 1);
IPAddress apGateway(192, 168, 4, 1);
IPAddress apSubnet(255, 255, 255, 0);

String apSSID = "ESP32-HID";
String apPassword = "12345678";

// ============================================================
// STORAGE
// ============================================================

Preferences prefs;

// ============================================================
// SETTINGS
// ============================================================

int mouseSensitivity = 2;

bool russianMode = false;

bool heldCtrl  = false;
bool heldShift = false;
bool heldAlt   = false;
bool heldWin   = false;

// ============================================================
// MACROS
// ============================================================

#define MAX_MACROS 50
#define MACRO_NAME_LEN 40
#define MACRO_DATA_LEN 1500

struct Macro {
  String name;
  String data;
};

Macro macros[MAX_MACROS];

int macroCount = 0;


// ============================================================
// NON-BLOCKING MACRO ENGINE
// ============================================================

bool macroRunning = false;
String macroBuffer = "";
size_t macroPos = 0;
unsigned long macroWaitUntil = 0;


// ============================================================
// SYSTEM MACROS
// ============================================================



// ============================================================
// FORWARD DECLARATIONS
// ============================================================

void handleRoot();
void handleMove();
void handleMouseDown();
void handleMouseUp();
void handleScroll();
void handleLanguage();
void handleModifiers();
void handleClearModifiers();
void handleVirtualKey();
void handleCombo();
void handleClipboard();
void handleType();
void handleSettings();
void handleSensitivity();
void handleWiFi();
void handleMacros();
void handleMacroGet();
void handleMacroSave();
void handleMacroDelete();
void handleMacroRun();
void handleMacroStop();
void handleReset();
void captivePortal();
void startWiFi();
void startServer();
void startMacroExecution(int id);
void stopMacroExecution();
void processMacroStep();
void executeAction(String action);
void saveMacros();
void loadMacros();
void createDefaultMacros();
void releaseAllModifiers();
void switchLanguagePC();
uint8_t usageForKey(String k);
uint8_t russianUsage(String k);

// ============================================================
// HTML
// ============================================================

const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no,viewport-fit=cover">
<meta name="mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="theme-color" content="#0b0e12">
<title>ESP32 HID</title>
<style>
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent;user-select:none}
html,body{width:100%;height:100%;margin:0;padding:0;overflow:hidden;background:#080b0e;color:#fff;overscroll-behavior:none;touch-action:none;font-family:Arial,Helvetica,sans-serif}
body{position:fixed;left:0;top:0;right:0;bottom:0}
button{border:0;color:#fff;background:#20262d;border-radius:12px;min-height:46px;padding:8px 12px;font-size:14px;font-weight:600;touch-action:manipulation}
button:active{transform:scale(.97)}
button.active{background:#0b6f9f;box-shadow:0 0 0 2px #24b9ff inset}
button.green{background:#205b43}
button.red{background:#632d2d}
button.blue{background:#1d506d}
button.mic{background:#6e1d50}
button.mic.listening{background:#b02854;animation:pulse 1s infinite}
@keyframes pulse{0%,100%{box-shadow:0 0 0 0 rgba(176,40,84,.7)}50%{box-shadow:0 0 0 8px rgba(176,40,84,0)}}
#app{width:100%;height:100dvh;display:flex;flex-direction:column;overflow:hidden}
#top{height:58px;flex-shrink:0;display:flex;align-items:center;gap:5px;padding:5px;background:#11161b;border-bottom:1px solid #293039}
.topButton{min-width:47px;min-height:46px;padding:5px 7px;font-size:12px}
#status{flex:1;min-width:0;text-align:center;color:#8995a1;font-size:12px;overflow:hidden;white-space:nowrap}
#touchpadContainer{flex:1;min-height:0;display:flex;position:relative;overflow:hidden;touch-action:none}
#touchpad{flex:1;position:relative;overflow:hidden;touch-action:none;background:radial-gradient(circle at center,#151c22 0,#0c1014 75%)}
#touchpad::after{content:"TOUCHPAD";position:absolute;left:50%;top:50%;transform:translate(-50%,-50%);color:#252e36;font-size:13px;letter-spacing:4px;pointer-events:none}
#scrollTrack{width:28px;background:#11161b;border-left:1px solid #293039;position:relative;display:flex;align-items:center;justify-content:center;touch-action:none}
#scrollThumb{width:20px;height:60px;background:#20262d;border:1px solid #3a4552;border-radius:10px;position:absolute;top:50%;transform:translateY(-50%);touch-action:none;transition:top .15s ease-out}
#scrollThumb.active{background:#0b6f9f;border-color:#24b9ff;transition:none}
#mouseButtons{height:100px;flex-shrink:0;display:grid;grid-template-columns:1fr 1fr;gap:6px;padding:6px;background:#11161b}
.mouseButton{height:100%;min-height:88px;border-radius:20px;font-size:20px;font-weight:700}
.mouseLeft{background:#1b3d51}
.mouseRight{background:#513127}
#keyboard{position:absolute;z-index:100;left:0;right:0;bottom:0;max-height:75dvh;padding:6px;overflow:auto;background:#0e1318;border-top:1px solid #303943;transform:translateY(105%);transition:transform .15s ease;touch-action:pan-y;overscroll-behavior:contain}
#keyboard.open{transform:translateY(0)}
.keyRow{display:flex;gap:3px;margin-bottom:3px}
.key{flex:1;min-width:0;min-height:44px;padding:3px;font-size:13px;border-radius:8px}
.key.pressed{transform:scale(1.055);box-shadow:0 0 0 2px #24b9ff inset,0 2px 10px rgba(36,185,255,.28);position:relative;z-index:5;transition:transform .06s ease,box-shadow .06s ease}
.key.modifier{background:#303944}
.key.symbol{background:#1a2430;font-size:15px}
.key.backspace{background:#632d2d}
.key.enter{background:#205b43;flex:2}
.key.space{flex:5;font-size:11px;color:#aeb8c2}
.key.func{background:#1d506d;font-size:12px}
.key.micKey{background:#6e1d50}
.key.micKey.listening{background:#b02854;animation:pulse 1s infinite}
#keyboardBar{display:flex;gap:5px;margin-bottom:6px}
#keyboardBar button{flex:1}
#keyboardTitle{flex:2;display:flex;align-items:center;justify-content:center;color:#aeb8c2}
#panel{display:none;position:absolute;z-index:200;inset:0;background:#090c10;flex-direction:column;overflow:hidden;touch-action:auto}
#panelHeader{height:57px;flex-shrink:0;display:flex;align-items:center;gap:7px;padding:5px;background:#11161b;border-bottom:1px solid #29313a}
#panelTitle{flex:1;font-weight:bold}
#panelBody{flex:1;overflow:auto;padding:9px;touch-action:pan-y;overscroll-behavior:contain}
.card{background:#141a20;border:1px solid #282f37;border-radius:14px;padding:10px;margin-bottom:9px}
.cardTitle{font-size:16px;font-weight:bold;margin-bottom:8px}
.grid{display:grid;grid-template-columns:repeat(2,1fr);gap:6px}
input,textarea,select{width:100%;border:1px solid #39434d;border-radius:10px;background:#080b0e;color:#fff;padding:10px;font-size:15px;user-select:text}
label{display:block;color:#8f9ba7;font-size:12px;margin:7px 0 4px}
.small{color:#788591;font-size:11px;line-height:1.45}
.macroRow{display:flex;gap:5px;margin-bottom:6px}
.macroRun{flex:1;text-align:left}
.actionItem{background:#1b2229;border:1px solid #2e3841;border-radius:10px;padding:8px;margin-bottom:5px}
.actionHeader{display:flex;align-items:center;gap:5px}
.actionHeader span{flex:1}
.actionButtons{display:flex;gap:3px}
.actionButtons button{min-height:36px;min-width:38px;padding:3px}
#lang{min-width:53px}
.voiceStatus{position:fixed;top:70px;left:50%;transform:translateX(-50%);background:#b02854;color:#fff;padding:10px 20px;border-radius:20px;z-index:300;display:none;font-size:13px}
.voiceStatus.active{display:block;animation:pulse 1.5s infinite}
</style>
</head>
<body>
<div id="app">
<div id="top">
<button class="topButton" onclick="clipboardAction('copy')">COPY</button>
<button class="topButton" onclick="clipboardAction('paste')">PASTE</button>
<div id="status">ESP32 HID</div>
<button id="lang" class="topButton" onclick="toggleLanguage()">EN</button>
<button class="topButton" onclick="openMacros()">MAC</button>
<button class="topButton" onclick="openSettings()">⚙</button>
<button class="topButton" onclick="toggleKeyboard()">⌨</button>
</div>
<div id="touchpadContainer">
<div id="touchpad"></div>
<div id="scrollTrack"><div id="scrollThumb"></div></div>
</div>
<div id="mouseButtons">
<button class="mouseButton mouseLeft" onpointerdown="mouseDown(1)" onpointerup="mouseUp(1)" onpointercancel="mouseUp(1)">LEFT</button>
<button class="mouseButton mouseRight" onpointerdown="mouseDown(2)" onpointerup="mouseUp(2)" onpointercancel="mouseUp(2)">RIGHT</button>
</div>
</div>
<div id="keyboard">
<div id="keyboardBar">
<button onclick="clearModifiers()">CLR</button>
<div id="keyboardTitle">KEYBOARD</div>
<button onclick="toggleLanguage()" id="keyboardLang">EN</button>
<button onclick="toggleKeyboard()">✕</button>
</div>
<div id="keys"></div>
</div>
<div id="panel">
<div id="panelHeader">
<button onclick="closePanel()">←</button>
<div id="panelTitle">PANEL</div>
</div>
<div id="panelBody"></div>
</div>
<div id="voiceStatus" class="voiceStatus">🎤 Говорите...</div>
<script>
let language='EN';
let keyboardLayer='letters';
let modifiers={CTRL:false,SHIFT:false,ALT:false,WIN:false};
let pointerActive=false,lastX=0,lastY=0,moveQueueX=0,moveQueueY=0,lastMoveSend=0;
let recognition=null,voiceListening=false;
let longPressTimer=null,longPressInterval=null;

let voiceFinalBuffer = '';

function voiceNormalize(text){
  let t = String(text || '').trim();
  if(!t) return '';

  if(language === 'RU'){
    const replacements = [
      [/точка с запятой/gi, ';'],
      [/новая строка/gi, '\n'],
      [/перенос строки/gi, '\n'],
      [/пробел/gi, ' '],
      [/табуляция/gi, '\t'],
      [/ввод/gi, '\n'],
      [/\bтаб\b/gi, '\t'],
      [/двоеточие/gi, ':'],
      [/точка/gi, '.'],
      [/запятая/gi, ','],
      [/вопросительный знак/gi, '?'],
      [/восклицательный знак/gi, '!']
    ];
    replacements.forEach(([re,v]) => t=t.replace(re,v));
  } else {
    const replacements = [
      [/semicolon/gi, ';'],
      [/new paragraph/gi, '\n\n'],
      [/new line/gi, '\n'],
      [/\bspace\b/gi, ' '],
      [/\btab\b/gi, '\t'],
      [/\benter\b/gi, '\n'],
      [/colon/gi, ':'],
      [/period/gi, '.'],
      [/comma/gi, ','],
      [/question mark/gi, '?'],
      [/exclamation mark/gi, '!']
    ];
    replacements.forEach(([re,v]) => t=t.replace(re,v));
  }

  return t;
}

function voiceSetStatus(text){
  const el=document.getElementById('voiceStatus');
  if(!el) return;
  el.innerText=text ? '🎤 ' + text : '🎤 Говорите...';
}

async function sendVoiceText(text){
  const normalized=voiceNormalize(text);
  if(!normalized) return;

  try{
    await fetch('/api/type',{
      method:'POST',
      headers:{
        'Content-Type':
          'application/x-www-form-urlencoded;charset=UTF-8'
      },
      body:
        'text='+encodeURIComponent(normalized)+
        '&lang='+encodeURIComponent(language)
    });
  }catch(e){
    console.log('voice send error',e);
  }
}

function initVoice(){
  const SR=window.SpeechRecognition||window.webkitSpeechRecognition;

  if(!SR)
    return false;

  recognition=new SR();

  recognition.continuous=true;
  recognition.interimResults=true;
  recognition.maxAlternatives=1;

  recognition.onstart=()=>{
    voiceListening=true;
    voiceFinalBuffer='';
    document.getElementById('voiceStatus').classList.add('active');
    voiceSetStatus('');
    document.querySelectorAll('.micKey').forEach(
      b=>b.classList.add('listening')
    );
  };

  recognition.onresult=async(e)=>{
    let interim='';

    for(
      let i=e.resultIndex;
      i<e.results.length;
      i++
    ){

      const text=
        e.results[i][0].transcript;

      if(e.results[i].isFinal){

        voiceFinalBuffer +=
          (voiceFinalBuffer ? ' ' : '') +
          text;

        await sendVoiceText(text);

      }else{

        interim += text;

      }

    }

    voiceSetStatus(
      interim || 'Говорите...'
    );

  };

  recognition.onerror=(e)=>{

    if(
      e.error === 'not-allowed' ||
      e.error === 'service-not-allowed'
    ){

      voiceListening=false;

      document
        .getElementById('voiceStatus')
        .classList.remove('active');

      document
        .querySelectorAll('.micKey')
        .forEach(
          b=>b.classList.remove('listening')
        );

    }

  };

  recognition.onend=()=>{

    if(voiceListening){

      try{

        recognition.lang =
          language === 'RU'
            ? 'ru-RU'
            : 'en-US';

        recognition.start();

        return;

      }catch(e){}

    }

    voiceListening=false;

    document
      .getElementById('voiceStatus')
      .classList.remove('active');

    document
      .querySelectorAll('.micKey')
      .forEach(
        b=>b.classList.remove('listening')
      );

  };

  return true;
}

function toggleVoice(){

  if(!recognition){

    if(!initVoice()){

      alert(
        'Голосовой ввод не поддерживается этим браузером.'
      );

      return;

    }

  }

  if(voiceListening){

    voiceListening=false;

    try{
      recognition.stop();
    }catch(e){}

  }else{

    voiceFinalBuffer='';

    recognition.lang =
      language === 'RU'
        ? 'ru-RU'
        : 'en-US';

    try{
      recognition.start();
    }catch(e){}

  }

}

async function toggleLanguage(){
await fetch('/api/language');
language=(language==='EN')?'RU':'EN';
updateLanguageUI();renderKeyboard();
}
function updateLanguageUI(){
document.getElementById('lang').innerText=language;
document.getElementById('keyboardLang').innerText=language;
}

const touchpad=document.getElementById('touchpad');
touchpad.addEventListener('pointerdown',function(e){e.preventDefault();pointerActive=true;lastX=e.clientX;lastY=e.clientY;touchpad.setPointerCapture(e.pointerId);},{passive:false});
touchpad.addEventListener('pointermove',function(e){e.preventDefault();if(!pointerActive)return;const dx=e.clientX-lastX,dy=e.clientY-lastY;lastX=e.clientX;lastY=e.clientY;moveQueueX+=dx;moveQueueY+=dy;sendMouseMove();},{passive:false});
touchpad.addEventListener('pointerup',function(e){e.preventDefault();pointerActive=false;},{passive:false});
touchpad.addEventListener('pointercancel',function(){pointerActive=false;},{passive:false});
document.addEventListener('touchmove',function(e){if(e.target===touchpad||touchpad.contains(e.target)||e.target===scrollTrack||scrollTrack.contains(e.target))e.preventDefault();},{passive:false});
['gesturestart','gesturechange','gestureend'].forEach(ev=>document.addEventListener(ev,e=>e.preventDefault(),{passive:false}));

const scrollTrack =
  document.getElementById('scrollTrack');

const scrollThumb =
  document.getElementById('scrollThumb');

let scrollActive = false;
let scrollTimer = null;
let scrollOffset = 0;
let scrollLastY = 0;

function clamp(v,min,max){
  return Math.max(
    min,
    Math.min(
      max,
      v
    )
  );
}

function updateScrollThumb(){

  scrollThumb.style.transition =
    'none';

  scrollThumb.style.top =
    'calc(50% + ' +
    scrollOffset +
    'px)';

}

function scrollDown(e){

  e.preventDefault();

  scrollActive = true;

  scrollLastY =
    e.clientY;

  const rect =
    scrollTrack.getBoundingClientRect();

  const limit =
    Math.max(
      20,
      rect.height / 2 - 45
    );

  const center =
    rect.top +
    rect.height / 2;

  scrollOffset =
    clamp(
      e.clientY - center,
      -limit,
      limit
    );

  scrollThumb.classList.add(
    'active'
  );

  updateScrollThumb();

  try{
    scrollTrack.setPointerCapture(
      e.pointerId
    );
  }catch(err){}

  startScrollTimer();

}

function scrollMove(e){

  if(!scrollActive)
    return;

  e.preventDefault();

  const rect =
    scrollTrack.getBoundingClientRect();

  const limit =
    Math.max(
      20,
      rect.height / 2 - 45
    );

  const dy =
    e.clientY -
    scrollLastY;

  scrollLastY =
    e.clientY;

  scrollOffset =
    clamp(
      scrollOffset + dy,
      -limit,
      limit
    );

  updateScrollThumb();

}

function scrollUp(e){

  if(!scrollActive)
    return;

  e.preventDefault();

  scrollActive = false;

  stopScrollTimer();

  try{
    scrollTrack.releasePointerCapture(
      e.pointerId
    );
  }catch(err){}

  scrollOffset = 0;

  scrollThumb.classList.remove(
    'active'
  );

  scrollThumb.style.transition =
    'top .16s ease-out';

  scrollThumb.style.top =
    '50%';

}

function startScrollTimer(){

  stopScrollTimer();

  scrollTimer =
    setInterval(
      function(){

        if(!scrollActive)
          return;

        if(
          Math.abs(scrollOffset) < 2
        )
          return;

        let amount =
          -Math.round(
            scrollOffset / 18
          );

        if(amount === 0)
          amount =
            scrollOffset < 0
              ? 1
              : -1;

        amount =
          clamp(
            amount,
            -10,
            10
          );

        fetch(
          '/api/scroll?v=' +
          amount,
          {
            keepalive:true
          }
        );

      },
      40
    );

}

function stopScrollTimer(){

  if(scrollTimer !== null){

    clearInterval(
      scrollTimer
    );

    scrollTimer = null;

  }

}

scrollTrack.addEventListener(
  'pointerdown',
  scrollDown,
  {passive:false}
);

scrollTrack.addEventListener(
  'pointermove',
  scrollMove,
  {passive:false}
);

scrollTrack.addEventListener(
  'pointerup',
  scrollUp,
  {passive:false}
);

scrollTrack.addEventListener(
  'pointercancel',
  scrollUp,
  {passive:false}
);

window.addEventListener(
  'blur',
  function(){

    if(scrollActive){

      scrollActive = false;

      stopScrollTimer();

      scrollOffset = 0;

      scrollThumb.classList.remove(
        'active'
      );

      scrollThumb.style.transition =
        'top .16s ease-out';

      scrollThumb.style.top =
        '50%';

    }

  }
);

function sendMouseMove(){
const now=performance.now();
if(now-lastMoveSend<12)return;
if(Math.abs(moveQueueX)<1&&Math.abs(moveQueueY)<1)return;
const x=Math.round(moveQueueX),y=Math.round(moveQueueY);
moveQueueX-=x;moveQueueY-=y;lastMoveSend=now;
fetch('/api/move?x='+x+'&y='+y,{keepalive:true});
}
function mouseDown(b){fetch('/api/mousedown?b='+b);}
function mouseUp(b){fetch('/api/mouseup?b='+b);}

function startLongPress(action){action();longPressTimer=setTimeout(()=>{longPressInterval=setInterval(action,80);},500);}
function cancelLongPress(){if(longPressTimer){clearTimeout(longPressTimer);longPressTimer=null;}if(longPressInterval){clearInterval(longPressInterval);longPressInterval=null;}}

function flashKeyButton(b){
  if(!b) return;
  b.classList.add('pressed');
  if(b._pressFlashTimer) clearTimeout(b._pressFlashTimer);
  b._pressFlashTimer=setTimeout(()=>{
    b.classList.remove('pressed');
  },135);
}

function bindKeyPressVisual(b){
  b.addEventListener('pointerdown',()=>flashKeyButton(b),{passive:true});
  b.addEventListener('pointerup',()=>{if(b._pressFlashTimer) clearTimeout(b._pressFlashTimer); b._pressFlashTimer=setTimeout(()=>b.classList.remove('pressed'),55);},{passive:true});
  b.addEventListener('pointercancel',()=>b.classList.remove('pressed'),{passive:true});
}

async function clipboardAction(action){
  const topButtons=document.querySelectorAll('#top .topButton');
  topButtons.forEach(b=>{
    if((action==='copy' && b.innerText==='COPY') || (action==='paste' && b.innerText==='PASTE')) flashKeyButton(b);
  });
  try{
    await fetch('/api/clipboard?a='+encodeURIComponent(action),{cache:'no-store'});
  }catch(e){console.log('clipboard error',e);}
}

function renderKeyboard(){
const root=document.getElementById('keys');
root.innerHTML='';
const lettersEN=[
['Q','W','E','R','T','Y','U','I','O','P'],
['A','S','D','F','G','H','J','K','L'],
['SHIFT','Z','X','C','V','B','N','M','BACKSPACE']
];
const lettersRU=[
['Й','Ц','У','К','Е','Н','Г','Ш','Щ','З','Х','Ъ'],
['Ф','Ы','В','А','П','Р','О','Л','Д','Ж','Э'],
['SHIFT','Я','Ч','С','М','И','Т','Ь','Б','Ю','BACKSPACE']
];
const symbolsEN=[
['1','2','3','4','5','6','7','8','9','0'],
['!','@','#','$','%','^','&','*','(',')'],
['=','_','+','[',']','{','}','\\','|']
];
const symbolsRU=[
['1','2','3','4','5','6','7','8','9','0'],
['!','"','№',';','%',':','?','*','(',')'],
['=','-','+','Ё',',','.','/','\\','|']
];
const symbolsExtra=[
['<','>','~','`','"',"'",':',';','?','/'],
['€','£','¥','©','®','™','°','•','…','–'],
['←','→','↑','↓','TAB','ESC','DEL','HOME','END']
];
let rows=[];
if(keyboardLayer==='letters'||keyboardLayer==='shifted'){rows=(language==='RU')?lettersRU:lettersEN;}
else if(keyboardLayer==='symbols'){rows=(language==='RU')?symbolsRU:symbolsEN;}
else if(keyboardLayer==='symbols2'){rows=symbolsExtra;}

rows.forEach(row=>{
const div=document.createElement('div');div.className='keyRow';
row.forEach(k=>{
if(k==='BACKSPACE')div.appendChild(createBackspaceKey());
else if(k==='SHIFT')div.appendChild(createShiftKey());
else div.appendChild(createKey(k,keyboardLayer!=='letters'));
});
root.appendChild(div);
});

const funcRow=document.createElement('div');funcRow.className='keyRow';
['ESC','TAB','DEL','HOME','END','PGUP','PGDN'].forEach(k=>{const b=createKey(k,false);b.classList.add('func');funcRow.appendChild(b);});
root.appendChild(funcRow);

const arrowRow=document.createElement('div');arrowRow.className='keyRow';
['LEFT','UP','DOWN','RIGHT','F1','F2','F3','F4','F5'].forEach(k=>{const b=createKey(k,false);b.classList.add('func');arrowRow.appendChild(b);});
root.appendChild(arrowRow);

const bottom=document.createElement('div');bottom.className='keyRow';
const layerBtn=document.createElement('button');layerBtn.className='key symbol';
layerBtn.innerText=(keyboardLayer==='letters'||keyboardLayer==='shifted')?'?123':'ABC';
layerBtn.onclick=()=>{if(keyboardLayer==='letters'||keyboardLayer==='shifted')keyboardLayer='symbols';else keyboardLayer='letters';renderKeyboard();};
bottom.appendChild(layerBtn);
const moreBtn=document.createElement('button');moreBtn.className='key symbol';
moreBtn.innerText=(keyboardLayer==='symbols2')?'123':'#+=';
moreBtn.onclick=()=>{if(keyboardLayer==='symbols2')keyboardLayer='symbols';else keyboardLayer='symbols2';renderKeyboard();};
bottom.appendChild(moreBtn);
bottom.appendChild(createKey(','));
const space=document.createElement('button');space.className='key space';space.innerText='SPACE';bindKeyPressVisual(space);space.onclick=()=>pressNormalKey('SPACE');bottom.appendChild(space);
bottom.appendChild(createKey('.'));
const micBtn=document.createElement('button');micBtn.className='key micKey';micBtn.innerText='🎤';micBtn.onclick=toggleVoice;bottom.appendChild(micBtn);
const enter=document.createElement('button');enter.className='key enter';enter.innerText='ENTER';bindKeyPressVisual(enter);enter.onclick=()=>pressNormalKey('ENTER');bottom.appendChild(enter);
root.appendChild(bottom);

const modRow=document.createElement('div');modRow.className='keyRow';
['CTRL','WIN','ALT'].forEach(k=>{const b=createKey(k);b.classList.add('modifier');modRow.appendChild(b);});
const comboBtn=document.createElement('button');comboBtn.className='key';comboBtn.innerText='COMBO';comboBtn.style.flex='2';
comboBtn.onclick=()=>{const c=prompt('Combo:','CTRL+C');if(c)quickCombo(c);};
modRow.appendChild(comboBtn);
root.appendChild(modRow);
updateModifierButtons();
}

function createKey(k,isSymbol){
const b=document.createElement('button');b.className='key';
if(isSymbol)b.classList.add('symbol');
b.innerText=k;
bindKeyPressVisual(b);
if(['CTRL','SHIFT','ALT','WIN'].includes(k)){b.classList.add('modifier');b.onclick=()=>toggleModifier(k);}
else{b.onclick=()=>pressNormalKey(k);}
return b;
}
function createShiftKey(){
const b=document.createElement('button');b.className='key modifier';b.innerText='⇧';b.style.flex='1.5';
bindKeyPressVisual(b);
if(keyboardLayer==='shifted')b.classList.add('active');
b.onclick=()=>{if(keyboardLayer==='letters')keyboardLayer='shifted';else keyboardLayer='letters';renderKeyboard();};
return b;
}
function createBackspaceKey(){
const b=document.createElement('button');b.className='key backspace';b.innerText='⌫';b.style.flex='1.5';
bindKeyPressVisual(b);
const doBs=()=>pressNormalKey('BACKSPACE');
b.onpointerdown=(e)=>{e.preventDefault();startLongPress(doBs);};
b.onpointerup=()=>cancelLongPress();b.onpointercancel=()=>cancelLongPress();b.onpointerleave=()=>cancelLongPress();
return b;
}

function toggleModifier(k){modifiers[k]=!modifiers[k];updateModifierButtons();syncModifiers();}
function updateModifierButtons(){document.querySelectorAll('.modifier').forEach(b=>{const k=b.innerText;if(modifiers[k])b.classList.add('active');else b.classList.remove('active');});}
async function syncModifiers(){await fetch('/api/modifiers?ctrl='+(modifiers.CTRL?1:0)+'&shift='+(modifiers.SHIFT?1:0)+'&alt='+(modifiers.ALT?1:0)+'&win='+(modifiers.WIN?1:0));}
async function clearModifiers(){modifiers.CTRL=modifiers.SHIFT=modifiers.ALT=modifiers.WIN=false;updateModifierButtons();await fetch('/api/modifiers/clear');}

async function pressNormalKey(k){
const shifted=(keyboardLayer==='shifted')||modifiers.SHIFT;
await fetch('/api/virtualkey?k='+encodeURIComponent(k)+'&lang='+language+'&ctrl='+(modifiers.CTRL?1:0)+'&shift='+(shifted?1:0)+'&alt='+(modifiers.ALT?1:0)+'&win='+(modifiers.WIN?1:0));
if(keyboardLayer==='shifted'){keyboardLayer='letters';renderKeyboard();}
modifiers.CTRL=modifiers.SHIFT=modifiers.ALT=modifiers.WIN=false;
updateModifierButtons();
}
async function quickCombo(combo){await fetch('/api/combo?c='+encodeURIComponent(combo));}
function toggleKeyboard(){document.getElementById('keyboard').classList.toggle('open');}
function closePanel(){document.getElementById('panel').style.display='none';}

async function openMacros(){
  document.getElementById('panel').style.display='flex';
  document.getElementById('panelTitle').innerText='MACROS';
  await loadMacros();
}

async function loadMacros(){
  const body=document.getElementById('panelBody');
  try{
    const r=await fetch('/api/macros',{cache:'no-store'});
    const data=await r.json();
    let html=`
      <div class="card">
        <div class="cardTitle">MACROS</div>
        <div class="actionRow">
          <button class="green" onclick="createMacro()">+ NEW MACRO</button>
          <button class="red" onclick="stopMacro()">STOP</button>
        </div>
        <div class="small">
          Макросы сохраняются во Flash. Можно запускать, редактировать,
          удалять и менять порядок действий.
        </div>
      </div>`;

    data.macros.forEach((m,i)=>{
      html+=`<div class="macroRow">
        <button class="macroRun" onclick="runMacro(${i})">▶ ${escapeHTML(m.name)}</button>
        <button onclick="editMacro(${i})">✎</button>
        <button class="red" onclick="deleteMacro(${i})">✕</button>
      </div>`;
    });

    body.innerHTML=html;
  }catch(e){
    body.innerHTML='<div class="card"><div class="small">Ошибка загрузки макросов.</div></div>';
  }
}

function createMacro(){openMacroEditor(-1,'New Macro','');}

async function editMacro(id){
  const r=await fetch('/api/macro?id='+id,{cache:'no-store'});
  if(!r.ok){alert('Ошибка открытия макроса');return;}
  const m=await r.json();
  openMacroEditor(id,m.name,m.data);
}

function openMacroEditor(id,name,data){
  const body=document.getElementById('panelBody');
  window.editorActions=parseActions(data);

  body.innerHTML=`
    <div class="card">
      <div class="cardTitle">MACRO</div>
      <label>Name</label>
      <input id="macroName" maxlength="40" value="${escapeHTML(name)}">
    </div>
    <div class="card">
      <div class="cardTitle">ACTIONS</div>
      <div id="actions"></div>
      <div class="grid">
        <button class="blue" onclick="addAction('KEY')">+ KEY</button>
        <button class="blue" onclick="addAction('COMBO')">+ COMBO</button>
        <button class="blue" onclick="addAction('TEXT')">+ TEXT</button>
        <button class="blue" onclick="addAction('WAIT')">+ DELAY</button>
        <button class="blue" onclick="addAction('MOVE')">+ MOUSE</button>
        <button class="blue" onclick="addAction('SCROLL')">+ SCROLL</button>
        <button class="blue" onclick="addAction('CLICK')">+ CLICK</button>
      </div>
    </div>
    <div class="card">
      <button class="green" style="width:100%;margin-bottom:6px" onclick="saveMacro(${id})">SAVE MACRO</button>
      <button style="width:100%" onclick="loadMacros()">CANCEL</button>
    </div>`;

  renderActions();
}

function parseActions(data){
  if(!data)return[];
  return data.split('\n').map(x=>x.trim()).filter(Boolean).map(x=>{
    if(x.startsWith('WAIT:'))return{type:'WAIT',value:x.substring(5)};
    if(x.startsWith('TYPE_EN:'))return{type:'TEXT_EN',value:x.substring(8)};
    if(x.startsWith('TYPE_RU:'))return{type:'TEXT_RU',value:x.substring(8)};
    if(x.startsWith('TYPE_EN:'))return{type:'TEXT',value:x.substring(5)};
    if(x.startsWith('MOVE:'))return{type:'MOVE',value:x.substring(5)};
    if(x.startsWith('SCROLL:'))return{type:'SCROLL',value:x.substring(7)};
    if(x.startsWith('KEY:'))return{type:'KEY',value:x.substring(4)};
    if(x==='LMB'||x==='RMB'||x==='MMB')return{type:'CLICK',value:x};
    if(x.includes('+'))return{type:'COMBO',value:x};
    return{type:'KEY',value:x};
  });
}

function renderActions(){
  const root=document.getElementById('actions');
  root.innerHTML='';
  window.editorActions.forEach((a,i)=>{
    const div=document.createElement('div');
    div.className='actionItem';
    div.innerHTML=`<div class="actionHeader"><span>${i+1}. ${escapeHTML(a.type)}</span><div class="actionButtons">
      <button onclick="moveAction(${i},-1)">↑</button>
      <button onclick="moveAction(${i},1)">↓</button>
      <button onclick="editAction(${i})">✎</button>
      <button class="red" onclick="deleteAction(${i})">✕</button>
    </div></div><div class="small">${escapeHTML(a.value)}</div>`;
    root.appendChild(div);
  });
}

function addAction(type){
  let v='';
  if(type==='KEY')v='ENTER';
  if(type==='COMBO')v='CTRL+C';
  if(type==='TEXT')v='Hello';
  if(type==='WAIT')v='500';
  if(type==='MOVE')v='100,0';
  if(type==='SCROLL')v='-3';
  if(type==='CLICK')v='LMB';
  window.editorActions.push({type:type,value:v});
  renderActions();
}

function editAction(i){
  const a=window.editorActions[i];
  const v=prompt('Action value:',a.value);
  if(v===null)return;
  a.value=v;
  renderActions();
}

function deleteAction(i){window.editorActions.splice(i,1);renderActions();}

function moveAction(i,d){
  const n=i+d;
  if(n<0||n>=window.editorActions.length)return;
  const t=window.editorActions[i];
  window.editorActions[i]=window.editorActions[n];
  window.editorActions[n]=t;
  renderActions();
}

async function saveMacro(id){
  const name=document.getElementById('macroName').value.trim();
  if(!name){alert('Введите имя макроса');return;}

  const data=window.editorActions.map(a=>{
    if(a.type==='WAIT')return'WAIT:'+a.value;
    if(a.type==='TEXT_EN')return'TYPE_EN:'+a.value;
    if(a.type==='TEXT_RU')return'TYPE_RU:'+a.value;
    if(a.type==='TEXT')return'TYPE_EN:'+a.value;
    if(a.type==='MOVE')return'MOVE:'+a.value;
    if(a.type==='SCROLL')return'SCROLL:'+a.value;
    if(a.type==='KEY')return'KEY:'+a.value;
    if(a.type==='CLICK')return a.value;
    if(a.type==='COMBO')return a.value;
    return'';
  }).join('\n');

  const body='id='+encodeURIComponent(id)+'&name='+encodeURIComponent(name)+'&data='+encodeURIComponent(data);
  const r=await fetch('/api/macro/save',{
    method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded;charset=UTF-8'},
    body
  });
  if(!r.ok){alert('Ошибка сохранения макроса');return;}
  await loadMacros();
}

async function runMacro(i){
  const r=await fetch('/api/macro/run?id='+i);
  if(!r.ok)alert('Не удалось запустить макрос');
}

async function stopMacro(){await fetch('/api/macro/stop');}

async function deleteMacro(i){
  if(!confirm('Удалить макрос?'))return;
  await fetch('/api/macro/delete?id='+i);
  await loadMacros();
}

async function openSettings(){
document.getElementById('panel').style.display='flex';document.getElementById('panelTitle').innerText='SETTINGS';
const body=document.getElementById('panelBody');
body.innerHTML='<div class="card"><div class="cardTitle">WI-FI</div><label>SSID</label><input id="ssid"><label>Password</label><input id="password" type="password"><button class="green" onclick="saveWiFi()">SAVE & RESTART</button></div><div class="card"><div class="cardTitle">MOUSE</div><label>Sensitivity</label><input id="sensitivity" type="range" min="1" max="6" onchange="saveSensitivity(this.value)"></div><div class="card"><div class="cardTitle">QUICK</div><div class="grid"><button onclick="quickCombo(\'WIN+R\')">WIN+R</button><button onclick="quickCombo(\'WIN+E\')">WIN+E</button><button onclick="quickCombo(\'ALT+F4\')">ALT+F4</button><button onclick="quickCombo(\'CTRL+SHIFT+ESC\')">TASKMGR</button><button onclick="quickCombo(\'WIN+L\')">LOCK</button><button onclick="quickCombo(\'WIN+D\')">DESKTOP</button></div></div><div class="card"><div class="cardTitle">DEVICE</div><button class="red" onclick="factoryReset()">FACTORY RESET</button></div>';
const r=await fetch('/api/settings');const s=await r.json();
document.getElementById('ssid').value=s.ssid;document.getElementById('password').value=s.password;document.getElementById('sensitivity').value=s.sensitivity;
}
async function saveSensitivity(v){await fetch('/api/sensitivity?v='+v);}
async function saveWiFi(){const s=document.getElementById('ssid').value;const p=document.getElementById('password').value;await fetch('/api/wifi?ssid='+encodeURIComponent(s)+'&pass='+encodeURIComponent(p));alert('Saved.');}
async function factoryReset(){if(!confirm('Reset all?'))return;await fetch('/api/reset');}
function escapeHTML(s){return String(s).replaceAll('&','&amp;').replaceAll('<','&lt;').replaceAll('>','&gt;').replaceAll('"','&quot;').replaceAll("'","&#039;");}

function escapeHTMLAttr(s){return escapeHTML(s);}

renderKeyboard();updateLanguageUI();initVoice();
</script>
</body>
</html>
)HTML";

// ============================================================
// URL DECODE
// ============================================================

String urlDecode(String s) {

  s.replace("+"," ");

  String out;

  char hex[3];

  hex[2] = 0;

  for(size_t i=0;i<s.length();i++) {

    if(
      s[i] == '%' &&
      i + 2 < s.length()
    ) {

      hex[0] = s[i+1];
      hex[1] = s[i+2];

      out +=
        (char)strtol(
          hex,
          nullptr,
          16
        );

      i += 2;

    } else {

      out += s[i];

    }

  }

  return out;

}

// ============================================================
// JSON ESCAPE
// ============================================================

String jsonEscape(String s) {

  s.replace("\\","\\\\");
  s.replace("\"","\\\"");
  s.replace("\r","");
  s.replace("\n","\\n");

  return s;

}

// ============================================================
// MODIFIER CONTROL
// ============================================================

void releaseAllModifiers() {

  Keyboard.release(
    KEY_LEFT_CTRL
  );

  Keyboard.release(
    KEY_LEFT_SHIFT
  );

  Keyboard.release(
    KEY_LEFT_ALT
  );

  Keyboard.release(
    KEY_LEFT_GUI
  );

  heldCtrl  = false;
  heldShift = false;
  heldAlt   = false;
  heldWin   = false;

}

// ============================================================

void setModifier(
  String name,
  bool state
) {

  name.toUpperCase();

  if(
    name == "CTRL" ||
    name == "CONTROL"
  ) {

    if(state && !heldCtrl)
      Keyboard.press(
        KEY_LEFT_CTRL
      );

    if(!state && heldCtrl)
      Keyboard.release(
        KEY_LEFT_CTRL
      );

    heldCtrl = state;

  }

  else if(
    name == "SHIFT"
  ) {

    if(state && !heldShift)
      Keyboard.press(
        KEY_LEFT_SHIFT
      );

    if(!state && heldShift)
      Keyboard.release(
        KEY_LEFT_SHIFT
      );

    heldShift = state;

  }

  else if(
    name == "ALT"
  ) {

    if(state && !heldAlt)
      Keyboard.press(
        KEY_LEFT_ALT
      );

    if(!state && heldAlt)
      Keyboard.release(
        KEY_LEFT_ALT
      );

    heldAlt = state;

  }

  else if(
    name == "WIN" ||
    name == "GUI" ||
    name == "CMD"
  ) {

    if(state && !heldWin)
      Keyboard.press(
        KEY_LEFT_GUI
      );

    if(!state && heldWin)
      Keyboard.release(
        KEY_LEFT_GUI
      );

    heldWin = state;

  }

}

// ============================================================
// LANGUAGE
// ============================================================

void switchLanguagePC() {

  /*
    Windows:
      Left Shift + Left Alt

    This is intentionally sent as
    a simultaneous modifier combination.
  */

  Keyboard.press(
    KEY_LEFT_SHIFT
  );

  delay(25);

  Keyboard.press(
    KEY_LEFT_ALT
  );

  delay(80);

  Keyboard.release(
    KEY_LEFT_ALT
  );

  Keyboard.release(
    KEY_LEFT_SHIFT
  );

}

// ============================================================
// HID USAGE MAP
// ============================================================

uint8_t usageForKey(
  String k
) {

  k.trim();

  k.toUpperCase();

  if(k == "A") return 0x04;
  if(k == "B") return 0x05;
  if(k == "C") return 0x06;
  if(k == "D") return 0x07;
  if(k == "E") return 0x08;
  if(k == "F") return 0x09;
  if(k == "G") return 0x0A;
  if(k == "H") return 0x0B;
  if(k == "I") return 0x0C;
  if(k == "J") return 0x0D;
  if(k == "K") return 0x0E;
  if(k == "L") return 0x0F;
  if(k == "M") return 0x10;
  if(k == "N") return 0x11;
  if(k == "O") return 0x12;
  if(k == "P") return 0x13;
  if(k == "Q") return 0x14;
  if(k == "R") return 0x15;
  if(k == "S") return 0x16;
  if(k == "T") return 0x17;
  if(k == "U") return 0x18;
  if(k == "V") return 0x19;
  if(k == "W") return 0x1A;
  if(k == "X") return 0x1B;
  if(k == "Y") return 0x1C;
  if(k == "Z") return 0x1D;

  if(k == "1") return 0x1E;
  if(k == "2") return 0x1F;
  if(k == "3") return 0x20;
  if(k == "4") return 0x21;
  if(k == "5") return 0x22;
  if(k == "6") return 0x23;
  if(k == "7") return 0x24;
  if(k == "8") return 0x25;
  if(k == "9") return 0x26;
  if(k == "0") return 0x27;

  if(k == "ENTER") return 0x28;
  if(k == "ESC") return 0x29;
  if(k == "BACKSPACE") return 0x2A;
  if(k == "TAB") return 0x2B;
  if(k == "SPACE") return 0x2C;

  if(k == "-") return 0x2D;
  if(k == "=") return 0x2E;

  if(k == "[") return 0x2F;
  if(k == "]") return 0x30;

  if(k == "\\") return 0x31;
  if(k == ";") return 0x33;
  if(k == "'") return 0x34;
  if(k == "`") return 0x35;

  if(k == ",") return 0x36;
  if(k == ".") return 0x37;
  if(k == "/") return 0x38;

  if(k == "CAPSLOCK") return 0x39;

  if(k == "F1") return 0x3A;
  if(k == "F2") return 0x3B;
  if(k == "F3") return 0x3C;
  if(k == "F4") return 0x3D;
  if(k == "F5") return 0x3E;
  if(k == "F6") return 0x3F;
  if(k == "F7") return 0x40;
  if(k == "F8") return 0x41;
  if(k == "F9") return 0x42;
  if(k == "F10") return 0x43;
  if(k == "F11") return 0x44;
  if(k == "F12") return 0x45;

  if(k == "PRINTSCREEN") return 0x46;
  if(k == "SCROLLLOCK") return 0x47;
  if(k == "PAUSE") return 0x48;

  if(k == "INSERT") return 0x49;
  if(k == "HOME") return 0x4A;
  if(k == "PAGEUP") return 0x4B;

  if(k == "DELETE") return 0x4C;
  if(k == "END") return 0x4D;
  if(k == "PAGEDOWN") return 0x4E;

  if(k == "RIGHT") return 0x4F;
  if(k == "LEFT") return 0x50;
  if(k == "DOWN") return 0x51;
  if(k == "UP") return 0x52;

  return 0;

}

// ============================================================
// RUSSIAN PHYSICAL KEY MAP
// ============================================================

uint8_t russianUsage(
  String k
) {

  k.toUpperCase();

  if(k == "Й") return 0x14;
  if(k == "Ц") return 0x1A;
  if(k == "У") return 0x08;
  if(k == "К") return 0x15;
  if(k == "Е") return 0x17;
  if(k == "Н") return 0x1C;
  if(k == "Г") return 0x18;
  if(k == "Ш") return 0x0C;
  if(k == "Щ") return 0x12;
  if(k == "З") return 0x13;
  if(k == "Х") return 0x2F;
  if(k == "Ъ") return 0x30;

  if(k == "Ф") return 0x04;
  if(k == "Ы") return 0x16;
  if(k == "В") return 0x07;
  if(k == "А") return 0x09;
  if(k == "П") return 0x0A;
  if(k == "Р") return 0x0B;
  if(k == "О") return 0x0D;
  if(k == "Л") return 0x0E;
  if(k == "Д") return 0x0F;
  if(k == "Ж") return 0x33;
  if(k == "Э") return 0x34;

  if(k == "Я") return 0x1D;
  if(k == "Ч") return 0x1B;
  if(k == "С") return 0x06;
  if(k == "М") return 0x19;
  if(k == "И") return 0x05;
  if(k == "Т") return 0x11;
  if(k == "Ь") return 0x10;
  if(k == "Б") return 0x36;
  if(k == "Ю") return 0x37;

  if(k == "Ё") return 0x35;

  return 0;

}

// ============================================================
// SEND VIRTUAL KEY
// ============================================================

void sendVirtualKey(
  String key,
  String lang,
  bool ctrl,
  bool shift,
  bool alt,
  bool win
) {

  bool ownCtrl =
    ctrl && !heldCtrl;

  bool ownShift =
    shift && !heldShift;

  bool ownAlt =
    alt && !heldAlt;

  bool ownWin =
    win && !heldWin;

  if(ownCtrl)
    Keyboard.press(KEY_LEFT_CTRL);

  if(ownShift)
    Keyboard.press(KEY_LEFT_SHIFT);

  if(ownAlt)
    Keyboard.press(KEY_LEFT_ALT);

  if(ownWin)
    Keyboard.press(KEY_LEFT_GUI);

  delay(10);

  uint8_t usage = 0;
  bool needExtraShift = false;

  if(lang == "RU") {
    // Russian Windows layout maps punctuation differently from US.
    // In particular, comma/period must NOT use the physical comma/period
    // usages 0x36/0x37, because those produce Б/Ю in RU layout.
    if(key == ".") {
      usage = 0x38;              // / key -> . in RU
    }
    else if(key == ",") {
      usage = 0x38;              // / key
      needExtraShift = true;     // Shift + / -> ,
    }
    else if(key == "?") {
      usage = 0x24;              // 7 key
      needExtraShift = true;
    }
    else if(key == "!") {
      usage = 0x1E;              // 1 key
      needExtraShift = true;
    }
    else if(key == ";") {
      usage = 0x21;              // 4 key
      needExtraShift = true;
    }
    else if(key == ":") {
      usage = 0x23;              // 6 key
      needExtraShift = true;
    }
    else {
      usage = russianUsage(key);
    }
  }

  if(!usage && key.length() == 1) {

    char c =
      key.charAt(0);

    String baseKey =
      key;

    if(c == '!') { baseKey="1"; needExtraShift=true; }
    else if(c == '@') { baseKey="2"; needExtraShift=true; }
    else if(c == '#') { baseKey="3"; needExtraShift=true; }
    else if(c == '$') { baseKey="4"; needExtraShift=true; }
    else if(c == '%') { baseKey="5"; needExtraShift=true; }
    else if(c == '^') { baseKey="6"; needExtraShift=true; }
    else if(c == '&') { baseKey="7"; needExtraShift=true; }
    else if(c == '*') { baseKey="8"; needExtraShift=true; }
    else if(c == '(') { baseKey="9"; needExtraShift=true; }
    else if(c == ')') { baseKey="0"; needExtraShift=true; }
    else if(c == '_') { baseKey="-"; needExtraShift=true; }
    else if(c == '+') { baseKey="="; needExtraShift=true; }
    else if(c == '{') { baseKey="["; needExtraShift=true; }
    else if(c == '}') { baseKey="]"; needExtraShift=true; }
    else if(c == '|') { baseKey="\\"; needExtraShift=true; }
    else if(c == ':') { baseKey=";"; needExtraShift=true; }
    else if(c == '"') { baseKey="'"; needExtraShift=true; }
    else if(c == '~') { baseKey="`"; needExtraShift=true; }
    else if(c == '<') { baseKey=","; needExtraShift=true; }
    else if(c == '>') { baseKey="."; needExtraShift=true; }
    else if(c == '?') { baseKey="/"; needExtraShift=true; }

    usage =
      usageForKey(baseKey);

  }
  else if(!usage) {

    usage =
      usageForKey(key);

  }

  if(usage) {

    if(needExtraShift && !ownShift)
      Keyboard.press(KEY_LEFT_SHIFT);

    Keyboard.pressRaw(usage);
    delay(30);
    Keyboard.releaseRaw(usage);

    if(needExtraShift && !ownShift)
      Keyboard.release(KEY_LEFT_SHIFT);

  }

  if(ownWin) Keyboard.release(KEY_LEFT_GUI);
  if(ownAlt) Keyboard.release(KEY_LEFT_ALT);
  if(ownShift) Keyboard.release(KEY_LEFT_SHIFT);
  if(ownCtrl) Keyboard.release(KEY_LEFT_CTRL);

}

// ============================================================
// COMBINATION
// ============================================================

void sendCombination(
  String combo
) {

  combo.trim();
  combo.replace(" ", "");

  String parts[8];
  int count = 0;
  int start = 0;

  while(
    start < combo.length() &&
    count < 8
  ) {

    int pos = combo.indexOf('+', start);
    String p;

    if(pos < 0) {
      p = combo.substring(start);
      start = combo.length();
    } else {
      p = combo.substring(start, pos);
      start = pos + 1;
    }

    p.trim();
    if(p.length())
      parts[count++] = p;
  }

  if(count == 0)
    return;

  // IMPORTANT: modifier constants such as KEY_LEFT_GUI are Arduino
  // keycodes, not raw HID usage IDs. The old implementation passed
  // them to pressRaw(), which could leave Windows in a bad HID state.
  // Use Keyboard.press() for modifiers and pressRaw() only for the
  // normal key usages.
  bool ctrl=false, shift=false, alt=false, win=false;
  uint8_t normal[8];
  int normalCount=0;

  for(int i=0; i<count; i++) {

    String p = parts[i];
    p.toUpperCase();

    if(p == "CTRL" || p == "CONTROL") {
      if(!ctrl) Keyboard.press(KEY_LEFT_CTRL);
      ctrl=true;
    }
    else if(p == "SHIFT") {
      if(!shift) Keyboard.press(KEY_LEFT_SHIFT);
      shift=true;
    }
    else if(p == "ALT") {
      if(!alt) Keyboard.press(KEY_LEFT_ALT);
      alt=true;
    }
    else if(p == "WIN" || p == "GUI" || p == "CMD") {
      if(!win) Keyboard.press(KEY_LEFT_GUI);
      win=true;
    }
    else {
      uint8_t usage = usageForKey(p);
      if(usage && normalCount < 8)
        normal[normalCount++] = usage;
    }
  }

  delay(35);

  for(int i=0; i<normalCount; i++) {
    Keyboard.pressRaw(normal[i]);
    delay(35);
  }

  delay(70);

  for(int i=normalCount-1; i>=0; i--) {
    Keyboard.releaseRaw(normal[i]);
    delay(10);
  }

  // Release only the modifiers belonging to this combination.
  // This is more deterministic than releaseAll() and prevents a
  // following macro action from inheriting WIN/ALT/CTRL/SHIFT.
  if(win)   Keyboard.release(KEY_LEFT_GUI);
  if(alt)   Keyboard.release(KEY_LEFT_ALT);
  if(shift) Keyboard.release(KEY_LEFT_SHIFT);
  if(ctrl)  Keyboard.release(KEY_LEFT_CTRL);

  heldCtrl=false;
  heldShift=false;
  heldAlt=false;
  heldWin=false;
}

// ============================================================
// TYPE TEXT
// ============================================================

void typeAsciiPhysical(String text) {

  for(size_t i = 0; i < text.length(); i++) {

    char c = text[i];

    if(c == '\r')
      continue;

    uint8_t usage = 0;
    bool needShift = false;

    if(c >= 'a' && c <= 'z')
      usage = 0x04 + (c - 'a');

    else if(c >= 'A' && c <= 'Z') {
      usage = 0x04 + (c - 'A');
      needShift = true;
    }

    else if(c >= '1' && c <= '9')
      usage = 0x1E + (c - '1');

    else if(c == '0')
      usage = 0x27;

    else if(c == ' ')
      usage = 0x2C;

    else if(c == '\n')
      usage = 0x28;

    else if(c == '\t')
      usage = 0x2B;

    else if(c == '-')
      usage = 0x2D;

    else if(c == '_') {
      usage = 0x2D;
      needShift = true;
    }

    else if(c == '=')
      usage = 0x2E;

    else if(c == '+') {
      usage = 0x2E;
      needShift = true;
    }

    else if(c == '[')
      usage = 0x2F;

    else if(c == '{') {
      usage = 0x2F;
      needShift = true;
    }

    else if(c == ']')
      usage = 0x30;

    else if(c == '}') {
      usage = 0x30;
      needShift = true;
    }

    else if(c == '\\')
      usage = 0x31;

    else if(c == '|') {
      usage = 0x31;
      needShift = true;
    }

    else if(c == ';')
      usage = 0x33;

    else if(c == ':') {
      usage = 0x33;
      needShift = true;
    }

    else if(c == '\'')
      usage = 0x34;

    else if(c == '"') {
      usage = 0x34;
      needShift = true;
    }

    else if(c == '`')
      usage = 0x35;

    else if(c == '~') {
      usage = 0x35;
      needShift = true;
    }

    else if(c == ',')
      usage = 0x36;

    else if(c == '<') {
      usage = 0x36;
      needShift = true;
    }

    else if(c == '.')
      usage = 0x37;

    else if(c == '>') {
      usage = 0x37;
      needShift = true;
    }

    else if(c == '/')
      usage = 0x38;

    else if(c == '?') {
      usage = 0x38;
      needShift = true;
    }

    else if(c == '!') {
      usage = 0x1E;
      needShift = true;
    }

    else if(c == '@') {
      usage = 0x1F;
      needShift = true;
    }

    else if(c == '#') {
      usage = 0x20;
      needShift = true;
    }

    else if(c == '$') {
      usage = 0x21;
      needShift = true;
    }

    else if(c == '%') {
      usage = 0x22;
      needShift = true;
    }

    else if(c == '^') {
      usage = 0x23;
      needShift = true;
    }

    else if(c == '&') {
      usage = 0x24;
      needShift = true;
    }

    else if(c == '*') {
      usage = 0x25;
      needShift = true;
    }

    else if(c == '(') {
      usage = 0x26;
      needShift = true;
    }

    else if(c == ')') {
      usage = 0x27;
      needShift = true;
    }

    if(!usage)
      continue;

    if(needShift)
      Keyboard.press(KEY_LEFT_SHIFT);

    Keyboard.pressRaw(usage);
    delay(6);
    Keyboard.releaseRaw(usage);

    if(needShift)
      Keyboard.release(KEY_LEFT_SHIFT);

    delay(2);

    if((i & 7) == 7) {
      server.handleClient();
      yield();
    }

  }

}

uint8_t russianCodepointUsage(
  uint32_t cp,
  bool &uppercase
) {

  uppercase = false;

  switch(cp) {

    case 0x0410: uppercase=true; return 0x04;
    case 0x0430: return 0x04;
    case 0x0411: uppercase=true; return 0x36;
    case 0x0431: return 0x36;
    case 0x0412: uppercase=true; return 0x07;
    case 0x0432: return 0x07;
    case 0x0413: uppercase=true; return 0x18;
    case 0x0433: return 0x18;
    case 0x0414: uppercase=true; return 0x0F;
    case 0x0434: return 0x0F;
    case 0x0415: uppercase=true; return 0x17;
    case 0x0435: return 0x17;
    case 0x0401: uppercase=true; return 0x35;
    case 0x0451: return 0x35;
    case 0x0416: uppercase=true; return 0x33;
    case 0x0436: return 0x33;
    case 0x0417: uppercase=true; return 0x13;
    case 0x0437: return 0x13;
    case 0x0418: uppercase=true; return 0x05;
    case 0x0438: return 0x05;
    case 0x0419: uppercase=true; return 0x14;
    case 0x0439: return 0x14;
    case 0x041A: uppercase=true; return 0x15;
    case 0x043A: return 0x15;
    case 0x041B: uppercase=true; return 0x0E;
    case 0x043B: return 0x0E;
    case 0x041C: uppercase=true; return 0x19;
    case 0x043C: return 0x19;
    case 0x041D: uppercase=true; return 0x1C;
    case 0x043D: return 0x1C;
    case 0x041E: uppercase=true; return 0x0D;
    case 0x043E: return 0x0D;
    case 0x041F: uppercase=true; return 0x0A;
    case 0x043F: return 0x0A;
    case 0x0420: uppercase=true; return 0x0B;
    case 0x0440: return 0x0B;
    case 0x0421: uppercase=true; return 0x06;
    case 0x0441: return 0x06;
    case 0x0422: uppercase=true; return 0x11;
    case 0x0442: return 0x11;
    case 0x0423: uppercase=true; return 0x08;
    case 0x0443: return 0x08;
    case 0x0424: uppercase=true; return 0x09;
    case 0x0444: return 0x09;
    case 0x0425: uppercase=true; return 0x2F;
    case 0x0445: return 0x2F;
    case 0x0426: uppercase=true; return 0x1A;
    case 0x0446: return 0x1A;
    case 0x0427: uppercase=true; return 0x1B;
    case 0x0447: return 0x1B;
    case 0x0428: uppercase=true; return 0x0C;
    case 0x0448: return 0x0C;
    case 0x0429: uppercase=true; return 0x12;
    case 0x0449: return 0x12;
    case 0x042A: uppercase=true; return 0x30;
    case 0x044A: return 0x30;
    case 0x042B: uppercase=true; return 0x16;
    case 0x044B: return 0x16;
    case 0x042C: uppercase=true; return 0x10;
    case 0x044C: return 0x10;
    case 0x042D: uppercase=true; return 0x34;
    case 0x044D: return 0x34;
    case 0x042E: uppercase=true; return 0x37;
    case 0x044E: return 0x37;
    case 0x042F: uppercase=true; return 0x1D;
    case 0x044F: return 0x1D;

    default: return 0;

  }

}

void typeTextRU(String text) {

  const uint8_t* data =
    (const uint8_t*)text.c_str();

  size_t i = 0;
  const size_t len = text.length();

  while(i < len) {

    uint8_t b = data[i];

    if(b < 0x80) {

      char c = (char)b;

      if(c == '\r') {
        i++;
        continue;
      }

      if(c == '\n') {
        Keyboard.write(KEY_RETURN);
        i++;
        continue;
      }

      if(c == '\t') {
        Keyboard.pressRaw(0x2B);
        delay(8);
        Keyboard.releaseRaw(0x2B);
        i++;
        continue;
      }

      String one;
      one += c;
      typeAsciiPhysical(one);

      i++;
      continue;

    }

    uint32_t cp = 0;
    int bytes = 0;

    if((b & 0xE0) == 0xC0 && i + 1 < len) {
      cp =
        ((uint32_t)(b & 0x1F) << 6) |
        (data[i+1] & 0x3F);
      bytes = 2;
    }
    else if((b & 0xF0) == 0xE0 && i + 2 < len) {
      cp =
        ((uint32_t)(b & 0x0F) << 12) |
        ((uint32_t)(data[i+1] & 0x3F) << 6) |
        (data[i+2] & 0x3F);
      bytes = 3;
    }
    else {
      i++;
      continue;
    }

    bool upper = false;
    uint8_t usage =
      russianCodepointUsage(cp, upper);

    if(usage) {

      if(upper)
        Keyboard.press(KEY_LEFT_SHIFT);

      Keyboard.pressRaw(usage);
      delay(7);
      Keyboard.releaseRaw(usage);

      if(upper)
        Keyboard.release(KEY_LEFT_SHIFT);

    }

    i += bytes;
    delay(2);

    if((i & 7) == 0) {
      server.handleClient();
      yield();
    }

  }

}

void typeText(
  String text,
  String requestedLang
) {

  requestedLang.toUpperCase();

  bool wantRU =
    requestedLang == "RU";

  bool originalRU =
    russianMode;

  if(wantRU != russianMode) {

    switchLanguagePC();

    russianMode = wantRU;

    delay(80);

  }

  if(wantRU)
    typeTextRU(text);
  else
    typeAsciiPhysical(text);

  if(originalRU != russianMode) {

    switchLanguagePC();

    russianMode = originalRU;

    delay(80);

  }

}

void typeText(String text) {
  typeText(
    text,
    russianMode ? "RU" : "EN"
  );
}

// ============================================================
// WINDOWS COMMAND TYPING
// ============================================================

// ============================================================
// MACRO ACTION
// ============================================================

void executeAction(
  String action
) {

  action.trim();
  if(!action.length())return;

  String upper=action;
  upper.toUpperCase();

  // WAIT is handled by the non-blocking macro scheduler in
  // processMacroStep(). Do not consume it here.
  if(upper.startsWith("WAIT:"))return;

  if(upper.startsWith("TYPE_EN:")){
    typeText(action.substring(8),"EN");
    return;
  }

  if(upper.startsWith("TYPE_RU:")){
    typeText(action.substring(8),"RU");
    return;
  }

  if(upper.startsWith("TYPE_EN:")){
    typeText(action.substring(5));
    return;
  }

  if(upper.startsWith("KEY:")){
    String k = action.substring(4);
    k.trim();
    // Macro special keys are always sent as raw HID usages with a
    // clean modifier state. This fixes ENTER and similar keys.
    releaseAllModifiers();
    uint8_t u = usageForKey(k);
    if(u) {
      Keyboard.pressRaw(u);
      delay(55);
      Keyboard.releaseRaw(u);
      delay(35);
    } else {
      sendVirtualKey(k,"EN",false,false,false,false);
    }
    return;
  }

  if(upper=="LMB"){
    Mouse.click(MOUSE_LEFT);
    return;
  }

  if(upper=="RMB"){
    Mouse.click(MOUSE_RIGHT);
    return;
  }

  if(upper=="MMB"){
    Mouse.click(MOUSE_MIDDLE);
    return;
  }

  if(upper.startsWith("MOVE:")){
    String v=action.substring(5);
    int comma=v.indexOf(',');
    if(comma>=0){
      int x=v.substring(0,comma).toInt()*mouseSensitivity;
      int y=v.substring(comma+1).toInt()*mouseSensitivity;
      while(x!=0||y!=0){
        int8_t dx=constrain(x,-127,127);
        int8_t dy=constrain(y,-127,127);
        Mouse.move(dx,dy);
        x-=dx;
        y-=dy;
      }
    }
    return;
  }

  if(upper.startsWith("SCROLL:")){
    int v=constrain(action.substring(7).toInt(),-10,10);
    Mouse.move(0,0,v);
    return;
  }

  if(action.indexOf('+')>=0){
    sendCombination(action);
    return;
  }

  releaseAllModifiers();
  uint8_t u = usageForKey(action);
  if(u) {
    Keyboard.pressRaw(u);
    delay(55);
    Keyboard.releaseRaw(u);
    delay(35);
  } else {
    sendVirtualKey(action,"EN",false,false,false,false);
  }
}

// ============================================================
// EXECUTE MACRO
// ============================================================

void startMacroExecution(
  int id
) {

  if(
    id < 0 ||
    id >= macroCount
  )
    return;

  macroBuffer =
    macros[id].data;

  macroPos = 0;
  macroWaitUntil = 0;
  macroRunning = true;

}

void stopMacroExecution() {

  macroRunning = false;
  macroBuffer = "";
  macroPos = 0;
  macroWaitUntil = 0;

  releaseAllModifiers();

}

void processMacroStep() {

  if(!macroRunning)
    return;

  if(
    millis() <
    macroWaitUntil
  )
    return;

  int nl =
    macroBuffer.indexOf(
      '\n',
      macroPos
    );

  String line;

  if(nl < 0) {

    line =
      macroBuffer.substring(
        macroPos
      );

    macroPos =
      macroBuffer.length();

  }
  else {

    line =
      macroBuffer.substring(
        macroPos,
        nl
      );

    macroPos =
      nl + 1;

  }

  line.trim();

  if(line.length()) {

    String upper =
      line;

    upper.toUpperCase();

    if(
      upper.startsWith("WAIT:")
    ) {

      int ms =
        constrain(
          line.substring(5).toInt(),
          0,
          30000
        );

      macroWaitUntil =
        millis() + ms;

      if(
        macroPos >=
        macroBuffer.length() &&
        ms == 0
      ) {

        macroRunning =
          false;

        releaseAllModifiers();

      }

      return;

    }

    executeAction(
      line
    );

  }

  if(
    macroPos >=
    macroBuffer.length()
  ) {

    macroRunning =
      false;

    releaseAllModifiers();

  }

}

void executeMacro(
  int id
) {

  startMacroExecution(id);

}

// ============================================================
// MACRO STORAGE
// ============================================================

void saveMacros() {

  prefs.begin(
    "hidmac",
    false
  );

  prefs.putUInt(
    "count",
    macroCount
  );

  for(
    int i=0;
    i<MAX_MACROS;
    i++
  ) {

    String n =
      "n" + String(i);

    String d =
      "d" + String(i);

    if(i < macroCount) {

      prefs.putString(
        n.c_str(),
        macros[i].name
      );

      prefs.putString(
        d.c_str(),
        macros[i].data
      );

    } else {

      prefs.remove(
        n.c_str()
      );

      prefs.remove(
        d.c_str()
      );

    }

  }

  prefs.end();

}

// ============================================================

void loadMacros() {

  prefs.begin(
    "hidmac",
    true
  );

  macroCount =
    prefs.getUInt(
      "count",
      0
    );

  if(
    macroCount < 0 ||
    macroCount > MAX_MACROS
  )
    macroCount = 0;

  for(
    int i=0;
    i<macroCount;
    i++
  ) {

    String n =
      "n" + String(i);

    String d =
      "d" + String(i);

    macros[i].name =
      prefs.getString(
        n.c_str(),
        "Macro"
      );

    macros[i].data =
      prefs.getString(
        d.c_str(),
        ""
      );

  }

  prefs.end();

}

// ============================================================
// DEFAULT MACROS
// ============================================================

bool macroExistsByName(
  const String &name
) {

  for(int i = 0; i < macroCount; i++) {

    if(
      macros[i].name == name
    )
      return true;

  }

  return false;

}

void addDefaultMacro(
  const char* name,
  const char* data
) {

  if(
    macroCount >=
    MAX_MACROS
  )
    return;

  if(
    macroExistsByName(name)
  )
    return;

  macros[macroCount].name =
    name;

  macros[macroCount].data =
    data;

  macroCount++;

}

void createDefaultMacros() {

  addDefaultMacro("Copy", "CTRL+C");
  addDefaultMacro("Paste", "CTRL+V");
  addDefaultMacro("Cut", "CTRL+X");
  addDefaultMacro("Undo", "CTRL+Z");
  addDefaultMacro("Select All", "CTRL+A");
  addDefaultMacro("Find", "CTRL+F");
  addDefaultMacro("Save", "CTRL+S");

  addDefaultMacro(
    "Task Mgr",
    "CTRL+SHIFT+ESC"
  );

  addDefaultMacro(
    "Run",
    "WIN+R"
  );

  addDefaultMacro(
    "Explorer",
    "WIN+E"
  );

  addDefaultMacro(
    "ALT+TAB",
    "ALT+TAB"
  );

  addDefaultMacro(
    "Close Window",
    "ALT+F4"
  );

  addDefaultMacro(
    "Lock PC",
    "WIN+L"
  );

  addDefaultMacro(
    "Show Desktop",
    "WIN+D"
  );

  addDefaultMacro(
    "Settings",
    "WIN+I"
  );

  addDefaultMacro(
    "Screenshot",
    "WIN+SHIFT+S"
  );

  addDefaultMacro(
    "CMD",
    "WIN+R\nWAIT:650\nTYPE_EN:cmd\nWAIT:100\nKEY:ENTER"
  );

  addDefaultMacro(
    "Admin CMD",
    "WIN+R\nWAIT:400\nTYPE_EN:cmd\nWAIT:250\nCTRL+SHIFT+ENTER\nWAIT:1500\nALT+Y"
  );

  addDefaultMacro(
    "PowerShell",
    "WIN+R\nWAIT:650\nTYPE_EN:powershell\nWAIT:100\nKEY:ENTER"
  );

  addDefaultMacro(
    "Admin PowerShell",
    "WIN+R\nWAIT:400\nTYPE_EN:powershell\nWAIT:250\nCTRL+SHIFT+ENTER\nWAIT:1500\nALT+Y"
  );

  addDefaultMacro(
    "Regedit",
    "WIN+R\nWAIT:650\nTYPE_EN:regedit\nWAIT:100\nKEY:ENTER"
  );

  addDefaultMacro(
    "Services",
    "WIN+R\nWAIT:650\nTYPE_EN:services.msc\nWAIT:100\nKEY:ENTER"
  );

  addDefaultMacro(
    "Device Manager",
    "WIN+R\nWAIT:650\nTYPE_EN:devmgmt.msc\nWAIT:100\nKEY:ENTER"
  );

  addDefaultMacro(
    "Disk Management",
    "WIN+R\nWAIT:650\nTYPE_EN:diskmgmt.msc\nWAIT:100\nKEY:ENTER"
  );

  addDefaultMacro(
    "Computer Management",
    "WIN+R\nWAIT:650\nTYPE_EN:compmgmt.msc\nWAIT:100\nKEY:ENTER"
  );

  addDefaultMacro(
    "Event Viewer",
    "WIN+R\nWAIT:650\nTYPE_EN:eventvwr.msc\nWAIT:100\nKEY:ENTER"
  );

  addDefaultMacro(
    "MSConfig",
    "WIN+R\nWAIT:650\nTYPE_EN:msconfig\nWAIT:100\nKEY:ENTER"
  );

  addDefaultMacro(
    "GPEdit",
    "WIN+R\nWAIT:650\nTYPE_EN:gpedit.msc\nWAIT:100\nKEY:ENTER"
  );

  addDefaultMacro(
    "Programs",
    "WIN+R\nWAIT:650\nTYPE_EN:appwiz.cpl\nWAIT:100\nKEY:ENTER"
  );

  addDefaultMacro(
    "Network Connections",
    "WIN+R\nWAIT:650\nTYPE_EN:ncpa.cpl\nWAIT:100\nKEY:ENTER"
  );

  addDefaultMacro(
    "User Accounts",
    "WIN+R\nWAIT:650\nTYPE_EN:netplwiz\nWAIT:100\nKEY:ENTER"
  );

  addDefaultMacro(
    "System Information",
    "WIN+R\nWAIT:650\nTYPE_EN:msinfo32\nWAIT:100\nKEY:ENTER"
  );

  addDefaultMacro(
    "IPConfig /all",
    "WIN+R\nWAIT:400\nTYPE_EN:cmd\nKEY:ENTER\nWAIT:700\nTYPE_EN:ipconfig /all\nKEY:ENTER"
  );

  addDefaultMacro(
    "Ping 8.8.8.8",
    "WIN+R\nWAIT:400\nTYPE_EN:cmd\nKEY:ENTER\nWAIT:700\nTYPE_EN:ping 8.8.8.8\nKEY:ENTER"
  );

  addDefaultMacro(
    "Netstat",
    "WIN+R\nWAIT:400\nTYPE_EN:cmd\nKEY:ENTER\nWAIT:700\nTYPE_EN:netstat -ano\nKEY:ENTER"
  );

  addDefaultMacro(
    "Flush DNS",
    "WIN+R\nWAIT:400\nTYPE_EN:cmd\nKEY:ENTER\nWAIT:700\nTYPE_EN:ipconfig /flushdns\nKEY:ENTER"
  );

  addDefaultMacro(
    "System Info",
    "WIN+R\nWAIT:400\nTYPE_EN:cmd\nKEY:ENTER\nWAIT:700\nTYPE_EN:systeminfo\nKEY:ENTER"
  );

  addDefaultMacro(
    "Task List",
    "WIN+R\nWAIT:400\nTYPE_EN:cmd\nKEY:ENTER\nWAIT:700\nTYPE_EN:tasklist\nKEY:ENTER"
  );

  addDefaultMacro(
    "GPUpdate",
    "WIN+R\nWAIT:400\nTYPE_EN:cmd\nKEY:ENTER\nWAIT:700\nTYPE_EN:gpupdate /force\nKEY:ENTER"
  );

  addDefaultMacro(
    "Restart Explorer",
    "WIN+R\nWAIT:400\nTYPE_EN:powershell -Command \"Stop-Process -Name explorer -Force; Start-Process explorer.exe\"\nKEY:ENTER"
  );

  addDefaultMacro(
    "CMD ipconfig",
    "WIN+R\nWAIT:400\nTYPE_EN:cmd /k ipconfig\nKEY:ENTER"
  );

  addDefaultMacro(
    "CMD tasklist",
    "WIN+R\nWAIT:400\nTYPE_EN:cmd /k tasklist\nKEY:ENTER"
  );

  addDefaultMacro(
    "CMD netstat",
    "WIN+R\nWAIT:400\nTYPE_EN:cmd /k netstat -ano\nKEY:ENTER"
  );

  addDefaultMacro(
    "Wi-Fi Profiles",
    "WIN+R\nWAIT:400\nTYPE_EN:cmd\nKEY:ENTER\nWAIT:900\nTYPE_EN:netsh wlan show profiles\nKEY:ENTER"
  );

  addDefaultMacro(
    "SFC /scannow",
    "WIN+R\nWAIT:400\nTYPE_EN:cmd\nWAIT:250\nCTRL+SHIFT+ENTER\nWAIT:1500\nALT+Y\nWAIT:800\nTYPE_EN:sfc /scannow\nKEY:ENTER"
  );

  addDefaultMacro(
    "Open Run + type",
    "WIN+R\nWAIT:300\nTYPE_EN:Hello from ESP32\nKEY:ENTER"
  );

  addDefaultMacro(
    "Mouse Click Sequence",
    "MOVE:100,0\nLMB\nWAIT:200\nMOVE:-100,0\nRMB"
  );

  saveMacros();

}

// ============================================================
// ROOT
// ============================================================

void handleRoot() {

  server.send_P(
    200,
    "text/html",
    INDEX_HTML
  );

}

// ============================================================
// MOUSE
// ============================================================

void handleMove() {

  int x =
    server.arg("x").toInt();

  int y =
    server.arg("y").toInt();

  x *=
    mouseSensitivity;

  y *=
    mouseSensitivity;

  while(
    x != 0 ||
    y != 0
  ) {

    int8_t dx =
      constrain(
        x,
        -127,
        127
      );

    int8_t dy =
      constrain(
        y,
        -127,
        127
      );

    Mouse.move(
      dx,
      dy
    );

    x -= dx;
    y -= dy;

  }

  server.send(
    200,
    "text/plain",
    "OK"
  );

}

// ============================================================

void handleMouseDown() {

  int b =
    server.arg("b")
      .toInt();

  if(b == 1)
    Mouse.press(
      MOUSE_LEFT
    );

  if(b == 2)
    Mouse.press(
      MOUSE_RIGHT
    );

  if(b == 3)
    Mouse.press(
      MOUSE_MIDDLE
    );

  server.send(
    200,
    "text/plain",
    "OK"
  );

}

// ============================================================

void handleMouseUp() {

  int b =
    server.arg("b")
      .toInt();

  if(b == 1)
    Mouse.release(
      MOUSE_LEFT
    );

  if(b == 2)
    Mouse.release(
      MOUSE_RIGHT
    );

  if(b == 3)
    Mouse.release(
      MOUSE_MIDDLE
    );

  server.send(
    200,
    "text/plain",
    "OK"
  );

}

// ============================================================
// SCROLL
// ============================================================

void handleScroll() {

  int v =
    server.arg("v")
      .toInt();

  v =
    constrain(
      v,
      -10,
      10
    );

  Mouse.move(
    0,
    0,
    v
  );

  server.send(
    200,
    "text/plain",
    "OK"
  );

}

// ============================================================
// LANGUAGE API
// ============================================================

void handleLanguage() {

  switchLanguagePC();

  russianMode =
    !russianMode;

  server.send(
    200,
    "text/plain",
    "OK"
  );

}

// ============================================================
// MODIFIERS API
// ============================================================

void handleModifiers() {

  bool ctrl =
    server.arg("ctrl") == "1";

  bool shift =
    server.arg("shift") == "1";

  bool alt =
    server.arg("alt") == "1";

  bool win =
    server.arg("win") == "1";

  setModifier(
    "CTRL",
    ctrl
  );

  setModifier(
    "SHIFT",
    shift
  );

  setModifier(
    "ALT",
    alt
  );

  setModifier(
    "WIN",
    win
  );

  server.send(
    200,
    "text/plain",
    "OK"
  );

}

// ============================================================

void handleClearModifiers() {

  releaseAllModifiers();

  server.send(
    200,
    "text/plain",
    "OK"
  );

}

// ============================================================
// VIRTUAL KEY
// ============================================================

void handleVirtualKey() {

  String k =
    urlDecode(
      server.arg("k")
    );

  String lang =
    server.arg("lang");

  bool ctrl =
    server.arg("ctrl") == "1";

  bool shift =
    server.arg("shift") == "1";

  bool alt =
    server.arg("alt") == "1";

  bool win =
    server.arg("win") == "1";

  sendVirtualKey(
    k,
    lang,
    ctrl,
    shift,
    alt,
    win
  );

  server.send(
    200,
    "text/plain",
    "OK"
  );

}

// ============================================================
// COMBINATION
// ============================================================

void handleCombo() {

  String combo =
    urlDecode(
      server.arg("c")
    );

  sendCombination(
    combo
  );

  server.send(
    200,
    "text/plain",
    "OK"
  );

}
// ============================================================
// DIRECT CLIPBOARD SHORTCUTS
// ============================================================

void handleClipboard() {

  String action = server.arg("a");
  action.toLowerCase();

  if(action != "copy" && action != "paste") {
    server.send(400,"text/plain","BAD ACTION");
    return;
  }

  // Use a dedicated raw HID path instead of the generic combo parser.
  // C = HID usage 0x06, V = HID usage 0x19.
  const uint8_t usage = (action == "copy") ? 0x06 : 0x19;

  // Ensure a previous modifier state cannot corrupt Ctrl+C/Ctrl+V.
  releaseAllModifiers();

  Keyboard.press(KEY_LEFT_CTRL);
  delay(35);
  Keyboard.pressRaw(usage);
  delay(65);
  Keyboard.releaseRaw(usage);
  delay(25);
  Keyboard.release(KEY_LEFT_CTRL);

  heldCtrl=false;
  heldShift=false;
  heldAlt=false;
  heldWin=false;

  server.send(200,"text/plain","OK");

}
// ============================================================
// HTTP: VOICE / TEXT INPUT
// ============================================================

void handleType() {

  String text =
    server.arg("text");

  String lang =
    server.arg("lang");

  text =
    urlDecode(text);

  if(lang.length() == 0)
    lang =
      russianMode ? "RU" : "EN";

  typeText(
    text,
    lang
  );

  server.send(
    200,
    "text/plain",
    "OK"
  );

}



// ============================================================
// SETTINGS
// ============================================================

void handleSettings() {

  String json =
    "{";

  json +=
    "\"ssid\":\"";

  json +=
    jsonEscape(
      apSSID
    );

  json +=
    "\",";

  json +=
    "\"password\":\"";

  json +=
    jsonEscape(
      apPassword
    );

  json +=
    "\",";

  json +=
    "\"sensitivity\":";

  json +=
    String(
      mouseSensitivity
    );

  json +=
    "}";

  server.send(
    200,
    "application/json",
    json
  );

}

// ============================================================
// SENSITIVITY
// ============================================================

void handleSensitivity() {

  int v =
    server.arg("v")
      .toInt();

  v =
    constrain(
      v,
      1,
      6
    );

  mouseSensitivity =
    v;

  prefs.begin(
    "hidcfg",
    false
  );

  prefs.putInt(
    "sens",
    v
  );

  prefs.end();

  server.send(
    200,
    "text/plain",
    "OK"
  );

}

// ============================================================
// WI-FI
// ============================================================

void handleWiFi() {

  String ssid =
    urlDecode(
      server.arg("ssid")
    );

  String pass =
    urlDecode(
      server.arg("pass")
    );

  if(
    ssid.length() < 1 ||
    ssid.length() > 32
  ) {

    server.send(
      400,
      "text/plain",
      "Invalid SSID"
    );

    return;

  }

  if(
    pass.length() > 0 &&
    pass.length() < 8
  ) {

    server.send(
      400,
      "text/plain",
      "Password must contain at least 8 characters"
    );

    return;

  }

  prefs.begin(
    "hidcfg",
    false
  );

  prefs.putString(
    "ssid",
    ssid
  );

  prefs.putString(
    "pass",
    pass
  );

  prefs.end();

  server.send(
    200,
    "text/plain",
    "OK"
  );

  delay(300);

  ESP.restart();

}

// ============================================================
// MACROS API
// ============================================================

void handleMacros() {

  String json =
    "{\"macros\":[";

  for(
    int i=0;
    i<macroCount;
    i++
  ) {

    if(i)
      json += ",";

    json +=
      "{\"id\":" +
      String(i) +
      ",\"name\":\"" +
      jsonEscape(
        macros[i].name
      ) +
      "\"}";

  }

  json +=
    "]}";

  server.send(
    200,
    "application/json",
    json
  );

}

// ============================================================

void handleMacroGet() {

  int id =
    server.arg("id")
      .toInt();

  if(
    id < 0 ||
    id >= macroCount
  ) {

    server.send(
      404,
      "text/plain",
      "Not found"
    );

    return;

  }

  String json =
    "{";

  json +=
    "\"id\":" +
    String(id) +
    ",";

  json +=
    "\"name\":\"" +
    jsonEscape(
      macros[id].name
    ) +
    "\",";

  json +=
    "\"data\":\"" +
    jsonEscape(
      macros[id].data
    ) +
    "\"}";

  server.send(
    200,
    "application/json",
    json
  );

}

// ============================================================

void handleMacroSave() {

  int id =
    server.arg("id")
      .toInt();

  String name =
    urlDecode(
      server.arg("name")
    );

  String data =
    urlDecode(
      server.arg("data")
    );

  name.trim();

  if(
    name.length() == 0
  )
    name =
      "New Macro";

  if(
    name.length() >
    MACRO_NAME_LEN
  )
    name =
      name.substring(
        0,
        MACRO_NAME_LEN
      );

  if(
    data.length() >
    MACRO_DATA_LEN
  )
    data =
      data.substring(
        0,
        MACRO_DATA_LEN
      );

  if(id < 0) {

    if(
      macroCount >=
      MAX_MACROS
    ) {

      server.send(
        400,
        "text/plain",
        "Macro limit reached"
      );

      return;

    }

    id =
      macroCount++;

  }

  if(
    id >= MAX_MACROS
  ) {

    server.send(
      400,
      "text/plain",
      "Invalid macro"
    );

    return;

  }

  macros[id].name =
    name;

  macros[id].data =
    data;

  saveMacros();

  server.send(
    200,
    "text/plain",
    "OK"
  );

}

// ============================================================

void handleMacroDelete() {

  int id =
    server.arg("id")
      .toInt();

  if(
    id < 0 ||
    id >= macroCount
  ) {

    server.send(
      400,
      "text/plain",
      "Invalid ID"
    );

    return;

  }

  for(
    int i=id;
    i<macroCount-1;
    i++
  ) {

    macros[i] =
      macros[i+1];

  }

  macroCount--;

  saveMacros();

  server.send(
    200,
    "text/plain",
    "OK"
  );

}

// ============================================================

void handleMacroRun() {

  int id =
    server.arg("id").toInt();

  if(
    id < 0 ||
    id >= macroCount
  ) {

    server.send(
      400,
      "text/plain",
      "Invalid ID"
    );

    return;

  }

  if(macroRunning)
    stopMacroExecution();

  startMacroExecution(
    id
  );

  server.send(
    200,
    "text/plain",
    "OK"
  );

}

void handleMacroStop() {

  stopMacroExecution();

  server.send(
    200,
    "text/plain",
    "OK"
  );

}

// ============================================================
// RESET
// ============================================================

void handleReset() {

  prefs.begin(
    "hidcfg",
    false
  );

  prefs.clear();

  prefs.end();

  prefs.begin(
    "hidmac",
    false
  );

  prefs.clear();

  prefs.end();

  releaseAllModifiers();

  server.send(
    200,
    "text/plain",
    "RESET"
  );

  delay(500);

  ESP.restart();

}

// ============================================================
// CAPTIVE PORTAL
// ============================================================

void captivePortal() {

  server.sendHeader(
    "Location",
    "http://192.168.4.1/",
    true
  );

  server.send(
    302,
    "text/plain",
    ""
  );

}

// ============================================================
// WI-FI START
// ============================================================

void startWiFi() {

  prefs.begin(
    "hidcfg",
    true
  );

  apSSID =
    prefs.getString(
      "ssid",
      "ESP32-HID"
    );

  apPassword =
    prefs.getString(
      "pass",
      "12345678"
    );

  mouseSensitivity =
    prefs.getInt(
      "sens",
      2
    );

  prefs.end();

  if(
    apPassword.length() > 0 &&
    apPassword.length() < 8
  )
    apPassword =
      "12345678";

  WiFi.mode(
    WIFI_AP
  );

  WiFi.softAPConfig(
    apIP,
    apGateway,
    apSubnet
  );

  WiFi.softAP(
    apSSID.c_str(),
    apPassword.length()
      ? apPassword.c_str()
      : nullptr,
    6,
    false,
    4
  );

  delay(200);

  dnsServer.start(
    53,
    "*",
    apIP
  );

}

// ============================================================
// SERVER
// ============================================================

void startServer() {

  server.on(
    "/",
    HTTP_GET,
    handleRoot
  );

  server.on(
    "/api/move",
    HTTP_GET,
    handleMove
  );

  server.on(
    "/api/mousedown",
    HTTP_GET,
    handleMouseDown
  );

  server.on(
    "/api/mouseup",
    HTTP_GET,
    handleMouseUp
  );

  server.on(
    "/api/scroll",
    HTTP_GET,
    handleScroll
  );

  server.on(
    "/api/language",
    HTTP_GET,
    handleLanguage
  );

  server.on(
    "/api/modifiers",
    HTTP_GET,
    handleModifiers
  );

  server.on(
    "/api/modifiers/clear",
    HTTP_GET,
    handleClearModifiers
  );

  server.on(
    "/api/virtualkey",
    HTTP_GET,
    handleVirtualKey
  );

  server.on(
    "/api/combo",
    HTTP_GET,
    handleCombo
  );
  server.on(
    "/api/clipboard",
    HTTP_GET,
    handleClipboard
  );
  server.on(
    "/api/type",
    HTTP_POST,
    handleType
  );
  server.on(
    "/api/type",
    HTTP_GET,
    handleType
  );



  server.on(
    "/api/settings",
    HTTP_GET,
    handleSettings
  );

  server.on(
    "/api/sensitivity",
    HTTP_GET,
    handleSensitivity
  );

  server.on(
    "/api/wifi",
    HTTP_GET,
    handleWiFi
  );

  server.on(
    "/api/macros",
    HTTP_GET,
    handleMacros
  );

  server.on(
    "/api/macro",
    HTTP_GET,
    handleMacroGet
  );

  server.on(
    "/api/macro/save",
    HTTP_POST,
    handleMacroSave
  );

  server.on(
    "/api/macro/save",
    HTTP_GET,
    handleMacroSave
  );

  server.on(
    "/api/macro/delete",
    HTTP_GET,
    handleMacroDelete
  );

  server.on(
    "/api/macro/run",
    HTTP_GET,
    handleMacroRun
  );
  server.on(
    "/api/macro/stop",
    HTTP_GET,
    handleMacroStop
  );

  server.on(
    "/api/reset",
    HTTP_GET,
    handleReset
  );

  server.on(
    "/generate_204",
    HTTP_GET,
    captivePortal
  );

  server.on(
    "/hotspot-detect.html",
    HTTP_GET,
    captivePortal
  );

  server.on(
    "/connecttest.txt",
    HTTP_GET,
    captivePortal
  );

  server.on(
    "/ncsi.txt",
    HTTP_GET,
    captivePortal
  );

  server.onNotFound(
    captivePortal
  );

  server.begin();

}

// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(
    115200
  );

  delay(500);

  /*
    USB HID initialization.
  */

  Keyboard.begin(
    KeyboardLayout_en_US
  );

  Mouse.begin();

  USB.begin();

  delay(500);

  /*
    Storage.
  */

  loadMacros();

  createDefaultMacros();

  /*
    Wi-Fi.
  */

  startWiFi();

  /*
    HTTP.
  */

  startServer();

  Serial.println();
  Serial.println(
    "================================"
  );

  Serial.println(
    " ESP32-S3 HID CONTROLLER"
  );

  Serial.println(
    "================================"
  );

  Serial.print(
    "SSID: "
  );

  Serial.println(
    apSSID
  );

  Serial.print(
    "IP: "
  );

  Serial.println(
    WiFi.softAPIP()
  );

  Serial.println(
    "USB: HID Keyboard + Mouse"
  );

  Serial.println(
    "Scroll slider: continuous + auto-return"
  );

  Serial.println(
    "Smartphone keyboard: enabled"
  );

}

// ============================================================
// LOOP
// ============================================================

void loop() {

  dnsServer.processNextRequest();

  server.handleClient();

  processMacroStep();

  delay(1);

}
