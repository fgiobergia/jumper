import json
import os
from flask import Flask, request, jsonify
from peaks import find_skips

app = Flask(__name__)
api_version = 1
auth_key = "abc123"
outdir = "storage"

tmp_storage = {}

@app.route(f"/api/v{api_version}/accel/reading", methods=["POST"])
def reading():
    global tmp_storage
    obj = request.json
    auth_key_submitted = obj.pop("authKey")
    seq = obj.pop("payload")
    if auth_key_submitted != auth_key:
        return jsonify({"status": "unauthorized"}), 401

    tmp_storage.update(seq)
    print(len(tmp_storage))

    if len(tmp_storage) > 500:
        min_ts = min(tmp_storage.keys())

        with open(os.path.join(outdir, f"{min_ts}.json"), "w") as f:
            json.dump(tmp_storage, f)
            tmp_storage = {}


    skips_count = find_skips(seq)
    return jsonify({"status": "OK", "count": skips_count }), 200
    

if __name__ == "__main__":
    app.run(debug=True, host="0.0.0.0")
