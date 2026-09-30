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
    - Vertical auto-returning scroll slider

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

#define MAX_MACROS 30
#define MACRO_NAME_LEN 40
#define MACRO_DATA_LEN 1500

struct Macro {
  String name;
  String data;
};

Macro macros[MAX_MACROS];

int macroCount = 0;

// ============================================================
// HTML
// ============================================================

const char INDEX_HTML[] PROGMEM = R"HTML(

<!DOCTYPE html>

<html lang="ru">

<head>

<meta charset="UTF-8">

<meta name="viewport"
      content="width=device-width,
               initial-scale=1,
               maximum-scale=1,
               user-scalable=no,
               viewport-fit=cover">

<meta name="mobile-web-app-capable"
      content="yes">

<meta name="apple-mobile-web-app-capable"
      content="yes">

<meta name="theme-color"
      content="#0b0e12">

<title>ESP32 HID</title>

<style>

* {
  box-sizing:border-box;
  -webkit-tap-highlight-color:transparent;
  user-select:none;
}

html,
body {

  width:100%;
  height:100%;

  margin:0;
  padding:0;

  overflow:hidden;

  background:#080b0e;
  color:#fff;

  overscroll-behavior:none;
  overscroll-behavior-y:none;

  touch-action:none;

  font-family:
    Arial,
    Helvetica,
    sans-serif;

}

body {

  position:fixed;

  left:0;
  top:0;
  right:0;
  bottom:0;

}

button {

  border:0;

  color:white;

  background:#20262d;

  border-radius:12px;

  min-height:46px;

  padding:8px 12px;

  font-size:14px;

  font-weight:600;

  touch-action:manipulation;

}

button:active {

  transform:scale(.97);

}

button.active {

  background:#0b6f9f;

  box-shadow:
    0 0 0 2px #24b9ff inset;

}

button.green {

  background:#205b43;

}

button.red {

  background:#632d2d;

}

button.blue {

  background:#1d506d;

}

#app {

  width:100%;
  height:100dvh;

  display:flex;

  flex-direction:column;

  overflow:hidden;

  overscroll-behavior:none;

}

#top {

  height:58px;

  flex-shrink:0;

  display:flex;

  align-items:center;

  gap:5px;

  padding:5px;

  background:#11161b;

  border-bottom:
    1px solid #293039;

}

.topButton {

  min-width:47px;

  min-height:46px;

  padding:5px 7px;

}

#status {

  flex:1;

  min-width:0;

  text-align:center;

  color:#8995a1;

  font-size:12px;

  overflow:hidden;

  white-space:nowrap;

}

#touchpadContainer {
  flex:1;
  min-height:0;
  display:flex;
  position:relative;
  overflow:hidden;
  touch-action:none;
  overscroll-behavior:none;
}

#touchpad {

  flex:1;

  min-height:0;

  position:relative;

  overflow:hidden;

  touch-action:none;

  overscroll-behavior:none;

  background:

    radial-gradient(
      circle at center,
      #151c22 0,
      #0c1014 75%
    );

}

#touchpad::after {

  content:"TOUCHPAD";

  position:absolute;

  left:50%;
  top:50%;

  transform:
    translate(-50%,-50%);

  color:#252e36;

  font-size:13px;

  letter-spacing:4px;

  pointer-events:none;

}

#scrollTrack {
  width: 24px;
  background: #11161b;
  border-left: 1px solid #293039;
  position: relative;
  display: flex;
  align-items: center;
  justify-content: center;
  touch-action: none;
}

#scrollThumb {
  width: 16px;
  height: 60px;
  background: #20262d;
  border: 1px solid #3a4552;
  border-radius: 8px;
  position: absolute;
  top: 50%;
  transform: translateY(-50%);
  touch-action: none;
  transition: top 0.15s ease-out;
}

#scrollThumb.active {
  background: #0b6f9f;
  border-color: #24b9ff;
  transition: none;
}

#mouseButtons {

  height:118px;

  flex-shrink:0;

  display:grid;

  grid-template-columns:
    1fr 1fr;

  gap:6px;

  padding:6px;

  background:#11161b;

}

.mouseButton {

  height:100%;

  min-height:105px;

  border-radius:20px;

  font-size:23px;

}

.mouseLeft {

  background:#1b3d51;

}

.mouseRight {

  background:#513127;

}

#keyboard {

  position:absolute;

  z-index:100;

  left:0;
  right:0;
  bottom:0;

  max-height:82dvh;

  padding:7px;

  overflow:auto;

  background:#0e1318;

  border-top:
    1px solid #303943;

  transform:
    translateY(105%);

  transition:
    transform .15s ease;

  touch-action:pan-y;

  overscroll-behavior:contain;

}

#keyboard.open {

  transform:
    translateY(0);

}

.keyRow {

  display:flex;

  gap:4px;

  margin-bottom:4px;

}

.key {

  flex:1;

  min-width:0;

  min-height:45px;

  padding:3px;

  font-size:13px;

}

.key.wide {

  flex:2;

}

.key.modifier {

  background:#303944;

}

