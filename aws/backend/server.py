import json
import os
import re
import time
from threading import Lock, Thread
from typing import Dict, List, Union

import folium
import git
from bs4 import BeautifulSoup
from flask import Flask, json, jsonify, render_template, request
from flask.helpers import make_response
from flask.wrappers import Response
from flask_cors import CORS
from git.exc import GitCommandError
from waitress import serve

app: Flask = Flask(
    __name__,
    template_folder=os.path.abspath('../frontend/templates'),
    static_folder=os.path.abspath('../frontend/static')
)
app.config["JSON_AS_ASCII"] = False
app.config["JSON_SORT_KEYS"] = False
CORS(app)
_lock: Lock = Lock()
database: List[Dict[str, Union[str, float]]]
if os.path.exists("../data/data.json"):
    with open("../data/data.json") as f:
        database = json.load(f)
else:
    database = [{
        "eval": "評価ポイント：3",
        "database_latitude": 35.1356448,
        "database_longitude": 135.6068311,
    }]



COUNTER: int = 0
class MakeMap:
    def __init__(
        self,
        frequency: float = 10.
    ) -> None:
        act: Thread = Thread(target=self.make, args=(frequency, ), daemon=True)
        act.start()

    def make(self, frequency: float) -> None:
        while True:
            map = folium.Map(location=[35.13509, 136.97644], zoom_start=16)
            print('======================================================')
            for data in database:
                print(data)
                popup = folium.Popup(data["eval"], min_width=0, max_width=1000)
                folium.Marker(location=[data["latitude"], data["longitude"]], popup=popup).add_to(map)

            global COUNTER
            map.save(os.path.join(os.path.abspath("../frontend/templates"), f"map-{COUNTER}.html"))

            self.modify()

            if COUNTER > 0:
                os.remove(os.path.join(os.path.abspath("../frontend/templates"), f"map-{COUNTER-1}.html"))

            COUNTER += 1

            time.sleep(frequency)

    def modify(self):
        with open(f"../frontend/templates/map-{COUNTER}.html", mode="rt", encoding="utf-8") as f:
            soup: BeautifulSoup = BeautifulSoup(f.read(), "html.parser")
        tag: BeautifulSoup =  soup.new_tag('script', type="text/javascript", src="{{ url_for('static', filename='../static/function.js')}}")
        soup.append(tag)
        map_name = str(soup.find('div', class_="folium-map")).split('"')[3]

        with open (f"../frontend/templates/map-{COUNTER}.html", "w") as f:
            f.write(str(soup))

        with open(f"../frontend/templates/map-{COUNTER}.html", mode="r", encoding="utf-8") as f:
            html = f.read()
        re_html = re.sub(map_name, "map_", html)
        with open (f"../frontend/templates/map-{COUNTER}.html", "w") as f:
            f.write(str(re_html))

class PushDatabase:
    def __init__(
        self,
        interval: float = 30.,
    ) -> None:
        p: Thread = Thread(target=self.run, args=(interval, ), daemon=True)
        p.start()

        self.repo = git.Repo(os.path.expanduser("~/Sony_Hackthon"))

    def run(self, interval: float) -> None:
        while True:
            with open(os.path.expanduser('~/Sony_Hackthon/aws/data/data.json'), 'w') as f:
                json.dump(database, f, indent=4, ensure_ascii=False)
            
            try:
                self.repo.git.add(os.path.expanduser('~/Sony_Hackthon/aws/data/data.json'))
                self.repo.git.commit(os.path.expanduser('~/Sony_Hackthon/aws/data/data.json'), message='update', author='njima')
                self.repo.git.push('origin', 'main')
            except (GitCommandError, AttributeError):
                pass

            time.sleep(interval)

@app.route("/")
def render_homepage():
    return render_template("display.html")

@app.route("/map.html")
def render_map():
    if COUNTER > 0:
        return render_template(f"map-{COUNTER-1}.html")
    else:
        return render_template("/map.html")

@app.route("/eval", methods=["POST"])
def get_data() -> Response:
    requests: Dict[str, str] = request.get_json(force=True)

    responses: Dict[str, bool] = {
        "contain": False,
    }
    
    valuetuon: str = "評価ポイント：" + requests["eval"]
    latitude: float = float(requests["request_latitude"])
    longitude: float = float(requests["request_longitude"])

    _lock.acquire()

    data: Dict[str, float] = {
        "eval": valuetuon,
        "latitude": latitude,
        "longitude": longitude,
    }
    database.append(data)
    _lock.release()
    
    responses["contain"] = True
    return make_response(jsonify(responses))


if __name__ == "__main__":
    MakeMap()
    serve(app, host='0.0.0.0', port=8089, threads=30)
