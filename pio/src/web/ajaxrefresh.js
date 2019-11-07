STRINGIFY(
<script>
function poll()
{
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/ajax");

  xhr.onload = function() {
    var json = JSON.parse(xhr.response);

    for (prop in json) {
      var el = document.getElementsByClassName("info-" + prop);

      if (el.length > 0) {
          el[0].innerText = json[prop];
      }
    };

    setTimeout(poll, 1000);
  };

  xhr.onerror = function() {
    setTimeout(poll, 1000);
  };

  xhr.send();
};

window.onload = function()
{
  poll();
}
</script>
)