#keyboardBar {

  display:flex;

  gap:5px;

  margin-bottom:6px;

}

#keyboardBar button {

  flex:1;

}

#keyboardTitle {

  flex:2;

  display:flex;

  align-items:center;

  justify-content:center;

  color:#aeb8c2;

}

#panel {

  display:none;

  position:absolute;

  z-index:200;

  inset:0;

  background:#090c10;

  flex-direction:column;

  overflow:hidden;

  touch-action:auto;

}

#panelHeader {

  height:57px;

  flex-shrink:0;

  display:flex;

  align-items:center;

  gap:7px;

  padding:5px;

  background:#11161b;

  border-bottom:
    1px solid #29313a;

}

#panelTitle {

  flex:1;

  font-weight:bold;

}

#panelBody {

  flex:1;

  overflow:auto;

  padding:9px;

  touch-action:pan-y;

  overscroll-behavior:contain;

}

.card {

  background:#141a20;

  border:1px solid #282f37;

  border-radius:14px;

  padding:10px;

  margin-bottom:9px;

}

.cardTitle {

  font-size:16px;

  font-weight:bold;

  margin-bottom:8px;

}

.grid {

  display:grid;

  grid-template-columns:
    repeat(2,1fr);

  gap:6px;

}

input,
textarea,
select {

  width:100%;

  border:1px solid #39434d;

  border-radius:10px;

  background:#080b0e;

  color:white;

  padding:10px;

  font-size:15px;

  user-select:text;

}

textarea {

  min-height:130px;

  font-family:monospace;

  resize:none;

}

label {

  display:block;

  color:#8f9ba7;

  font-size:12px;

  margin:
    7px 0 4px;

}

.small {

  color:#788591;

  font-size:11px;

  line-height:1.45;

}

.actionRow {

  display:flex;

  gap:5px;

  margin-bottom:5px;

}

.actionRow button {

  flex:1;

}

.macroRow {

  display:flex;

  gap:5px;

  margin-bottom:6px;

}

.macroRun {

  flex:1;

  text-align:left;

}

.actionItem {

  background:#1b2229;

  border:1px solid #2e3841;

  border-radius:10px;

  padding:8px;

  margin-bottom:5px;

}

.actionHeader {

  display:flex;

  align-items:center;

  gap:5px;

}

.actionHeader span {

  flex:1;

}

.actionButtons {

  display:flex;

  gap:3px;

}

.actionButtons button {

  min-height:36px;

  min-width:38px;

  padding:3px;

}

#lang {

  min-width:53px;

}

</style>

</head>

<body>

<div id="app">

  <div id="top">

    <button
      class="topButton"
      onclick="quickCombo('CTRL+C')">
      COPY
    </button>

    <button
      class="topButton"
      onclick="quickCombo('CTRL+V')">
      PASTE
    </button>

    <div id="status">
      ESP32 HID
    </div>

    <button
      id="lang"
      class="topButton"
      onclick="toggleLanguage()">
      EN
    </button>

    <button
      class="topButton"
      onclick="openMacros()">
      MAC
    </button>

    <button
      class="topButton"
      onclick="openSettings()">
      ⚙
    </button>

    <button
      class="topButton"
      onclick="toggleKeyboard()">
      ⌨
    </button>

  </div>

  <div id="touchpadContainer">
    <div id="touchpad"></div>
    <div id="scrollTrack">
      <div id="scrollThumb"></div>
    </div>
  </div>

  <div id="mouseButtons">

    <button
      class="mouseButton mouseLeft"
      onpointerdown="mouseDown(1)"
      onpointerup="mouseUp(1)"
      onpointercancel="mouseUp(1)">
      LEFT
    </button>

    <button
      class="mouseButton mouseRight"
      onpointerdown="mouseDown(2)"
      onpointerup="mouseUp(2)"
      onpointercancel="mouseUp(2)">
      RIGHT
    </button>

  </div>

</div>

<div id="keyboard">

  <div id="keyboardBar">

    <button onclick="clearModifiers()">
      CLR
    </button>

    <div id="keyboardTitle">
      KEYBOARD
    </div>

    <button onclick="toggleLanguage()"
            id="keyboardLang">
      EN
    </button>

    <button onclick="toggleKeyboard()">
      ✕
    </button>

  </div>

  <div id="keys"></div>

</div>

<div id="panel">

  <div id="panelHeader">

    <button onclick="closePanel()">
      ←
    </button>

    <div id="panelTitle">
      PANEL
    </div>

  </div>

  <div id="panelBody"></div>

</div>

<script>

/* ============================================================
   STATE
   ============================================================ */

let language = 'EN';

let modifiers = {

  CTRL:false,
  SHIFT:false,
  ALT:false,
  WIN:false

};

let pointerActive = false;

let lastX = 0;
let lastY = 0;

let moveQueueX = 0;
let moveQueueY = 0;

let lastMoveSend = 0;

/* ============================================================
   LANGUAGE
   ============================================================ */

async function toggleLanguage() {

  await fetch('/api/language');

  language =
    language === 'EN'
      ? 'RU'
      : 'EN';

  updateLanguageUI();

  renderKeyboard();

}

