// #include <WebServer.h>
// #include <WiFi.h>
// #include <esp32cam.h>
// #include <SPI.h>
// #include <Wire.h>
 
// const char* WIFI_SSID = "vierobot";
// const char* WIFI_PASS = "robotviet";
 
// WebServer server(80);
 
 
// static auto loRes = esp32cam::Resolution::find(320, 240);
// static auto midRes = esp32cam::Resolution::find(350, 530);
// static auto hiRes = esp32cam::Resolution::find(800, 600);
// void serveJpg()
// {
//   auto frame = esp32cam::capture();
//   if (frame == nullptr) {
//     Serial.println("CAPTURE FAIL");
//     server.send(503, "", "");
//     return;
//   }
//   Serial.printf("CAPTURE OK %dx%d %db\n", frame->getWidth(), frame->getHeight(),
//                 static_cast<int>(frame->size()));
 
//   server.setContentLength(frame->size());
//   server.send(200, "image/jpeg");
//   WiFiClient client = server.client();
//   frame->writeTo(client);
// }
 
// void handleJpgLo()
// {
//   if (!esp32cam::Camera.changeResolution(loRes)) {
//     Serial.println("SET-LO-RES FAIL");
//   }
//   serveJpg();
// }
 
// void handleJpgHi()
// {
//   if (!esp32cam::Camera.changeResolution(hiRes)) {
//     Serial.println("SET-HI-RES FAIL");
//   }
//   serveJpg();
// }
 
// void handleJpgMid()
// {
//   if (!esp32cam::Camera.changeResolution(midRes)) {
//     Serial.println("SET-MID-RES FAIL");
//   }
//   serveJpg();
// }
 
 
// void  setup(){
//   Serial.begin(115200);
//   Serial.println();
//   {
//     using namespace esp32cam;
//     Config cfg;
//     cfg.setPins(pins::AiThinker);
//     cfg.setResolution(hiRes);
//     cfg.setBufferCount(2);
//     cfg.setJpeg(80);
 
//     bool ok = Camera.begin(cfg);
//     Serial.println(ok ? "CAMERA OK" : "CAMERA FAIL");
//   }
// //   WiFi.persistent(false);
// //   WiFi.mode(WIFI_STA);
// //   WiFi.begin(WIFI_SSID, WIFI_PASS);
//   WiFi.softAP(WIFI_SSID, WIFI_PASS);
//   Serial.println("Access Point created.");
// //   while (WiFi.status() != WL_CONNECTED) {
// //     delay(500);
// //   }
//   Serial.println("");
//   Serial.println("APIP address: ");
//   Serial.print("http://");
//   Serial.println(WiFi.softAPIP());
//   Serial.println(WiFi.localIP());
//   Serial.println("  /cam-lo.jpg");
//   Serial.println("  /cam-hi.jpg");
//   Serial.println("  /cam-mid.jpg");
 
//   server.on("/cam-lo.jpg", handleJpgLo);
//   server.on("/cam-hi.jpg", handleJpgHi);
//   server.on("/cam-mid.jpg", handleJpgMid);
 
//   server.begin();
// }
 
// void loop()
// {
//   server.handleClient();
// }

#include "../lib/esp32cam-main/examples/WifiCam/WifiCam.hpp"
#include <WiFi.h>

static const char* WIFI_SSID = "5A1-8";
static const char* WIFI_PASS = "sky25a18";

esp32cam::Resolution initialResolution;

WebServer server(80);

#include <StreamString.h>
#include <uri/UriBraces.h>

static const char FRONTPAGE[] = R"EOT(
<!doctype html>
<title>esp32cam WifiCam example</title>
<style>
table,th,td { border: solid 1px #000000; border-collapse: collapse; }
th,td { padding: 0.4rem; }
a { text-decoration: none; }
footer { margin-top: 1rem; }
</style>
<body>
<h1>esp32cam WifiCam example</h1>
<table>
<thead>
<tr><th>BMP<th>JPG<th>MJPEG
<tbody id="resolutions">
<tr><td colspan="3">loading
</table>
<footer>Powered by <a href="https://esp32cam.yoursunny.dev/">esp32cam</a></footer>
<script type="module">
async function fetchText(uri, init) {
  const response = await fetch(uri, init);
  if (!response.ok) {
    throw new Error(await response.text());
  }
  return (await response.text()).trim().replaceAll("\r\n", "\n");
}

try {
  const list = (await fetchText("/resolutions.csv")).split("\n");
  document.querySelector("#resolutions").innerHTML = list.map((r) => `<tr>${
    ["bmp", "jpg", "mjpeg"].map((fmt) => `<td><a href="/${r}.${fmt}">${r}</a>`).join("")
  }`).join("");
} catch (err) {
  document.querySelector("#resolutions td").textContent = err.toString();
}
</script>
)EOT";

