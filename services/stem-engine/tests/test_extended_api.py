import json
import sys
import pytest
pytest.importorskip('fastapi')
pytest.importorskip('httpx')
from fastapi.testclient import TestClient
from studio_stem_engine.cloud import Jobs, create_app
from test_cloud import AUTH, TOKEN, PAYLOAD, wait


def test_settings_persist_and_replay_conflicts(tmp_path):
    manager = Jobs(tmp_path, command=[sys.executable, '-c', 'import time;time.sleep(30)'])
    with TestClient(create_app(token=TOKEN, manager=manager)) as c:
        assert c.get('/v1/models', headers=AUTH).json()['max_channels'] == 16
        payload = {**PAYLOAD, 'mode':'dynamic', 'stem_depth':8}
        r = c.post('/v1/jobs', headers=AUTH, json=payload)
        assert r.status_code == 202
        identifier = r.json()['id']
        spec = json.loads((tmp_path/identifier/'request.json').read_text())
        assert spec['settings']['model'] == 'htdemucs_6s'
        assert spec['settings']['mode'] == 'dynamic'
        assert spec['settings']['stem_depth'] == 8
        assert c.post('/v1/jobs', headers=AUTH, json=payload).json()['id'] == identifier
        assert c.post('/v1/jobs', headers=AUTH, json={**payload, 'mode':'four'}).status_code == 409
        assert c.post('/v1/jobs', headers=AUTH, json={**payload, 'model':'../../weights'}).status_code == 400
        assert c.post('/v1/jobs', headers=AUTH, json={**payload, 'stem_depth':17}).status_code == 422
        manager.cancel(identifier)
        assert wait(manager, identifier)['state'] == 'cancelled'