function updateLanguageUI() {

  document.getElementById('lang')
    .innerText = language;

  document.getElementById('keyboardLang')
    .innerText = language;

}

/* ============================================================
   TOUCHPAD
   ============================================================ */

const touchpad =
  document.getElementById('touchpad');

touchpad.addEventListener(
  'pointerdown',
  function(e) {

    e.preventDefault();

    pointerActive = true;

    lastX = e.clientX;
    lastY = e.clientY;

    touchpad.setPointerCapture(
      e.pointerId
    );

  },
  {passive:false}
);

touchpad.addEventListener(
  'pointermove',
  function(e) {

    e.preventDefault();

    if(!pointerActive)
      return;

    const dx =
      e.clientX - lastX;

    const dy =
      e.clientY - lastY;

    lastX = e.clientX;
    lastY = e.clientY;

    moveQueueX += dx;
    moveQueueY += dy;

    sendMouseMove();

  },
  {passive:false}
);

touchpad.addEventListener(
  'pointerup',
  function(e) {

    e.preventDefault();

    pointerActive = false;

  },
  {passive:false}
);

touchpad.addEventListener(
  'pointercancel',
  function(e) {

    pointerActive = false;

  },
  {passive:false}
);

/*
  Do not allow browser gesture handling.
*/

document.addEventListener(
  'touchmove',
  function(e) {

    if(
      e.target === touchpad ||
      touchpad.contains(e.target) ||
      e.target === scrollTrack ||
      scrollTrack.contains(e.target)
    ) {

      e.preventDefault();

    }

  },
  {passive:false}
);

document.addEventListener(
  'gesturestart',
  function(e) {
    e.preventDefault();
  },
  {passive:false}
);

document.addEventListener(
  'gesturechange',
  function(e) {
    e.preventDefault();
  },
  {passive:false}
);

document.addEventListener(
  'gestureend',
  function(e) {
    e.preventDefault();
  },
  {passive:false}
);

/* ============================================================
   SCROLL JOYSTICK
   ============================================================ */

const scrollTrack = document.getElementById('scrollTrack');
const scrollThumb = document.getElementById('scrollThumb');
let scrollActive = false;
let scrollInterval = null;
let scrollDirection = 0;

function updateScrollPos(e) {
  const rect = scrollTrack.getBoundingClientRect();
  const centerY = rect.height / 2;
  let y = e.clientY - rect.top;
  
  const thumbH = 60;
  const minY = thumbH / 2;
  const maxY = rect.height - thumbH / 2;
  y = Math.max(minY, Math.min(maxY, y));
  
  scrollThumb.style.top = y + 'px';
  
  const diff = y - centerY;
  if (diff > 15) scrollDirection = 1;
  else if (diff < -15) scrollDirection = -1;
  else scrollDirection = 0;
}

scrollTrack.addEventListener('pointerdown', function(e) {
  e.preventDefault();
  scrollActive = true;
  scrollThumb.classList.add('active');
  scrollTrack.setPointerCapture(e.pointerId);
  updateScrollPos(e);
  
  if (!scrollInterval) {
    scrollInterval = setInterval(() => {
      if (scrollActive && scrollDirection !== 0) {
        fetch('/api/scroll?v=' + (scrollDirection * 3), {keepalive: true});
      }
    }, 50);
  }
}, {passive: false});

scrollTrack.addEventListener('pointermove', function(e) {
  if (!scrollActive) return;
  e.preventDefault();
  updateScrollPos(e);
}, {passive: false});

function endScroll(e) {
  if (!scrollActive) return;
  scrollActive = false;
  scrollDirection = 0;
  scrollThumb.classList.remove('active');
  scrollThumb.style.top = '50%';
  
  if (scrollInterval) {
    clearInterval(scrollInterval);
    scrollInterval = null;
  }
}

scrollTrack.addEventListener('pointerup', endScroll, {passive: false});
scrollTrack.addEventListener('pointercancel', endScroll, {passive: false});

/* ============================================================
   MOUSE MOVE
   ============================================================ */

function sendMouseMove() {

  const now =
    performance.now();

  if(
    now - lastMoveSend < 12
  )
    return;

  if(
    Math.abs(moveQueueX) < 1 &&
    Math.abs(moveQueueY) < 1
  )
    return;

  const x =
    Math.round(moveQueueX);

  const y =
    Math.round(moveQueueY);

  moveQueueX -= x;
  moveQueueY -= y;

  lastMoveSend = now;

  fetch(
    '/api/move?x=' +
    x +
    '&y=' +
    y,
    {
      keepalive:true
    }
  );

}

/* ============================================================
   MOUSE BUTTONS
   ============================================================ */

function mouseDown(button) {

  fetch(
    '/api/mousedown?b=' +
    button
  );

}

function mouseUp(button) {

  fetch(
    '/api/mouseup?b=' +
    button
  );

}

/* ============================================================
   KEYBOARD
   ============================================================ */

