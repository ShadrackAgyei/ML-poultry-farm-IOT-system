
const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<title>Node Control Panel</title>
<style>
body {
  font-family: Arial;
  background: #f2f2f2;
  padding: 20px;
}
h2 {
  text-align: center;
}
.card {
  background: white;
  padding: 20px;
  margin: 15px auto;
  width: 300px;
  border-radius: 10px;
  box-shadow: 0px 0px 8px rgba(0,0,0,0.2);
}
.btn {
  padding: 10px 20px;
  margin: 8px;
  border: none;
  border-radius: 6px;
  cursor: pointer;
  font-size: 16px;
}
.on {
  background: #4CAF50;
  color: white;
}
.off {
  background: #d9534f;
  color: white;
}
</style>
</head>

<body>
<h2>Farm Nodes Control Panel</h2>

<div id="nodes"></div>

<script>
function sendCommand(node, cmd) {
    fetch(`/control?node=${node}&cmd=${cmd}`)
      .then(response => response.text())
      .then(text => console.log(text));
}

let container = document.getElementById("nodes");

for (let i = 1; i <= 5; i++) {
  container.innerHTML += `
    <div class="card">
      <h3>Node ${i}</h3>
      <button class="btn on" onclick="sendCommand(${i},1)">ON</button>
      <button class="btn off" onclick="sendCommand(${i},0)">OFF</button>
    </div>
  `;
}
</script>

</body>
</html>
)=====";


