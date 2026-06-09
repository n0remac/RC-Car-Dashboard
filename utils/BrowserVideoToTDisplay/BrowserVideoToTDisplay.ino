#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>

// ----------------------
// WiFi AP config
// ----------------------
const char* AP_SSID = "TDisplayVideo";
const char* AP_PASSWORD = "videoplay123";

// ----------------------
// Screen config
// ----------------------
static const int SCREEN_W = 240;
static const int SCREEN_H = 135;
static const int SCREEN_BACKLIGHT_PIN = 4;
static const int SCREEN_BRIGHTNESS_PERCENT = 80;

// ----------------------
// JPEG frame buffer
// ----------------------
#define MAX_JPEG_SIZE 70*1024
uint8_t jpegBuffer[MAX_JPEG_SIZE];
size_t jpegLength = 0;
bool frameReady = false;
bool videoMode = false;
unsigned long receivedFrames = 0;
unsigned long droppedFrames = 0;

// ----------------------
// TFT
// ----------------------
TFT_eSPI tft = TFT_eSPI();

// ----------------------
// WebServer & WebSocket
// ----------------------
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// ----------------------
// JPEG draw callback
// ----------------------
bool tftJpegOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap){
  if(y>=tft.height()) return false;
  if(x>=tft.width()) return false;
  if((x+w)>tft.width()) w=tft.width()-x;
  if((y+h)>tft.height()) h=tft.height()-y;

  tft.pushImage(x,y,w,h,bitmap);
  return true;
}

// ----------------------
// WebSocket handler
// ----------------------
void webSocketEvent(uint8_t clientNum, WStype_t type, uint8_t *payload, size_t length){
  switch(type){
    case WStype_CONNECTED:
      Serial.printf("WS client %u connected\n", clientNum);
      videoMode=true;
      tft.fillScreen(TFT_BLACK);
      break;

    case WStype_DISCONNECTED:
      Serial.printf("WS client %u disconnected\n", clientNum);
      videoMode=false;
      break;

    case WStype_BIN:
      if(!videoMode) return;
      if(length>MAX_JPEG_SIZE){
        droppedFrames++;
        return;
      }
      memcpy(jpegBuffer,payload,length);
      jpegLength=length;
      frameReady=true;
      receivedFrames++;
      break;

    default: break;
  }
}

