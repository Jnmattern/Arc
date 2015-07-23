var dateorder = 0;
var lang = 0;
var backlight = 0;
var themecode = "";

function logVariables() {
	console.log("	dateorder: " + dateorder);
	console.log("	lang: " + lang);
	console.log("	backlight: " + backlight);
  console.log(" themecode: " + themecode);
}

Pebble.addEventListener("ready", function() {
	console.log("Ready Event");
	
	dateorder = localStorage.getItem("dateorder");
	if (!dateorder) {
		dateorder = 1;
	}

	lang = localStorage.getItem("lang");
	if (!lang) {
		lang = 1;
	}
	
	backlight = localStorage.getItem("backlight");
	if (!backlight) {
		backlight = 0;
	}

  themecode = localStorage.getItem("themecode");
  if (!themecode) {
    themecode = "c0fcf4ed";
  }

	logVariables();
  
  var message = '{"dateorder":'+dateorder+',"lang":'+lang+',"backlight":'+backlight+',"themecode":"'+themecode+'"}';
  console.log("Sending message : " + message);
	Pebble.sendAppMessage(JSON.parse(message));
});


function isWatchColorCapable() {
  if (typeof Pebble.getActiveWatchInfo === "function") {
    try {
      if (Pebble.getActiveWatchInfo().platform != 'aplite') {
        return true;
      } else {
        return false;
      }
    } catch(err) {
      console.log('ERROR calling Pebble.getActiveWatchInfo() : ' + JSON.stringify(err));
      // Assuming Pebble App 3.0
      return true;
    }
  }
  //return ((typeof Pebble.getActiveWatchInfo === "function") && Pebble.getActiveWatchInfo().platform!='aplite');
}


Pebble.addEventListener("showConfiguration", function(e) {
	console.log("showConfiguration Event");

	logVariables();
  
  var url = "http://www.famillemattern.com/jnm/pebble/Arc/Arc_3.0.html?dateorder=" + dateorder +
				   "&lang=" + lang + "&backlight=" + backlight + "&themecode=" + themecode + "&colorCapable=" + isWatchColorCapable();
  console.log(url);
	Pebble.openURL(url);
});

Pebble.addEventListener("webviewclosed", function(e) {
	console.log("Configuration window closed");
	console.log(e.type);
  console.log("Response: " + decodeURIComponent(e.response));

	var configuration = JSON.parse(decodeURIComponent(e.response));
	Pebble.sendAppMessage(configuration);
						
	dateorder = configuration["dateorder"];
	localStorage.setItem("dateorder", dateorder);

	lang = configuration["lang"];
	localStorage.setItem("lang", lang);

	backlight = configuration["backlight"];
	localStorage.setItem("backlight", backlight);
  
  themecode = configuration["themecode"];
	localStorage.setItem("themecode", themecode);
});