function renderKeyboard() {

  const root =
    document.getElementById('keys');

  root.innerHTML = '';

  const rowsEN = [
    [
      'ESC',
      'F1','F2','F3','F4',
      'F5','F6','F7','F8',
      'F9','F10','F11','F12'
    ],
    [
      '`','1','2','3','4',
      '5','6','7','8','9','0',
      '-','='
    ],
    [
      'Q','W','E','R','T',
      'Y','U','I','O','P',
      '[',']'
    ],
    [
      'A','S','D','F','G',
      'H','J','K','L',
      ';',"'"
    ],
    [
      'SHIFT','Z','X','C','V',
      'B','N','M',',','.',
      'BACKSPACE'
    ]
  ];

  const rowsRU = [
    [
      'ESC',
      'F1','F2','F3','F4',
      'F5','F6','F7','F8',
      'F9','F10','F11','F12'
    ],
    [
      'Ё','1','2','3','4',
      '5','6','7','8','9','0',
      '-','='
    ],
    [
      'Й','Ц','У','К','Е',
      'Н','Г','Ш','Щ','З',
      'Х','Ъ'
    ],
    [
      'Ф','Ы','В','А','П',
      'Р','О','Л','Д','Ж',
      'Э'
    ],
    [
      'SHIFT','Я','Ч','С','М',
      'И','Т','Ь','Б','Ю',
      'BACKSPACE'
    ]
  ];

  const rows =
    language === 'RU'
      ? rowsRU
      : rowsEN;

  rows.forEach(
    row => {

      const div =
        document.createElement('div');

      div.className =
        'keyRow';

      row.forEach(
        k => {

          div.appendChild(
            createKey(k)
          );

        }
      );

      root.appendChild(div);

    }
  );

  const bottom =
    document.createElement('div');

  bottom.className =
    'keyRow';

  [
    'CTRL',
    'WIN',
    'ALT'
  ].forEach(
    k => {
      const b = createKey(k);
      b.classList.add('modifier');
      bottom.appendChild(b);
    }
  );

  const space = createKey('SPACE');
  space.style.flex = '5';
  bottom.appendChild(space);

  const enter = createKey('ENTER');
  enter.style.flex = '2';
  enter.classList.add('green');
  bottom.appendChild(enter);

  root.appendChild(bottom);

  const utilRow =
    document.createElement('div');

  utilRow.className =
    'keyRow';

  [
    'TAB',
    'DELETE',
    'HOME',
    'END',
    'PAGEUP',
    'PAGEDOWN'
  ].forEach(
    k => {
      utilRow.appendChild(createKey(k));
    }
  );

  root.appendChild(utilRow);

  const arrows =
    document.createElement('div');

  arrows.className =
    'keyRow';

  [
    'LEFT',
    'DOWN',
    'UP',
    'RIGHT'
  ].forEach(
    k => {

      arrows.appendChild(
        createKey(k)
      );

    }
  );

  root.appendChild(arrows);

  updateModifierButtons();

}

/* ============================================================
   CREATE KEY
   ============================================================ */

function createKey(k) {

  const b =
    document.createElement('button');

  b.className =
    'key';

  b.innerText =
    k;

  if(
    ['CTRL','SHIFT','ALT','WIN']
      .includes(k)
  ) {

    b.classList.add(
      'modifier'
    );

    b.onclick =
      () => toggleModifier(k);

  } else {

    b.onclick =
      () => pressNormalKey(k);

  }

  return b;

}

/* ============================================================
   MODIFIERS
   ============================================================ */

function toggleModifier(k) {

  modifiers[k] =
    !modifiers[k];

  updateModifierButtons();

  syncModifiers();

}

function updateModifierButtons() {

  document
    .querySelectorAll(
      '.modifier'
    )
    .forEach(
      b => {

        const k =
          b.innerText;

        if(
          modifiers[k]
        )
          b.classList.add('active');
        else
          b.classList.remove('active');

      }
    );

}

/* ============================================================
   SYNC MODIFIERS
   ============================================================ */

async function syncModifiers() {

  await fetch(
    '/api/modifiers?' +
    'ctrl=' + (modifiers.CTRL ? 1 : 0) +
    '&shift=' + (modifiers.SHIFT ? 1 : 0) +
    '&alt=' + (modifiers.ALT ? 1 : 0) +
    '&win=' + (modifiers.WIN ? 1 : 0)
  );

}

/* ============================================================
   CLEAR MODIFIERS
   ============================================================ */

async function clearModifiers() {

  modifiers.CTRL = false;
  modifiers.SHIFT = false;
  modifiers.ALT = false;
  modifiers.WIN = false;

  updateModifierButtons();

  await fetch(
    '/api/modifiers/clear'
  );

}

/* ============================================================
   NORMAL KEY
   ============================================================ */

async function pressNormalKey(k) {

  await fetch(
    '/api/virtualkey?k=' +
    encodeURIComponent(k) +
    '&lang=' +
    language +
    '&ctrl=' +
    (modifiers.CTRL ? 1 : 0) +
    '&shift=' +
    (modifiers.SHIFT ? 1 : 0) +
    '&alt=' +
    (modifiers.ALT ? 1 : 0) +
    '&win=' +
    (modifiers.WIN ? 1 : 0)
  );

  /*
    One-shot modifiers:
    after a normal key is pressed,
    modifiers are cleared.
  */

  modifiers.CTRL = false;
  modifiers.SHIFT = false;
  modifiers.ALT = false;
  modifiers.WIN = false;

  updateModifierButtons();

}

