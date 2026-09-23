import pytest
pytest.importorskip("fastapi")
pytest.importorskip("httpx")
from fastapi.testclient import TestClient
from studio_stem_engine.cloud import Jobs, create_app
from test_cloud import AUTH, TOKEN


def test_device_origin_requires_pairing_and_limits_cors(tmp_path):
    with TestClient(create_app(token=TOKEN, manager=Jobs(tmp_path), device_url="http://dubbox.local/")) as client:
        origin = {"Origin": "http://dubbox.local"}
        preflight = {**origin, "Access-Control-Request-Method": "POST", "Access-Control-Request-Headers": "authorization,content-type"}
        result = client.options("/v1/uploads", headers=preflight)
        assert result.status_code == 200
        assert result.headers["access-control-allow-origin"] == origin["Origin"]
        assert client.get("/v1/models", headers=origin).status_code == 401
        result = client.get("/v1/models", headers={**origin, **AUTH})
        assert result.status_code == 200
        assert result.headers["access-control-allow-origin"] == origin["Origin"]
        assert "demucs-six" in result.json()["models"]
        result = client.options("/v1/uploads", headers={**preflight, "Origin": "http://untrusted.invalid"})
        assert result.status_code == 400
        assert "access-control-allow-origin" not in result.headers
        result = client.post("/v1/uploads?title=test.wav", headers=origin, content=b"fake")
        assert result.status_code == 401


def test_cors_disabled_without_device(tmp_path):
    with TestClient(create_app(token=TOKEN, manager=Jobs(tmp_path))) as client:
        assert "access-control-allow-origin" not in client.get("/v1/models", headers={**AUTH, "Origin": "http://dubbox.local"}).headers