// ----------------------
// HTML page
// ----------------------
String htmlPage(){
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>T-Display Video</title>
<style>
body{background:#090909;color:#f2f2f2;font-family:Arial;padding:20px;}
button{padding:14px;border:none;border-radius:10px;font-size:16px;margin:4px;background:#2f80ed;color:white;cursor:pointer;}
canvas{image-rendering:pixelated;border-radius:12px;border:1px solid #333;background:#000;}
input[type="range"]{width:100%;}
.value-label{margin:2px 0;font-size:14px;}
</style>
</head>
<body>
<h1>T-Display Video</h1>
<input id="videoFile" type="file" accept="video/*">
<video id="sourceVideo" controls playsinline muted></video>
<canvas id="frameCanvas" width="240" height="135"></canvas>

<div>
<div class="value-label">Target FPS: <span id="fpsValue">8</span></div>
<input id="fpsSlider" type="range" min="1" max="15" step="1" value="8">

<div class="value-label">JPEG Quality: <span id="qualityValue">0.55</span></div>
<input id="qualitySlider" type="range" min="0.2" max="0.9" step="0.05" value="0.55">
</div>

<div>
<button id="startBtn" disabled>Start Streaming</button>
<button id="stopBtn" disabled>Stop Streaming</button>
</div>

<script>
const videoFile=document.getElementById('videoFile');
const sourceVideo=document.getElementById('sourceVideo');
const canvas=document.getElementById('frameCanvas');
const ctx=canvas.getContext('2d',{alpha:false});
const fpsSlider=document.getElementById('fpsSlider');
const fpsValue=document.getElementById('fpsValue');
const qualitySlider=document.getElementById('qualitySlider');
const qualityValue=document.getElementById('qualityValue');
const startBtn=document.getElementById('startBtn');
const stopBtn=document.getElementById('stopBtn');

let ws;
let streaming=false;
let objectUrl=null;
let lastSend=0;

videoFile.addEventListener('change',()=>{
  const file=videoFile.files[0];
  if(!file) return;
  if(objectUrl) URL.revokeObjectURL(objectUrl);
  objectUrl=URL.createObjectURL(file);
  sourceVideo.src=objectUrl;
  sourceVideo.load();
  startBtn.disabled=false;
});

fpsSlider.addEventListener('input',()=>{fpsValue.textContent=fpsSlider.value;});
qualitySlider.addEventListener('input',()=>{qualityValue.textContent=parseFloat(qualitySlider.value).toFixed(2);});

startBtn.addEventListener('click',()=>{
  ws=new WebSocket('ws://'+location.hostname+':81/');
  ws.binaryType='arraybuffer';
  ws.onopen=()=>{streaming=true;sourceVideo.play();requestAnimationFrame(streamLoop);};
  startBtn.disabled=true; stopBtn.disabled=false;
});

stopBtn.addEventListener('click',()=>{
  streaming=false;
  if(ws) ws.close();
  startBtn.disabled=false; stopBtn.disabled=true;
});

function drawVideo(){
  const vw=sourceVideo.videoWidth;
  const vh=sourceVideo.videoHeight;
  if(!vw||!vh) return;
  ctx.fillStyle='#000'; ctx.fillRect(0,0,canvas.width,canvas.height);
  const scale=Math.min(canvas.width/vw,canvas.height/vh);
  const dw=Math.round(vw*scale); const dh=Math.round(vh*scale);
  const dx=Math.floor((canvas.width-dw)/2); const dy=Math.floor((canvas.height-dh)/2);
  ctx.drawImage(sourceVideo,dx,dy,dw,dh);
}

function streamLoop(timestamp){
  if(!streaming || !ws || ws.readyState!==1) return;
  const fps=parseInt(fpsSlider.value);
  const interval=1000/fps;
  if(timestamp-lastSend>=interval){
    lastSend=timestamp;
    drawVideo();
    canvas.toBlob(blob=>{
      if(blob && ws.readyState===1){
        blob.arrayBuffer().then(buf=>ws.send(buf));
      }
    },'image/jpeg',parseFloat(qualitySlider.value));
  }
  requestAnimationFrame(streamLoop);
}
</script>
</body>
</html>
)rawliteral";
}

// ----------------------
// Server routes
// ----------------------
void handleRoot(){ server.send(200,"text/html",htmlPage()); }

void handleStatus(){
  String json="{\"frames\":"+String(receivedFrames)+",\"drops\":"+String(droppedFrames)+",\"video\":"+(videoMode?"true":"false")+"}";
  server.send(200,"application/json",json);
}

// ----------------------
// Draw latest frame
// ----------------------
void renderLatestFrame(){
  if(videoMode && frameReady){
    frameReady=false;
    if(jpegLength>0){
      TJpgDec.drawJpg(0,0,jpegBuffer,jpegLength);
    }
  }
}

// ----------------------
// Setup / loop
// ----------------------
void setup(){
  Serial.begin(115200);
  pinMode(SCREEN_BACKLIGHT_PIN,OUTPUT);
  analogWrite(SCREEN_BACKLIGHT_PIN,map(SCREEN_BRIGHTNESS_PERCENT,0,100,0,255));

  tft.init(); tft.setRotation(1); tft.setSwapBytes(true);
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE,TFT_BLACK);
  tft.drawString("Starting AP...",SCREEN_W/2,SCREEN_H/2,2);
  tft.setTextDatum(TL_DATUM);

  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(tftJpegOutput);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID,AP_PASSWORD);
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.begin();

  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE,TFT_BLACK);
  tft.drawString("Connect WiFi:",SCREEN_W/2,35,2);
  tft.drawString(AP_SSID,SCREEN_W/2,58,2);
  tft.drawString("Open 192.168.4.1",SCREEN_W/2,85,2);
  tft.setTextDatum(TL_DATUM);
}

void loop(){
  server.handleClient();
  webSocket.loop();
  renderLatestFrame();
}