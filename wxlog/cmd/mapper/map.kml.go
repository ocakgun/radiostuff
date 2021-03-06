package main

import "html/template"

var tmplMap = template.Must(template.New("").Parse(`
<!DOCTYPE html>
<html>
  <head>
    <meta name="viewport" content="initial-scale=1.0">
    <meta charset="utf-8">
    <title>FT8 Seen transmissions</title>
    <style>
      #map {
        height: 100%;
      }
      /* Optional: Makes the sample page fill the window. */
      html, body {
        height: 100%;
        margin: 0;
        padding: 0;
      }
      #floating-panel {
        position: absolute;
        top: 10px;
        left: 25%;
        z-index: 5;
        background-color: #fff;
        padding: 5px;
        border: 1px solid #999;
        text-align: center;
        font-family: 'Roboto','sans-serif';
        line-height: 30px;
        padding-left: 10px;
      }
    </style>
  </head>
  <body>
    <div id="floating-panel">
      <button onclick="toggleHeatmap()">Toggle Heatmap</button>
      <button onclick="toggleMarkers()">Toogle markers</button>
    </div>
    <div id="map"></div>
    <script>
      var map, heatmap, markers;

      function toggleHeatmap() {
        heatmap.setMap(heatmap.getMap() ? null : map);
      }

      function toggleMarkers() {
        for (var i = 0; i < markers.length; i++) {
          if(markers[i].getVisible()) {
            markers[i].setVisible(false);
          } else {
            markers[i].setVisible(true);
          }
        }
      }

      function make_heatmap_data(points) {
        var ret = [];
        var ll;
        var x;
        var y;
        var dup = {};
        for (var i = 0; i < points.length; i++) {
          x = points[i][0];
          y = points[i][1];
          ll = x + "/" + y;
          if (dup[ll]) {
            continue;
          }
          dup[ll] = true;
          ret.push({
              location: new google.maps.LatLng(x, y)
          });
        }
        return ret;
      }

      function initMap() {
        map = new google.maps.Map(document.getElementById('map'), {
          zoom: 3,
          center: {lat: 20, lng: 0}
        });
        var heatmapData = make_heatmap_data([{{range .Data}}[{{.Lat}}, {{.Long}}],{{end}}]);

        markers = [{{range .Data}}
          new google.maps.Marker({
            position: {lat: {{.Lat}}, lng: {{.Long}}},
            map: map,
            title: '{{.Callsign}}:\n{{range .Seen}}{{.}}\n{{end}}'
          }),
{{- end -}}
        ];
        heatmap = new google.maps.visualization.HeatmapLayer({
          data: heatmapData,
          radius: 3,
          dissipating: false
        });
      
        heatmap.setMap(map);
      }
    </script>
    <script async defer
            src="https://maps.googleapis.com/maps/api/js?key={{.APIKey}}&callback=initMap&libraries=visualization">
    </script>
  </body>
</html>
`))