/* ============================================================
   QUICK COMBO
   ============================================================ */

async function quickCombo(combo) {

  await fetch(
    '/api/combo?c=' +
    encodeURIComponent(combo)
  );

}

/* ============================================================
   KEYBOARD PANEL
   ============================================================ */

function toggleKeyboard() {

  document
    .getElementById('keyboard')
    .classList.toggle('open');

}

/* ============================================================
   PANEL
   ============================================================ */

function closePanel() {

  document
    .getElementById('panel')
    .style.display = 'none';

}

/* ============================================================
   MACROS
   ============================================================ */

async function openMacros() {

  document
    .getElementById('panel')
    .style.display = 'flex';

  document
    .getElementById('panelTitle')
    .innerText = 'MACROS';

  loadMacros();

}

async function loadMacros() {

  const body =
    document.getElementById('panelBody');

  const r =
    await fetch('/api/macros');

  const data =
    await r.json();

  let html = `

    <div class="card">

      <div class="cardTitle">
        MACRO MANAGER
      </div>

      <div class="actionRow">

        <button
          class="green"
          onclick="createMacro()">
          + NEW MACRO
        </button>

      </div>

      <div class="small">

        Создавай макросы из отдельных действий.
        Не требуется запоминать синтаксис.

      </div>

    </div>

  `;

  data.macros.forEach(
    (m,i) => {

      html += `

        <div class="macroRow">

          <button
            class="macroRun"
            onclick="runMacro(${i})">

            ▶ ${escapeHTML(m.name)}

          </button>

          <button
            onclick="editMacro(${i})">
            EDIT
          </button>

          <button
            class="red"
            onclick="deleteMacro(${i})">
            ✕
          </button>

        </div>

      `;

    }
  );

  body.innerHTML =
    html;

}

/* ============================================================
   MACRO EDITOR
   ============================================================ */

async function createMacro() {

  openMacroEditor(
    -1,
    'New Macro',
    ''
  );

}

async function editMacro(id) {

  const r =
    await fetch(
      '/api/macro?id=' + id
    );

  const m =
    await r.json();

  openMacroEditor(
    id,
    m.name,
    m.data
  );

}

/* ============================================================
   MACRO EDITOR UI
   ============================================================ */

function openMacroEditor(
  id,
  name,
  data
) {

  const body =
    document.getElementById(
      'panelBody'
    );

  let actions =
    parseActions(data);

  let html = `

    <div class="card">

      <div class="cardTitle">
        MACRO
      </div>

      <label>
        Name
      </label>

      <input
        id="macroName"
        value="${escapeHTML(name)}">

    </div>

    <div class="card">

      <div class="cardTitle">
        ACTIONS
      </div>

      <div id="actions"></div>

      <div class="grid">

        <button
          class="blue"
          onclick="addAction('KEY')">
          + KEY
        </button>

        <button
          class="blue"
          onclick="addAction('COMBO')">
          + COMBO
        </button>

        <button
          class="blue"
          onclick="addAction('TEXT')">
          + TEXT
        </button>

        <button
          class="blue"
          onclick="addAction('WAIT')">
          + DELAY
        </button>

        <button
          class="blue"
          onclick="addAction('MOVE')">
          + MOUSE
        </button>

        <button
          class="blue"
          onclick="addAction('SCROLL')">
          + SCROLL
        </button>

      </div>

    </div>

    <div class="card">

      <button
        class="green"
        onclick="saveMacro(${id})">
        SAVE MACRO
      </button>

      <button
        onclick="loadMacros()">
        CANCEL
      </button>

    </div>

  `;

  body.innerHTML =
    html;

  window.editorActions =
    actions;

  renderActions();

}

/* ============================================================
   ACTION PARSER
   ============================================================ */

function parseActions(data) {

  if(!data)
    return [];

  return data
    .split('\n')
    .map(
      x => x.trim()
    )
    .filter(
      x => x.length
    )
    .map(
      x => {

        if(
          x.startsWith('WAIT:')
        )
          return {
            type:'WAIT',
            value:x.substring(5)
          };

        if(
          x.startsWith('TYPE:')
        )
          return {
            type:'TEXT',
            value:x.substring(5)
          };

        if(
          x.startsWith('MOVE:')
        )
          return {
            type:'MOVE',
            value:x.substring(5)
          };

        if(
          x.startsWith('SCROLL:')
        )
          return {
            type:'SCROLL',
            value:x.substring(7)
          };

        if(
          x.startsWith('KEY:')
        )
          return {
            type:'KEY',
            value:x.substring(4)
          };

        if(
          x.includes('+')
        )
          return {
            type:'COMBO',
            value:x
          };

        return {
          type:'KEY',
          value:x
        };

      }
    );

}

/* ============================================================
   ACTION RENDER
   ============================================================ */

