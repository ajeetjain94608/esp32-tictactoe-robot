/*
 * Simple OTA Page - Direct upload, no login
 */

const char* otaPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32 OTA Update</title>
  <style>
    body { font-family: Arial; text-align: center; margin-top: 50px; background: #1a1a2e; color: #eee; }
    h1 { color: #0f0; }
    .container { background: #16213e; padding: 30px; border-radius: 10px; display: inline-block; }
    input[type=file] { margin: 20px 0; padding: 10px; }
    input[type=submit] { background: #0f0; color: #000; padding: 15px 40px; border: none; cursor: pointer; font-size: 18px; border-radius: 5px; }
    input[type=submit]:hover { background: #0c0; }
    #prg { margin-top: 20px; font-size: 20px; color: #0f0; }
    .bar { width: 0%; height: 30px; background: #0f0; border-radius: 5px; transition: width 0.3s; }
    .barBg { background: #333; border-radius: 5px; margin-top: 10px; }
  </style>
</head>
<body>
  <div class="container">
    <h1>OTA Update</h1>
    <form method='POST' action='/update' enctype='multipart/form-data' id='upload_form'>
      <input type='file' name='update' accept='.bin'><br>
      <input type='submit' value='Upload'>
    </form>
    <div id='prg'>Select .bin file and click Upload</div>
    <div class="barBg"><div class="bar" id="bar"></div></div>
  </div>
  <script>
    document.getElementById('upload_form').addEventListener('submit', function(e) {
      e.preventDefault();
      var form = document.getElementById('upload_form');
      var data = new FormData(form);
      var xhr = new XMLHttpRequest();
      xhr.open('POST', '/update', true);
      xhr.upload.addEventListener('progress', function(evt) {
        if (evt.lengthComputable) {
          var per = Math.round((evt.loaded / evt.total) * 100);
          document.getElementById('prg').innerHTML = 'Uploading: ' + per + '%';
          document.getElementById('bar').style.width = per + '%';
        }
      }, false);
      xhr.onload = function() {
        if (xhr.status == 200) {
          document.getElementById('prg').innerHTML = 'Done! Rebooting...';
        } else {
          document.getElementById('prg').innerHTML = 'Update Failed!';
        }
      };
      xhr.send(data);
    });
  </script>
</body>
</html>
)rawliteral";
