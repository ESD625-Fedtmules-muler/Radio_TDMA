import serial
import folium
import time
import webbrowser
ser = serial.Serial('COM5', 115200, timeout=1)

nodes = {}
webbrowser.open("map.html")
def create_map():
    if not nodes:
        return

    first = list(nodes.values())[0]
    m = folium.Map(location=first, zoom_start=12)

    for nid, (lat, lon) in nodes.items():
        folium.Marker([lat, lon], popup=str(nid)).add_to(m)

    m.save("map.html")


while True:
    line = ser.readline().decode(errors='ignore').strip()

    print(line)

    if line.startswith("NODE"):
        _, node_id, lat, lon = line.split(",")

        nodes[int(node_id)] = (float(lat), float(lon))

        create_map()

    time.sleep(0.5)