function renderActions() {

  const root =
    document.getElementById(
      'actions'
    );

  root.innerHTML = '';

  window.editorActions
    .forEach(
      (a,i) => {

        const div =
          document.createElement(
            'div'
          );

        div.className =
          'actionItem';

        div.innerHTML = `

          <div class="actionHeader">

            <span>
              ${i+1}. ${a.type}
            </span>

            <div class="actionButtons">

              <button
                onclick="moveAction(${i},-1)">
                ↑
              </button>

              <button
                onclick="moveAction(${i},1)">
                ↓
              </button>

              <button
                onclick="editAction(${i})">
                ✎
              </button>

              <button
                class="red"
                onclick="deleteAction(${i})">
                ✕
              </button>

            </div>

          </div>

          <div class="small">
            ${escapeHTML(a.value)}
          </div>

        `;

        root.appendChild(div);

      }
    );

}

/* ============================================================
   ADD ACTION
   ============================================================ */

function addAction(type) {

  let value = '';

  if(type === 'KEY')
    value = 'ENTER';

  if(type === 'COMBO')
    value = 'CTRL+C';

  if(type === 'TEXT')
    value = 'Hello';

  if(type === 'WAIT')
    value = '500';

  if(type === 'MOVE')
    value = '100,0';

  if(type === 'SCROLL')
    value = '-3';

  window.editorActions.push({
    type:type,
    value:value
  });

  renderActions();

}

/* ============================================================
   EDIT ACTION
   ============================================================ */

function editAction(i) {

  const a =
    window.editorActions[i];

  let value =
    prompt(
      'Action value:',
      a.value
    );

  if(value === null)
    return;

  a.value =
    value;

  renderActions();

}

/* ============================================================
   DELETE ACTION
   ============================================================ */

function deleteAction(i) {

  window.editorActions
    .splice(i,1);

  renderActions();

}

/* ============================================================
   MOVE ACTION
   ============================================================ */

function moveAction(i,direction) {

  const n =
    i + direction;

  if(n < 0 ||
     n >= window.editorActions.length)
    return;

  const tmp =
    window.editorActions[i];

  window.editorActions[i] =
    window.editorActions[n];

  window.editorActions[n] =
    tmp;

  renderActions();

}

/* ============================================================
   SAVE MACRO
   ============================================================ */

async function saveMacro(id) {

  const name =
    document.getElementById(
      'macroName'
    ).value;

  const data =
    window.editorActions
      .map(
        a => {

          if(a.type === 'WAIT')
            return 'WAIT:' + a.value;

          if(a.type === 'TEXT')
            return 'TYPE:' + a.value;

          if(a.type === 'MOVE')
            return 'MOVE:' + a.value;

          if(a.type === 'SCROLL')
            return 'SCROLL:' + a.value;

          if(a.type === 'KEY')
            return 'KEY:' + a.value;

          if(a.type === 'COMBO')
            return a.value;

          return '';

        }
      )
      .join('\n');

  await fetch(
    '/api/macro/save?id=' +
    id +
    '&name=' +
    encodeURIComponent(name) +
    '&data=' +
    encodeURIComponent(data)
  );

  loadMacros();

}

/* ============================================================
   RUN / DELETE
   ============================================================ */

async function runMacro(i) {

  await fetch(
    '/api/macro/run?id=' + i
  );

}

async function deleteMacro(i) {

  if(
    !confirm(
      'Delete this macro?'
    )
  )
    return;

  await fetch(
    '/api/macro/delete?id=' +
    i
  );

  loadMacros();

}

/* ============================================================
   SETTINGS
   ============================================================ */

async function openSettings() {

  document
    .getElementById('panel')
    .style.display = 'flex';

  document
    .getElementById('panelTitle')
    .innerText = 'SETTINGS';

  const body =
    document.getElementById(
      'panelBody'
    );

  body.innerHTML = `

    <div class="card">

      <div class="cardTitle">
        WI-FI
      </div>

      <label>
        SSID
      </label>

      <input id="ssid">

      <label>
        Password
      </label>

      <input
        id="password"
        type="password">

      <button
        class="green"
        onclick="saveWiFi()">
        SAVE & RESTART
      </button>

    </div>

    <div class="card">

      <div class="cardTitle">
        MOUSE
      </div>

      <label>
        Sensitivity
      </label>

      <input
        id="sensitivity"
        type="range"
        min="1"
        max="6">

    </div>

    <div class="card">

      <div class="cardTitle">
        QUICK ACTIONS
      </div>

      <div class="grid">

        <button onclick="quickCombo('WIN+R')">
          WIN+R
        </button>

        <button onclick="quickCombo('WIN+E')">
          WIN+E
        </button>

        <button onclick="quickCombo('ALT+F4')">
          ALT+F4
        </button>

        <button onclick="quickCombo('CTRL+SHIFT+ESC')">
          TASK MANAGER
        </button>

        <button onclick="quickCombo('CTRL+ALT+DELETE')">
          CTRL+ALT+DEL
        </button>

        <button onclick="quickCombo('CTRL+C')">
          CTRL+C
        </button>

        <button onclick="quickCombo('CTRL+V')">
          CTRL+V
        </button>

      </div>

    </div>

    <div class="card">

      <div class="cardTitle">
        LANGUAGE
      </div>

      <button
        class="blue"
        onclick="toggleLanguage()">
        SHIFT + ALT — ${language}
      </button>

      <div class="small">

        Переключение отправляется одновременно
        на ПК и в интерфейсе ESP32.

      </div>

    </div>

    <div class="card">

      <div class="cardTitle">
        DEVICE
      </div>

      <button
        class="red"
        onclick="factoryReset()">
        FACTORY RESET
      </button>

    </div>

  `;

  const r =
    await fetch(
      '/api/settings'
    );

  const s =
    await r.json();

  document
    .getElementById('ssid')
    .value = s.ssid;

  document
    .getElementById('password')
    .value = s.password;

  document
    .getElementById('sensitivity')
    .value =
      s.sensitivity;

}

