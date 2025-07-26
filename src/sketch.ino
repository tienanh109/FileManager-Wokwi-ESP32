#include <WiFi.h>
#include <WebServer.h>
#include <SD.h>
#include <SPI.h>

#define SD_CS 5

const char* ssid = "YOUR_SSID"; // if it was wokwi it would be Wokwi-GUEST
const char* password = "YOUR_PASSWORD"; // leave empty if it was 

WebServer server(80);
File uploadFile;

// CODE BY tienanh109
void handleRoot() {
  String html = R"rawliteral(
  <!DOCTYPE html><html><head>
  <title>ESP32 File Manager</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: 'Segoe UI', sans-serif; background: #f0f0f0; color: #111; padding: 20px; }
    h1 { text-align: center; color: #4CAF50; }
    .tabs { display: flex; justify-content: center; margin-bottom: 20px; }
    .tab-button {
      padding: 10px 20px;
      background: #eee;
      border: none;
      border-bottom: 2px solid transparent;
      cursor: pointer;
      font-weight: bold;
    }
    .tab-button.active {
      border-bottom: 3px solid #4CAF50;
      background: #fff;
    }
    .tab-content { display: none; }
    .tab-content.active { display: block; animation: fadein 0.3s; }
    @keyframes fadein {
      from { opacity: 0; } to { opacity: 1; }
    }
    .file-list { display: flex; flex-direction: column; gap: 10px; }
    .file-card {
      background: #fff;
      padding: 15px;
      border-radius: 8px;
      box-shadow: 0 2px 5px rgba(0,0,0,0.1);
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .file-actions a {
      margin-left: 12px;
      color: #03A9F4;
      text-decoration: none;
    }
    .file-actions a:hover { text-decoration: underline; }
    .upload-box {
      background: #fff;
      padding: 20px;
      border-radius: 10px;
      box-shadow: 0 2px 5px rgba(0,0,0,0.1);
    }
    input[type="file"] { margin-top: 10px; }
    input[type="submit"] {
      margin-top: 10px;
      padding: 8px 20px;
      background: #4CAF50;
      color: white;
      border: none;
      border-radius: 6px;
      cursor: pointer;
    }
    input[type="submit"]:hover { background: #45a049; }
  </style>
  </head><body>
  <h1>ESP32 SD File Manager</h1>
  <div class="tabs">
    <button class="tab-button active" onclick="showTab(0)">File Manager</button>
    <button class="tab-button" onclick="showTab(1)">Upload</button>
  </div>

  <div class="tab-content active" id="tab-0">
    <div class="file-list">
  )rawliteral";

  File root = SD.open("/");
  File file = root.openNextFile();
  while (file) {
    String fullName = String(file.name());
    String shortName = fullName.startsWith("/") ? fullName.substring(1) : fullName;

    html += "<div class='file-card'><span>" + shortName + "</span><div class='file-actions'>";
    html += "<a href='/view?name=" + shortName + "'>View</a>";
    html += "<a href='/download?name=" + shortName + "'>Download</a>";
    html += "<a href='/delete?name=" + shortName + "' onclick=\"return confirm('Delete this file?');\">Delete</a>";
    html += "</div></div>";
    file = root.openNextFile();
  }

  html += R"rawliteral(
    </div>
  </div>

  <div class="tab-content" id="tab-1">
    <div class="upload-box">
      <h3>Upload File</h3>
      <form method="POST" action="/upload" enctype="multipart/form-data">
        <input type="file" name="upload"><br>
        <input type="submit" value="Upload">
      </form>
    </div>
  </div>

  <script>
    function showTab(index) {
      let tabs = document.querySelectorAll('.tab-content');
      let buttons = document.querySelectorAll('.tab-button');
      tabs.forEach(t => t.classList.remove('active'));
      buttons.forEach(b => b.classList.remove('active'));
      tabs[index].classList.add('active');
      buttons[index].classList.add('active');
    }
  </script>
  </body></html>
  )rawliteral";

  server.send(200, "text/html", html);
}


void handleView() {
  String filename = "/" + server.arg("name");
  if (!SD.exists(filename)) {
    server.send(404, "text/plain", "File not found.");
    return;
  }

  File file = SD.open(filename);
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>View</title></head><body style='background:#1e1e1e; color:#eee; font-family:monospace; padding:20px'>";
  html += "<h2>📄 " + filename + "</h2><pre style='background:#111; padding:20px; border-radius:8px; overflow:auto;'>";

  while (file.available()) html += (char)file.read();
  html += "</pre><br><a href='/' style='color:#03A9F4'>⬅ Back</a></body></html>";
  file.close();
  server.send(200, "text/html", html);
}


void handleDownload() {
  String shortName = server.arg("name");
  String filename = "/" + shortName;

  if (!SD.exists(filename)) {
    server.send(404, "text/plain", "File not found.");
    return;
  }

  File file = SD.open(filename, FILE_READ);
  server.sendHeader("Content-Type", "application/octet-stream");
  server.sendHeader("Content-Disposition", "attachment; filename=\"" + shortName + "\"");
  server.streamFile(file, "application/octet-stream");
  file.close();
}


void handleDelete() {
  String filename = "/" + server.arg("name");
  if (!SD.exists(filename)) {
    server.send(404, "text/plain", "File not found.");
    return;
  }

  SD.remove(filename);
  server.sendHeader("Location", "/", true);
  server.send(303);
}


void handleUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = "/" + upload.filename;
    uploadFile = SD.open(filename, FILE_WRITE);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) uploadFile.close();
  }
}


void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.print("🔌 Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected. IP: " + WiFi.localIP().toString());

  if (!SD.begin(SD_CS)) {
    Serial.println("❌ SD Card mount failed!");
    while (true);
  }
  Serial.println("✅ SD Card initialized");

  server.on("/", handleRoot);
  server.on("/view", handleView);
  server.on("/download", handleDownload);
  server.on("/delete", handleDelete);
  server.on("/upload", HTTP_POST, []() {
    server.sendHeader("Location", "/", true);
    server.send(303);
  }, handleUpload);

  server.begin();
  Serial.println("🚀 Web server running.");
}

void loop() {
  server.handleClient();
}
