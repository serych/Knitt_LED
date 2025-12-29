#include "WebUi.h"

static WebUiDeps D;

static String htmlEscape(const String& s) {
  String out;
  out.reserve(s.length());
  for (char c : s) {
    switch (c) {
      case '&': out += F("&amp;"); break;
      case '<': out += F("&lt;"); break;
      case '>': out += F("&gt;"); break;
      case '"': out += F("&quot;"); break;
      case '\'': out += F("&#39;"); break;
      default: out += c; break;
    }
  }
  return out;
}

// -------- LittleFS helpers ----------
bool loadPatternFile(const String& path, Pattern& p) {
  if (!LittleFS.exists(path)) return false;
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  String json = f.readString();
  f.close();
  return jsonToPattern(json, p);
}

bool savePatternFile(const String& path, const Pattern& p) {
  File f = LittleFS.open(path, "w");
  if (!f) return false;
  f.print(patternToJson(p));
  f.close();
  return true;
}

String listPatternFilesJson() {
  String out = "[";
  File dir = LittleFS.open("/patterns");
  if (!dir || !dir.isDirectory()) return "[]";
  File f = dir.openNextFile();
  bool first = true;
  while (f) {
    if (!f.isDirectory()) {
      if (!first) out += ",";
      first = false;
      out += "\"";
      out += htmlEscape(String(f.name()));
      out += "\"";
    }
    f = dir.openNextFile();
  }
  out += "]";
  return out;
}

// -------- Upload support ----------
static File uploadFile;

static void handleUpload() {
  HTTPUpload& up = D.server->upload();
  if (up.status == UPLOAD_FILE_START) {
    String fname = up.filename;
    fname.replace("..", "");
    fname.replace("\\", "/");
    if (!fname.endsWith(".json")) fname += ".json";
    String path = "/patterns/" + fname;
    uploadFile = LittleFS.open(path, "w");
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) uploadFile.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    if (uploadFile) uploadFile.close();
  }
}

static void handleUploadDone() {
  D.server->send(200, "text/plain", "Upload OK");
}

// -------- API: /api/state --------
// Used by browser polling to stay in sync with hardware buttons.
static void apiState() {
  // report: activeRow, totalPulses, w/h, warn flag, autoAdvance, brightness, colors
  String out = "{";
  out += "\"activeRow\":" + String(D.cfg->activeRow) + ",";
  out += "\"totalPulses\":" + String(D.cfg->totalPulses) + ",";
  out += "\"w\":" + String(D.pattern->w) + ",";
  out += "\"h\":" + String(D.pattern->h) + ",";
  out += "\"warn\":" + String(D.cfg->warnBlinkActive ? "true" : "false") + ",";
  out += "\"autoAdvance\":" + String(D.cfg->autoAdvance ? "true" : "false") + ",";
  out += "\"blinkWarning\":" + String(D.cfg->blinkWarning ? "true" : "false") + ",";
  out += "\"brightness\":" + String(D.cfg->brightness) + ",";
  out += "\"colorActive\":" + String((unsigned long)D.cfg->colorActive) + ",";
  out += "\"colorConfirmed\":" + String((unsigned long)D.cfg->colorConfirmed);
  out += "}";
  D.server->send(200, "application/json", out);
}

// -------- API: list files ----------
static void apiFiles() {
  D.server->send(200, "application/json", listPatternFilesJson());
}

// -------- API: get pattern ----------
static void apiGetPattern() {
  String file = D.server->arg("file");
  if (file.isEmpty()) file = D.cfg->currentPatternFile;

  Pattern p;
  if (!loadPatternFile(file, p)) {
    // create default if missing
    p = *D.pattern;
    savePatternFile(file, p);
  }

  D.cfg->currentPatternFile = file;
  *D.pattern = p;
  if (D.cfg->activeRow < 0) D.cfg->activeRow = 0;
  if (D.cfg->activeRow >= D.pattern->h) D.cfg->activeRow = D.pattern->h - 1;

  String out = "{";
  out += "\"file\":\"" + htmlEscape(file) + "\",";
  out += "\"activeRow\":" + String(D.cfg->activeRow) + ",";
  out += "\"pattern\":" + patternToJson(*D.pattern);
  out += "}";
  D.server->send(200, "application/json", out);
}

