/*
 * simpleviewer and streamviewer
 */

/*
 * R"=====(...)====" is raw string with delimiter ====
 */
const uint8_t index_simple_html[] = R"=====(<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <title id="title">ESP32-CAM Simplified View</title>
    <link rel="icon" type="image/png" sizes="32x32" href="/favicon-32x32.png">
    <link rel="icon" type="image/png" sizes="16x16" href="/favicon-16x16.png">
    <link rel="stylesheet" type="text/css" href="style.css">
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
      <label for="nav-toggle-cb" id="nav-toggle" style="float:left;" title="Hide / show settings">&#9776;&nbsp;</label>
      <button id="swap-viewer" style="float:left;" title="Swap to full feature viewer">Full</button>
      <button id="get-still" style="float:left;">Get Still</button>
      <a id="save-still-link" href="/capture" download="capture"><button id="save-still-button" style="float:left;">Save Still</button></a>
      <button id="toggle-stream" style="float:left;" class="hidden">Start Stream</button>
      <div id="fps" style="float:left;" class="hidden" title="FPS of started stream">0.0</div>
      <div id="wait-settings" style="float:left;" class="loader" title="Waiting for camera settings to load"></div>
    </div>
    <div id="content">
      <div class="hidden" id="sidebar">
        <input type="checkbox" id="nav-toggle-cb" checked="checked">
        <nav id="menu" style="width:28em;">
          <div class="input-group hidden" id="lamp-group">
            <label id="lamp-label" for="lamp">Light</label>
            <div class="range-min">Off</div>
            <input type="range" id="lamp" min="0" max="100" value="42" class="default-action">
            <div class="range-max">Full</div>
          </div>
          <div class="input-group hidden" id="autolamp-group">
            <label for="autolamp">Auto Lamp</label>
            <div class="switch">
              <input id="autolamp" type="checkbox" class="default-action" title="Lamp only on when camera active">
              <label class="slider" for="autolamp"></label>
            </div>
          </div>
          <div class="input-group hidden" id="pan-group">
            <label id="pan-label" for="pan">Pan</label>
            <div class="range-min">Left</div>
            <input type="range" id="pan" min="20" max="160" value="42" class="default-action">
            <div class="range-max">Right</div>
          </div>
          <div class="input-group hidden" id="speed-group">
            <label id="speed-label" for="motor-speed">Speed</label>
            <div class="range-min">0</div>
            <input type="range" id="motor-speed" min="0" max="255" value="42" class="default-action">
            <div class="range-max">100%</div>
          </div>
          <div class="input-group hidden" id="move-group">
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
            <div class="input-group" id="quality-group">
              <label id="quality-label" for="quality">Quality</label>
              <div class="range-min">Low<br><span style="font-size: 80%;">(fast)</span></div>
              <!-- Note; the following element is 'flipped' in CSS so that it slides from High to Low
                   As a result the 'min' and 'max' values are reversed here too -->
              <input type="range" id="quality" min="6" max="63" value="10" class="default-action">
              <div class="range-max">High<br><span style="font-size: 80%;">(slow)</span></div>
            </div>
          <div class="input-group" id="sys_info-group">
            <a href="/dump" title="System Info" target="_blank">System Info</a>
          </div>
          <!-- Hide the next entries, they are present in the body so that we
              can pass settings to/from them for use in the scripting, not for user setting -->
          <div id="cam_name" class="default-action hidden"></div>
          <div id="stream_url" class="default-action hidden"></div>
        </nav>
      </div>
      <figure>
        <div id="stream-container" class="image-container hidden">
            <div class="close close-rot-none" id="close-stream">×</div>
          <img id="ESP32-Cam-stream" src="">
        </div>
      </figure>
    </div>
  </section>

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
  const closeButton = document.getElementById('close-stream')
  const swapButton = document.getElementById('swap-viewer')

  const saveStillButton = document.getElementById('save-still-button')

