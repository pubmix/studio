import json
import sys
import time
import pytest
pytest.importorskip("fastapi")
pytest.importorskip("httpx")
from fastapi.testclient import TestClient
from studio_stem_engine.cloud import Jobs, create_app
from test_cloud import AUTH, TOKEN, wait


def test_stage_split_auth_replay_and_compact_views(tmp_path):
    manager = Jobs(tmp_path, command=[sys.executable, "-c", "import time;time.sleep(30)"])
    with TestClient(create_app(token=TOKEN, manager=manager)) as client:
        assert client.get("/v1/panel/library").status_code == 401
        assert client.post("/v1/panel/sources?title=song.wav", content=b"x").status_code == 401
        assert client.post("/v1/panel/sources?title=song.zip", headers=AUTH, content=b"x").status_code == 400
        assert client.post("/v1/panel/sources?title=song.wav", headers=AUTH, content=b"").status_code == 400
        response = client.post("/v1/panel/sources?title=song.wav", headers=AUTH, content=b"fixture"*1000)
        assert response.status_code == 201
        source = response.json()["id"]
        listing = client.get("/v1/panel/library", headers=AUTH).json()
        assert listing["items"][0]["kind"] == "source"
        payload = {"source_id": source, "mode": "extended", "rights_confirmed": True, "request_id": "panel-request-12345"}
        assert client.post("/v1/panel/jobs", headers=AUTH, json={**payload,"rights_confirmed":False}).status_code == 400
        response = client.post("/v1/panel/jobs", headers=AUTH, json=payload)
        assert response.status_code == 202
        identifier = response.json()["id"]
        assert (tmp_path/identifier/"input.audio").read_bytes() == b"fixture"*1000
        assert (tmp_path/"panel-sources"/(source+".audio")).exists()
        assert json.loads((tmp_path/identifier/"request.json").read_text())["settings"]["model"] == "htdemucs_6s"
        assert client.post("/v1/panel/jobs", headers=AUTH, json=payload).json()["id"] == identifier
        source2 = client.post("/v1/panel/sources?title=other.wav", headers=AUTH, content=b"other").json()["id"]
        assert client.post("/v1/panel/jobs", headers=AUTH, json={**payload,"source_id":source2}).status_code == 409
        assert client.post("/v1/panel/jobs", headers=AUTH, json={**payload,"video_id":"abcdefghijk"}).status_code == 422
        assert client.post("/v1/panel/jobs", headers=AUTH, json={**payload,"source_id":"../../escape"}).status_code == 422
        response = client.get("/v1/panel/jobs/"+identifier, headers=AUTH)
        assert response.status_code == 200 and len(response.content)<2000
        assert "requested_settings" not in response.json()
        assert client.post("/v1/panel/jobs/"+identifier+"/send?transfer=bogus", headers=AUTH,json={}).status_code == 422
        assert client.post("/v1/panel/jobs/"+identifier+"/cancel", headers=AUTH,json={}).status_code == 200
        assert wait(manager,identifier)["state"] == "cancelled"


def test_panel_search_paging_and_expiry(tmp_path):
    manager = Jobs(tmp_path)
    searcher = lambda query:[{"id":"abcdefghijk","title":"Caf\u00e9\nSong","duration":180,"uploader":"test"}]
    with TestClient(create_app(token=TOKEN, manager=manager, searcher=searcher)) as client:
        result=client.get("/v1/panel/search?q=track",headers=AUTH).json()
        assert result["items"][0]["title"] == "Cafe Song"
        assert result["items"][0]["kind"] == "video"
        for i in range(7):
            client.post("/v1/panel/sources?title=song.wav",headers=AUTH,content=b"x")
        assert client.get("/v1/panel/library",headers=AUTH).json()["more"]
        assert len(client.get("/v1/panel/library?offset=6",headers=AUTH).json()["items"])==1
        for path in (tmp_path/"panel-sources").glob("*.json"):
            data=json.loads(path.read_text());data["created"]=0;path.write_text(json.dumps(data))
        assert client.get("/v1/panel/library",headers=AUTH).json()["items"]==[]
        assert not list((tmp_path/"panel-sources").glob("*.audio"))