// -------- API: save pattern (POST) ----------
static void apiPostPattern() {
  String body = D.server->arg("plain");

  int fpos = body.indexOf("\"file\":\"");
  if (fpos < 0) { D.server->send(400, "text/plain", "Missing file"); return; }
  fpos += 8;
  int fend = body.indexOf("\"", fpos);
  if (fend < 0) { D.server->send(400, "text/plain", "Bad file"); return; }
  String file = body.substring(fpos, fend);

  int ppos = body.indexOf("\"pattern\":");
  if (ppos < 0) { D.server->send(400, "text/plain", "Missing pattern"); return; }
  String pjson = body.substring(ppos + 10);
  int a = pjson.indexOf("{");
  int b = pjson.lastIndexOf("}");
  if (a < 0 || b < 0 || b <= a) { D.server->send(400, "text/plain", "Bad pattern json"); return; }
  pjson = pjson.substring(a, b + 1);

  Pattern p;
  if (!jsonToPattern(pjson, p)) { D.server->send(400, "text/plain", "Invalid pattern"); return; }
  if (!savePatternFile(file, p)) { D.server->send(500, "text/plain", "Write failed"); return; }

  D.cfg->currentPatternFile = file;
  *D.pattern = p;
  for (int i = 0; i < MAX_H; i++) D.rowConfirmed[i] = false;

  D.server->send(200, "application/json", "{\"ok\":true}");
}

// -------- API: delete ----------
static void apiDelete() {
  String body = D.server->arg("plain");
  int fpos = body.indexOf("\"file\":\"");
  if (fpos < 0) { D.server->send(400, "text/plain", "Missing file"); return; }
  fpos += 8;
  int fend = body.indexOf("\"", fpos);
  if (fend < 0) { D.server->send(400, "text/plain", "Bad file"); return; }
  String file = body.substring(fpos, fend);

  if (file == "/patterns/default.json") {
    D.server->send(400, "text/plain", "Refusing to delete default.json");
    return;
  }

  if (LittleFS.exists(file)) LittleFS.remove(file);
  D.server->send(200, "application/json", "{\"ok\":true}");
}

// -------- API: row change ----------
static void apiRow() {
  String body = D.server->arg("plain");
  int dpos = body.indexOf("\"delta\":");
  int delta = 0;
  if (dpos >= 0) delta = body.substring(dpos + 8).toInt();

  D.cfg->warnBlinkActive = false; // user interacted => clear warning
  D.cfg->activeRow += delta;
  if (D.cfg->activeRow < 0) D.cfg->activeRow = 0;
  if (D.cfg->activeRow >= D.pattern->h) D.cfg->activeRow = D.pattern->h - 1;

  D.server->send(200, "application/json", "{\"ok\":true,\"activeRow\":" + String(D.cfg->activeRow) + "}");
}

// -------- API: confirm ----------
static void apiConfirm() {
  D.rowConfirmed[D.cfg->activeRow] = true;
  D.cfg->warnBlinkActive = false;

  if (D.cfg->autoAdvance && D.cfg->activeRow < D.pattern->h - 1) {
    D.cfg->activeRow++;
  }
  D.server->send(200, "application/json", "{\"ok\":true,\"activeRow\":" + String(D.cfg->activeRow) + "}");
}

// -------- API: config (GET/POST) ----------
static void apiGetConfig() {
  String out = "{";
  out += "\"colorActive\":" + String((unsigned long)D.cfg->colorActive) + ",";
  out += "\"colorConfirmed\":" + String((unsigned long)D.cfg->colorConfirmed) + ",";
  out += "\"brightness\":" + String(D.cfg->brightness) + ",";
  out += "\"autoAdvance\":" + String(D.cfg->autoAdvance ? "true" : "false") + ",";
  out += "\"blinkWarning\":" + String(D.cfg->blinkWarning ? "true" : "false");
  out += "}";
  D.server->send(200, "application/json", out);
}