/* ============================================================
   SAVE WIFI
   ============================================================ */

async function saveWiFi() {

  const ssid =
    document.getElementById(
      'ssid'
    ).value;

  const password =
    document.getElementById(
      'password'
    ).value;

  await fetch(
    '/api/wifi?ssid=' +
    encodeURIComponent(ssid) +
    '&pass=' +
    encodeURIComponent(password)
  );

  alert(
    'Saved. ESP32 is restarting.'
  );

}

/* ============================================================
   RESET
   ============================================================ */

async function factoryReset() {

  if(
    !confirm(
      'Reset all settings and macros?'
    )
  )
    return;

  await fetch(
    '/api/reset'
  );

}

/* ============================================================
   ESCAPE
   ============================================================ */

function escapeHTML(s) {

  return String(s)
    .replaceAll('&','&amp;')
    .replaceAll('<','&lt;')
    .replaceAll('>','&gt;')
    .replaceAll('"','&quot;')
    .replaceAll("'","&#039;");

}

/* ============================================================
   INIT
   ============================================================ */

renderKeyboard();

updateLanguageUI();

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
    Keyboard.press(
      KEY_LEFT_CTRL
    );

  if(ownShift)
    Keyboard.press(
      KEY_LEFT_SHIFT
    );

  if(ownAlt)
    Keyboard.press(
      KEY_LEFT_ALT
    );

  if(ownWin)
    Keyboard.press(
      KEY_LEFT_GUI
    );

  uint8_t usage = 0;

  if(
    lang == "RU" &&
    key.length() > 0
  ) {

    usage =
      russianUsage(key);

  }

  if(!usage)
    usage =
      usageForKey(key);

  if(usage) {

    Keyboard.pressRaw(
      usage
    );

    delay(30);

    Keyboard.releaseRaw(
      usage
    );

  }

  if(ownWin)
    Keyboard.release(
      KEY_LEFT_GUI
    );

  if(ownAlt)
    Keyboard.release(
      KEY_LEFT_ALT
    );

  if(ownShift)
    Keyboard.release(
      KEY_LEFT_SHIFT
    );

  if(ownCtrl)
    Keyboard.release(
      KEY_LEFT_CTRL
    );

}

// ============================================================
// COMBINATION
// ============================================================

void sendCombination(
  String combo
) {

  combo.trim();
  combo.replace(" ","");

  uint8_t keys[8];

  int count = 0;

  int start = 0;

  while(
    start < combo.length() &&
    count < 8
  ) {

    int pos =
      combo.indexOf(
        '+',
        start
      );

    String p;

    if(pos < 0) {

      p =
        combo.substring(start);

      start =
        combo.length();

    } else {

      p =
        combo.substring(
          start,
          pos
        );

      start =
        pos + 1;

    }

    p.toUpperCase();

    if(
      p == "CTRL" ||
      p == "CONTROL"
    ) {

      keys[count++] =
        KEY_LEFT_CTRL;

    }

    else if(p == "SHIFT") {

      keys[count++] =
        KEY_LEFT_SHIFT;

    }

    else if(p == "ALT") {

      keys[count++] =
        KEY_LEFT_ALT;

    }

    else if(
      p == "WIN" ||
      p == "GUI" ||
      p == "CMD"
    ) {

      keys[count++] =
        KEY_LEFT_GUI;

    }

    else {

      uint8_t u =
        usageForKey(p);

      if(u)
        keys[count++] =
          u;

    }

  }

  if(count == 0)
    return;

  for(
    int i=0;
    i<count;
    i++
  ) {

    Keyboard.pressRaw(
      keys[i]
    );

    delay(15);

  }

  delay(70);

  Keyboard.releaseAll();

}

// ============================================================
// TYPE TEXT
// ============================================================

void typeText(
  String text
) {

  /*
    ASCII is typed through the current
    USB keyboard layout.

    Russian text is handled separately
    through physical RU key positions.
  */

  for(
    size_t i=0;
    i<text.length();
    i++
  ) {

    char c =
      text[i];

    if(c == '\r')
      continue;

    if(c == '\n') {

      Keyboard.write(
        KEY_RETURN
      );

      continue;

    }

    Keyboard.write(
      (uint8_t)c
    );

    delay(3);

  }

}

