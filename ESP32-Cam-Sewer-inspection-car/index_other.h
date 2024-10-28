/*
 * R"=====(...)====" is raw string with delimiter ====
 */
/* Stream Viewer */

const uint8_t streamviewer_html[] = R"=====(<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title id="title">ESP32-CAM StreamViewer</title>
  <link rel="icon" type="image/png" sizes="32x32" href="/favicon-32x32.png">
  <link rel="icon" type="image/png" sizes="16x16" href="/favicon-16x16.png">
  <style>
    /* No stylesheet, define all style elements here */
    body {
      font-family: Arial,Helvetica,sans-serif;
      background: #181818;
      color: #EFEFEF;
      font-size: 16px;
      margin: 0px;
      overflow:hidden;
    }

    img {
      object-fit: contain;
      display: block;
      margin: 0px;
      padding: 0px;
      width: 100vw;
      height: 100vh;
    }

    .loader {
      border: 0.5em solid #f3f3f3;
      border-top: 0.5em solid #000000;
      border-radius: 50%;
      width: 1em;
      height: 1em;
      -webkit-animation: spin 2s linear infinite; /* Safari */
      animation: spin 2s linear infinite;
    }

    @-webkit-keyframes spin {   /* Safari */
      0% { -webkit-transform: rotate(0deg); }
      100% { -webkit-transform: rotate(360deg); }
    }

    @keyframes spin {
      0% { transform: rotate(0deg); }
      100% { transform: rotate(360deg); }
    }
  </style>
</head>

<body>
  <section class="main">
    <div id="wait-settings" style="float:left;" class="loader" title="Waiting for stream settings to load"></div>
    <div style="display: none;">
      <!-- Hide the next entries, they are present in the body so that we
           can pass settings to/from them for use in the scripting -->
      <div id="rotate" class="default-action hidden">0</div>
      <div id="cam_name" class="default-action hidden"></div>
      <div id="stream_url" class="default-action hidden"></div>
    </div>
    <img id="ESP32-Cam-stream" src="">
  </section>

<script>
document.addEventListener('DOMContentLoaded', function (event) {
  var baseHost = document.location.origin;
  var streamURL = 'Undefined';

  const rotate = document.getElementById('rotate')
  const stream = document.getElementById('ESP32-Cam-stream')
  const spinner = document.getElementById('wait-settings')

  const updateValue = (el, value, updateRemote) => {
    updateRemote = updateRemote == null ? true : updateRemote
    let initialValue
    if (el.type === 'checkbox') {
      initialValue = el.checked
      value = !!value
      el.checked = value
    } else {
      initialValue = el.value
      el.value = value
    }

    if (updateRemote && initialValue !== value) {
      updateConfig(el);
    } else if(!updateRemote){
      if(el.id === "cam_name"){
        window.document.title = value;
        ESP32-Cam-stream.setAttribute("title", value + "\n(doubleclick for fullscreen)");
        console.log('Name set to: ' + value);
      } else if(el.id === "rotate"){
        rotate.value = value;
        console.log('Rotate recieved: ' + rotate.value);
      } else if(el.id === "stream_url"){
        streamURL = value;
        console.log('Stream URL set to:' + value);
      }
    }
  }

  // read initial values
  fetch(`${baseHost}/info`)
    .then(function (response) {
      return response.json()
    })
    .then(function (state) {
      document
        .querySelectorAll('.default-action')
        .forEach(el => {
          updateValue(el, state[el.id], false)
        })
      spinner.style.display = `none`;
      applyRotation();
      startStream();
    })

  const startStream = () => {
    ESP32-Cam-stream.src = streamURL;
    ESP32-Cam-stream.style.display = `block`;
  }

  const applyRotation = () => {
    rot = rotate.value;
    if (rot == -90) {
      ESP32-Cam-stream.style.transform = `rotate(-90deg)`;
    } else if (rot == 90) {
      ESP32-Cam-stream.style.transform = `rotate(90deg)`;
    }
    console.log('Rotation ' + rot + ' applied');
  }

  ESP32-Cam-stream.ondblclick = () => {
    if (ESP32-Cam-stream.requestFullscreen) {
      ESP32-Cam-stream.requestFullscreen();
    } else if (ESP32-Cam-stream.mozRequestFullScreen) { /* Firefox */
      ESP32-Cam-stream.mozRequestFullScreen();
    } else if (ESP32-Cam-stream.webkitRequestFullscreen) { /* Chrome, Safari and Opera */
      ESP32-Cam-stream.webkitRequestFullscreen();
    } else if (ESP32-Cam-stream.msRequestFullscreen) { /* IE/Edge */
      ESP32-Cam-stream.msRequestFullscreen();
    }
  }
})
</script>
</body>
</html>)=====";

size_t streamviewer_html_len = sizeof(streamviewer_html)-1;

/* Captive Portal page
   we replace the <> delimited strings with correct values as it is served */

const std::string portal_html = R"=====(<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <title id="title"><CAMNAME> - portal</title>
    <link rel="icon" type="image/png" sizes="32x32" href="<APPURL>favicon-32x32.png">
    <link rel="icon" type="image/png" sizes="16x16" href="<APPURL>favicon-16x16.png">
    <link rel="stylesheet" type="text/css" href="<APPURL>style.css">
  </head>
  <body style="text-align: center;">
    <img src="<APPURL>logo.svg" style="position: relative; float: right;">
    <h1><CAMNAME> - access portal</h1>
    <div class="input-group" style="margin: auto; width: max-content;">
      <a href="<APPURL>?view=simple" title="Click here for a simple view with minimum control" style="text-decoration: none;" target="_blank">
      <button>Simple Viewer</button></a>
      <a href="<APPURL>?view=full" title="Click here for the main camera page with full controls" style="text-decoration: none;" target="_blank">
      <button>Full Viewer</button></a>
      <a href="<STREAMURL>view" title="Click here for the dedicated stream viewer" style="text-decoration: none;" target="_blank">
      <button>Stream Viewer</button></a>
    </div>
    <hr>
    <a href="<APPURL>dump" title="Information dump page" target="_blank">Camera Details</a><br>
  </body>
</html>)=====";

/* Error page
   we replace the <> delimited strings with correct values as it is served */

const std::string error_html = R"=====(<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <title id="title"><CAMNAME> - Error</title>
    <link rel="icon" type="image/png" sizes="32x32" href="/favicon-32x32.png">
    <link rel="ico\" type="image/png" sizes="16x16" href="/favicon-16x16.png">
    <link rel="stylesheet" type="text/css" href="<APPURL>style.css">
  </head>
  <body style="text-align: center;">
    <img src="<APPURL>logo.svg" style="position: relative; float: right;">
    <h1><CAMNAME></h1>
    <ERRORTEXT>
  <script>
    setTimeout(function(){
      location.replace(document.URL);
    }, 60000);
  </script>
  </body>
</html>)=====";