static void apiPostConfig() {
  String body = D.server->arg("plain");

  auto getNum = [&](const char* key, uint32_t& v) -> bool {
    String k = String("\"") + key + "\":";
    int i = body.indexOf(k);
    if (i < 0) return false;
    i += k.length();
    int j = i;
    while (j < (int)body.length() && (isdigit(body[j]) || body[j]=='-')) j++;
    v = (uint32_t)body.substring(i, j).toInt();
    return true;
  };
  auto getBool = [&](const char* key, bool& v) -> bool {
    String k = String("\"") + key + "\":";
    int i = body.indexOf(k);
    if (i < 0) return false;
    i += k.length();
    String t = body.substring(i, min(i + 5, (int)body.length()));
    if (t.startsWith("true")) v = true;
    else if (t.startsWith("false")) v = false;
    else return false;
    return true;
  };

  uint32_t ca, cc, br;
  bool aa, bw;

  if (getNum("colorActive", ca)) D.cfg->colorActive = ca;
  if (getNum("colorConfirmed", cc)) D.cfg->colorConfirmed = cc;
  if (getNum("brightness", br)) D.cfg->brightness = (uint8_t)constrain((int)br, 0, 255);
  if (getBool("autoAdvance", aa)) D.cfg->autoAdvance = aa;
  if (getBool("blinkWarning", bw)) D.cfg->blinkWarning = bw;

  D.server->send(200, "application/json", "{\"ok\":true}");
}

// -------- download ----------
static void handleDownload() {
  String file = D.server->arg("file");
  if (file.isEmpty()) { D.server->send(400, "text/plain", "Missing file"); return; }
  if (!LittleFS.exists(file)) { D.server->send(404, "text/plain", "Not found"); return; }

  File f = LittleFS.open(file, "r");
  D.server->sendHeader("Content-Disposition", "attachment; filename=\"pattern.json\"");
  D.server->streamFile(f, "application/json");
  f.close();
}

