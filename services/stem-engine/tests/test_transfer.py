import importlib.util
import json
from pathlib import Path
import wave
import numpy as np
import pytest
import soundfile as sf
from studio_stem_engine import Engine, EngineError, STEMS
from studio_stem_engine.engine import digest
from studio_stem_engine.transfer import export_transfer, verify_transfer
from studio_stem_engine.device import upload_transfer, UploadError
from studio_stem_engine import device, cli


class Backend:
    def identity(self, settings): return {"test": "transfer"}
    def separate(self, samples, *args):
        return {"vocals": samples * .1, "other": samples * .2,
                "bass": samples * .3, "drums": samples * .4}


def masters(tmp_path, channels=2, rate=48000, peak=2.0):
    data = np.zeros((4801, channels), dtype=np.float32)
    data[1000:1010] = peak
    source = tmp_path / "input.wav"
    sf.write(source, data, rate, subtype="FLOAT")
    return Engine(tmp_path / "masters", backends={"demucs": Backend()}).separate(source)


@pytest.mark.parametrize("channels,rate", [(1,48000), (2,48000), (2,44100)])
def test_export_contract_and_shared_gain(tmp_path, channels, rate):
    source = masters(tmp_path, channels, rate)
    before = {p.name: digest(p) for p in source.iterdir()}
    dest = export_transfer(source, tmp_path / "transfer", set_id=7, title="My Song")
    m = verify_transfer(dest)
    assert m["frames"] == (4801 * 44100 + rate - 1) // rate
    assert 0 < m["gain"] < 1
    arrays = [sf.read(dest / (s + ".wav"), always_2d=True)[0] for s in STEMS]
    assert len({a.shape for a in arrays}) == 1
    assert len({np.argmax(a[:,0]) for a in arrays}) == 1
    assert np.max(np.abs(sum(arrays))) <= 10 ** (-1/20)
    assert np.allclose(arrays[3], arrays[0] * 4, atol=5/32768)
    if channels == 1:
        assert all(np.array_equal(a[:,0], a[:,1]) for a in arrays)
    assert before == {p.name: digest(p) for p in source.iterdir()}
    # Cross-component check against the existing firmware importer, unchanged.
    validator = Path(__file__).resolve().parents[3] / "firmware/teensy/tools/validate_prepared.py"
    spec = importlib.util.spec_from_file_location("validator", validator)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    assert mod.validate(dest)["stage"] == "ready"


def test_silence_and_no_overwrite(tmp_path):
    source = masters(tmp_path, peak=0)
    dest = export_transfer(source, tmp_path / "transfer", set_id=1)
    assert verify_transfer(dest)["gain"] == 1
    with pytest.raises(EngineError, match="already exists"):
        export_transfer(source, dest, set_id=1)


@pytest.mark.parametrize("value", [0, -1, True, 2**32])
def test_invalid_id(tmp_path, value):
    with pytest.raises(ValueError, match="set_id"):
        export_transfer("not-read", tmp_path / "out", set_id=value)


def test_failed_export_never_publishes(tmp_path, monkeypatch):
    source = masters(tmp_path)
    import studio_stem_engine.transfer as transfer
    monkeypatch.setattr(transfer, "resample", lambda *a: np.full((4411,2),np.nan))
    with pytest.raises(EngineError):
        export_transfer(source, tmp_path / "out", set_id=1)
    assert not (tmp_path / "out").exists()
    assert not list(tmp_path.glob(".transfer-*"))


@pytest.mark.parametrize("damage", ["truncated", "checksum", "order", "symlink"])
def test_bad_transfer_rejected(tmp_path, damage):
    dest = export_transfer(masters(tmp_path), tmp_path / "out", set_id=1)
    p = dest / "vocals.wav"
    if damage == "truncated": p.write_bytes(p.read_bytes()[:-8])
    elif damage == "checksum": p.write_bytes(p.read_bytes() + b"changed")
    elif damage == "symlink":
        moved = tmp_path / "outside.wav"
        p.rename(moved)
        p.symlink_to(moved)
    else:
        m = json.loads((dest / "manifest.json").read_text())
        m["stems"].reverse()
        (dest / "manifest.json").write_text(json.dumps(m))
    with pytest.raises(EngineError): verify_transfer(dest)