// Extensions
  const fpsInfo = document.getElementById('fps')
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


  function updateRangeLabel (el) {
    if(el.id === "lamp"){
      document.getElementById('lamp-label').innerHTML = "Light " + el.value + "%";
    } else if(el.id === "pan"){
      document.getElementById('pan-label').innerHTML = "Pan " + el.value + "&deg;";
    } else if(el.id === "motor-speed"){
      document.getElementById('speed-label').innerHTML = "Speed " + Math.round((el.value * 100) / 255) + "%";
    } else if(el.id === "quality"){
      document.getElementById('quality-label').innerHTML = "Quality " + el.value;
    }
  }

  const updateValue = (el, value, updateRemote) => {
    updateRemote = updateRemote == null ? true : updateRemote

    if(!updateRemote){
    // Change visibility of elements and set some special values
      if(typeof value != 'undefined') {
        if(el.id === "pan" || el.id === "motor-speed" || el.id === "lamp"){
          el.parentElement.classList.remove('hidden') // show hidden element, if value is not undefined, i.e. sent from host
          if(el.id === "motor-speed"){
            show(document.getElementById('move-group')); // enable this group too if we set the speed
            console.log('PWM Motor control enabled');
          }
        } else if(el.id === "autolamp"){
          show(document.getElementById('autolamp-group'));
          console.log('Autolamp enabled');

        } else if(el.id === "cam_name"){
          window.document.title = value;
          console.log('Name set to: ' + value);
        } else if(el.id === "code_ver"){
          console.log('Firmware Build: ' + value);
        } else if(el.id === "rotate"){
          rotate.value = value; // required for applyRotation()
          applyRotation();
        } else if(el.id === "stream_url"){
          streamURL = value;
          streamButton.setAttribute("title", `Start the stream :: {streamURL}`);
          console.log('Stream URL set to:' + value);
        }
      }
    }

    // Get elements initial value and then set new value
    let initialValue
    if (el.type === 'checkbox') {
      initialValue = el.checked
      value = !!value
      el.checked = value
    } else {
      initialValue = el.value
      el.value = value
    }

    // Update range labels
    if (el.type === 'range') {
      updateRangeLabel(el);
    }

    // send to host if value changed
    if (updateRemote && initialValue !== value) {
      updateConfig(el);
    }

  }

  var rangeUpdateScheduled = false
  var latestRangeConfig

  function updateRangeConfig (el) {
    updateRangeLabel(el);
    // call updateConfig() for this element after 100 ms
    latestRangeConfig = el
    if (!rangeUpdateScheduled) {
      rangeUpdateScheduled = true;
      setTimeout(function(){
        rangeUpdateScheduled = false
        updateConfig(latestRangeConfig)
      }, 100);
    }
  }

// Update values on host
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

//
// Get json with FPS from host and show it
//
  function getFps(){
    fetch(`${baseHost}/fps_info`)
      .then(function (response) {
        return response.json()
      })
      .then(function (fpsinfo) {
        fpsInfo.innerHTML = " &nbsp; " + fpsinfo["fps"] + " fps"; // set value
          })
  }

//
// Start document processing
// Get status from host and set GUI values accordingly
//
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
          updateValue(el, state[el.id], false) // set value and show element if hidden
        })
      hide(waitSettings);
      show(settings);
      show(streamButton);
    })

  // Put some helpful text on the 'Still' button
  stillButton.setAttribute("title", `Capture a still image :: ${baseHost}/capture`);

  var fpsIntervalHandle;

  const stopStream = () => {
    window.stop();
    streamButton.innerHTML = 'Start Stream';
    streamButton.setAttribute("title", `Start the stream :: ${streamURL}`);
    hide(viewContainer);
    hide(fpsInfo);
    clearInterval(fpsIntervalHandle);
  }

  const startStream = () => {
    view.src = streamURL;
    view.scrollIntoView(false);
    streamButton.innerHTML = 'Stop Stream';
    streamButton.setAttribute("title", `Stop the stream`);
    show(viewContainer);
    show(fpsInfo);
    fpsIntervalHandle = setInterval(getFps, 1000);
  }

  const applyRotation = () => {
    rot = rotate.value;
    if (rot == -90) {
      viewContainer.style.transform = `rotate(-90deg)  translate(-100%)`;
      closeButton.classList.remove('close-rot-none');
      closeButton.classList.remove('close-rot-right');
      closeButton.classList.add('close-rot-left');
    } else if (rot == 90) {
      viewContainer.style.transform = `rotate(90deg) translate(0, -100%)`;
      closeButton.classList.remove('close-rot-left');
      closeButton.classList.remove('close-rot-none');
      closeButton.classList.add('close-rot-right');
    } else {
      viewContainer.style.transform = `rotate(0deg)`;
      closeButton.classList.remove('close-rot-left');
      closeButton.classList.remove('close-rot-right');
      closeButton.classList.add('close-rot-none');
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

  closeButton.onclick = () => {
    stopStream();
    hide(viewContainer);
  }

  streamButton.onclick = () => {
    const streamEnabled = streamButton.innerHTML === 'Stop Stream'
    if (streamEnabled) {
      stopStream();
    } else {
      startStream();
    }
  }

  // Attach default on change action for all elements
  document
    .querySelectorAll('.default-action')
    .forEach(el => {
      el.onchange = () => updateConfig(el)
    })

  // Set on update action for range sliders. 
  // Update host value as well as range label as they are being moved
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
</body>
</html>)=====";

size_t index_simple_html_len = sizeof(index_simple_html)-1;