// -------- UI HTML ----------
static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head>
<meta charset="utf-8"/><meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>KnittLED</title>
<style>
body{font-family:system-ui,Arial;margin:16px;max-width:980px}
h1{font-size:22px;margin:0 0 8px}
.small{color:#555;font-size:13px}
.row{display:flex;gap:12px;flex-wrap:wrap}
.card{border:1px solid #ddd;border-radius:14px;padding:12px;flex:1;min-width:300px}
button,input,select{font:inherit}
button{padding:10px 12px;border:0;border-radius:12px;background:#111;color:#fff;font-weight:650}
button.secondary{background:#666}
canvas{touch-action:manipulation;border-radius:12px;border:1px solid #ccc}
.controls{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
.pill{display:inline-block;padding:6px 10px;border-radius:999px;background:#f2f2f2}
label{display:block;margin-top:10px}
input,select{width:100%;padding:10px;border-radius:12px;border:1px solid #ccc}
.warn{background:#fff0f0;border-color:#f2b6b6}
</style>
</head><body>
<h1>KnittLED</h1>
<div class="small" id="status">Loading...</div>

<div class="row" style="margin-top:12px">
  <div class="card" id="gridCard">
    <div class="controls">
      <span class="pill" id="modePill">EDIT</span>
      <button class="secondary" id="btnEdit">Edit</button>
      <button class="secondary" id="btnKnit">Knit</button>
      <button class="secondary" id="btnReload">Reload</button>
      <button class="secondary" id="btnConfig">Config</button>
    </div>

    <div style="margin-top:10px;overflow:auto">
      <canvas id="grid" width="600" height="600"></canvas>
    </div>

    <div class="small" style="margin-top:10px">
      Needle #1 is the <b>rightmost</b>. (LED0 should be rightmost too.)
    </div>
  </div>

  <div class="card" id="panelCard">
    <div class="controls">
      <span class="pill" id="rowPill">Row: --</span>
      <span class="pill" id="totPill">Tot: --</span>
      <span class="pill" id="warnPill" style="display:none">WARNING</span>
    </div>

    <label>Stored patterns</label>
    <select id="fileList"></select>
    <div class="controls" style="margin-top:10px">
      <button id="btnLoad">Load</button>
      <button id="btnSave">Save</button>
      <button id="btnDownload">Download</button>
    </div>

    <label>New file name</label>
    <input id="newName" placeholder="snowflake.json"/>
    <div class="controls" style="margin-top:10px">
      <button class="secondary" id="btnNew">Create</button>
      <button class="secondary" id="btnUpload">Upload</button>
      <input type="file" id="uploadFile" accept=".json"/>
      <button class="secondary" id="btnDelete">Delete</button>
    </div>

    <hr style="margin:14px 0;border:0;border-top:1px solid #eee"/>

    <div><b>Size</b> <span class="small">(max 12×24)</span></div>
    <div class="controls" style="margin-top:10px">
      <div style="flex:1">
        <label class="small">Width</label>
        <input id="w" type="number" min="1" max="12" value="12"/>
      </div>
      <div style="flex:1">
        <label class="small">Height</label>
        <input id="h" type="number" min="1" max="24" value="24"/>
      </div>
      <button class="secondary" id="btnResize">Resize</button>
    </div>

    <hr style="margin:14px 0;border:0;border-top:1px solid #eee"/>

    <div><b>Knitting controls</b></div>
    <div class="controls" style="margin-top:10px">
      <button class="secondary" id="btnPrevRow">Row -</button>
      <button class="secondary" id="btnNextRow">Row +</button>
      <button id="btnConfirm">Confirm</button>
    </div>

    <div class="small" style="margin-top:10px">
      In knit mode, active row is highlighted. Page auto-updates from hardware buttons.
    </div>
  </div>
</div>

<!-- Config modal -->
<div id="cfg" style="display:none;position:fixed;inset:0;background:rgba(0,0,0,.35);padding:16px">
  <div class="card" style="max-width:520px;margin:40px auto;background:#fff">
    <div class="controls" style="justify-content:space-between">
      <b>Config</b>
      <button class="secondary" id="cfgClose">Close</button>
    </div>

    <label>Active color</label>
    <input id="cfgActive" type="color" value="#00ff00"/>

    <label>Confirmed color</label>
    <input id="cfgConfirmed" type="color" value="#0000ff"/>

    <label>Brightness (0..255)</label>
    <input id="cfgBright" type="range" min="0" max="255" value="64"/>

    <div class="controls" style="margin-top:10px">
      <label style="display:flex;gap:10px;align-items:center;margin:0">
        <input id="cfgAA" type="checkbox"/> Auto-advance on confirm
      </label>
    </div>

    <div class="controls" style="margin-top:10px">
      <label style="display:flex;gap:10px;align-items:center;margin:0">
        <input id="cfgBW" type="checkbox"/> Blink warning on carriage without confirm
      </label>
    </div>

    <div class="controls" style="margin-top:14px">
      <button id="cfgSave">Save config</button>
    </div>

    <div class="small" style="margin-top:10px">
      Blink-warning: if carriage triggers while current row is not confirmed, LEDs blink until you confirm (or change row).
    </div>
  </div>
</div>

<script>
let mode="edit";
let pat={name:"",w:12,h:24,pixels:[]};
let activeRow=0;
let totalPulses=0;
let warn=false;

const c=document.getElementById("grid");
const ctx=c.getContext("2d");

function setStatus(t){document.getElementById("status").textContent=t;}
function setMode(m){mode=m;document.getElementById("modePill").textContent=(m==="edit"?"EDIT":"KNIT"); draw();}
document.getElementById("btnEdit").onclick=()=>setMode("edit");
document.getElementById("btnKnit").onclick=()=>setMode("knit");

function ensurePixels(){
  if(!pat.pixels||pat.pixels.length!==pat.h){
    pat.pixels=[];
    for(let r=0;r<pat.h;r++) pat.pixels.push("0".repeat(pat.w));
  } else {
    pat.pixels=pat.pixels.map(row=>{
      row=row.replace(/[^01]/g,"");
      if(row.length<pat.w) row=row+"0".repeat(pat.w-row.length);
      if(row.length>pat.w) row=row.slice(0,pat.w);
      return row;
    });
  }
}

function draw(){
  ensurePixels();
  const size=24;
  c.width=pat.w*size; c.height=pat.h*size;
  ctx.clearRect(0,0,c.width,c.height);

  for(let r=0;r<pat.h;r++){
    for(let col=0;col<pat.w;col++){
      const v=pat.pixels[r][col]==="1";
      const x=col*size,y=r*size;

      if(mode==="knit" && r===activeRow){
        ctx.fillStyle="#fff7d6"; ctx.fillRect(x,y,size,size);
        ctx.fillStyle=v?"#111":"#fff"; ctx.fillRect(x+4,y+4,size-8,size-8);
      } else {
        ctx.fillStyle=v?"#111":"#fff"; ctx.fillRect(x,y,size,size);
      }
      ctx.strokeStyle="#ccc"; ctx.strokeRect(x,y,size,size);
    }
  }
}

function toggleCell(clientX,clientY){
  if(mode!=="edit") return;
  const rect=c.getBoundingClientRect();
  const x=clientX-rect.left, y=clientY-rect.top;
  const size=c.width/pat.w;
  const col=Math.floor(x/size), row=Math.floor(y/size);
  if(row<0||row>=pat.h||col<0||col>=pat.w) return;
  let s=pat.pixels[row].split("");
  s[col]=s[col]==="1"?"0":"1";
  pat.pixels[row]=s.join("");
  draw();
}
c.addEventListener("click",e=>toggleCell(e.clientX,e.clientY));
c.addEventListener("touchstart",e=>{const t=e.touches[0];toggleCell(t.clientX,t.clientY);},{passive:true});

async function apiGET(u){const r=await fetch(u); if(!r.ok) throw new Error(await r.text()); return r.json();}
async function apiPOST(u,o){const r=await fetch(u,{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(o)}); if(!r.ok) throw new Error(await r.text()); return r.json();}

async function refreshFiles(){
  const list=await apiGET("/api/files");
  const sel=document.getElementById("fileList");
  sel.innerHTML="";
  list.forEach(f=>{const o=document.createElement("option"); o.value=f; o.textContent=f; sel.appendChild(o);});
}
async function loadSelected(){
  const file=document.getElementById("fileList").value;
  const data=await apiGET("/api/pattern?file="+encodeURIComponent(file));
  pat=data.pattern;
  activeRow=data.activeRow||0;
  document.getElementById("w").value=pat.w;
  document.getElementById("h").value=pat.h;
  draw();
  setStatus("Loaded "+file);
}
async function saveSelected(){
  const file=document.getElementById("fileList").value;
  await apiPOST("/api/pattern",{file,pattern:pat});
  setStatus("Saved "+file);
  await refreshFiles();
  document.getElementById("fileList").value=file;
}
document.getElementById("btnLoad").onclick=loadSelected;
document.getElementById("btnSave").onclick=saveSelected;

document.getElementById("btnReload").onclick=async()=>{await refreshFiles(); await loadSelected();};

document.getElementById("btnNew").onclick=async()=>{
  const name=document.getElementById("newName").value.trim();
  if(!name) return alert("Enter a file name");
  const file="/patterns/"+name.replace(/[^a-zA-Z0-9._-]/g,"_");
  await apiPOST("/api/pattern",{file,pattern:pat});
  await refreshFiles();
  document.getElementById("fileList").value=file;
  setStatus("Created "+file);
};

document.getElementById("btnDelete").onclick=async()=>{
  const file=document.getElementById("fileList").value;
  if(!confirm("Delete "+file+" ?")) return;
  await apiPOST("/api/delete",{file});
  await refreshFiles();
  setStatus("Deleted "+file);
};

document.getElementById("btnResize").onclick=()=>{
  let w=parseInt(document.getElementById("w").value,10);
  let h=parseInt(document.getElementById("h").value,10);
  w=Math.max(1,Math.min(12,w)); h=Math.max(1,Math.min(24,h));
  const newPix=[];
  for(let r=0;r<h;r++){
    let row=(pat.pixels[r]||"0".repeat(pat.w));
    row=row.slice(0,w);
    if(row.length<w) row=row+"0".repeat(w-row.length);
    newPix.push(row);
  }
  pat.w=w; pat.h=h; pat.pixels=newPix;
  draw();
};

document.getElementById("btnPrevRow").onclick=async()=>{const d=await apiPOST("/api/row",{delta:-1}); activeRow=d.activeRow; draw();};
document.getElementById("btnNextRow").onclick=async()=>{const d=await apiPOST("/api/row",{delta:+1}); activeRow=d.activeRow; draw();};
document.getElementById("btnConfirm").onclick=async()=>{const d=await apiPOST("/api/confirm",{}); activeRow=d.activeRow; draw();};

document.getElementById("btnDownload").onclick=()=>{
  const file=document.getElementById("fileList").value;
  window.location="/download?file="+encodeURIComponent(file);
};

document.getElementById("btnUpload").onclick=async()=>{
  const inp=document.getElementById("uploadFile");
  if(!inp.files.length) return alert("Choose a file first");
  const f=inp.files[0];
  const fd=new FormData(); fd.append("upload",f,f.name);
  const r=await fetch("/upload",{method:"POST",body:fd});
  if(!r.ok) return alert("Upload failed: "+await r.text());
  setStatus(await r.text());
  await refreshFiles();
};

// ---- Config modal ----
function intToHexColor(v){
  // v is 0xRRGGBB
  const r=(v>>16)&255, g=(v>>8)&255, b=v&255;
  return "#"+[r,g,b].map(x=>x.toString(16).padStart(2,"0")).join("");
}
function hexToInt(s){
  // "#rrggbb" -> int
  const v=parseInt(s.slice(1),16);
  return v;
}
document.getElementById("btnConfig").onclick=async()=>{
  const cfg=await apiGET("/api/config");
  document.getElementById("cfgActive").value=intToHexColor(cfg.colorActive);
  document.getElementById("cfgConfirmed").value=intToHexColor(cfg.colorConfirmed);
  document.getElementById("cfgBright").value=cfg.brightness;
  document.getElementById("cfgAA").checked=!!cfg.autoAdvance;
  document.getElementById("cfgBW").checked=!!cfg.blinkWarning;
  document.getElementById("cfg").style.display="block";
};
document.getElementById("cfgClose").onclick=()=>{document.getElementById("cfg").style.display="none";};
document.getElementById("cfgSave").onclick=async()=>{
  const payload={
    colorActive: hexToInt(document.getElementById("cfgActive").value),
    colorConfirmed: hexToInt(document.getElementById("cfgConfirmed").value),
    brightness: parseInt(document.getElementById("cfgBright").value,10),
    autoAdvance: document.getElementById("cfgAA").checked,
    blinkWarning: document.getElementById("cfgBW").checked
  };
  await apiPOST("/api/config", payload);
  setStatus("Config saved");
  document.getElementById("cfg").style.display="none";
};

// ---- state polling ----
function renderState(){
  document.getElementById("rowPill").textContent = "Row: " + String(activeRow+1).padStart(2,"0") + "/" + String(pat.h).padStart(2,"0");
  document.getElementById("totPill").textContent = "Tot: " + totalPulses;
  const wp=document.getElementById("warnPill");
  const gc=document.getElementById("gridCard");
  if(warn){
    wp.style.display="inline-block";
    gc.classList.add("warn");
  } else {
    wp.style.display="none";
    gc.classList.remove("warn");
  }
}

async function poll(){
  try{
    const s=await apiGET("/api/state");
    // if ESP activeRow changed via hardware, update UI
    activeRow=s.activeRow;
    totalPulses=s.totalPulses;
    warn=!!s.warn;
    renderState();
    if(mode==="knit") draw();
  }catch(e){}
  setTimeout(poll, 350);
}

async function init(){
  await refreshFiles();
  await loadSelected();
  setMode("edit");
  renderState();
  poll();
  setStatus("Ready.");
}
init().catch(e=>setStatus("Error: "+e.message));
</script>
</body></html>
)HTML";

static void sendIndex() {
  D.server->send(200, "text/html; charset=utf-8", FPSTR(INDEX_HTML));
}

void webuiBegin(WebUiDeps deps) {
  D = deps;

  D.server->on("/", HTTP_GET, sendIndex);

  D.server->on("/api/files", HTTP_GET, apiFiles);
  D.server->on("/api/pattern", HTTP_GET, apiGetPattern);
  D.server->on("/api/pattern", HTTP_POST, apiPostPattern);
  D.server->on("/api/delete", HTTP_POST, apiDelete);

  D.server->on("/api/row", HTTP_POST, apiRow);
  D.server->on("/api/confirm", HTTP_POST, apiConfirm);

  D.server->on("/api/state", HTTP_GET, apiState);

  D.server->on("/api/config", HTTP_GET, apiGetConfig);
  D.server->on("/api/config", HTTP_POST, apiPostConfig);

  D.server->on("/download", HTTP_GET, handleDownload);
  D.server->on("/upload", HTTP_POST, handleUploadDone, handleUpload);

  D.server->onNotFound([]() { D.server->sendHeader("Location", "/"); D.server->send(302); });
}