class FakeConnection:
    instances = []
    replies = []
    def __init__(self, *args, **kwargs):
        self.headers = {}; self.chunks = []; self.closed = False
        self.instances.append(self)
    def request(self, method, path, headers): self.method, self.path = method, path
    def putrequest(self, method, path): self.method, self.path = method, path
    def putheader(self, key, value): self.headers[key] = value
    def endheaders(self): pass
    def send(self, block): self.chunks.append(block)
    def close(self): self.closed = True
    def getresponse(self):
        body = self.replies.pop(0)
        class Reply:
            status = 200
            def read(self, limit): return json.dumps(body).encode()
        return Reply()


@pytest.fixture
def network(monkeypatch):
    FakeConnection.instances = []
    FakeConnection.replies = []
    monkeypatch.setattr(device.http.client, "HTTPConnection", FakeConnection)
    return FakeConnection


READY = {"linked": True, "busy": False, "playing": False}


def test_existing_upload_protocol(tmp_path, network):
    from urllib.parse import urlsplit, parse_qs
    dest = export_transfer(masters(tmp_path), tmp_path / "out", set_id=12)
    for role in STEMS:
        network.replies.extend([READY, {"ok": True, "name": f"S12-{role.upper()}-2.wav"}])
    result = upload_transfer(dest, "http://dubbox.local")
    assert [r["role"] for r in result] == [s.upper() for s in STEMS]
    assert result[0]["file"] == "S12-VOCALS-2.wav"
    assert len(network.instances) == 8
    for role, conn in zip(STEMS, network.instances[1::2]):
        query = parse_qs(urlsplit(conn.path).query)
        assert query["name"] == ["S12-" + role.upper()]
        content = b"".join(conn.chunks)
        assert len(content) == int(conn.headers["Content-Length"])
        assert int(query["size"][0]) == (dest / (role + ".wav")).stat().st_size
        assert content.split(b"\r\n\r\n", 1)[1].rsplit(b"\r\n--", 1)[0] == (dest / (role + ".wav")).read_bytes()
    assert all(c.closed for c in network.instances)


@pytest.mark.parametrize("state", [{**READY,"busy":True},{**READY,"playing":True},{**READY,"linked":False},{}])
def test_busy_disconnected_never_uploads(tmp_path, network, state):
    dest = export_transfer(masters(tmp_path), tmp_path / "out", set_id=1)
    network.replies = [state]
    with pytest.raises(UploadError) as exc:
        upload_transfer(dest, "http://dubbox.local")
    assert exc.value.uploaded == []
    assert len(network.instances) == 1


def test_partial_failure_reports_actual_files_no_retry(tmp_path, network):
    dest = export_transfer(masters(tmp_path), tmp_path / "out", set_id=1)
    network.replies = [READY, {"ok":True,"name":"S1-VOCALS.wav"}, READY, {"ok":False,"error":"sd"}]
    with pytest.raises(UploadError) as exc:
        upload_transfer(dest, "http://dubbox.local")
    assert exc.value.uploaded == [{"lane":1,"role":"VOCALS","file":"S1-VOCALS.wav"}]
    assert len(network.instances) == 4


def test_export_cli_without_inference(tmp_path, monkeypatch, capsys):
    source = masters(tmp_path)
    monkeypatch.setattr("sys.argv", ["studio-stems", "--export", str(source), "--transfer-output", str(tmp_path / "out"), "--set-id", "9"])
    assert cli.main() == 0
    assert json.loads(capsys.readouterr().out)["transfer"] == str(tmp_path / "out")


def test_reject_corruption_before_network(tmp_path, network):
    dest = export_transfer(masters(tmp_path), tmp_path / "out", set_id=1)
    (dest / "bass.wav").write_bytes(b"bad")
    with pytest.raises(EngineError): upload_transfer(dest, "http://dubbox.local")
    assert network.instances == []