static void
serveStill(bool wantBmp)
{
  auto frame = esp32cam::capture();
  if (frame == nullptr) {
    Serial.println("capture() failure");
    server.send(500, "text/plain", "still capture error\n");
    return;
  }
  Serial.printf("capture() success: %dx%d %zub\n", frame->getWidth(), frame->getHeight(),
                frame->size());

  if (wantBmp) {
    if (!frame->toBmp()) {
      Serial.println("toBmp() failure");
      server.send(500, "text/plain", "convert to BMP error\n");
      return;
    }
    Serial.printf("toBmp() success: %dx%d %zub\n", frame->getWidth(), frame->getHeight(),
                  frame->size());
  }

  server.setContentLength(frame->size());
  server.send(200, wantBmp ? "image/bmp" : "image/jpeg");
  WiFiClient client = server.client();
  frame->writeTo(client);
}

static void
serveMjpeg()
{
  Serial.println("MJPEG streaming begin");
  WiFiClient client = server.client();
  auto startTime = millis();
  int nFrames = esp32cam::Camera.streamMjpeg(client);
  auto duration = millis() - startTime;
  Serial.printf("MJPEG streaming end: %dfrm %0.2ffps\n", nFrames, 1000.0 * nFrames / duration);
}

void
addRequestHandlers()
{
  server.on("/", HTTP_GET, [] {
    server.setContentLength(sizeof(FRONTPAGE));
    server.send(200, "text/html");
    server.sendContent(FRONTPAGE, sizeof(FRONTPAGE));
  });

  server.on("/robots.txt", HTTP_GET,
            [] { server.send(200, "text/html", "User-Agent: *\nDisallow: /\n"); });

  server.on("/resolutions.csv", HTTP_GET, [] {
    StreamString b;
    for (const auto& r : esp32cam::Camera.listResolutions()) {
      b.println(r);
    }
    server.send(200, "text/csv", b);
  });

  server.on(UriBraces("/{}x{}.{}"), HTTP_GET, [] {
    long width = server.pathArg(0).toInt();
    long height = server.pathArg(1).toInt();
    String format = server.pathArg(2);
    if (width == 0 || height == 0 || !(format == "bmp" || format == "jpg" || format == "mjpeg")) {
      server.send(404);
      return;
    }

    auto r = esp32cam::Camera.listResolutions().find(width, height);
    if (!r.isValid()) {
      server.send(404, "text/plain", "non-existent resolution\n");
      return;
    }
    if (r.getWidth() != width || r.getHeight() != height) {
      server.sendHeader("Location",
                        String("/") + r.getWidth() + "x" + r.getHeight() + "." + format);
      server.send(302);
      return;
    }

    if (!esp32cam::Camera.changeResolution(r)) {
      Serial.printf("changeResolution(%ld,%ld) failure\n", width, height);
      server.send(500, "text/plain", "changeResolution error\n");
    }
    Serial.printf("changeResolution(%ld,%ld) success\n", width, height);

    if (format == "bmp") {
      serveStill(true);
    } else if (format == "jpg") {
      serveStill(false);
    } else if (format == "mjpeg") {
      serveMjpeg();
    }
  });
}


void setup() {
  Serial.begin(115200);
  Serial.println();
  delay(2000);

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.printf("WiFi failure %d\n", WiFi.status());
    delay(5000);
    ESP.restart();
  }
  Serial.println("WiFi connected");

  {
    using namespace esp32cam;

    initialResolution = Resolution::find(1024, 768);

    Config cfg;
    cfg.setPins(pins::AiThinker);
    cfg.setResolution(initialResolution);
    cfg.setJpeg(80);

    bool ok = Camera.begin(cfg);
    if (!ok) {
      Serial.println("camera initialize failure");
      delay(5000);
      ESP.restart();
    }
    Serial.println("camera initialize success");
  }

  Serial.println("camera starting");
  Serial.print("http://");
  Serial.println(WiFi.localIP());

  addRequestHandlers();
  server.begin();
}

void loop() {
  server.handleClient();
}
