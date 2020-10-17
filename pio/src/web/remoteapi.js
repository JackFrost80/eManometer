STRINGIFY(
<script>

function showElem(name) {
	var el = document.getElementsByName(name);

	if (el.length > 0)
		el[0].style.display = "block";

	el = document.getElementsByName("label-" + name);
	
	if (el.length > 0)
		el[0].style.display = "block";

};

function hideElem(name) {
	var el = document.getElementsByName(name);

	if (el.length > 0)
		el[0].style.display = "none";

	el = document.getElementsByName("label-" + name);
	
	if (el.length > 0)
		el[0].style.display = "none";
};

function hideAll()
{
	hideElem("token");
	hideElem("address");
	hideElem("port");
	hideElem("url");
	hideElem("db");
	hideElem("username");
	hideElem("password");
	hideElem("job");
	hideElem("instance");
}

function refresh()
{
	hideAll();

	var el = document.getElementsByName("api");

	if (el.length == 0) {
		return;
	};

	var e = el[0];

	switch (parseInt(e.value, 10)) {
		// Off
		case 0:
			break;
		// Ubidots & ThingSpeak
		case 1:
		case 8:
			showElem("token");
			break;
		// Craftbeerpi
		case 2:
			showElem("address");
			break;
		// HTTP
		case 3:
			showElem("address");
			showElem("url");
			showElem("port");
			break;
		// TCP
		case 4:
			showElem("address");
			showElem("port");
			break;
		// InfluxDB
		case 5:
			showElem("address");
			showElem("port");
			showElem("db");
			showElem("username");
			showElem("password");
			break;
		// Prometheus
		case 6:
			showElem("address");
			showElem("port");
			showElem("job");
			showElem("instance");
			break;
		// MQTT
		case 7:
			showElem("address");
			showElem("port");
			showElem("username");
			showElem("password");
			break;
	};
}

window.onload = function()
{
	var el = document.getElementsByName("api");

	if (el.length > 0) {
		var e = el[0];

		e.onchange = refresh;
	};

	refresh();
}
</script>
)
