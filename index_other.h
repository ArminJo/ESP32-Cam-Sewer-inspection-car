/*
 * simpleviewer and streamviewer
 */

const uint8_t index_simple_html[] = R"=====(<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <title id="title">ESP32-CAM Simplified View</title>
    <link rel="icon" type="image/png" sizes="32x32" href="/favicon-32x32.png">
    <link rel="icon" type="image/png" sizes="16x16" href="/favicon-16x16.png">
    <link rel="stylesheet" type="text/css" href="/style.css">
    <style>
      @media (min-width: 800px) and (orientation:landscape) {
        #content {
          display:flex;
          flex-wrap: nowrap;
          flex-direction: column;
          align-items: flex-start;
        }
      }
    </style>
  </head>

  <body>
    <section class="main">
      <div id="header-button-line" style="width:28em;">
        <label for="nav-toggle-cb" id="nav-toggle" style="float:left;" title="Settings">&#9776;&nbsp;</label>
        <button id="swap-viewer" style="float:left;" title="Swap to full feature viewer">Full</button>
        <button id="get-still" style="float:left;">Get Still</button>
        <a id="save-still-link" href="/capture" download="capture"><button id="save-still-button" style="float:left;">Save Still</button></a>
        <button id="toggle-stream" style="float:left;" class="hidden">Start Stream</button>
        <div id="wait-settings" style="float:left;" class="loader" title="Waiting for camera settings to load"></div>
      </div>
      <div id="content">
        <div class="hidden" id="sidebar">
          <input type="checkbox" id="nav-toggle-cb" checked="checked">
          <nav id="menu" style="width:28em;">
            <div class="input-group" id="lamp-group">
              <label for="lamp">Light</label>
              <div class="range-min">Off</div>
              <input type="range" id="lamp" min="0" max="100" value="42" class="default-action">
              <div class="range-max">Full</div>
            </div>
            <div class="input-group" id="pan-group">
              <label id="pan-label" for="pan">Pan 90&deg;</label>
              <div class="range-min">Left</div>
                <input type="range" id="pan" min="20" max="160" value="42" class="default-action">
              <div class="range-max">Right</div>
            </div>
            <div class="input-group" id="speed-group">
              <label id="speed-label" for="motor-speed">Speed 50%</label>
              <div class="range-min">0</div>
                <input type="range" id="motor-speed" min="0" max="255" value="42" class="default-action">
              <div class="range-max">100%</div>
            </div>
            <div class="input-group" id="move-group">
              <label for="move-backward" style="line-height: 2em;">Moving</label>
              <button id="move-backward" title="Move backward" name="move-car" value="-1000">&lt;</button>
              <button id="go-backward20" title="Go back 20 cm" name="move-car" value="-20">20</button>
              <button id="go-backward5" title="Go back 5 cm" name="move-car" value="-5">5</button>
              <button id="stop-motor" title="Stop" name="move-car" value="0">Stop</button>
              <button id="go-forward5" title="Go forward 5 cm" name="move-car" value="5">5</button>
              <button id="go-forward20" title="Go forward 20 cm" name="move-car" value="20">20</button>
              <button id="move-forward" title="Move forward" name="move-car" value="1000">&gt;</button>
            </div>
            <div class="input-group" id="rotate-group">
              <label for="rotate">Rotate in Browser</label>
              <select id="rotate" class="default-action">
                <option value="90">90&deg; (Right)</option>
                <option value="0" selected="selected">0&deg; (None)</option>
                <option value="-90">-90&deg; (Left)</option>
              </select>
            </div>
            <div class="input-group" id="framesize-group">
              <label for="framesize">Resolution</label>
              <select id="framesize" class="default-action">
                <option value="13">UXGA (1600x1200)</option>
                <option value="12">SXGA (1280x1024)</option>
                <option value="11">HD (1280x720)</option>
                <option value="10">XGA (1024x768)</option>
                <option value="9">SVGA (800x600)</option>
                <option value="8">VGA (640x480)</option>
                <option value="7">HVGA (480x320)</option>
                <option value="6">CIF (400x296)</option>
                <option value="5">QVGA (320x240)</option>
                <option value="3">HQVGA (240x176)</option>
                <option value="1">QQVGA (160x120)</option>
                <option value="0">THUMB (96x96)</option>
              </select>
            </div>
            <!-- Hide the next entries, they are present in the body so that we
                can pass settings to/from them for use in the scripting, not for user setting -->
            <div id="cam_name" class="default-action hidden"></div>
            <div id="stream_url" class="default-action hidden"></div>
          </nav>
        </div>
        <figure>
          <div id="stream-container" class="image-container hidden">
            <img id="ESP32-Cam-stream" src="">
          </div>
        </figure>
      </div>
    </section>
  </body>

  <script>
  document.addEventListener('DOMContentLoaded', function (event) {
    var baseHost = document.location.origin;
    var streamURL = 'Undefined';

    const settings = document.getElementById('sidebar')
    const waitSettings = document.getElementById('wait-settings')
    const lampGroup = document.getElementById('lamp-group')

    const rotate = document.getElementById('rotate')
    const view = document.getElementById('ESP32-Cam-stream')
    const viewContainer = document.getElementById('stream-container')
    const stillButton = document.getElementById('get-still')
    const streamButton = document.getElementById('toggle-stream')
    const swapButton = document.getElementById('swap-viewer')

    const saveStillButton = document.getElementById('save-still-button')
    const panGroup = document.getElementById('pan-group')
    const speedGroup = document.getElementById('speed-group')
    const forwardMoveButton = document.getElementById('move-forward')
    const forwardGo20Button = document.getElementById('go-forward20')
    const forwardGo5Button = document.getElementById('go-forward5')
    const stopMotorButton = document.getElementById('stop-motor')
    const backwardGo5Button = document.getElementById('go-backward5')
    const backwardGo20Button = document.getElementById('go-backward20')
    const backwardMoveButton = document.getElementById('move-backward')

    const hide = el => {
      el.classList.add('hidden')
    }
    const show = el => {
      el.classList.remove('hidden')
    }

    const disable = el => {
      el.classList.add('disabled')
      el.disabled = true
    }

    const enable = el => {
      el.classList.remove('disabled')
      el.disabled = false
    }

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
        if(el.id === "lamp"){
          if (value == -1) { 
            hide(lampGroup)
          } else {
            show(lampGroup)
          }
        } else if(el.id === "cam_name"){
          window.document.title = value;
          console.log('Name set to: ' + value);
        } else if(el.id === "code_ver"){
          console.log('Firmware Build: ' + value);
        } else if(el.id === "rotate"){
          rotate.value = value;
          applyRotation();
        } else if(el.id === "stream_url"){
          streamURL = value;
          streamButton.setAttribute("title", `Start the stream :: {streamURL}`);
          console.log('Stream URL set to:' + value);
        } 
      }
    }

    var rangeUpdateScheduled = false
    var latestRangeConfig

    function updateRangeConfig (el) {
      if(el.id === "pan"){
        document.getElementById('pan-label').innerHTML = "Pan " + el.value + "&deg;";
      } else if(el.id === "motor-speed"){
        document.getElementById('speed-label').innerHTML = "Speed " + Math.round((el.value * 100) / 255) + "%";
      }
      latestRangeConfig = el
      if (!rangeUpdateScheduled) {
        rangeUpdateScheduled = true;
        setTimeout(function(){
          rangeUpdateScheduled = false
          updateConfig(latestRangeConfig)
        }, 100);
      }
    }

    function updateConfig (el) {
      let value
      switch (el.type) {
        case 'checkbox':
          value = el.checked ? 1 : 0
          break
        case 'range':
        case 'select-one':
          value = el.value
          break
        case 'button':
        case 'submit':
          value = '1'
          break
        default:
          return
      }

      const query = `${baseHost}/control?var=${el.id}&val=${value}`

      fetch(query)
        .then(response => {
          console.log(`request to ${query} finished, status: ${response.status}`)
        })
    }

    document
      .querySelectorAll('.close')
      .forEach(el => {
        el.onclick = () => {
          hide(el.parentNode)
        }
      })

    // read initial values
    fetch(`${baseHost}/status`)
      .then(function (response) {
        return response.json()
      })
      .then(function (state) {
        document
          .querySelectorAll('.default-action')
          .forEach(el => {
            updateValue(el, state[el.id], false)
          })
        hide(waitSettings);
        show(settings);
        show(streamButton);
      })

    // Put some helpful text on the 'Still' button
    stillButton.setAttribute("title", `Capture a still image :: ${baseHost}/capture`);

    const stopStream = () => {
      window.stop();
      streamButton.innerHTML = 'Start Stream';
          streamButton.setAttribute("title", `Start the stream :: ${streamURL}`);
      hide(viewContainer);
    }

    const startStream = () => {
      view.src = streamURL;
      view.scrollIntoView(false);
      streamButton.innerHTML = 'Stop Stream';
      streamButton.setAttribute("title", `Stop the stream`);
      show(viewContainer);
    }

    const applyRotation = () => {
      rot = rotate.value;
      if (rot == -90) {
        viewContainer.style.transform = `rotate(-90deg)  translate(-100%)`;
      } else if (rot == 90) {
        viewContainer.style.transform = `rotate(90deg) translate(0, -100%)`;
      } else {
        viewContainer.style.transform = `rotate(0deg)`;
      }
       console.log('Rotation ' + rot + ' applied');
   }

    // Attach actions to controls

    stillButton.onclick = () => {
      stopStream();
      view.src = `${baseHost}/capture?_cb=${Date.now()}`;
      view.scrollIntoView(false);
      show(viewContainer);

    }

    streamButton.onclick = () => {
      const streamEnabled = streamButton.innerHTML === 'Stop Stream'
      if (streamEnabled) {
        stopStream();
      } else {
        startStream();
      }
    }

    // Attach default on change action
    document
      .querySelectorAll('.default-action')
      .forEach(el => {
        el.onchange = () => updateConfig(el)
      })

    // Update range sliders as they are being moved
    document
      .querySelectorAll('input[type="range"]')
      .forEach(el => {
        el.oninput = () => updateRangeConfig(el)
      })

    // Custom actions
    // Detection and framesize
    rotate.onchange = () => {
      applyRotation();
      updateConfig(rotate);
    }

    framesize.onchange = () => {
      updateConfig(framesize);
    }

    swapButton.onclick = () => {
      window.open('/?view=full','_self');
    }

    function sendToCommandHandlerByName (aElement) {
      const query = `${baseHost}/control?var=${aElement.name}&val=${aElement.value}`

      fetch(query)
        .then(response => {
          console.log(`request to ${query} finished, status: ${response.status}`)
        })
    }

    saveStillButton.setAttribute("title", `Download a still image :: ${baseHost}/capture`);

    saveStillButton.onclick = () => {
      // set download attribute to right date and time
      var tDate = new Date();
      tDate.setMinutes(tDate.getMinutes() - tDate.getTimezoneOffset());
      var tDateString = tDate.toISOString();
      tDateString = tDateString.substring(0,10) + "-" + tDateString.substring(11,20).replace(":","h").replace(":","m").replace(".","s");
      document.getElementById('save-still-link').setAttribute("download", "capture_" + tDateString);
      console.log("set download target string to:" + "capture_" + tDateString);
    }

    forwardMoveButton.onclick = () => {
        sendToCommandHandlerByName(forwardMoveButton);
    }
    forwardGo20Button.onclick = () => {
        sendToCommandHandlerByName(forwardGo20Button);
    }
    forwardGo5Button.onclick = () => {
        sendToCommandHandlerByName(forwardGo5Button);
    }
    stopMotorButton.onclick = () => {
        sendToCommandHandlerByName(stopMotorButton);
    }
    backwardGo5Button.onclick = () => {
        sendToCommandHandlerByName(backwardGo5Button);
    }
    backwardGo20Button.onclick = () => {
        sendToCommandHandlerByName(backwardGo20Button);
    }
    backwardMoveButton.onclick = () => {
        sendToCommandHandlerByName(backwardMoveButton);
    }
  })
  </script>
</html>)=====";

size_t index_simple_html_len = sizeof(index_simple_html)-1;

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
  </body>

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
  </body>
  <script>
    setTimeout(function(){
      location.replace(document.URL);
    }, 60000);
  </script>
</html>)=====";
