import json
import os
from flask import Flask, request, jsonify

app = Flask(__name__)
api_version = 1
outdir = "storage"

tmp_storage = {}

@app.route(f"/api/v{api_version}/accel/reading", methods=["POST"])
def reading():
    global tmp_storage
    obj = request.json
    tmp_storage.update(obj)
    print(len(tmp_storage))

    if len(tmp_storage) > 500:
        min_ts = min(tmp_storage.keys())
        with open(os.path.join(outdir, f"{min_ts}.json"), "w") as f:
            json.dump(tmp_storage, f)
            tmp_storage = {}

    return jsonify({"status": "OK"}), 200
    

if __name__ == "__main__":
    app.run(debug=True, host="0.0.0.0")