// ============================================================
// MACRO ACTION
// ============================================================

void executeAction(
  String action
) {

  action.trim();

  if(!action.length())
    return;

  String upper =
    action;

  upper.toUpperCase();

  if(
    upper.startsWith("WAIT:")
  ) {

    int ms =
      action.substring(5)
        .toInt();

    ms =
      constrain(
        ms,
        0,
        30000
      );

    delay(ms);

    return;

  }

  if(
    upper.startsWith("TYPE:")
  ) {

    typeText(
      action.substring(5)
    );

    return;

  }

  if(
    upper.startsWith("KEY:")
  ) {

    sendVirtualKey(
      action.substring(4),
      "EN",
      false,
      false,
      false,
      false
    );

    return;

  }

  if(
    upper == "LMB"
  ) {

    Mouse.click(
      MOUSE_LEFT
    );

    return;

  }

  if(
    upper == "RMB"
  ) {

    Mouse.click(
      MOUSE_RIGHT
    );

    return;

  }

  if(
    upper == "MMB"
  ) {

    Mouse.click(
      MOUSE_MIDDLE
    );

    return;

  }

  if(
    upper.startsWith("MOVE:")
  ) {

    String v =
      action.substring(5);

    int comma =
      v.indexOf(',');

    if(comma >= 0) {

      int x =
        v.substring(
          0,
          comma
        ).toInt();

      int y =
        v.substring(
          comma + 1
        ).toInt();

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

    }

    return;

  }

  if(
    upper.startsWith("SCROLL:")
  ) {

    int v =
      action.substring(7)
        .toInt();

    v =
      constrain(
        v,
        -127,
        127
      );

    Mouse.move(
      0,
      0,
      v
    );

    return;

  }

  if(
    action.indexOf('+') >= 0
  ) {

    sendCombination(
      action
    );

  } else {

    sendVirtualKey(
      action,
      "EN",
      false,
      false,
      false,
      false
    );

  }

}

// ============================================================
// EXECUTE MACRO
// ============================================================

void executeMacro(
  int id
) {

  if(
    id < 0 ||
    id >= macroCount
  )
    return;

  String data =
    macros[id].data;

  int start = 0;

  while(
    start < data.length()
  ) {

    int nl =
      data.indexOf(
        '\n',
        start
      );

    String line;

    if(nl < 0) {

      line =
        data.substring(
          start
        );

      start =
        data.length();

    } else {

      line =
        data.substring(
          start,
          nl
        );

      start =
        nl + 1;

    }

    executeAction(
      line
    );

    yield();

  }

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

void createDefaultMacros() {

  if(macroCount)
    return;

  macros[0] = {
    "Copy",
    "CTRL+C"
  };

  macros[1] = {
    "Paste",
    "CTRL+V"
  };

  macros[2] = {
    "Cut",
    "CTRL+X"
  };

  macros[3] = {
    "Undo",
    "CTRL+Z"
  };

  macros[4] = {
    "Select All",
    "CTRL+A"
  };

  macros[5] = {
    "Task Manager",
    "CTRL+SHIFT+ESC"
  };

  macros[6] = {
    "Run",
    "WIN+R"
  };

  macros[7] = {
    "Explorer",
    "WIN+E"
  };

  macros[8] = {
    "Switch Window",
    "ALT+TAB"
  };

  macros[9] = {
    "Close Window",
    "ALT+F4"
  };
  
  macros[10] = {
    "CMD",
    "WIN+R\nWAIT:300\nTYPE:cmd\nKEY:ENTER"
  };
  
  macros[11] = {
    "PowerShell",
    "WIN+R\nWAIT:300\nTYPE:powershell\nKEY:ENTER"
  };
  
  macros[12] = {
    "IPConfig",
    "WIN+R\nWAIT:300\nTYPE:cmd\nKEY:ENTER\nWAIT:800\nTYPE:ipconfig /all\nKEY:ENTER"
  };
  
  macros[13] = {
    "Ping 8.8.8.8",
    "WIN+R\nWAIT:300\nTYPE:cmd\nKEY:ENTER\nWAIT:800\nTYPE:ping 8.8.8.8 -t\nKEY:ENTER"
  };
  
  macros[14] = {
    "Regedit",
    "WIN+R\nWAIT:300\nTYPE:regedit\nKEY:ENTER"
  };
  
  macros[15] = {
    "Services",
    "WIN+R\nWAIT:300\nTYPE:services.msc\nKEY:ENTER"
  };
  
  macros[16] = {
    "Device Mgr",
    "WIN+R\nWAIT:300\nTYPE:devmgmt.msc\nKEY:ENTER"
  };
  
  macros[17] = {
    "Lock PC",
    "WIN+L"
  };
  
  macros[18] = {
    "Show Desktop",
    "WIN+D"
  };

  macroCount =
    19;

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

  executeMacro(
    id
  );

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

}

// ============================================================
// LOOP
// ============================================================

void loop() {

  dnsServer.processNextRequest();

  server.handleClient();

  delay(1);

}
